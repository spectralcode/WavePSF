#ifndef RECTITEMGROUP_H
#define RECTITEMGROUP_H

#include <QObject>
#include <QVector>
#include <QGraphicsItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QPen>
#include "rectitem.h"


class RectItemGroup : public QObject, public QGraphicsItemGroup
{
	Q_OBJECT
public:
	RectItemGroup(QGraphicsItem *parent = nullptr);
	~RectItemGroup();
	QVector<QPointF> getVertices();


private:
	QVector<RectItem*> rectItems;
	int selectedId;

	void removeAndDeleteAllItems();


protected:
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

public slots:
	void generateRects(int totalWidth, int totalHeight, int numberOfRectsInX, int numberOfRectsInY);
	void highlightSingleRect(int rectId);
	void highlightMultipleRects(const QVector<int>& rectIds);

signals:
	void error(QString);
	void info(QString);
	void selectionChanged(int id);
	void gridGenerated(QVector<RectItem*>);

};

#endif // RECTITEMGROUP_H
