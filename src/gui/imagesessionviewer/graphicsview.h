#ifndef GRAPHICSVIEW_H
#define GRAPHICSVIEW_H

#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QObject>
#include "rectitemgroup.h"

class GraphicsView : public QGraphicsView
{
	Q_OBJECT
public:
	explicit GraphicsView(QWidget* parent = nullptr);
	~GraphicsView();

	void enableFileDrops(bool enable = true);
	void refreshForStyleChange();

private:
	QGraphicsScene* scene;
	QGraphicsPixmapItem* inputItem;
	RectItemGroup* grid;
	int frameWidth;
	int frameHeight;
	int mousePosX;
	int mousePosY;
	bool hflipped;
	bool vflipped;
	bool acceptFileDrops;
	bool isHighlightedForDrop;
	bool syncInProgress;
	bool syncActive;
	bool axisOverlayVisible;
	QGraphicsLineItem* yPositionLine;
	bool yPositionLineActive;
	bool draggingYLine;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void contextMenuEvent(QContextMenuEvent* event) override;
	void scaleView(qreal scaleFactor);

	QString getFirstFileFromUrls(const QList<QUrl>& urls) const;
	void setDropHighlight(bool highlight);

protected:
	void paintEvent(QPaintEvent* event) override;
	void scrollContentsBy(int dx, int dy) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dragLeaveEvent(QDragLeaveEvent* event) override;
	void dropEvent(QDropEvent* event) override;

public slots:
	void zoomIn();
	void zoomOut();
	void rotate90();
	void flipH();
	void flipV();
	void emitViewTransform();
	void displayFullScene();
	void displayFrame(uchar* frame, int width, int height);
	void displayFrame(QImage frame);
	void generateRects(const PatchLayout& layout);
	void setRectsVisible(bool visible);
	void highlightSingleRect(int rectId);
	void highlightMultipleRects(const QVector<int>& rectIds);
	void applyViewTransform(QTransform transform, QPointF centerInScene);
	void setSyncActive(bool active);
	void setAxisOverlayVisible(bool visible);
	void setYPositionLineY(int y);
	void setYPositionLineVisible(bool visible);

signals:
	void info(QString);
	void error(QString);
	void mouseMiddleButtonPressed(int x, int y);
	void mouseMoved(int x, int y);
	void rectangleSelectionChanged(int id);
	void gridGenerated(QVector<RectItem*>);
	void deletePressed();
	void copyPressed();
	void pastePressed();
	void togglePressed();
	void toggleReleased();
	void pressed();
	void fileDropRequested(const QString& filePath);
	void viewTransformChanged(QTransform transform, QPointF centerInScene);
	void navigatePatch(int dx, int dy);
	void yPositionLineDragged(int y);
	void yPositionLineToggled(bool visible);
};

#endif // GRAPHICSVIEW_H
