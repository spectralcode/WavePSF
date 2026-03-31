#include "datacrosssectionwidget.h"
#include "imagerenderworker.h"
#include "gui/lut.h"
#include "data/imagedata.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsLineItem>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QImage>
#include <QtGlobal>
#include <QMenu>
#include <QContextMenuEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QHoverEvent>
#include <QPen>
#include <QtMath>
#include <QMetaObject>
#include <algorithm>
#include <cmath>
#include <cstring>


DataCrossSectionWidget::DataCrossSectionWidget(QWidget* parent)
	: QWidget(parent)
	, inputData(nullptr)
	, outputData(nullptr)
	, currentFrame(0)
	, showFrameLine(true)
	, pendingRefresh(false)
	, draggingFrameLine(false)
	, draggingPanel(nullptr)
{
	qRegisterMetaType<RenderRequest>("RenderRequest");
	this->setupUI();
	this->connectSignals();
	this->updateYControls();
}

DataCrossSectionWidget::~DataCrossSectionWidget()
{
	auto stopRenderer = [](Panel& p) {
		if (p.renderThread) {
			p.renderThread->quit();
			p.renderThread->wait();
			p.renderThread = nullptr;
			p.renderWorker = nullptr;
		}
	};
	stopRenderer(this->inputPanel);
	stopRenderer(this->outputPanel);
}

void DataCrossSectionWidget::initPanel(Panel& panel, const QString& title)
{
	panel.baseTitle = title;

	panel.titleLabel = new QLabel(title, this);

	panel.scene = new QGraphicsScene(this);
	panel.scene->setItemIndexMethod(QGraphicsScene::NoIndex);

	panel.view = new QGraphicsView(panel.scene, this);
	panel.view->setCacheMode(QGraphicsView::CacheBackground);
	panel.view->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	panel.view->setRenderHint(QPainter::Antialiasing);
	panel.view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	panel.view->setDragMode(QGraphicsView::ScrollHandDrag);
	panel.view->setMinimumSize(64, 64);
	panel.view->setMouseTracking(true);
	panel.view->viewport()->setMouseTracking(true);
	panel.view->viewport()->setAttribute(Qt::WA_Hover, true);

	panel.pixmapItem = new QGraphicsPixmapItem();
	panel.scene->addItem(panel.pixmapItem);

	// Frame position indicator line (horizontal, red)
	panel.frameLine = new QGraphicsLineItem();
	panel.frameLine->setPen(QPen(Qt::red, 1));
	panel.frameLine->setVisible(false);
	panel.scene->addItem(panel.frameLine);

	panel.view->viewport()->installEventFilter(this);

	// Async render worker (matches ImageDataViewer pattern)
	panel.renderThread = new QThread(this);
	panel.renderWorker = new ImageRenderWorker();
	panel.renderWorker->moveToThread(panel.renderThread);
	panel.renderWorker->setLatestRequestIdSource(&panel.latestRequestId);
	connect(panel.renderThread, &QThread::finished, panel.renderWorker, &QObject::deleteLater);
	panel.renderThread->start();
}

void DataCrossSectionWidget::setInputData(const ImageData* data)
{
	this->inputData = data;
	this->updateYControls();
	this->refreshPanels();
}

void DataCrossSectionWidget::setOutputData(const ImageData* data)
{
	this->outputData = data;
	this->updateYControls();
	this->refreshPanels();
}

void DataCrossSectionWidget::setDisplaySettings(const DisplaySettings& settings)
{
	this->displaySettings = settings;
	this->refreshPanels();
}

void DataCrossSectionWidget::setFrameLineVisible(bool visible)
{
	this->showFrameLine = visible;
	this->updateFrameLine(this->inputPanel, this->currentFrame, this->inputData ? this->inputData->getFrames() : 0);
	this->updateFrameLine(this->outputPanel, this->currentFrame, this->outputData ? this->outputData->getFrames() : 0);
}

void DataCrossSectionWidget::setCurrentFrame(int frame)
{
	this->currentFrame = frame;
	const auto clampFrame = [](int f, int total) {
		if (total <= 0) return 0;
		return qBound(0, f, total - 1);
	};

	if (this->inputData) {
		this->updateFrameLine(this->inputPanel, clampFrame(frame, this->inputData->getFrames()), this->inputData->getFrames());
	}
	if (this->outputData) {
		this->updateFrameLine(this->outputPanel, clampFrame(frame, this->outputData->getFrames()), this->outputData->getFrames());
	}
}

void DataCrossSectionWidget::updatePanel(Panel& panel, const ImageData* data, int yIndex)
{
	if (!data) return;

	const int height = data->getHeight();
	if (height <= 0) return;
	int clampedY = qBound(0, yIndex, height - 1);
	const int frames = data->getFrames();
	if (frames <= 0) {
		panel.frameLine->setVisible(false);
		panel.titleLabel->setText(panel.baseTitle);
		return;
	}

	// Update title and frame line immediately (don't depend on render result)
	panel.titleLabel->setText(panel.baseTitle + tr(" (y=%1)").arg(clampedY));
	this->updateFrameLine(panel, qBound(0, this->currentFrame, frames - 1), frames);

	// Dispatch async render via ImageRenderWorker
	this->dispatchPanelRender(panel, data, clampedY);
}

void DataCrossSectionWidget::fitPanelToView(Panel& panel)
{
	panel.scene->setSceneRect(panel.scene->itemsBoundingRect());
	panel.view->fitInView(panel.scene->sceneRect(), Qt::KeepAspectRatio);
	panel.view->ensureVisible(panel.pixmapItem);
	panel.view->centerOn(panel.scene->itemsBoundingRect().center());
}

void DataCrossSectionWidget::updateFrameLine(Panel& panel, int frame, int numFrames)
{
	if (!this->showFrameLine || numFrames <= 1) {
		panel.frameLine->setVisible(false);
		return;
	}

	const int clampedFrame = qBound(0, frame, numFrames - 1);

	QPixmap px = panel.pixmapItem->pixmap();
	if (px.isNull()) return;

	// Frame = row in the XZ image
	qreal y = static_cast<qreal>(clampedFrame) + 0.5;
	panel.frameLine->setLine(0, y, px.width(), y);
	panel.frameLine->setVisible(true);
}

QByteArray DataCrossSectionWidget::extractXZSliceBytes(const ImageData* data, int yIndex)
{
	if (!data) return QByteArray();

	const int width = data->getWidth();
	const int height = data->getHeight();
	const int frames = data->getFrames();
	if (yIndex < 0 || yIndex >= height || frames == 0 || width == 0) {
		return QByteArray();
	}

	const size_t bps = data->getBytesPerSample();
	const size_t rowBytes = width * bps;

	// Copy raw bytes for row yIndex from each frame — no per-pixel type conversion
	QByteArray bytes(static_cast<int>(frames * rowBytes), '\0');
	char* dst = bytes.data();
	for (int z = 0; z < frames; ++z) {
		const char* framePtr = static_cast<const char*>(data->getData(z));
		if (!framePtr) return QByteArray();
		std::memcpy(dst + z * rowBytes, framePtr + yIndex * rowBytes, rowBytes);
	}
	return bytes;
}

void DataCrossSectionWidget::dispatchPanelRender(Panel& panel, const ImageData* data, int yIndex)
{
	QByteArray sliceBytes = this->extractXZSliceBytes(data, yIndex);
	if (sliceBytes.isEmpty()) return;

	RenderRequest req;
	req.frameBytes = sliceBytes;
	req.width = data->getWidth();
	req.height = data->getFrames();
	req.dataType = data->getDataType();
	req.useAutoRange = (this->displaySettings.autoRangeMode != AutoRangeMode::Off);
	req.usePercentile = false;
	req.manualMin = this->displaySettings.rangeMin;
	req.manualMax = this->displaySettings.rangeMax;
	req.logScale = this->displaySettings.logScale;
	req.colorTable = LUT::get(this->displaySettings.lutName);

	const quint64 id = panel.latestRequestId.fetchAndAddOrdered(1) + 1;
	panel.latestRequestId.storeRelease(id);
	req.requestId = id;

	QMetaObject::invokeMethod(panel.renderWorker, [worker = panel.renderWorker, req]() {
		worker->renderFrame(req);
	}, Qt::QueuedConnection);
}

void DataCrossSectionWidget::handlePanelRendered(Panel& panel, const QImage& image, quint64 reqId)
{
	if (reqId != panel.latestRequestId.loadAcquire()) return;
	if (image.isNull()) return;

	panel.pixmapItem->setPixmap(QPixmap::fromImage(image));

	if (panel.lastWidth != image.width() || panel.lastHeight != image.height()) {
		panel.lastWidth = image.width();
		panel.lastHeight = image.height();
		this->fitPanelToView(panel);
	}
}

bool DataCrossSectionWidget::eventFilter(QObject* obj, QEvent* event)
{
	// Handle events for both panels' viewports
	Panel* panel = nullptr;
	if (obj == this->inputPanel.view->viewport()) {
		panel = &this->inputPanel;
	} else if (obj == this->outputPanel.view->viewport()) {
		panel = &this->outputPanel;
	}

	if (!panel) {
		return QWidget::eventFilter(obj, event);
	}

	switch (event->type()) {
		case QEvent::Wheel: {
			QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
			double factor = (wheelEvent->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
			qreal current = panel->view->transform().scale(factor, factor)
				.mapRect(QRectF(0, 0, 1, 1)).width();
			if (current >= 0.07 && current <= 100.0) {
				panel->view->scale(factor, factor);
			}
			return true;
		}

		case QEvent::HoverMove: {
			if (this->draggingFrameLine) {
				return true;
			}
			QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
			if (this->showFrameLine && panel->frameLine->isVisible()) {
				QPointF scenePos = panel->view->mapToScene(hoverEvent->pos());
				qreal lineY = panel->frameLine->line().y1();
				bool nearLine = (std::abs(scenePos.y() - lineY) <= 3.0);
				if (nearLine) {
					panel->view->viewport()->setCursor(Qt::SplitVCursor);
				} else if (panel->view->viewport()->cursor().shape() == Qt::SplitVCursor) {
					panel->view->viewport()->unsetCursor();
				}
			}
			return false;
		}

		case QEvent::MouseButtonPress: {
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->button() == Qt::LeftButton && this->showFrameLine && panel->frameLine->isVisible()) {
				QPointF scenePos = panel->view->mapToScene(mouseEvent->pos());
				qreal lineY = panel->frameLine->line().y1();
				if (std::abs(scenePos.y() - lineY) <= 3.0) {
					this->draggingFrameLine = true;
					this->draggingPanel = panel;
					// Temporarily disable ScrollHandDrag so it doesn't fight the frame line drag
					panel->view->setDragMode(QGraphicsView::NoDrag);
					panel->view->viewport()->setCursor(Qt::SplitVCursor);
					return true;
				}
			}
			break; // Let ScrollHandDrag handle normal left-click panning
		}

		case QEvent::MouseMove: {
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			if (this->draggingFrameLine && this->draggingPanel == panel && (mouseEvent->buttons() & Qt::LeftButton)) {
				QPointF scenePos = panel->view->mapToScene(mouseEvent->pos());
				int maxFrames = 0;
				if (panel == &this->inputPanel && this->inputData) {
					maxFrames = this->inputData->getFrames();
				} else if (panel == &this->outputPanel && this->outputData) {
					maxFrames = this->outputData->getFrames();
				}
				if (maxFrames > 0) {
					int targetFrame = qBound(0, qRound(scenePos.y()), maxFrames - 1);
					this->currentFrame = targetFrame;
					this->updateFrameLine(*panel, targetFrame, maxFrames);
					emit frameChangeRequested(targetFrame);
				}
				return true;
			}
			break; // Let ScrollHandDrag handle normal mouse move
		}

		case QEvent::MouseButtonRelease: {
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->button() == Qt::LeftButton && this->draggingFrameLine) {
				this->draggingFrameLine = false;
				// Restore ScrollHandDrag after frame line drag ends
				if (this->draggingPanel) {
					this->draggingPanel->view->setDragMode(QGraphicsView::ScrollHandDrag);
				}
				this->draggingPanel = nullptr;
				return true;
			}
			break;
		}

		case QEvent::MouseButtonDblClick: {
			this->fitPanelToView(*panel);
			return true;
		}

		case QEvent::ContextMenu: {
			QContextMenuEvent* ctxEvent = static_cast<QContextMenuEvent*>(event);
			QMenu menu(this);
			QAction* toggleLine = menu.addAction(tr("Show Position Line"));
			toggleLine->setCheckable(true);
			toggleLine->setChecked(this->showFrameLine);
			connect(toggleLine, &QAction::toggled, this, [this](bool checked) {
				this->setFrameLineVisible(checked);
			});
			menu.exec(ctxEvent->globalPos());
			return true;
		}

		default:
			break;
	}

	return QWidget::eventFilter(obj, event);
}

void DataCrossSectionWidget::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	if (this->pendingRefresh) {
		this->pendingRefresh = false;
		this->refreshPanels();
	}
	if (!this->inputPanel.pixmapItem->pixmap().isNull()) {
		this->fitPanelToView(this->inputPanel);
	}
	if (!this->outputPanel.pixmapItem->pixmap().isNull()) {
		this->fitPanelToView(this->outputPanel);
	}
}

void DataCrossSectionWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	if (!this->inputPanel.pixmapItem->pixmap().isNull()) {
		this->fitPanelToView(this->inputPanel);
	}
	if (!this->outputPanel.pixmapItem->pixmap().isNull()) {
		this->fitPanelToView(this->outputPanel);
	}
}

void DataCrossSectionWidget::setupUI()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* viewersLayout = new QHBoxLayout();
	auto addPanel = [&](Panel& panel, const QString& title) {
		this->initPanel(panel, title);
		QWidget* container = new QWidget(this);
		QVBoxLayout* layout = new QVBoxLayout(container);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(panel.titleLabel);
		layout->addWidget(panel.view, 1);
		viewersLayout->addWidget(container, 1);
	};
	addPanel(this->inputPanel, tr("Input XZ"));
	addPanel(this->outputPanel, tr("Output XZ"));

	// Connect render completion signals (worker thread → GUI thread)
	connect(this->inputPanel.renderWorker, &ImageRenderWorker::frameRendered,
	        this, [this](const QImage& image, quint64 reqId) {
		this->handlePanelRendered(this->inputPanel, image, reqId);
	}, Qt::QueuedConnection);
	connect(this->outputPanel.renderWorker, &ImageRenderWorker::frameRendered,
	        this, [this](const QImage& image, quint64 reqId) {
		this->handlePanelRendered(this->outputPanel, image, reqId);
	}, Qt::QueuedConnection);
	mainLayout->addLayout(viewersLayout, 1);

	QHBoxLayout* sliderRow = new QHBoxLayout();
	sliderRow->addWidget(new QLabel(tr("Y:"), this));
	this->ySlider = new QSlider(Qt::Horizontal, this);
	this->ySlider->setRange(0, 0);
	this->ySlider->setEnabled(false);
	sliderRow->addWidget(this->ySlider, 1);
	this->ySpinBox = new QSpinBox(this);
	this->ySpinBox->setRange(0, 0);
	this->ySpinBox->setEnabled(false);
	sliderRow->addWidget(this->ySpinBox);
	mainLayout->addLayout(sliderRow);
}

void DataCrossSectionWidget::connectSignals()
{
	connect(this->ySlider, &QSlider::valueChanged, this->ySpinBox, &QSpinBox::setValue);
	connect(this->ySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this->ySlider, &QSlider::setValue);
	connect(this->ySlider, &QSlider::valueChanged, this, [this](int yIndex) {
		this->updatePanel(this->inputPanel, this->inputData, yIndex);
		this->updatePanel(this->outputPanel, this->outputData, yIndex);
		emit yPositionChanged(yIndex);
	});
}

void DataCrossSectionWidget::updateYControls()
{
	int maxHeight = -1;
	if (this->inputData) maxHeight = this->inputData->getHeight();
	if (this->outputData) maxHeight = (maxHeight < 0) ? this->outputData->getHeight()
		: std::max(maxHeight, this->outputData->getHeight());

	const bool hasData = maxHeight > 0;
	const int maxIndex = hasData ? maxHeight - 1 : 0;

	this->ySlider->blockSignals(true);
	this->ySpinBox->blockSignals(true);
	this->ySlider->setEnabled(hasData);
	this->ySpinBox->setEnabled(hasData);
	bool rangeChanged = (this->ySlider->maximum() != maxIndex);
	this->ySlider->setRange(0, maxIndex);
	this->ySpinBox->setRange(0, maxIndex);
	int value = rangeChanged ? (maxIndex / 2) : qBound(0, this->ySlider->value(), maxIndex);
	this->ySlider->setValue(value);
	this->ySpinBox->setValue(value);
	this->ySlider->blockSignals(false);
	this->ySpinBox->blockSignals(false);

	if (!hasData) {
		this->clearPanel(this->inputPanel);
		this->clearPanel(this->outputPanel);
	}
}

void DataCrossSectionWidget::refreshPanels()
{
	if (!this->isVisible()) {
		this->pendingRefresh = true;
		return;
	}
	int yIndex = this->ySlider->value();
	this->updatePanel(this->inputPanel, this->inputData, yIndex);
	this->updatePanel(this->outputPanel, this->outputData, yIndex);
}

void DataCrossSectionWidget::setYPosition(int y)
{
	this->ySlider->setValue(y);
}

int DataCrossSectionWidget::currentYPosition() const
{
	return this->ySlider->value();
}

void DataCrossSectionWidget::clearPanel(Panel& panel)
{
	panel.pixmapItem->setPixmap(QPixmap());
	panel.frameLine->setVisible(false);
	panel.lastWidth = 0;
	panel.lastHeight = 0;
	panel.titleLabel->setText(panel.baseTitle);
}
