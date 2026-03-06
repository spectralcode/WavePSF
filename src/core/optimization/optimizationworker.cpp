#include "optimizationworker.h"
#include "ioptimizer.h"
#include "optimizerfactory.h"
#include "imagemetriccalculator.h"
#include "core/psf/iwavefrontgenerator.h"
#include "core/psf/wavefrontgeneratorfactory.h"
#include "core/psf/psfcalculator.h"
#include "core/psf/deconvolver.h"
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

	// Create LOCAL PSF pipeline instances on this worker thread
	IWavefrontGenerator* generator = WavefrontGeneratorFactory::create(
		config.psfSettings.generatorTypeName, nullptr);
	if (!generator) {
		emit error(QStringLiteral("Unknown generator type: %1").arg(config.psfSettings.generatorTypeName));
		OptimizationResult result;
		result.wasCancelled = false;
		emit optimizationFinished(result);
		return;
	}
	generator->deserializeSettings(config.psfSettings.generatorSettings);

	PSFCalculator calculator(config.psfSettings.phaseScale,
							 config.psfSettings.apertureRadius);
	calculator.setNormalizationMode(
		static_cast<PSFCalculator::NormalizationMode>(config.psfSettings.normalizationMode));

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
			af::array wavefront = generator->generateWavefront(config.psfSettings.gridSize);
			af::array psf = calculator.computePSF(wavefront);
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
