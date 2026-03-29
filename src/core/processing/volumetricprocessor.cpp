#include "volumetricprocessor.h"
#include "controller/imagesession.h"
#include "core/psf/psfmodule.h"
#include "core/psf/ipsfgenerator.h"
#include "data/wavefrontparametertable.h"
#include "utils/logging.h"

// --- Static helpers for per-patch volumetric deconvolution ---

af::array VolumetricProcessor::resolve2DPSF(PSFModule* psfModule,
	WavefrontParameterTable* paramTable,
	int frame, int patchIdx)
{
	psfModule->setCurrentPatch(frame, patchIdx);

	if (psfModule->getGenerator()->supportsCoefficients()) {
		QVector<double> coeffs = paramTable->getCoefficients(frame, patchIdx);
		if (!coeffs.isEmpty()) {
			return psfModule->computePSFFromCoefficients(coeffs);
		}
	} else {
		PSFRequest req;
		req.gridSize = 0; // ignored by FilePSFGenerator
		req.frame = frame;
		req.patchIdx = patchIdx;
		af::array psf = psfModule->getGenerator()->generatePSF(req);
		if (!psf.isempty()) {
			return psf;
		}
	}

	return af::array();
}

af::array VolumetricProcessor::assembleSubvolume(ImageSession* session,
	int patchX, int patchY)
{
	int frames = session->getInputFrames();
	if (frames == 0) return af::array();

	ImagePatch first = session->getInputPatch(0, patchX, patchY);
	if (!first.isValid()) return af::array();

	int pH = first.data.dims(0);
	int pW = first.data.dims(1);

	af::array subvolume = af::constant(0.0f, pH, pW, frames);
	subvolume(af::span, af::span, 0) = first.data.as(f32);

	for (int f = 1; f < frames; ++f) {
		ImagePatch patch = session->getInputPatch(f, patchX, patchY);
		if (!patch.isValid()) return af::array();
		subvolume(af::span, af::span, f) = patch.data.as(f32);
	}

	return subvolume;
}

af::array VolumetricProcessor::assemble3DPSF(PSFModule* psfModule,
	WavefrontParameterTable* paramTable,
	int patchIdx, int numFrames)
{
	if (numFrames == 0) return af::array();

	// 3D generator mode: generate the full 3D PSF directly
	if (psfModule->getGenerator()->is3D()) {
		psfModule->setCurrentPatch(0, patchIdx);

		if (psfModule->getGenerator()->supportsCoefficients()) {
			QVector<double> coeffs = paramTable->getCoefficients(0, patchIdx);
			if (!coeffs.isEmpty()) {
				return psfModule->computePSFFromCoefficients(coeffs);
			}
		} else {
			PSFRequest req;
			req.frame = 0;
			req.patchIdx = patchIdx;
			af::array psf = psfModule->getGenerator()->generatePSF(req);
			if (!psf.isempty()) {
				return psf;
			}
		}

		// Fallback: use the currently cached PSF
		return psfModule->getCurrentPSF();
	}

	// Scalar mode: stack per-frame 2D PSFs into a 3D volume
	af::array firstPSF = resolve2DPSF(psfModule, paramTable, 0, patchIdx);
	if (firstPSF.isempty()) return af::array();

	int pH = firstPSF.dims(0);
	int pW = firstPSF.dims(1);

	af::array psf3d = af::constant(0.0f, pH, pW, numFrames);
	psf3d(af::span, af::span, 0) = firstPSF;

	for (int f = 1; f < numFrames; ++f) {
		af::array slice = resolve2DPSF(psfModule, paramTable, f, patchIdx);
		if (slice.isempty()) {
			LOG_WARNING() << "No PSF available for frame" << f << "patch" << patchIdx;
			continue;
		}
		psf3d(af::span, af::span, f) = slice;
	}

	return psf3d;
}

void VolumetricProcessor::writeSubvolumeToOutput(ImageSession* session,
	int patchX, int patchY, const af::array& result)
{
	int frames = result.dims(2);
	for (int f = 0; f < frames; ++f) {
		af::array slice = result(af::span, af::span, f);
		session->setOutputPatch(f, patchX, patchY, slice);
	}
}
