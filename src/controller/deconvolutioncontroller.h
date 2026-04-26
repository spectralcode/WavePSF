#ifndef DECONVOLUTIONCONTROLLER_H
#define DECONVOLUTIONCONTROLLER_H

#include <QObject>
#include "core/processing/deconvolutionjobbuilder.h"
#include "core/processing/deconvolutiontypes.h"

class AFDeviceManager;
class PSFModule;
class ImageSession;
class CoefficientWorkspace;
class DeconvolutionWorkerController;

class DeconvolutionController : public QObject
{
	Q_OBJECT

public:
	explicit DeconvolutionController(
		AFDeviceManager* afDeviceManager,
		PSFModule* psfModule,
		ImageSession* imageSession,
		CoefficientWorkspace* coefficientWorkspace,
		QObject* parent = nullptr);

public slots:
	void requestCurrentDeconvolution();
	bool requestBatchDeconvolution();
	void cancelDeconvolution();

signals:
	void deconvolutionRunStarted();
	void deconvolutionRunProgressUpdated(DeconvolutionProgress progress);
	void deconvolutionRunCancellationRequested();
	void deconvolutionRunFinished(DeconvolutionRunResult result);
	void deconvolutionOutputUpdated();

private slots:
	void startPendingBatchDeconvolution();
	void handleDeconvolutionPatchOutput(const DeconvolutionPatchOutput& output);
	void handleDeconvolutionVolumeOutput(const DeconvolutionVolumeOutput& output);
	void handleDeconvolutionFinished(const DeconvolutionRunResult& result);

private:
	void runOnCurrentPatch();
	void emitBatchPreparationProgress();
	void flushBufferedVolumeOutputs();
	void finalizeDeconvolutionRun(const DeconvolutionRunResult& result);
	void syncVoxelSize();

	AFDeviceManager* afDeviceManager;
	PSFModule* psfModule;
	ImageSession* imageSession;
	CoefficientWorkspace* coefficientWorkspace;
	DeconvolutionWorkerController* deconvolutionWorkerController;
	bool asyncDeconvolutionInProgress;
	bool pendingBatchDeconvolutionStart;
	bool pendingDeconvolutionCancellation;
	bool deconvolutionCancellationInProgress;
	DeconvolutionOperationKind pendingBatchOperationKind;
	DeconvolutionOperationKind activeAsyncDeconvolutionOperationKind;
	DeconvolutionJobBuilder::BatchPreparationState batchPreparationState;
	QList<DeconvolutionVolumeOutput> bufferedVolumeOutputs;
};

#endif // DECONVOLUTIONCONTROLLER_H
