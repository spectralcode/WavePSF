#include "applicationcontroller.h"
#include "optimizationcontroller.h"
#include "psffilecontroller.h"
#include "coefficientworkspace.h"
#include "deconvolutionorchestrator.h"
#include "imagesession.h"
#include "data/inputdatareader.h"
#include "data/imagedata.h"
#include "data/wavefrontparametertable.h"
#include "core/psf/psfmodule.h"
#include "core/psf/ipsfgenerator.h"
#include "core/optimization/optimizationjobbuilder.h"
#include "core/interpolation/interpolationorchestrator.h"
#include "core/psf/psfgridgenerator.h"
#include "utils/logging.h"
#include <QFileInfo>
#include "utils/settingsfilemanager.h"
#include "utils/afdevicemanager.h"

ApplicationController::ApplicationController(AFDeviceManager* afDeviceManager, QObject* parent)
	: QObject(parent), afDeviceManager(afDeviceManager), imageSession(nullptr), inputDataReader(nullptr), psfModule(nullptr)
	, coefficientWorkspace(nullptr), deconvolutionLiveMode(false)
	, optimizationController(nullptr), suppressLiveDeconv(false), psfFileController(nullptr)
	, interpolationOrchestrator(nullptr), deconvolutionOrchestrator(nullptr), psfGridGenerator(nullptr)
{
	this->initializeComponents();
	this->connectSessionSignals();
	this->connectPSFModuleSignals();
	this->connectDeconvolutionSignals();

	// React to device changes from AFDeviceManager (each component clears its own caches via self-connection)
	connect(this->afDeviceManager, &AFDeviceManager::deviceChanged, this, [this]() {
		if (this->hasInputData())
			this->psfModule->applyPSFSettings(this->psfModule->getPSFSettings());
	});

	// Optimization controller (owns worker thread and progress throttling)
	this->optimizationController = new OptimizationController(this);
	connect(this->optimizationController, &OptimizationController::started,
			this, &ApplicationController::optimizationStarted);
	connect(this->optimizationController, &OptimizationController::progressUpdated,
			this, &ApplicationController::optimizationProgressUpdated);
	connect(this->optimizationController, &OptimizationController::livePreviewReady,
			this, &ApplicationController::handleOptimizationLivePreview);
	connect(this->optimizationController, &OptimizationController::finished,
			this, &ApplicationController::handleOptimizationFinished);

	// PSF file controller (owns PSFFileManager, file-based PSF logic)
	connect(this->psfFileController, &PSFFileController::filePSFInfoUpdated,
			this, &ApplicationController::filePSFInfoUpdated);

	// Coefficient workspace (owns parameter table, cached tables, clipboard/undo)
	connect(this->coefficientWorkspace, &CoefficientWorkspace::coefficientsLoaded,
			this, &ApplicationController::coefficientsLoaded);
	connect(this->coefficientWorkspace, &CoefficientWorkspace::parametersLoaded,
			this, &ApplicationController::parametersLoaded);

	qRegisterMetaType<InterpolationResult>("InterpolationResult");
}

ApplicationController::~ApplicationController()
{
}

bool ApplicationController::openInputFile(const QString& filePath)
{
	return this->loadFileToSession(filePath, false);
}

bool ApplicationController::openInputFolder(const QString& folderPath)
{
	if (this->inputDataReader == nullptr || this->imageSession == nullptr) {
		LOG_ERROR() << "Components not initialized";
		return false;
	}

	try {
		LOG_INFO() << tr("Attempting to load folder: ") << folderPath;
		ImageData* imageData = this->inputDataReader->loadFolder(folderPath);
		if (imageData == nullptr) {
			LOG_WARNING() << "Failed to load image data from folder:" << folderPath;
			return false;
		}

		this->imageSession->setInputData(imageData);
		LOG_INFO() << "Input folder loaded successfully:" << folderPath;
		return true;

	} catch (const QString& error) {
		LOG_WARNING() << "Exception during folder loading:" << error << "folder:" << folderPath;
		return false;
	} catch (...) {
		LOG_WARNING() << "Unknown exception during folder loading:" << folderPath;
		return false;
	}
}

bool ApplicationController::openGroundTruthFile(const QString& filePath)
{
	return this->loadFileToSession(filePath, true);
}

void ApplicationController::setCurrentFrame(int frame)
{
	if (this->imageSession == nullptr) return;
	if (this->imageSession->getCurrentFrame() == frame) return;

	this->coefficientWorkspace->store();

	bool suppress3D = this->deconvolutionLiveMode &&
					  this->psfModule != nullptr &&
					  this->psfModule->is3DAlgorithm();

	// Suppress psfUpdated-triggered deconv during coefficient loading
	// to avoid double-fire.  We trigger explicitly below instead,
	// because input data always changes on frame switch even when
	// the PSF stays the same.
	bool oldSuppress = this->suppressLiveDeconv;
	this->suppressLiveDeconv = true;

	this->imageSession->setCurrentFrame(frame);
	this->coefficientWorkspace->loadForCurrentPatch();

	this->suppressLiveDeconv = oldSuppress;

	// Re-deconvolve: input data changed (different frame).
	// Skip for 3D algorithms — they process all frames at once.
	if (this->deconvolutionLiveMode && !suppress3D) {
		this->deconvolutionOrchestrator->runOnCurrentPatch();
	}
}

void ApplicationController::setCurrentPatch(int x, int y)
{
	if (this->imageSession != nullptr) {
		this->coefficientWorkspace->store();

		bool oldSuppress = this->suppressLiveDeconv;
		this->suppressLiveDeconv = true;

		this->imageSession->setCurrentPatch(x, y);
		this->coefficientWorkspace->loadForCurrentPatch();

		this->suppressLiveDeconv = oldSuppress;

		if (this->deconvolutionLiveMode) {
			this->deconvolutionOrchestrator->runOnCurrentPatch();
		}
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
		this->coefficientWorkspace->clearCache();
		this->coefficientWorkspace->resize();
	}
}

void ApplicationController::setPSFCoefficient(int id, double value)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setCoefficient(id, value);
	}
	this->coefficientWorkspace->store();
}

void ApplicationController::resetPSFCoefficients()
{
	if (this->psfModule != nullptr) {
		this->psfModule->resetCoefficients();
	}
	this->coefficientWorkspace->store();
	if (this->psfModule != nullptr) {
		emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	}
}

void ApplicationController::copyCoefficients()
{
	this->coefficientWorkspace->copy();
}

void ApplicationController::pasteCoefficients()
{
	this->coefficientWorkspace->paste();
}

void ApplicationController::undoPasteCoefficients()
{
	this->coefficientWorkspace->undoPaste();
}

void ApplicationController::resetAllCoefficients()
{
	this->coefficientWorkspace->resetAll();
}

void ApplicationController::saveParametersToFile(const QString& filePath)
{
	this->coefficientWorkspace->saveToFile(filePath);
}

void ApplicationController::loadParametersFromFile(const QString& filePath)
{
	this->coefficientWorkspace->loadFromFile(filePath);
}

void ApplicationController::applyPSFSettings(const PSFSettings& settings)
{
	if (this->psfModule != nullptr) {
		this->psfModule->applyPSFSettings(settings);
		// Resize parameter table if coefficient count changed
		this->coefficientWorkspace->resize();
		// Re-send current coefficients so UI sliders reflect actual values
		// (parameterDescriptorsChanged rebuilds sliders to defaults)
		emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	}
	emit psfSettingsUpdated(this->psfModule->getPSFSettings());
	this->psfFileController->refreshFileInfo();
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

	// Save current coefficients and cache the table for the outgoing generator type
	this->coefficientWorkspace->store();
	this->coefficientWorkspace->cacheCurrentTable(oldTypeName);

	// Switch generator (PSFModule handles caching generator settings internally)
	this->psfModule->switchGenerator(typeName);

	// Restore or create parameter table for the new type
	this->coefficientWorkspace->restoreOrCreateTable(typeName);

	// Sync z-planes for 3D generators
	this->syncNumZPlanesWithInput();

	// Notify GUI before loading coefficients so preview mode is correct
	emit psfSettingsUpdated(this->psfModule->getPSFSettings());

	// Load coefficients for current patch from the restored/fresh table
	this->coefficientWorkspace->loadForCurrentPatch();

	// Ensure a PSF is generated even when no input data is loaded
	// (loadForCurrentPatch early-returns without input data)
	if (this->psfModule->getCurrentPSF().isempty()) {
		this->psfModule->refreshPSF();
	}

	this->psfFileController->refreshFileInfo();
}

void ApplicationController::applyInlineSettings(const QVariantMap& settings)
{
	if (this->psfModule != nullptr) {
		this->psfModule->applyInlineSettings(settings);
		emit psfSettingsUpdated(this->psfModule->getPSFSettings());
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

void ApplicationController::setRegularizer3D(int mode)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setRegularizer3D(mode);
	}
}

void ApplicationController::setRegularizationWeight(float weight)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setRegularizationWeight(weight);
	}
}

void ApplicationController::setDeconvolutionLiveMode(bool enabled)
{
	this->deconvolutionLiveMode = enabled;
	if (enabled) {
		this->suppressLiveDeconv = false;
		this->deconvolutionOrchestrator->runOnCurrentPatch();
	}
}

void ApplicationController::requestDeconvolution()
{
	this->suppressLiveDeconv = false;
	this->deconvolutionOrchestrator->runOnCurrentPatch();
}


void ApplicationController::requestBatchDeconvolution()
{
	this->suppressLiveDeconv = true;
	bool ok = this->deconvolutionOrchestrator->runBatch();
	this->suppressLiveDeconv = false;
	if (!ok) return;
	this->coefficientWorkspace->loadForCurrentPatch();
	emit batchDeconvolutionCompleted();
}

void ApplicationController::handlePSFUpdatedForDeconvolution(af::array psf)
{
	Q_UNUSED(psf);
	if (this->deconvolutionLiveMode && !this->suppressLiveDeconv) {
		this->deconvolutionOrchestrator->runOnCurrentPatch();
	}

	// Auto-save PSF if enabled
	if (this->hasInputData() && this->coefficientWorkspace->table() != nullptr) {
		int frame = this->getCurrentFrame();
		int patchIdx = this->coefficientWorkspace->table()->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
		this->psfFileController->autoSaveIfEnabled(frame, patchIdx);
	}
}

void ApplicationController::handleDeconvolutionSettingsChanged()
{
	if (this->deconvolutionLiveMode) {
		this->deconvolutionOrchestrator->runOnCurrentPatch();
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
	// Route folder paths to folder loader (e.g. from recent files menu)
	if (QFileInfo(filePath).isDir()) {
		this->requestOpenInputFolder(filePath);
		return;
	}

	if (this->openInputFile(filePath)) {
		emit inputFileLoaded(filePath);
	} else {
		emit fileLoadError(filePath, "Failed to load input file");
	}
}

void ApplicationController::requestOpenInputFolder(const QString& folderPath)
{
	if (this->openInputFolder(folderPath)) {
		emit inputFileLoaded(folderPath);
	} else {
		emit fileLoadError(folderPath, "Failed to load image folder");
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
	// Invalidate cached parameter tables (dimensions no longer valid)
	this->coefficientWorkspace->clearCache();

	// Emit ImageSession changed so viewers can update their data connections
	emit imageSessionChanged(this->imageSession);

	// Resize parameter table to match new data dimensions
	this->coefficientWorkspace->resize();

	this->syncNumZPlanesWithInput();

	// Reset psfModule and UI sliders to the new dataset's patch (0,0) values,
	// preventing stale coefficients from the previous file's active patch from
	// being written back on the next storeCurrentCoefficients() call.
	this->coefficientWorkspace->loadForCurrentPatch();
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
	this->coefficientWorkspace = new CoefficientWorkspace(this->psfModule, this->imageSession, this);
	this->interpolationOrchestrator = new InterpolationOrchestrator(this);
	this->psfFileController = new PSFFileController(this->psfModule, this);
	this->deconvolutionOrchestrator = new DeconvolutionOrchestrator(
		this->psfModule, this->imageSession, this->coefficientWorkspace, this);
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
			this->coefficientWorkspace->clearAndResize();
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
		// Note: deconvolution on patch/frame change is triggered explicitly
		// from setCurrentFrame()/setCurrentPatch(), not through the
		// psfUpdated signal chain.

		// Forward outputPatchUpdated to deconvolutionCompleted
		connect(this->imageSession, &ImageSession::outputPatchUpdated,
				this, &ApplicationController::deconvolutionCompleted);
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

void ApplicationController::startOptimization(const OptimizationConfig& uiConfig)
{
	if (!this->hasInputData() || this->psfModule == nullptr) {
		LOG_WARNING() << "Cannot start optimization: no input data or PSF module";
		return;
	}

	// Store current coefficients before starting
	this->coefficientWorkspace->store();

	// Copy UI config and fill in controller-owned data
	OptimizationConfig config = uiConfig;

	// ArrayFire backend + device (worker thread needs explicit selection; both are per-thread)
	config.afBackend = this->afDeviceManager->getActiveBackendId();
	config.afDeviceId = this->afDeviceManager->getActiveDeviceId();

	// PSF settings
	config.psfSettings = this->psfModule->getPSFSettings();

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
			this->coefficientWorkspace->table(), this->getCurrentFrame(), this->getCurrentPatchX(), this->getCurrentPatchY())) {
		LOG_WARNING() << "Failed to build optimization jobs";
		return;
	}

	LOG_INFO() << "Starting optimization with" << config.jobs.size() << "jobs";

	this->optimizationController->start(config);
}

void ApplicationController::cancelDeconvolution()
{
	this->deconvolutionOrchestrator->cancel();
}

void ApplicationController::cancelOptimization()
{
	this->optimizationController->cancel();
}

void ApplicationController::updateOptimizationLivePreview(bool enabled, int interval)
{
	this->optimizationController->setLivePreview(enabled, interval);
}

void ApplicationController::updateOptimizationAlgorithmParameters(const QVariantMap& params)
{
	this->optimizationController->updateAlgorithmParameters(params);
}

void ApplicationController::handleOptimizationLivePreview(
	const QVector<double>& coefficients, int frame, int patchX, int patchY)
{
	bool oldSuppress = this->suppressLiveDeconv;
	this->suppressLiveDeconv = true;
	this->psfModule->setAllCoefficients(coefficients);
	emit coefficientsLoaded(coefficients);
	this->imageSession->setCurrentFrame(frame);
	this->imageSession->setCurrentPatch(patchX, patchY);
	this->suppressLiveDeconv = oldSuppress;
	this->deconvolutionOrchestrator->runOnCurrentPatch();
}

void ApplicationController::handleOptimizationFinished(const OptimizationResult& result)
{
	LOG_INFO() << "Optimization finished:" << result.jobResults.size()
			   << "jobs," << result.totalOuterIterations << "iterations"
			   << (result.wasCancelled ? "(cancelled)" : "");

	// Store coefficients and write deconvolved output for every job
	for (const OptimizationJobResult& jobResult : result.jobResults) {
		int patchIdx = this->coefficientWorkspace->table()->patchIndex(jobResult.patchX, jobResult.patchY);
		this->coefficientWorkspace->table()->setCoefficients(jobResult.frameNr, patchIdx, jobResult.bestCoefficients);

		bool oldSuppress = this->suppressLiveDeconv;
		this->suppressLiveDeconv = true;
		this->psfModule->setAllCoefficients(jobResult.bestCoefficients);
		this->imageSession->setCurrentFrame(jobResult.frameNr);
		this->imageSession->setCurrentPatch(jobResult.patchX, jobResult.patchY);
		this->suppressLiveDeconv = oldSuppress;
		this->deconvolutionOrchestrator->runOnCurrentPatch();
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
	this->psfFileController->savePSFToFile(filePath);
}

void ApplicationController::setAutoSavePSF(bool enabled)
{
	this->psfFileController->setAutoSavePSF(enabled);
}

void ApplicationController::setPSFSaveFolder(const QString& folder)
{
	this->psfFileController->setPSFSaveFolder(folder);
}

void ApplicationController::setFilePSFSource(const QString& path)
{
	this->psfFileController->setFilePSFSource(path);
}

// --- PSF Grid ---

void ApplicationController::generatePSFGrid(int frame, int cropSize)
{
	if (!this->hasInputData() || this->psfModule == nullptr || this->coefficientWorkspace->table() == nullptr) {
		LOG_WARNING() << "Cannot generate PSF grid: missing input data or parameters";
		return;
	}

	this->coefficientWorkspace->store();

	PSFGridResult result = this->psfGridGenerator->generate(
		this->psfModule, this->coefficientWorkspace->table(),
		frame, this->getPatchGridCols(), this->getPatchGridRows(),
		cropSize);
	emit psfGridGenerated(result);
}

// --- Interpolation ---

void ApplicationController::interpolateCoefficientsInX()
{
	if (this->coefficientWorkspace->table() == nullptr || !this->hasInputData()) return;
	this->coefficientWorkspace->store();

	InterpolationResult result = this->interpolationOrchestrator->interpolateInX(
		this->coefficientWorkspace->table(), this->getCurrentFrame(), this->getCurrentPatchX(), this->getCurrentPatchY());

	this->coefficientWorkspace->loadForCurrentPatch();
	emit interpolationCompleted(result);
}

void ApplicationController::interpolateCoefficientsInY()
{
	if (this->coefficientWorkspace->table() == nullptr || !this->hasInputData()) return;
	this->coefficientWorkspace->store();

	InterpolationResult result = this->interpolationOrchestrator->interpolateInY(
		this->coefficientWorkspace->table(), this->getCurrentFrame(), this->getCurrentPatchX(), this->getCurrentPatchY());

	this->coefficientWorkspace->loadForCurrentPatch();
	emit interpolationCompleted(result);
}

void ApplicationController::interpolateCoefficientsInZ()
{
	if (this->coefficientWorkspace->table() == nullptr || !this->hasInputData()) return;
	this->coefficientWorkspace->store();

	InterpolationResult result = this->interpolationOrchestrator->interpolateInZ(
		this->coefficientWorkspace->table(), this->getCurrentPatchX(), this->getCurrentPatchY());

	this->coefficientWorkspace->loadForCurrentPatch();
	emit interpolationCompleted(result);
}

void ApplicationController::interpolateAllCoefficientsInZ()
{
	if (this->coefficientWorkspace->table() == nullptr || !this->hasInputData()) return;
	this->coefficientWorkspace->store();

	this->interpolationOrchestrator->interpolateAllInZ(this->coefficientWorkspace->table());

	this->coefficientWorkspace->loadForCurrentPatch();
}

void ApplicationController::setInterpolationPolynomialOrder(int order)
{
	this->interpolationOrchestrator->setPolynomialOrder(order);
}
