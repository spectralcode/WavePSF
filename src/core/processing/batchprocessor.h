#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <QObject>
#include "deconvolutiontypes.h"

class IPSFGenerator;
class Deconvolver;
struct PatchLayout;

class BatchProcessor : public QObject
{
	Q_OBJECT
public:
	explicit BatchProcessor(QObject* parent = nullptr);

	// GUI-free executor used by worker-side batch and single-patch 3D processing.
	DeconvolutionRunResult executeBatchDeconvolution(
		const DeconvolutionRequest& request,
		IPSFGenerator* generator,
		Deconvolver* deconvolver,
		const DeconvolutionCancelToken* cancelToken = nullptr);

signals:
	void progressUpdated(DeconvolutionProgress progress);
	void patchOutputReady(DeconvolutionPatchOutput output);
	void volumeOutputReady(DeconvolutionVolumeOutput output);

private:
	DeconvolutionRunResult executeBatch2D(
		const DeconvolutionRequest& request,
		const PatchLayout& patchLayout,
		IPSFGenerator* generator,
		Deconvolver* deconvolver,
		const DeconvolutionCancelToken* cancelToken);

	DeconvolutionRunResult executeVolumeJobs(
		const DeconvolutionRequest& request,
		const PatchLayout& patchLayout,
		IPSFGenerator* generator,
		Deconvolver* deconvolver,
		const DeconvolutionCancelToken* cancelToken);
};

#endif // BATCHPROCESSOR_H
