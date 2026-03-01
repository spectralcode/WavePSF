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

	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void contextMenuEvent(QContextMenuEvent* event) override;
	void scaleView(qreal scaleFactor);

	QString getFirstFileFromUrls(const QList<QUrl>& urls) const;
	void setDropHighlight(bool highlight);

protected:
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dragLeaveEvent(QDragLeaveEvent* event) override;
	void dropEvent(QDropEvent* event) override;

public slots:
	void zoomIn();
	void zoomOut();
	void displayFullScene();
	void displayFrame(uchar* frame, int width, int height);
	void displayFrame(QImage frame);
	void generateRects(int totalWidth, int totalHeight, int numberOfRectsInX, int numberOfRectsInY);
	void setRectsVisible(bool visible);
	void highlightSingleRect(int rectId);
	void highlightMultipleRects(const QVector<int>& rectIds);

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
	void fileDropRequested(const QString& filePath);
};

#endif // GRAPHICSVIEW_H
