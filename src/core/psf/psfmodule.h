#ifndef PSFMODULE_H
#define PSFMODULE_H

#include <QObject>
#include <QVector>
#include <arrayfire.h>
#include "wavefrontparameter.h"
#include "psfsettings.h"

class IWavefrontGenerator;
class PSFCalculator;
class RichardsWolfCalculator;
class Deconvolver;
class AFDeviceManager;

class PSFModule : public QObject
{
	Q_OBJECT
public:
	// PSF computation model — the "PSF Generator" dropdown is a mode selector
	// that maps to a wavefront generator type + PSF model:
	//   Zernike            → ZernikeGenerator + SCALAR_FOURIER
	//   Deformable Mirror  → DeformableMirrorGenerator + SCALAR_FOURIER
	//   3D PSF Microscopy  → ZernikeGenerator + MICROSCOPY_3D
	enum PSFModel { SCALAR_FOURIER = 0, MICROSCOPY_3D = 1 };

	explicit PSFModule(AFDeviceManager* afDeviceManager, QObject* parent = nullptr);
	~PSFModule() override;

	QVector<WavefrontParameter> getParameterDescriptors() const;
	QVector<double> getAllCoefficients() const;
	af::array getCurrentWavefront() const;
	af::array getCurrentPSF() const;
	PSFSettings getPSFSettings() const;
	QString getGeneratorTypeName() const;
	bool isUsingExternalPSF() const;
	af::array computePSFFromCoefficients(const QVector<double>& coefficients);

	PSFModel getPSFModel() const;
	RichardsWolfCalculator* getRWCalculator() const;

	// Returns mode names for the "PSF Generator" dropdown
	static QStringList availablePSFModes();

	// Extract focal plane (center z-slice) from a potentially 3D PSF for 2D display/export
	static af::array focalSlice(const af::array& psf);

public slots:
	void setCoefficient(int id, double value);
	void setAllCoefficients(const QVector<double>& coefficients);
	void resetCoefficients();
	void setExternalPSF(const af::array& psf);
	void clearExternalPSF();
	void setGridSize(int size);
	af::array deconvolve(const af::array& input);
	af::array deconvolve(const af::array& input, const af::array& psf);
	bool is3DAlgorithm() const;

	// Generator/mode switching
	void setGeneratorType(const QString& typeName);
	void setPSFMode(const QString& modeName);

	// PSF settings
	void applyPSFSettings(const PSFSettings& settings);
	void applyRWSettings(const QVariantMap& rwSettings);

	// Deconvolution settings forwarding
	void setDeconvolutionAlgorithm(int algorithm);
	void setDeconvolutionIterations(int iterations);
	void setDeconvolutionRelaxationFactor(float factor);
	void setDeconvolutionRegularizationFactor(float factor);
	void setDeconvolutionNoiseToSignalFactor(float factor);
	void setVolumePaddingMode(int mode);
	void setAccelerationMode(int mode);

	void clearCachedArrays();

signals:
	void wavefrontUpdated(af::array wavefront);
	void psfUpdated(af::array psf);
	void parameterDescriptorsChanged(QVector<WavefrontParameter> descriptors);
	void generatorTypeChanged(QString typeName);
	void psfModelChanged(int model);
	void psfModeChanged(QString modeName);
	void nollIndicesChanged();
	void deconvolutionSettingsChanged();
	void deconvolutionIterationCompleted(int currentIteration, int totalIterations);
	void error(QString message);

private:
	void regeneratePipeline();

	IWavefrontGenerator* generator;
	PSFCalculator* calculator;
	RichardsWolfCalculator* rwCalculator;
	Deconvolver* deconvolver;

	PSFModel psfModel;
	int gridSize;
	QMap<QString, QVariantMap> allGeneratorSettings;
	QVector<WavefrontParameter> cachedDescriptors;
	af::array currentWavefront;
	af::array currentPSF;
	af::array externalPSF;
	bool usingExternalPSF;
};

#endif // PSFMODULE_H
