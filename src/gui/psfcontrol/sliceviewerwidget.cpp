#include "sliceviewerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QWheelEvent>
#include <QMenu>
#include <QAction>
#include <QFileDialog>


SliceViewerWidget::SliceViewerWidget(const QString& axisLabel, const QString& defaultTitle,
                                     QWidget* parent)
	: QWidget(parent)
	, lastWidth(0), lastHeight(0)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	this->titleLabel = new QLabel(defaultTitle, this);
	layout->addWidget(this->titleLabel);

	this->scene = new QGraphicsScene(this);
	this->scene->setItemIndexMethod(QGraphicsScene::NoIndex);
	this->view = new QGraphicsView(this->scene, this);
	this->view->setCacheMode(QGraphicsView::CacheBackground);
	this->view->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	this->view->setRenderHint(QPainter::Antialiasing);
	this->view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	this->view->setDragMode(QGraphicsView::ScrollHandDrag);
	this->view->setMinimumSize(64, 64);
	this->pixmapItem = new QGraphicsPixmapItem();
	this->scene->addItem(this->pixmapItem);
	layout->addWidget(this->view, 1);

	QHBoxLayout* sliderRow = new QHBoxLayout();
	sliderRow->addWidget(new QLabel(axisLabel + QStringLiteral(":"), this));
	this->slider = new QSlider(Qt::Horizontal, this);
	this->slider->setRange(0, 0);
	sliderRow->addWidget(this->slider, 1);
	this->spinBox = new QSpinBox(this);
	this->spinBox->setRange(0, 0);
	sliderRow->addWidget(this->spinBox);
	layout->addLayout(sliderRow);

	connect(this->slider, &QSlider::valueChanged,
	        this, &SliceViewerWidget::onSliderValueChanged);
	connect(this->spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
	        this, &SliceViewerWidget::onSpinBoxValueChanged);

	this->view->viewport()->installEventFilter(this);
}

void SliceViewerWidget::setImage(const QImage& img)
{
	this->pixmapItem->setPixmap(QPixmap::fromImage(img));

	if (this->lastWidth != img.width() || this->lastHeight != img.height()) {
		this->lastWidth = img.width();
		this->lastHeight = img.height();
		this->fitToView();
	}
}

void SliceViewerWidget::setSliderRange(int min, int max)
{
	this->slider->blockSignals(true);
	this->spinBox->blockSignals(true);
	this->slider->setRange(min, max);
	this->spinBox->setRange(min, max);
	this->slider->blockSignals(false);
	this->spinBox->blockSignals(false);
}

void SliceViewerWidget::setSliderValue(int value)
{
	this->slider->blockSignals(true);
	this->spinBox->blockSignals(true);
	this->slider->setValue(value);
	this->spinBox->setValue(value);
	this->slider->blockSignals(false);
	this->spinBox->blockSignals(false);
}

int SliceViewerWidget::sliderValue() const
{
	return this->slider->value();
}

int SliceViewerWidget::sliderMaximum() const
{
	return this->slider->maximum();
}

void SliceViewerWidget::setTitle(const QString& text)
{
	this->titleLabel->setText(text);
}

void SliceViewerWidget::setValueLabel(const QString& /* text */)
{
	// Position info is shown in the title label; no separate value label needed.
}

void SliceViewerWidget::onSliderValueChanged(int value)
{
	this->spinBox->blockSignals(true);
	this->spinBox->setValue(value);
	this->spinBox->blockSignals(false);
	emit sliceChanged(value);
}

void SliceViewerWidget::onSpinBoxValueChanged(int value)
{
	this->slider->blockSignals(true);
	this->slider->setValue(value);
	this->slider->blockSignals(false);
	emit sliceChanged(value);
}

bool SliceViewerWidget::eventFilter(QObject* obj, QEvent* event)
{
	if (obj != this->view->viewport()) {
		return QWidget::eventFilter(obj, event);
	}

	switch (event->type()) {
		case QEvent::Wheel: {
			QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
			double factor = (wheelEvent->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
			this->scaleView(factor);
			return true;
		}

		case QEvent::MouseButtonDblClick: {
			this->fitToView();
			return true;
		}

		case QEvent::ContextMenu: {
			QContextMenuEvent* contextEvent = static_cast<QContextMenuEvent*>(event);
			QMenu menu(this);
			QAction* centerAction = menu.addAction(tr("Go to Center"));
			connect(centerAction, &QAction::triggered,
			        this, &SliceViewerWidget::goToCenter);
			QAction* saveAction = menu.addAction(tr("Save image..."));
			connect(saveAction, &QAction::triggered,
			        this, &SliceViewerWidget::saveImage);
			for (QAction* action : qAsConst(this->extraActions)) {
				menu.addAction(action);
			}
			for (QMenu* submenu : qAsConst(this->extraSubmenus)) {
				menu.addMenu(submenu);
			}
			menu.exec(contextEvent->globalPos());
			return true;
		}

		default:
			break;
	}

	return QWidget::eventFilter(obj, event);
}

void SliceViewerWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	//this->fitToView();
}

void SliceViewerWidget::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	this->fitToView();
}

void SliceViewerWidget::fitToView()
{
	QGraphicsScene* s = this->view->scene();
	if (s) {
		s->setSceneRect(s->itemsBoundingRect());
		this->view->fitInView(s->sceneRect(), Qt::KeepAspectRatio);
	}
}

void SliceViewerWidget::scaleView(qreal scaleFactor)
{
	qreal factor = this->view->transform().scale(scaleFactor, scaleFactor)
		.mapRect(QRectF(0, 0, 1, 1)).width();
	if (factor < 0.07 || factor > 100.0) {
		return;
	}
	this->view->scale(scaleFactor, scaleFactor);
}

void SliceViewerWidget::saveImage()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save PSF Image"),
		QString(), tr("PNG Image (*.png)"));
	if (!fileName.isEmpty()) {
		this->pixmapItem->pixmap().save(fileName, "PNG");
	}
}

void SliceViewerWidget::goToCenter()
{
	int center = (this->slider->maximum() + this->slider->minimum()) / 2;
	this->slider->setValue(center);
}

void SliceViewerWidget::addContextMenuAction(QAction* action)
{
	this->extraActions.append(action);
}

void SliceViewerWidget::addContextMenuSubmenu(QMenu* submenu)
{
	this->extraSubmenus.append(submenu);
}
