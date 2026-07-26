#ifndef DIFFERENTIALEVOLUTIONOPTIMIZER_H
#define DIFFERENTIALEVOLUTIONOPTIMIZER_H

#include "ioptimizer.h"

class DifferentialEvolutionOptimizer : public IOptimizer
{
public:
	DifferentialEvolutionOptimizer();
	~DifferentialEvolutionOptimizer() override = default;

	QString typeName() const override;
	QVector<OptimizerParameter> getParameterDescriptors() const override;
	QVariantMap serializeSettings() const override;
	void deserializeSettings(const QVariantMap& settings) override;

	OptimizerResult run(
		std::function<double(const QVector<double>&)> objective,
		const QVector<double>& initialCoefficients,
		const QVector<int>& selectedIndices,
		const QVector<double>& lowerBounds,
		const QVector<double>& upperBounds,
		QAtomicInt& cancelFlag,
		OptimizerProgressCallback progressCallback) override;

private:
	double mutationFactor;   // F
	double crossoverRate;    // CR
	int populationSize;      // 0 = auto (N+2, min 6); explicit 1-2 raised to 3
	int maxGenerations;

	static double randomDouble(double low, double high);
	static int randomInt(int low, int high);
};

#endif // DIFFERENTIALEVOLUTIONOPTIMIZER_H
