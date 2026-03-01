#ifndef WAVEFRONTGENERATORFACTORY_H
#define WAVEFRONTGENERATORFACTORY_H

#include <QString>
#include <QStringList>

class IWavefrontGenerator;
class QObject;

class WavefrontGeneratorFactory
{
public:
	static IWavefrontGenerator* create(const QString& typeName, QObject* parent = nullptr);
	static QStringList availableTypeNames();
};

#endif // WAVEFRONTGENERATORFACTORY_H
