#ifndef PSFMODULE_H
#define PSFMODULE_H

#include <QObject>
#include <QVector>
#include <arrayfire.h>
#include "wavefrontparameter.h"
#include "psfsettings.h"

class IWavefrontGenerator;
class PSFCalculator;
class Deconvolver;

class PSFModule : public QObject
{
	Q_OBJECT
public:
	explicit PSFModule(QObject* parent = nullptr);
	~PSFModule() override;

	QVector<WavefrontParameter> getParameterDescriptors() const;
	QVector<double> getAllCoefficients() const;
	af::array getCurrentWavefront() const;
	af::array getCurrentPSF() const;
	PSFSettings getPSFSettings() const;
	bool isUsingExternalPSF() const;

public slots:
	void setCoefficient(int id, double value);
	void setAllCoefficients(const QVector<double>& coefficients);
	void resetCoefficients();
	void setExternalPSF(const af::array& psf);
	void setGridSize(int size);
	af::array deconvolve(const af::array& input);

	// PSF settings
	void applyPSFSettings(const PSFSettings& settings);

	// Deconvolution settings forwarding
	void setDeconvolutionAlgorithm(int algorithm);
	void setDeconvolutionIterations(int iterations);
	void setDeconvolutionRelaxationFactor(float factor);
	void setDeconvolutionRegularizationFactor(float factor);
	void setDeconvolutionNoiseToSignalFactor(float factor);

signals:
	void wavefrontUpdated(af::array wavefront);
	void psfUpdated(af::array psf);
	void parameterDescriptorsChanged(QVector<WavefrontParameter> descriptors);
	void nollIndicesChanged();
	void deconvolutionSettingsChanged();
	void error(QString message);

private:
	void regeneratePipeline();

	IWavefrontGenerator* generator;
	PSFCalculator* calculator;
	Deconvolver* deconvolver;

	int gridSize;
	af::array currentWavefront;
	af::array currentPSF;
	af::array externalPSF;
	bool usingExternalPSF;
};

#endif // PSFMODULE_H
