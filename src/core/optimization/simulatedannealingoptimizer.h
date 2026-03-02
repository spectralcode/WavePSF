#ifndef SIMULATEDANNEALINGOPTIMIZER_H
#define SIMULATEDANNEALINGOPTIMIZER_H

#include "ioptimizer.h"
#include <QMutex>

class SimulatedAnnealingOptimizer : public IOptimizer
{
public:
	SimulatedAnnealingOptimizer();
	~SimulatedAnnealingOptimizer() override = default;

	QString typeName() const override;
	QVector<OptimizerParameter> getParameterDescriptors() const override;
	QVariantMap serializeSettings() const override;
	void deserializeSettings(const QVariantMap& settings) override;
	void updateLiveParameters(const QVariantMap& params) override;

	OptimizerResult run(
		std::function<double(const QVector<double>&)> objective,
		const QVector<double>& initialCoefficients,
		const QVector<int>& selectedIndices,
		const QVector<double>& lowerBounds,
		const QVector<double>& upperBounds,
		QAtomicInt& cancelFlag,
		OptimizerProgressCallback progressCallback) override;

private:
	double startTemperature;
	double endTemperature;
	double coolingFactor;
	double startPerturbance;
	double endPerturbance;
	int iterationsPerTemperature;

	// Live-updatable copies (protected by mutex)
	QMutex liveParamsMutex;
	double liveEndTemperature;
	double liveCoolingFactor;
	double liveStartPerturbance;
	double liveEndPerturbance;
	int liveIterationsPerTemperature;

	void perturbCoefficients(QVector<double>& coefficients,
							 const QVector<int>& selectedIndices,
							 double perturbance,
							 const QVector<double>& minBounds,
							 const QVector<double>& maxBounds);
	double randomDouble(double low, double high);
};

#endif // SIMULATEDANNEALINGOPTIMIZER_H
