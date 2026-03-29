#ifndef IPSFGENERATOR_H
#define IPSFGENERATOR_H

#include <QString>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "wavefrontparameter.h"
#include "iwavefrontgenerator.h"

class IPSFGenerator
{
public:
	virtual ~IPSFGenerator() = default;

	// Identity and persistence
	virtual QString typeName() const = 0;
	virtual QVariantMap serializeSettings() const = 0;
	virtual void deserializeSettings(const QVariantMap& settings) = 0;

	// Settings descriptors for SettingsDialog auto-UI
	virtual QVector<NumericSettingDescriptor> getSettingsDescriptors() const { return {}; }

	// Wavefront parameters and coefficients
	virtual bool supportsCoefficients() const { return !getParameterDescriptors().isEmpty(); }
	virtual QVector<WavefrontParameter> getParameterDescriptors() const { return {}; }
	virtual void setCoefficient(int id, double value) { Q_UNUSED(id); Q_UNUSED(value); }
	virtual double getCoefficient(int id) const { Q_UNUSED(id); return 0.0; }
	virtual QVector<double> getAllCoefficients() const { return {}; }
	virtual void setAllCoefficients(const QVector<double>& coefficients) { Q_UNUSED(coefficients); }
	virtual void resetCoefficients() {}

	// Core: generate the PSF (2D or 3D)
	virtual af::array generatePSF(int gridSize) = 0;

	// Wavefront display (optional)
	virtual bool hasWavefront() const { return false; }
	virtual af::array getLastWavefront() const { return af::array(); }

	// Aperture display hints for wavefront plot
	virtual int getApertureGeometry() const { return 0; }
	virtual double getApertureRadius() const { return 1.0; }

	// Capabilities
	virtual bool is3D() const { return false; }

	// Inline settings updates (e.g., RW settings widget)
	virtual void applyInlineSettings(const QVariantMap& settings) { Q_UNUSED(settings); }

	// Z-plane count for 3D generators
	virtual void setNumOutputPlanes(int numPlanes) { Q_UNUSED(numPlanes); }

	// Cache management
	virtual void invalidateCache() {}
};

#endif // IPSFGENERATOR_H
