#include "verticalscrollarea.h"
#include <QScrollBar>
#include <QEvent>

VerticalScrollArea::VerticalScrollArea(QWidget* parent)
	: QScrollArea(parent)
{
	this->setWidgetResizable(true);
	this->setFrameShape(QFrame::NoFrame);
	this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

QSize VerticalScrollArea::sizeHint() const
{
	int w = 0;
	if (this->widget()) {
		w = this->widget()->sizeHint().width();
	}
	if (this->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
		w += this->verticalScrollBar()->sizeHint().width();
	}
	return QSize(w, QScrollArea::sizeHint().height());
}

QSize VerticalScrollArea::minimumSizeHint() const
{
	int w = 0;
	if (this->widget()) {
		w = this->widget()->minimumSizeHint().width();
	}
	if (this->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
		w += this->verticalScrollBar()->sizeHint().width();
	}
	return QSize(w, QScrollArea::minimumSizeHint().height());
}

bool VerticalScrollArea::event(QEvent* e)
{
	if (e->type() == QEvent::StyleChange) {
		this->updateGeometry();
	}
	return QScrollArea::event(e);
}
