#ifndef RICHARDSWOLFCALCULATOR_H
#define RICHARDSWOLFCALCULATOR_H

#include <QObject>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "ipsfpropagator.h"
#include "iwavefrontgenerator.h"

// Vectorial Cartesian Richards-Wolf PSF calculator for high-NA microscopy.
// Supports unpolarized (fluorescence) and linearly polarized illumination.
// Takes a wavefront phase map (from any IWavefrontGenerator) and produces
// a 3D PSF by computing E-field components via 2D FFT per z-plane.
class RichardsWolfCalculator : public QObject, public IPSFPropagator
{
	Q_OBJECT
public:
	enum NormalizationMode { SumNormalization = 0, PeakNormalization = 1, NoNormalization = 2 };
	enum PolarizationMode { Unpolarized = 0, Linear = 1 };
	enum ScalingMode { FastScaling = 0, ExactScaling = 1 };

	explicit RichardsWolfCalculator(QObject* parent = nullptr);

	// IPSFPropagator
	af::array computePSF(const af::array& wavefront, int gridSize) override;
	bool is3D() const override { return true; }
	QVariantMap serializeSettings() const override;
	void deserializeSettings(const QVariantMap& settings) override;
	QVector<NumericSettingDescriptor> getSettingsDescriptors() const override;
	void applyInlineSettings(const QVariantMap& settings) override;
	void setNumOutputPlanes(int numPlanes) override;
	void invalidateCache() override;

	// Phase scaling (same convention as PSFCalculator — converts generator output to radians)
	void setPhaseScale(double value);
	double getPhaseScale() const;

	// Physical parameters
	void setWavelengthNm(double value);
	double getWavelengthNm() const;
	void setNumericalAperture(double value);
	double getNumericalAperture() const;
	void setImmersionIndex(double value);
	double getImmersionIndex() const;
	void setZStepNm(double value);
	double getZStepNm() const;
	void setNumZPlanes(int value);
	int getNumZPlanes() const;
	void setApodization(bool enabled);
	bool getApodization() const;
	void setNormalizationMode(NormalizationMode mode);
	NormalizationMode getNormalizationMode() const;
	void setPolarizationMode(PolarizationMode mode);
	PolarizationMode getPolarizationMode() const;
	void setPolarizationAngle(double degrees);
	double getPolarizationAngle() const;
	void setXyStepNm(double value);
	double getXyStepNm() const;
	void setScalingMode(ScalingMode mode);
	ScalingMode getScalingMode() const;

private:
	void buildCoordinateCache(int gridSize);

	// Vectorial pupil computation (general Cartesian formulation)
	void buildVectorialPupil(
		const af::array& complexPupil, float e0x, float e0y,
		af::array& pupilX, af::array& pupilY, af::array& pupilZ) const;

	// Settings
	double phaseScale;
	double wavelengthNm;
	double na;
	double nImmersion;
	double zStepNm;
	int numZPlanes;
	bool apodizationEnabled;
	NormalizationMode normMode;
	PolarizationMode polMode;
	double polAngle;
	double xyStepNm;
	ScalingMode scalingMode;
	bool confocalEnabled;

	// Cached coordinate grids (rebuilt when gridSize changes)
	int cachedGridSize;
	af::array cosTheta, sinTheta;
	af::array cosPhi, sinPhi;
	af::array sin2Phi, cos2Phi;
	af::array apertureMask;
	af::array szCorrection;
	af::array apodizationFactor;
};

#endif // RICHARDSWOLFCALCULATOR_H
