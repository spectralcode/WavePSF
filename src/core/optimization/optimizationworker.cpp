#include "optimizationworker.h"
#include "imagemetriccalculator.h"
#include "core/psf/zernikegenerator.h"
#include "core/psf/psfcalculator.h"
#include "core/psf/deconvolver.h"
#include <QRandomGenerator>
#include <QtMath>
#include <limits>


OptimizationWorker::OptimizationWorker(QObject* parent)
	: QObject(parent)
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
	QVector<int> nollIndices = parseNollIndexSpec(config.psfSettings.nollIndexSpec);
	if (nollIndices.isEmpty()) {
		emit error(QStringLiteral("Invalid Noll index specification."));
		OptimizationResult result;
		result.wasCancelled = false;
		emit optimizationFinished(result);
		return;
	}

	ZernikeGenerator generator;
	generator.setNollIndices(nollIndices);
	generator.setGlobalRange(config.psfSettings.globalMinCoefficient,
							 config.psfSettings.globalMaxCoefficient);
	generator.setStepValue(config.psfSettings.coefficientStep);
	for (auto it = config.psfSettings.coefficientRangeOverrides.constBegin();
		 it != config.psfSettings.coefficientRangeOverrides.constEnd(); ++it) {
		generator.setParameterRange(it.key(), it.value().first, it.value().second);
	}

	PSFCalculator calculator(config.psfSettings.wavelengthNm / 1000.0,
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
			generator.setAllCoefficients(coefficients);
			af::array wavefront = generator.generateWavefront(config.psfSettings.gridSize);
			af::array psf = calculator.computePSF(wavefront);
			af::array deconvolved = deconvolver.deconvolve(inputPatch, psf);
			if (deconvolved.isempty()) return (std::numeric_limits<double>::max)();

			if (config.useReferenceMetric && !groundTruthPatch.isempty()) {
				return ImageMetricCalculator::calculate(
					deconvolved, groundTruthPatch,
					static_cast<ImageMetricCalculator::ReferenceMetric>(config.referenceMetric));
			} else {
				return ImageMetricCalculator::calculate(
					deconvolved,
					static_cast<ImageMetricCalculator::ImageMetric>(config.imageMetric));
			}
		} catch (af::exception&) {
			return (std::numeric_limits<double>::max)();
		}
	};

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

		// Initialize SA state for this job
		QVector<double> currentCoeffs = job.startCoefficients;
		// Source=5: use previous job's best result as starting point
		if (config.startCoefficientSource == 5 && jobIdx > 0 &&
			!finalResult.jobResults.isEmpty()) {
			currentCoeffs = finalResult.jobResults.last().bestCoefficients;
		}
		// If start coefficients are still empty, use zeros
		if (currentCoeffs.isEmpty()) {
			int coeffCount = generator.getAllCoefficients().size();
			currentCoeffs.fill(0.0, coeffCount);
		}

		QVector<double> bestCoeffs = currentCoeffs;
		QVector<double> oldCoeffs = currentCoeffs;
		double temperature = config.startTemperature;
		double metricOld = (std::numeric_limits<double>::max)();
		double bestMetric = metricOld;
		int outerIteration = 0;

		// SA main loop
		while (temperature > config.endTemperature && !this->cancelRequested.loadAcquire()) {
			for (int i = 0; i < config.iterationsPerTemperature; ++i) {
				if (this->cancelRequested.loadAcquire()) break;

				// Evaluate current coefficients
				double metricNew = evaluateMetric(currentCoeffs, job.inputPatch, job.groundTruthPatch);

				// Metropolis acceptance criterion
				if (metricNew < metricOld) {
					// Accept improvement
					metricOld = metricNew;
					bestCoeffs = currentCoeffs;
					bestMetric = metricNew;
					oldCoeffs = currentCoeffs;
				} else {
					double deltaMetric = metricNew - metricOld;
					double acceptProb = qExp(-deltaMetric / temperature);
					if (acceptProb > this->randomDouble(0.0, 1.0)) {
						// Accept worse solution probabilistically
						metricOld = metricNew;
						oldCoeffs = currentCoeffs;
					} else {
						// Reject: revert to old
						currentCoeffs = oldCoeffs;
					}
				}

				// Perturb for next iteration
				double iterPerturbance = config.perturbance / (i + 1.0);
				if (iterPerturbance < 0.00005) iterPerturbance = 0.00005;
				this->perturbCoefficients(currentCoeffs, config.selectedCoefficientIndices,
										  iterPerturbance, config.minBounds, config.maxBounds);
			}

			// Cool down
			temperature *= config.coolingFactor;
			outerIteration++;
			finalResult.totalOuterIterations++;

			// Report progress
			OptimizationProgress progress;
			progress.currentJobIndex = jobIdx;
			progress.totalJobs = config.jobs.size();
			progress.outerIteration = outerIteration;
			progress.currentMetric = metricOld;
			progress.bestMetric = bestMetric;
			progress.temperature = temperature;
			progress.currentFrameNr = job.frameNr;
			progress.currentPatchX = job.patchX;
			progress.currentPatchY = job.patchY;
			progress.currentBestCoefficients = bestCoeffs;
			progress.currentCoefficients = currentCoeffs;
			emit progressUpdated(progress);
		}

		// Store result for this job
		OptimizationJobResult jobResult;
		jobResult.frameNr = job.frameNr;
		jobResult.patchX = job.patchX;
		jobResult.patchY = job.patchY;
		jobResult.bestCoefficients = bestCoeffs;
		jobResult.bestMetric = bestMetric;
		finalResult.jobResults.append(jobResult);
	}

	finalResult.wasCancelled = this->cancelRequested.loadAcquire() != 0;
	emit optimizationFinished(finalResult);
}

void OptimizationWorker::perturbCoefficients(QVector<double>& coefficients,
											 const QVector<int>& selectedIndices,
											 double perturbance,
											 const QVector<double>& minBounds,
											 const QVector<double>& maxBounds)
{
	for (int idx : selectedIndices) {
		if (idx < 0 || idx >= coefficients.size()) continue;
		double current = coefficients[idx];
		double newVal = this->randomDouble(current - perturbance, current + perturbance);
		double lo = (idx < minBounds.size()) ? minBounds[idx] : -0.3;
		double hi = (idx < maxBounds.size()) ? maxBounds[idx] : 0.3;
		newVal = qBound(lo, newVal, hi);
		coefficients[idx] = newVal;
	}
}

double OptimizationWorker::randomDouble(double low, double high)
{
	double r = QRandomGenerator::global()->generateDouble(); // [0, 1)
	return low + r * (high - low);
}
