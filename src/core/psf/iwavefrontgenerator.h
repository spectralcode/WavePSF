#ifndef IWAVEFRONTGENERATOR_H
#define IWAVEFRONTGENERATOR_H

#include <QString>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "wavefrontparameter.h"

// Descriptor for a single numeric generator configuration setting.
// Used by PSFSettingsDialog to auto-build UI for generator-specific settings.
// Mirrors the OptimizerParameter pattern used in IOptimizer.
struct WavefrontGeneratorSetting {
	QString key;          // serialization key (must match serializeSettings/deserializeSettings)
	QString name;         // UI label
	QString tooltip;      // UI help text
	double minValue;
	double maxValue;
	double step;
	double defaultValue;
	int decimals;         // 0 = QSpinBox (integer), >0 = QDoubleSpinBox with N decimal places
};

class IWavefrontGenerator
{
public:
	virtual ~IWavefrontGenerator() = default;

	// Generator identity and settings persistence
	virtual QString typeName() const = 0;
	virtual QVariantMap serializeSettings() const = 0;
	virtual void deserializeSettings(const QVariantMap& settings) = 0;

	// Returns descriptors for simple numeric generator settings.
	// PSFSettingsDialog auto-builds UI from these — no dialog changes needed for new generators.
	// Default: empty list (used by generators with fully custom UI, e.g. ZernikeGenerator).
	virtual QVector<WavefrontGeneratorSetting> getSettingsDescriptors() const { return {}; }

	virtual QVector<WavefrontParameter> getParameterDescriptors() const = 0;
	virtual void setCoefficient(int id, double value) = 0;
	virtual double getCoefficient(int id) const = 0;
	virtual QVector<double> getAllCoefficients() const = 0;
	virtual void setAllCoefficients(const QVector<double>& coefficients) = 0;
	virtual void resetCoefficients() = 0;
	virtual af::array generateWavefront(int gridSize) = 0;
};

#endif // IWAVEFRONTGENERATOR_H
