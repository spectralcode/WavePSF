#ifndef PSFMODULE_H
#define PSFMODULE_H

#include <QObject>
#include <QVector>
#include <arrayfire.h>
#include "wavefrontparameter.h"
#include "deconvolutionsettings.h"
#include "psfsettings.h"

class IPSFGenerator;
class Deconvolver;
class AFDeviceManager;

class PSFModule : public QObject
{
	Q_OBJECT
public:
	explicit PSFModule(AFDeviceManager* afDeviceManager, QObject* parent = nullptr);
	~PSFModule() override;

	QVector<WavefrontParameter> getParameterDescriptors() const;
	QVector<double> getAllCoefficients() const;
	af::array getCurrentWavefront() const;
	af::array getCurrentPSF() const;
	PSFSettings getPSFSettings() const;
	DeconvolutionSettings getDeconvolutionSettings() const;
	QString getGeneratorTypeName() const;
	af::array computePSFFromCoefficients(const QVector<double>& coefficients);

	IPSFGenerator* getGenerator() const;

	// Returns mode names for the "PSF Generator" dropdown
	static QStringList availablePSFModes();

	// Extract frame-specific z-slice from a potentially 3D PSF
	static af::array extractFrame(const af::array& psf, int frame);

	int getCurrentFrame() const;

public slots:
	void setCoefficient(int id, double value);
	void setAllCoefficients(const QVector<double>& coefficients);
	void resetCoefficients();
	void setCurrentPatch(int frame, int patchIdx);
	void refreshPSF();
	void setGridSize(int size);
	af::array deconvolve(const af::array& input);
	af::array deconvolve(const af::array& input, const af::array& psf);
	bool is3DAlgorithm() const;

	// Single switching API
	void switchGenerator(const QString& typeName);

	// PSF settings
	void applyPSFSettings(const PSFSettings& settings);
	void applyInlineSettings(const QVariantMap& settings);
	void setNumOutputPlanes(int numPlanes);

	// Deconvolution settings forwarding
	void setDeconvolutionAlgorithm(int algorithm);
	void setDeconvolutionIterations(int iterations);
	void setDeconvolutionRelaxationFactor(float factor);
	void setDeconvolutionTikhonovRegularizationFactor(float factor);
	void setDeconvolutionWienerNoiseToSignalFactor(float factor);
	void setVolumePaddingMode(int mode);
	void setAccelerationMode(int mode);
	void setRegularizer3D(int mode);
	void setRegularizationWeight(float weight);
	void setDeconvolutionVoxelSize(float sizeY, float sizeX, float sizeZ);

	// Cooperative deconvolution cancellation
	void requestDeconvolutionCancel();
	void resetDeconvolutionCancel();
	bool wasDeconvolutionCancelled() const;

	void clearCachedArrays();

signals:
	void wavefrontUpdated(af::array wavefront);
	void psfUpdated(af::array psf);
	void parameterDescriptorsChanged(QVector<WavefrontParameter> descriptors);
	void generatorChanged(QString typeName);
	void nollIndicesChanged();
	void deconvolutionSettingsChanged();
	void deconvolutionIterationCompleted(int currentIteration, int totalIterations);
	void error(QString message);

private:
	void regeneratePipeline();

	IPSFGenerator* generator;
	Deconvolver* deconvolver;

	int gridSize;
	int currentFrame;
	int currentPatchIdx;
	QMap<QString, QVariantMap> allGeneratorSettings;
	QVector<WavefrontParameter> cachedDescriptors;
	af::array currentPSF;
};

#endif // PSFMODULE_H
