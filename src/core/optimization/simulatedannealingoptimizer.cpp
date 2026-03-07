#include "simulatedannealingoptimizer.h"
#include <QRandomGenerator>
#include <QMutexLocker>
#include <QtMath>
#include <limits>

namespace {
	// Key names
	const QString KEY_START_TEMPERATURE          = QStringLiteral("start_temperature");
	const QString KEY_END_TEMPERATURE            = QStringLiteral("end_temperature");
	const QString KEY_COOLING_FACTOR             = QStringLiteral("cooling_factor");
	const QString KEY_START_PERTURBANCE          = QStringLiteral("start_perturbance");
	const QString KEY_END_PERTURBANCE            = QStringLiteral("end_perturbance");
	const QString KEY_ITERATIONS_PER_TEMPERATURE = QStringLiteral("iterations_per_temperature");

	// Default values (match constructor initialization)
	const double DEF_START_TEMPERATURE          = 0.4;
	const double DEF_END_TEMPERATURE            = 0.001;
	const double DEF_COOLING_FACTOR             = 0.996;
	const double DEF_START_PERTURBANCE          = 0.2;
	const double DEF_END_PERTURBANCE            = 0.02;
	const int    DEF_ITERATIONS_PER_TEMPERATURE = 1;
}


SimulatedAnnealingOptimizer::SimulatedAnnealingOptimizer()
	: startTemperature(DEF_START_TEMPERATURE)
	, endTemperature(DEF_END_TEMPERATURE)
	, coolingFactor(DEF_COOLING_FACTOR)
	, startPerturbance(DEF_START_PERTURBANCE)
	, endPerturbance(DEF_END_PERTURBANCE)
	, iterationsPerTemperature(DEF_ITERATIONS_PER_TEMPERATURE)
	, liveEndTemperature(DEF_END_TEMPERATURE)
	, liveCoolingFactor(DEF_COOLING_FACTOR)
	, liveStartPerturbance(DEF_START_PERTURBANCE)
	, liveEndPerturbance(DEF_END_PERTURBANCE)
	, liveIterationsPerTemperature(DEF_ITERATIONS_PER_TEMPERATURE)
{
}

QString SimulatedAnnealingOptimizer::typeName() const
{
	return QStringLiteral("Simulated Annealing");
}

QVector<OptimizerParameter> SimulatedAnnealingOptimizer::getParameterDescriptors() const
{
	return {
		{ KEY_START_TEMPERATURE,          "Start Temperature",
		  "Initial temperature for Metropolis acceptance criterion.\nHigher = more likely to accept worse solutions early on.",
		  0.001,   1000.0, 0.1,   0.4,   3 },
		{ KEY_END_TEMPERATURE,            "End Temperature",
		  "Final temperature at which the algorithm stops.\nLower = longer annealing schedule.",
		  0.00001, 100.0,  0.001, 0.001, 5 },
		{ KEY_COOLING_FACTOR,             "Cooling Factor",
		  "Temperature multiplier per step (0-1).\nCloser to 1 = slower cooling, more thorough search.",
		  0.01,    0.9999, 0.001, 0.996, 4 },
		{ KEY_START_PERTURBANCE,          "Start Perturbance",
		  "Initial random step size for coefficient perturbation.\nLarger = bigger jumps early on.",
		  0.0001,  10.0,   0.001, DEF_START_PERTURBANCE, 4 },
		{ KEY_END_PERTURBANCE,            "End Perturbance",
		  "Final perturbation size.\nSmaller = finer refinement near the end.",
		  0.0001,  10.0,   0.0001, DEF_END_PERTURBANCE, 4 },
		{ KEY_ITERATIONS_PER_TEMPERATURE, "Iterations/Temperature",
		  "Number of trial moves at each temperature level before cooling.",
		  1,       10000,  1,     1,     0 }
	};
}

QVariantMap SimulatedAnnealingOptimizer::serializeSettings() const
{
	QVariantMap map;
	map[KEY_START_TEMPERATURE]          = this->startTemperature;
	map[KEY_END_TEMPERATURE]            = this->endTemperature;
	map[KEY_COOLING_FACTOR]             = this->coolingFactor;
	map[KEY_START_PERTURBANCE]          = this->startPerturbance;
	map[KEY_END_PERTURBANCE]            = this->endPerturbance;
	map[KEY_ITERATIONS_PER_TEMPERATURE] = this->iterationsPerTemperature;
	return map;
}

void SimulatedAnnealingOptimizer::deserializeSettings(const QVariantMap& settings)
{
	this->startTemperature          = settings.value(KEY_START_TEMPERATURE,          DEF_START_TEMPERATURE).toDouble();
	this->endTemperature            = settings.value(KEY_END_TEMPERATURE,            DEF_END_TEMPERATURE).toDouble();
	this->coolingFactor             = settings.value(KEY_COOLING_FACTOR,             DEF_COOLING_FACTOR).toDouble();
	this->startPerturbance          = settings.value(KEY_START_PERTURBANCE,          DEF_START_PERTURBANCE).toDouble();
	this->endPerturbance            = settings.value(KEY_END_PERTURBANCE,            DEF_END_PERTURBANCE).toDouble();
	this->iterationsPerTemperature  = settings.value(KEY_ITERATIONS_PER_TEMPERATURE, DEF_ITERATIONS_PER_TEMPERATURE).toInt();

	// Initialize live copies
	QMutexLocker locker(&this->liveParamsMutex);
	this->liveEndTemperature = this->endTemperature;
	this->liveCoolingFactor = this->coolingFactor;
	this->liveStartPerturbance = this->startPerturbance;
	this->liveEndPerturbance = this->endPerturbance;
	this->liveIterationsPerTemperature = this->iterationsPerTemperature;
}

void SimulatedAnnealingOptimizer::updateLiveParameters(const QVariantMap& params)
{
	QMutexLocker locker(&this->liveParamsMutex);
	if (params.contains(KEY_END_TEMPERATURE))
		this->liveEndTemperature = params.value(KEY_END_TEMPERATURE).toDouble();
	if (params.contains(KEY_COOLING_FACTOR))
		this->liveCoolingFactor = params.value(KEY_COOLING_FACTOR).toDouble();
	if (params.contains(KEY_START_PERTURBANCE))
		this->liveStartPerturbance = params.value(KEY_START_PERTURBANCE).toDouble();
	if (params.contains(KEY_END_PERTURBANCE))
		this->liveEndPerturbance = params.value(KEY_END_PERTURBANCE).toDouble();
	if (params.contains(KEY_ITERATIONS_PER_TEMPERATURE))
		this->liveIterationsPerTemperature = params.value(KEY_ITERATIONS_PER_TEMPERATURE).toInt();
}

OptimizerResult SimulatedAnnealingOptimizer::run(
	std::function<double(const QVector<double>&)> objective,
	const QVector<double>& initialCoefficients,
	const QVector<int>& selectedIndices,
	const QVector<double>& lowerBounds,
	const QVector<double>& upperBounds,
	QAtomicInt& cancelFlag,
	OptimizerProgressCallback progressCallback)
{
	QVector<double> currentCoeffs = initialCoefficients;
	QVector<double> bestCoeffs = currentCoeffs;
	QVector<double> oldCoeffs = currentCoeffs;
	double temperature = this->startTemperature;
	double metricOld = (std::numeric_limits<double>::max)();
	double bestMetric = metricOld;
	int outerIteration = 0;

	// Local copies of live-updatable parameters
	double saEndTemp, saCoolingFactor, saStartPerturb, saEndPerturb;
	int saItersPerTemp;
	{
		QMutexLocker locker(&this->liveParamsMutex);
		saEndTemp = this->liveEndTemperature;
		saCoolingFactor = this->liveCoolingFactor;
		saStartPerturb = this->liveStartPerturbance;
		saEndPerturb = this->liveEndPerturbance;
		saItersPerTemp = this->liveIterationsPerTemperature;
	}

	while (temperature > saEndTemp && !cancelFlag.loadAcquire()) {
		for (int i = 0; i < saItersPerTemp; ++i) {
			if (cancelFlag.loadAcquire()) break;

			double metricNew = objective(currentCoeffs);

			if (metricNew < metricOld) {
				metricOld = metricNew;
				oldCoeffs = currentCoeffs;
				if (metricNew < bestMetric) {
					bestCoeffs = currentCoeffs;
					bestMetric = metricNew;
				}
			} else {
				double deltaMetric = metricNew - metricOld;
				double acceptProb = qExp(-deltaMetric / temperature);
				if (acceptProb > this->randomDouble(0.0, 1.0)) {
					metricOld = metricNew;
					oldCoeffs = currentCoeffs;
				} else {
					currentCoeffs = oldCoeffs;
				}
			}

			double t = (temperature - saEndTemp) / (this->startTemperature - saEndTemp);
			double basePerturbance = saEndPerturb + t * (saStartPerturb - saEndPerturb);
			double iterPerturbance = basePerturbance / (i + 1.0);
			if (iterPerturbance < 0.00005) iterPerturbance = 0.00005;
			this->perturbCoefficients(currentCoeffs, selectedIndices,
									  iterPerturbance, lowerBounds, upperBounds);
		}

		temperature *= saCoolingFactor;
		outerIteration++;

		// Read live parameter updates
		{
			QMutexLocker locker(&this->liveParamsMutex);
			saEndTemp = this->liveEndTemperature;
			saCoolingFactor = this->liveCoolingFactor;
			saStartPerturb = this->liveStartPerturbance;
			saEndPerturb = this->liveEndPerturbance;
			saItersPerTemp = this->liveIterationsPerTemperature;
		}

		// Report progress
		OptimizerProgress progress;
		progress.iteration = outerIteration;
		progress.currentMetric = metricOld;
		progress.bestMetric = bestMetric;
		progress.bestCoefficients = bestCoeffs;
		progress.currentCoefficients = currentCoeffs;
		progress.algorithmStatus = QString("T: %1").arg(temperature, 0, 'g', 4);
		progressCallback(progress);
	}

	OptimizerResult result;
	result.bestCoefficients = bestCoeffs;
	result.bestMetric = bestMetric;
	result.totalIterations = outerIteration;
	return result;
}

void SimulatedAnnealingOptimizer::perturbCoefficients(QVector<double>& coefficients,
													  const QVector<int>& selectedIndices,
													  double perturbance,
													  const QVector<double>& minBounds,
													  const QVector<double>& maxBounds)
{
	for (int idx : selectedIndices) {
		if (idx < 0 || idx >= coefficients.size()) continue;
		double current = coefficients[idx];
		double newVal = this->randomDouble(current - perturbance, current + perturbance);
		double lo = minBounds[idx];
		double hi = maxBounds[idx];
		// Reflect off boundaries so the perturbation distribution stays symmetric.
		// Simple clamp would cause the optimizer to pile up at the boundary.
		if (newVal > hi) newVal = 2.0 * hi - newVal;
		if (newVal < lo) newVal = 2.0 * lo - newVal;
		newVal = qBound(lo, newVal, hi); // safety clamp if perturbance > range
		coefficients[idx] = newVal;
	}
}

double SimulatedAnnealingOptimizer::randomDouble(double low, double high)
{
	double r = QRandomGenerator::global()->generateDouble();
	return low + r * (high - low);
}
