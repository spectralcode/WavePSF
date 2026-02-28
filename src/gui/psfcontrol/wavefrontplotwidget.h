#ifndef WAVEFRONTPLOTWIDGET_H
#define WAVEFRONTPLOTWIDGET_H

#include <QWidget>
#include <arrayfire.h>

class QCustomPlot;
class QCPColorMap;
class QCPColorScale;
class QComboBox;

class WavefrontPlotWidget : public QWidget
{
	Q_OBJECT
public:
	explicit WavefrontPlotWidget(QWidget* parent = nullptr);
	~WavefrontPlotWidget() override;

public slots:
	void updatePlot(af::array wavefront);

private slots:
	void resetView();
	void enforceAspectRatio();
	void applyGradient(int index);

private:
	void setupGradientCombo();
	void applyBlueWhiteRedGradient();
	void applyCustomGradient();

	QCustomPlot* plot;
	QCPColorMap* colorMap;
	QCPColorScale* colorScale;
	QComboBox* gradientCombo;
};

#endif // WAVEFRONTPLOTWIDGET_H
