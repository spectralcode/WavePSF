#include "volumedeconvolutionprocessor.h"
#include "core/psf/deconvolver.h"

af::array VolumeDeconvolutionProcessor::process(
	const VolumeDeconvolutionInput& input,
	Deconvolver* deconvolver)
{
	if (deconvolver == nullptr
		|| input.inputVolume.isempty()
		|| input.psfVolume.isempty()) {
		return af::array();
	}

	return deconvolver->deconvolve(
		input.inputVolume,
		input.psfVolume);
}
