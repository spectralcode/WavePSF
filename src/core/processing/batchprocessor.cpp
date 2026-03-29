#include "batchprocessor.h"
#include "volumetricprocessor.h"
#include "controller/imagesession.h"
#include "core/psf/psfmodule.h"
#include "core/psf/ipsfgenerator.h"
#include "data/wavefrontparametertable.h"
#include "utils/logging.h"
#include <QProgressDialog>
#include <QApplication>

BatchProcessor::BatchProcessor(QObject* parent)
	: QObject(parent)
{
}

bool BatchProcessor::executeBatchDeconvolution(
	ImageSession* imageSession,
	PSFModule* psfModule,
	WavefrontParameterTable* parameterTable,
	QWidget* parentWidget)
{
	// Delegate to volumetric batch if 3D algorithm is selected
	if (psfModule->is3DAlgorithm()) {
		return this->executeBatchVolumetricDeconvolution(
			imageSession, psfModule, parameterTable, parentWidget);
	}

	int frames = imageSession->getInputFrames();
	int cols = imageSession->getPatchGridCols();
	int rows = imageSession->getPatchGridRows();
	int totalSteps = frames * rows * cols;

	if (totalSteps == 0) {
		LOG_WARNING() << "Cannot run batch deconvolution: no patches to process";
		return false;
	}

	// Block PSFModule signals to prevent GUI flickering during batch
	bool oldBlockState = psfModule->blockSignals(true);

	// Create progress dialog
	QProgressDialog progressDialog("Initializing batch deconvolution...", "Cancel", 0, totalSteps, parentWidget);
	progressDialog.setWindowTitle("Batch Deconvolution");
	progressDialog.setWindowModality(Qt::ApplicationModal);
	progressDialog.setMinimumDuration(0);
	progressDialog.setValue(0);

	LOG_INFO() << "Starting batch deconvolution:" << frames << "frames," << cols << "x" << rows << "patches";

	bool supportsCoeffs = psfModule->getGenerator()->supportsCoefficients();
	int step = 0;
	bool cancelled = false;

	for (int frame = 0; frame < frames && !cancelled; frame++) {
		for (int y = 0; y < rows && !cancelled; y++) {
			for (int x = 0; x < cols && !cancelled; x++) {
				// Update progress label
				progressDialog.setLabelText(
					QString("Processing frame %1/%2, patch (%3,%4)...")
						.arg(frame + 1).arg(frames).arg(x).arg(y));

				int patchIdx = parameterTable->patchIndex(x, y);
				psfModule->setCurrentPatch(frame, patchIdx);

				if (supportsCoeffs) {
					QVector<double> coeffs = parameterTable->getCoefficients(frame, patchIdx);
					if (coeffs.isEmpty()) {
						step++;
						progressDialog.setValue(step);
						QApplication::processEvents();
						if (progressDialog.wasCanceled()) { cancelled = true; }
						continue;
					}
					psfModule->setAllCoefficients(coeffs);
				} else {
					psfModule->refreshPSF();
				}

				// Get input patch
				ImagePatch inputPatch = imageSession->getInputPatch(frame, x, y);
				if (!inputPatch.isValid()) {
					step++;
					progressDialog.setValue(step);
					QApplication::processEvents();
					if (progressDialog.wasCanceled()) { cancelled = true; }
					continue;
				}

				// Deconvolve
				af::array result = psfModule->deconvolve(inputPatch.data);
				if (!result.isempty()) {
					imageSession->setOutputPatch(frame, x, y, result);
				}

				step++;
				progressDialog.setValue(step);
				QApplication::processEvents();
				if (progressDialog.wasCanceled()) {
					cancelled = true;
				}
			}
		}
	}

	// Flush last frame to CPU
	imageSession->flushOutput();

	psfModule->blockSignals(oldBlockState);

	if (cancelled) {
		LOG_INFO() << "Batch deconvolution cancelled at step" << step << "of" << totalSteps;
	} else {
		LOG_INFO() << "Batch deconvolution completed:" << totalSteps << "patches processed";
	}

	return !cancelled;
}

bool BatchProcessor::executeBatchVolumetricDeconvolution(
	ImageSession* imageSession,
	PSFModule* psfModule,
	WavefrontParameterTable* parameterTable,
	QWidget* parentWidget)
{
	int frames = imageSession->getInputFrames();
	int cols = imageSession->getPatchGridCols();
	int rows = imageSession->getPatchGridRows();
	int totalPatches = cols * rows;

	if (totalPatches == 0 || frames == 0) {
		LOG_WARNING() << "Cannot run volumetric batch deconvolution: no patches or frames";
		return false;
	}

	// Block PSFModule signals to prevent GUI flickering during batch
	bool oldBlockState = psfModule->blockSignals(true);

	// Create progress dialog
	QProgressDialog progressDialog("Initializing volumetric batch deconvolution...", "Cancel", 0, totalPatches, parentWidget);
	progressDialog.setWindowTitle("Batch Deconvolution (3D)");
	progressDialog.setWindowModality(Qt::ApplicationModal);
	progressDialog.setMinimumDuration(0);
	progressDialog.setValue(0);

	// Connect iteration progress for per-patch RL tracking
	int patchStep = 0;
	QMetaObject::Connection iterConn = connect(psfModule, &PSFModule::deconvolutionIterationCompleted,
		[&](int curIter, int totalIter) {
			progressDialog.setLabelText(
				QString("Patch %1/%2: iteration %3/%4")
					.arg(patchStep + 1).arg(totalPatches).arg(curIter).arg(totalIter));
			QApplication::processEvents();
		});

	// Temporarily unblock signals so iteration progress comes through
	psfModule->blockSignals(false);

	LOG_INFO() << "Starting volumetric batch deconvolution:" << totalPatches << "patches," << frames << "frames";

	bool cancelled = false;

	for (int y = 0; y < rows && !cancelled; y++) {
		for (int x = 0; x < cols && !cancelled; x++) {
			int patchIdx = parameterTable->patchIndex(x, y);

			progressDialog.setLabelText(
				QString("Patch %1/%2: assembling subvolume...")
					.arg(patchStep + 1).arg(totalPatches));
			QApplication::processEvents();

			af::array subvolume = VolumetricProcessor::assembleSubvolume(imageSession, x, y);
			if (subvolume.isempty()) {
				LOG_WARNING() << "Skipping patch (" << x << "," << y << "): empty subvolume";
				patchStep++;
				progressDialog.setValue(patchStep);
				continue;
			}

			af::array psf3D = VolumetricProcessor::assemble3DPSF(
				psfModule, parameterTable, patchIdx, frames);
			if (psf3D.isempty()) {
				LOG_WARNING() << "Skipping patch (" << x << "," << y << "): no 3D PSF available";
				patchStep++;
				progressDialog.setValue(patchStep);
				continue;
			}

			LOG_INFO() << "Patch (" << x << "," << y << "): subvolume"
				   << subvolume.dims(0) << "x" << subvolume.dims(1) << "x" << subvolume.dims(2)
				   << "PSF" << psf3D.dims(0) << "x" << psf3D.dims(1) << "x" << psf3D.dims(2);

			af::array result = psfModule->deconvolve(subvolume, psf3D);
			if (!result.isempty()) {
				VolumetricProcessor::writeSubvolumeToOutput(imageSession, x, y, result);
			} else {
				LOG_WARNING() << "Patch (" << x << "," << y << "): deconvolution returned empty result";
			}

			// Free GPU memory between patches
			subvolume = af::array();
			psf3D = af::array();
			result = af::array();
			af::deviceGC();

			patchStep++;
			progressDialog.setValue(patchStep);
			QApplication::processEvents();
			if (progressDialog.wasCanceled()) {
				cancelled = true;
			}
		}
	}

	// Flush output to CPU
	imageSession->flushOutput();

	// Disconnect iteration progress and restore signal state
	disconnect(iterConn);
	psfModule->blockSignals(oldBlockState);

	if (cancelled) {
		LOG_INFO() << "Volumetric batch deconvolution cancelled at patch" << patchStep << "of" << totalPatches;
	} else {
		LOG_INFO() << "Volumetric batch deconvolution completed:" << totalPatches << "patches processed";
	}

	return !cancelled;
}
