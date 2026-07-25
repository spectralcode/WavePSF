#include "deconvolutionworker.h"
#include "batchprocessor.h"
#include "core/psf/deconvolver.h"
#include "core/psf/ipsfgenerator.h"
#include "core/psf/psfgeneratorfactory.h"
#include "utils/afdevicemanager.h"
#include <QtGlobal>
#include <QScopedPointer>
#include <exception>

DeconvolutionWorker::DeconvolutionWorker(QObject* parent)
	: QObject(parent)
{
}

DeconvolutionWorker::~DeconvolutionWorker()
{
}

void DeconvolutionWorker::requestCancel()
{
	this->cancelToken.requestCancel();
}

void DeconvolutionWorker::runDeconvolution(const DeconvolutionRequest& request)
{
	this->cancelToken.reset();

	DeconvolutionRunResult result;
	try {
		AFDeviceManager::setDeviceForCurrentThread(request.afBackend, request.afDeviceId);

		switch (request.operationKind) {
			case DeconvolutionOperationKind::SINGLE_PATCH_3D:
				result = this->runVolumeJobs(request);
				break;

			case DeconvolutionOperationKind::BATCH_2D:
				result = this->runBatch2D(request);
				break;

			case DeconvolutionOperationKind::BATCH_3D:
				result = this->runVolumeJobs(request);
				break;

			default:
				result.operationKind = request.operationKind;
				result.totalUnits = request.patchJobs.size() + request.volumeJobs.size();
				result.status = DeconvolutionRunStatus::FAILED;
				result.message = QStringLiteral("DeconvolutionWorker execution is not implemented for this operation.");
				emit this->error(result.message);
				break;
		}
	} catch (const af::exception& e) {
		result.operationKind = request.operationKind;
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("Deconvolution failed with an ArrayFire error: %1")
			.arg(QString::fromUtf8(e.what()));
		emit this->error(result.message);
	} catch (const std::exception& e) {
		result.operationKind = request.operationKind;
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("Deconvolution failed with an error: %1")
			.arg(QString::fromUtf8(e.what()));
		emit this->error(result.message);
	}

	// Emitted exactly once, also on exceptions, so the GUI never gets stuck.
	emit this->deconvolutionFinished(result);
}

DeconvolutionRunResult DeconvolutionWorker::runBatch2D(const DeconvolutionRequest& request)
{
	DeconvolutionRunResult result;
	result.operationKind = request.operationKind;
	result.totalUnits = request.patchJobs.size();

	if (request.patchJobs.isEmpty()) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("No batch 2D deconvolution jobs specified.");
		emit this->error(result.message);
		return result;
	}

	QScopedPointer<IPSFGenerator> generator(PSFGeneratorFactory::create(
		request.psfSettings.generatorTypeName, nullptr));
	if (generator.isNull()) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("Unknown generator type: %1")
			.arg(request.psfSettings.generatorTypeName);
		emit this->error(result.message);
		return result;
	}

	QVariantMap cachedSettings = request.psfSettings.allGeneratorSettings.value(
		request.psfSettings.generatorTypeName);
	if (!cachedSettings.isEmpty()) {
		generator->deserializeSettings(cachedSettings);
	}

	Deconvolver deconvolver(request.deconvolutionSettings.iterations);
	deconvolver.applySettings(request.deconvolutionSettings);
	connect(&deconvolver, &Deconvolver::error,
			this, &DeconvolutionWorker::error);

	BatchProcessor batchProcessor;
	connect(&batchProcessor, &BatchProcessor::progressUpdated,
			this, &DeconvolutionWorker::progressUpdated);
	connect(&batchProcessor, &BatchProcessor::patchOutputReady,
			this, &DeconvolutionWorker::patchOutputReady);
	result = batchProcessor.executeBatchDeconvolution(
		request,
		generator.data(),
		&deconvolver,
		&this->cancelToken);

	return result;
}

DeconvolutionRunResult DeconvolutionWorker::runVolumeJobs(
	const DeconvolutionRequest& request)
{
	DeconvolutionRunResult result;
	result.operationKind = request.operationKind;
	result.totalUnits = request.volumeJobs.size();

	if (request.volumeJobs.isEmpty()) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("No 3D deconvolution jobs specified.");
		emit this->error(result.message);
		return result;
	}

	QScopedPointer<IPSFGenerator> generator(PSFGeneratorFactory::create(
		request.psfSettings.generatorTypeName, nullptr));
	if (generator.isNull()) {
		result.status = DeconvolutionRunStatus::FAILED;
		result.message = QStringLiteral("Unknown generator type: %1")
			.arg(request.psfSettings.generatorTypeName);
		emit this->error(result.message);
		return result;
	}

	QVariantMap cachedSettings = request.psfSettings.allGeneratorSettings.value(
		request.psfSettings.generatorTypeName);
	if (!cachedSettings.isEmpty()) {
		generator->deserializeSettings(cachedSettings);
	}

	Deconvolver deconvolver(request.deconvolutionSettings.iterations);
	deconvolver.applySettings(request.deconvolutionSettings);
	connect(&deconvolver, &Deconvolver::error,
			this, &DeconvolutionWorker::error);

	BatchProcessor batchProcessor;
	connect(&batchProcessor, &BatchProcessor::progressUpdated,
			this, &DeconvolutionWorker::progressUpdated);
	connect(&batchProcessor, &BatchProcessor::volumeOutputReady,
			this, &DeconvolutionWorker::volumeOutputReady);
	result = batchProcessor.executeBatchDeconvolution(
		request,
		generator.data(),
		&deconvolver,
		&this->cancelToken);

	return result;
}
