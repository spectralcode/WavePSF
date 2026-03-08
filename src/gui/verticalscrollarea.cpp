#include "verticalscrollarea.h"
#include <QScrollBar>
#include <QEvent>
#include <QTimer>

VerticalScrollArea::VerticalScrollArea(QWidget* parent)
	: QScrollArea(parent)
{
	this->setWidgetResizable(true);
	this->setFrameShape(QFrame::NoFrame);
	this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	this->viewport()->installEventFilter(this);
}

QSize VerticalScrollArea::sizeHint() const
{
	int w = 0;
	if (this->widget()) {
		w = this->widget()->sizeHint().width();
	}
	if (this->verticalScrollBar()->isVisible()) {
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
	if (this->verticalScrollBar()->isVisible()) {
		w += this->verticalScrollBar()->sizeHint().width();
	}
	return QSize(w, QScrollArea::minimumSizeHint().height());
}

bool VerticalScrollArea::event(QEvent* e)
{
	if (e->type() == QEvent::StyleChange) {
		// Defer so child widgets finish processing their style change first
		QTimer::singleShot(0, this, [this]() { this->updateGeometry(); });
	}
	return QScrollArea::event(e);
}

bool VerticalScrollArea::eventFilter(QObject* obj, QEvent* e)
{
	if (obj == this->viewport() && e->type() == QEvent::LayoutRequest) {
		this->updateGeometry();
	}
	return QScrollArea::eventFilter(obj, e);
}
