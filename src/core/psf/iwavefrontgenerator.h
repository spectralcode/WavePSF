#ifndef IWAVEFRONTGENERATOR_H
#define IWAVEFRONTGENERATOR_H

#include <QString>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "wavefrontparameter.h"

class IWavefrontGenerator
{
public:
	virtual ~IWavefrontGenerator() = default;

	// Generator identity and settings persistence
	virtual QString typeName() const = 0;
	virtual QVariantMap serializeSettings() const = 0;
	virtual void deserializeSettings(const QVariantMap& settings) = 0;

	virtual QVector<WavefrontParameter> getParameterDescriptors() const = 0;
	virtual void setCoefficient(int id, double value) = 0;
	virtual double getCoefficient(int id) const = 0;
	virtual QVector<double> getAllCoefficients() const = 0;
	virtual void setAllCoefficients(const QVector<double>& coefficients) = 0;
	virtual void resetCoefficients() = 0;
	virtual af::array generateWavefront(int gridSize) = 0;
};

#endif // IWAVEFRONTGENERATOR_H
