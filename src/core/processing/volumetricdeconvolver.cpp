#define NOMINMAX
#include "volumetricdeconvolver.h"
#include "utils/logging.h"
#include <limits>
#include <cmath>

VolumetricDeconvolver::VolumetricDeconvolver(QObject* parent)
	: QObject(parent)
	, paddingMode(MIRROR_PAD)
	, accelerationMode(ACCEL_BIGGS_ANDREWS)
	, regularizer(REGULARIZER_NONE)
	, regularizationWeight(DEFAULT_REGULARIZATION_WEIGHT)
	, voxelSizeY(1.0f)
	, voxelSizeX(1.0f)
	, voxelSizeZ(1.0f)
	, cancelRequested(false)
{
}

void VolumetricDeconvolver::requestCancel()
{
	this->cancelRequested = true;
}

void VolumetricDeconvolver::resetCancel()
{
	this->cancelRequested = false;
}

bool VolumetricDeconvolver::wasCancelled() const
{
	return this->cancelRequested;
}

void VolumetricDeconvolver::setPaddingMode(PaddingMode mode)
{
	this->paddingMode = mode;
}

void VolumetricDeconvolver::setAccelerationMode(AccelerationMode mode)
{
	this->accelerationMode = mode;
}

void VolumetricDeconvolver::setRegularizer(RegularizerMode mode)
{
	this->regularizer = mode;
}

void VolumetricDeconvolver::setRegularizationWeight(float weight)
{
	this->regularizationWeight = weight;
}

void VolumetricDeconvolver::setVoxelSize(float sizeY, float sizeX, float sizeZ)
{
	this->voxelSizeY = (std::max)(sizeY, 1e-6f);
	this->voxelSizeX = (std::max)(sizeX, 1e-6f);
	this->voxelSizeZ = (std::max)(sizeZ, 1e-6f);
}

QStringList VolumetricDeconvolver::getAccelerationModeNames()
{
	return QStringList {
		"None",
		"Biggs-Andrews"
	};
}

QStringList VolumetricDeconvolver::getPaddingModeNames()
{
	return QStringList {
		"Zero Padding",
		"Mirror Padding"
	};
}

QStringList VolumetricDeconvolver::getRegularizerNames()
{
	return QStringList {
		"None",
		"Total Variation"
	};
}

af::array VolumetricDeconvolver::computeRegularizationDenominator3D(const af::array& x) const
{
	//see: Dey et al, "Richardson–Lucy algorithm with total variation regularization for 3D confocal microscope deconvolution"
	const float eps = 1e-8f;
	int H = x.dims(0);
	int W = x.dims(1);
	int D = x.dims(2);

	float invVY = 1.0f / this->voxelSizeY;
	float invVX = 1.0f / this->voxelSizeX;
	float invVZ = 1.0f / this->voxelSizeZ;

	// forward differences
	af::array dfy = (af::shift(x, -1, 0, 0) - x) * invVY;
	dfy(af::seq(H - 1, H - 1), af::span, af::span) = 0.0f;

	af::array dfx = (af::shift(x, 0, -1, 0) - x) * invVX;
	dfx(af::span, af::seq(W - 1, W - 1), af::span) = 0.0f;

	af::array dfz = (af::shift(x, 0, 0, -1) - x) * invVZ;
	dfz(af::span, af::span, af::seq(D - 1, D - 1)) = 0.0f;

	// Gradient magnitude
	af::array norm = af::sqrt(dfy * dfy + dfx * dfx + dfz * dfz + eps);

	// Normalized gradient (TV)
	af::array ny = dfy / norm;
	af::array nx = dfx / norm;
	af::array nz = dfz / norm;

	// backward differences: d(n)/di = n[i] - n[i-1]
	af::array divY = (ny - af::shift(ny, 1, 0, 0)) * invVY;
	divY(af::seq(0, 0), af::span, af::span) = 0.0f;

	af::array divX = (nx - af::shift(nx, 0, 1, 0)) * invVX;
	divX(af::span, af::seq(0, 0), af::span) = 0.0f;

	af::array divZ = (nz - af::shift(nz, 0, 0, 1)) * invVZ;
	divZ(af::span, af::span, af::seq(0, 0)) = 0.0f;

	af::array divN = divY + divX + divZ;

	return af::max(1.0f - this->regularizationWeight * divN, 0.001f); //clamped to stay positive
}

int VolumetricDeconvolver::nextGoodFFTSize(int n)
{
	// Find the smallest number >= n that factors into only 2, 3, 5, 7
	// cuFFT is highly optimized for these prime factors
	// todo: check if this is also beneficial for cpu and openCL backends
	while (true) {
		int m = n;
		while (m % 2 == 0) m /= 2;
		while (m % 3 == 0) m /= 3;
		while (m % 5 == 0) m /= 5;
		while (m % 7 == 0) m /= 7;
		if (m == 1) return n;
		++n;
	}
}

af::array VolumetricDeconvolver::preparePSF(const af::array& psf,
	int volH, int volW, int volD) const
{
	int pH = psf.dims(0);
	int pW = psf.dims(1);
	int pD = psf.dims(2);

	// Zero-pad PSF to volume size
	af::array padded = af::constant(0.0f, volH, volW, volD);
	padded(af::seq(pH), af::seq(pW), af::seq(pD)) = psf;

	// Shift PSF center to origin for correct FFT-based convolution
	padded = af::shift(padded, -(pH / 2), -(pW / 2), -(pD / 2));

	return padded;
}

af::array VolumetricDeconvolver::mirrorPad3D(const af::array& vol,
	int fH, int fW, int fD)
{
	int H = vol.dims(0);
	int W = vol.dims(1);
	int D = vol.dims(2);

	// Build mirror-reflected index arrays using split halos for FFT circular layout.
	// Right halo (after data) reflects the right boundary.
	// Left halo (end of array, wrapping to negative FFT positions) reflects the left
	// boundary, keeping the circular wrap-around transition smooth.
	auto buildMirrorIndices = [](int padSize, int origSize) -> af::array {
		if (origSize <= 1) {
			return af::constant(0, padSize, s32);
		}
		int totalPad = padSize - origSize;
		// Prefer left halo (end of array = FFT circular wrap) for smooth wrap-around
		int leftPad = (totalPad + 1) / 2;

		af::array idx = af::range(af::dim4(padSize), 0, s32);

		// Right halo reflects right boundary: index H+k → source H-2-k
		af::array rightSrc = 2 * (origSize - 1) - idx;
		// Left halo reflects left boundary: index fH-1-k → source 1+k
		af::array leftSrc = padSize - idx;

		af::array isOriginal = idx < origSize;
		af::array isLeftHalo = idx >= (padSize - leftPad);

		af::array src = af::select(isOriginal, idx,
						af::select(isLeftHalo, leftSrc, rightSrc));
		return af::clamp(src, 0, origSize - 1).as(s32);
	};

	// Pad each dimension sequentially via af::lookup (gather along axis)
	af::array result = vol;
	if (fH > H) {
		result = af::lookup(result, buildMirrorIndices(fH, H), 0);
	}
	if (fW > W) {
		result = af::lookup(result, buildMirrorIndices(fW, W), 1);
	}
	if (fD > D) {
		result = af::lookup(result, buildMirrorIndices(fD, D), 2);
	}
	return result;
}

af::array VolumetricDeconvolver::padVolume(const af::array& volume,
	int fH, int fW, int fD) const
{
	int H = volume.dims(0);
	int W = volume.dims(1);
	int D = volume.dims(2);

	if (fH == H && fW == W && fD == D) {
		return volume.as(f32);
	}

	switch (this->paddingMode) {
		case MIRROR_PAD:
			LOG_INFO() << "Mirror-padding volume:" << H << "x" << W << "x" << D
					   << "->" << fH << "x" << fW << "x" << fD;
			return mirrorPad3D(volume.as(f32), fH, fW, fD);

		case ZERO_PAD:
		default:
			LOG_INFO() << "Zero-padding volume:" << H << "x" << W << "x" << D
					   << "->" << fH << "x" << fW << "x" << fD;
			{
				af::array padded = af::constant(0.0f, fH, fW, fD);
				padded(af::seq(H), af::seq(W), af::seq(D)) = volume.as(f32);
				return padded;
			}
	}
}

af::array VolumetricDeconvolver::deconvolve(const af::array& volume,
	const af::array& psf, int iterations)
{
	if (volume.isempty() || psf.isempty()) {
		emit error("Empty volume or PSF for 3D deconvolution.");
		return af::array();
	}

	int H = volume.dims(0);
	int W = volume.dims(1);
	int D = volume.dims(2);
	int pH = psf.dims(0);
	int pW = psf.dims(1);
	int pD = psf.dims(2);

	af::array psfFloat = psf.as(f32);

	// FFT size: at least max(volume, PSF) per axis, then round to FFT-friendly
	int fH = nextGoodFFTSize((std::max)(H, pH));
	int fW = nextGoodFFTSize((std::max)(W, pW));
	int fD = nextGoodFFTSize((std::max)(D, pD));

	af::array input = this->padVolume(volume, fH, fW, fD);

	// Normalize PSF to sum to 1
	float psfSum = af::sum<float>(af::flat(psfFloat));
	if (psfSum > 0.0f) {
		psfFloat = psfFloat / psfSum;
	}

	try {
		// Precompute OTF and its conjugate (used every iteration, never changes)
		af::array otf = af::fft3(this->preparePSF(psfFloat, fH, fW, fD));
		af::array otfConj = af::conjg(otf);
		psfFloat = af::array(); // free PSF data

		// Release temporary allocations from PSF preparation
		af::deviceGC();

		const float epsilon = 1e-12f;

		af::array estimate = input.copy();

		// Biggs-Andrews acceleration state
		af::array prevEstimate;
		af::array prevResidual;
		float lambda = 0.0f;

		af::array temp;
		for (int i = 0; i < iterations; ++i) {
			af::array target = estimate;
			if (this->accelerationMode == ACCEL_BIGGS_ANDREWS
				&& i >= 1 && !prevEstimate.isempty()) {
				target = af::max(estimate + lambda * (estimate - prevEstimate), 0.0f);
				af::eval(target);
			}

			// --- Standard Richardson-Lucy step --- // similar to the implementation in Flowdec, restoration.py: https://github.com/hammerlab/flowdec/blob/fa29e33e9729404572bec1e8a100f567dd6b747f/python/flowdec/restoration.py#L295
			temp = af::real(af::ifft3(otf * af::fft3(target)));
			temp = input / af::max(temp, epsilon);
			temp = af::real(af::ifft3(otfConj * af::fft3(temp)));

			// --- Optional regularization ---
			af::array rlResult;
			if (this->regularizer == REGULARIZER_TOTAL_VARIATION) {
				af::array regDenom = this->computeRegularizationDenominator3D(target);
				rlResult = af::max((target / regDenom) * temp, 0.0f);
			} else {
				rlResult = af::max(target * temp, 0.0f);
			}
			temp = af::array();

			// --- Acceleration update ---
			if (this->accelerationMode == ACCEL_BIGGS_ANDREWS) {
				if (i >= 1) {
					af::array residual = rlResult - target;
					af::eval(residual);
					if (!prevResidual.isempty()) {
						float dot = af::sum<float>(af::flat(residual * prevResidual));
						float dotPrev = af::sum<float>(af::flat(prevResidual * prevResidual));
						lambda = (dotPrev > 0.0f)
							? (std::min)(1.0f, (std::max)(0.0f, dot / dotPrev))
							: 0.0f;
					}
					prevResidual = residual;
				}
				prevEstimate = estimate;
			}
			estimate = rlResult;
			rlResult = af::array();
			af::eval(estimate);

			// GPU sync for correct progress reporting (not needed for Biggs-Andrews since af::sum<float> does implicit sync)
			if (this->accelerationMode != ACCEL_BIGGS_ANDREWS) {
				af::sync();
			}

			emit iterationCompleted(i + 1, iterations);

			if (this->cancelRequested) {
				LOG_INFO() << "3D deconvolution cancelled at iteration" << (i + 1) << "/" << iterations;
				return af::array();
			}
		}

		// Crop back to original volume dimensions
		if (fH != H || fW != W || fD != D) {
			estimate = estimate(af::seq(H), af::seq(W), af::seq(D));
		}

		return estimate;
	} catch (af::exception& e) {
		emit error(QString("3D deconvolution error: %1").arg(e.what()));
		return af::array();
	}
}
