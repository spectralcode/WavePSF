#include "graphicsview.h"

#include <QKeyEvent>
#include <QWheelEvent>
#include <QtMath>
#include <QMimeData>
#include <QApplication>
#include <QScrollBar>
#include "utils/supportedfilechecker.h"

GraphicsView::GraphicsView(QWidget* parent) : QGraphicsView(parent)
{
	this->scene = new QGraphicsScene(this);
	this->scene->setItemIndexMethod(QGraphicsScene::NoIndex);
	this->setScene(scene);
	this->setCacheMode(CacheBackground);
	this->setViewportUpdateMode(BoundingRectViewportUpdate);
	this->setRenderHint(QPainter::Antialiasing);
	this->setTransformationAnchor(AnchorUnderMouse);

	this->inputItem = new QGraphicsPixmapItem();
	this->scene->addItem(inputItem);
	this->scene->update();

	this->frameWidth = 0;
	this->frameHeight = 0;
	this->mousePosX = 0;
	this->mousePosY = 0;

	this->vflipped = false;
	this->hflipped = false;

	this->grid = new RectItemGroup(nullptr);
	this->scene->addItem(grid);
	connect(this->grid, &RectItemGroup::selectionChanged, this, &GraphicsView::rectangleSelectionChanged);
	connect(this->grid, &RectItemGroup::gridGenerated, this, &GraphicsView::gridGenerated);

	this->acceptFileDrops = false;
	this->isHighlightedForDrop = false;
	this->setAcceptDrops(false);
}

GraphicsView::~GraphicsView()
{
	delete this->grid;
}

void GraphicsView::enableFileDrops(bool enable) {
	this->acceptFileDrops = enable;
	this->setAcceptDrops(enable);

	if (!enable && this->isHighlightedForDrop) {
		this->setDropHighlight(false);
	}
}

void GraphicsView::refreshForStyleChange() {
	CacheMode oldCache = this->cacheMode();
	this->setCacheMode(CacheNone);

	QColor sceneBackground = this->palette().base().color();

	if (this->scene) {
		this->scene->setBackgroundBrush(QBrush(sceneBackground));
		this->scene->invalidate();
		this->scene->update();
	}

	this->viewport()->update();
	this->update();

	this->setCacheMode(oldCache);
}

void GraphicsView::mouseDoubleClickEvent(QMouseEvent* event) {
	this->displayFullScene();
	QGraphicsView::mouseDoubleClickEvent(event);
}

void GraphicsView::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::MiddleButton) {
		this->mousePosX = event->x();
		this->mousePosY = event->y();

		QPointF mousePosInScene = mapToScene(event->pos());
		emit mouseMiddleButtonPressed(static_cast<int>(mousePosInScene.x()), static_cast<int>(mousePosInScene.y()));

		event->accept();
		return;
	}

	QGraphicsView::mousePressEvent(event);
}

void GraphicsView::mouseMoveEvent(QMouseEvent* event) {
	QPointF currentPosition = mapToScene(event->pos());
	emit mouseMoved(static_cast<int>(currentPosition.x()), static_cast<int>(currentPosition.y()));

	if (event->buttons() & Qt::MiddleButton) {
		const QPoint delta = event->pos() - QPoint(this->mousePosX, this->mousePosY);
		this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() - delta.x());
		this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - delta.y());

		this->mousePosX = event->x();
		this->mousePosY = event->y();

		event->accept();
		return;
	}

	QGraphicsView::mouseMoveEvent(event);
}

void GraphicsView::keyPressEvent(QKeyEvent* event) {
	switch (event->key()) {
		case Qt::Key_X:
			if (!event->isAutoRepeat()) {
				emit togglePressed();
			}
			break;

		case Qt::Key_Delete:
			emit deletePressed();
			break;

		case Qt::Key_C:
			if(event->modifiers() == Qt::ControlModifier){
				emit copyPressed();
			}
			break;

		case Qt::Key_Plus:
			zoomIn();
			break;

		case Qt::Key_Minus:
			zoomOut();
			break;

		case Qt::Key_R:
			this->rotate(90);
			break;

		case Qt::Key_V:
			if(event->modifiers() == Qt::ControlModifier){
				emit pastePressed();
				break;
			}
			// Mirror horizontal axis
			this->scale(-1, 1);
			this->vflipped = !this->vflipped;
			break;

		case Qt::Key_H:
			// Mirror on vertical axis
			this->scale(1, -1);
			this->hflipped = !this->hflipped;
			break;

		default:
			QGraphicsView::keyPressEvent(event);
	}
}

void GraphicsView::keyReleaseEvent(QKeyEvent* event) {
	switch (event->key()) {
		case Qt::Key_X:
			if (!event->isAutoRepeat()) {
				emit toggleReleased();
			}
			break;
		default:
			QGraphicsView::keyReleaseEvent(event);
	}
}

void GraphicsView::wheelEvent(QWheelEvent* event) {
	if (event->modifiers() & Qt::ControlModifier) {
		//wheel-based zoom about the cursor position
		double angle = event->angleDelta().y();
		double factor = qPow(1.0015, angle);

		QPoint targetViewportPos = event->pos();
		QPointF targetScenePos = mapToScene(event->pos());

		this->scale(factor, factor);
		this->centerOn(targetScenePos);

		QPointF deltaViewportPos = targetViewportPos - QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0);
		QPointF viewportCenter = mapFromScene(targetScenePos) - deltaViewportPos;
		this->centerOn(mapToScene(viewportCenter.toPoint()));
		return;
	}

	QGraphicsView::wheelEvent(event);
}

void GraphicsView::scaleView(qreal scaleFactor) {
	qreal factor = transform().scale(scaleFactor, scaleFactor).mapRect(QRectF(0, 0, 1, 1)).width();
	if(factor < 0.07 || factor > 100.0){
		return;
	}
	this->scale(scaleFactor, scaleFactor);
}

QString GraphicsView::getFirstFileFromUrls(const QList<QUrl>& urls) const {
	for (const QUrl& url : urls) {
		if (url.isLocalFile()) {
			return url.toLocalFile();
		}
	}
	return QString();
}

void GraphicsView::setDropHighlight(bool highlight) {
	if (this->isHighlightedForDrop == highlight) {
		return;
	}

	this->isHighlightedForDrop = highlight;

	if (highlight) {
		this->setStyleSheet("GraphicsView { border: 3px dashed #007ACC; }");
	} else {
		this->setStyleSheet("");
	}
}

void GraphicsView::dragEnterEvent(QDragEnterEvent* event) {
	if (!this->acceptFileDrops) {
		event->ignore();
		return;
	}

	if (event->mimeData()->hasUrls()) {
		QString firstFile = this->getFirstFileFromUrls(event->mimeData()->urls());
		if (!firstFile.isEmpty() && SupportedFileChecker::isValidImageFile(firstFile)) {
			event->acceptProposedAction();
			this->setDropHighlight(true);
			return;
		}
	}

	event->ignore();
}

void GraphicsView::dragMoveEvent(QDragMoveEvent* event) {
	if (!this->acceptFileDrops) {
		event->ignore();
		return;
	}

	if (event->mimeData()->hasUrls()) {
		QString firstFile = this->getFirstFileFromUrls(event->mimeData()->urls());
		if (!firstFile.isEmpty() && SupportedFileChecker::isValidImageFile(firstFile)) {
			event->acceptProposedAction();
			return;
		}
	}

	event->ignore();
}

void GraphicsView::dragLeaveEvent(QDragLeaveEvent* event) {
	this->setDropHighlight(false);
	QGraphicsView::dragLeaveEvent(event);
}

void GraphicsView::dropEvent(QDropEvent* event) {
	this->setDropHighlight(false);

	if (!this->acceptFileDrops) {
		event->ignore();
		return;
	}

	if (event->mimeData()->hasUrls()) {
		QString firstFile = this->getFirstFileFromUrls(event->mimeData()->urls());
		if (!firstFile.isEmpty()) {
			emit fileDropRequested(firstFile);
			event->acceptProposedAction();
			return;
		}
	}

	event->ignore();
}

void GraphicsView::zoomIn() {
	this->scaleView(qreal(1.2));
}

void GraphicsView::zoomOut() {
	this->scaleView(1/qreal(1.2));
}

void GraphicsView::displayFullScene() {
	this->scene->setSceneRect(this->scene->itemsBoundingRect());
	this->fitInView(this->scene->sceneRect(), Qt::KeepAspectRatio);
	this->ensureVisible(this->inputItem);
	this->centerOn(this->scene->itemsBoundingRect().center());
}

void GraphicsView::displayFrame(uchar* frame, int width, int height) {
	QImage image(frame, width, height, QImage::Format_Grayscale8);
	this->displayFrame(image);
}

void GraphicsView::displayFrame(QImage frame) {
	this->inputItem->setPixmap(QPixmap::fromImage(frame));

	//re-fit view only when the input size have changed
	const int width = frame.width();
	const int height = frame.height();

	if (this->frameWidth != width || this->frameHeight != height) {
		this->frameWidth = width;
		this->frameHeight = height;

		this->scene->setSceneRect(this->scene->itemsBoundingRect());
		this->fitInView(this->scene->sceneRect(), Qt::KeepAspectRatio);
		this->ensureVisible(this->inputItem);
		this->centerOn(this->scene->itemsBoundingRect().center());
	}
}

void GraphicsView::generateRects(int totalWidth, int totalHeight, int numberOfRectsInX, int numberOfRectsInY) {
	this->grid->generateRects(totalWidth, totalHeight, numberOfRectsInX, numberOfRectsInY);
}

void GraphicsView::setRectsVisible(bool visible) {
	this->grid->setVisible(visible);
	this->scene->update();
}

void GraphicsView::highlightSingleRect(int rectId) {
	this->grid->highlightSingleRect(rectId);
}

void GraphicsView::highlightMultipleRects(const QVector<int>& rectIds) {
	this->grid->highlightMultipleRects(rectIds);
}
