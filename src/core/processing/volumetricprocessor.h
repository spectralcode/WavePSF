#ifndef VOLUMETRICPROCESSOR_H
#define VOLUMETRICPROCESSOR_H

#include <QVector>
#include <arrayfire.h>
#include "data/patchlayout.h"

class IPSFGenerator;

class VolumetricProcessor
{
public:
	// Assemble a subvolume from frame snapshots.
	static af::array assembleSubvolume(
		const QVector<af::array>& inputFrames,
		const PatchLayout& patchLayout,
		int patchX,
		int patchY,
		int borderExtension);

	// Assemble a PSF volume from a generator snapshot.
	// For 3D generators, only the first coefficient vector is used.
	static af::array assemble3DPSF(
		IPSFGenerator* generator,
		int gridSize,
		int patchIdx,
		int numFrames,
		const QVector<QVector<double>>& coefficientsByFrame = {});

private:
	VolumetricProcessor() = delete;

	static af::array generatePSF(
		IPSFGenerator* generator,
		int gridSize,
		int frame,
		int patchIdx,
		const QVector<double>& coefficients = {});
};

#endif // VOLUMETRICPROCESSOR_H
