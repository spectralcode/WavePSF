#include "batchprocessor.h"
#include "controller/imagesession.h"
#include "core/psf/psfmodule.h"
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

	int step = 0;
	bool cancelled = false;

	for (int frame = 0; frame < frames && !cancelled; frame++) {
		for (int y = 0; y < rows && !cancelled; y++) {
			for (int x = 0; x < cols && !cancelled; x++) {
				// Update progress label
				progressDialog.setLabelText(
					QString("Processing frame %1/%2, patch (%3,%4)...")
						.arg(frame + 1).arg(frames).arg(x).arg(y));

				// Load coefficients from parameter table
				int patchIdx = parameterTable->patchIndex(x, y);
				QVector<double> coeffs = parameterTable->getCoefficients(frame, patchIdx);
				if (coeffs.isEmpty()) {
					step++;
					progressDialog.setValue(step);
					QApplication::processEvents();
					if (progressDialog.wasCanceled()) { cancelled = true; }
					continue;
				}

				// Set coefficients (signals blocked, no GUI update)
				psfModule->setAllCoefficients(coeffs);

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

	// Restore signal state
	psfModule->blockSignals(oldBlockState);

	if (cancelled) {
		LOG_INFO() << "Batch deconvolution cancelled at step" << step << "of" << totalSteps;
	} else {
		LOG_INFO() << "Batch deconvolution completed:" << totalSteps << "patches processed";
	}

	return !cancelled;
}
