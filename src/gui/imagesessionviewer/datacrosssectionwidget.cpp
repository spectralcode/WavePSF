#include "datacrosssectionwidget.h"
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
#include <QWheelEvent>
#include <QPen>
#include <QtMath>
#include <cstring>


DataCrossSectionWidget::DataCrossSectionWidget(QWidget* parent)
	: QWidget(parent)
	, inputData(nullptr)
	, outputData(nullptr)
	, currentFrame(0)
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	// Two panels side by side
	QHBoxLayout* viewersLayout = new QHBoxLayout();

	auto addPanel = [&](Panel& panel, const QString& title) {
		panel = this->createPanel(title);
		QWidget* container = new QWidget(this);
		QVBoxLayout* layout = new QVBoxLayout(container);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(panel.titleLabel);
		layout->addWidget(panel.view, 1);
		viewersLayout->addWidget(container, 1);
	};
	addPanel(this->inputPanel, tr("Input XZ"));
	addPanel(this->outputPanel, tr("Output XZ"));

	mainLayout->addLayout(viewersLayout, 1);

	// Shared Y slider
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

	// Sync slider and spinbox
	connect(this->ySlider, &QSlider::valueChanged, this->ySpinBox, &QSpinBox::setValue);
	connect(this->ySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this->ySlider, &QSlider::setValue);

	// Render on slider change
	connect(this->ySlider, &QSlider::valueChanged, this, [this](int yIndex) {
		this->updatePanel(this->inputPanel, this->inputData, yIndex);
		this->updatePanel(this->outputPanel, this->outputData, yIndex);
	});
}

DataCrossSectionWidget::Panel DataCrossSectionWidget::createPanel(const QString& title)
{
	Panel p;
	p.lastWidth = 0;
	p.lastHeight = 0;

	p.titleLabel = new QLabel(title, this);

	p.scene = new QGraphicsScene(this);
	p.scene->setItemIndexMethod(QGraphicsScene::NoIndex);

	p.view = new QGraphicsView(p.scene, this);
	p.view->setCacheMode(QGraphicsView::CacheBackground);
	p.view->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	p.view->setRenderHint(QPainter::Antialiasing);
	p.view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	p.view->setDragMode(QGraphicsView::ScrollHandDrag);
	p.view->setMinimumSize(64, 64);

	p.pixmapItem = new QGraphicsPixmapItem();
	p.scene->addItem(p.pixmapItem);

	// Frame position indicator line (horizontal, red)
	p.frameLine = new QGraphicsLineItem();
	p.frameLine->setPen(QPen(Qt::red, 1));
	p.frameLine->setVisible(false);
	p.scene->addItem(p.frameLine);

	p.view->viewport()->installEventFilter(this);

	return p;
}

void DataCrossSectionWidget::setInputData(const ImageData* data)
{
	this->inputData = data;

	if (data) {
		int height = data->getHeight();
		this->ySlider->setRange(0, height - 1);
		this->ySpinBox->setRange(0, height - 1);
		this->ySlider->setValue(height / 2);
		this->ySlider->setEnabled(true);
		this->ySpinBox->setEnabled(true);
		this->updatePanel(this->inputPanel, data, this->ySlider->value());
	} else {
		this->inputPanel.pixmapItem->setPixmap(QPixmap());
		this->inputPanel.frameLine->setVisible(false);
	}
}

void DataCrossSectionWidget::setOutputData(const ImageData* data)
{
	this->outputData = data;

	if (data) {
		this->updatePanel(this->outputPanel, data, this->ySlider->value());
	} else {
		this->outputPanel.pixmapItem->setPixmap(QPixmap());
		this->outputPanel.frameLine->setVisible(false);
	}
}

void DataCrossSectionWidget::setCurrentFrame(int frame)
{
	this->currentFrame = frame;

	if (this->inputData) {
		this->updateFrameLine(this->inputPanel, frame, this->inputData->getFrames());
	}
	if (this->outputData) {
		this->updateFrameLine(this->outputPanel, frame, this->outputData->getFrames());
	}
}

void DataCrossSectionWidget::updatePanel(Panel& panel, const ImageData* data, int yIndex)
{
	if (!data) return;

	QImage img = this->extractXZSlice(data, yIndex);
	if (img.isNull()) return;

	panel.pixmapItem->setPixmap(QPixmap::fromImage(img));

	if (panel.lastWidth != img.width() || panel.lastHeight != img.height()) {
		panel.lastWidth = img.width();
		panel.lastHeight = img.height();
		this->fitPanelToView(panel);
	}

	this->updateFrameLine(panel, this->currentFrame, data->getFrames());

	panel.titleLabel->setText(
		(data == this->inputData ? tr("Input XZ") : tr("Output XZ"))
		+ tr(" (y=%1)").arg(yIndex));
}

void DataCrossSectionWidget::fitPanelToView(Panel& panel)
{
	panel.scene->setSceneRect(panel.scene->itemsBoundingRect());
	panel.view->fitInView(panel.scene->sceneRect(), Qt::KeepAspectRatio);
}

void DataCrossSectionWidget::updateFrameLine(Panel& panel, int frame, int numFrames)
{
	if (numFrames <= 1) {
		panel.frameLine->setVisible(false);
		return;
	}

	QPixmap px = panel.pixmapItem->pixmap();
	if (px.isNull()) return;

	// Frame = row in the XZ image
	qreal y = static_cast<qreal>(frame) + 0.5;
	panel.frameLine->setLine(0, y, px.width(), y);
	panel.frameLine->setVisible(true);
}

QImage DataCrossSectionWidget::extractXZSlice(const ImageData* data, int yIndex)
{
	if (!data) return QImage();

	int width = data->getWidth();
	int height = data->getHeight();
	int frames = data->getFrames();
	if (yIndex < 0 || yIndex >= height || frames == 0 || width == 0) {
		return QImage();
	}

	size_t bps = data->getBytesPerSample();
	EnviDataType dtype = data->getDataType();

	// Extract all pixel values at row yIndex across all frames
	QVector<double> values(frames * width);

	for (int z = 0; z < frames; ++z) {
		const char* framePtr = static_cast<const char*>(data->getData(z));
		const char* rowPtr = framePtr + yIndex * width * bps;

		for (int x = 0; x < width; ++x) {
			double val = 0.0;
			const char* pixPtr = rowPtr + x * bps;

			switch (dtype) {
				case UNSIGNED_CHAR_8BIT:
					val = *reinterpret_cast<const uint8_t*>(pixPtr);
					break;
				case SIGNED_SHORT_16BIT:
					val = *reinterpret_cast<const int16_t*>(pixPtr);
					break;
				case UNSIGNED_SHORT_16BIT:
					val = *reinterpret_cast<const uint16_t*>(pixPtr);
					break;
				case SIGNED_INT_32BIT:
					val = *reinterpret_cast<const int32_t*>(pixPtr);
					break;
				case UNSIGNED_INT_32BIT:
					val = *reinterpret_cast<const uint32_t*>(pixPtr);
					break;
				case FLOAT_32BIT:
					val = *reinterpret_cast<const float*>(pixPtr);
					break;
				case DOUBLE_64BIT:
					val = *reinterpret_cast<const double*>(pixPtr);
					break;
				default:
					val = *reinterpret_cast<const float*>(pixPtr);
					break;
			}

			values[z * width + x] = val;
		}
	}

	// Find min/max for auto-ranging
	double minVal = values[0];
	double maxVal = values[0];
	for (double v : qAsConst(values)) {
		if (v < minVal) minVal = v;
		if (v > maxVal) maxVal = v;
	}

	double range = maxVal - minVal;
	if (range <= 0.0) range = 1.0;

	// Build grayscale QImage: rows = frames (Z), cols = width (X)
	QImage img(width, frames, QImage::Format_Grayscale8);
	for (int z = 0; z < frames; ++z) {
		uchar* scanLine = img.scanLine(z);
		for (int x = 0; x < width; ++x) {
			double normalized = (values[z * width + x] - minVal) / range;
			scanLine[x] = static_cast<uchar>(qBound(0.0, normalized * 255.0, 255.0));
		}
	}

	return img;
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

		case QEvent::MouseButtonDblClick: {
			this->fitPanelToView(*panel);
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
	if (this->inputData) {
		this->fitPanelToView(this->inputPanel);
	}
	if (this->outputData) {
		this->fitPanelToView(this->outputPanel);
	}
}
