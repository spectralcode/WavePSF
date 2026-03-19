#include "optimizationjobbuilder.h"
#include "controller/imagesession.h"
#include "core/psf/psfmodule.h"
#include "data/wavefrontparametertable.h"
#include "core/psf/psfsettings.h"
#include "utils/logging.h"
#include <QRandomGenerator>

bool OptimizationJobBuilder::buildJobs(
	OptimizationConfig& config,
	ImageSession* imageSession,
	PSFModule* psfModule,
	WavefrontParameterTable* parameterTable,
	int currentFrame,
	int currentPatchX,
	int currentPatchY)
{
	const int gridCols = imageSession->getPatchGridCols();
	const int gridRows = imageSession->getPatchGridRows();
	const int totalPatches = gridCols * gridRows;
	const int totalFrames = imageSession->getInputFrames();
	const bool hasGroundTruth = imageSession->hasGroundTruthData();
	const int coeffCount = psfModule->getAllCoefficients().size();

	config.jobs.clear();

	QVector<int> frameList;
	QVector<int> patchList;

	if (config.mode == 0) {
		// Single patch mode: current frame and current patch
		frameList.append(currentFrame);
		int linearPatch = parameterTable->patchIndex(currentPatchX, currentPatchY);
		patchList.append(linearPatch);
	} else {
		// Batch mode: parse specs
		frameList = parseFrameSpec(config.frameSpec);
		patchList = parseIndexSpec(config.patchSpec);

		if (frameList.isEmpty()) {
			LOG_WARNING() << "No valid frames in frame spec:" << config.frameSpec;
			return false;
		}
		if (patchList.isEmpty()) {
			LOG_WARNING() << "No valid patches in patch spec:" << config.patchSpec;
			return false;
		}

		// Validate frame and patch ranges
		for (int i = frameList.size() - 1; i >= 0; --i) {
			if (frameList[i] < 0 || frameList[i] >= totalFrames) {
				frameList.removeAt(i);
			}
		}
		for (int i = patchList.size() - 1; i >= 0; --i) {
			if (patchList[i] < 0 || patchList[i] >= totalPatches) {
				patchList.removeAt(i);
			}
		}

		if (frameList.isEmpty() || patchList.isEmpty()) {
			LOG_WARNING() << "No valid frames or patches after filtering";
			return false;
		}
	}

	// Build jobs: iterate frames, then patches within each frame
	for (int frameNr : qAsConst(frameList)) {
		for (int linearPatch : qAsConst(patchList)) {
			int patchX = linearPatch % gridCols;
			int patchY = linearPatch / gridCols;

			OptimizationJob job;
			job.frameNr = frameNr;
			job.patchX = patchX;
			job.patchY = patchY;

			// Extract input patch data (deep copy for thread safety)
			ImagePatch inputPatch = imageSession->getInputPatch(frameNr, patchX, patchY);
			if (!inputPatch.isValid()) {
				LOG_WARNING() << "Invalid input patch at frame" << frameNr
							  << "patch (" << patchX << "," << patchY << "), skipping";
				continue;
			}
			job.inputPatch = inputPatch.data.copy();

			// Extract ground truth patch (deep copy) if available and reference metric requested
			if (hasGroundTruth && config.useReferenceMetric) {
				ImagePatch gtPatch = imageSession->getGroundTruthPatch(frameNr, patchX, patchY);
				if (gtPatch.isValid()) {
					job.groundTruthPatch = gtPatch.data.copy();
				}
			}

			// Determine start coefficients
			int patchIdx = parameterTable->patchIndex(patchX, patchY);
			switch (config.startCoefficientSource) {
				case 0: {
					// Current stored coefficients
					QVector<double> coeffs = parameterTable->getCoefficients(frameNr, patchIdx);
					job.startCoefficients = coeffs.isEmpty()
						? QVector<double>(coeffCount, 0.0) : coeffs;
					break;
				}
				case 1: {
					// Fixed frame
					int sourceFrame = qBound(0, config.sourceParam, totalFrames - 1);
					QVector<double> coeffs = parameterTable->getCoefficients(sourceFrame, patchIdx);
					job.startCoefficients = coeffs.isEmpty()
						? QVector<double>(coeffCount, 0.0) : coeffs;
					break;
				}
				case 2: {
					// Offset from current frame
					int sourceFrame = qBound(0, frameNr + config.sourceParam, totalFrames - 1);
					QVector<double> coeffs = parameterTable->getCoefficients(sourceFrame, patchIdx);
					job.startCoefficients = coeffs.isEmpty()
						? QVector<double>(coeffCount, 0.0) : coeffs;
					break;
				}
				case 3: {
					// Random within bounds
					QVector<double> coeffs(coeffCount, 0.0);
					for (int c = 0; c < coeffCount; ++c) {
						double lo = (c < config.minBounds.size()) ? config.minBounds[c] : -0.3;
						double hi = (c < config.maxBounds.size()) ? config.maxBounds[c] : 0.3;
						coeffs[c] = lo + QRandomGenerator::global()->generateDouble() * (hi - lo);
					}
					job.startCoefficients = coeffs;
					break;
				}
				case 4:
					// All zeros
					job.startCoefficients = QVector<double>(coeffCount, 0.0);
					break;
				case 5:
					// Previous patch result — handled by worker at runtime;
					// fall back to stored coefficients for the first job
					if (config.jobs.isEmpty()) {
						QVector<double> coeffs = parameterTable->getCoefficients(frameNr, patchIdx);
						job.startCoefficients = coeffs.isEmpty()
							? QVector<double>(coeffCount, 0.0) : coeffs;
					} else {
						// Worker will override with previous job's best result
						job.startCoefficients = QVector<double>(coeffCount, 0.0);
					}
					break;
				case 6: {
					// From specific patch
					int sourcePatchIdx = config.sourceParam;
					QVector<double> coeffs = parameterTable->getCoefficients(frameNr, sourcePatchIdx);
					job.startCoefficients = coeffs.isEmpty()
						? QVector<double>(coeffCount, 0.0) : coeffs;
					break;
				}
				default:
					job.startCoefficients = QVector<double>(coeffCount, 0.0);
					break;
			}

			config.jobs.append(job);
		}
	}

	LOG_INFO() << "Built" << config.jobs.size() << "optimization jobs";
	return !config.jobs.isEmpty();
}
