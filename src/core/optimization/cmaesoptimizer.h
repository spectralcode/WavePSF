#ifndef CMAESOPTIMIZER_H
#define CMAESOPTIMIZER_H

#include "ioptimizer.h"

class CMAESOptimizer : public IOptimizer
{
public:
	CMAESOptimizer();
	~CMAESOptimizer() override = default;

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
	double initialSigma;
	int populationSize;   // 0 = auto
	int maxGenerations;

	// Jacobi eigendecomposition for symmetric matrix
	static void eigenDecomposition(const QVector<double>& matrix, int n,
								   QVector<double>& eigenvectors,
								   QVector<double>& eigenvalues);

	static double randomGaussian();
	static double randomDouble(double low, double high);
};

#endif // CMAESOPTIMIZER_H
