#include "deconvolver.h"
#include "core/processing/volumetricdeconvolver.h"
#include "utils/logging.h"
#include <algorithm>


Deconvolver::Deconvolver(int iterations, QObject* parent)
	: QObject(parent)
	, volumetricDeconvolver(new VolumetricDeconvolver(this))
{
	connect(this->volumetricDeconvolver, &VolumetricDeconvolver::iterationCompleted,
			this, &Deconvolver::iterationCompleted);
	connect(this->volumetricDeconvolver, &VolumetricDeconvolver::error,
			this, &Deconvolver::error);

	this->settings.iterations = iterations;
	this->applySettings(this->settings);
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
		switch (static_cast<Algorithm>(this->settings.algorithm)) {
			case RICHARDSON_LUCY:
				result = af::iterativeDeconv(input, kernel, this->settings.iterations, 1.0f, AF_ITERATIVE_DECONV_RICHARDSONLUCY);
				break;

			case LANDWEBER:
				result = af::iterativeDeconv(input, kernel, this->settings.iterations, this->settings.relaxationFactor, AF_ITERATIVE_DECONV_LANDWEBER);
				this->conserveTotalIntensity(input, result);
				break;

			case TIKHONOV:
				result = af::inverseDeconv(input, kernel, this->settings.tikhonovRegularizationFactor, AF_INVERSE_DECONV_TIKHONOV);
				this->conserveTotalIntensity(input, result);
				break;

			case WIENER:
				result = this->wienerDeconvolution(input, kernel, this->settings.wienerNoiseToSignalFactor);
				this->conserveTotalIntensity(input, result);
				break;

			case CONVOLUTION:
				result = af::convolve2(input, kernel);
				break;

			case RICHARDSON_LUCY_3D:
				result = this->volumetricDeconvolver->deconvolve(input, kernel, this->settings.iterations);
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
	this->settings.algorithm = static_cast<int>(algo);
}

void Deconvolver::setIterations(int iterations)
{
	if (iterations < 1) {
		LOG_WARNING() << "Deconvolution iterations must be >= 1, clamping to 1";
		this->settings.iterations = 1;
	} else {
		this->settings.iterations = iterations;
	}
}

void Deconvolver::setRelaxationFactor(float factor)
{
	if (factor <= 0.0f) {
		LOG_WARNING() << "Relaxation factor must be > 0, clamping to 0.001";
		this->settings.relaxationFactor = 0.001f;
	} else {
		this->settings.relaxationFactor = factor;
	}
}

void Deconvolver::setTikhonovRegularizationFactor(float factor)
{
	if (factor <= 0.0f) {
		LOG_WARNING() << "Regularization factor must be > 0, clamping to 0.0001";
		this->settings.tikhonovRegularizationFactor = 0.0001f;
	} else {
		this->settings.tikhonovRegularizationFactor = factor;
	}
}

void Deconvolver::setWienerNoiseToSignalFactor(float factor)
{
	if (factor <= 0.0f) {
		LOG_WARNING() << "NSR factor must be > 0, clamping to 0.001";
		this->settings.wienerNoiseToSignalFactor = 0.001f;
	} else {
		this->settings.wienerNoiseToSignalFactor = factor;
	}
}

void Deconvolver::setVolumePaddingMode(int mode)
{
	this->settings.volumePaddingMode = mode;
	this->volumetricDeconvolver->setPaddingMode(
		static_cast<VolumetricDeconvolver::PaddingMode>(mode));
}

void Deconvolver::setAccelerationMode(int mode)
{
	this->settings.accelerationMode = mode;
	this->volumetricDeconvolver->setAccelerationMode(
		static_cast<VolumetricDeconvolver::AccelerationMode>(mode));
}

void Deconvolver::setRegularizer3D(int mode)
{
	this->settings.regularizer3D = mode;
	this->volumetricDeconvolver->setRegularizer(
		static_cast<VolumetricDeconvolver::RegularizerMode>(mode));
}

void Deconvolver::setRegularizationWeight(float weight)
{
	this->settings.regularizationWeight = weight;
	this->volumetricDeconvolver->setRegularizationWeight(weight);
}

void Deconvolver::setVoxelSize(float sizeY, float sizeX, float sizeZ)
{
	this->settings.voxelSizeY = sizeY;
	this->settings.voxelSizeX = sizeX;
	this->settings.voxelSizeZ = sizeZ;
	this->volumetricDeconvolver->setVoxelSize(sizeY, sizeX, sizeZ);
}

DeconvolutionSettings Deconvolver::getSettings() const
{
	return this->settings;
}

void Deconvolver::applySettings(const DeconvolutionSettings& settings)
{
	this->setAlgorithm(static_cast<Algorithm>(settings.algorithm));
	this->setIterations(settings.iterations);
	this->setRelaxationFactor(settings.relaxationFactor);
	this->setTikhonovRegularizationFactor(settings.tikhonovRegularizationFactor);
	this->setWienerNoiseToSignalFactor(settings.wienerNoiseToSignalFactor);
	this->setVolumePaddingMode(settings.volumePaddingMode);
	this->setAccelerationMode(settings.accelerationMode);
	this->setRegularizer3D(settings.regularizer3D);
	this->setRegularizationWeight(settings.regularizationWeight);
	this->setVoxelSize(settings.voxelSizeY, settings.voxelSizeX, settings.voxelSizeZ);
}

void Deconvolver::requestDeconvolutionCancel()
{
	this->volumetricDeconvolver->requestCancel();
}

void Deconvolver::resetDeconvolutionCancel()
{
	this->volumetricDeconvolver->resetCancel();
}

bool Deconvolver::wasDeconvolutionCancelled() const
{
	return this->volumetricDeconvolver->wasCancelled();
}

bool Deconvolver::is3DAlgorithm() const
{
	return this->settings.algorithm == RICHARDSON_LUCY_3D;
}

QStringList Deconvolver::getAlgorithmNames()
{
	return QStringList {
		"Richardson-Lucy",
		"Landweber",
		"Tikhonov",
		"Wiener",
		"Convolution",
		"Richardson-Lucy 3D"
	};
}

af::array Deconvolver::wienerDeconvolution(const af::array& blurredInput, const af::array& psf, float nsr) const
{
	// Standard ifftshift: move PSF center to origin so the linear phase term
	// from tip/tilt correctly shifts the deconvolved output.
	int psfRows = psf.dims(0);
	int psfCols = psf.dims(1);
	af::array psfCorner = af::shift(psf, -(psfRows / 2), -(psfCols / 2));

	// Compute at the larger of image and PSF size to avoid truncating the PSF.
	// When PSF > image: zero-pad image to PSF size (no PSF truncation).
	// When PSF <= image: zero-pad PSF to image size (standard behavior).
	int inRows = blurredInput.dims(0);
	int inCols = blurredInput.dims(1);
	int compRows = (std::max)(inRows, psfRows);
	int compCols = (std::max)(inCols, psfCols);

	af::array G = af::fft2(blurredInput, compRows, compCols);
	af::array H = af::fft2(psfCorner, compRows, compCols);

	// Conjugate of the PSF spectrum
	af::array HConj = af::conjg(H);

	// Wiener filter: H* / (|H|^2 + NSR)
	af::array WienerFilter = HConj / (af::abs(H) * af::abs(H) + nsr);

	// Apply filter in frequency domain and inverse FFT
	af::array F = WienerFilter * G;
	af::array result = af::real(af::ifft2(F));

	// Crop back to original image size (only differs when PSF was larger than image)
	return result(af::seq(inRows), af::seq(inCols));
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
