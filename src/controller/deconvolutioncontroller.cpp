#include "deconvolutioncontroller.h"
#include "utils/logging.h"

DeconvolutionController::DeconvolutionController(QObject* parent)
	: QObject(parent)
	, thread(nullptr)
	, worker(nullptr)
{
	qRegisterMetaType<DeconvolutionPatchOutput>("DeconvolutionPatchOutput");
	qRegisterMetaType<DeconvolutionVolumeOutput>("DeconvolutionVolumeOutput");
	qRegisterMetaType<DeconvolutionRequest>("DeconvolutionRequest");
	qRegisterMetaType<DeconvolutionProgress>("DeconvolutionProgress");
	qRegisterMetaType<DeconvolutionRunResult>("DeconvolutionRunResult");

	this->thread = new QThread(this);
	this->worker = new DeconvolutionWorker();
	this->worker->moveToThread(this->thread);

	connect(this->thread, &QThread::finished,
			this->worker, &QObject::deleteLater);

	connect(this, &DeconvolutionController::runOnWorker,
			this->worker, &DeconvolutionWorker::runDeconvolution,
			Qt::QueuedConnection);

	connect(this->worker, &DeconvolutionWorker::progressUpdated,
			this, &DeconvolutionController::handleWorkerProgress,
			Qt::QueuedConnection);
	connect(this->worker, &DeconvolutionWorker::patchOutputReady,
			this, &DeconvolutionController::handleWorkerPatchOutput,
			Qt::QueuedConnection);
	connect(this->worker, &DeconvolutionWorker::volumeOutputReady,
			this, &DeconvolutionController::handleWorkerVolumeOutput,
			Qt::QueuedConnection);
	connect(this->worker, &DeconvolutionWorker::deconvolutionFinished,
			this, &DeconvolutionController::handleWorkerFinished,
			Qt::QueuedConnection);

	this->thread->start();
}

DeconvolutionController::~DeconvolutionController()
{
	if (this->thread) {
		this->worker->requestCancel();
		this->thread->quit();
		this->thread->wait();
		this->thread = nullptr;
		this->worker = nullptr;
	}
}

void DeconvolutionController::start(const DeconvolutionRequest& request)
{
	emit this->started();
	emit this->runOnWorker(request);
}

void DeconvolutionController::cancel()
{
	if (this->worker != nullptr) {
		this->worker->requestCancel();
		LOG_INFO() << "Deconvolution cancellation requested";
	}
}

void DeconvolutionController::handleWorkerProgress(const DeconvolutionProgress& progress)
{
	emit this->progressUpdated(progress);
}

void DeconvolutionController::handleWorkerPatchOutput(const DeconvolutionPatchOutput& output)
{
	emit this->patchOutputReady(output);
}

void DeconvolutionController::handleWorkerVolumeOutput(const DeconvolutionVolumeOutput& output)
{
	emit this->volumeOutputReady(output);
}

void DeconvolutionController::handleWorkerFinished(const DeconvolutionRunResult& result)
{
	emit this->finished(result);
}
