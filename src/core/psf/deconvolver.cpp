#include "deconvolver.h"
#include "utils/logging.h"


Deconvolver::Deconvolver(int iterations, QObject* parent)
	: QObject(parent)
	, algorithm(RICHARDSON_LUCY)
	, iterations(iterations)
	, landweberRelaxationFactor(0.65f)
	, tikhonovRegularizationFactor(0.005f)
	, wienerNoiseToSignalFactor(0.01f)
{
}

Deconvolver::~Deconvolver()
{
}

af::array Deconvolver::deconvolve(const af::array& blurredInput, const af::array& psf)
{
	if (blurredInput.isempty()) {
		emit error(tr("Blurred input image is empty."));
		return af::array();
	}
	if (psf.isempty()) {
		emit error(tr("PSF kernel is empty."));
		return af::array();
	}

	// Ensure input and PSF are float
	af::array input = blurredInput.as(f32);
	af::array kernel = psf.as(f32);

	// ArrayFire's deconvolution functions handle PSF/input size mismatches internally.
	// No manual zero-padding needed.
	af::array result;
	try {
		switch (this->algorithm) {
			case RICHARDSON_LUCY:
				result = af::iterativeDeconv(input, kernel, this->iterations, 1.0f, AF_ITERATIVE_DECONV_RICHARDSONLUCY);
				break;

			case LANDWEBER:
				result = af::iterativeDeconv(input, kernel, this->iterations, this->landweberRelaxationFactor, AF_ITERATIVE_DECONV_LANDWEBER);
				this->conserveTotalIntensity(input, result);
				break;

			case TIKHONOV:
				result = af::inverseDeconv(input, kernel, this->tikhonovRegularizationFactor, AF_INVERSE_DECONV_TIKHONOV);
				this->conserveTotalIntensity(input, result);
				break;

			case WIENER:
				result = this->wienerDeconvolution(input, kernel, this->wienerNoiseToSignalFactor);
				this->conserveTotalIntensity(input, result);
				break;

			case CONVOLUTION:
				result = af::convolve2(input, kernel);
				break;

			default:
				emit error(tr("Invalid deconvolution algorithm."));
				return af::array();
		}
	} catch (af::exception& e) {
		emit error(tr("ArrayFire deconvolution error: ") + QString(e.what()));
		return af::array();
	}

	return result;
}

void Deconvolver::setAlgorithm(Algorithm algo)
{
	this->algorithm = algo;
}

void Deconvolver::setIterations(int iterations)
{
	if (iterations < 1) {
		LOG_WARNING() << "Deconvolution iterations must be >= 1, clamping to 1";
		this->iterations = 1;
	} else {
		this->iterations = iterations;
	}
}

void Deconvolver::setRelaxationFactor(float factor)
{
	if (factor <= 0.0f) {
		LOG_WARNING() << "Relaxation factor must be > 0, clamping to 0.001";
		this->landweberRelaxationFactor = 0.001f;
	} else {
		this->landweberRelaxationFactor = factor;
	}
}

void Deconvolver::setRegularizationFactor(float factor)
{
	if (factor <= 0.0f) {
		LOG_WARNING() << "Regularization factor must be > 0, clamping to 0.0001";
		this->tikhonovRegularizationFactor = 0.0001f;
	} else {
		this->tikhonovRegularizationFactor = factor;
	}
}

void Deconvolver::setNoiseToSignalFactor(float factor)
{
	if (factor <= 0.0f) {
		LOG_WARNING() << "NSR factor must be > 0, clamping to 0.001";
		this->wienerNoiseToSignalFactor = 0.001f;
	} else {
		this->wienerNoiseToSignalFactor = factor;
	}
}

Deconvolver::Algorithm Deconvolver::getAlgorithm() const
{
	return this->algorithm;
}

int Deconvolver::getIterations() const
{
	return this->iterations;
}

float Deconvolver::getRelaxationFactor() const
{
	return this->landweberRelaxationFactor;
}

float Deconvolver::getRegularizationFactor() const
{
	return this->tikhonovRegularizationFactor;
}

float Deconvolver::getNoiseToSignalFactor() const
{
	return this->wienerNoiseToSignalFactor;
}

QStringList Deconvolver::getAlgorithmNames()
{
	return QStringList {
		"Richardson-Lucy",
		"Landweber",
		"Tikhonov",
		"Wiener",
		"Convolution"
	};
}

af::array Deconvolver::wienerDeconvolution(const af::array& blurredInput, const af::array& psf, float nsr) const
{
	// Fourier Transform of the blurred image
	af::array G = af::fft2(blurredInput);

	// Fourier Transform of the PSF, zero-padded to input dimensions by af::fft2
	af::array H = af::fft2(psf, blurredInput.dims(0), blurredInput.dims(1));

	// Conjugate of the PSF spectrum
	af::array HConj = af::conjg(H);

	// Wiener filter: H* / (|H|^2 + NSR)
	af::array WienerFilter = HConj / (af::abs(H) * af::abs(H) + nsr);

	// Apply filter in frequency domain
	af::array F = WienerFilter * G;

	// Inverse FFT and shift to compensate for center-placed PSF
	int xShift = blurredInput.dims(0) / 2;
	int yShift = blurredInput.dims(1) / 2;
	af::array result = af::shift(af::ifft2(F), xShift, yShift);

	return af::real(result);
}

void Deconvolver::conserveTotalIntensity(const af::array& blurredInput, af::array& result)
{
	try {
		float totalIntensityBlurred, totalIntensityResult;
		af::sum(af::sum(blurredInput, 0), 1).host(&totalIntensityBlurred);
		af::sum(af::sum(result, 0), 1).host(&totalIntensityResult);

		if (totalIntensityResult > 0) {
			result *= (totalIntensityBlurred / totalIntensityResult);
		} else {
			emit error(tr("Total intensity of result is zero, skipping intensity conservation."));
		}
	} catch (af::exception& e) {
		emit error(tr("Intensity conservation error: ") + QString(e.what()));
	}
}
