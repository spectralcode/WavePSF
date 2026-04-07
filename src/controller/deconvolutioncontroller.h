#ifndef DECONVOLUTIONCONTROLLER_H
#define DECONVOLUTIONCONTROLLER_H

#include <QObject>
#include <QThread>
#include "core/processing/deconvolutionworker.h"

class DeconvolutionController : public QObject
{
	Q_OBJECT

public:
	explicit DeconvolutionController(QObject* parent = nullptr);
	~DeconvolutionController() override;

public slots:
	void start(const DeconvolutionRequest& request);
	void cancel();

signals:
	void started();
	void progressUpdated(DeconvolutionProgress progress);
	void patchOutputReady(DeconvolutionPatchOutput output);
	void volumeOutputReady(DeconvolutionVolumeOutput output);
	void finished(DeconvolutionRunResult result);

	// Internal: queued dispatch to worker thread.
	void runOnWorker(DeconvolutionRequest request);

private slots:
	void handleWorkerProgress(const DeconvolutionProgress& progress);
	void handleWorkerPatchOutput(const DeconvolutionPatchOutput& output);
	void handleWorkerVolumeOutput(const DeconvolutionVolumeOutput& output);
	void handleWorkerFinished(const DeconvolutionRunResult& result);

private:
	QThread* thread;
	DeconvolutionWorker* worker;
};

#endif // DECONVOLUTIONCONTROLLER_H
