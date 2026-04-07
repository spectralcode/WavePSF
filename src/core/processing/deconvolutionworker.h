#ifndef DECONVOLUTIONWORKER_H
#define DECONVOLUTIONWORKER_H

#include <QObject>
#include <QAtomicInt>
#include "deconvolutiontypes.h"

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
	QAtomicInt cancelRequested;
};

#endif // DECONVOLUTIONWORKER_H
