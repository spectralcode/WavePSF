#include "optimizationcontroller.h"
#include "utils/logging.h"

OptimizationController::OptimizationController(QObject* parent)
	: QObject(parent)
	, thread(nullptr)
	, worker(nullptr)
	, livePreview(false)
	, livePreviewInterval(10)
	, progressCounter(0)
{
	qRegisterMetaType<OptimizationConfig>("OptimizationConfig");
	qRegisterMetaType<OptimizationProgress>("OptimizationProgress");
	qRegisterMetaType<OptimizationResult>("OptimizationResult");

	this->thread = new QThread(this);
	this->worker = new OptimizationWorker();
	this->worker->moveToThread(this->thread);

	connect(this->thread, &QThread::finished,
			this->worker, &QObject::deleteLater);

	// OC → Worker (queued, cross-thread)
	connect(this, &OptimizationController::runOnWorker,
			this->worker, &OptimizationWorker::runOptimization,
			Qt::QueuedConnection);

	// Worker → OC (queued, cross-thread)
	connect(this->worker, &OptimizationWorker::progressUpdated,
			this, &OptimizationController::handleWorkerProgress,
			Qt::QueuedConnection);
	connect(this->worker, &OptimizationWorker::optimizationFinished,
			this, &OptimizationController::handleWorkerFinished,
			Qt::QueuedConnection);

	this->thread->start();
}

OptimizationController::~OptimizationController()
{
	if (this->thread) {
		this->worker->requestCancel();
		this->thread->quit();
		this->thread->wait();
		this->thread = nullptr;
		this->worker = nullptr;  // deleted by QThread::finished → deleteLater
	}
}

void OptimizationController::start(const OptimizationConfig& config)
{
	this->livePreview = config.livePreview;
	this->livePreviewInterval = config.livePreviewInterval;
	this->progressCounter = 0;
	this->progressTimer.start();

	// Worker resets cancelRequested to 0 at the start of runOptimization()
	emit started();
	emit runOnWorker(config);
}

void OptimizationController::cancel()
{
	if (this->worker != nullptr) {
		this->worker->requestCancel();
		LOG_INFO() << "Optimization cancellation requested";
	}
}

void OptimizationController::setLivePreview(bool enabled, int interval)
{
	this->livePreview = enabled;
	this->livePreviewInterval = interval;
	if (enabled) {
		this->progressCounter = 0;
		this->progressTimer.restart();
	}
}

void OptimizationController::updateAlgorithmParameters(const QVariantMap& params)
{
	if (this->worker != nullptr) {
		this->worker->updateLiveAlgorithmParameters(params);
	}
}

void OptimizationController::handleWorkerProgress(const OptimizationProgress& progress)
{
	// Always forward to GUI (the widget throttles expensive replot itself)
	emit progressUpdated(progress);

	// Live preview: throttle GPU work (setAllCoefficients + deconvolution)
	if (this->livePreview) {
		this->progressCounter++;
		if (this->progressCounter >= this->livePreviewInterval &&
			this->progressTimer.elapsed() >= 200) {
			this->progressCounter = 0;
			this->progressTimer.restart();

			if (!progress.currentCoefficients.isEmpty()) {
				emit livePreviewReady(
					progress.currentCoefficients,
					progress.currentFrameNr,
					progress.currentPatchX,
					progress.currentPatchY);
			}
		}
	}
}

void OptimizationController::handleWorkerFinished(const OptimizationResult& result)
{
	emit finished(result);
}
