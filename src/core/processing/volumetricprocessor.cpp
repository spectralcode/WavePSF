#include "volumetricprocessor.h"
#include "controller/imagesession.h"
#include "core/psf/psfmodule.h"
#include "core/psf/ipsfgenerator.h"
#include "core/psf/psffilemanager.h"
#include "data/wavefrontparametertable.h"
#include "utils/logging.h"

// --- Static helpers for per-patch volumetric deconvolution ---

af::array VolumetricProcessor::resolve2DPSF(PSFModule* psfModule,
	WavefrontParameterTable* paramTable,
	PSFFileManager* psfFileManager,
	int frame, int patchIdx)
{
	// Priority 1: Override map (one-shot loaded PSFs)
	if (psfFileManager != nullptr && psfFileManager->hasOverride(frame, patchIdx)) {
		return psfFileManager->getOverride(frame, patchIdx);
	}

	// Priority 2: Custom PSF folder
	if (psfFileManager != nullptr && psfFileManager->isCustomFolderMode()) {
		af::array psf = psfFileManager->loadPSFFromFolder(frame, patchIdx);
		if (!psf.isempty()) {
			psfFileManager->storeOverride(frame, patchIdx, psf);
			return psf;
		}
	}

	// Priority 3: Compute from coefficients
	QVector<double> coeffs = paramTable->getCoefficients(frame, patchIdx);
	if (!coeffs.isEmpty()) {
		return psfModule->computePSFFromCoefficients(coeffs);
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
	PSFFileManager* psfFileManager,
	int patchIdx, int numFrames)
{
	if (numFrames == 0) return af::array();

	// 3D generator mode: resolve per-patch 3D PSF (frame 0 is canonical slot)
	if (psfModule->getGenerator()->is3D()) {
		// Priority 1: per-patch 3D override (loaded PSF file)
		if (psfFileManager != nullptr && psfFileManager->hasOverride(0, patchIdx)) {
			return psfFileManager->getOverride(0, patchIdx);
		}

		// Priority 2: custom PSF folder
		if (psfFileManager != nullptr && psfFileManager->isCustomFolderMode()) {
			af::array psf = psfFileManager->loadPSFFromFolder(0, patchIdx);
			if (!psf.isempty()) {
				psfFileManager->storeOverride(0, patchIdx, psf);
				return psf;
			}
		}

		// Priority 3: compute from stored coefficients
		QVector<double> coeffs = paramTable->getCoefficients(0, patchIdx);
		if (!coeffs.isEmpty()) {
			return psfModule->computePSFFromCoefficients(coeffs);
		}

		// Fallback: use the currently cached PSF
		return psfModule->getCurrentPSF();
	}

	// Scalar mode: stack per-frame 2D PSFs into a 3D volume
	af::array firstPSF = resolve2DPSF(psfModule, paramTable, psfFileManager, 0, patchIdx);
	if (firstPSF.isempty()) return af::array();

	int pH = firstPSF.dims(0);
	int pW = firstPSF.dims(1);

	af::array psf3d = af::constant(0.0f, pH, pW, numFrames);
	psf3d(af::span, af::span, 0) = firstPSF;

	for (int f = 1; f < numFrames; ++f) {
		af::array slice = resolve2DPSF(psfModule, paramTable, psfFileManager, f, patchIdx);
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
