#ifndef RANGESLIDER_H
#define RANGESLIDER_H

#include <QWidget>
#include <QVector>
#include <QRgb>
#include "histogramdata.h"

class QLineEdit;

class RangeSlider : public QWidget
{
	Q_OBJECT
public:
	explicit RangeSlider(QWidget* parent = nullptr);

	void setRange(double absMin, double absMax);
	void setValues(double low, double high);
	void setGradient(const QVector<QRgb>& lut);
	void setHistogram(const HistogramData& hist);
	void clearHistogram();
	void setIntegerMode(bool intMode);

	double low() const { return this->lowValue; }
	double high() const { return this->highValue; }
	double rangeMinimum() const { return this->rangeMin; }
	double rangeMaximum() const { return this->rangeMax; }
	bool isNearHandle(int x) const;

signals:
	void valuesChanged(double low, double high);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

private:
	int valueToX(double value) const;
	double xToValue(int x) const;
	QRect barRect() const;
	QString formatValue(double v) const;
	void commitEditor(bool accept);

	double rangeMin;
	double rangeMax;
	double lowValue;
	double highValue;

	QVector<QRgb> lutColors;
	HistogramData histogram;
	bool integerMode;

	enum DragTarget { None, Low, High, Both };
	DragTarget dragging;
	double dragOffset;

	QLineEdit* activeEditor;
	DragTarget editTarget;

	static const int HANDLE_W = 8;
	static const int HANDLE_H = 18;
	static const int BAR_H = 12;
	static const int MARGIN = HANDLE_W / 2 + 2;
};

#endif // RANGESLIDER_H
