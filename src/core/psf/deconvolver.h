#ifndef DECONVOLVER_H
#define DECONVOLVER_H

#include <QObject>
#include <QStringList>
#include <arrayfire.h>

class Deconvolver : public QObject
{
	Q_OBJECT
public:
	enum Algorithm {
		RICHARDSON_LUCY = 0,
		LANDWEBER,
		TIKHONOV,
		WIENER,
		CONVOLUTION
	};

	explicit Deconvolver(int iterations = 128, QObject* parent = nullptr);
	~Deconvolver() override;

	af::array deconvolve(const af::array& blurredInput, const af::array& psf);

	void setAlgorithm(Algorithm algo);
	void setIterations(int iterations);
	void setRelaxationFactor(float factor);
	void setRegularizationFactor(float factor);
	void setNoiseToSignalFactor(float factor);

	Algorithm getAlgorithm() const;
	int getIterations() const;
	float getRelaxationFactor() const;
	float getRegularizationFactor() const;
	float getNoiseToSignalFactor() const;

	static QStringList getAlgorithmNames();

signals:
	void error(QString message);

private:
	Algorithm algorithm;
	int iterations;
	float landweberRelaxationFactor;
	float tikhonovRegularizationFactor;
	float wienerNoiseToSignalFactor;

	af::array wienerDeconvolution(const af::array& blurredInput, const af::array& psf, float nsr) const;
	void conserveTotalIntensity(const af::array& blurredInput, af::array& result);
};

#endif // DECONVOLVER_H
