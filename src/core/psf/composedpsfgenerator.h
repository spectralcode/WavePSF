#ifndef COMPOSEDPSFGENERATOR_H
#define COMPOSEDPSFGENERATOR_H

#include <QObject>
#include <arrayfire.h>
#include "ipsfgenerator.h"

class IWavefrontGenerator;
class IPSFPropagator;

class ComposedPSFGenerator : public QObject, public IPSFGenerator
{
	Q_OBJECT
public:
	// Takes ownership of both generator and propagator
	ComposedPSFGenerator(const QString& typeName,
						 IWavefrontGenerator* generator,
						 IPSFPropagator* propagator,
						 QObject* parent = nullptr);
	~ComposedPSFGenerator() override;

	// IPSFGenerator — identity
	QString typeName() const override;
	QVariantMap serializeSettings() const override;
	void deserializeSettings(const QVariantMap& settings) override;
	QVector<NumericSettingDescriptor> getSettingsDescriptors() const override;

	// Coefficients — delegate to IWavefrontGenerator
	QVector<WavefrontParameter> getParameterDescriptors() const override;
	void setCoefficient(int id, double value) override;
	double getCoefficient(int id) const override;
	QVector<double> getAllCoefficients() const override;
	void setAllCoefficients(const QVector<double>& coefficients) override;
	void resetCoefficients() override;

	// PSF generation — generator + propagator pipeline
	af::array generatePSF(int gridSize) override;
	bool hasWavefront() const override { return true; }
	af::array getLastWavefront() const override;

	// Aperture — delegate to propagator
	int getApertureGeometry() const override;
	double getApertureRadius() const override;

	// Capabilities — delegate to propagator
	bool is3D() const override;

	// Inline settings — delegate to propagator
	void applyInlineSettings(const QVariantMap& settings) override;
	void setNumOutputPlanes(int numPlanes) override;
	void invalidateCache() override;

	// Temporary migration accessors (remove after SettingsDialog refactoring)
	IWavefrontGenerator* wavefrontGenerator() const;
	IPSFPropagator* propagator() const;

private:
	QString name;
	IWavefrontGenerator* generator;
	IPSFPropagator* propagator_;
	af::array lastWavefront;
};

#endif // COMPOSEDPSFGENERATOR_H
