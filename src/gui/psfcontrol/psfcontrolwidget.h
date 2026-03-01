#ifndef PSFCONTROLWIDGET_H
#define PSFCONTROLWIDGET_H

#include <QGroupBox>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "core/psf/wavefrontparameter.h"
#include "core/psf/psfsettings.h"
#include "core/optimization/optimizationworker.h"
#include "core/interpolation/tableinterpolator.h"

class CoefficientEditorWidget;
class WavefrontPlotWidget;
class PSFPreviewWidget;
class DeconvolutionSettingsWidget;
class OptimizationWidget;
class InterpolationWidget;
class QTabWidget;
class QComboBox;

class PSFControlWidget : public QGroupBox
{
	Q_OBJECT
public:
	explicit PSFControlWidget(QWidget* parent = nullptr);
	~PSFControlWidget() override;

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);
	PSFSettings getPSFSettings() const;

public slots:
	void setParameterDescriptors(QVector<WavefrontParameter> descriptors);
	void setCoefficients(const QVector<double>& values);
	void updateWavefront(af::array wavefront);
	void updatePSF(af::array psf);
	void setPSFSettings(const PSFSettings& settings);
	void setGeneratorType(const QString& typeName);
	void setGroundTruthAvailable(bool available);
	void updateOptimizationProgress(const OptimizationProgress& progress);
	void onOptimizationFinished(const OptimizationResult& result);
	void onOptimizationStarted();
	void updateInterpolationResult(const InterpolationResult& result);

signals:
	void coefficientChanged(int id, double value);
	void resetRequested();
	void generatorTypeChangeRequested(QString typeName);

	// Deconvolution settings signals (forwarded from DeconvolutionSettingsWidget)
	void deconvAlgorithmChanged(int algorithm);
	void deconvIterationsChanged(int iterations);
	void deconvRelaxationFactorChanged(float factor);
	void deconvRegularizationFactorChanged(float factor);
	void deconvNoiseToSignalFactorChanged(float factor);
	void deconvLiveModeChanged(bool enabled);
	void deconvolutionRequested();

	// Optimization signals (forwarded from OptimizationWidget)
	void optimizationRequested(OptimizationConfig config);
	void optimizationCancelRequested();
	void optimizationPatchSelectionChanged(QVector<int> patchLinearIds);
	void optimizationLivePreviewChanged(bool enabled, int interval);
	void optimizationSAParametersChanged(double endTemp, double coolingFactor,
										 double startPerturb, double endPerturb,
										 int itersPerTemp);

	// Interpolation signals (forwarded from InterpolationWidget)
	void interpolateInXRequested();
	void interpolateInYRequested();
	void interpolateInZRequested();
	void interpolateAllInZRequested();
	void interpolationPolynomialOrderChanged(int order);

private:
	QTabWidget* tabWidget;
	QComboBox* generatorTypeCombo;
	CoefficientEditorWidget* coeffEditor;
	WavefrontPlotWidget* wavefrontPlot;
	PSFPreviewWidget* psfPreview;
	DeconvolutionSettingsWidget* deconvSettings;
	OptimizationWidget* optimizationWidget;
	InterpolationWidget* interpolationWidget;
	PSFSettings currentSettings;
};

#endif // PSFCONTROLWIDGET_H
