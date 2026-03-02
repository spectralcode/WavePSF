#ifndef PSFCONTROLWIDGET_H
#define PSFCONTROLWIDGET_H

#include <QWidget>
#include <QVector>
#include <QVariantMap>
#include "core/psf/wavefrontparameter.h"
#include "core/optimization/optimizationworker.h"
#include "core/interpolation/tableinterpolator.h"

class DeconvolutionSettingsWidget;
class OptimizationWidget;
class InterpolationWidget;
class QTabWidget;
class QLabel;
class QSpinBox;

class PSFControlWidget : public QWidget
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
	void setGroundTruthAvailable(bool available);
	void updateOptimizationProgress(const OptimizationProgress& progress);
	void onOptimizationFinished(const OptimizationResult& result);
	void onOptimizationStarted();
	void updateInterpolationResult(const InterpolationResult& result);
	void updateCurrentPatch(int x, int y);
	void setPatchGridConfiguration(int cols, int rows, int borderExtension);

signals:
	// Deconvolution settings signals
	void deconvAlgorithmChanged(int algorithm);
	void deconvIterationsChanged(int iterations);
	void deconvRelaxationFactorChanged(float factor);
	void deconvRegularizationFactorChanged(float factor);
	void deconvNoiseToSignalFactorChanged(float factor);
	void deconvLiveModeChanged(bool enabled);
	void deconvolutionRequested();

	// Optimization signals
	void optimizationRequested(OptimizationConfig config);
	void optimizationCancelRequested();
	void optimizationPatchSelectionChanged(QVector<int> patchLinearIds);
	void optimizationLivePreviewChanged(bool enabled, int interval);
	void optimizationSAParametersChanged(double endTemp, double coolingFactor,
										 double startPerturb, double endPerturb,
										 int itersPerTemp);

	// Interpolation signals
	void interpolateInXRequested();
	void interpolateInYRequested();
	void interpolateInZRequested();
	void interpolateAllInZRequested();
	void interpolationPolynomialOrderChanged(int order);

	// Patch grid signals
	void patchGridConfigurationRequested(int cols, int rows, int borderExtension);

private:
	void applyPatchGridSettings();

	QTabWidget* tabWidget;
	DeconvolutionSettingsWidget* deconvSettings;
	OptimizationWidget* optimizationWidget;
	InterpolationWidget* interpolationWidget;

	// Patch grid controls
	QLabel* patchGridInfoLabel;
	QSpinBox* patchColsSpinBox;
	QSpinBox* patchRowsSpinBox;
	QSpinBox* borderExtensionSpinBox;
	bool updatingPatchGrid;
};

#endif // PSFCONTROLWIDGET_H
