#include "psfcalculator.h"
#include "apertureutils.h"


PSFCalculator::PSFCalculator(double lambda, double apertureRadius, QObject* parent)
	: QObject(parent)
	, lambda(lambda)
	, apertureRadius(apertureRadius)
	, normMode(SumNormalization)
	, paddingFactor(1)
	, apertureGeometry(0)
	, cachedGridSize(0)
{
}

PSFCalculator::~PSFCalculator()
{
}

af::array PSFCalculator::computePSF(const af::array& wavefront)
{
	int gridSize = wavefront.dims(0);

	if (gridSize != this->cachedGridSize) {
		this->buildApertureCache(gridSize);
	}

	// scaling factor converts from unit of wavefront to radians of phase, but is actually
	// not needed here since the Zernike coefficients (the thing the optimization algorithm optimizes) 
	// are dimensionless and can be interpreted as phase values directly.
	// todo: maybe remove scaling factor here (currently onle kept to match an earlier implementaion)
	float phaseScale = static_cast<float>(2.0 * af::Pi / this->lambda);
	af::array phase = phaseScale * wavefront;

	// Pupil function = complex(cos(phase), sin(phase)) * aperture mask
	af::array pupilReal = af::cos(phase) * this->cachedApertureMask;
	af::array pupilImag = af::sin(phase) * this->cachedApertureMask;
	af::array pupil = af::complex(pupilReal, pupilImag);

	// FFT (with optional zero-padding for smoother PSF)
	af::array ft;
	if (this->paddingFactor > 1) {
		int paddedSize = gridSize * this->paddingFactor;
		ft = af::fft2(pupil, paddedSize, paddedSize);
	} else {
		ft = af::fft2(pupil);
	}

	// PSF = |FFT|^2 (intensity)
	af::array psf = af::real(ft * af::conjg(ft));

	// Shift DC component to center
	psf = fftshift2D(psf);

	// Crop center gridSize×gridSize from padded result
	if (this->paddingFactor > 1) {
		int paddedSize = psf.dims(0);
		int offset = (paddedSize - gridSize) / 2;
		psf = psf(af::seq(offset, offset + gridSize - 1), af::seq(offset, offset + gridSize - 1));
	}

	// Normalize PSF
	if (this->normMode == SumNormalization) {
		double totalIntensity = af::sum<double>(psf);
		if (totalIntensity > 0.0) {
			psf = psf / static_cast<float>(totalIntensity);
		}
	} else if (this->normMode == PeakNormalization) {
		double peak = af::max<double>(psf);
		if (peak > 0.0) {
			psf = psf / static_cast<float>(peak);
		}
	}
	// NoNormalization: do nothing

	return psf;
}

void PSFCalculator::setLambda(double lambda)
{
	this->lambda = lambda;
}

double PSFCalculator::getLambda() const
{
	return this->lambda;
}

void PSFCalculator::setApertureRadius(double radius)
{
	this->apertureRadius = radius;
	this->cachedGridSize = 0; // Invalidate cache
}

double PSFCalculator::getApertureRadius() const
{
	return this->apertureRadius;
}

void PSFCalculator::setNormalizationMode(NormalizationMode mode)
{
	this->normMode = mode;
}

PSFCalculator::NormalizationMode PSFCalculator::getNormalizationMode() const
{
	return this->normMode;
}

void PSFCalculator::setPaddingFactor(int factor)
{
	this->paddingFactor = (factor > 0) ? factor : 1;
}

int PSFCalculator::getPaddingFactor() const
{
	return this->paddingFactor;
}

void PSFCalculator::setApertureGeometry(int geometry)
{
	this->apertureGeometry = geometry;
	this->cachedGridSize = 0; // Invalidate cache
}

int PSFCalculator::getApertureGeometry() const
{
	return this->apertureGeometry;
}

void PSFCalculator::buildApertureCache(int gridSize)
{
	this->cachedGridSize = gridSize;
	this->cachedApertureMask = ApertureUtils::buildMask(
		gridSize, static_cast<ApertureUtils::Geometry>(this->apertureGeometry),
		this->apertureRadius);
}

af::array PSFCalculator::fftshift2D(const af::array& input)
{
	int rows = input.dims(0);
	int cols = input.dims(1);
	int halfRows = rows / 2;
	int halfCols = cols / 2;

	// Swap quadrants: top-left <-> bottom-right, top-right <-> bottom-left
	af::array shifted = af::constant(0.0f, input.dims(), input.type());
	shifted(af::seq(halfRows, rows - 1), af::seq(halfCols, cols - 1)) = input(af::seq(0, halfRows - 1), af::seq(0, halfCols - 1));
	shifted(af::seq(0, halfRows - 1), af::seq(0, halfCols - 1)) = input(af::seq(halfRows, rows - 1), af::seq(halfCols, cols - 1));
	shifted(af::seq(0, halfRows - 1), af::seq(halfCols, cols - 1)) = input(af::seq(halfRows, rows - 1), af::seq(0, halfCols - 1));
	shifted(af::seq(halfRows, rows - 1), af::seq(0, halfCols - 1)) = input(af::seq(0, halfRows - 1), af::seq(halfCols, cols - 1));

	return shifted;
}
