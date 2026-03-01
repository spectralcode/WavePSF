#ifndef QCPPALETTEOBSERVER_H
#define QCPPALETTEOBSERVER_H

#include <QObject>
#include <QEvent>
#include <QPalette>
#include <QPen>
#include "qcustomplot.h"

// Watches a QCustomPlot for palette changes and maps the Qt palette
// to QCustomPlot's explicit color properties (background, axes, grid, etc.).
// Usage: new QCPPaletteObserver(plot);  // parent = plot, auto-deleted
class QCPPaletteObserver : public QObject
{
	Q_OBJECT
public:
	explicit QCPPaletteObserver(QCustomPlot* plot)
		: QObject(plot)
		, plot(plot)
	{
		plot->installEventFilter(this);
		this->applyTheme();
	}

protected:
	bool eventFilter(QObject* obj, QEvent* event) override
	{
		if (obj == this->plot && event->type() == QEvent::PaletteChange) {
			this->applyTheme();
		}
		return QObject::eventFilter(obj, event);
	}

private:
	void applyTheme()
	{
		QPalette pal = this->plot->palette();
		QColor base = pal.color(QPalette::Base);
		QColor text = pal.color(QPalette::Text);
		QColor mid = QColor(
			(base.red() + text.red()) / 2,
			(base.green() + text.green()) / 2,
			(base.blue() + text.blue()) / 2);

		// Plot backgrounds
		this->plot->setBackground(QBrush(base));
		this->plot->axisRect()->setBackground(QBrush(base));

		// All four axes
		QList<QCPAxis*> axes;
		axes << this->plot->xAxis << this->plot->yAxis
			 << this->plot->xAxis2 << this->plot->yAxis2;
		for (QCPAxis* axis : axes) {
			axis->setTickLabelColor(text);
			axis->setLabelColor(text);
			axis->setBasePen(QPen(text, 1));
			axis->setTickPen(QPen(text, 1));
			axis->setSubTickPen(QPen(text, 1));
			axis->grid()->setPen(QPen(mid, 1, Qt::DotLine));
			axis->grid()->setZeroLinePen(QPen(mid, 1));
		}

		// Color scale (if any)
		for (int i = 0; i < this->plot->plotLayout()->elementCount(); ++i) {
			QCPColorScale* cs = qobject_cast<QCPColorScale*>(
				this->plot->plotLayout()->elementAt(i));
			if (cs) {
				QCPAxis* csAxis = cs->axis();
				csAxis->setTickLabelColor(text);
				csAxis->setLabelColor(text);
				csAxis->setBasePen(QPen(text, 1));
				csAxis->setTickPen(QPen(text, 1));
				csAxis->setSubTickPen(QPen(text, 1));
			}
		}

		// Legend (if visible)
		if (this->plot->legend && this->plot->legend->visible()) {
			this->plot->legend->setTextColor(text);
			this->plot->legend->setBorderPen(QPen(mid, 1));
			this->plot->legend->setBrush(QBrush(base));
		}

		this->plot->replot();
	}

	QCustomPlot* plot;
};

#endif // QCPPALETTEOBSERVER_H
