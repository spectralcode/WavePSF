#ifndef WAVEFRONTPLOTWIDGET_H
#define WAVEFRONTPLOTWIDGET_H

#include <QWidget>
#include <QVariantMap>
#include <arrayfire.h>

class QCustomPlot;
class QCPColorMap;
class QCPColorScale;
class QAction;
class QActionGroup;
class QMenu;

class WavefrontPlotWidget : public QWidget
{
	Q_OBJECT
public:
	explicit WavefrontPlotWidget(QWidget* parent = nullptr);
	~WavefrontPlotWidget() override;

	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

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
	void setupContextMenu();
	void applyBlueWhiteRedGradient();
	void applyWavefrontGradient();
	void applySeismicGradient();
	void applyDataRange();

	QCustomPlot* plot;
	QCPColorMap* colorMap;
	QCPColorScale* colorScale;

	QMenu* contextMenu;
	QActionGroup* gradientGroup;
	QAction* autoScaleAction;
	QAction* symmetricZeroAction;
	QAction* showGridAction;
	QAction* showAxisAction;
	QAction* showColorScaleAction;
};

#endif // WAVEFRONTPLOTWIDGET_H
