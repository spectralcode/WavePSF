#ifndef PSFCONTROLWIDGET_H
#define PSFCONTROLWIDGET_H

#include <QGroupBox>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "core/psf/wavefrontparameter.h"
#include "core/psf/psfsettings.h"

class CoefficientEditorWidget;
class WavefrontPlotWidget;
class PSFPreviewWidget;
class DeconvolutionSettingsWidget;
class QTabWidget;

class PSFControlWidget : public QGroupBox
{
	Q_OBJECT
public:
	explicit PSFControlWidget(QWidget* parent = nullptr);
	~PSFControlWidget() override;

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

public slots:
	void setParameterDescriptors(QVector<WavefrontParameter> descriptors);
	void setCoefficients(const QVector<double>& values);
	void updateWavefront(af::array wavefront);
	void updatePSF(af::array psf);
	void setPSFSettings(const PSFSettings& settings);

signals:
	void coefficientChanged(int id, double value);
	void resetRequested();

	// Deconvolution settings signals (forwarded from DeconvolutionSettingsWidget)
	void deconvAlgorithmChanged(int algorithm);
	void deconvIterationsChanged(int iterations);
	void deconvRelaxationFactorChanged(float factor);
	void deconvRegularizationFactorChanged(float factor);
	void deconvNoiseToSignalFactorChanged(float factor);
	void deconvLiveModeChanged(bool enabled);
	void deconvolutionRequested();

private:
	QTabWidget* tabWidget;
	CoefficientEditorWidget* coeffEditor;
	WavefrontPlotWidget* wavefrontPlot;
	PSFPreviewWidget* psfPreview;
	DeconvolutionSettingsWidget* deconvSettings;
	PSFSettings currentSettings;
};

#endif // PSFCONTROLWIDGET_H
