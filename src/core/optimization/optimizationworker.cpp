#include "optimizationworker.h"
#include "ioptimizer.h"
#include "optimizerfactory.h"
#include "imagemetriccalculator.h"
#include "core/psf/ipsfgenerator.h"
#include "core/psf/psfgeneratorfactory.h"
#include "core/psf/psfmodule.h"
#include "core/psf/deconvolver.h"
#include "utils/afdevicemanager.h"
#include <limits>


OptimizationWorker::OptimizationWorker(QObject* parent)
	: QObject(parent)
	, currentOptimizer(nullptr)
{
	this->cancelRequested.storeRelease(0);
}

OptimizationWorker::~OptimizationWorker()
{
}

void OptimizationWorker::requestCancel()
{
	this->cancelRequested.storeRelease(1);
}

void OptimizationWorker::updateLiveAlgorithmParameters(const QVariantMap& params)
{
	QMutexLocker locker(&this->optimizerMutex);
	if (this->currentOptimizer != nullptr) {
		this->currentOptimizer->updateLiveParameters(params);
	}
}

void OptimizationWorker::runOptimization(const OptimizationConfig& config)
{
	this->cancelRequested.storeRelease(0);

	// Set ArrayFire backend + device for this worker thread (both are per-thread)
	AFDeviceManager::setDeviceForCurrentThread(config.afBackend, config.afDeviceId);

	if (config.jobs.isEmpty()) {
		emit error(QStringLiteral("No optimization jobs specified."));
		OptimizationResult result;
		result.wasCancelled = false;
		emit optimizationFinished(result);
		return;
	}

	if (config.selectedCoefficientIndices.isEmpty()) {
		emit error(QStringLiteral("No coefficients selected for optimization."));
		OptimizationResult result;
		result.wasCancelled = false;
		emit optimizationFinished(result);
		return;
	}

	// Create LOCAL PSF generator on this worker thread
	IPSFGenerator* generator = PSFGeneratorFactory::create(
		config.psfSettings.generatorTypeName, nullptr);
	if (!generator) {
		emit error(QStringLiteral("Unknown generator type: %1").arg(config.psfSettings.generatorTypeName));
		OptimizationResult result;
		result.wasCancelled = false;
		emit optimizationFinished(result);
		return;
	}
	QVariantMap cachedSettings = config.psfSettings.allGeneratorSettings.value(
		config.psfSettings.generatorTypeName);
	if (!cachedSettings.isEmpty()) {
		generator->deserializeSettings(cachedSettings);
	}

	Deconvolver deconvolver(config.deconvIterations);
	deconvolver.setAlgorithm(static_cast<Deconvolver::Algorithm>(config.deconvAlgorithm));
	deconvolver.setRelaxationFactor(config.deconvRelaxationFactor);
	deconvolver.setRegularizationFactor(config.deconvRegularizationFactor);
	deconvolver.setNoiseToSignalFactor(config.deconvNoiseToSignalFactor);

	// Helper: evaluate metric for given coefficients and input/reference
	auto evaluateMetric = [&](const QVector<double>& coefficients,
							  const af::array& inputPatch,
							  const af::array& groundTruthPatch) -> double {
		try {
			generator->setAllCoefficients(coefficients);
			af::array psf = PSFModule::focalSlice(
				generator->generatePSF(config.psfSettings.gridSize));
			af::array deconvolved = deconvolver.deconvolve(inputPatch, psf);
			if (deconvolved.isempty()) return (std::numeric_limits<double>::max)();

			if (config.useReferenceMetric && !groundTruthPatch.isempty()) {
				return config.metricMultiplier * ImageMetricCalculator::calculate(
					deconvolved, groundTruthPatch,
					static_cast<ImageMetricCalculator::ReferenceMetric>(config.referenceMetric));
			} else {
				return config.metricMultiplier * ImageMetricCalculator::calculate(
					deconvolved,
					static_cast<ImageMetricCalculator::ImageMetric>(config.imageMetric));
			}
		} catch (af::exception&) {
			return (std::numeric_limits<double>::max)();
		}
	};

	// Create optimizer
	IOptimizer* optimizer = OptimizerFactory::create(config.algorithmName);
	optimizer->deserializeSettings(config.algorithmSettings);
	{
		QMutexLocker locker(&this->optimizerMutex);
		this->currentOptimizer = optimizer;
	}

	OptimizationResult finalResult;
	finalResult.totalOuterIterations = 0;
	finalResult.wasCancelled = false;

	// Process each job
	for (int jobIdx = 0; jobIdx < config.jobs.size(); ++jobIdx) {
		if (this->cancelRequested.loadAcquire()) {
			finalResult.wasCancelled = true;
			break;
		}

		const OptimizationJob& job = config.jobs[jobIdx];

		// Determine start coefficients
		QVector<double> startCoeffs = job.startCoefficients;
		if (config.startCoefficientSource == 5 && jobIdx > 0 &&
			!finalResult.jobResults.isEmpty()) {
			startCoeffs = finalResult.jobResults.last().bestCoefficients;
		}
		// "From relative frame": if the source frame was optimized
		// earlier in this batch, use those results instead of the stale
		// pre-built values from the job builder
		if (config.startCoefficientSource == 2 && !finalResult.jobResults.isEmpty()) {
			int sourceFrame = qBound(0, job.frameNr + config.sourceParam, config.jobs.last().frameNr);
			for (int i = finalResult.jobResults.size() - 1; i >= 0; --i) {
				const OptimizationJobResult& prev = finalResult.jobResults[i];
				if (prev.frameNr == sourceFrame && prev.patchX == job.patchX && prev.patchY == job.patchY) {
					startCoeffs = prev.bestCoefficients;
					break;
				}
			}
		}
		if (startCoeffs.isEmpty()) {
			int coeffCount = generator->getAllCoefficients().size();
			startCoeffs.fill(0.0, coeffCount);
		}

		// Build objective function for this job
		auto objective = [&](const QVector<double>& coefficients) -> double {
			return evaluateMetric(coefficients, job.inputPatch, job.groundTruthPatch);
		};

		// Build progress callback
		auto progressCb = [&](const OptimizerProgress& optProg) {
			finalResult.totalOuterIterations = optProg.iteration;

			OptimizationProgress progress;
			progress.currentJobIndex = jobIdx;
			progress.totalJobs = config.jobs.size();
			progress.outerIteration = optProg.iteration;
			progress.currentMetric = optProg.currentMetric;
			progress.bestMetric = optProg.bestMetric;
			progress.algorithmStatus = optProg.algorithmStatus;
			progress.currentFrameNr = job.frameNr;
			progress.currentPatchX = job.patchX;
			progress.currentPatchY = job.patchY;
			progress.currentBestCoefficients = optProg.bestCoefficients;
			progress.currentCoefficients = optProg.currentCoefficients;
			emit progressUpdated(progress);
		};

		OptimizerResult jobOptResult = optimizer->run(
			objective,
			startCoeffs,
			config.selectedCoefficientIndices,
			config.minBounds,
			config.maxBounds,
			this->cancelRequested,
			progressCb);

		OptimizationJobResult jobResult;
		jobResult.frameNr = job.frameNr;
		jobResult.patchX = job.patchX;
		jobResult.patchY = job.patchY;
		jobResult.bestCoefficients = jobOptResult.bestCoefficients;
		jobResult.bestMetric = jobOptResult.bestMetric;
		finalResult.jobResults.append(jobResult);
	}

	// Cleanup
	{
		QMutexLocker locker(&this->optimizerMutex);
		this->currentOptimizer = nullptr;
	}
	delete optimizer;
	delete generator;

	finalResult.wasCancelled = this->cancelRequested.loadAcquire() != 0;
	emit optimizationFinished(finalResult);
}
