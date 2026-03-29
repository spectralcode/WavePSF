#include "psfcalculator.h"
#include "apertureutils.h"


PSFCalculator::PSFCalculator(double phaseScale, double apertureRadius, QObject* parent)
	: QObject(parent)
	, phaseScale(phaseScale)
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

	// scaling factor (= 2pi/lambda) converts from unit of wavefront to radians of phase, but is actually
	// not needed here since the Zernike coefficients (the thing the optimization algorithm optimizes) 
	// are dimensionless and can be interpreted as phase values directly.
	// I keep this for now as it allows me to recreate some results that were 
	//generated with a different code, where the phase scale value 114.240 was hardcoded
	float phaseScale = static_cast<float>(this->phaseScale);
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

void PSFCalculator::setPhaseScale(double phaseScale)
{
	this->phaseScale = phaseScale;
}

double PSFCalculator::getPhaseScale() const
{
	return this->phaseScale;
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

af::array PSFCalculator::computePSF(const af::array& wavefront, int gridSize)
{
	Q_UNUSED(gridSize);
	return this->computePSF(wavefront);
}

namespace {
	const QString KEY_PHASE_SCALE        = QStringLiteral("phase_scale");
	const QString KEY_APERTURE_RADIUS    = QStringLiteral("aperture_radius");
	const QString KEY_NORMALIZATION_MODE = QStringLiteral("normalization_mode");
	const QString KEY_PADDING_FACTOR     = QStringLiteral("padding_factor");
	const QString KEY_APERTURE_GEOMETRY  = QStringLiteral("aperture_geometry");
}

QVariantMap PSFCalculator::serializeSettings() const
{
	QVariantMap map;
	map[KEY_PHASE_SCALE]        = this->phaseScale;
	map[KEY_APERTURE_RADIUS]    = this->apertureRadius;
	map[KEY_NORMALIZATION_MODE] = static_cast<int>(this->normMode);
	map[KEY_PADDING_FACTOR]     = this->paddingFactor;
	map[KEY_APERTURE_GEOMETRY]  = this->apertureGeometry;
	return map;
}

void PSFCalculator::deserializeSettings(const QVariantMap& settings)
{
	if (settings.contains(KEY_PHASE_SCALE))
		this->setPhaseScale(settings.value(KEY_PHASE_SCALE).toDouble());
	if (settings.contains(KEY_APERTURE_RADIUS))
		this->setApertureRadius(settings.value(KEY_APERTURE_RADIUS).toDouble());
	if (settings.contains(KEY_NORMALIZATION_MODE))
		this->setNormalizationMode(static_cast<NormalizationMode>(settings.value(KEY_NORMALIZATION_MODE).toInt()));
	if (settings.contains(KEY_PADDING_FACTOR))
		this->setPaddingFactor(settings.value(KEY_PADDING_FACTOR).toInt());
	if (settings.contains(KEY_APERTURE_GEOMETRY))
		this->setApertureGeometry(settings.value(KEY_APERTURE_GEOMETRY).toInt());
}

QVector<NumericSettingDescriptor> PSFCalculator::getSettingsDescriptors() const
{
	return {
		{KEY_PHASE_SCALE,        tr("Phase Scale"),        tr("Phase scale in rad per wavefront unit.\n"
		                                                      "The wavefront is multiplied by this value as first step of PSF calculation.\n"
		                                                      "Default value: 1.0."),
		 0.001, 10000.0, 1.0,  this->phaseScale, 3, {}},
		{KEY_APERTURE_RADIUS,    tr("Aperture Radius"),    tr("Normalized pupil aperture radius [0, 1].\nDefines the active area of the wavefront."),
		 0.01,  1.0,     0.01, this->apertureRadius, 3, {}},
		{KEY_PADDING_FACTOR,     tr("Padding Factor"),     tr("Zero-padding factor for the FFT.\n"
		                                                      "Higher values produce smoother PSFs by oversampling\n"
		                                                      "the diffraction pattern before cropping back to grid size.\n"
		                                                      "Increases computation time."),
		 1,     8,       1,    static_cast<double>(this->paddingFactor), 0, {}},
		{KEY_NORMALIZATION_MODE, tr("Normalization"),       tr("How the PSF intensity values are normalized."),
		 0,     2,       1,    static_cast<double>(this->normMode), 0, {tr("Sum"), tr("Peak"), tr("None")}},
		{KEY_APERTURE_GEOMETRY,  tr("Aperture Geometry"),   tr("Shape of the pupil aperture mask."),
		 0,     2,       1,    static_cast<double>(this->apertureGeometry), 0, {tr("Circle"), tr("Rectangle"), tr("Triangle")}}
	};
}

void PSFCalculator::invalidateCache()
{
	this->cachedGridSize = 0;
	this->cachedApertureMask = af::array();
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
