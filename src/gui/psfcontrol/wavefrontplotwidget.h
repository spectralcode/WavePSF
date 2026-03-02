#ifndef WAVEFRONTPLOTWIDGET_H
#define WAVEFRONTPLOTWIDGET_H

#include <QWidget>
#include <arrayfire.h>

class QCustomPlot;
class QCPColorMap;
class QCPColorScale;
class QComboBox;
class QAction;
class QMenu;

class WavefrontPlotWidget : public QWidget
{
	Q_OBJECT
public:
	explicit WavefrontPlotWidget(QWidget* parent = nullptr);
	~WavefrontPlotWidget() override;

public slots:
	void updatePlot(af::array wavefront);

protected:
	void showEvent(QShowEvent* event) override;

private slots:
	void resetView();
	void enforceAspectRatio();
	void applyGradient(int index);
	void showContextMenu(const QPoint& pos);

private:
	void setupGradientCombo();
	void setupContextMenu();
	void applyBlueWhiteRedGradient();
	void applyWavefrontGradient();
	void applySeismicGradient();
	void applyDataRange();

	QCustomPlot* plot;
	QCPColorMap* colorMap;
	QCPColorScale* colorScale;
	QComboBox* gradientCombo;

	QMenu* contextMenu;
	QAction* autoScaleAction;
	QAction* symmetricZeroAction;
	QAction* showGridAction;
	QAction* showAxisAction;
};

#endif // WAVEFRONTPLOTWIDGET_H
