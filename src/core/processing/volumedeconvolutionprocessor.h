#ifndef VOLUMEDECONVOLUTIONPROCESSOR_H
#define VOLUMEDECONVOLUTIONPROCESSOR_H

#include <arrayfire.h>

class Deconvolver;

struct VolumeDeconvolutionInput {
	af::array inputVolume;
	af::array psfVolume;
};

class VolumeDeconvolutionProcessor
{
public:
	static af::array process(
		const VolumeDeconvolutionInput& input,
		Deconvolver* deconvolver);
};

#endif // VOLUMEDECONVOLUTIONPROCESSOR_H
