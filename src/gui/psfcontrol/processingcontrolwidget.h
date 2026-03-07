#ifndef PROCESSINGCONTROLWIDGET_H
#define PROCESSINGCONTROLWIDGET_H

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

class ProcessingControlWidget : public QWidget
{
	Q_OBJECT
public:
	explicit ProcessingControlWidget(QWidget* parent = nullptr);
	~ProcessingControlWidget() override;

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

public slots:
	void setParameterDescriptors(QVector<WavefrontParameter> descriptors);
	void setGroundTruthAvailable(bool available);
	void setCurrentFrame(int frame);
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
	void optimizationAlgorithmParametersChanged(QVariantMap params);

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

#endif // PROCESSINGCONTROLWIDGET_H
