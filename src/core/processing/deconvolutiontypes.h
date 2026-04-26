#ifndef DECONVOLUTIONTYPES_H
#define DECONVOLUTIONTYPES_H

#include <QAtomicInt>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <arrayfire.h>
#include "core/psf/deconvolutionsettings.h"
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
	QVector<double> coefficients;
};

struct DeconvolutionVolumeJob {
	int patchX = 0;
	int patchY = 0;
	int patchIdx = -1;
	int displayFrame = 0;
	QVector<QVector<double>> coefficientsByFrame;
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

	// Snapshot of PSF and deconvolution settings for worker-side execution.
	PSFSettings psfSettings;
	DeconvolutionSettings deconvolutionSettings;
	int patchGridCols = 0;
	int patchGridRows = 0;
	int patchBorderExtension = 0;

	// Batch and single-patch 3D requests snapshot full input frames once
	// and let the worker reconstruct patches or subvolumes on demand.
	QVector<af::array> inputFrames;

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
	DeconvolutionOperationKind operationKind = DeconvolutionOperationKind::UNKNOWN;
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
