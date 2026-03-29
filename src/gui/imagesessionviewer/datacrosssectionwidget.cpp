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
#include <QtGlobal>
#include <QMenu>
#include <QContextMenuEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QHoverEvent>
#include <QPen>
#include <QtMath>
#include <algorithm>
#include <cmath>


DataCrossSectionWidget::DataCrossSectionWidget(QWidget* parent)
	: QWidget(parent)
	, inputData(nullptr)
	, outputData(nullptr)
	, currentFrame(0)
	, showFrameLine(true)
	, autoRangeEnabled(true)
	, manualMinValue(0.0)
	, manualMaxValue(255.0)
	, pendingRefresh(false)
	, draggingFrameLine(false)
	, draggingPanel(nullptr)
{
	this->setupUI();
	this->connectSignals();
	this->updateYControls();
}

DataCrossSectionWidget::Panel DataCrossSectionWidget::createPanel(const QString& title)
{
	Panel p;
	p.lastWidth = 0;
	p.lastHeight = 0;
	p.baseTitle = title;

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
	p.view->setMouseTracking(true);
	p.view->viewport()->setMouseTracking(true);
	p.view->viewport()->setAttribute(Qt::WA_Hover, true);

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
	this->updateYControls();
	this->refreshPanels();
}

void DataCrossSectionWidget::setOutputData(const ImageData* data)
{
	this->outputData = data;
	this->updateYControls();
	this->refreshPanels();
}

void DataCrossSectionWidget::setDisplaySettings(bool autoRange, double minValue, double maxValue)
{
	this->autoRangeEnabled = autoRange;
	this->manualMinValue = minValue;
	this->manualMaxValue = maxValue;
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

	QImage img = this->extractXZSlice(data, clampedY);
	if (img.isNull()) return;

	panel.pixmapItem->setPixmap(QPixmap::fromImage(img));

	if (panel.lastWidth != img.width() || panel.lastHeight != img.height()) {
		panel.lastWidth = img.width();
		panel.lastHeight = img.height();
		this->fitPanelToView(panel);
	}

	this->updateFrameLine(panel, qBound(0, this->currentFrame, frames - 1), frames);

	panel.titleLabel->setText(
		panel.baseTitle + tr(" (y=%1)").arg(clampedY));
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

	auto readValue = [&](const char* pixPtr) -> double {
		switch (dtype) {
			case UNSIGNED_CHAR_8BIT:
				return *reinterpret_cast<const uint8_t*>(pixPtr);
			case SIGNED_SHORT_16BIT:
				return *reinterpret_cast<const int16_t*>(pixPtr);
			case UNSIGNED_SHORT_16BIT:
				return *reinterpret_cast<const uint16_t*>(pixPtr);
			case SIGNED_INT_32BIT:
				return *reinterpret_cast<const int32_t*>(pixPtr);
			case UNSIGNED_INT_32BIT:
				return *reinterpret_cast<const uint32_t*>(pixPtr);
			case FLOAT_32BIT:
				return *reinterpret_cast<const float*>(pixPtr);
			case DOUBLE_64BIT:
				return *reinterpret_cast<const double*>(pixPtr);
			case SIGNED_LONG_INT_64BIT:
				return *reinterpret_cast<const long long*>(pixPtr);
			case UNSIGNED_LONG_INT_64BIT:
				return *reinterpret_cast<const unsigned long long*>(pixPtr);
			case COMPLEX_FLOAT_2X32BIT: {
				auto c = reinterpret_cast<const float*>(pixPtr);
				return std::hypot(c[0], c[1]);
			}
			case COMPLEX_DOUBLE_2X64BIT: {
				auto c = reinterpret_cast<const double*>(pixPtr);
				return std::hypot(c[0], c[1]);
			}
			default:
				return *reinterpret_cast<const float*>(pixPtr);
		}
	};

	for (int z = 0; z < frames; ++z) {
		const char* framePtr = static_cast<const char*>(data->getData(z));
		if (!framePtr) return QImage();
		const char* rowPtr = framePtr + yIndex * width * bps;

		for (int x = 0; x < width; ++x) {
			const char* pixPtr = rowPtr + x * bps;
			values[z * width + x] = readValue(pixPtr);
		}
	}

	if (values.isEmpty()) return QImage();

	// Determine display range (match ImageDataViewer behavior: auto-range per slice or manual)
	double minVal = this->manualMinValue;
	double maxVal = this->manualMaxValue;
	if (this->autoRangeEnabled) {
		minVal = values[0];
		maxVal = values[0];
		for (double v : values) {
			if (v < minVal) minVal = v;
			if (v > maxVal) maxVal = v;
		}
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
	this->ySlider->setRange(0, maxIndex);
	this->ySpinBox->setRange(0, maxIndex);
	int clamped = qBound(0, this->ySlider->value(), maxIndex);
	this->ySlider->setValue(clamped);
	this->ySpinBox->setValue(clamped);
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

void DataCrossSectionWidget::clearPanel(Panel& panel)
{
	panel.pixmapItem->setPixmap(QPixmap());
	panel.frameLine->setVisible(false);
	panel.lastWidth = 0;
	panel.lastHeight = 0;
	panel.titleLabel->setText(panel.baseTitle);
}
