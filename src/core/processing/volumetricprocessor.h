#ifndef VOLUMETRICPROCESSOR_H
#define VOLUMETRICPROCESSOR_H

#include <QString>
#include <arrayfire.h>

class ImageSession;
class PSFModule;
class PSFFileManager;
class WavefrontParameterTable;

class VolumetricProcessor
{
public:
	// Assemble subvolume from all frames for one spatial patch
	static af::array assembleSubvolume(ImageSession* session, int patchX, int patchY);

	// Assemble 3D PSF by stacking per-frame 2D PSFs (same priority as 2D batch)
	static af::array assemble3DPSF(PSFModule* psfModule,
								   WavefrontParameterTable* paramTable,
								   PSFFileManager* psfFileManager,
								   int patchIdx, int numFrames);

	// Write 3D result back to output, one frame at a time
	static void writeSubvolumeToOutput(ImageSession* session,
									   int patchX, int patchY,
									   const af::array& result);

private:
	VolumetricProcessor() = delete;

	// Resolve single 2D PSF for (frame, patchIdx) using existing priority
	static af::array resolve2DPSF(PSFModule* psfModule,
								  WavefrontParameterTable* paramTable,
								  PSFFileManager* psfFileManager,
								  int frame, int patchIdx);
};

#endif // VOLUMETRICPROCESSOR_H
