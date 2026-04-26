#include "batchprocessor.h"
#include "volumetricprocessor.h"
#include "volumedeconvolutionprocessor.h"
#include "patchdeconvolutionprocessor.h"
#include "core/psf/deconvolver.h"
#include "core/psf/ipsfgenerator.h"
#include "data/patchextractor.h"
#include "utils/logging.h"
#include <QtGlobal>

BatchProcessor::BatchProcessor(QObject* parent)
	: QObject(parent)
{
}

DeconvolutionRunResult BatchProcessor::executeBatchDeconvolution(
	const DeconvolutionRequest& request,
	IPSFGenerator* generator,
	Deconvolver* deconvolver,
	const DeconvolutionCancelToken* cancelToken)
{
	DeconvolutionRunResult result;
	result.operationKind = request.operationKind;

	if (generator == nullptr || deconvolver == nullptr) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("Missing generator or deconvolver for batch execution.");
		return result;
	}

	if (request.inputFrames.isEmpty()) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("No input frames available for batch execution.");
		return result;
	}
	if (request.patchGridCols <= 0 || request.patchGridRows <= 0) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("Invalid patch layout for batch execution.");
		return result;
	}

	const PatchLayout patchLayout{
		static_cast<int>(request.inputFrames.first().dims(0)),
		static_cast<int>(request.inputFrames.first().dims(1)),
		request.patchGridCols,
		request.patchGridRows
	};
	if (!patchLayout.isValid()) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("Invalid frame dimensions for batch execution.");
		return result;
	}

	if (request.operationKind == DeconvolutionOperationKind::BATCH_2D) {
		return this->executeBatch2D(
			request,
			patchLayout,
			generator,
			deconvolver,
			cancelToken);
	}

	if (request.operationKind == DeconvolutionOperationKind::BATCH_3D
		|| request.operationKind == DeconvolutionOperationKind::SINGLE_PATCH_3D) {
		return this->executeVolumeJobs(
			request,
			patchLayout,
			generator,
			deconvolver,
			cancelToken);
	}

	result.status = DeconvolutionRunStatus::FAILED;
	result.message = QStringLiteral("Unsupported batch deconvolution operation.");
	return result;
}

DeconvolutionRunResult BatchProcessor::executeBatch2D(
	const DeconvolutionRequest& request,
	const PatchLayout& patchLayout,
	IPSFGenerator* generator,
	Deconvolver* deconvolver,
	const DeconvolutionCancelToken* cancelToken)
{
	DeconvolutionRunResult result;
	result.operationKind = request.operationKind;
	result.totalUnits = request.patchJobs.size();

	if (request.patchJobs.isEmpty()) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("No batch 2D deconvolution jobs specified.");
		return result;
	}

	const int totalFrames = request.inputFrames.size();
	auto emitProgress = [&](const QString& message,
							int completedUnits,
							int frameNr,
							int patchX,
							int patchY) {
		DeconvolutionProgress progress;
		progress.operationKind = request.operationKind;
		progress.phase = QStringLiteral("batch_2d");
		progress.message = message;
		progress.completedUnits = completedUnits;
		progress.totalUnits = request.patchJobs.size();
		progress.currentFrameNr = frameNr;
		progress.currentPatchX = patchX;
		progress.currentPatchY = patchY;
		progress.isCancellable = true;
		emit this->progressUpdated(progress);
	};

	LOG_INFO() << "Starting batch 2D deconvolution with" << request.patchJobs.size() << "jobs";
	emitProgress(QStringLiteral("Initializing batch deconvolution..."), 0, -1, -1, -1);

	for (int jobIndex = 0; jobIndex < request.patchJobs.size(); ++jobIndex) {
		if (cancelToken != nullptr && cancelToken->isCancellationRequested()) {
			result.status = DeconvolutionRunStatus::CANCELLED;
			result.message = QStringLiteral("Batch deconvolution cancelled.");
			result.completedUnits = jobIndex;
			LOG_INFO() << "Batch 2D deconvolution cancelled at job" << jobIndex
					   << "of" << request.patchJobs.size();
			return result;
		}

		const DeconvolutionPatchJob& job = request.patchJobs[jobIndex];
		const QString progressMessage = QString("Processing frame %1/%2, patch (%3,%4)...")
			.arg(job.frameNr + 1)
			.arg(qMax(1, totalFrames))
			.arg(job.patchX)
			.arg(job.patchY);
		emitProgress(progressMessage, jobIndex, job.frameNr, job.patchX, job.patchY);

		if (job.frameNr < 0 || job.frameNr >= request.inputFrames.size()) {
			result.completedUnits = jobIndex + 1;
			emitProgress(progressMessage, result.completedUnits, job.frameNr, job.patchX, job.patchY);
			continue;
		}

		ImagePatch inputPatch = PatchExtractor::extractExtendedPatch(
			request.inputFrames[job.frameNr],
			patchLayout,
			job.patchX,
			job.patchY,
			request.patchBorderExtension);
		if (!inputPatch.isValid()) {
			result.completedUnits = jobIndex + 1;
			emitProgress(progressMessage, result.completedUnits, job.frameNr, job.patchX, job.patchY);
			continue;
		}

		try {
			PatchDeconvolutionInput patchRequest;
			patchRequest.inputPatch = inputPatch.data;
			patchRequest.frameNr = job.frameNr;
			patchRequest.patchIdx = job.patchIdx;
			patchRequest.gridSize = request.psfSettings.gridSize;
			patchRequest.coefficients = job.coefficients;
			patchRequest.generatePSFIfMissing = true;

			af::array outputPatch = PatchDeconvolutionProcessor::process(
				patchRequest,
				generator,
				deconvolver);
			if (outputPatch.isempty()) {
				LOG_WARNING() << "Batch 2D deconvolution returned empty result at frame"
							  << job.frameNr << "patch (" << job.patchX << "," << job.patchY << ")";
				result.completedUnits = jobIndex + 1;
				emitProgress(progressMessage, result.completedUnits, job.frameNr, job.patchX, job.patchY);
				continue;
			}

			DeconvolutionPatchOutput output;
			output.frameNr = job.frameNr;
			output.patchX = job.patchX;
			output.patchY = job.patchY;
			output.outputPatch = outputPatch;

			emit this->patchOutputReady(output);
		} catch (const af::exception& e) {
			LOG_WARNING() << "Batch 2D deconvolution failed at frame" << job.frameNr
						  << "patch (" << job.patchX << "," << job.patchY << "):"
						  << e.what();
		}

		result.completedUnits = jobIndex + 1;
		emitProgress(progressMessage, result.completedUnits, job.frameNr, job.patchX, job.patchY);
	}

	result.status = DeconvolutionRunStatus::COMPLETED;
	result.message = QStringLiteral("Batch deconvolution completed.");
	LOG_INFO() << "Batch 2D deconvolution completed:" << result.completedUnits << "jobs processed";
	return result;
}

DeconvolutionRunResult BatchProcessor::executeVolumeJobs(
	const DeconvolutionRequest& request,
	const PatchLayout& patchLayout,
	IPSFGenerator* generator,
	Deconvolver* deconvolver,
	const DeconvolutionCancelToken* cancelToken)
{
	DeconvolutionRunResult result;
	result.operationKind = request.operationKind;
	result.totalUnits = request.volumeJobs.size();

	if (request.volumeJobs.isEmpty()) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("No 3D deconvolution jobs specified.");
		return result;
	}

	const bool singlePatch3D = request.operationKind == DeconvolutionOperationKind::SINGLE_PATCH_3D;
	auto isCancelled = [&]() {
		return cancelToken != nullptr && cancelToken->isCancellationRequested();
	};
	auto finishCancelled = [&](int completedUnits) {
		result.status = DeconvolutionRunStatus::CANCELLED;
		result.message = singlePatch3D
			? QStringLiteral("3D deconvolution cancelled.")
			: QStringLiteral("Batch deconvolution cancelled.");
		result.completedUnits = completedUnits;
		return result;
	};
	auto emitProgress = [&](const QString& message,
							int completedUnits,
							int patchX,
							int patchY,
							int displayFrame,
							int currentIteration = 0,
							int totalIterations = 0) {
		DeconvolutionProgress progress;
		progress.operationKind = request.operationKind;
		progress.phase = singlePatch3D
			? QStringLiteral("single_patch_3d")
			: QStringLiteral("batch_3d");
		progress.message = message;
		progress.completedUnits = singlePatch3D
			? (currentIteration > 0 ? currentIteration : 0)
			: completedUnits;
		progress.totalUnits = singlePatch3D
			? (totalIterations > 0 ? totalIterations : qMax(0, request.deconvolutionSettings.iterations))
			: request.volumeJobs.size();
		progress.currentIteration = currentIteration;
		progress.totalIterations = totalIterations;
		progress.currentFrameNr = displayFrame;
		progress.currentPatchX = patchX;
		progress.currentPatchY = patchY;
		progress.isCancellable = true;
		emit this->progressUpdated(progress);
	};

	int currentJobIndex = -1;
	QMetaObject::Connection iterationConnection = QObject::connect(
		deconvolver,
		&Deconvolver::iterationCompleted,
		[&](int currentIteration, int totalIterations) {
			if (cancelToken != nullptr && cancelToken->isCancellationRequested()) {
				deconvolver->requestDeconvolutionCancel();
			}

			if (currentJobIndex < 0 || currentJobIndex >= request.volumeJobs.size()) {
				return;
			}

			const DeconvolutionVolumeJob& job = request.volumeJobs[currentJobIndex];
			const QString message = QString("Patch %1/%2: iteration %3/%4")
				.arg(currentJobIndex + 1)
				.arg(request.volumeJobs.size())
				.arg(currentIteration)
				.arg(totalIterations);
			emitProgress(
				message,
				currentJobIndex,
				job.patchX,
				job.patchY,
				job.displayFrame,
				currentIteration,
				totalIterations);
		});

	LOG_INFO() << "Starting" << (singlePatch3D ? "single-patch" : "batch")
			   << "3D deconvolution with" << request.volumeJobs.size() << "jobs";
	emitProgress(
		singlePatch3D
			? QStringLiteral("Preparing 3D deconvolution...")
			: QStringLiteral("Initializing batch deconvolution..."),
		0, -1, -1, -1);

	for (int jobIndex = 0; jobIndex < request.volumeJobs.size(); ++jobIndex) {
		if (isCancelled()) {
			QObject::disconnect(iterationConnection);
			LOG_INFO() << "Batch 3D deconvolution cancelled at job" << jobIndex
					   << "of" << request.volumeJobs.size();
			return finishCancelled(jobIndex);
		}

		currentJobIndex = jobIndex;
		const DeconvolutionVolumeJob& job = request.volumeJobs[jobIndex];
		const QString patchPrefix = QString("Patch %1/%2")
			.arg(jobIndex + 1)
			.arg(request.volumeJobs.size());

		emitProgress(
			patchPrefix + QStringLiteral(": assembling subvolume..."),
			jobIndex,
			job.patchX,
			job.patchY,
			job.displayFrame);

		try {
			af::array inputVolume = VolumetricProcessor::assembleSubvolume(
				request.inputFrames,
				patchLayout,
				job.patchX,
				job.patchY,
				request.patchBorderExtension);
			if (inputVolume.isempty()) {
				LOG_WARNING() << "Skipping batch 3D patch (" << job.patchX << "," << job.patchY
							  << "): empty subvolume";
				result.completedUnits = jobIndex + 1;
				emitProgress(
					patchPrefix + QStringLiteral(": skipped empty subvolume"),
					result.completedUnits,
					job.patchX,
					job.patchY,
					job.displayFrame);
				continue;
			}
			if (isCancelled()) {
				QObject::disconnect(iterationConnection);
				LOG_INFO() << "Batch 3D deconvolution cancelled after subvolume assembly at patch"
						   << job.patchX << job.patchY;
				return finishCancelled(jobIndex);
			}

			emitProgress(
				patchPrefix + QStringLiteral(": assembling 3D PSF..."),
				jobIndex,
				job.patchX,
				job.patchY,
				job.displayFrame);

			af::array psfVolume = VolumetricProcessor::assemble3DPSF(
				generator,
				request.psfSettings.gridSize,
				job.patchIdx,
				request.inputFrames.size(),
				job.coefficientsByFrame);
			if (psfVolume.isempty()) {
				LOG_WARNING() << "Skipping batch 3D patch (" << job.patchX << "," << job.patchY
							  << "): empty PSF volume";
				result.completedUnits = jobIndex + 1;
				emitProgress(
					patchPrefix + QStringLiteral(": skipped empty PSF"),
					result.completedUnits,
					job.patchX,
					job.patchY,
					job.displayFrame);
				continue;
			}
			if (isCancelled()) {
				QObject::disconnect(iterationConnection);
				LOG_INFO() << "Batch 3D deconvolution cancelled after PSF assembly at patch"
						   << job.patchX << job.patchY;
				return finishCancelled(jobIndex);
			}

			deconvolver->resetDeconvolutionCancel();

			VolumeDeconvolutionInput volumeRequest;
			volumeRequest.inputVolume = inputVolume;
			volumeRequest.psfVolume = psfVolume;
			af::array outputVolume = VolumeDeconvolutionProcessor::process(
				volumeRequest,
				deconvolver);
			if (outputVolume.isempty()) {
				const bool wasCancelled =
					isCancelled()
					|| deconvolver->wasDeconvolutionCancelled();
				if (wasCancelled) {
					QObject::disconnect(iterationConnection);
					LOG_INFO() << "Batch 3D deconvolution cancelled at patch"
							   << job.patchX << job.patchY;
					return finishCancelled(jobIndex);
				}

				LOG_WARNING() << "Batch 3D deconvolution returned empty result at patch ("
							  << job.patchX << "," << job.patchY << ")";
				result.completedUnits = jobIndex + 1;
				emitProgress(
					patchPrefix + QStringLiteral(": deconvolution returned empty result"),
					result.completedUnits,
					job.patchX,
					job.patchY,
					job.displayFrame);
				af::deviceGC();
				continue;
			}

			DeconvolutionVolumeOutput output;
			output.patchX = job.patchX;
			output.patchY = job.patchY;
			output.displayFrame = job.displayFrame;
			output.outputVolume = outputVolume;

			emit this->volumeOutputReady(output);

			inputVolume = af::array();
			psfVolume = af::array();
			outputVolume = af::array();
			af::deviceGC();

			result.completedUnits = jobIndex + 1;
			emitProgress(
				patchPrefix + QStringLiteral(": completed"),
				result.completedUnits,
				job.patchX,
				job.patchY,
				job.displayFrame,
				singlePatch3D ? request.deconvolutionSettings.iterations : 0,
				singlePatch3D ? request.deconvolutionSettings.iterations : 0);
		} catch (const af::exception& e) {
			LOG_WARNING() << "Batch 3D deconvolution failed at patch (" << job.patchX
						  << "," << job.patchY << "):" << e.what();
			result.completedUnits = jobIndex + 1;
			emitProgress(
				patchPrefix + QStringLiteral(": failed"),
				result.completedUnits,
				job.patchX,
				job.patchY,
				job.displayFrame);
			af::deviceGC();
		}
	}

	QObject::disconnect(iterationConnection);
	result.status = DeconvolutionRunStatus::COMPLETED;
	result.message = singlePatch3D
		? QStringLiteral("3D deconvolution completed.")
		: QStringLiteral("Batch deconvolution completed.");
	LOG_INFO() << (singlePatch3D ? "Single-patch" : "Batch")
			   << "3D deconvolution completed:" << result.completedUnits << "jobs processed";
	return result;
}
