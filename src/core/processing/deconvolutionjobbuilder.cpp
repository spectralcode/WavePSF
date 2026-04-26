#include "deconvolutionjobbuilder.h"
#include "controller/imagesession.h"
#include "core/psf/psfmodule.h"
#include "core/psf/ipsfgenerator.h"
#include "data/wavefrontparametertable.h"
#include "utils/logging.h"
#include <QtGlobal>

namespace {
	bool initializeBatchPreparation(
		DeconvolutionJobBuilder::BatchPreparationState& state,
		DeconvolutionOperationKind operationKind,
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable)
	{
		if (imageSession == nullptr || psfModule == nullptr || parameterTable == nullptr) {
			LOG_WARNING() << "Cannot build batch deconvolution request: missing components";
			return false;
		}

		const int frames = imageSession->getInputFrames();
		const int cols = imageSession->getPatchGridCols();
		const int rows = imageSession->getPatchGridRows();
		const int totalJobs = (operationKind == DeconvolutionOperationKind::BATCH_2D)
			? frames * cols * rows
			: cols * rows;

		if (frames <= 0 || totalJobs <= 0) {
			LOG_WARNING() << "Cannot build batch deconvolution request: no frames or patches to process";
			return false;
		}

		state.reset();
		state.operationKind = operationKind;
		state.imageSession = imageSession;
		state.psfModule = psfModule;
		state.parameterTable = parameterTable;
		state.totalFrames = frames;
		state.patchGridCols = cols;
		state.patchGridRows = rows;
		state.displayFrame = imageSession->getCurrentFrame();
		state.supportsCoefficients = psfModule->getGenerator()->supportsCoefficients();
		state.is3DGenerator = psfModule->getGenerator()->is3D();

		state.request.operationKind = operationKind;
		state.request.psfSettings = psfModule->getPSFSettings();
		state.request.deconvolutionSettings = psfModule->getDeconvolutionSettings();
		state.request.patchGridCols = cols;
		state.request.patchGridRows = rows;
		state.request.patchBorderExtension = imageSession->getPatchBorderExtension();
		state.request.inputFrames.reserve(frames);

		if (operationKind == DeconvolutionOperationKind::BATCH_2D) {
			state.request.patchJobs.reserve(totalJobs);
		} else {
			state.request.volumeJobs.reserve(totalJobs);
		}

		return true;
	}

	bool appendNextBatch2DJob(DeconvolutionJobBuilder::BatchPreparationState& state)
	{
		if (state.nextJobFrame >= state.totalFrames || state.parameterTable == nullptr) {
			return false;
		}

		DeconvolutionPatchJob job;
		job.frameNr = state.nextJobFrame;
		job.patchX = state.nextPatchX;
		job.patchY = state.nextPatchY;
		job.patchIdx = state.parameterTable->patchIndex(job.patchX, job.patchY);

		if (state.supportsCoefficients) {
			job.coefficients = state.parameterTable->getCoefficients(job.frameNr, job.patchIdx);
		}

		state.request.patchJobs.append(job);

		state.nextPatchX++;
		if (state.nextPatchX >= state.patchGridCols) {
			state.nextPatchX = 0;
			state.nextPatchY++;
			if (state.nextPatchY >= state.patchGridRows) {
				state.nextPatchY = 0;
				state.nextJobFrame++;
			}
		}

		return true;
	}

	DeconvolutionVolumeJob makeVolumeJob(
		WavefrontParameterTable* parameterTable,
		int patchX,
		int patchY,
		int displayFrame,
		int totalFrames,
		bool supportsCoefficients,
		bool is3DGenerator)
	{
		DeconvolutionVolumeJob job;
		job.patchX = patchX;
		job.patchY = patchY;
		job.patchIdx = parameterTable->patchIndex(job.patchX, job.patchY);
		job.displayFrame = displayFrame;

		if (supportsCoefficients) {
			if (is3DGenerator) {
				job.coefficientsByFrame.append(
					parameterTable->getCoefficients(0, job.patchIdx));
			} else {
				job.coefficientsByFrame.reserve(totalFrames);
				for (int frameNr = 0; frameNr < totalFrames; ++frameNr) {
					job.coefficientsByFrame.append(
						parameterTable->getCoefficients(frameNr, job.patchIdx));
				}
			}
		}

		return job;
	}

	bool appendNextBatch3DJob(DeconvolutionJobBuilder::BatchPreparationState& state)
	{
		if (state.nextPatchY >= state.patchGridRows || state.parameterTable == nullptr) {
			return false;
		}

		state.request.volumeJobs.append(makeVolumeJob(
			state.parameterTable,
			state.nextPatchX,
			state.nextPatchY,
			state.displayFrame,
			state.totalFrames,
			state.supportsCoefficients,
			state.is3DGenerator));

		state.nextPatchX++;
		if (state.nextPatchX >= state.patchGridCols) {
			state.nextPatchX = 0;
			state.nextPatchY++;
		}

		return true;
	}
}

bool DeconvolutionJobBuilder::buildSinglePatch3DRequest(
	DeconvolutionRequest& request,
	ImageSession* imageSession,
	PSFModule* psfModule,
	WavefrontParameterTable* parameterTable,
	int patchX,
	int patchY)
{
	if (imageSession == nullptr || psfModule == nullptr || parameterTable == nullptr
		|| psfModule->getGenerator() == nullptr) {
		LOG_WARNING() << "Cannot build single-patch 3D deconvolution request: missing components";
		return false;
	}

	if (!imageSession->hasInputData() || !psfModule->is3DAlgorithm()) {
		LOG_WARNING() << "Cannot build single-patch 3D deconvolution request: invalid input or algorithm";
		return false;
	}

	const int frames = imageSession->getInputFrames();
	const int cols = imageSession->getPatchGridCols();
	const int rows = imageSession->getPatchGridRows();
	if (frames <= 0 || patchX < 0 || patchX >= cols || patchY < 0 || patchY >= rows) {
		LOG_WARNING() << "Cannot build single-patch 3D deconvolution request: invalid frame or patch range";
		return false;
	}

	request = DeconvolutionRequest();
	request.operationKind = DeconvolutionOperationKind::SINGLE_PATCH_3D;
	request.psfSettings = psfModule->getPSFSettings();
	request.deconvolutionSettings = psfModule->getDeconvolutionSettings();
	request.patchGridCols = cols;
	request.patchGridRows = rows;
	request.patchBorderExtension = imageSession->getPatchBorderExtension();
	request.inputFrames.reserve(frames);

	for (int frameNr = 0; frameNr < frames; ++frameNr) {
		af::array inputFrame = imageSession->getInputFrame(frameNr);
		if (inputFrame.isempty()) {
			LOG_WARNING() << "Single-patch 3D request: invalid input frame" << frameNr;
			return false;
		}
		request.inputFrames.append(inputFrame.copy());
	}

	DeconvolutionVolumeJob job = makeVolumeJob(
		parameterTable,
		patchX,
		patchY,
		imageSession->getCurrentFrame(),
		frames,
		psfModule->getGenerator()->supportsCoefficients(),
		psfModule->getGenerator()->is3D());
	if (psfModule->getGenerator()->supportsCoefficients()
		&& psfModule->getGenerator()->is3D()
		&& (job.coefficientsByFrame.isEmpty() || job.coefficientsByFrame.first().isEmpty())) {
		job.coefficientsByFrame.clear();
		job.coefficientsByFrame.append(psfModule->getGenerator()->getAllCoefficients());
	}
	request.volumeJobs.append(job);

	return true;
}

bool DeconvolutionJobBuilder::initializeBatchPreparation(
	BatchPreparationState& state,
	DeconvolutionOperationKind operationKind,
	ImageSession* imageSession,
	PSFModule* psfModule,
	WavefrontParameterTable* parameterTable)
{
	return ::initializeBatchPreparation(
		state,
		operationKind,
		imageSession,
		psfModule,
		parameterTable);
}

DeconvolutionJobBuilder::BatchPreparationStatus
DeconvolutionJobBuilder::advanceBatchPreparation(
	BatchPreparationState& state,
	int maxFrameCopies,
	int maxJobBuilds)
{
	if (!state.isInitialized()
		|| state.imageSession == nullptr
		|| state.psfModule == nullptr
		|| state.parameterTable == nullptr) {
		LOG_WARNING() << "Cannot advance batch deconvolution preparation: invalid state";
		return BatchPreparationStatus::FAILED;
	}

	int frameCopies = 0;
	while (state.nextFrameToCopy < state.totalFrames && frameCopies < qMax(1, maxFrameCopies)) {
		af::array inputFrame = state.imageSession->getInputFrame(state.nextFrameToCopy);
		if (inputFrame.isempty()) {
			LOG_WARNING() << "Batch request: invalid input frame" << state.nextFrameToCopy;
			return BatchPreparationStatus::FAILED;
		}

		state.request.inputFrames.append(inputFrame.copy());
		state.nextFrameToCopy++;
		frameCopies++;
	}

	if (state.nextFrameToCopy < state.totalFrames) {
		return BatchPreparationStatus::IN_PROGRESS;
	}

	int jobBuilds = 0;
	const int maxJobs = qMax(1, maxJobBuilds);
	if (state.operationKind == DeconvolutionOperationKind::BATCH_2D) {
		while (state.nextJobFrame < state.totalFrames && jobBuilds < maxJobs) {
			if (!appendNextBatch2DJob(state)) {
				return BatchPreparationStatus::FAILED;
			}
			jobBuilds++;
		}

		return state.nextJobFrame >= state.totalFrames
			? BatchPreparationStatus::COMPLETED
			: BatchPreparationStatus::IN_PROGRESS;
	}

	if (state.operationKind == DeconvolutionOperationKind::BATCH_3D) {
		while (state.nextPatchY < state.patchGridRows && jobBuilds < maxJobs) {
			if (!appendNextBatch3DJob(state)) {
				return BatchPreparationStatus::FAILED;
			}
			jobBuilds++;
		}

		return state.nextPatchY >= state.patchGridRows
			? BatchPreparationStatus::COMPLETED
			: BatchPreparationStatus::IN_PROGRESS;
	}

	return BatchPreparationStatus::FAILED;
}

bool DeconvolutionJobBuilder::buildBatch2DRequest(
	DeconvolutionRequest& request,
	ImageSession* imageSession,
	PSFModule* psfModule,
	WavefrontParameterTable* parameterTable)
{
	BatchPreparationState state;
	if (!DeconvolutionJobBuilder::initializeBatchPreparation(
			state,
			DeconvolutionOperationKind::BATCH_2D,
			imageSession,
			psfModule,
			parameterTable)) {
		return false;
	}

	BatchPreparationStatus status = BatchPreparationStatus::IN_PROGRESS;
	while (status == BatchPreparationStatus::IN_PROGRESS) {
		status = DeconvolutionJobBuilder::advanceBatchPreparation(
			state,
			state.totalFrames,
			state.totalJobCount());
	}
	if (status == BatchPreparationStatus::FAILED) {
		return false;
	}

	request = state.request;
	LOG_INFO() << "Built" << request.patchJobs.size() << "batch 2D deconvolution jobs";
	return !request.patchJobs.isEmpty();
}

bool DeconvolutionJobBuilder::buildBatch3DRequest(
	DeconvolutionRequest& request,
	ImageSession* imageSession,
	PSFModule* psfModule,
	WavefrontParameterTable* parameterTable)
{
	BatchPreparationState state;
	if (!DeconvolutionJobBuilder::initializeBatchPreparation(
			state,
			DeconvolutionOperationKind::BATCH_3D,
			imageSession,
			psfModule,
			parameterTable)) {
		return false;
	}

	BatchPreparationStatus status = BatchPreparationStatus::IN_PROGRESS;
	while (status == BatchPreparationStatus::IN_PROGRESS) {
		status = DeconvolutionJobBuilder::advanceBatchPreparation(
			state,
			state.totalFrames,
			state.totalJobCount());
	}
	if (status == BatchPreparationStatus::FAILED) {
		return false;
	}

	request = state.request;
	LOG_INFO() << "Built" << request.volumeJobs.size() << "batch 3D deconvolution jobs";
	return !request.volumeJobs.isEmpty();
}
