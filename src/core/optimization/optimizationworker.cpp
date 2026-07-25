#include "optimizationworker.h"
#include "ioptimizer.h"
#include "optimizerfactory.h"
#include "imagemetriccalculator.h"
#include "core/psf/ipsfgenerator.h"
#include "core/psf/psfgeneratorfactory.h"
#include "core/psf/psfmodule.h"
#include "core/psf/deconvolver.h"
#include "utils/afdevicemanager.h"
#include <QScopedPointer>
#include <limits>
#include <cmath>
#include <exception>


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

	// optimizationFinished is emitted exactly once - also on exceptions -
	// so the GUI never gets stuck in a running state.
	OptimizationResult finalResult;
	try {
		this->executeOptimization(config, finalResult);
	} catch (const af::exception& e) {
		finalResult.status = RunStatus::FAILED;
		finalResult.message = QStringLiteral("Optimization failed with an ArrayFire error: %1")
			.arg(QString::fromUtf8(e.what()));
	} catch (const std::exception& e) {
		finalResult.status = RunStatus::FAILED;
		finalResult.message = QStringLiteral("Optimization failed with an error: %1")
			.arg(QString::fromUtf8(e.what()));
	} catch (...) {
		finalResult.status = RunStatus::FAILED;
		finalResult.message = QStringLiteral("Optimization failed with an unknown error.");
	}

	emit optimizationFinished(finalResult);
}

void OptimizationWorker::executeOptimization(const OptimizationConfig& config, OptimizationResult& finalResult)
{
	// Set ArrayFire backend + device for this worker thread (both are per-thread)
	AFDeviceManager::setDeviceForCurrentThread(config.afBackend, config.afDeviceId);

	if (config.jobs.isEmpty()) {
		finalResult.status = RunStatus::FAILED;
		finalResult.message = QStringLiteral("No optimization jobs specified.");
		return;
	}

	if (config.selectedCoefficientIndices.isEmpty()) {
		finalResult.status = RunStatus::FAILED;
		finalResult.message = QStringLiteral("No coefficients selected for optimization.");
		return;
	}

	// Create LOCAL PSF generator on this worker thread
	QScopedPointer<IPSFGenerator> generator(PSFGeneratorFactory::create(
		config.psfSettings.generatorTypeName, nullptr));
	if (generator.isNull()) {
		finalResult.status = RunStatus::FAILED;
		finalResult.message = QStringLiteral("Unknown generator type: %1").arg(config.psfSettings.generatorTypeName);
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
	deconvolver.setTikhonovRegularizationFactor(config.deconvTikhonovRegularizationFactor);
	deconvolver.setWienerNoiseToSignalFactor(config.deconvWienerNoiseToSignalFactor);

	// Helper: evaluate metric for given coefficients and input/reference
	int currentJobFrame = 0;
	auto evaluateMetric = [&](const QVector<double>& coefficients,
							  const af::array& inputPatch,
							  const af::array& groundTruthPatch) -> double {
		try {
			generator->setAllCoefficients(coefficients);
			PSFRequest req;
			req.gridSize = config.psfSettings.gridSize;
			req.frame = currentJobFrame;
			af::array psf = PSFModule::extractFrame(
				generator->generatePSF(req), currentJobFrame);
			af::array deconvolved = deconvolver.deconvolve(inputPatch, psf);
			if (deconvolved.isempty()) return (std::numeric_limits<double>::max)();

			double metric = 0.0;
			if (config.useReferenceMetric && !groundTruthPatch.isempty()) {
				metric = ImageMetricCalculator::calculate(
					deconvolved, groundTruthPatch,
					static_cast<ImageMetricCalculator::ReferenceMetric>(config.referenceMetric));
			} else {
				metric = ImageMetricCalculator::calculate(
					deconvolved,
					static_cast<ImageMetricCalculator::ImageMetric>(config.imageMetric));
			}

			// Validate BEFORE applying the multiplier: the calculator returns
			// DBL_MAX as its failure sentinel, and a negative multiplier would
			// otherwise turn a failed evaluation into the unbeatable best.
			if (!std::isfinite(metric) || metric == (std::numeric_limits<double>::max)()) {
				return (std::numeric_limits<double>::max)();
			}
			double scaledMetric = config.metricMultiplier * metric;
			return std::isfinite(scaledMetric)
				? scaledMetric
				: (std::numeric_limits<double>::max)();
		} catch (af::exception&) {
			return (std::numeric_limits<double>::max)();
		}
	};

	// Create optimizer
	QScopedPointer<IOptimizer> optimizer(OptimizerFactory::create(config.algorithmName));
	if (optimizer.isNull()) {
		finalResult.status = RunStatus::FAILED;
		finalResult.message = QStringLiteral("Unknown optimization algorithm: %1").arg(config.algorithmName);
		return;
	}
	optimizer->deserializeSettings(config.algorithmSettings);
	{
		QMutexLocker locker(&this->optimizerMutex);
		this->currentOptimizer = optimizer.data();
	}

	// Clears the live-parameter target under the mutex before the scoped
	// optimizer is destroyed - including during exception unwinding
	// (declared after the optimizer, so it is destroyed first) - so
	// updateLiveAlgorithmParameters can never touch a dead object.
	struct OptimizerRegistrationGuard {
		OptimizationWorker* worker;
		~OptimizerRegistrationGuard()
		{
			QMutexLocker locker(&this->worker->optimizerMutex);
			this->worker->currentOptimizer = nullptr;
		}
	} registrationGuard{this};
	Q_UNUSED(registrationGuard);

	finalResult.totalOuterIterations = 0;

	// Process each job
	for (int jobIdx = 0; jobIdx < config.jobs.size(); ++jobIdx) {
		if (this->cancelRequested.loadAcquire()) {
			break;
		}

		const OptimizationJob& job = config.jobs[jobIdx];
		currentJobFrame = job.frameNr;

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
		// A best metric stuck at the failure sentinel means no evaluation
		// ever produced a valid result for this job.
		jobResult.succeeded = jobOptResult.bestMetric < (std::numeric_limits<double>::max)();
		if (!jobResult.succeeded) {
			finalResult.failedJobs++;
		}
		finalResult.jobResults.append(jobResult);
	}

	const int totalJobs = finalResult.jobResults.size();
	if (this->cancelRequested.loadAcquire() != 0) {
		finalResult.status = RunStatus::CANCELLED;
		finalResult.message = QStringLiteral("Optimization cancelled.");
	} else if (totalJobs > 0 && finalResult.failedJobs >= totalJobs) {
		finalResult.status = RunStatus::FAILED;
		finalResult.message = QStringLiteral("Optimization failed: none of the %1 jobs found a valid result.")
			.arg(totalJobs);
	} else if (finalResult.failedJobs > 0) {
		finalResult.status = RunStatus::PARTIAL;
		finalResult.message = QStringLiteral("Optimization completed, but %1 of %2 jobs found no valid result.")
			.arg(finalResult.failedJobs).arg(totalJobs);
	} else {
		finalResult.status = RunStatus::COMPLETED;
	}
}
