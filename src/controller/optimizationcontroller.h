#ifndef OPTIMIZATIONCONTROLLER_H
#define OPTIMIZATIONCONTROLLER_H

#include <QObject>
#include <QThread>
#include <QElapsedTimer>
#include <QVector>
#include "core/optimization/optimizationworker.h"

class OptimizationController : public QObject
{
	Q_OBJECT

public:
	explicit OptimizationController(QObject* parent = nullptr);
	~OptimizationController() override;

public slots:
	void start(const OptimizationConfig& config);
	void cancel();
	void setLivePreview(bool enabled, int interval);
	void updateAlgorithmParameters(const QVariantMap& params);

signals:
	void started();
	void progressUpdated(OptimizationProgress progress);
	void livePreviewReady(QVector<double> coefficients, int frame, int patchX, int patchY);
	void finished(OptimizationResult result);

	// Internal: queued dispatch to worker thread
	void runOnWorker(OptimizationConfig config);

private slots:
	void handleWorkerProgress(const OptimizationProgress& progress);
	void handleWorkerFinished(const OptimizationResult& result);

private:
	QThread* thread;
	OptimizationWorker* worker;

	bool livePreview;
	int livePreviewInterval;
	int progressCounter;
	QElapsedTimer progressTimer;
};

#endif // OPTIMIZATIONCONTROLLER_H
