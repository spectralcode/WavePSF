#ifndef RECTITEM_H
#define RECTITEM_H

#include <QObject>
#include <QVector>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QPen>


class RectItem : public QObject, public QGraphicsRectItem
{
	Q_OBJECT
public:
	RectItem(qreal x, qreal y, qreal width, qreal height, QGraphicsItem *parent = nullptr);
	~RectItem();

private:
	QPen penActive;
	QPen penInactive;

protected:
	QPainterPath shape() const override;

public slots:
	void setRectSelected(bool selected);

signals:
	void selected(int id);

};

#endif // RECTITEM_H
