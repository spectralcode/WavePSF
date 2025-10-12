#include "rectitem.h"


RectItem::RectItem(qreal x, qreal y, qreal width, qreal height, QGraphicsItem *parent) : QObject(0), QGraphicsRectItem(x, y, width, height, parent)
{
	this->setAcceptHoverEvents(true);
	this->penActive = QPen(QColor(255, 0, 0, 127), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	this->penInactive = QPen(QColor(127, 127, 127, 127), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	this->setPen(this->penInactive);
	this->setFlag(QGraphicsItem::ItemIsSelectable, true);
	this->setFlag(QGraphicsItem::ItemIsMovable, false);
}

RectItem::~RectItem() {

}

QPainterPath RectItem::shape() const
{
	//info: this reimplementation of shape() is necessary so that only a single rect is selected when the boundary between two rects is clicked
	QPainterPath path;
	int penWidth = this->pen().width();
	int x = this->boundingRect().x();
	int y = this->boundingRect().y();
	int width = this->boundingRect().width();
	int height = this->boundingRect().height();
	path.addRect(x, y, width-penWidth, height-penWidth);
	return path;
}

void RectItem::setRectSelected(bool selected)
{
	if(selected){
		this->setPen(this->penActive);
	} else {
		this->setPen(this->penInactive);
	}
}
