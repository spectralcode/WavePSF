#include "optimizerfactory.h"
#include "simulatedannealingoptimizer.h"
#include "cmaesoptimizer.h"


IOptimizer* OptimizerFactory::create(const QString& typeName)
{
	if (typeName == QLatin1String("CMA-ES")) {
		return new CMAESOptimizer();
	}
	// Default: Simulated Annealing
	return new SimulatedAnnealingOptimizer();
}

QStringList OptimizerFactory::availableTypeNames()
{
	return { QStringLiteral("Simulated Annealing"), QStringLiteral("CMA-ES") };
}
