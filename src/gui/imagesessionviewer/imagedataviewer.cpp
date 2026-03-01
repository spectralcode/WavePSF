#include "imagedataviewer.h"
#include "data/imagedata.h"
#include "utils/logging.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QImage>
#include <QtGlobal>
#include <QMetaType>
#include <cstring>

namespace {
	const double DEFAULT_MIN_VALUE = 0.0;
	const double DEFAULT_MAX_VALUE = 255.0;
}

ImageDataViewer::ImageDataViewer(const QString& viewerName, QWidget *parent): QWidget(parent),
	  imageData(nullptr), referenceImageData(nullptr),
	  viewerName(viewerName), originalViewerName(viewerName),
	  mousePosX(-1), mousePosY(-1), currentFrame(-1),
	  autoRangeEnabled(true), manualMinValue(DEFAULT_MIN_VALUE), manualMaxValue(DEFAULT_MAX_VALUE),
	  showingReference(false),
	  renderThread(nullptr), renderWorker(nullptr), latestRequestId(0),
	  renderBusy(false), hasPending(false)
{
	this->setupUI();
	this->connectSignals();
	this->setFocusPolicy(Qt::StrongFocus);

	qRegisterMetaType<RenderRequest>("RenderRequest");

	//async renderer setup
	this->renderThread = new QThread(this);
	this->renderWorker = new ImageRenderWorker();
	this->renderWorker->moveToThread(this->renderThread);
	this->renderWorker->setLatestRequestIdSource(&this->latestRequestId);
	connect(this->renderThread, &QThread::finished, this->renderWorker, &QObject::deleteLater);
	// Viewer → Worker (queued): request render
	connect(this, &ImageDataViewer::renderRequested,
	        this->renderWorker, &ImageRenderWorker::renderFrame,
	        Qt::QueuedConnection);
	// Worker → Viewer (queued): deliver image
	connect(this->renderWorker, &ImageRenderWorker::frameRendered,
	        this, &ImageDataViewer::updateRenderedImage,
	        Qt::QueuedConnection);

	this->renderThread->start();
}

ImageDataViewer::~ImageDataViewer()
{
	if (this->renderThread) {
		this->renderThread->quit();
		this->renderThread->wait();
		this->renderThread = nullptr;
		this->renderWorker = nullptr;
	}
}

void ImageDataViewer::connectImageData(const ImageData* imageData)
{
	if (this->imageData != nullptr) {
		this->disconnectImageData();
	}

	this->imageData = imageData;
	if (this->imageData != nullptr) {
		connect(this->imageData, &ImageData::dataChanged, this, &ImageDataViewer::refresh);
		this->currentFrame = 0;
		this->displayFrame(0);
		emit imageDataConnected();
	}
}

void ImageDataViewer::disconnectImageData()
{
	if (this->imageData != nullptr) {
		disconnect(this->imageData, nullptr, this, nullptr);
		this->imageData = nullptr;
	}

	// Invalidate in-flight work (latest changes)
	this->latestRequestId.fetchAndAddOrdered(1);

	this->currentFrame = -1;
	this->showingReference = false;
	this->updateInfoDisplay();

	// Drop any pending
	this->hasPending = false;
}

void ImageDataViewer::connectReferenceImageData(const ImageData* referenceImageData)
{
	this->referenceImageData = referenceImageData;
}

const ImageData* ImageDataViewer::getImageData() const
{
	return this->imageData;
}

void ImageDataViewer::setCurrentFrame(int frame)
{
	const ImageData* dataSource = this->getCurrentDataSource();
	if (dataSource != nullptr && frame >= 0 && frame < dataSource->getFrames()) {
		this->displayFrame(frame);
	}
}

int ImageDataViewer::getFrameNr() const
{
	return this->currentFrame;
}

void ImageDataViewer::setAutoRange(bool enabled)
{
	this->autoRangeEnabled = enabled;
	this->updateDisplayRange();
}

bool ImageDataViewer::isAutoRange() const
{
	return this->autoRangeEnabled;
}

void ImageDataViewer::setDisplayRange(double minValue, double maxValue)
{
	this->manualMinValue = minValue;
	this->manualMaxValue = maxValue;
	if (!this->autoRangeEnabled) {
		this->updateDisplayRange();
	}
}

void ImageDataViewer::updateDisplayRange()
{
	if (this->currentFrame >= 0) {
		this->displayFrame(this->currentFrame);
	}
}

void ImageDataViewer::configurePatchGrid(int cols, int rows)
{
	const ImageData* dataSource = this->getCurrentDataSource();
	if (dataSource != nullptr) {
		const int width = dataSource->getWidth();
		const int height = dataSource->getHeight();
		this->frameView->generateRects(width, height, cols, rows);
	}
}

void ImageDataViewer::setPatchGridVisible(bool visible)
{
	this->frameView->setRectsVisible(visible);
}

void ImageDataViewer::highlightPatch(int patchId)
{
	this->frameView->highlightSingleRect(patchId);
}

void ImageDataViewer::highlightPatches(const QVector<int>& patchIds)
{
	this->frameView->highlightMultipleRects(patchIds);
}

void ImageDataViewer::reset()
{
	if (this->getCurrentDataSource() != nullptr) {
		this->setCurrentFrame(0);
		this->frameView->displayFullScene();
	}
}

void ImageDataViewer::enableInputDataDrops(bool enable)
{
	if (this->frameView != nullptr) {
		this->frameView->enableFileDrops(enable);
	}
}

void ImageDataViewer::focusInEvent(QFocusEvent* event)
{
	QWidget::focusInEvent(event);
}

void ImageDataViewer::focusOutEvent(QFocusEvent* event)
{
	QWidget::focusOutEvent(event);
}

QSize ImageDataViewer::sizeHint() const
{
	return QSize(600, 500);
}

void ImageDataViewer::displayFrame(int frameNr)
{
	const ImageData* dataSource = this->getCurrentDataSource();
	if (dataSource == nullptr) return;

	// Clamp frame for current data source (e.g. single-frame ground truth → 0)
	// but preserve the logical frame position so switching back restores it
	const int validFrame = this->getValidFrameForDataSource(frameNr, dataSource);
	this->currentFrame = frameNr;

	// Update bottom info immediately
	QStringList frameNames = dataSource->getFrameNames();
	this->labelFrameName->setText(validFrame < frameNames.size()
		? frameNames[validFrame]
		: QString("Frame %1").arg(validFrame));

	// Only emit frame/wavelength signals when showing primary data,
	// not during temporary ground truth preview
	if (!this->showingReference) {
		QVector<qreal> wavelengths = dataSource->getWavelengths();
		if (validFrame < wavelengths.size()) {
			emit currentWavelengthChanged(wavelengths[validFrame]);
		}
		emit currentFrameChanged(validFrame);
	}

	// Coalesce rapid changes
	this->hasPending = true;
	this->dispatchRenderNow();
}

void ImageDataViewer::dispatchRenderNow()
{
	if (!this->hasPending) return;

	const ImageData* dataSource = this->getCurrentDataSource();
	if (!dataSource) { this->hasPending = false; return; }

	const int w = dataSource->getWidth();
	const int h = dataSource->getHeight();
	if (w <= 0 || h <= 0) { this->hasPending = false; return; }

	if (this->renderBusy) {
		this->hasPending = true;
		return;
	}

	const int renderFrame = this->getValidFrameForDataSource(this->currentFrame, dataSource);
	void* framePtr = dataSource->getData(renderFrame);
	if (!framePtr) { this->hasPending = false; return; }

	const int count = w * h;
	const EnviDataType dt = dataSource->getDataType();

	RenderRequest req;
	req.width = w;
	req.height = h;
	req.dataType = dt;
	req.useAutoRange = this->autoRangeEnabled;
	req.usePercentile = false;		// if set to true, auto range calculation will use 2% 98% percentile (this can take some time, so this option is set to false which means that manually set min and max values are used for auto range)
	req.manualMin = this->manualMinValue;
	req.manualMax = this->manualMaxValue;

	copyFrameToBytes(req.frameBytes, framePtr, count, dt);

	const quint64 id = this->latestRequestId.fetchAndAddOrdered(1) + 1;
	this->latestRequestId.storeRelease(id);
	req.requestId = id;

	this->renderBusy = true;
	this->hasPending = false;
	emit this->renderRequested(req);
}

void ImageDataViewer::updateRenderedImage(const QImage& image, quint64 requestId)
{
	const bool isLatest = (requestId == this->latestRequestId.loadAcquire());

	//update view only for the latest request.
	if (isLatest && !image.isNull()) {
		this->frameView->displayFrame(image);
		this->readCurrentFrameValueAt(this->mousePosX, this->mousePosY);
	}

	this->renderBusy = false;

	if (this->hasPending) {
		this->dispatchRenderNow();
	}
}

void ImageDataViewer::refresh()
{
	if (this->currentFrame >= 0) {
		this->displayFrame(this->currentFrame);
	}
}

void ImageDataViewer::readCurrentFrameValueAt(int x, int y)
{
	const ImageData* dataSource = this->getCurrentDataSource();
	if (dataSource == nullptr) {
		this->labelXCoordinate->setText("-");
		this->labelYCoordinate->setText("-");
		this->labelPixelValue->setText("-");
		return;
	}

	const int width = dataSource->getWidth();
	const int height = dataSource->getHeight();
	this->mousePosX = x;
	this->mousePosY = y;

	if (x >= width || x < 0 || y >= height || y < 0 || width <= 0 || height <= 0) {
		this->labelXCoordinate->setText("-");
		this->labelYCoordinate->setText("-");
		this->labelPixelValue->setText("-");
		return;
	}

	void* data = dataSource->getData();
	const int validFrame = this->getValidFrameForDataSource(this->currentFrame, dataSource);
	const int samplesPerFrame = width * height;
	const int idx = validFrame * samplesPerFrame + x + y * width;

	double value = -1;

	const EnviDataType dataType = dataSource->getDataType();
	switch (dataType) {
		case UNSIGNED_CHAR_8BIT: value = static_cast<unsigned char*>(data)[idx]; break;
		case UNSIGNED_SHORT_16BIT: value = static_cast<unsigned short*>(data)[idx]; break;
		case SIGNED_SHORT_16BIT: value = static_cast<short*>(data)[idx]; break;
		case UNSIGNED_INT_32BIT: value = static_cast<unsigned int*>(data)[idx]; break;
		case SIGNED_INT_32BIT: value = static_cast<int*>(data)[idx]; break;
		case FLOAT_32BIT: value = static_cast<float*>(data)[idx]; break;
		case DOUBLE_64BIT: value = static_cast<double*>(data)[idx]; break;
		case SIGNED_LONG_INT_64BIT: value = static_cast<long long*>(data)[idx]; break;
		case UNSIGNED_LONG_INT_64BIT: value = static_cast<unsigned long long*>(data)[idx]; break;
		default: value = -1; break;
	}

	this->labelXCoordinate->setText(QString::number(x));
	this->labelYCoordinate->setText(QString::number(y));
	this->labelPixelValue->setText(QString::number(value));
}

void ImageDataViewer::selectPatch(int patchId)
{
	this->highlightPatch(patchId);
	emit patchSelectionChanged(patchId);
}

void ImageDataViewer::beginReferencePreview()
{
	if (this->referenceImageData != nullptr) {
		this->showReference(true);
	}
	//emit togglePressed();
}

void ImageDataViewer::endReferencePreview()
{
	this->showReference(false);
	emit toggleReleased();
}

void ImageDataViewer::setupUI()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

	QHBoxLayout* topBarLayout = new QHBoxLayout();
	this->labelViewerName = new QLabel(this->viewerName, this);
	this->labelViewerName->setObjectName("viewerNameLabel");
	this->labelViewerName->setStyleSheet("#viewerNameLabel { font-weight: bold; color: #333; }");
	topBarLayout->addWidget(this->labelViewerName);
	topBarLayout->addStretch();
	mainLayout->addLayout(topBarLayout);

	this->frameView = new GraphicsView(this);
	mainLayout->addWidget(this->frameView);

	QHBoxLayout* infoBarLayout = new QHBoxLayout();

	this->labelFrameName = new QLabel(this);

	this->labelX = new QLabel(tr("x: "), this);
	this->labelXCoordinate = new QLabel(this);
	this->labelY = new QLabel(tr("y: "), this);
	this->labelYCoordinate = new QLabel(this);
	this->labelValue = new QLabel(tr("value: "), this);
	this->labelPixelValue = new QLabel(this);

	infoBarLayout->addWidget(this->labelFrameName);
	infoBarLayout->addSpacing(10);
	infoBarLayout->addWidget(this->labelX);
	infoBarLayout->addWidget(this->labelXCoordinate);
	infoBarLayout->addSpacing(6);
	infoBarLayout->addWidget(this->labelY);
	infoBarLayout->addWidget(this->labelYCoordinate);
	infoBarLayout->addSpacing(6);
	infoBarLayout->addWidget(this->labelValue);
	infoBarLayout->addWidget(this->labelPixelValue);
	infoBarLayout->addStretch();

	mainLayout->addLayout(infoBarLayout);
}

void ImageDataViewer::connectSignals()
{
	connect(this->frameView, &GraphicsView::mouseMoved, this, &ImageDataViewer::readCurrentFrameValueAt);

	connect(this->frameView, &GraphicsView::rectangleSelectionChanged, this, &ImageDataViewer::selectPatch);

	connect(this->frameView, &GraphicsView::deletePressed, this, &ImageDataViewer::deletePressed);
	connect(this->frameView, &GraphicsView::copyPressed, this, &ImageDataViewer::copyPressed);
	connect(this->frameView, &GraphicsView::pastePressed, this, &ImageDataViewer::pastePressed);
	connect(this->frameView, &GraphicsView::togglePressed, this, &ImageDataViewer::beginReferencePreview);
	connect(this->frameView, &GraphicsView::toggleReleased, this, &ImageDataViewer::endReferencePreview);
	connect(this->frameView, &GraphicsView::fileDropRequested, this, &ImageDataViewer::inputFileDropRequested);
}

void ImageDataViewer::updateInfoDisplay()
{
	this->readCurrentFrameValueAt(this->mousePosX, this->mousePosY);
}

void ImageDataViewer::showReference(bool show)
{
	if(	this->showingReference == show){
		return;
	}
	this->showingReference = show;
	this->labelViewerName->setText(this->originalViewerName);
	if (this->currentFrame >= 0) {
		this->displayFrame(this->currentFrame);
	}
}

const ImageData* ImageDataViewer::getCurrentDataSource() const
{
	if (this->showingReference && this->referenceImageData != nullptr) {
		return this->referenceImageData;
	}
	return this->imageData;
}

int ImageDataViewer::getValidFrameForDataSource(int requestedFrame, const ImageData* dataSource) const
{
	if (dataSource == nullptr) return 0;
	const int frameCount = dataSource->getFrames();
	if (frameCount == 1) return 0;
	return qBound(0, requestedFrame, frameCount - 1);
}

int ImageDataViewer::sampleSizeFor(EnviDataType dt)
{
	switch (dt) {
		case UNSIGNED_CHAR_8BIT:		return sizeof(unsigned char);
		case UNSIGNED_SHORT_16BIT:		return sizeof(unsigned short);
		case SIGNED_SHORT_16BIT:		return sizeof(short);
		case UNSIGNED_INT_32BIT:		return sizeof(unsigned int);
		case SIGNED_INT_32BIT:			return sizeof(int);
		case FLOAT_32BIT:				return sizeof(float);
		case DOUBLE_64BIT:				return sizeof(double);
		case SIGNED_LONG_INT_64BIT:		return sizeof(long long);
		case UNSIGNED_LONG_INT_64BIT:	return sizeof(unsigned long long);
		default:						return 0;
	}
}

void ImageDataViewer::copyFrameToBytes(QByteArray& out, const void* src, int count, EnviDataType dt)
{
	const int bytesPerSample = sampleSizeFor(dt);
	const qsizetype bytes = static_cast<qsizetype>(count) * bytesPerSample;
	out.resize(bytes);
	if (bytes > 0 && src) {
		std::memcpy(out.data(), src, static_cast<size_t>(bytes));
	}
}
