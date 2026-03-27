#define NOMINMAX
#include "volumetricdeconvolver.h"
#include "utils/logging.h"
#include <limits>

VolumetricDeconvolver::VolumetricDeconvolver(QObject* parent)
	: QObject(parent)
{
}

int VolumetricDeconvolver::nextGoodFFTSize(int n)
{
	// Find the smallest number >= n that factors into only 2, 3, 5, 7
	// (cuFFT is highly optimized for these prime factors)
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

	// Pad to FFT-friendly dimensions (factorable into 2, 3, 5, 7)
	int fH = nextGoodFFTSize(H);
	int fW = nextGoodFFTSize(W);
	int fD = nextGoodFFTSize(D);

	bool needsPadding = (fH != H || fW != W || fD != D);
	if (needsPadding) {
		LOG_INFO() << "Padding volume for FFT:" << H << "x" << W << "x" << D
				   << "->" << fH << "x" << fW << "x" << fD;
	}

	af::array input;
	if (needsPadding) {
		input = af::constant(0.0f, fH, fW, fD);
		input(af::seq(H), af::seq(W), af::seq(D)) = volume.as(f32);
	} else {
		input = volume.as(f32);
	}

	af::array psfFloat = psf.as(f32);

	// Normalize PSF to sum to 1
	float psfSum = af::sum<float>(af::flat(psfFloat));
	if (psfSum > 0.0f) {
		psfFloat = psfFloat / psfSum;
	}

	try {
		// Precompute OTF using native 3D FFT
		af::array otf = af::fft3(this->preparePSF(psfFloat, fH, fW, fD));
		psfFloat = af::array(); // free PSF data

		// Release temporary allocations from PSF preparation
		af::deviceGC();

		const float epsilon = 1e-12f;

		af::array estimate = input.copy();
		const float maxVal = (std::numeric_limits<float>::max)();
		const float convergenceThreshold = 1e-4f;

		// Biggs-Andrews acceleration state
		af::array prevUpdate;
		af::array checkEstimate = estimate.copy();

		af::array temp;
		int finalIteration = iterations;
		for (int i = 0; i < iterations; ++i) {
			// --- Standard RL step ---
			// af::eval() between steps forces the JIT to dispatch work and
			// release intermediate GPU buffers — prevents VRAM accumulation
			// that causes slowdowns on GPUs with limited memory.
			temp = af::real(af::ifft3(otf * af::fft3(estimate)));
			af::eval(temp);

			temp = input / af::clamp(temp, epsilon, maxVal);
			af::eval(temp);

			temp = af::real(af::ifft3(af::conjg(otf) * af::fft3(temp)));
			af::eval(temp);

			af::array rlResult = af::clamp(estimate * temp, 0.0f, maxVal);
			temp = af::array();
			af::eval(rlResult);

			// --- Biggs-Andrews acceleration ---
			// Uses momentum from previous iterations to speed convergence
			// while preserving RL's multiplicative structure and positivity.
			if (i >= 2 && !prevUpdate.isempty()) {
				af::array curUpdate = rlResult - estimate;
				af::eval(curUpdate);
				float dot = af::sum<float>(af::flat(curUpdate * prevUpdate));
				float dotPrev = af::sum<float>(af::flat(prevUpdate * prevUpdate));
				float alpha = (dotPrev > 0.0f)
					? (std::min)(1.0f, (std::max)(0.0f, dot / dotPrev))
					: 0.0f;
				if (alpha > 0.0f) {
					af::array ratio = rlResult / af::clamp(estimate, epsilon, maxVal);
					estimate = af::clamp(rlResult * af::pow(ratio, alpha), 0.0f, maxVal);
				} else {
					estimate = rlResult;
				}
				prevUpdate = curUpdate;
			} else {
				if (i >= 1) {
					prevUpdate = rlResult - estimate;
					af::eval(prevUpdate);
				}
				estimate = rlResult;
			}
			rlResult = af::array();
			af::eval(estimate);

			// Full GPU sync only every few iterations to reduce overhead
			if ((i + 1) % 5 == 0 || i == iterations - 1) {
				af::sync();

				// Early stopping: check convergence every 5 iterations
				float normEst = af::norm(af::flat(estimate));
				if (normEst > 0.0f) {
					float relChange = af::norm(af::flat(estimate - checkEstimate)) / normEst;
					if (relChange < convergenceThreshold) {
						finalIteration = i + 1;
						LOG_INFO() << "3D RL converged at iteration" << finalIteration
								   << "(relative change:" << relChange << ")";
						emit iterationCompleted(iterations, iterations);
						break;
					}
				}
				checkEstimate = estimate.copy();
			}
			emit iterationCompleted(i + 1, iterations);
		}

		// Crop back to original dimensions
		if (needsPadding) {
			estimate = estimate(af::seq(H), af::seq(W), af::seq(D));
		}

		return estimate;
	} catch (af::exception& e) {
		emit error(QString("3D deconvolution error: %1").arg(e.what()));
		return af::array();
	}
}

size_t VolumetricDeconvolver::estimateGPUMemory(int height, int width, int depth)
{
	// Use padded dimensions for accurate estimate
	int fH = nextGoodFFTSize(height);
	int fW = nextGoodFFTSize(width);
	int fD = nextGoodFFTSize(depth);

	size_t realArray = static_cast<size_t>(fH) * fW * fD * sizeof(float);
	size_t complexArray = realArray * 2;

	// Persistent: input + estimate + prevUpdate + checkEstimate + rlResult (5 real) + otf (complex)
	// Per-iteration peak: fft3 temps + conjg + intermediates (~5 complex)
	return 5 * realArray + 6 * complexArray;
}
