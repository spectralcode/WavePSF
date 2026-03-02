#ifndef IOPTIMIZER_H
#define IOPTIMIZER_H

#include <QString>
#include <QVector>
#include <QVariantMap>
#include <QAtomicInt>
#include <functional>

struct OptimizerParameter {
	QString key;
	QString name;
	QString tooltip;
	double minValue;
	double maxValue;
	double step;
	double defaultValue;
	int decimals;  // 0 = integer (QSpinBox), >0 = double (QDoubleSpinBox)
};

struct OptimizerProgress {
	int iteration;
	double currentMetric;
	double bestMetric;
	QVector<double> bestCoefficients;
	QVector<double> currentCoefficients;
	QString algorithmStatus;
};

using OptimizerProgressCallback = std::function<void(const OptimizerProgress&)>;

struct OptimizerResult {
	QVector<double> bestCoefficients;
	double bestMetric;
	int totalIterations;
};

class IOptimizer
{
public:
	virtual ~IOptimizer() = default;

	virtual QString typeName() const = 0;

	virtual QVector<OptimizerParameter> getParameterDescriptors() const = 0;

	virtual QVariantMap serializeSettings() const = 0;
	virtual void deserializeSettings(const QVariantMap& settings) = 0;

	// Thread-safe live parameter update (called from main thread while run() executes)
	virtual void updateLiveParameters(const QVariantMap& params) { Q_UNUSED(params); }

	// Execute the optimization. Blocks until done or cancelled.
	virtual OptimizerResult run(
		std::function<double(const QVector<double>&)> objective,
		const QVector<double>& initialCoefficients,
		const QVector<int>& selectedIndices,
		const QVector<double>& lowerBounds,
		const QVector<double>& upperBounds,
		QAtomicInt& cancelFlag,
		OptimizerProgressCallback progressCallback) = 0;
};

#endif // IOPTIMIZER_H
