#include "psfpreviewwidget.h"
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QResizeEvent>
#include <QtMath>


PSFPreviewWidget::PSFPreviewWidget(QWidget* parent)
	: QWidget(parent)
	, lastWidth(0)
	, lastHeight(0)
	, mousePosX(0)
	, mousePosY(0)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	this->scene = new QGraphicsScene(this);
	this->scene->setItemIndexMethod(QGraphicsScene::NoIndex);

	this->view = new QGraphicsView(this->scene, this);
	this->view->setCacheMode(QGraphicsView::CacheBackground);
	this->view->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	this->view->setRenderHint(QPainter::Antialiasing);
	this->view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	this->view->setMinimumSize(64, 64);

	this->pixmapItem = new QGraphicsPixmapItem();
	this->scene->addItem(this->pixmapItem);

	layout->addWidget(this->view);

	// Install event filter on the viewport to handle mouse interactions
	this->view->viewport()->installEventFilter(this);
}

PSFPreviewWidget::~PSFPreviewWidget()
{
}

void PSFPreviewWidget::updateImage(af::array psf)
{
	if (psf.isempty()) return;

	int rows = psf.dims(0);
	int cols = psf.dims(1);

	// Copy to host
	QVector<float> hostData(rows * cols);
	psf.as(f32).host(hostData.data());

	// Find max for log-scale normalization
	float maxVal = 0.0f;
	for (float v : qAsConst(hostData)) {
		if (v > maxVal) maxVal = v;
	}

	if (maxVal <= 0.0f) return;

	// Convert to grayscale QImage using log scale for better visibility
	this->currentImage = QImage(cols, rows, QImage::Format_Grayscale8);
	float logMax = qLn(1.0f + maxVal);

	for (int y = 0; y < rows; ++y) {
		uchar* scanLine = this->currentImage.scanLine(y);
		for (int x = 0; x < cols; ++x) {
			float val = hostData[y + x * rows]; // column-major: element(row=y, col=x)
			float logVal = qLn(1.0f + qMax(0.0f, val));
			scanLine[x] = static_cast<uchar>(255.0f * logVal / logMax);
		}
	}

	// Update the graphics item
	this->pixmapItem->setPixmap(QPixmap::fromImage(this->currentImage));

	// Auto-fit when image size changes
	if (this->lastWidth != cols || this->lastHeight != rows) {
		this->lastWidth = cols;
		this->lastHeight = rows;
		this->fitToView();
	}
}

bool PSFPreviewWidget::eventFilter(QObject* obj, QEvent* event)
{
	if (obj != this->view->viewport()) {
		return QWidget::eventFilter(obj, event);
	}

	switch (event->type()) {
		case QEvent::Wheel: {
			QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
			double angle = wheelEvent->angleDelta().y();
			double factor = qPow(1.0015, angle);

			QPoint targetViewportPos = wheelEvent->pos();
			QPointF targetScenePos = this->view->mapToScene(wheelEvent->pos());

			this->scaleView(factor);
			this->view->centerOn(targetScenePos);

			QPointF deltaViewportPos = targetViewportPos
				- QPointF(this->view->viewport()->width() / 2.0,
						  this->view->viewport()->height() / 2.0);
			QPointF viewportCenter = this->view->mapFromScene(targetScenePos) - deltaViewportPos;
			this->view->centerOn(this->view->mapToScene(viewportCenter.toPoint()));

			return true;
		}

		case QEvent::MouseButtonPress: {
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->button() == Qt::LeftButton) {
				this->mousePosX = mouseEvent->x();
				this->mousePosY = mouseEvent->y();
				return true;
			}
			break;
		}

		case QEvent::MouseMove: {
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->buttons() & Qt::LeftButton) {
				QPointF oldPos = this->view->mapToScene(this->mousePosX, this->mousePosY);
				QPointF newPos = this->view->mapToScene(mouseEvent->pos());
				QPointF delta = newPos - oldPos;
				this->view->translate(delta.x(), delta.y());
				this->mousePosX = mouseEvent->x();
				this->mousePosY = mouseEvent->y();
				return true;
			}
			break;
		}

		case QEvent::MouseButtonDblClick: {
			this->fitToView();
			return true;
		}

		case QEvent::ContextMenu: {
			QContextMenuEvent* contextEvent = static_cast<QContextMenuEvent*>(event);
			QMenu menu(this);
			QAction* saveAction = menu.addAction(tr("Save image..."));
			connect(saveAction, &QAction::triggered, this, &PSFPreviewWidget::saveImage);
			menu.exec(contextEvent->globalPos());
			return true;
		}

		default:
			break;
	}

	return QWidget::eventFilter(obj, event);
}

void PSFPreviewWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	if (!this->currentImage.isNull()) {
		this->fitToView();
	}
}

void PSFPreviewWidget::fitToView()
{
	this->scene->setSceneRect(this->scene->itemsBoundingRect());
	this->view->fitInView(this->scene->sceneRect(), Qt::KeepAspectRatio);
	this->view->ensureVisible(this->pixmapItem);
	this->view->centerOn(this->scene->itemsBoundingRect().center());
}

void PSFPreviewWidget::saveImage()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save PSF Image"),
		QString(), tr("PNG Image (*.png)"));
	if (!fileName.isEmpty()) {
		this->pixmapItem->pixmap().save(fileName, "PNG");
	}
}

void PSFPreviewWidget::scaleView(qreal scaleFactor)
{
	qreal factor = this->view->transform().scale(scaleFactor, scaleFactor)
		.mapRect(QRectF(0, 0, 1, 1)).width();
	if (factor < 0.07 || factor > 100.0) {
		return;
	}
	this->view->scale(scaleFactor, scaleFactor);
}
