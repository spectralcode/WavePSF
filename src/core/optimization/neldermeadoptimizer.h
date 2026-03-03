#ifndef NELDERMEADOPTIMIZER_H
#define NELDERMEADOPTIMIZER_H

#include "ioptimizer.h"

class NelderMeadOptimizer : public IOptimizer
{
public:
	NelderMeadOptimizer();
	~NelderMeadOptimizer() override = default;

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
	int maxIterations;
	double initialStepSize;  // fraction of parameter range
};

#endif // NELDERMEADOPTIMIZER_H
