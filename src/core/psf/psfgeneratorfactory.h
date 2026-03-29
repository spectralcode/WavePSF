#ifndef PSFGENERATORFACTORY_H
#define PSFGENERATORFACTORY_H

#include <QString>
#include <QStringList>

class IPSFGenerator;
class QObject;

class PSFGeneratorFactory
{
public:
	static IPSFGenerator* create(const QString& typeName, QObject* parent = nullptr);
	static QStringList availableTypeNames();
};

#endif // PSFGENERATORFACTORY_H
