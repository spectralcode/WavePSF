#include "optimizerfactory.h"
#include "simulatedannealingoptimizer.h"
#include "cmaesoptimizer.h"
#include "differentialevolutionoptimizer.h"
#include "neldermeadoptimizer.h"


IOptimizer* OptimizerFactory::create(const QString& typeName)
{
	if (typeName == QLatin1String("CMA-ES")) {
		return new CMAESOptimizer();
	}
	if (typeName == QLatin1String("Differential Evolution")) {
		return new DifferentialEvolutionOptimizer();
	}
	if (typeName == QLatin1String("Nelder-Mead")) {
		return new NelderMeadOptimizer();
	}
	// Default: Simulated Annealing
	return new SimulatedAnnealingOptimizer();
}

QStringList OptimizerFactory::availableTypeNames()
{
	return { QStringLiteral("Simulated Annealing"), QStringLiteral("CMA-ES"),
			 QStringLiteral("Differential Evolution"), QStringLiteral("Nelder-Mead") };
}
