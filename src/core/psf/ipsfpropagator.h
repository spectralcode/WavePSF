#ifndef IPSFPROPAGATOR_H
#define IPSFPROPAGATOR_H

#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "iwavefrontgenerator.h"

class IPSFPropagator
{
public:
	virtual ~IPSFPropagator() = default;

	// Core computation: wavefront → PSF
	virtual af::array computePSF(const af::array& wavefront, int gridSize) = 0;

	// Capabilities
	virtual bool is3D() const { return false; }

	// Settings persistence
	virtual QVariantMap serializeSettings() const = 0;
	virtual void deserializeSettings(const QVariantMap& settings) = 0;
	virtual QVector<NumericSettingDescriptor> getSettingsDescriptors() const { return {}; }

	// Aperture display hints for wavefront plot
	virtual int getApertureGeometry() const { return 0; }
	virtual double getApertureRadius() const { return 1.0; }

	// Inline settings updates (e.g., RW settings widget changes)
	virtual void applyInlineSettings(const QVariantMap& settings) { Q_UNUSED(settings); }

	// Z-plane count for 3D propagators
	virtual void setNumOutputPlanes(int numPlanes) { Q_UNUSED(numPlanes); }

	// Cache management
	virtual void invalidateCache() {}
};

#endif // IPSFPROPAGATOR_H
