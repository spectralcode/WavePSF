#include "deconvolutionworkercontroller.h"
#include "utils/logging.h"

DeconvolutionWorkerController::DeconvolutionWorkerController(QObject* parent)
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

	connect(this, &DeconvolutionWorkerController::runOnWorker,
			this->worker, &DeconvolutionWorker::runDeconvolution,
			Qt::QueuedConnection);

	connect(this->worker, &DeconvolutionWorker::progressUpdated,
			this, &DeconvolutionWorkerController::handleWorkerProgress,
			Qt::QueuedConnection);
	connect(this->worker, &DeconvolutionWorker::patchOutputReady,
			this, &DeconvolutionWorkerController::handleWorkerPatchOutput,
			Qt::QueuedConnection);
	connect(this->worker, &DeconvolutionWorker::volumeOutputReady,
			this, &DeconvolutionWorkerController::handleWorkerVolumeOutput,
			Qt::QueuedConnection);
	connect(this->worker, &DeconvolutionWorker::deconvolutionFinished,
			this, &DeconvolutionWorkerController::handleWorkerFinished,
			Qt::QueuedConnection);

	this->thread->start();
}

DeconvolutionWorkerController::~DeconvolutionWorkerController()
{
	if (this->thread) {
		this->worker->requestCancel();
		this->thread->quit();
		this->thread->wait();
		this->thread = nullptr;
		this->worker = nullptr;
	}
}

void DeconvolutionWorkerController::start(const DeconvolutionRequest& request)
{
	emit this->started();
	emit this->runOnWorker(request);
}

void DeconvolutionWorkerController::cancel()
{
	if (this->worker != nullptr) {
		this->worker->requestCancel();
		LOG_INFO() << "Deconvolution cancellation requested";
	}
}

void DeconvolutionWorkerController::handleWorkerProgress(const DeconvolutionProgress& progress)
{
	emit this->progressUpdated(progress);
}

void DeconvolutionWorkerController::handleWorkerPatchOutput(const DeconvolutionPatchOutput& output)
{
	emit this->patchOutputReady(output);
}

void DeconvolutionWorkerController::handleWorkerVolumeOutput(const DeconvolutionVolumeOutput& output)
{
	emit this->volumeOutputReady(output);
}

void DeconvolutionWorkerController::handleWorkerFinished(const DeconvolutionRunResult& result)
{
	emit this->finished(result);
}
