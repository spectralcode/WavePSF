// Regression test for the Differential Evolution population-size hang:
// the mutation step draws two random indices that must be distinct from
// each other and from the current individual, so populations of 1 or 2
// made those selection loops spin forever. A hang here is the failure.
#include "core/optimization/differentialevolutionoptimizer.h"
#include <QAtomicInt>
#include <QVariantMap>
#include <QVector>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

int main()
{
	// The original bug is an infinite loop, so a hang IS the failure mode.
	std::thread([]() {
		std::this_thread::sleep_for(std::chrono::seconds(10));
		std::fprintf(stderr, "FAIL: optimizer hung (watchdog timeout)\n");
		std::_Exit(1);
	}).detach();

	for (int populationSize : {0, 1, 2, 3, 8}) {
		DifferentialEvolutionOptimizer optimizer;
		QVariantMap settings;
		settings["population_size"] = populationSize;
		settings["max_generations"] = 1;
		optimizer.deserializeSettings(settings);

		int evaluations = 0;
		auto objective = [&evaluations](const QVector<double>& c) -> double {
			++evaluations;
			return c[0] * c[0];
		};

		QAtomicInt cancel(0);
		OptimizerResult result = optimizer.run(
			objective,
			QVector<double>{0.5},
			QVector<int>{0},
			QVector<double>{-1.0},
			QVector<double>{1.0},
			cancel,
			[](const OptimizerProgress&) {});

		if (result.totalIterations != 1 || evaluations < 3
			|| !std::isfinite(result.bestMetric)) {
			std::fprintf(stderr, "FAIL: pop=%d iterations=%d evaluations=%d\n",
				populationSize, result.totalIterations, evaluations);
			return 1;
		}
		std::printf("pop=%d ok (%d evaluations)\n", populationSize, evaluations);
	}

	std::printf("PASS\n");
	return 0;
}
