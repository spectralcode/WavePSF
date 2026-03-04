#include "rectitemgroup.h"


RectItemGroup::RectItemGroup(QGraphicsItem *parent) : QObject(0), QGraphicsItemGroup(parent), selectedId(-1)
{
	this->setAcceptHoverEvents(true);
	this->setFlag(QGraphicsItem::ItemIsMovable, false);
	qRegisterMetaType<QVector<RectItem*>>("QVector<RectItem*>");
}

RectItemGroup::~RectItemGroup() = default;

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



void RectItemGroup::generateRects(const PatchLayout& layout)
{
	this->removeAndDeleteAllItems();

	for (int row = 0; row < layout.rows; row++) {
		for (int col = 0; col < layout.cols; col++) {
			const QRect b = layout.patchBounds(col, row);
			RectItem* newRectItem = new RectItem(b.x(), b.y(), b.width(), b.height(), this);
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
		this->selectedId = rectId;
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




