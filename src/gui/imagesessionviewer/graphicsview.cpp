#include "graphicsview.h"

#include <QKeyEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
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
	this->syncInProgress = false;
	this->syncActive = false;
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

void GraphicsView::emitViewTransform() {
	if (!this->syncInProgress && this->syncActive) {
		QPointF center = mapToScene(viewport()->rect().center());
		emit viewTransformChanged(this->transform(), center);
	}
}

void GraphicsView::setSyncActive(bool active) {
	this->syncActive = active;
}

void GraphicsView::applyViewTransform(QTransform t, QPointF center) {
	this->syncInProgress = true;
	this->setTransform(t);
	this->centerOn(center);
	this->syncInProgress = false;
}

void GraphicsView::scrollContentsBy(int dx, int dy) {
	QGraphicsView::scrollContentsBy(dx, dy);
	emitViewTransform();
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
			emitViewTransform();
			break;

		case Qt::Key_V:
			if(event->modifiers() == Qt::ControlModifier){
				emit pastePressed();
				break;
			}
			// Mirror horizontal axis
			this->scale(-1, 1);
			this->vflipped = !this->vflipped;
			emitViewTransform();
			break;

		case Qt::Key_H:
			// Mirror on vertical axis
			this->scale(1, -1);
			this->hflipped = !this->hflipped;
			emitViewTransform();
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

void GraphicsView::contextMenuEvent(QContextMenuEvent* event) {
	QMenu menu(this);

	// Coefficient operations
	QAction* copyAction = menu.addAction(tr("Copy Coefficients"));
	copyAction->setShortcut(QKeySequence::Copy);
	connect(copyAction, &QAction::triggered, this, &GraphicsView::copyPressed);

	QAction* pasteAction = menu.addAction(tr("Paste Coefficients"));
	pasteAction->setShortcut(QKeySequence::Paste);
	connect(pasteAction, &QAction::triggered, this, &GraphicsView::pastePressed);

	QAction* resetAction = menu.addAction(tr("Reset Coefficients"));
	resetAction->setShortcut(QKeySequence(Qt::Key_Delete));
	connect(resetAction, &QAction::triggered, this, &GraphicsView::deletePressed);

	menu.addSeparator();

	// View transform operations
	QAction* rotateAction = menu.addAction(tr("Rotate 90%1").arg(QChar(0x00B0)));
	rotateAction->setShortcut(QKeySequence(Qt::Key_R));
	connect(rotateAction, &QAction::triggered, this, [this]() {
		this->rotate(90);
		emitViewTransform();
	});

	QAction* flipHAction = menu.addAction(tr("Flip Horizontal"));
	flipHAction->setShortcut(QKeySequence(Qt::Key_H));
	connect(flipHAction, &QAction::triggered, this, [this]() {
		this->scale(1, -1);
		this->hflipped = !this->hflipped;
		emitViewTransform();
	});

	QAction* flipVAction = menu.addAction(tr("Flip Vertical"));
	flipVAction->setShortcut(QKeySequence(Qt::Key_V));
	connect(flipVAction, &QAction::triggered, this, [this]() {
		this->scale(-1, 1);
		this->vflipped = !this->vflipped;
		emitViewTransform();
	});

	menu.addSeparator();

	// Zoom operations
	QAction* zoomInAction = menu.addAction(tr("Zoom In"));
	zoomInAction->setShortcut(QKeySequence(Qt::Key_Plus));
	connect(zoomInAction, &QAction::triggered, this, &GraphicsView::zoomIn);

	QAction* zoomOutAction = menu.addAction(tr("Zoom Out"));
	zoomOutAction->setShortcut(QKeySequence(Qt::Key_Minus));
	connect(zoomOutAction, &QAction::triggered, this, &GraphicsView::zoomOut);

	QAction* fitAction = menu.addAction(tr("Fit to View"));
	connect(fitAction, &QAction::triggered, this, &GraphicsView::displayFullScene);

	menu.exec(event->globalPos());
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

void GraphicsView::generateRects(const PatchLayout& layout) {
	this->grid->generateRects(layout);
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
