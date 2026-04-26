#ifndef DECONVOLUTIONWORKER_H
#define DECONVOLUTIONWORKER_H

#include <QObject>
#include "deconvolutiontypes.h"

class Deconvolver;

class DeconvolutionWorker : public QObject
{
	Q_OBJECT

public:
	explicit DeconvolutionWorker(QObject* parent = nullptr);
	~DeconvolutionWorker() override;

	void requestCancel();

public slots:
	void runDeconvolution(const DeconvolutionRequest& request);

signals:
	void progressUpdated(DeconvolutionProgress progress);
	void patchOutputReady(DeconvolutionPatchOutput output);
	void volumeOutputReady(DeconvolutionVolumeOutput output);
	void deconvolutionFinished(DeconvolutionRunResult result);
	void error(QString message);

private:
	DeconvolutionRunResult runBatch2D(const DeconvolutionRequest& request);
	DeconvolutionRunResult runVolumeJobs(const DeconvolutionRequest& request);

	DeconvolutionCancelToken cancelToken;
};

#endif // DECONVOLUTIONWORKER_H
