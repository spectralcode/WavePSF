#ifndef APPLICATIONCONTROLLER_H
#define APPLICATIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QThread>
#include <QElapsedTimer>
#include <QMap>
#include <QPair>
#include <arrayfire.h>
#include "core/psf/wavefrontparameter.h"
#include "core/psf/psfsettings.h"
#include "core/optimization/optimizationworker.h"
#include "core/interpolation/tableinterpolator.h"

// Forward declarations
class ImageSession;
class InputDataReader;
class ImageData;
class SettingsFileManager;
class PSFModule;
class WavefrontParameterTable;
class TableInterpolator;

class ApplicationController : public QObject
{
	Q_OBJECT

public:
	explicit ApplicationController(QObject* parent = nullptr);
	~ApplicationController();

	// Direct session access for modules
	ImageSession* getImageSession() const;

	// Session information access (for compatibility)
	bool hasInputData() const;
	bool hasOutputData() const;
	bool hasGroundTruthData() const;

	int getCurrentFrame() const;
	int getCurrentPatchX() const;
	int getCurrentPatchY() const;
	int getPatchGridCols() const;
	int getPatchGridRows() const;
	int getPatchBorderExtension() const;

	// Data dimensions
	int getInputWidth() const;
	int getInputHeight() const;
	int getInputFrames() const;

	// Broadcast current state for initialization
	void broadcastCurrentState();

public slots:
	// File loading with error handling
	void requestOpenInputFile(const QString& filePath);
	void requestOpenGroundTruthFile(const QString& filePath);

	// Session state management - slots for GUI widgets
	void setCurrentFrame(int frame);
	void setCurrentPatch(int x, int y);
	void configurePatchGrid(int cols, int rows, int borderExtension = 0);

	// PSF pipeline - slots for GUI widgets
	void setPSFCoefficient(int id, double value);
	void resetPSFCoefficients();

	// Parameter file I/O
	void saveParametersToFile(const QString& filePath);
	void loadParametersFromFile(const QString& filePath);

	// PSF settings
	void applyPSFSettings(const PSFSettings& settings);

	// Generator type switching
	void setGeneratorType(const QString& typeName);

	// Deconvolution settings - slots for GUI widgets
	void setDeconvolutionAlgorithm(int algorithm);
	void setDeconvolutionIterations(int iterations);
	void setDeconvolutionRelaxationFactor(float factor);
	void setDeconvolutionRegularizationFactor(float factor);
	void setDeconvolutionNoiseToSignalFactor(float factor);
	void setDeconvolutionLiveMode(bool enabled);
	void requestDeconvolution();

	// Optimization
	void startOptimization(const OptimizationConfig& uiConfig);
	void cancelOptimization();
	void updateOptimizationLivePreview(bool enabled, int interval);
	void updateOptimizationAlgorithmParameters(const QVariantMap& params);

	// Coefficient clipboard
	void copyCoefficients();
	void pasteCoefficients();

	// File output
	void saveOutputToFile(const QString& filePath);

	// PSF file I/O
	void loadPSFFromFile(const QString& filePath);
	void savePSFToFile(const QString& filePath);
	void setAutoSavePSF(bool enabled);
	void setPSFSaveFolder(const QString& folder);
	void setUseCustomPSFFolder(bool enabled);
	void setCustomPSFFolder(const QString& folder);

	// Batch processing
	void requestBatchDeconvolution();

	// Interpolation
	void interpolateCoefficientsInX();
	void interpolateCoefficientsInY();
	void interpolateCoefficientsInZ();
	void interpolateAllCoefficientsInZ();
	void setInterpolationPolynomialOrder(int order);

private slots:
	// Handle data changes that require session broadcast
	void handleInputDataChanged();
	void handleOutputDataChanged();
	void handleGroundTruthDataChanged();

	// Live deconvolution triggers
	void handlePSFUpdatedForDeconvolution(af::array psf);
	void handleDeconvolutionSettingsChanged();

	// Optimization handling
	void handleOptimizationProgress(const OptimizationProgress& progress);
	void handleOptimizationFinished(const OptimizationResult& result);

private:
	// File operations
	bool openInputFile(const QString& filePath);
	bool openGroundTruthFile(const QString& filePath);

	void initializeComponents();
	void connectSessionSignals();
	void connectPSFModuleSignals();
	void connectDeconvolutionSignals();
	bool loadFileToSession(const QString& filePath, bool isGroundTruth);

	// Deconvolution orchestration
	void runDeconvolutionOnCurrentPatch();

	// Optimization orchestration
	void initializeOptimizationThread();
	bool buildOptimizationJobs(OptimizationConfig& config);

	// Parameter table orchestration
	void storeCurrentCoefficients();
	void loadCoefficientsForCurrentPatch();
	void resizeParameterTable();

	// Core components
	ImageSession* imageSession;
	InputDataReader* inputDataReader;
	PSFModule* psfModule;
	WavefrontParameterTable* parameterTable;

	// Deconvolution state
	bool deconvolutionLiveMode;

	// Optimization threading
	QThread* optimizationThread;
	OptimizationWorker* optimizationWorker;

	// Optimization live preview state
	bool optimizationLivePreview;
	int optimizationLivePreviewInterval;
	int optimizationProgressCounter;
	QElapsedTimer progressUpdateTimer;
	bool suppressLiveDeconv;

	// Coefficient clipboard
	QVector<double> coefficientClipboard;

	// Interpolation
	TableInterpolator* tableInterpolator;

	// PSF file management
	bool autoSavePSFEnabled;
	QString psfSaveFolder;
	bool useCustomPSFFolder;
	QString customPSFFolder;
	QMap<QPair<int,int>, af::array> externalPSFOverrides;  // (frame, patchIdx) → loaded PSF

	// Generator state cache: preserves settings when switching between generator types
	QMap<QString, QVariantMap> cachedGeneratorSettings;
	QMap<QString, WavefrontParameterTable*> cachedParameterTables;

signals:
	// File loading results
	void inputFileLoaded(const QString& filePath);
	void groundTruthFileLoaded(const QString& filePath);
	void fileLoadError(const QString& filePath, const QString& error);

	// Session state changes - emitted for GUI widgets
	void frameChanged(int frame);
	void patchChanged(int x, int y);
	void imageSessionChanged(ImageSession* imageSession);
	void patchGridConfigured(int cols, int rows, int borderExtension);

	// Session events
	void sessionClosed();

	// PSF pipeline results
	void psfWavefrontUpdated(af::array wavefront);
	void psfUpdated(af::array psf);
	void psfParameterDescriptorsChanged(QVector<WavefrontParameter> descriptors);
	void generatorTypeChanged(QString typeName);

	// Deconvolution results
	void deconvolutionCompleted();

	// Coefficient loading (for GUI update)
	void coefficientsLoaded(QVector<double> coefficients);

	// PSF settings broadcast
	void psfSettingsUpdated(PSFSettings settings);

	// Optimization signals
	void optimizationStarted();
	void optimizationProgressUpdated(OptimizationProgress progress);
	void optimizationFinished(OptimizationResult result);
	void runOptimizationOnWorker(OptimizationConfig config);

	// Batch processing
	void parametersLoaded();
	void batchDeconvolutionCompleted();

	// Interpolation results
	void interpolationCompleted(InterpolationResult result);
};

#endif // APPLICATIONCONTROLLER_H
