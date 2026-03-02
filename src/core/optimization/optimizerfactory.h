#ifndef OPTIMIZERFACTORY_H
#define OPTIMIZERFACTORY_H

#include <QString>
#include <QStringList>

class IOptimizer;

class OptimizerFactory
{
public:
	static IOptimizer* create(const QString& typeName);
	static QStringList availableTypeNames();
};

#endif // OPTIMIZERFACTORY_H
