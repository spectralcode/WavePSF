#include "deconvolutioncontroller.h"
#include "deconvolutionworkercontroller.h"
#include "controller/imagesession.h"
#include "controller/coefficientworkspace.h"
#include "core/psf/psfmodule.h"
#include "core/psf/ipsfgenerator.h"
#include "utils/afdevicemanager.h"
#include "utils/logging.h"

namespace {
	const int BATCH_PREPARATION_FRAME_CHUNK = 1;
	const int BATCH_PREPARATION_JOB_CHUNK = 8;
	const int BATCH_3D_OUTPUT_FLUSH_PATCH_COUNT = 8;
}

DeconvolutionController::DeconvolutionController(
	AFDeviceManager* afDeviceManager,
	PSFModule* psfModule,
	ImageSession* imageSession,
	CoefficientWorkspace* coefficientWorkspace,
	QObject* parent)
	: QObject(parent)
	, afDeviceManager(afDeviceManager)
	, psfModule(psfModule)
	, imageSession(imageSession)
	, coefficientWorkspace(coefficientWorkspace)
	, deconvolutionWorkerController(new DeconvolutionWorkerController(this))
	, asyncDeconvolutionInProgress(false)
	, pendingBatchDeconvolutionStart(false)
	, pendingDeconvolutionCancellation(false)
	, deconvolutionCancellationInProgress(false)
	, pendingBatchOperationKind(DeconvolutionOperationKind::UNKNOWN)
	, activeAsyncDeconvolutionOperationKind(DeconvolutionOperationKind::UNKNOWN)
{
	connect(this->deconvolutionWorkerController, &DeconvolutionWorkerController::progressUpdated,
			this, &DeconvolutionController::deconvolutionRunProgressUpdated);
	connect(this->deconvolutionWorkerController, &DeconvolutionWorkerController::patchOutputReady,
			this, &DeconvolutionController::handleDeconvolutionPatchOutput);
	connect(this->deconvolutionWorkerController, &DeconvolutionWorkerController::volumeOutputReady,
			this, &DeconvolutionController::handleDeconvolutionVolumeOutput);
	connect(this->deconvolutionWorkerController, &DeconvolutionWorkerController::finished,
			this, &DeconvolutionController::handleDeconvolutionFinished);
}

void DeconvolutionController::requestCurrentDeconvolution()
{
	if (this->psfModule == nullptr || this->imageSession == nullptr) {
		return;
	}

	if (!this->psfModule->is3DAlgorithm()) {
		this->runOnCurrentPatch();
		return;
	}

	if (this->asyncDeconvolutionInProgress || this->pendingBatchDeconvolutionStart) {
		LOG_INFO() << "Skipping 3D deconvolution request while another async deconvolution is already running";
		return;
	}

	if (this->coefficientWorkspace != nullptr) {
		this->coefficientWorkspace->store();
	}

	DeconvolutionRequest request;
	this->syncVoxelSize();
	if (!DeconvolutionJobBuilder::buildSinglePatch3DRequest(
			request,
			this->imageSession,
			this->psfModule,
			this->coefficientWorkspace != nullptr ? this->coefficientWorkspace->table() : nullptr,
			this->imageSession->getCurrentPatch().x(),
			this->imageSession->getCurrentPatch().y())) {
		return;
	}

	if (this->afDeviceManager != nullptr) {
		request.afBackend = this->afDeviceManager->getActiveBackendId();
		request.afDeviceId = this->afDeviceManager->getActiveDeviceId();
	}

	this->deconvolutionCancellationInProgress = false;
	this->pendingDeconvolutionCancellation = false;
	this->bufferedVolumeOutputs.clear();
	this->asyncDeconvolutionInProgress = true;
	this->activeAsyncDeconvolutionOperationKind = request.operationKind;

	emit this->deconvolutionRunStarted();
	this->deconvolutionWorkerController->start(request);
}

bool DeconvolutionController::requestBatchDeconvolution()
{
	if (this->psfModule == nullptr || this->imageSession == nullptr
		|| this->coefficientWorkspace == nullptr) {
		return false;
	}

	if (this->asyncDeconvolutionInProgress) {
		LOG_INFO() << "Ignoring batch deconvolution request while another async deconvolution is already running";
		return false;
	}

	if (this->pendingBatchDeconvolutionStart) {
		return false;
	}

	this->coefficientWorkspace->store();
	this->pendingDeconvolutionCancellation = false;
	this->deconvolutionCancellationInProgress = false;
	this->pendingBatchDeconvolutionStart = true;
	this->activeAsyncDeconvolutionOperationKind = DeconvolutionOperationKind::UNKNOWN;
	this->batchPreparationState.reset();
	this->bufferedVolumeOutputs.clear();
	this->pendingBatchOperationKind = this->psfModule->is3DAlgorithm()
		? DeconvolutionOperationKind::BATCH_3D
		: DeconvolutionOperationKind::BATCH_2D;

	emit this->deconvolutionRunStarted();

	DeconvolutionProgress progress;
	progress.operationKind = this->pendingBatchOperationKind;
	progress.phase = QStringLiteral("prepare_batch");
	progress.message = tr("Preparing batch deconvolution...");
	progress.isCancellable = true;
	emit this->deconvolutionRunProgressUpdated(progress);

	QMetaObject::invokeMethod(
		this,
		"startPendingBatchDeconvolution",
		Qt::QueuedConnection);
	return true;
}

void DeconvolutionController::cancelDeconvolution()
{
	if (this->pendingBatchDeconvolutionStart) {
		this->pendingDeconvolutionCancellation = true;
		emit this->deconvolutionRunCancellationRequested();
		return;
	}

	if (!this->asyncDeconvolutionInProgress) {
		return;
	}

	this->deconvolutionCancellationInProgress = true;
	this->bufferedVolumeOutputs.clear();
	this->deconvolutionWorkerController->cancel();
	emit this->deconvolutionRunCancellationRequested();
}

void DeconvolutionController::runOnCurrentPatch()
{
	if (this->imageSession == nullptr || !this->imageSession->hasInputData()
		|| this->psfModule == nullptr) {
		return;
	}

	if (this->psfModule->is3DAlgorithm()) {
		LOG_WARNING() << "runOnCurrentPatch() is reserved for 2D deconvolution when a 3D algorithm is selected";
		return;
	}

	ImagePatch inputPatch = this->imageSession->getCurrentInputPatch();
	if (!inputPatch.isValid()) {
		LOG_WARNING() << "No valid input patch for deconvolution";
		return;
	}

	af::array result = this->psfModule->deconvolve(inputPatch.data);
	if (result.isempty()) {
		return;
	}

	this->imageSession->setCurrentOutputPatch(result);
	emit this->deconvolutionOutputUpdated();
}

void DeconvolutionController::startPendingBatchDeconvolution()
{
	if (!this->pendingBatchDeconvolutionStart) {
		return;
	}

	DeconvolutionRunResult result;
	result.operationKind = this->pendingBatchOperationKind;

	if (this->pendingDeconvolutionCancellation) {
		this->pendingBatchDeconvolutionStart = false;
		this->batchPreparationState.reset();
		this->pendingDeconvolutionCancellation = false;
		this->deconvolutionCancellationInProgress = false;
		result.status = DeconvolutionRunStatus::CANCELLED;
		result.message = tr("Batch deconvolution cancelled.");
		this->handleDeconvolutionFinished(result);
		return;
	}

	if (!this->batchPreparationState.isInitialized()) {
		this->syncVoxelSize();

		if (!DeconvolutionJobBuilder::initializeBatchPreparation(
				this->batchPreparationState,
				this->pendingBatchOperationKind,
				this->imageSession,
				this->psfModule,
				this->coefficientWorkspace != nullptr ? this->coefficientWorkspace->table() : nullptr)) {
			this->pendingBatchDeconvolutionStart = false;
			this->batchPreparationState.reset();
			this->pendingDeconvolutionCancellation = false;
			this->deconvolutionCancellationInProgress = false;
			result.status = DeconvolutionRunStatus::FAILED;
			result.message = tr("Failed to prepare batch deconvolution.");
			this->handleDeconvolutionFinished(result);
			return;
		}
	}

	DeconvolutionJobBuilder::BatchPreparationStatus status =
		DeconvolutionJobBuilder::advanceBatchPreparation(
			this->batchPreparationState,
			BATCH_PREPARATION_FRAME_CHUNK,
			BATCH_PREPARATION_JOB_CHUNK);

	if (status == DeconvolutionJobBuilder::BatchPreparationStatus::FAILED) {
		this->pendingBatchDeconvolutionStart = false;
		this->batchPreparationState.reset();
		this->pendingDeconvolutionCancellation = false;
		this->deconvolutionCancellationInProgress = false;
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = tr("Failed to prepare batch deconvolution.");
		this->handleDeconvolutionFinished(result);
		return;
	}

	this->emitBatchPreparationProgress();

	if (this->pendingDeconvolutionCancellation) {
		this->pendingBatchDeconvolutionStart = false;
		this->batchPreparationState.reset();
		this->pendingDeconvolutionCancellation = false;
		this->deconvolutionCancellationInProgress = false;
		result.status = DeconvolutionRunStatus::CANCELLED;
		result.message = tr("Batch deconvolution cancelled.");
		this->handleDeconvolutionFinished(result);
		return;
	}

	if (status == DeconvolutionJobBuilder::BatchPreparationStatus::IN_PROGRESS) {
		QMetaObject::invokeMethod(
			this,
			"startPendingBatchDeconvolution",
			Qt::QueuedConnection);
		return;
	}

	this->pendingBatchDeconvolutionStart = false;
	DeconvolutionRequest request = this->batchPreparationState.request;
	this->batchPreparationState.reset();
	if (this->afDeviceManager != nullptr) {
		request.afBackend = this->afDeviceManager->getActiveBackendId();
		request.afDeviceId = this->afDeviceManager->getActiveDeviceId();
	}
	this->pendingDeconvolutionCancellation = false;
	this->asyncDeconvolutionInProgress = true;
	this->activeAsyncDeconvolutionOperationKind = request.operationKind;
	this->deconvolutionWorkerController->start(request);
}

void DeconvolutionController::emitBatchPreparationProgress()
{
	if (!this->batchPreparationState.isInitialized()) {
		return;
	}

	DeconvolutionProgress progress;
	progress.operationKind = this->batchPreparationState.operationKind;
	progress.phase = QStringLiteral("prepare_batch");
	progress.completedUnits = this->batchPreparationState.completedPreparationUnits();
	progress.totalUnits = this->batchPreparationState.totalPreparationUnits();
	progress.isCancellable = true;

	if (this->batchPreparationState.nextFrameToCopy < this->batchPreparationState.totalFrames) {
		progress.currentFrameNr = this->batchPreparationState.nextFrameToCopy;
		progress.message = tr("Preparing batch deconvolution: copying frame %1 / %2...")
			.arg(this->batchPreparationState.nextFrameToCopy + 1)
			.arg(this->batchPreparationState.totalFrames);
	} else {
		const int completedJobs = this->batchPreparationState.operationKind == DeconvolutionOperationKind::BATCH_2D
			? this->batchPreparationState.request.patchJobs.size()
			: this->batchPreparationState.request.volumeJobs.size();
		progress.message = tr("Preparing batch deconvolution: building jobs %1 / %2...")
			.arg(completedJobs)
			.arg(this->batchPreparationState.totalJobCount());
	}

	emit this->deconvolutionRunProgressUpdated(progress);
}

void DeconvolutionController::handleDeconvolutionPatchOutput(const DeconvolutionPatchOutput& output)
{
	if (this->imageSession == nullptr || this->deconvolutionCancellationInProgress) {
		return;
	}

	this->imageSession->setOutputPatch(
		output.frameNr,
		output.patchX,
		output.patchY,
		output.outputPatch);
	if (output.frameNr == this->imageSession->getCurrentFrame()) {
		emit this->deconvolutionOutputUpdated();
	}
}

void DeconvolutionController::handleDeconvolutionVolumeOutput(
	const DeconvolutionVolumeOutput& output)
{
	if (this->imageSession == nullptr || this->deconvolutionCancellationInProgress) {
		return;
	}

	if (this->activeAsyncDeconvolutionOperationKind == DeconvolutionOperationKind::SINGLE_PATCH_3D) {
		this->imageSession->setOutputSubvolume(
			output.patchX,
			output.patchY,
			output.outputVolume);
		emit this->deconvolutionOutputUpdated();
		return;
	}

	const int currentFrame = this->imageSession->getCurrentFrame();
	if (!output.outputVolume.isempty()
		&& output.outputVolume.numdims() >= 3
		&& currentFrame >= 0
		&& currentFrame < output.outputVolume.dims(2)) {
		this->imageSession->setOutputPatch(
			currentFrame,
			output.patchX,
			output.patchY,
			output.outputVolume(af::span, af::span, currentFrame));
		emit this->deconvolutionOutputUpdated();
	}

	this->bufferedVolumeOutputs.append(output);
	if (this->bufferedVolumeOutputs.size() >= BATCH_3D_OUTPUT_FLUSH_PATCH_COUNT) {
		this->flushBufferedVolumeOutputs();
	}
}

void DeconvolutionController::handleDeconvolutionFinished(const DeconvolutionRunResult& result)
{
	this->flushBufferedVolumeOutputs();
	this->finalizeDeconvolutionRun(result);
}

void DeconvolutionController::flushBufferedVolumeOutputs()
{
	if (this->imageSession == nullptr
		|| this->bufferedVolumeOutputs.isEmpty()
		|| this->deconvolutionCancellationInProgress) {
		this->bufferedVolumeOutputs.clear();
		return;
	}

	const int totalFrames = this->imageSession->getInputFrames();
	const int currentFrame = this->imageSession->getCurrentFrame();
	QList<DeconvolutionVolumeOutput> outputs = this->bufferedVolumeOutputs;
	this->bufferedVolumeOutputs.clear();

	for (int frameNr = 0; frameNr < totalFrames; ++frameNr) {
		QList<QPoint> patchCoords;
		QList<af::array> patchData;

		for (const DeconvolutionVolumeOutput& output : qAsConst(outputs)) {
			if (output.outputVolume.isempty() || output.outputVolume.numdims() < 3) {
				continue;
			}
			if (frameNr >= output.outputVolume.dims(2)) {
				continue;
			}

			patchCoords.append(QPoint(output.patchX, output.patchY));
			patchData.append(output.outputVolume(af::span, af::span, frameNr));
		}

		if (!patchCoords.isEmpty()) {
			this->imageSession->setOutputPatchResults(frameNr, patchCoords, patchData);
		}
	}

	for (const DeconvolutionVolumeOutput& output : qAsConst(outputs)) {
		if (output.outputVolume.isempty() || output.outputVolume.numdims() < 3) {
			continue;
		}
		if (currentFrame < 0 || currentFrame >= output.outputVolume.dims(2)) {
			continue;
		}

		if (output.patchX == this->imageSession->getCurrentPatch().x()
			&& output.patchY == this->imageSession->getCurrentPatch().y()) {
			this->imageSession->setCurrentOutputPatch(
				output.outputVolume(af::span, af::span, currentFrame));
			break;
		}
	}

	emit this->deconvolutionOutputUpdated();
}

void DeconvolutionController::finalizeDeconvolutionRun(
	const DeconvolutionRunResult& result)
{
	this->pendingBatchDeconvolutionStart = false;
	this->pendingDeconvolutionCancellation = false;
	this->deconvolutionCancellationInProgress = false;
	this->pendingBatchOperationKind = DeconvolutionOperationKind::UNKNOWN;
	this->activeAsyncDeconvolutionOperationKind = DeconvolutionOperationKind::UNKNOWN;
	this->batchPreparationState.reset();
	this->bufferedVolumeOutputs.clear();
	this->asyncDeconvolutionInProgress = false;

	if ((result.operationKind == DeconvolutionOperationKind::BATCH_2D
		 || result.operationKind == DeconvolutionOperationKind::BATCH_3D)
		&& this->coefficientWorkspace != nullptr) {
		this->coefficientWorkspace->loadForCurrentPatch();
	}

	emit this->deconvolutionRunFinished(result);
}

void DeconvolutionController::syncVoxelSize()
{
	if (this->psfModule == nullptr || !this->psfModule->is3DAlgorithm()) {
		return;
	}

	IPSFGenerator* generator = this->psfModule->getGenerator();
	if (generator == nullptr) {
		return;
	}

	QVariantMap generatorSettings = generator->serializeSettings();
	float xyStep = generatorSettings.value("xy_step_nm", 1.0).toFloat();
	float zStep = generatorSettings.value("z_step_nm", 1.0).toFloat();
	if (xyStep > 0.0f && zStep > 0.0f) {
		this->psfModule->setDeconvolutionVoxelSize(xyStep, xyStep, zStep);
	}
}
