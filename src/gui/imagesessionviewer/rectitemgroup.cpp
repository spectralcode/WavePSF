#include "rectitemgroup.h"


RectItemGroup::RectItemGroup(QGraphicsItem *parent) : QObject(0), QGraphicsItemGroup(parent)
{
	this->setAcceptHoverEvents(true);
	this->setFlag(QGraphicsItem::ItemIsMovable, false);
	qRegisterMetaType<QVector<RectItem*>>("QVector<RectItem*>");
}

RectItemGroup::~RectItemGroup() {
//	foreach(auto item,this->rectItems) {
//		if(item != nullptr){
//			delete item;
//		}
//	}
}

void RectItemGroup::removeAndDeleteAllItems()
{
	foreach(auto item,this->rectItems) {
		this->removeFromGroup(item);
		delete(item);
	}
	this->rectItems.clear();
}

void RectItemGroup::mousePressEvent(QGraphicsSceneMouseEvent* event) {
	QGraphicsItem::mousePressEvent(event);
	if (event->button() == Qt::LeftButton && this->isVisible()) {
		foreach(auto item,this->rectItems) {
			if(item->contains(event->pos())){
				item->setRectSelected(true);
				int id = this->rectItems.indexOf(item);
				if(this->selectedId != id){
					this->selectedId = id;
					emit selectionChanged(this->selectedId);
				}
			} else {
				item->setRectSelected(false);
			}
		}
	}
}



void RectItemGroup::generateRects(int totalWidth, int totalHeight, int numberOfRectsInX, int numberOfRectsInY)
{
	int width = totalWidth / numberOfRectsInX;
	int height = totalHeight / numberOfRectsInY;
	int xPos = 0;
	int yPos = 0;

	this->removeAndDeleteAllItems();

	for(int y = 0; y < numberOfRectsInY; y++) {
		for(int x = 0; x < numberOfRectsInX; x++) {
			xPos = x*width;
			yPos = y*height;
			RectItem* newRectItem = new RectItem(xPos, yPos, width, height, this);
			this->rectItems.append(newRectItem);
			this->addToGroup(newRectItem);
		}
	}
	emit gridGenerated(this->rectItems);
}

void RectItemGroup::highlightSingleRect(int rectId)
{
	if(rectId >= 0 && rectId < this->rectItems.size()){
		foreach(auto item,this->rectItems) {
			item->setRectSelected(false);
		}
		this->rectItems.at(rectId)->setRectSelected(true);
	}
}

void RectItemGroup::highlightMultipleRects(const QVector<int>& rectIds)
{
	foreach(auto item, this->rectItems) {
		item->setRectSelected(false);
	}
	for (int rectId : rectIds) {
		if (rectId >= 0 && rectId < this->rectItems.size()) {
			this->rectItems.at(rectId)->setRectSelected(true);
		}
	}
}




