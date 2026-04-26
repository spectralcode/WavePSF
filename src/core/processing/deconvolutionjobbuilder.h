#ifndef DECONVOLUTIONJOBBUILDER_H
#define DECONVOLUTIONJOBBUILDER_H

#include "deconvolutiontypes.h"

class ImageSession;
class PSFModule;
class WavefrontParameterTable;

class DeconvolutionJobBuilder
{
public:
	enum class BatchPreparationStatus {
		FAILED = 0,
		IN_PROGRESS,
		COMPLETED
	};

	struct BatchPreparationState {
		DeconvolutionRequest request;
		DeconvolutionOperationKind operationKind = DeconvolutionOperationKind::UNKNOWN;
		ImageSession* imageSession = nullptr;
		PSFModule* psfModule = nullptr;
		WavefrontParameterTable* parameterTable = nullptr;
		int totalFrames = 0;
		int patchGridCols = 0;
		int patchGridRows = 0;
		int nextFrameToCopy = 0;
		int nextJobFrame = 0;
		int nextPatchX = 0;
		int nextPatchY = 0;
		int displayFrame = 0;
		bool supportsCoefficients = false;
		bool is3DGenerator = false;

		void reset()
		{
			*this = BatchPreparationState();
		}

		bool isInitialized() const
		{
			return this->operationKind == DeconvolutionOperationKind::BATCH_2D
				|| this->operationKind == DeconvolutionOperationKind::BATCH_3D;
		}

		int totalJobCount() const
		{
			if (this->operationKind == DeconvolutionOperationKind::BATCH_2D) {
				return this->totalFrames * this->patchGridCols * this->patchGridRows;
			}

			if (this->operationKind == DeconvolutionOperationKind::BATCH_3D) {
				return this->patchGridCols * this->patchGridRows;
			}

			return 0;
		}

		int totalPreparationUnits() const
		{
			return this->totalFrames + this->totalJobCount();
		}

		int completedPreparationUnits() const
		{
			return this->request.inputFrames.size()
				+ this->request.patchJobs.size()
				+ this->request.volumeJobs.size();
		}
	};

	static bool initializeBatchPreparation(
		BatchPreparationState& state,
		DeconvolutionOperationKind operationKind,
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable);

	static BatchPreparationStatus advanceBatchPreparation(
		BatchPreparationState& state,
		int maxFrameCopies,
		int maxJobBuilds);

	static bool buildBatch2DRequest(
		DeconvolutionRequest& request,
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable);

	static bool buildBatch3DRequest(
		DeconvolutionRequest& request,
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable);

	static bool buildSinglePatch3DRequest(
		DeconvolutionRequest& request,
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable,
		int patchX,
		int patchY);
};

#endif // DECONVOLUTIONJOBBUILDER_H
