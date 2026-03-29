#ifndef PSFCALCULATOR_H
#define PSFCALCULATOR_H

#include <QObject>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "ipsfpropagator.h"
#include "iwavefrontgenerator.h"

class PSFCalculator : public QObject, public IPSFPropagator
{
	Q_OBJECT
public:
	enum NormalizationMode { SumNormalization = 0, PeakNormalization = 1, NoNormalization = 2 };

	explicit PSFCalculator(double phaseScale = 1.0, double apertureRadius = 1.0, QObject* parent = nullptr);
	~PSFCalculator() override;

	af::array computePSF(const af::array& wavefront);

	// IPSFPropagator
	af::array computePSF(const af::array& wavefront, int gridSize) override;
	QVariantMap serializeSettings() const override;
	void deserializeSettings(const QVariantMap& settings) override;
	QVector<NumericSettingDescriptor> getSettingsDescriptors() const override;
	int getApertureGeometry() const override;
	double getApertureRadius() const override;
	void invalidateCache() override;

	void setPhaseScale(double phaseScale);
	double getPhaseScale() const;
	void setApertureRadius(double radius);
	void setNormalizationMode(NormalizationMode mode);
	NormalizationMode getNormalizationMode() const;
	void setPaddingFactor(int factor);
	int getPaddingFactor() const;
	void setApertureGeometry(int geometry);

private:
	double phaseScale;
	double apertureRadius;
	NormalizationMode normMode;
	int paddingFactor;
	int apertureGeometry;

	int cachedGridSize;
	af::array cachedApertureMask;

	void buildApertureCache(int gridSize);
	static af::array fftshift2D(const af::array& input);
};

#endif // PSFCALCULATOR_H
