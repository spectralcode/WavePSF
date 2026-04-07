#ifndef DECONVOLUTIONTYPES_H
#define DECONVOLUTIONTYPES_H

#include <QAtomicInt>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <arrayfire.h>
#include "core/psf/psfsettings.h"

enum class DeconvolutionOperationKind {
	UNKNOWN = 0,
	SINGLE_PATCH_3D,
	BATCH_2D,
	BATCH_3D
};

enum class DeconvolutionRunStatus {
	COMPLETED = 0,
	CANCELLED,
	FAILED
};

class DeconvolutionCancelToken
{
public:
	DeconvolutionCancelToken()
	{
		this->cancelRequested.storeRelease(0);
	}

	void requestCancel()
	{
		this->cancelRequested.storeRelease(1);
	}

	void reset()
	{
		this->cancelRequested.storeRelease(0);
	}

	bool isCancellationRequested() const
	{
		return this->cancelRequested.loadAcquire() != 0;
	}

private:
	QAtomicInt cancelRequested;
};

struct DeconvolutionPatchJob {
	int frameNr = 0;
	int patchX = 0;
	int patchY = 0;
	int patchIdx = -1;
	af::array inputPatch;
	af::array psf;
	QVector<double> coefficients;
};

struct DeconvolutionVolumeJob {
	int patchX = 0;
	int patchY = 0;
	int patchIdx = -1;
	int displayFrame = 0;
	af::array inputVolume;
	af::array psfVolume;
	QVector<double> coefficients;
};

struct DeconvolutionPatchOutput {
	int frameNr = 0;
	int patchX = 0;
	int patchY = 0;
	af::array outputPatch;
};

struct DeconvolutionVolumeOutput {
	int patchX = 0;
	int patchY = 0;
	int displayFrame = 0;
	af::array outputVolume;
};

struct DeconvolutionRequest {
	DeconvolutionOperationKind operationKind = DeconvolutionOperationKind::UNKNOWN;

	// Worker thread uses the correct AF context for the requested device.
	int afBackend = 0;
	int afDeviceId = 0;

	// Snapshot of PSF and deconvolution settings for future worker-side execution.
	PSFSettings psfSettings;
	int deconvAlgorithm = 0;
	int deconvIterations = 128;
	float deconvRelaxationFactor = 0.65f;
	float deconvRegularizationFactor = 0.005f;
	float deconvNoiseToSignalFactor = 0.01f;
	int volumePaddingMode = 0;
	int accelerationMode = 0;
	int regularizer3D = 0;
	float regularizationWeight = 0.0f;
	float voxelSizeY = 1.0f;
	float voxelSizeX = 1.0f;
	float voxelSizeZ = 1.0f;

	QVector<DeconvolutionPatchJob> patchJobs;
	QVector<DeconvolutionVolumeJob> volumeJobs;
};

struct DeconvolutionProgress {
	DeconvolutionOperationKind operationKind = DeconvolutionOperationKind::UNKNOWN;
	QString phase;
	QString message;
	int completedUnits = 0;
	int totalUnits = 0;
	int currentIteration = 0;
	int totalIterations = 0;
	int currentFrameNr = -1;
	int currentPatchX = -1;
	int currentPatchY = -1;
	bool isCancellable = false;
};

struct DeconvolutionRunResult {
	DeconvolutionRunStatus status = DeconvolutionRunStatus::COMPLETED;
	QString message;
	int completedUnits = 0;
	int totalUnits = 0;
	QVector<DeconvolutionPatchOutput> patchOutputs;
	QVector<DeconvolutionVolumeOutput> volumeOutputs;
};

Q_DECLARE_METATYPE(DeconvolutionPatchJob)
Q_DECLARE_METATYPE(DeconvolutionVolumeJob)
Q_DECLARE_METATYPE(DeconvolutionPatchOutput)
Q_DECLARE_METATYPE(DeconvolutionVolumeOutput)
Q_DECLARE_METATYPE(DeconvolutionRequest)
Q_DECLARE_METATYPE(DeconvolutionProgress)
Q_DECLARE_METATYPE(DeconvolutionRunResult)

#endif // DECONVOLUTIONTYPES_H
