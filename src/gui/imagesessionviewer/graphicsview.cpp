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
#include <QPainter>
#include <QGraphicsLineItem>
#include <cmath>
#include "utils/supportedfilechecker.h"

static constexpr qreal LINE_GRAB_TOLERANCE_PX = 5.0;

static qreal pointToSegmentDistancePx(const QPointF& point, const QPointF& p1, const QPointF& p2)
{
	QPointF d = p2 - p1;
	qreal lenSq = d.x() * d.x() + d.y() * d.y();
	if (lenSq < 1e-12) return QLineF(point, p1).length();
	qreal t = qBound(0.0, ((point.x() - p1.x()) * d.x() + (point.y() - p1.y()) * d.y()) / lenSq, 1.0);
	QPointF closest(p1.x() + t * d.x(), p1.y() + t * d.y());
	return QLineF(point, closest).length();
}

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

	this->yPositionLine = new QGraphicsLineItem();
	this->yPositionLine->setPen(QPen(Qt::red, 1));
	this->yPositionLine->setVisible(false);
	this->scene->addItem(this->yPositionLine);
	this->yPositionLineActive = false;
	this->draggingYLine = false;

	this->viewport()->setMouseTracking(true);

	this->acceptFileDrops = false;
	this->isHighlightedForDrop = false;
	this->syncInProgress = false;
	this->syncActive = false;
	this->axisOverlayVisible = false;
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
	if (this->axisOverlayVisible) this->viewport()->update();
	emitViewTransform();
}

void GraphicsView::mouseDoubleClickEvent(QMouseEvent* event) {
	this->displayFullScene();
	QGraphicsView::mouseDoubleClickEvent(event);
}

void GraphicsView::mousePressEvent(QMouseEvent* event) {
	emit pressed();
	if (event->button() == Qt::LeftButton && this->yPositionLine->isVisible()) {
		if (this->yLineDistancePx(event->pos()) <= LINE_GRAB_TOLERANCE_PX) {
			this->draggingYLine = true;
			this->viewport()->setCursor(this->lineCursorShape());
			event->accept();
			return;
		}
	}
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

	if (this->draggingYLine && (event->buttons() & Qt::LeftButton)) {
		QPointF scenePos = mapToScene(event->pos());
		int y = qBound(0, static_cast<int>(scenePos.y()), this->frameHeight - 1);
		this->setYPositionLineY(y);
		emit yPositionLineDragged(y);
		event->accept();
		return;
	}

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

	if (this->yPositionLine->isVisible() && !(event->buttons())) {
		bool nearLine = (this->yLineDistancePx(event->pos()) <= LINE_GRAB_TOLERANCE_PX);
		if (nearLine) {
			this->viewport()->setCursor(this->lineCursorShape());
		} else {
			Qt::CursorShape cur = this->viewport()->cursor().shape();
			if (cur == Qt::SplitVCursor || cur == Qt::SplitHCursor) {
				this->viewport()->unsetCursor();
			}
		}
	}
}

void GraphicsView::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton && this->draggingYLine) {
		this->draggingYLine = false;
		this->viewport()->unsetCursor();
		event->accept();
		return;
	}
	QGraphicsView::mouseReleaseEvent(event);
}

qreal GraphicsView::yLineDistancePx(const QPoint& viewPos) const
{
	QLineF sceneLine = this->yPositionLine->line();
	QPointF p1 = mapFromScene(sceneLine.p1());
	QPointF p2 = mapFromScene(sceneLine.p2());
	return pointToSegmentDistancePx(QPointF(viewPos), p1, p2);
}

Qt::CursorShape GraphicsView::lineCursorShape() const
{
	QTransform t = this->transform();
	// Scene-X direction in view space: line is horizontal in scene
	return (std::abs(t.m12()) > std::abs(t.m11()))
		? Qt::SplitHCursor   // line appears vertical on screen
		: Qt::SplitVCursor;  // line appears horizontal on screen
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
			this->rotate90();
			break;

		case Qt::Key_V:
			if(event->modifiers() == Qt::ControlModifier){
				emit pastePressed();
				break;
			}
			this->flipV();
			break;

		case Qt::Key_H:
			this->flipH();
			break;

		case Qt::Key_Left:
		case Qt::Key_Right:
		case Qt::Key_Up:
		case Qt::Key_Down: {
			double vx = 0, vy = 0;
			if      (event->key() == Qt::Key_Left)  vx = -1;
			else if (event->key() == Qt::Key_Right) vx =  1;
			else if (event->key() == Qt::Key_Up)    vy = -1;
			else                                    vy =  1;

			bool invertible;
			QTransform inv = this->transform().inverted(&invertible);
			if (invertible) {
				double sx = inv.m11() * vx + inv.m21() * vy;
				double sy = inv.m12() * vx + inv.m22() * vy;
				int dx = 0, dy = 0;
				if (qAbs(sx) >= qAbs(sy))
					dx = (sx >= 0) ? 1 : -1;
				else
					dy = (sy >= 0) ? 1 : -1;
				emit navigatePatch(dx, dy);
			}
			event->accept();
			break;
		}

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
	connect(rotateAction, &QAction::triggered, this, &GraphicsView::rotate90);

	QAction* flipHAction = menu.addAction(tr("Flip Horizontal"));
	flipHAction->setShortcut(QKeySequence(Qt::Key_H));
	connect(flipHAction, &QAction::triggered, this, &GraphicsView::flipH);

	QAction* flipVAction = menu.addAction(tr("Flip Vertical"));
	flipVAction->setShortcut(QKeySequence(Qt::Key_V));
	connect(flipVAction, &QAction::triggered, this, &GraphicsView::flipV);

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

	if (this->yPositionLineActive) {
		menu.addSeparator();
		QAction* toggleLine = menu.addAction(tr("Show Position Line"));
		toggleLine->setCheckable(true);
		toggleLine->setChecked(this->yPositionLine->isVisible());
		connect(toggleLine, &QAction::toggled, this, [this](bool checked) {
			this->yPositionLine->setVisible(checked);
			emit yPositionLineToggled(checked);
		});
	}

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

void GraphicsView::rotate90() {
	// Post-multiply so rotation always appears CW on screen (view-space, flip-independent)
	this->setTransform(this->transform() * QTransform().rotate(-90));
	this->emitViewTransform();
}

void GraphicsView::flipH() {
	// Post-multiply so the flip always operates in view space (rotation-independent)
	this->setTransform(this->transform() * QTransform(-1, 0, 0, 1, 0, 0));
	this->hflipped = !this->hflipped;
	this->emitViewTransform();
}

void GraphicsView::flipV() {
	// Post-multiply so the flip always operates in view space (rotation-independent)
	this->setTransform(this->transform() * QTransform(1, 0, 0, -1, 0, 0));
	this->vflipped = !this->vflipped;
	this->emitViewTransform();
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

	if (this->yPositionLine->isVisible()) {
		qreal lineY = this->yPositionLine->line().y1();
		this->yPositionLine->setLine(0, lineY, width, lineY);
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

void GraphicsView::setAxisOverlayVisible(bool visible) {
	this->axisOverlayVisible = visible;
	this->viewport()->update();
}

void GraphicsView::setYPositionLineY(int y) {
	qreal lineY = static_cast<qreal>(y) + 0.5;
	this->yPositionLine->setLine(0, lineY, this->frameWidth, lineY);
}

void GraphicsView::setYPositionLineVisible(bool visible) {
	this->yPositionLineActive = visible;
	this->yPositionLine->setVisible(visible);
}

void GraphicsView::paintEvent(QPaintEvent* event) {
	QGraphicsView::paintEvent(event);

	if (!this->axisOverlayVisible) return;

	// Extract axis directions from the view transform (rotation + flip, ignoring scale)
	QTransform t = this->transform();
	QPointF xDir(t.m11(), t.m12());
	QPointF yDir(t.m21(), t.m22());
	qreal xLen = qSqrt(xDir.x() * xDir.x() + xDir.y() * xDir.y());
	qreal yLen = qSqrt(yDir.x() * yDir.x() + yDir.y() * yDir.y());
	if (xLen > 0) xDir /= xLen;
	if (yLen > 0) yDir /= yLen;

	const int axisLength = 30;
	const int margin = 20;
	const int arrowSize = 6;

	// Origin in bottom-left corner of viewport
	QPointF origin(margin + axisLength, this->viewport()->height() - margin - axisLength);
	QPointF xEnd = origin + xDir * axisLength;
	QPointF yEnd = origin + yDir * axisLength;

	QPainter painter(this->viewport());
	painter.setRenderHint(QPainter::Antialiasing);

	// Semi-transparent background circle for readability
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0, 0, 0, 100));
	painter.drawEllipse(origin, axisLength + margin, axisLength + margin);

	const QColor xColor(180, 100, 100);
	const QColor yColor(100, 160, 100);

	// X axis
	QPen xPen(xColor, 2);
	painter.setPen(xPen);
	painter.drawLine(origin, xEnd);
	// Arrowhead
	QPointF xPerp(-xDir.y(), xDir.x());
	QPolygonF xArrow;
	xArrow << xEnd << (xEnd - xDir * arrowSize + xPerp * arrowSize * 0.5)
		   << (xEnd - xDir * arrowSize - xPerp * arrowSize * 0.5);
	painter.setBrush(xColor);
	painter.drawPolygon(xArrow);

	// Y axis
	QPen yPen(yColor, 2);
	painter.setPen(yPen);
	painter.drawLine(origin, yEnd);
	// Arrowhead
	QPointF yPerp(-yDir.y(), yDir.x());
	QPolygonF yArrow;
	yArrow << yEnd << (yEnd - yDir * arrowSize + yPerp * arrowSize * 0.5)
		   << (yEnd - yDir * arrowSize - yPerp * arrowSize * 0.5);
	painter.setBrush(yColor);
	painter.drawPolygon(yArrow);

	// Labels (always upright)
	QFont font = painter.font();
	font.setBold(true);
	font.setPointSize(9);
	painter.setFont(font);
	QPointF xLabel = xEnd + xDir * 8;
	QPointF yLabel = yEnd + yDir * 8;

	painter.setPen(xColor);
	painter.drawText(QRectF(xLabel.x() - 8, xLabel.y() - 8, 16, 16),
					 Qt::AlignCenter, QStringLiteral("X"));
	painter.setPen(yColor);
	painter.drawText(QRectF(yLabel.x() - 8, yLabel.y() - 8, 16, 16),
					 Qt::AlignCenter, QStringLiteral("Y"));
}
