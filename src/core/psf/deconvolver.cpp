#include "deconvolver.h"
#include "utils/logging.h"


Deconvolver::Deconvolver(int iterations, QObject* parent)
	: QObject(parent)
	, iterations(iterations)
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

	// Ensure input is float
	af::array input = blurredInput.as(f32);

	// Zero-pad PSF to match input dimensions if needed
	af::array psfPadded = psf;
	if (psf.dims(0) != input.dims(0) || psf.dims(1) != input.dims(1)) {
		psfPadded = this->zeroPadPSF(psf, input.dims(0), input.dims(1));
	}

	af::array result;
	try {
		result = af::iterativeDeconv(input, psfPadded.as(f32), this->iterations, 1.0f, AF_ITERATIVE_DECONV_RICHARDSONLUCY);
	} catch (af::exception& e) {
		emit error(tr("ArrayFire deconvolution error: ") + QString(e.what()));
		return af::array();
	}

	return result;
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

int Deconvolver::getIterations() const
{
	return this->iterations;
}

af::array Deconvolver::zeroPadPSF(const af::array& psf, int targetRows, int targetCols) const
{
	af::array padded = af::constant(0.0f, targetRows, targetCols, f32);

	int psfRows = psf.dims(0);
	int psfCols = psf.dims(1);
	int offsetR = (targetRows - psfRows) / 2;
	int offsetC = (targetCols - psfCols) / 2;

	padded(af::seq(offsetR, offsetR + psfRows - 1), af::seq(offsetC, offsetC + psfCols - 1)) = psf;

	return padded;
}
