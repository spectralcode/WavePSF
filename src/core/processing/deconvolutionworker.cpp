#include "deconvolutionworker.h"
#include "utils/afdevicemanager.h"

DeconvolutionWorker::DeconvolutionWorker(QObject* parent)
	: QObject(parent)
{
	this->cancelRequested.storeRelease(0);
}

DeconvolutionWorker::~DeconvolutionWorker()
{
}

void DeconvolutionWorker::requestCancel()
{
	this->cancelRequested.storeRelease(1);
}

void DeconvolutionWorker::runDeconvolution(const DeconvolutionRequest& request)
{
	this->cancelRequested.storeRelease(0);

	AFDeviceManager::setDeviceForCurrentThread(request.afBackend, request.afDeviceId);

	DeconvolutionRunResult result;
	result.totalUnits = request.patchJobs.size() + request.volumeJobs.size();

	if (result.totalUnits == 0) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("No deconvolution jobs specified.");
		emit this->error(result.message);
		emit this->deconvolutionFinished(result);
		return;
	}

	if (this->cancelRequested.loadAcquire() != 0) {
		result.status = DeconvolutionRunStatus::CANCELLED;
		result.message = QStringLiteral("Deconvolution cancelled before execution started.");
		emit this->deconvolutionFinished(result);
		return;
	}

	result.status = DeconvolutionRunStatus::FAILED;
	result.message = QStringLiteral("DeconvolutionWorker execution is not implemented yet.");
	emit this->error(result.message);
	emit this->deconvolutionFinished(result);
}
