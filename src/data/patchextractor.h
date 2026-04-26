#ifndef PATCHEXTRACTOR_H
#define PATCHEXTRACTOR_H

#include "imagepatch.h"
#include "patchlayout.h"

class PatchExtractor
{
public:
	static ImagePatch extractExtendedPatch(
		const af::array& frame,
		const PatchLayout& layout,
		int patchX,
		int patchY,
		int borderExtension);
};

#endif // PATCHEXTRACTOR_H
