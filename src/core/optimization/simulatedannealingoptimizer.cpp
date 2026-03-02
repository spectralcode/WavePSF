#include "simulatedannealingoptimizer.h"
#include <QRandomGenerator>
#include <QMutexLocker>
#include <QtMath>
#include <limits>


SimulatedAnnealingOptimizer::SimulatedAnnealingOptimizer()
	: startTemperature(1.0)
	, endTemperature(0.001)
	, coolingFactor(0.95)
	, startPerturbance(0.05)
	, endPerturbance(0.001)
	, iterationsPerTemperature(10)
	, liveEndTemperature(0.001)
	, liveCoolingFactor(0.95)
	, liveStartPerturbance(0.05)
	, liveEndPerturbance(0.001)
	, liveIterationsPerTemperature(10)
{
}

QString SimulatedAnnealingOptimizer::typeName() const
{
	return QStringLiteral("Simulated Annealing");
}

QVector<OptimizerParameter> SimulatedAnnealingOptimizer::getParameterDescriptors() const
{
	return {
		{ "startTemperature",        "Start Temperature",
		  "Initial temperature for Metropolis acceptance criterion.\nHigher = more likely to accept worse solutions early on.",
		  0.001,   1000.0, 0.1,   1.0,   3 },
		{ "endTemperature",          "End Temperature",
		  "Final temperature at which the algorithm stops.\nLower = longer annealing schedule.",
		  0.00001, 100.0,  0.001, 0.001, 5 },
		{ "coolingFactor",           "Cooling Factor",
		  "Temperature multiplier per step (0-1).\nCloser to 1 = slower cooling, more thorough search.",
		  0.01,    0.9999, 0.01,  0.95,  4 },
		{ "startPerturbance",        "Start Perturbance",
		  "Initial random step size for coefficient perturbation.\nLarger = bigger jumps early on.",
		  0.0001,  10.0,   0.01,  0.05,  4 },
		{ "endPerturbance",          "End Perturbance",
		  "Final perturbation size.\nSmaller = finer refinement near the end.",
		  0.0001,  10.0,   0.001, 0.001, 4 },
		{ "iterationsPerTemperature","Iterations/Temperature",
		  "Number of trial moves at each temperature level before cooling.",
		  1,       10000,  1,     10,    0 }
	};
}

QVariantMap SimulatedAnnealingOptimizer::serializeSettings() const
{
	QVariantMap map;
	map["startTemperature"] = this->startTemperature;
	map["endTemperature"] = this->endTemperature;
	map["coolingFactor"] = this->coolingFactor;
	map["startPerturbance"] = this->startPerturbance;
	map["endPerturbance"] = this->endPerturbance;
	map["iterationsPerTemperature"] = this->iterationsPerTemperature;
	return map;
}

void SimulatedAnnealingOptimizer::deserializeSettings(const QVariantMap& settings)
{
	this->startTemperature = settings.value("startTemperature", this->startTemperature).toDouble();
	this->endTemperature = settings.value("endTemperature", this->endTemperature).toDouble();
	this->coolingFactor = settings.value("coolingFactor", this->coolingFactor).toDouble();
	this->startPerturbance = settings.value("startPerturbance", this->startPerturbance).toDouble();
	this->endPerturbance = settings.value("endPerturbance", this->endPerturbance).toDouble();
	this->iterationsPerTemperature = settings.value("iterationsPerTemperature", this->iterationsPerTemperature).toInt();

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
	if (params.contains("endTemperature"))
		this->liveEndTemperature = params.value("endTemperature").toDouble();
	if (params.contains("coolingFactor"))
		this->liveCoolingFactor = params.value("coolingFactor").toDouble();
	if (params.contains("startPerturbance"))
		this->liveStartPerturbance = params.value("startPerturbance").toDouble();
	if (params.contains("endPerturbance"))
		this->liveEndPerturbance = params.value("endPerturbance").toDouble();
	if (params.contains("iterationsPerTemperature"))
		this->liveIterationsPerTemperature = params.value("iterationsPerTemperature").toInt();
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
		double lo = (idx < minBounds.size()) ? minBounds[idx] : -0.3;
		double hi = (idx < maxBounds.size()) ? maxBounds[idx] : 0.3;
		newVal = qBound(lo, newVal, hi);
		coefficients[idx] = newVal;
	}
}

double SimulatedAnnealingOptimizer::randomDouble(double low, double high)
{
	double r = QRandomGenerator::global()->generateDouble();
	return low + r * (high - low);
}
