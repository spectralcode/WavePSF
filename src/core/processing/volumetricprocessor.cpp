#include "volumetricprocessor.h"
#include "core/psf/ipsfgenerator.h"
#include "data/patchextractor.h"
#include "utils/logging.h"

// --- Static helpers for per-patch volumetric deconvolution ---

af::array VolumetricProcessor::generatePSF(
	IPSFGenerator* generator,
	int gridSize,
	int frame,
	int patchIdx,
	const QVector<double>& coefficients)
{
	if (generator == nullptr) {
		return af::array();
	}

	QVector<double> savedCoefficients;
	bool restoreCoefficients = false;
	if (generator->supportsCoefficients()) {
		if (coefficients.isEmpty()) {
			return af::array();
		}

		savedCoefficients = generator->getAllCoefficients();
		generator->setAllCoefficients(coefficients);
		restoreCoefficients = true;
	}

	PSFRequest request;
	request.gridSize = gridSize;
	request.frame = frame;
	request.patchIdx = patchIdx;

	try {
		af::array psf = generator->generatePSF(request);
		if (restoreCoefficients) {
			generator->setAllCoefficients(savedCoefficients);
		}
		return psf;
	} catch (...) {
		if (restoreCoefficients) {
			generator->setAllCoefficients(savedCoefficients);
		}
		throw;
	}
}

af::array VolumetricProcessor::assembleSubvolume(
	const QVector<af::array>& inputFrames,
	const PatchLayout& patchLayout,
	int patchX,
	int patchY,
	int borderExtension)
{
	if (inputFrames.isEmpty() || !patchLayout.isValid()) {
		return af::array();
	}

	ImagePatch firstPatch = PatchExtractor::extractExtendedPatch(
		inputFrames.first(),
		patchLayout,
		patchX,
		patchY,
		borderExtension);
	if (!firstPatch.isValid()) {
		return af::array();
	}

	const int patchHeight = firstPatch.data.dims(0);
	const int patchWidth = firstPatch.data.dims(1);
	const int frames = inputFrames.size();

	af::array subvolume = af::constant(0.0f, patchHeight, patchWidth, frames);
	subvolume(af::span, af::span, 0) = firstPatch.data.as(f32);

	for (int frame = 1; frame < frames; ++frame) {
		ImagePatch patch = PatchExtractor::extractExtendedPatch(
			inputFrames[frame],
			patchLayout,
			patchX,
			patchY,
			borderExtension);
		if (!patch.isValid()) {
			return af::array();
		}

		subvolume(af::span, af::span, frame) = patch.data.as(f32);
	}

	return subvolume;
}

af::array VolumetricProcessor::assemble3DPSF(
	IPSFGenerator* generator,
	int gridSize,
	int patchIdx,
	int numFrames,
	const QVector<QVector<double>>& coefficientsByFrame)
{
	if (generator == nullptr || numFrames == 0) {
		return af::array();
	}

	// 3D generator mode: generate the full 3D PSF directly.
	if (generator->is3D()) {
		const QVector<double> coefficients = coefficientsByFrame.isEmpty()
			? QVector<double>()
			: coefficientsByFrame.first();
		return VolumetricProcessor::generatePSF(
			generator,
			gridSize,
			0,
			patchIdx,
			coefficients);
	}

	// Scalar mode: stack per-frame 2D PSFs into a 3D volume.
	const QVector<double> firstCoefficients = coefficientsByFrame.isEmpty()
		? QVector<double>()
		: coefficientsByFrame.first();
	af::array firstPSF = VolumetricProcessor::generatePSF(
		generator,
		gridSize,
		0,
		patchIdx,
		firstCoefficients);
	if (firstPSF.isempty()) {
		return af::array();
	}

	const int patchHeight = firstPSF.dims(0);
	const int patchWidth = firstPSF.dims(1);

	af::array psf3d = af::constant(0.0f, patchHeight, patchWidth, numFrames);
	psf3d(af::span, af::span, 0) = firstPSF;

	for (int frame = 1; frame < numFrames; ++frame) {
		const QVector<double> coefficients = frame < coefficientsByFrame.size()
			? coefficientsByFrame[frame]
			: QVector<double>();
		af::array slice = VolumetricProcessor::generatePSF(
			generator,
			gridSize,
			frame,
			patchIdx,
			coefficients);
		if (slice.isempty()) {
			LOG_WARNING() << "No PSF available for frame" << frame << "patch" << patchIdx;
			continue;
		}

		psf3d(af::span, af::span, frame) = slice;
	}

	return psf3d;
}
