#define NOMINMAX
#define _USE_MATH_DEFINES
#include "richardswolfcalculator.h"
#include <cmath>
#include <algorithm>

namespace {

const int    MIN_GRID_FOR_FAST_HALVING = 256;
const int    MAX_FFT_SIZE              = 2048;
const double RESAMPLE_TOLERANCE_NM     = 0.01;
const float  COS_THETA_CLAMP_MIN      = 0.001f;
const float  SIN_THETA_SQ_CLAMP_MAX   = 0.999f;

const QString KEY_PHASE_SCALE     = QStringLiteral("phase_scale");
const QString KEY_WAVELENGTH_NM   = QStringLiteral("wavelength_nm");
const QString KEY_NA              = QStringLiteral("numerical_aperture");
const QString KEY_N_IMMERSION     = QStringLiteral("immersion_index");
const QString KEY_Z_STEP_NM       = QStringLiteral("z_step_nm");
const QString KEY_NUM_Z_PLANES    = QStringLiteral("num_z_planes");
const QString KEY_APODIZATION     = QStringLiteral("apodization");
const QString KEY_NORMALIZATION   = QStringLiteral("normalization_mode");
const QString KEY_POLARIZATION   = QStringLiteral("polarization_mode");
const QString KEY_POL_ANGLE      = QStringLiteral("polarization_angle");
const QString KEY_XY_STEP_NM     = QStringLiteral("xy_step_nm");
const QString KEY_SCALING_MODE   = QStringLiteral("scaling_mode");

const double DEF_PHASE_SCALE     = 1.0;
const double DEF_WAVELENGTH_NM   = 560.0;
const double DEF_NA              = 1.4;
const double DEF_N_IMMERSION     = 1.518;
const double DEF_Z_STEP_NM       = 160.0;
const int    DEF_NUM_Z_PLANES    = 128;
const bool   DEF_APODIZATION     = true;
const int    DEF_NORMALIZATION    = 0;
const int    DEF_POLARIZATION    = 0;  // Unpolarized
const double DEF_POL_ANGLE       = 0.0;
const double DEF_XY_STEP_NM      = 64.0;
const int    DEF_SCALING_MODE    = 0;  // FastScaling
const QString KEY_CONFOCAL       = QStringLiteral("confocal_enabled");
const bool   DEF_CONFOCAL        = false;
}


RichardsWolfCalculator::RichardsWolfCalculator(QObject* parent)
	: QObject(parent)
	, phaseScale(DEF_PHASE_SCALE)
	, wavelengthNm(DEF_WAVELENGTH_NM)
	, na(DEF_NA)
	, nImmersion(DEF_N_IMMERSION)
	, zStepNm(DEF_Z_STEP_NM)
	, numZPlanes(DEF_NUM_Z_PLANES)
	, apodizationEnabled(DEF_APODIZATION)
	, normMode(SumNormalization)
	, polMode(Unpolarized)
	, polAngle(DEF_POL_ANGLE)
	, xyStepNm(DEF_XY_STEP_NM)
	, scalingMode(FastScaling)
	, confocalEnabled(DEF_CONFOCAL)
	, cachedGridSize(0)
{
}

af::array RichardsWolfCalculator::computePSF(const af::array& wavefront, int gridSize)
{
	// Fast mode: compute on half-resolution XY grid, center-pad at the end.
	int computeSize = gridSize;
	af::array wf = wavefront;
	if (this->scalingMode == FastScaling && gridSize >= MIN_GRID_FOR_FAST_HALVING) {
		computeSize = gridSize / 2;
		float s = static_cast<float>(gridSize - 1) / (computeSize - 1);
		af::array di = af::range(af::dim4(computeSize, computeSize), 0).as(f32) * s;
		af::array dj = af::range(af::dim4(computeSize, computeSize), 1).as(f32) * s;
		wf = af::approx2(wavefront, di, dj, AF_INTERP_BILINEAR, 0.0f);
	}

	if (computeSize != this->cachedGridSize) {
		this->buildCoordinateCache(computeSize);
	}

	// 1. Determine FFT size from requested pixel spacing.
	//    Native pixel spacing: dx_native = lambda * (N-1) / (2 * N * NA)
	//    (immersion index cancels out in the Fourier relationship)
	double nativeDx = this->wavelengthNm * (computeSize - 1) / (2.0 * computeSize * this->na);
	int fftSize = computeSize;
	if (this->xyStepNm > 0.0 && this->scalingMode == ExactScaling
	    && this->xyStepNm < nativeDx) {
		// Exact: zero-pad FFT for finer spacing (capped to MAX_FFT_SIZE)
		int padding = static_cast<int>(std::ceil(nativeDx / this->xyStepNm));
		padding = std::min(padding, std::max(1, MAX_FFT_SIZE / computeSize));
		fftSize = computeSize * padding;
	}

	// Actual FFT output spacing (may differ from requested due to cap or Fast mode)
	double actualXyStepNm = nativeDx;
	if (fftSize > computeSize) {
		actualXyStepNm = nativeDx / static_cast<double>(fftSize / computeSize);
	}

	// Resample via interpolation whenever requested spacing differs from actual
	bool needsResample = false;
	if (this->xyStepNm > 0.0) {
		needsResample = (std::abs(this->xyStepNm - actualXyStepNm) > RESAMPLE_TOLERANCE_NM);
	}

	// 2. Build complex pupil from wavefront phase.
	float ps = static_cast<float>(this->phaseScale);
	af::array phase = ps * wf;
	af::array pupilPhase = af::complex(af::cos(phase), af::sin(phase));
	af::array complexPupil = pupilPhase * this->apertureMask;

	// Apply 1/sz Jacobian correction (Cartesian formulation)
	complexPupil = complexPupil * this->szCorrection;

	// Apply apodization (energy conservation): sqrt(cos theta)
	if (this->apodizationEnabled) {
		complexPupil = complexPupil * this->apodizationFactor;
	}

	// 3. Polarization setup
	float e0x = 1.0f, e0y = 0.0f;
	if (this->polMode == Linear) {
		float rad = static_cast<float>(this->polAngle * M_PI / 180.0);
		e0x = std::cos(rad);
		e0y = std::sin(rad);
	}

	bool dualPol = (this->polMode == Unpolarized);

	// 4. Precompute vectorial pupils once (z-independent)
	af::array pupilX1, pupilY1, pupilZ1;
	af::array pupilX2, pupilY2, pupilZ2;
	if (dualPol) {
		this->buildVectorialPupil(complexPupil, 1.0f, 0.0f, pupilX1, pupilY1, pupilZ1);
		this->buildVectorialPupil(complexPupil, 0.0f, 1.0f, pupilX2, pupilY2, pupilZ2);
	} else {
		this->buildVectorialPupil(complexPupil, e0x, e0y, pupilX1, pupilY1, pupilZ1);
	}

	// 5. Per-plane intensity: FFT each component, accumulate |E|²
	auto computeIntensity = [&](const af::array& pX, const af::array& pY,
	                            const af::array& pZ, const af::array& defocusPhase) {
		af::array E = af::fft2(pX * defocusPhase, fftSize, fftSize);
		af::array intensity = af::real(E * af::conjg(E));
		E = af::fft2(pY * defocusPhase, fftSize, fftSize);
		intensity += af::real(E * af::conjg(E));
		E = af::fft2(pZ * defocusPhase, fftSize, fftSize);
		intensity += af::real(E * af::conjg(E));
		intensity = af::shift(intensity, fftSize / 2, fftSize / 2);
		if (fftSize > computeSize) {
			int off = (fftSize - computeSize) / 2;
			af::seq crop(off, off + computeSize - 1);
			intensity = intensity(crop, crop);
		}
		return intensity;
	};

	auto computeSlice = [&](const af::array& defocusPhase) {
		if (dualPol) {
			return af::array(
				(computeIntensity(pupilX1, pupilY1, pupilZ1, defocusPhase)
			   + computeIntensity(pupilX2, pupilY2, pupilZ2, defocusPhase)) * 0.5f);
		}
		return computeIntensity(pupilX1, pupilY1, pupilZ1, defocusPhase);
	};

	// Build resampling coordinate arrays for XY pixel size adjustment
	af::array srcRow, srcCol;
	if (needsResample) {
		float ratio = static_cast<float>(this->xyStepNm / actualXyStepNm);
		float center = (computeSize - 1) / 2.0f;
		af::array idx = af::range(af::dim4(computeSize, computeSize), 0).as(f32);
		af::array idy = af::range(af::dim4(computeSize, computeSize), 1).as(f32);
		srcRow = center + (idx - center) * ratio;
		srcCol = center + (idy - center) * ratio;
	}

	auto resampleSlice = [&](const af::array& slice) -> af::array {
		if (!needsResample) return slice;
		return af::approx2(slice, srcRow, srcCol, AF_INTERP_BILINEAR, 0.0f);
	};

	auto centerPad = [&](af::array& arr) {
		if (gridSize == computeSize) return;
		int off = (gridSize - computeSize) / 2;
		af::dim4 dims = arr.dims();
		dims[0] = gridSize;
		dims[1] = gridSize;
		af::array padded = af::constant(0.0f, dims);
		if (arr.dims(2) > 1) {
			padded(af::seq(off, off + computeSize - 1),
			       af::seq(off, off + computeSize - 1),
			       af::span) = arr;
		} else {
			padded(af::seq(off, off + computeSize - 1),
			       af::seq(off, off + computeSize - 1)) = arr;
		}
		arr = padded;
	};

	auto normalize = [&](af::array& arr) {
		if (this->normMode == SumNormalization) {
			double total = af::sum<double>(af::flat(arr));
			if (total > 0.0) arr = arr / static_cast<float>(total);
		} else if (this->normMode == PeakNormalization) {
			double peak = af::max<double>(af::flat(arr));
			if (peak > 0.0) arr = arr / static_cast<float>(peak);
		}
	};

	// Single z-plane: return 2D result (no defocus phase needed)
	if (this->numZPlanes == 1) {
		af::array unity = af::complex(
			af::constant(1.0f, computeSize, computeSize),
			af::constant(0.0f, computeSize, computeSize));

		af::array psf = resampleSlice(computeSlice(unity));
		centerPad(psf);
		if (this->confocalEnabled) {
			psf = psf * psf;
		}
		normalize(psf);
		return psf;
	}

	// 6. Multiple z-planes: per-plane loop
	float k = static_cast<float>(2.0 * M_PI / this->wavelengthNm);
	float n = static_cast<float>(this->nImmersion);
	int centerZ = this->numZPlanes / 2;

	af::array psf3D = af::constant(0.0f, computeSize, computeSize, this->numZPlanes);

	for (int iz = 0; iz < this->numZPlanes; ++iz) {
		float z = static_cast<float>((iz - centerZ) * this->zStepNm);
		af::array defocusArg = k * n * z * this->cosTheta;
		af::array defocusPhase = af::complex(af::cos(defocusArg), af::sin(defocusArg));
		psf3D(af::span, af::span, iz) = resampleSlice(computeSlice(defocusPhase));
	}

	centerPad(psf3D);
	if (this->confocalEnabled) {
		psf3D = psf3D * psf3D;
	}
	normalize(psf3D);
	return psf3D;
}

void RichardsWolfCalculator::buildCoordinateCache(int gridSize)
{
	this->cachedGridSize = gridSize;

	// Normalized pupil coordinates [-1, 1]
	af::array sx = (2.0f * af::range(af::dim4(gridSize, gridSize), 1).as(f32)
	                / (gridSize - 1) - 1.0f);
	af::array sy = (2.0f * af::range(af::dim4(gridSize, gridSize), 0).as(f32)
	                / (gridSize - 1) - 1.0f);

	float sMax = static_cast<float>(this->na / this->nImmersion);
	af::array rhoSq = sx * sx + sy * sy;

	// Circular aperture mask
	this->apertureMask = (rhoSq <= 1.0f).as(f32);

	// Physical angles from normalized coordinates scaled by NA/n
	af::array sinThetaSq = af::clamp(sMax * sMax * rhoSq, 0.0f, SIN_THETA_SQ_CLAMP_MAX);
	this->cosTheta = af::sqrt(1.0f - sinThetaSq) * this->apertureMask;
	this->sinTheta = af::sqrt(sinThetaSq) * this->apertureMask;

	af::array phi = af::atan2(sy * sMax, sx * sMax);
	this->cosPhi = af::cos(phi);
	this->sinPhi = af::sin(phi);
	this->sin2Phi = 2.0f * this->sinPhi * this->cosPhi;
	this->cos2Phi = this->cosPhi * this->cosPhi - this->sinPhi * this->sinPhi;

	// 1/sz Jacobian correction (Cartesian formulation)
	af::array szSafe = af::clamp(this->cosTheta, COS_THETA_CLAMP_MIN, 1.0f);
	this->szCorrection = (1.0f / szSafe) * this->apertureMask;

	// Apodization factor: sqrt(cos theta)
	this->apodizationFactor = af::sqrt(szSafe) * this->apertureMask;

	af::eval(this->apertureMask, this->cosTheta, this->sinTheta,
	         this->cosPhi, this->sinPhi, this->sin2Phi, this->cos2Phi,
	         this->szCorrection, this->apodizationFactor);
}

void RichardsWolfCalculator::buildVectorialPupil(
	const af::array& complexPupil, float e0x, float e0y,
	af::array& pupilX, af::array& pupilY, af::array& pupilZ) const
{
	// General Cartesian Richards-Wolf formulation for incident field (e0x, e0y):
	// e_inf_x = [(cosθ+1) + (cosθ-1)cos2φ] e0x/2 + (cosθ-1)sin2φ e0y/2
	// e_inf_y = (cosθ-1)sin2φ e0x/2 + [(cosθ+1) - (cosθ-1)cos2φ] e0y/2
	// e_inf_z = -sinθ (cosφ e0x + sinφ e0y)
	af::array cosP1 = this->cosTheta + 1.0f;
	af::array cosM1 = this->cosTheta - 1.0f;

	pupilX = complexPupil * ((cosP1 + cosM1 * this->cos2Phi) * e0x
	                       + cosM1 * this->sin2Phi * e0y) * 0.5f;
	pupilY = complexPupil * (cosM1 * this->sin2Phi * e0x
	                       + (cosP1 - cosM1 * this->cos2Phi) * e0y) * 0.5f;
	pupilZ = complexPupil * (-this->sinTheta * (this->cosPhi * e0x + this->sinPhi * e0y));
}

// --- Setters / Getters ---

void RichardsWolfCalculator::setPhaseScale(double value) { this->phaseScale = value; }
double RichardsWolfCalculator::getPhaseScale() const { return this->phaseScale; }

void RichardsWolfCalculator::setWavelengthNm(double value)
{
	this->wavelengthNm = value;
}

double RichardsWolfCalculator::getWavelengthNm() const { return this->wavelengthNm; }

void RichardsWolfCalculator::setNumericalAperture(double value)
{
	this->na = value;
	this->cachedGridSize = 0;
}

double RichardsWolfCalculator::getNumericalAperture() const { return this->na; }

void RichardsWolfCalculator::setImmersionIndex(double value)
{
	this->nImmersion = value;
	this->cachedGridSize = 0;
}

double RichardsWolfCalculator::getImmersionIndex() const { return this->nImmersion; }

void RichardsWolfCalculator::setZStepNm(double value) { this->zStepNm = value; }
double RichardsWolfCalculator::getZStepNm() const { return this->zStepNm; }

void RichardsWolfCalculator::setNumZPlanes(int value) { this->numZPlanes = (value > 0) ? value : 1; }
int RichardsWolfCalculator::getNumZPlanes() const { return this->numZPlanes; }

void RichardsWolfCalculator::setApodization(bool enabled) { this->apodizationEnabled = enabled; }
bool RichardsWolfCalculator::getApodization() const { return this->apodizationEnabled; }

void RichardsWolfCalculator::setNormalizationMode(NormalizationMode mode) { this->normMode = mode; }
RichardsWolfCalculator::NormalizationMode RichardsWolfCalculator::getNormalizationMode() const { return this->normMode; }

void RichardsWolfCalculator::setPolarizationMode(PolarizationMode mode) { this->polMode = mode; }
RichardsWolfCalculator::PolarizationMode RichardsWolfCalculator::getPolarizationMode() const { return this->polMode; }

void RichardsWolfCalculator::setPolarizationAngle(double degrees) { this->polAngle = degrees; }
double RichardsWolfCalculator::getPolarizationAngle() const { return this->polAngle; }

void RichardsWolfCalculator::setXyStepNm(double value) { this->xyStepNm = (value >= 0.0) ? value : 0.0; }
double RichardsWolfCalculator::getXyStepNm() const { return this->xyStepNm; }

void RichardsWolfCalculator::setScalingMode(ScalingMode mode) { this->scalingMode = mode; }
RichardsWolfCalculator::ScalingMode RichardsWolfCalculator::getScalingMode() const { return this->scalingMode; }

void RichardsWolfCalculator::invalidateCache()
{
	this->cachedGridSize = 0;
	this->cosTheta = af::array();
	this->sinTheta = af::array();
	this->cosPhi = af::array();
	this->sinPhi = af::array();
	this->sin2Phi = af::array();
	this->cos2Phi = af::array();
	this->apertureMask = af::array();
	this->szCorrection = af::array();
	this->apodizationFactor = af::array();
}

// --- Settings descriptors for auto-UI ---

QVector<NumericSettingDescriptor> RichardsWolfCalculator::getSettingsDescriptors() const
{
	return {
		{KEY_PHASE_SCALE,    tr("Phase Scale"),            tr("Phase scale in rad per wavefront unit.\n"
		                                                      "The wavefront is multiplied by this value as first step of PSF calculation.\n"
		                                                      "Default value: 1.0."),
		                                                      0.001, 10000.0, 1.0, DEF_PHASE_SCALE, 3, {}},
		{KEY_WAVELENGTH_NM,  tr("Wavelength (nm)"),        tr("Emission wavelength in nanometers"),      1.0, 10000.0, 1.0,   DEF_WAVELENGTH_NM, 0, {}},
		{KEY_NA,             tr("Numerical Aperture"),     tr("Objective numerical aperture"),            0.01,   2.0,  0.01,  DEF_NA,            2, {}},
		{KEY_N_IMMERSION,    tr("Immersion Index"),        tr("Refractive index of immersion medium"),    1.0,    3.0,  0.001, DEF_N_IMMERSION,   3, {}},
		{KEY_Z_STEP_NM,      tr("Z Step (nm)"),            tr("Axial step between z-planes"),             1.0, 99999.0, 10.0,  DEF_Z_STEP_NM,     0, {}},
		{KEY_NUM_Z_PLANES,   tr("Num Z Planes"),           tr("Number of axial planes"),                  1.0, 10240.0,  1.0, static_cast<double>(DEF_NUM_Z_PLANES), 0, {}},
		{KEY_XY_STEP_NM,     tr("XY Pixel Size (nm)"),    tr("Lateral pixel spacing in nm (0 = native FFT spacing)"), 0.0, 99999999.0, 1.0, DEF_XY_STEP_NM, 1, {}},
	};
}

QVariantMap RichardsWolfCalculator::serializeSettings() const
{
	QVariantMap m;
	m[KEY_PHASE_SCALE]     = this->phaseScale;
	m[KEY_WAVELENGTH_NM]   = this->wavelengthNm;
	m[KEY_NA]              = this->na;
	m[KEY_N_IMMERSION]     = this->nImmersion;
	m[KEY_Z_STEP_NM]       = this->zStepNm;
	m[KEY_NUM_Z_PLANES]    = this->numZPlanes;
	m[KEY_APODIZATION]     = this->apodizationEnabled;
	m[KEY_NORMALIZATION]   = static_cast<int>(this->normMode);
	m[KEY_POLARIZATION]   = static_cast<int>(this->polMode);
	m[KEY_POL_ANGLE]      = this->polAngle;
	m[KEY_XY_STEP_NM]     = this->xyStepNm;
	m[KEY_SCALING_MODE]   = static_cast<int>(this->scalingMode);
	m[KEY_CONFOCAL]       = this->confocalEnabled;
	return m;
}

void RichardsWolfCalculator::deserializeSettings(const QVariantMap& settings)
{
	this->phaseScale       = settings.value(KEY_PHASE_SCALE, DEF_PHASE_SCALE).toDouble();
	this->wavelengthNm     = settings.value(KEY_WAVELENGTH_NM, DEF_WAVELENGTH_NM).toDouble();
	this->na               = settings.value(KEY_NA, DEF_NA).toDouble();
	this->nImmersion       = settings.value(KEY_N_IMMERSION, DEF_N_IMMERSION).toDouble();
	this->zStepNm          = settings.value(KEY_Z_STEP_NM, DEF_Z_STEP_NM).toDouble();
	this->numZPlanes       = settings.value(KEY_NUM_Z_PLANES, DEF_NUM_Z_PLANES).toInt();
	this->apodizationEnabled = settings.value(KEY_APODIZATION, DEF_APODIZATION).toBool();
	this->normMode         = static_cast<NormalizationMode>(
		settings.value(KEY_NORMALIZATION, DEF_NORMALIZATION).toInt());
	this->polMode          = static_cast<PolarizationMode>(
		settings.value(KEY_POLARIZATION, DEF_POLARIZATION).toInt());
	this->polAngle         = settings.value(KEY_POL_ANGLE, DEF_POL_ANGLE).toDouble();
	this->xyStepNm         = settings.value(KEY_XY_STEP_NM, DEF_XY_STEP_NM).toDouble();
	this->scalingMode      = static_cast<ScalingMode>(
		settings.value(KEY_SCALING_MODE, DEF_SCALING_MODE).toInt());
	this->confocalEnabled  = settings.value(KEY_CONFOCAL, DEF_CONFOCAL).toBool();
	this->cachedGridSize   = 0; // force rebuild
}

void RichardsWolfCalculator::applyInlineSettings(const QVariantMap& settings)
{
	if (!settings.isEmpty()) {
		this->deserializeSettings(settings);
	}
}

void RichardsWolfCalculator::setNumOutputPlanes(int numPlanes)
{
	this->setNumZPlanes(numPlanes);
}
