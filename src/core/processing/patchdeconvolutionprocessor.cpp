#include "patchdeconvolutionprocessor.h"
#include "core/psf/deconvolver.h"
#include "core/psf/ipsfgenerator.h"
#include <QtGlobal>

af::array PatchDeconvolutionProcessor::process(
	const PatchDeconvolutionInput& input,
	IPSFGenerator* generator,
	Deconvolver* deconvolver)
{
	if (deconvolver == nullptr || input.inputPatch.isempty()) {
		return af::array();
	}

	af::array psf = input.psf;
	if (psf.isempty()) {
		if (!input.generatePSFIfMissing || generator == nullptr) {
			return af::array();
		}

		if (generator->supportsCoefficients()) {
			if (input.coefficients.isEmpty()) {
				return af::array();
			}
			generator->setAllCoefficients(input.coefficients);
		}

		PSFRequest request;
		request.gridSize = input.gridSize;
		request.frame = input.frameNr;
		request.patchIdx = input.patchIdx;
		psf = generator->generatePSF(request);
	}

	af::array psfFrame = PatchDeconvolutionProcessor::extractPSFFrame(
		psf, input.frameNr);
	if (psfFrame.isempty()) {
		return af::array();
	}

	return deconvolver->deconvolve(
		input.inputPatch,
		psfFrame);
}

af::array PatchDeconvolutionProcessor::extractPSFFrame(
	const af::array& psf,
	int frameNr)
{
	if (psf.numdims() > 2 && psf.dims(2) > 1) {
		int maxZ = static_cast<int>(psf.dims(2)) - 1;
		int z = qBound(0, frameNr, maxZ);
		return psf(af::span, af::span, z);
	}
	return psf;
}
