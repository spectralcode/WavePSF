#ifndef DECONVOLVER_H
#define DECONVOLVER_H

#include <QObject>
#include <QStringList>
#include <arrayfire.h>

class VolumetricDeconvolver;

class Deconvolver : public QObject
{
	Q_OBJECT
public:
	enum Algorithm {
		RICHARDSON_LUCY = 0,
		LANDWEBER,
		TIKHONOV,
		WIENER,
		CONVOLUTION,
		RICHARDSON_LUCY_3D
	};

	explicit Deconvolver(int iterations = 128, QObject* parent = nullptr);
	~Deconvolver() override;

	af::array deconvolve(const af::array& blurredInput, const af::array& psf);

	void setAlgorithm(Algorithm algo);
	void setIterations(int iterations);
	void setRelaxationFactor(float factor);
	void setRegularizationFactor(float factor);
	void setNoiseToSignalFactor(float factor);
	void setVolumePaddingMode(int mode);
	void setAccelerationMode(int mode);

	void requestDeconvolutionCancel();
	void resetDeconvolutionCancel();
	bool wasDeconvolutionCancelled() const;

	bool is3DAlgorithm() const;

	static QStringList getAlgorithmNames();

signals:
	void error(QString message);
	void iterationCompleted(int currentIteration, int totalIterations);

private:
	Algorithm algorithm;
	int iterations;
	float landweberRelaxationFactor;
	float tikhonovRegularizationFactor;
	float wienerNoiseToSignalFactor;

	VolumetricDeconvolver* volumetricDeconvolver;

	af::array wienerDeconvolution(const af::array& blurredInput, const af::array& psf, float nsr) const;
	void conserveTotalIntensity(const af::array& blurredInput, af::array& result);
};

#endif // DECONVOLVER_H
