#include "deconvolutionorchestrator.h"
#include "core/psf/psfmodule.h"
#include "core/psf/ipsfgenerator.h"
#include "controller/imagesession.h"
#include "controller/coefficientworkspace.h"
#include "data/wavefrontparametertable.h"
#include "core/processing/batchprocessor.h"
#include "core/processing/volumetricprocessor.h"
#include "utils/logging.h"
#include <QProgressDialog>
#include <QApplication>

DeconvolutionOrchestrator::DeconvolutionOrchestrator(
	PSFModule* psfModule,
	ImageSession* imageSession,
	CoefficientWorkspace* coefficientWorkspace,
	QObject* parent)
	: QObject(parent)
	, psfModule(psfModule)
	, imageSession(imageSession)
	, coefficientWorkspace(coefficientWorkspace)
	, batchProcessor(new BatchProcessor(this))
{
}

void DeconvolutionOrchestrator::runOnCurrentPatch()
{
	if (this->imageSession == nullptr || !this->imageSession->hasInputData()
		|| this->psfModule == nullptr) {
		return;
	}

	if (this->psfModule->is3DAlgorithm()) {
		this->runVolumetricOnCurrentPatch();
		return;
	}

	ImagePatch inputPatch = this->imageSession->getCurrentInputPatch();
	if (!inputPatch.isValid()) {
		LOG_WARNING() << "No valid input patch for deconvolution";
		return;
	}

	af::array result = this->psfModule->deconvolve(inputPatch.data);
	if (result.isempty()) {
		return;
	}

	this->imageSession->setCurrentOutputPatch(result);
}

bool DeconvolutionOrchestrator::runBatch()
{
	if (this->imageSession == nullptr || !this->imageSession->hasInputData()
		|| this->psfModule == nullptr || this->coefficientWorkspace->table() == nullptr) {
		LOG_WARNING() << "Cannot run batch deconvolution: missing input data or parameters";
		return false;
	}

	this->coefficientWorkspace->store();
	this->syncVoxelSize();

	this->batchProcessor->executeBatchDeconvolution(
		this->imageSession, this->psfModule, this->coefficientWorkspace->table());

	return true;
}

void DeconvolutionOrchestrator::cancel()
{
	if (this->psfModule != nullptr) {
		this->psfModule->requestDeconvolutionCancel();
	}
}

void DeconvolutionOrchestrator::runVolumetricOnCurrentPatch()
{
	if (this->coefficientWorkspace->table() == nullptr || this->imageSession == nullptr) {
		return;
	}

	int patchX = this->imageSession->getCurrentPatch().x();
	int patchY = this->imageSession->getCurrentPatch().y();
	int patchIdx = this->coefficientWorkspace->table()->patchIndex(patchX, patchY);
	int frames = this->imageSession->getInputFrames();

	af::array subvolume = VolumetricProcessor::assembleSubvolume(
		this->imageSession, patchX, patchY);
	if (subvolume.isempty()) {
		LOG_WARNING() << "3D deconv: empty subvolume for patch" << patchX << patchY;
		return;
	}
	LOG_INFO() << "3D deconv: subvolume" << subvolume.dims(0) << "x"
			   << subvolume.dims(1) << "x" << subvolume.dims(2);

	af::array psf3D = VolumetricProcessor::assemble3DPSF(
		this->psfModule, this->coefficientWorkspace->table(),
		patchIdx, frames);
	if (psf3D.isempty()) {
		LOG_WARNING() << "3D deconv: empty 3D PSF for patch" << patchX << patchY;
		return;
	}
	LOG_INFO() << "3D deconv: PSF" << psf3D.dims(0) << "x"
			   << psf3D.dims(1) << "x" << psf3D.dims(2);

	this->syncVoxelSize();

	// Show progress dialog for 3D RL iterations
	this->psfModule->resetDeconvolutionCancel();
	QProgressDialog progress(tr("Preparing 3D deconvolution..."), tr("Cancel"), 0, 0);
	progress.setWindowTitle(tr("3D Deconvolution"));
	progress.setWindowModality(Qt::ApplicationModal);
	progress.setMinimumDuration(0);
	progress.show();
	QApplication::processEvents();

	connect(&progress, &QProgressDialog::canceled,
			this, &DeconvolutionOrchestrator::cancel);

	QMetaObject::Connection iterConn = connect(this->psfModule,
		&PSFModule::deconvolutionIterationCompleted,
		[&progress](int curIter, int totalIter) {
			if (progress.maximum() != totalIter) {
				progress.setMaximum(totalIter);
			}
			progress.setValue(curIter);
			progress.setLabelText(
				QString("3D Richardson-Lucy: iteration %1 / %2")
					.arg(curIter).arg(totalIter));
			QApplication::processEvents();
		});

	af::array result = this->psfModule->deconvolve(subvolume, psf3D);

	disconnect(iterConn);
	progress.close();

	if (result.isempty()) {
		if (this->psfModule->wasDeconvolutionCancelled()) {
			LOG_INFO() << "3D deconv: cancelled by user";
		} else {
			LOG_WARNING() << "3D deconv: deconvolution returned empty result";
		}
		return;
	}
	LOG_INFO() << "3D deconv: result" << result.dims(0) << "x"
			   << result.dims(1) << "x" << result.dims(2);

	VolumetricProcessor::writeSubvolumeToOutput(
		this->imageSession, patchX, patchY, result);

	// Trigger viewer refresh via the same signal chain as 2D:
	// setCurrentOutputPatch → outputPatchUpdated → deconvolutionCompleted
	this->imageSession->setCurrentOutputPatch(
		result(af::span, af::span, this->imageSession->getCurrentFrame()));
}

void DeconvolutionOrchestrator::syncVoxelSize()
{
	if (this->psfModule == nullptr || !this->psfModule->is3DAlgorithm()) {
		return;
	}
	QVariantMap genSettings = this->psfModule->getGenerator()->serializeSettings();
	float xyStep = genSettings.value("xy_step_nm", 1.0).toFloat();
	float zStep = genSettings.value("z_step_nm", 1.0).toFloat();
	if (xyStep > 0.0f && zStep > 0.0f) {
		this->psfModule->setDeconvolutionVoxelSize(xyStep, xyStep, zStep);
	}
}
