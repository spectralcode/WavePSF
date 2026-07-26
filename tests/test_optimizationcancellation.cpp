// Regression test for cancellation during the initial population/simplex
// evaluation: Differential Evolution and Nelder-Mead value-initialized
// their fitness storage to 0.0, so cancelling while the initial candidates
// were being evaluated let an unevaluated candidate win the best-scan and
// a cancelled run reported metric 0.0 as a genuine result.
// Contract: cancelled before any evaluation -> initial coefficients with
// DBL_MAX (the worker's failure sentinel, so the run cannot look
// successful); cancelled after some evaluations -> the best candidate that
// was actually evaluated.
#include "core/optimization/differentialevolutionoptimizer.h"
#include "core/optimization/neldermeadoptimizer.h"
#include <QAtomicInt>
#include <QVector>
#include <cstdio>
#include <limits>

namespace {

const double knownMetric = 42.0;
const double failureSentinel = (std::numeric_limits<double>::max)();

// Case 1: the cancel flag is already set when run() starts. The optimizer
// must not call the objective and must report the failure sentinel.
bool testCancelledBeforeFirstEvaluation(IOptimizer& optimizer)
{
	const QVector<double> initialCoefficients{0.5};
	int evaluations = 0;
	QAtomicInt cancel(1);

	OptimizerResult result = optimizer.run(
		[&evaluations](const QVector<double>&) -> double {
			++evaluations;
			return knownMetric;
		},
		initialCoefficients,
		QVector<int>{0},
		QVector<double>{-1.0},
		QVector<double>{1.0},
		cancel,
		[](const OptimizerProgress&) {});

	if (evaluations != 0) {
		std::fprintf(stderr, "FAIL [%s, cancel before eval]: objective called %d times, expected 0\n",
			qPrintable(optimizer.typeName()), evaluations);
		return false;
	}
	if (result.bestMetric != failureSentinel) {
		std::fprintf(stderr, "FAIL [%s, cancel before eval]: bestMetric=%g, expected the DBL_MAX sentinel\n",
			qPrintable(optimizer.typeName()), result.bestMetric);
		return false;
	}
	if (result.bestCoefficients != initialCoefficients) {
		std::fprintf(stderr, "FAIL [%s, cancel before eval]: bestCoefficients differ from the initial coefficients\n",
			qPrintable(optimizer.typeName()));
		return false;
	}
	if (result.totalIterations != 0) {
		std::fprintf(stderr, "FAIL [%s, cancel before eval]: totalIterations=%d, expected 0\n",
			qPrintable(optimizer.typeName()), result.totalIterations);
		return false;
	}
	return true;
}

// Case 2: the first objective call sets the cancel flag. The optimizer must
// return that first evaluated candidate, not an unevaluated 0.0 one.
bool testCancelledDuringInitialEvaluation(IOptimizer& optimizer)
{
	int evaluations = 0;
	QAtomicInt cancel(0);
	QVector<double> evaluatedCoefficients;

	OptimizerResult result = optimizer.run(
		[&](const QVector<double>& coefficients) -> double {
			++evaluations;
			evaluatedCoefficients = coefficients;
			cancel.storeRelease(1);
			return knownMetric;
		},
		QVector<double>{0.5},
		QVector<int>{0},
		QVector<double>{-1.0},
		QVector<double>{1.0},
		cancel,
		[](const OptimizerProgress&) {});

	if (evaluations != 1) {
		std::fprintf(stderr, "FAIL [%s, cancel during eval]: objective called %d times, expected 1\n",
			qPrintable(optimizer.typeName()), evaluations);
		return false;
	}
	if (result.bestMetric != knownMetric) {
		std::fprintf(stderr, "FAIL [%s, cancel during eval]: bestMetric=%g, expected %g from the evaluated candidate\n",
			qPrintable(optimizer.typeName()), result.bestMetric, knownMetric);
		return false;
	}
	if (result.bestCoefficients != evaluatedCoefficients) {
		std::fprintf(stderr, "FAIL [%s, cancel during eval]: bestCoefficients are not the evaluated candidate's\n",
			qPrintable(optimizer.typeName()));
		return false;
	}
	if (result.totalIterations != 0) {
		std::fprintf(stderr, "FAIL [%s, cancel during eval]: totalIterations=%d, expected 0\n",
			qPrintable(optimizer.typeName()), result.totalIterations);
		return false;
	}
	return true;
}

bool testOptimizer(IOptimizer& optimizer)
{
	bool ok = testCancelledBeforeFirstEvaluation(optimizer);
	ok = testCancelledDuringInitialEvaluation(optimizer) && ok;
	if (ok) {
		std::printf("%s ok\n", qPrintable(optimizer.typeName()));
	}
	return ok;
}

} // namespace

int main()
{
	DifferentialEvolutionOptimizer differentialEvolution;
	NelderMeadOptimizer nelderMead;

	bool ok = testOptimizer(differentialEvolution);
	ok = testOptimizer(nelderMead) && ok;

	if (!ok) {
		return 1;
	}
	std::printf("PASS\n");
	return 0;
}
