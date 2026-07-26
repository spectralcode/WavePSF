// Regression test for the Wiener kernel pad/shift order: the PSF was
// circularly shifted at its own size and only zero-padded afterwards by
// af::fft2, which stranded the wrapped kernel halves in the middle of the
// padded array instead of at its far edges. With a PSF smaller than the
// computation size, any kernel with mass that wraps around the center -
// every realistic PSF - produced ghosted/shifted output.
// Contract under test: deconvolving with a small PSF must be numerically
// equivalent to deconvolving with the same kernel values embedded centered
// in a full-size array (the matched-size path, which was always correct).
// A centered delta cannot expose the bug (nothing wraps), so it serves as
// a harness sanity check instead.
#include "core/psf/deconvolver.h"
#include <arrayfire.h>
#include <QObject>
#include <QString>
#include <QVector>
#include <cmath>
#include <cstdio>

namespace {

const int inputSize = 32;
const int psfSize = 8;
const float noiseToSignalFactor = 0.01f;
const double tolerance = 1e-3;

// Deterministic, strictly positive, aperiodic and asymmetric test image so
// that intensity conservation never sees a near-zero sum and any spatial
// displacement of the output stays visible. Column-major host layout.
af::array makeInput()
{
	QVector<float> buffer(inputSize * inputSize);
	for (int c = 0; c < inputSize; ++c) {
		for (int r = 0; r < inputSize; ++r) {
			double v = r * 0.371 + c * 0.613;
			buffer[r + c * inputSize] = 0.2f + static_cast<float>(v - std::floor(v));
		}
	}
	buffer[7 + 5 * inputSize] += 3.0f;
	buffer[20 + 11 * inputSize] += 2.0f;
	buffer[13 + 26 * inputSize] += 2.5f;
	return af::array(inputSize, inputSize, buffer.constData());
}

// Normalized Gaussian centered at (psfSize/2, psfSize/2); its upper-left
// mass is what wraps in the ifftshift and exposes the padding-order bug.
QVector<float> gaussianKernelHost()
{
	const double sigma = 1.5;
	const int center = psfSize / 2;
	QVector<float> kernel(psfSize * psfSize);
	double sum = 0.0;
	for (int c = 0; c < psfSize; ++c) {
		for (int r = 0; r < psfSize; ++r) {
			double dr = r - center;
			double dc = c - center;
			double v = std::exp(-(dr * dr + dc * dc) / (2.0 * sigma * sigma));
			kernel[r + c * psfSize] = static_cast<float>(v);
			sum += v;
		}
	}
	for (int i = 0; i < kernel.size(); ++i) {
		kernel[i] = static_cast<float>(kernel[i] / sum);
	}
	return kernel;
}

QVector<float> deltaKernelHost(int row, int col)
{
	QVector<float> kernel(psfSize * psfSize, 0.0f);
	kernel[row + col * psfSize] = 1.0f;
	return kernel;
}

af::array smallPsf(const QVector<float>& kernel)
{
	return af::array(psfSize, psfSize, kernel.constData());
}

// The same kernel values embedded so its center (psfSize/2, psfSize/2)
// lands on (inputSize/2, inputSize/2) - the reference goes through the
// matched-size path, which is correct independent of the bug under test.
af::array embeddedPsf(const QVector<float>& kernel)
{
	const int offset = inputSize / 2 - psfSize / 2;
	QVector<float> big(inputSize * inputSize, 0.0f);
	for (int c = 0; c < psfSize; ++c) {
		for (int r = 0; r < psfSize; ++r) {
			big[(offset + r) + (offset + c) * inputSize] = kernel[r + c * psfSize];
		}
	}
	return af::array(inputSize, inputSize, big.constData());
}

bool checkResultValid(const af::array& result, const QString& lastError,
	const char* caseName, const char* which)
{
	if (result.isempty()) {
		std::fprintf(stderr, "FAIL [%s]: %s deconvolution returned empty (%s)\n",
			caseName, which,
			lastError.isEmpty() ? "no error message" : qPrintable(lastError));
		return false;
	}
	// af::max ignores NaNs (and NaN > tolerance is false), so a non-finite
	// result could otherwise slip through the max|diff| checks as a pass.
	if (af::anyTrue<bool>(af::isNaN(result))
			|| af::anyTrue<bool>(af::isInf(result))) {
		std::fprintf(stderr, "FAIL [%s]: %s result contains non-finite values\n",
			caseName, which);
		return false;
	}
	return true;
}

// Core property: small PSF and its centered full-size embedding must give
// numerically equivalent results.
bool testEquivalence(Deconvolver& deconvolver, const QString& lastError,
	const af::array& input, const QVector<float>& kernel, const char* caseName)
{
	// Not named "small": Windows' rpcndr.h defines small as a macro.
	af::array smallResult = deconvolver.deconvolve(input, smallPsf(kernel));
	if (!checkResultValid(smallResult, lastError, caseName, "small-PSF")) return false;

	af::array reference = deconvolver.deconvolve(input, embeddedPsf(kernel));
	if (!checkResultValid(reference, lastError, caseName, "embedded-PSF")) return false;

	double maxDiff = af::max<float>(af::abs(smallResult - reference));
	if (maxDiff > tolerance) {
		std::fprintf(stderr,
			"FAIL [%s]: small vs embedded PSF differ, max|diff|=%g (tolerance %g)\n",
			caseName, maxDiff, tolerance);
		return false;
	}
	std::printf("%s ok (max|diff|=%g)\n", caseName, maxDiff);
	return true;
}

// Harness sanity: a centered delta has no wrapping mass, so this passes
// even with the bug present. |H|=1, and intensity conservation cancels the
// uniform 1/(1+nsr) Wiener scale, so the output must reproduce the input.
bool testCenteredDeltaIdentity(Deconvolver& deconvolver, const QString& lastError,
	const af::array& input)
{
	af::array result = deconvolver.deconvolve(
		input, smallPsf(deltaKernelHost(psfSize / 2, psfSize / 2)));
	if (!checkResultValid(result, lastError, "centered delta identity", "small-PSF")) {
		return false;
	}

	double maxDiff = af::max<float>(af::abs(result - input));
	if (maxDiff > tolerance) {
		std::fprintf(stderr,
			"FAIL [centered delta identity]: output differs from input, "
			"max|diff|=%g (tolerance %g)\n", maxDiff, tolerance);
		return false;
	}
	std::printf("centered delta identity ok (max|diff|=%g)\n", maxDiff);
	return true;
}

} // namespace

int main()
{
	// Pin the CPU backend so the test does not depend on GPU availability,
	// driver state, or JIT behavior. All af::array objects must be created
	// after this call - arrays are bound to the active backend.
	try {
		af::setBackend(AF_BACKEND_CPU);
		af::setDevice(0);
	} catch (const af::exception& e) {
		std::fprintf(stderr, "FAIL: cannot initialize ArrayFire CPU backend: %s\n",
			e.what());
		return 1;
	}

	Deconvolver deconvolver;
	deconvolver.setAlgorithm(Deconvolver::WIENER);
	deconvolver.setWienerNoiseToSignalFactor(noiseToSignalFactor);

	QString lastError;
	QObject::connect(&deconvolver, &Deconvolver::error,
		[&lastError](QString message) { lastError = message; });

	af::array input = makeInput();

	bool ok = testEquivalence(deconvolver, lastError, input,
		gaussianKernelHost(), "gaussian wrap equivalence");
	ok = testEquivalence(deconvolver, lastError, input,
		deltaKernelHost(2, 2), "off-center delta equivalence") && ok;
	ok = testCenteredDeltaIdentity(deconvolver, lastError, input) && ok;

	if (!ok) {
		return 1;
	}
	std::printf("PASS\n");
	return 0;
}
