#ifndef IWAVEFRONTGENERATOR_H
#define IWAVEFRONTGENERATOR_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "wavefrontparameter.h"

// Descriptor for a single configuration setting.
// Used by SettingsDialog to auto-build UI for any component
// (wavefront generators, PSF calculators, etc.).
struct NumericSettingDescriptor {
	QString key;          // serialization key (must match serializeSettings/deserializeSettings)
	QString name;         // UI label
	QString tooltip;      // UI help text
	double minValue;
	double maxValue;
	double step;
	double defaultValue;
	int decimals;         // 0 = QSpinBox (integer), >0 = QDoubleSpinBox with N decimal places
	QStringList options;  // if non-empty: render as QComboBox (index = stored integer value)
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
	// SettingsDialog auto-builds UI from these — no dialog changes needed for new generators.
	// Default: empty list (used by generators with fully custom UI, e.g. ZernikeGenerator).
	virtual QVector<NumericSettingDescriptor> getSettingsDescriptors() const { return {}; }

	virtual QVector<WavefrontParameter> getParameterDescriptors() const = 0;
	virtual void setCoefficient(int id, double value) = 0;
	virtual double getCoefficient(int id) const = 0;
	virtual QVector<double> getAllCoefficients() const = 0;
	virtual void setAllCoefficients(const QVector<double>& coefficients) = 0;
	virtual void resetCoefficients() = 0;
	virtual af::array generateWavefront(int gridSize) = 0;
	virtual void invalidateCache() {};
};

#endif // IWAVEFRONTGENERATOR_H
