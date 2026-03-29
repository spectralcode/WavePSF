#include "applicationcontroller.h"
#include "imagesession.h"
#include "data/inputdatareader.h"
#include "data/imagedata.h"
#include "data/wavefrontparametertable.h"
#include "core/psf/psfmodule.h"
#include "core/psf/ipsfgenerator.h"
#include "core/psf/psffilemanager.h"
#include "core/optimization/optimizationjobbuilder.h"
#include "core/processing/batchprocessor.h"
#include "core/processing/volumetricprocessor.h"
#include "core/interpolation/interpolationorchestrator.h"
#include "core/psf/psfgridgenerator.h"
#include "utils/logging.h"
#include "utils/settingsfilemanager.h"
#include "utils/afdevicemanager.h"
#include <QProgressDialog>
#include <QApplication>

ApplicationController::ApplicationController(AFDeviceManager* afDeviceManager, QObject* parent)
	: QObject(parent), afDeviceManager(afDeviceManager), imageSession(nullptr), inputDataReader(nullptr), psfModule(nullptr)
	, parameterTable(nullptr), deconvolutionLiveMode(false)
	, optimizationThread(nullptr), optimizationWorker(nullptr)
	, optimizationLivePreview(false), optimizationLivePreviewInterval(10), optimizationProgressCounter(0)
	, suppressLiveDeconv(false)
{
	this->initializeComponents();
	this->connectSessionSignals();
	this->connectPSFModuleSignals();
	this->connectDeconvolutionSignals();
	this->initializeOptimizationThread();

	// React to device changes from AFDeviceManager (each component clears its own caches via self-connection)
	connect(this->afDeviceManager, &AFDeviceManager::aboutToChangeDevice, this, [this]() {
		this->psfFileManager->clearOverrides();
	});
	connect(this->afDeviceManager, &AFDeviceManager::deviceChanged, this, [this]() {
		if (this->hasInputData())
			this->psfModule->applyPSFSettings(this->psfModule->getPSFSettings());
	});

	qRegisterMetaType<OptimizationConfig>("OptimizationConfig");
	qRegisterMetaType<OptimizationProgress>("OptimizationProgress");
	qRegisterMetaType<OptimizationResult>("OptimizationResult");
	qRegisterMetaType<InterpolationResult>("InterpolationResult");
}

ApplicationController::~ApplicationController()
{
	if (this->optimizationThread) {
		this->optimizationWorker->requestCancel();
		this->optimizationThread->quit();
		this->optimizationThread->wait();
		this->optimizationThread = nullptr;
		this->optimizationWorker = nullptr;  // deleted by QThread::finished → deleteLater
	}
}

bool ApplicationController::openInputFile(const QString& filePath)
{
	return this->loadFileToSession(filePath, false);
}

bool ApplicationController::openGroundTruthFile(const QString& filePath)
{
	return this->loadFileToSession(filePath, true);
}

void ApplicationController::setCurrentFrame(int frame)
{
	if (this->imageSession != nullptr) {
		this->storeCurrentCoefficients();

		// Suppress live deconvolution during frame change for 3D algorithms.
		// 3D processes all frames at once, so frame change should not re-trigger.
		bool suppress3D = this->deconvolutionLiveMode &&
						  this->psfModule != nullptr &&
						  this->psfModule->is3DAlgorithm();
		if (suppress3D) this->suppressLiveDeconv = true;

		this->imageSession->setCurrentFrame(frame);
		this->loadCoefficientsForCurrentPatch();

		if (suppress3D) this->suppressLiveDeconv = false;
	}
}

void ApplicationController::setCurrentPatch(int x, int y)
{
	if (this->imageSession != nullptr) {
		this->storeCurrentCoefficients();
		this->imageSession->setCurrentPatch(x, y);
		this->loadCoefficientsForCurrentPatch();
	}
}

void ApplicationController::navigatePatch(int dx, int dy)
{
	if (!this->hasInputData()) return;
	int x = qBound(0, this->getCurrentPatchX() + dx, this->getPatchGridCols() - 1);
	int y = qBound(0, this->getCurrentPatchY() + dy, this->getPatchGridRows() - 1);
	this->setCurrentPatch(x, y);
}

void ApplicationController::configurePatchGrid(int cols, int rows, int borderExtension)
{
	if (this->imageSession != nullptr) {
		this->imageSession->configurePatchGrid(cols, rows, borderExtension);

		// Invalidate cached parameter tables (patch dimensions changed)
		qDeleteAll(this->cachedParameterTables);
		this->cachedParameterTables.clear();

		this->resizeParameterTable();
	}
}

void ApplicationController::setPSFCoefficient(int id, double value)
{
	// Clear external PSF override for current patch (user is manually adjusting coefficients)
	if (this->hasInputData() && this->parameterTable != nullptr) {
		int frame = this->getCurrentFrame();
		int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
		this->psfFileManager->removeOverride(frame, patchIdx);
	}

	if (this->psfModule != nullptr) {
		this->psfModule->setCoefficient(id, value);
	}
	this->storeCurrentCoefficients();
}

void ApplicationController::resetPSFCoefficients()
{
	// Clear external PSF override for current patch
	if (this->hasInputData() && this->parameterTable != nullptr) {
		int frame = this->getCurrentFrame();
		int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
		this->psfFileManager->removeOverride(frame, patchIdx);
	}

	if (this->psfModule != nullptr) {
		this->psfModule->resetCoefficients();
	}
	this->storeCurrentCoefficients();
	if (this->psfModule != nullptr) {
		emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	}
}

void ApplicationController::copyCoefficients()
{
	this->storeCurrentCoefficients();
	if (this->psfModule != nullptr) {
		this->coefficientClipboard = this->psfModule->getAllCoefficients();
	}
}

void ApplicationController::pasteCoefficients()
{
	if (this->coefficientClipboard.isEmpty() || this->psfModule == nullptr) return;

	// Save undo state before overwriting
	this->undoCoefficients = this->psfModule->getAllCoefficients();
	this->undoFrame = this->getCurrentFrame();
	this->undoPatchX = this->getCurrentPatchX();
	this->undoPatchY = this->getCurrentPatchY();

	// Clear external PSF override for current patch
	if (this->hasInputData() && this->parameterTable != nullptr) {
		int frame = this->getCurrentFrame();
		int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
		this->psfFileManager->removeOverride(frame, patchIdx);
	}

	this->psfModule->setAllCoefficients(this->coefficientClipboard);
	this->storeCurrentCoefficients();
	emit coefficientsLoaded(this->coefficientClipboard);
}

void ApplicationController::undoPasteCoefficients()
{
	if (this->undoCoefficients.isEmpty() || this->psfModule == nullptr || this->parameterTable == nullptr) return;

	// Write undo coefficients back to the parameter table
	int patchIdx = this->parameterTable->patchIndex(this->undoPatchX, this->undoPatchY);
	this->parameterTable->setCoefficients(this->undoFrame, patchIdx, this->undoCoefficients);

	// If we're still on the same patch, update sliders + PSF
	if (this->getCurrentFrame() == this->undoFrame &&
	    this->getCurrentPatchX() == this->undoPatchX &&
	    this->getCurrentPatchY() == this->undoPatchY) {
		this->psfModule->setAllCoefficients(this->undoCoefficients);
		emit coefficientsLoaded(this->undoCoefficients);
	}

	this->undoCoefficients.clear();
}

void ApplicationController::resetAllCoefficients()
{
	if (this->parameterTable == nullptr) return;
	this->parameterTable->resetAllCoefficients();
	this->loadCoefficientsForCurrentPatch();
}

void ApplicationController::clearExternalPSFs()
{
	if (this->psfFileManager != nullptr) {
		this->psfFileManager->clearOverrides();
		this->psfFileManager->setUseCustomPSFFolder(false);
		emit customPSFFolderDisabled();
	}
	if (this->psfModule != nullptr) {
		this->psfModule->clearExternalPSF();
	}
	this->loadCoefficientsForCurrentPatch();
}

void ApplicationController::saveParametersToFile(const QString& filePath)
{
	this->storeCurrentCoefficients();
	if (this->parameterTable != nullptr) {
		this->parameterTable->saveToFile(filePath);
	}
}

void ApplicationController::loadParametersFromFile(const QString& filePath)
{
	if (this->parameterTable != nullptr) {
		if (this->parameterTable->loadFromFile(filePath)) {
			this->loadCoefficientsForCurrentPatch();
			emit parametersLoaded();
		}
	}
}

void ApplicationController::applyPSFSettings(const PSFSettings& settings)
{
	if (this->psfModule != nullptr) {
		this->psfModule->applyPSFSettings(settings);
		// Resize parameter table if coefficient count changed
		this->resizeParameterTable();
		// Re-send current coefficients so UI sliders reflect actual values
		// (parameterDescriptorsChanged rebuilds sliders to defaults)
		emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	}
	emit psfSettingsUpdated(this->psfModule->getPSFSettings());
}

void ApplicationController::switchGenerator(const QString& typeName)
{
	if (this->psfModule == nullptr) {
		return;
	}
	QString oldTypeName = this->psfModule->getGeneratorTypeName();
	if (oldTypeName == typeName) {
		return;
	}

	// Save current coefficients and parameter table for the outgoing generator type
	this->storeCurrentCoefficients();
	if (this->parameterTable != nullptr) {
		this->cachedParameterTables[oldTypeName] = this->parameterTable;
		this->parameterTable = nullptr;
	}

	// Switch generator (PSFModule handles caching generator settings internally)
	this->psfModule->switchGenerator(typeName);

	// Restore or create parameter table for the new type
	if (this->cachedParameterTables.contains(typeName)) {
		this->parameterTable = this->cachedParameterTables.take(typeName);
	} else {
		this->parameterTable = new WavefrontParameterTable(this);
		this->resizeParameterTable();
	}

	// Sync z-planes for 3D generators
	this->syncNumZPlanesWithInput();

	// Load coefficients for current patch from the restored/fresh table
	this->loadCoefficientsForCurrentPatch();

	emit psfSettingsUpdated(this->psfModule->getPSFSettings());
}

void ApplicationController::applyInlineSettings(const QVariantMap& settings)
{
	if (this->psfModule != nullptr) {
		this->psfModule->applyInlineSettings(settings);
	}
}

// Deconvolution settings forwarding
void ApplicationController::setDeconvolutionAlgorithm(int algorithm)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setDeconvolutionAlgorithm(algorithm);
	}
}

void ApplicationController::setDeconvolutionIterations(int iterations)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setDeconvolutionIterations(iterations);
	}
}

void ApplicationController::setDeconvolutionRelaxationFactor(float factor)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setDeconvolutionRelaxationFactor(factor);
	}
}

void ApplicationController::setDeconvolutionRegularizationFactor(float factor)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setDeconvolutionRegularizationFactor(factor);
	}
}

void ApplicationController::setDeconvolutionNoiseToSignalFactor(float factor)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setDeconvolutionNoiseToSignalFactor(factor);
	}
}

void ApplicationController::setVolumePaddingMode(int mode)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setVolumePaddingMode(mode);
	}
}

void ApplicationController::setAccelerationMode(int mode)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setAccelerationMode(mode);
	}
}

void ApplicationController::setDeconvolutionLiveMode(bool enabled)
{
	this->deconvolutionLiveMode = enabled;
	if (enabled) {
		this->suppressLiveDeconv = false;
		this->runDeconvolutionOnCurrentPatch();
	}
}

void ApplicationController::requestDeconvolution()
{
	this->suppressLiveDeconv = false;
	this->runDeconvolutionOnCurrentPatch();
}


void ApplicationController::requestBatchDeconvolution()
{
	if (!this->hasInputData() || this->psfModule == nullptr || this->parameterTable == nullptr) {
		LOG_WARNING() << "Cannot run batch deconvolution: missing input data or parameters";
		return;
	}

	this->storeCurrentCoefficients();
	this->suppressLiveDeconv = true;

	this->batchProcessor->executeBatchDeconvolution(
		this->imageSession, this->psfModule, this->parameterTable, this->psfFileManager);

	this->suppressLiveDeconv = false;
	this->loadCoefficientsForCurrentPatch();
	emit batchDeconvolutionCompleted();
}

void ApplicationController::runDeconvolutionOnCurrentPatch()
{
	if (!this->hasInputData() || this->psfModule == nullptr) {
		return;
	}

	if (this->psfModule->is3DAlgorithm()) {
		this->runVolumetricDeconvolutionOnCurrentPatch();
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

void ApplicationController::runVolumetricDeconvolutionOnCurrentPatch()
{
	if (this->parameterTable == nullptr || this->imageSession == nullptr) {
		return;
	}

	int patchX = this->getCurrentPatchX();
	int patchY = this->getCurrentPatchY();
	int patchIdx = this->parameterTable->patchIndex(patchX, patchY);
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
		this->psfModule, this->parameterTable, this->psfFileManager,
		patchIdx, frames);
	if (psf3D.isempty()) {
		LOG_WARNING() << "3D deconv: empty 3D PSF for patch" << patchX << patchY;
		return;
	}
	LOG_INFO() << "3D deconv: PSF" << psf3D.dims(0) << "x"
			   << psf3D.dims(1) << "x" << psf3D.dims(2);

	// Show progress dialog for 3D RL iterations
	QProgressDialog progress("Preparing 3D deconvolution...", QString(), 0, 0);
	progress.setWindowTitle("3D Deconvolution");
	progress.setWindowModality(Qt::ApplicationModal);
	progress.setMinimumDuration(0);
	progress.show();
	QApplication::processEvents();

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
		LOG_WARNING() << "3D deconv: deconvolution returned empty result";
		return;
	}
	LOG_INFO() << "3D deconv: result" << result.dims(0) << "x"
			   << result.dims(1) << "x" << result.dims(2);

	VolumetricProcessor::writeSubvolumeToOutput(
		this->imageSession, patchX, patchY, result);

	// Trigger viewer refresh via the same signal chain as 2D:
	// setCurrentOutputPatch → outputPatchUpdated → deconvolutionCompleted
	this->imageSession->setCurrentOutputPatch(
		result(af::span, af::span, this->getCurrentFrame()));
}

void ApplicationController::handlePSFUpdatedForDeconvolution(af::array psf)
{
	Q_UNUSED(psf);
	if (this->deconvolutionLiveMode && !this->suppressLiveDeconv) {
		this->runDeconvolutionOnCurrentPatch();
	}

	// Auto-save PSF if enabled
	if (this->hasInputData() && this->parameterTable != nullptr) {
		int frame = this->getCurrentFrame();
		int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
		this->psfFileManager->autoSaveIfEnabled(frame, patchIdx, this->psfModule);
	}
}

void ApplicationController::handleDeconvolutionSettingsChanged()
{
	if (this->deconvolutionLiveMode) {
		this->runDeconvolutionOnCurrentPatch();
	}
}

// Session information access
bool ApplicationController::hasInputData() const
{
	return this->imageSession != nullptr && this->imageSession->hasInputData();
}

bool ApplicationController::hasOutputData() const
{
	return this->imageSession != nullptr && this->imageSession->hasOutputData();
}

int ApplicationController::getCurrentFrame() const
{
	return this->imageSession != nullptr ? this->imageSession->getCurrentFrame() : -1;
}

int ApplicationController::getCurrentPatchX() const
{
	return this->imageSession != nullptr ? this->imageSession->getCurrentPatch().x() : -1;
}

int ApplicationController::getCurrentPatchY() const
{
	return this->imageSession != nullptr ? this->imageSession->getCurrentPatch().y() : -1;
}

int ApplicationController::getPatchGridCols() const
{
	return this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : 1;
}

int ApplicationController::getPatchGridRows() const
{
	return this->imageSession != nullptr ? this->imageSession->getPatchGridRows() : 1;
}

int ApplicationController::getInputWidth() const
{
	return this->imageSession != nullptr ? this->imageSession->getInputWidth() : 0;
}

int ApplicationController::getInputHeight() const
{
	return this->imageSession != nullptr ? this->imageSession->getInputHeight() : 0;
}

int ApplicationController::getInputFrames() const
{
	return this->imageSession != nullptr ? this->imageSession->getInputFrames() : 0;
}

void ApplicationController::broadcastCurrentState()
{
	if (this->imageSession != nullptr) {
		// Emit current ImageSession
		emit imageSessionChanged(this->imageSession);

		// Emit current frame
		emit frameChanged(this->imageSession->getCurrentFrame());

		// Emit current patch
		QPoint currentPatch = this->imageSession->getCurrentPatch();
		emit patchChanged(currentPatch.x(), currentPatch.y());

		// Emit patch grid configuration (business logic owns this)
		emit patchGridConfigured(
			this->imageSession->getPatchGridCols(),
			this->imageSession->getPatchGridRows(),
			this->imageSession->getPatchBorderExtension()
		);

		LOG_DEBUG() << "Broadcast current state: frame=" << this->imageSession->getCurrentFrame()
					<< ", patch=(" << currentPatch.x() << "," << currentPatch.y() << ")"
					<< ", grid=" << this->imageSession->getPatchGridCols() << "x" << this->imageSession->getPatchGridRows();
	}

	// Broadcast PSF parameter descriptors and settings
	if (this->psfModule != nullptr) {
		emit psfParameterDescriptorsChanged(this->psfModule->getParameterDescriptors());
		emit psfSettingsUpdated(this->psfModule->getPSFSettings());
	}
}

void ApplicationController::requestOpenInputFile(const QString& filePath)
{
	if (this->openInputFile(filePath)) {
		emit inputFileLoaded(filePath);
	} else {
		emit fileLoadError(filePath, "Failed to load input file");
	}
}

void ApplicationController::requestOpenGroundTruthFile(const QString& filePath)
{
	if (this->openGroundTruthFile(filePath)) {
		emit groundTruthFileLoaded(filePath);
	} else {
		emit fileLoadError(filePath, "Failed to load ground truth file");
	}
}

void ApplicationController::handleInputDataChanged()
{
	// Clear per-patch PSF overrides (new file loaded)
	this->psfFileManager->clearOverrides();

	// Invalidate cached parameter tables (dimensions no longer valid)
	qDeleteAll(this->cachedParameterTables);
	this->cachedParameterTables.clear();

	// Emit ImageSession changed so viewers can update their data connections
	emit imageSessionChanged(this->imageSession);

	// Resize parameter table to match new data dimensions
	this->resizeParameterTable();

	this->syncNumZPlanesWithInput();

	// Reset psfModule and UI sliders to the new dataset's patch (0,0) values,
	// preventing stale coefficients from the previous file's active patch from
	// being written back on the next storeCurrentCoefficients() call.
	this->loadCoefficientsForCurrentPatch();
}

void ApplicationController::handleOutputDataChanged()
{
	// Emit ImageSession changed so viewers can update their data connections
	emit imageSessionChanged(this->imageSession);
}

void ApplicationController::handleGroundTruthDataChanged()
{
	// Emit ImageSession changed so viewers can update their data connections
	emit imageSessionChanged(this->imageSession);
}

void ApplicationController::initializeComponents()
{
	// Create core components with Qt ownership
	this->imageSession = new ImageSession(this->afDeviceManager, this);
	this->inputDataReader = new InputDataReader(this);
	this->psfModule = new PSFModule(this->afDeviceManager, this);
	this->parameterTable = new WavefrontParameterTable(this);
	this->interpolationOrchestrator = new InterpolationOrchestrator(this);
	this->psfFileManager = new PSFFileManager(this);
	this->batchProcessor = new BatchProcessor(this);
	this->psfGridGenerator = new PSFGridGenerator(this);
}

void ApplicationController::connectSessionSignals()
{
	if (this->imageSession != nullptr) {
		// Direct signal-to-signal connections for simple forwarding
		connect(this->imageSession, &ImageSession::frameChanged,
				this, &ApplicationController::frameChanged);
		connect(this->imageSession, &ImageSession::patchChanged,
				this, &ApplicationController::patchChanged);
		connect(this->imageSession, &ImageSession::patchGridConfigured,
				this, &ApplicationController::patchGridConfigured);

		// Data change signals need transformation to imageSessionChanged
		connect(this->imageSession, &ImageSession::inputDataChanged,
				this, &ApplicationController::handleInputDataChanged);
		connect(this->imageSession, &ImageSession::outputDataChanged,
				this, &ApplicationController::handleOutputDataChanged);
		connect(this->imageSession, &ImageSession::groundTruthDataChanged,
				this, &ApplicationController::handleGroundTruthDataChanged);
	}
}

void ApplicationController::connectPSFModuleSignals()
{
	if (this->psfModule != nullptr) {
		connect(this->psfModule, &PSFModule::wavefrontUpdated,
				this, &ApplicationController::psfWavefrontUpdated);
		connect(this->psfModule, &PSFModule::psfUpdated, this, [this](af::array psf) {
			emit this->psfUpdated(psf);
			emit this->psfUpdatedForPatch(psf, this->getCurrentPatchX(), this->getCurrentPatchY());
		});
		connect(this->psfModule, &PSFModule::parameterDescriptorsChanged,
				this, &ApplicationController::psfParameterDescriptorsChanged);

		connect(this->psfModule, &PSFModule::generatorChanged,
				this, &ApplicationController::psfModeChanged);

		// When Noll indices change, parameter table must be cleared and resized
		connect(this->psfModule, &PSFModule::nollIndicesChanged, this, [this]() {
			if (this->parameterTable != nullptr) {
				this->parameterTable->clear();
				this->resizeParameterTable();
			}
		});
	}
}

void ApplicationController::connectDeconvolutionSignals()
{
	if (this->psfModule != nullptr) {
		// When PSF changes and live mode is on, re-deconvolve
		connect(this->psfModule, &PSFModule::psfUpdated,
				this, &ApplicationController::handlePSFUpdatedForDeconvolution);

		// When deconvolution settings change and live mode is on, re-deconvolve
		connect(this->psfModule, &PSFModule::deconvolutionSettingsChanged,
				this, &ApplicationController::handleDeconvolutionSettingsChanged);
	}

	if (this->imageSession != nullptr) {
		// Note: deconvolution on patch/frame change is triggered via
		// loadCoefficientsForCurrentPatch() → setAllCoefficients()/setExternalPSF()
		// → psfUpdated → handlePSFUpdatedForDeconvolution(), so no separate
		// patchChanged/frameChanged → deconvolve connections are needed.

		// Forward outputPatchUpdated to deconvolutionCompleted
		connect(this->imageSession, &ImageSession::outputPatchUpdated,
				this, &ApplicationController::deconvolutionCompleted);
	}
}

int ApplicationController::coefficientFrame() const
{
	// In 3D generator mode, coefficients are frame-independent (shared wavefront)
	if (this->psfModule != nullptr && this->psfModule->getGenerator()->is3D()) {
		return 0;
	}
	return this->getCurrentFrame();
}

void ApplicationController::storeCurrentCoefficients()
{
	if (this->parameterTable == nullptr || this->psfModule == nullptr || this->imageSession == nullptr) {
		return;
	}
	if (!this->hasInputData()) {
		return;
	}
	int frame = this->coefficientFrame();
	int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
	this->parameterTable->setCoefficients(frame, patchIdx, this->psfModule->getAllCoefficients());
}

void ApplicationController::loadCoefficientsForCurrentPatch()
{
	if (this->parameterTable == nullptr || this->psfModule == nullptr || this->imageSession == nullptr) {
		return;
	}
	if (!this->hasInputData()) {
		return;
	}

	int frame = this->coefficientFrame();
	int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());

	// Check per-patch external PSF override (from one-shot "Load PSF from File")
	if (this->psfFileManager->hasOverride(frame, patchIdx)) {
		this->psfModule->setExternalPSF(this->psfFileManager->getOverride(frame, patchIdx));
		return;
	}

	// Custom PSF folder mode: load PSF from file instead of computing from coefficients
	if (this->psfFileManager->isCustomFolderMode()) {
		af::array psf = this->psfFileManager->loadPSFFromFolder(frame, patchIdx);
		if (!psf.isempty()) {
			this->psfModule->setExternalPSF(psf);
			this->psfFileManager->storeOverride(frame, patchIdx, psf);
		}
		return;
	}

	QVector<double> coeffs = this->parameterTable->getCoefficients(frame, patchIdx);
	if (!coeffs.isEmpty()) {
		this->psfModule->setAllCoefficients(coeffs);
		emit coefficientsLoaded(coeffs);
	}
}

void ApplicationController::syncNumZPlanesWithInput()
{
	if (this->psfModule == nullptr ||
		!this->psfModule->getGenerator()->is3D() ||
		!this->hasInputData()) {
		return;
	}
	int inputFrames = this->getInputFrames();
	this->psfModule->setNumOutputPlanes(inputFrames);
	emit psfSettingsUpdated(this->psfModule->getPSFSettings());
}

void ApplicationController::resizeParameterTable()
{
	if (this->parameterTable == nullptr || this->psfModule == nullptr || this->imageSession == nullptr) {
		return;
	}
	if (!this->hasInputData()) {
		return;
	}
	int frames = this->imageSession->getInputFrames();
	int cols = this->imageSession->getPatchGridCols();
	int rows = this->imageSession->getPatchGridRows();
	int coeffs = this->psfModule->getAllCoefficients().size();

	// Only resize (and zero-fill) when dimensions actually changed
	if (frames != this->parameterTable->getNumberOfFrames()
		|| cols != this->parameterTable->getNumberOfPatchesInX()
		|| rows != this->parameterTable->getNumberOfPatchesInY()
		|| coeffs != this->parameterTable->getCoefficientsPerPatch()) {
		this->parameterTable->resize(frames, cols, rows, coeffs);
	}
}

bool ApplicationController::loadFileToSession(const QString& filePath, bool isGroundTruth)
{
	if (this->inputDataReader == nullptr || this->imageSession == nullptr) {
		LOG_ERROR() << "Components not initialized";
		return false;
	}

	try {
		// Load the file
		LOG_INFO() << tr("Attempting to load file: ") << filePath;
		ImageData* imageData = this->inputDataReader->loadFile(filePath);
		if (imageData == nullptr) {
			LOG_WARNING() << "Failed to load image data from file:" << filePath;
			return false;
		}

		// Set data in session
		if (isGroundTruth) {
			this->imageSession->setGroundTruthData(imageData);
		} else {
			this->imageSession->setInputData(imageData);
			LOG_INFO() << "Input file loaded successfully:" << filePath;
		}

		return true;

	} catch (const QString& error) {
		LOG_WARNING() << "Exception during file loading:" << error << "file:" << filePath;
		return false;
	} catch (...) {
		LOG_WARNING() << "Unknown exception during file loading:" << filePath;
		return false;
	}
}

// --- Optimization ---

void ApplicationController::initializeOptimizationThread()
{
	this->optimizationThread = new QThread(this);
	this->optimizationWorker = new OptimizationWorker();
	this->optimizationWorker->moveToThread(this->optimizationThread);

	connect(this->optimizationThread, &QThread::finished,
			this->optimizationWorker, &QObject::deleteLater);

	// Controller → Worker (queued)
	connect(this, &ApplicationController::runOptimizationOnWorker,
			this->optimizationWorker, &OptimizationWorker::runOptimization,
			Qt::QueuedConnection);

	// Worker → Controller (queued, routed through slot for live preview)
	connect(this->optimizationWorker, &OptimizationWorker::progressUpdated,
			this, &ApplicationController::handleOptimizationProgress,
			Qt::QueuedConnection);
	connect(this->optimizationWorker, &OptimizationWorker::optimizationFinished,
			this, &ApplicationController::handleOptimizationFinished,
			Qt::QueuedConnection);

	this->optimizationThread->start();
}

void ApplicationController::startOptimization(const OptimizationConfig& uiConfig)
{
	if (!this->hasInputData() || this->psfModule == nullptr) {
		LOG_WARNING() << "Cannot start optimization: no input data or PSF module";
		return;
	}

	// Store current coefficients before starting
	this->storeCurrentCoefficients();

	// Copy UI config and fill in controller-owned data
	OptimizationConfig config = uiConfig;

	// ArrayFire backend + device (worker thread needs explicit selection; both are per-thread)
	config.afBackend = this->afDeviceManager->getActiveBackendId();
	config.afDeviceId = this->afDeviceManager->getActiveDeviceId();

	// PSF settings
	config.psfSettings = this->psfModule->getPSFSettings();

	// Deconvolution settings from current PSFModule/Deconvolver
	// (read via PSFModule which owns the Deconvolver)
	// These are stored in the config struct with defaults that match the UI

	// Per-coefficient bounds from parameter descriptors
	QVector<WavefrontParameter> descriptors = this->psfModule->getParameterDescriptors();
	config.minBounds.resize(descriptors.size());
	config.maxBounds.resize(descriptors.size());
	for (int i = 0; i < descriptors.size(); ++i) {
		config.minBounds[i] = descriptors[i].minValue;
		config.maxBounds[i] = descriptors[i].maxValue;
	}

	// Build jobs
	if (!OptimizationJobBuilder::buildJobs(config, this->imageSession, this->psfModule,
			this->parameterTable, this->getCurrentFrame(), this->getCurrentPatchX(), this->getCurrentPatchY())) {
		LOG_WARNING() << "Failed to build optimization jobs";
		return;
	}

	LOG_INFO() << "Starting optimization with" << config.jobs.size() << "jobs";

	// Store live preview settings
	this->optimizationLivePreview = config.livePreview;
	this->optimizationLivePreviewInterval = config.livePreviewInterval;
	this->optimizationProgressCounter = 0;
	this->progressUpdateTimer.start();

	// Worker resets cancelRequested to 0 at the start of runOptimization()
	emit optimizationStarted();
	emit runOptimizationOnWorker(config);
}

void ApplicationController::cancelOptimization()
{
	if (this->optimizationWorker != nullptr) {
		this->optimizationWorker->requestCancel();
		LOG_INFO() << "Optimization cancellation requested";
	}
}

void ApplicationController::updateOptimizationLivePreview(bool enabled, int interval)
{
	this->optimizationLivePreview = enabled;
	this->optimizationLivePreviewInterval = interval;
	if (enabled) {
		this->optimizationProgressCounter = 0;
		this->progressUpdateTimer.restart();
	}
}

void ApplicationController::updateOptimizationAlgorithmParameters(const QVariantMap& params)
{
	if (this->optimizationWorker != nullptr) {
		this->optimizationWorker->updateLiveAlgorithmParameters(params);
	}
}

void ApplicationController::handleOptimizationProgress(const OptimizationProgress& progress)
{
	// Always forward to GUI (data accumulation + label updates are cheap;
	// the widget throttles the expensive replot() call itself)
	emit optimizationProgressUpdated(progress);

	// Live preview: throttle GPU work (setAllCoefficients + deconvolution)
	if (this->optimizationLivePreview) {
		this->optimizationProgressCounter++;
		if (this->optimizationProgressCounter >= this->optimizationLivePreviewInterval &&
			this->progressUpdateTimer.elapsed() >= 200) {
			this->optimizationProgressCounter = 0;
			this->progressUpdateTimer.restart();

			if (!progress.currentCoefficients.isEmpty()) {
				// Suppress live-deconv trigger from psfUpdated signal to avoid
				// double deconvolution (we call runDeconvolution explicitly below)
				this->suppressLiveDeconv = true;
				this->psfModule->setAllCoefficients(progress.currentCoefficients);
				emit coefficientsLoaded(progress.currentCoefficients);
				this->suppressLiveDeconv = false;
				// Navigate viewer to current job's patch so the live preview reflects it
				this->imageSession->setCurrentFrame(progress.currentFrameNr);
				this->imageSession->setCurrentPatch(progress.currentPatchX, progress.currentPatchY);
				this->runDeconvolutionOnCurrentPatch();
			}
		}
	}
}

void ApplicationController::handleOptimizationFinished(const OptimizationResult& result)
{
	LOG_INFO() << "Optimization finished:" << result.jobResults.size()
			   << "jobs," << result.totalOuterIterations << "iterations"
			   << (result.wasCancelled ? "(cancelled)" : "");

	// Store coefficients and write deconvolved output for every job
	for (const OptimizationJobResult& jobResult : result.jobResults) {
		int patchIdx = this->parameterTable->patchIndex(jobResult.patchX, jobResult.patchY);
		this->parameterTable->setCoefficients(jobResult.frameNr, patchIdx, jobResult.bestCoefficients);

		this->suppressLiveDeconv = true;
		this->psfModule->setAllCoefficients(jobResult.bestCoefficients);
		this->suppressLiveDeconv = false;
		this->imageSession->setCurrentFrame(jobResult.frameNr);
		this->imageSession->setCurrentPatch(jobResult.patchX, jobResult.patchY);
		this->runDeconvolutionOnCurrentPatch();
	}

	// Emit the last job's coefficients so the UI reflects the final state
	if (!result.jobResults.isEmpty()) {
		const OptimizationJobResult& lastJob = result.jobResults.last();
		emit coefficientsLoaded(lastJob.bestCoefficients);
	}

	emit optimizationFinished(result);
}

// --- File Output ---

void ApplicationController::saveOutputToFile(const QString& filePath)
{
	if (this->imageSession != nullptr) {
		this->imageSession->saveOutputToFile(filePath, this->getCurrentFrame());
	}
}

// --- PSF file I/O ---

void ApplicationController::savePSFToFile(const QString& filePath)
{
	this->psfFileManager->savePSFToFile(filePath, this->psfModule);
}

void ApplicationController::loadPSFFromFile(const QString& filePath)
{
	if (this->psfModule == nullptr) return;

	af::array psf = this->psfFileManager->loadPSFFromFile(filePath);
	if (!psf.isempty()) {
		this->psfModule->setExternalPSF(psf);

		// Store per-patch override so it persists across patch switches
		if (this->hasInputData() && this->parameterTable != nullptr) {
			int frame = this->getCurrentFrame();
			int patchIdx = this->parameterTable->patchIndex(
				this->getCurrentPatchX(), this->getCurrentPatchY());
			this->psfFileManager->storeOverride(frame, patchIdx, psf);
		}
	}
}

void ApplicationController::setAutoSavePSF(bool enabled)
{
	this->psfFileManager->setAutoSavePSF(enabled);
}

void ApplicationController::setPSFSaveFolder(const QString& folder)
{
	this->psfFileManager->setPSFSaveFolder(folder);
}

void ApplicationController::setUseCustomPSFFolder(bool enabled)
{
	this->psfFileManager->setUseCustomPSFFolder(enabled);
}

void ApplicationController::setCustomPSFFolder(const QString& folder)
{
	this->psfFileManager->setCustomPSFFolder(folder);
}

// --- PSF Grid ---

void ApplicationController::generatePSFGrid(int frame, int cropSize)
{
	if (!this->hasInputData() || this->psfModule == nullptr || this->parameterTable == nullptr) {
		LOG_WARNING() << "Cannot generate PSF grid: missing input data or parameters";
		return;
	}

	this->storeCurrentCoefficients();

	PSFGridResult result = this->psfGridGenerator->generate(
		this->psfModule, this->parameterTable,
		frame, this->getPatchGridCols(), this->getPatchGridRows(),
		cropSize);
	emit psfGridGenerated(result);
}

// --- Interpolation ---

void ApplicationController::interpolateCoefficientsInX()
{
	if (this->parameterTable == nullptr || !this->hasInputData()) return;
	this->storeCurrentCoefficients();

	InterpolationResult result = this->interpolationOrchestrator->interpolateInX(
		this->parameterTable, this->getCurrentFrame(), this->getCurrentPatchX(), this->getCurrentPatchY());

	this->loadCoefficientsForCurrentPatch();
	emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	emit interpolationCompleted(result);
}

void ApplicationController::interpolateCoefficientsInY()
{
	if (this->parameterTable == nullptr || !this->hasInputData()) return;
	this->storeCurrentCoefficients();

	InterpolationResult result = this->interpolationOrchestrator->interpolateInY(
		this->parameterTable, this->getCurrentFrame(), this->getCurrentPatchX(), this->getCurrentPatchY());

	this->loadCoefficientsForCurrentPatch();
	emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	emit interpolationCompleted(result);
}

void ApplicationController::interpolateCoefficientsInZ()
{
	if (this->parameterTable == nullptr || !this->hasInputData()) return;
	this->storeCurrentCoefficients();

	InterpolationResult result = this->interpolationOrchestrator->interpolateInZ(
		this->parameterTable, this->getCurrentPatchX(), this->getCurrentPatchY());

	this->loadCoefficientsForCurrentPatch();
	emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	emit interpolationCompleted(result);
}

void ApplicationController::interpolateAllCoefficientsInZ()
{
	if (this->parameterTable == nullptr || !this->hasInputData()) return;
	this->storeCurrentCoefficients();

	this->interpolationOrchestrator->interpolateAllInZ(this->parameterTable);

	this->loadCoefficientsForCurrentPatch();
	emit coefficientsLoaded(this->psfModule->getAllCoefficients());
}

void ApplicationController::setInterpolationPolynomialOrder(int order)
{
	this->interpolationOrchestrator->setPolynomialOrder(order);
}
