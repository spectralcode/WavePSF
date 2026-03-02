#ifndef INTERPOLATIONWIDGET_H
#define INTERPOLATIONWIDGET_H

#include <QWidget>
#include "core/interpolation/tableinterpolator.h"

class QPushButton;
class QSpinBox;
class QLabel;
class QCustomPlot;

class InterpolationWidget : public QWidget
{
	Q_OBJECT
public:
	explicit InterpolationWidget(QWidget* parent = nullptr);
	~InterpolationWidget() override;

public slots:
	void updateInterpolationResult(const InterpolationResult& result);

signals:
	void interpolateInXRequested();
	void interpolateInYRequested();
	void interpolateInZRequested();
	void interpolateAllInZRequested();
	void polynomialOrderChanged(int order);

private slots:
	void onCoefficientSelectionChanged(int index);
	void showPlotContextMenu(const QPoint& pos);

private:
	void setupUI();
	void updatePlot();
	void installScrollGuard(QWidget* widget);
	bool eventFilter(QObject* obj, QEvent* event) override;

	// Controls
	QSpinBox* polynomialOrderSpinBox;
	QPushButton* interpolateXButton;
	QPushButton* interpolateYButton;
	QPushButton* interpolateZButton;
	QPushButton* interpolateAllZButton;
	QSpinBox* coefficientSpinBox;
	QLabel* statusLabel;

	// Plot
	QCustomPlot* plot;

	// Cached result
	InterpolationResult lastResult;
	bool hasResult;
};

#endif // INTERPOLATIONWIDGET_H
