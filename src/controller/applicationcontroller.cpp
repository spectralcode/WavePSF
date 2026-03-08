#include "applicationcontroller.h"
#include "imagesession.h"
#include "data/inputdatareader.h"
#include "data/imagedata.h"
#include "data/wavefrontparametertable.h"
#include "core/psf/psfmodule.h"
#include "utils/logging.h"
#include "utils/settingsfilemanager.h"
#include "utils/afdevicemanager.h"
#include <QFileInfo>
#include <QDir>
#include <QRandomGenerator>
#include <QImage>
#include <QProgressDialog>
#include <QApplication>

ApplicationController::ApplicationController(AFDeviceManager* afDeviceManager, QObject* parent)
	: QObject(parent), afDeviceManager(afDeviceManager), imageSession(nullptr), inputDataReader(nullptr), psfModule(nullptr)
	, parameterTable(nullptr), deconvolutionLiveMode(false)
	, optimizationThread(nullptr), optimizationWorker(nullptr)
	, optimizationLivePreview(false), optimizationLivePreviewInterval(10), optimizationProgressCounter(0)
	, suppressLiveDeconv(false)
	, autoSavePSFEnabled(false), useCustomPSFFolder(false)
{
	this->initializeComponents();
	this->connectSessionSignals();
	this->connectPSFModuleSignals();
	this->connectDeconvolutionSignals();
	this->initializeOptimizationThread();

	// React to device changes from AFDeviceManager (each component clears its own caches via self-connection)
	connect(this->afDeviceManager, &AFDeviceManager::aboutToChangeDevice, this, [this]() {
		this->externalPSFOverrides.clear();
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

ImageSession* ApplicationController::getImageSession() const
{
	return this->imageSession;
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
		this->imageSession->setCurrentFrame(frame);
		this->loadCoefficientsForCurrentPatch();
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
		this->externalPSFOverrides.remove(qMakePair(frame, patchIdx));
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
		this->externalPSFOverrides.remove(qMakePair(frame, patchIdx));
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

	// Clear external PSF override for current patch
	if (this->hasInputData() && this->parameterTable != nullptr) {
		int frame = this->getCurrentFrame();
		int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
		this->externalPSFOverrides.remove(qMakePair(frame, patchIdx));
	}

	this->psfModule->setAllCoefficients(this->coefficientClipboard);
	this->storeCurrentCoefficients();
	emit coefficientsLoaded(this->coefficientClipboard);
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

void ApplicationController::setGeneratorType(const QString& typeName)
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
	this->psfModule->setGeneratorType(typeName);

	// Restore or create parameter table for the new type
	if (this->cachedParameterTables.contains(typeName)) {
		this->parameterTable = this->cachedParameterTables.take(typeName);
	} else {
		this->parameterTable = new WavefrontParameterTable(this);
		this->resizeParameterTable();
	}

	// Load coefficients for current patch from the restored/fresh table
	this->loadCoefficientsForCurrentPatch();

	emit psfSettingsUpdated(this->psfModule->getPSFSettings());
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

void ApplicationController::setDeconvolutionLiveMode(bool enabled)
{
	this->deconvolutionLiveMode = enabled;
	if (enabled) {
		this->runDeconvolutionOnCurrentPatch();
	}
}

void ApplicationController::requestDeconvolution()
{
	this->runDeconvolutionOnCurrentPatch();
}


void ApplicationController::requestBatchDeconvolution()
{
	if (!this->hasInputData() || this->psfModule == nullptr || this->parameterTable == nullptr) {
		LOG_WARNING() << "Cannot run batch deconvolution: missing input data or parameters";
		return;
	}

	int frames = this->imageSession->getInputFrames();
	int cols = this->imageSession->getPatchGridCols();
	int rows = this->imageSession->getPatchGridRows();
	int totalSteps = frames * rows * cols;

	if (totalSteps == 0) {
		LOG_WARNING() << "Cannot run batch deconvolution: no patches to process";
		return;
	}

	// Store current coefficients before batch processing
	this->storeCurrentCoefficients();

	// Block PSFModule signals to prevent GUI flickering during batch
	bool oldBlockState = this->psfModule->blockSignals(true);
	this->suppressLiveDeconv = true;

	// Create progress dialog
	QProgressDialog progressDialog("Initializing batch deconvolution...", "Cancel", 0, totalSteps);
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
				int patchIdx = this->parameterTable->patchIndex(x, y);
				QVector<double> coeffs = this->parameterTable->getCoefficients(frame, patchIdx);
				if (coeffs.isEmpty()) {
					step++;
					progressDialog.setValue(step);
					QApplication::processEvents();
					if (progressDialog.wasCanceled()) { cancelled = true; }
					continue;
				}

				// Set coefficients (signals blocked, no GUI update)
				this->psfModule->setAllCoefficients(coeffs);

				// Get input patch
				ImagePatch inputPatch = this->imageSession->getInputPatch(frame, x, y);
				if (!inputPatch.isValid()) {
					step++;
					progressDialog.setValue(step);
					QApplication::processEvents();
					if (progressDialog.wasCanceled()) { cancelled = true; }
					continue;
				}

				// Deconvolve
				af::array result = this->psfModule->deconvolve(inputPatch.data);
				if (!result.isempty()) {
					this->imageSession->setOutputPatch(frame, x, y, result);
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
	this->imageSession->flushOutput();

	// Restore state
	this->psfModule->blockSignals(oldBlockState);
	this->suppressLiveDeconv = false;

	// Reload current patch to restore GUI state and show result
	this->loadCoefficientsForCurrentPatch();

	if (cancelled) {
		LOG_INFO() << "Batch deconvolution cancelled at step" << step << "of" << totalSteps;
	} else {
		LOG_INFO() << "Batch deconvolution completed:" << totalSteps << "patches processed";
	}

	emit batchDeconvolutionCompleted();
}

void ApplicationController::runDeconvolutionOnCurrentPatch()
{
	if (!this->hasInputData() || this->psfModule == nullptr) {
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

void ApplicationController::handlePSFUpdatedForDeconvolution(af::array psf)
{
	Q_UNUSED(psf);
	if (this->deconvolutionLiveMode && !this->suppressLiveDeconv) {
		this->runDeconvolutionOnCurrentPatch();
	}

	// Auto-save PSF if enabled
	if (this->autoSavePSFEnabled && !this->psfSaveFolder.isEmpty() && this->hasInputData()) {
		int frame = this->getCurrentFrame();
		int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
		QString name = QString("%1_%2.tif").arg(frame).arg(patchIdx);
		this->savePSFToFile(this->psfSaveFolder + "/" + name);
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

bool ApplicationController::hasGroundTruthData() const
{
	return this->imageSession != nullptr && this->imageSession->hasGroundTruthData();
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

int ApplicationController::getPatchBorderExtension() const
{
	return this->imageSession != nullptr ? this->imageSession->getPatchBorderExtension() : 0;
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
	this->externalPSFOverrides.clear();

	// Invalidate cached parameter tables (dimensions no longer valid)
	qDeleteAll(this->cachedParameterTables);
	this->cachedParameterTables.clear();

	// Emit ImageSession changed so viewers can update their data connections
	emit imageSessionChanged(this->imageSession);

	// Resize parameter table to match new data dimensions
	this->resizeParameterTable();

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
	this->tableInterpolator = new TableInterpolator(this);
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
		connect(this->psfModule, &PSFModule::psfUpdated,
				this, &ApplicationController::psfUpdated);
		connect(this->psfModule, &PSFModule::parameterDescriptorsChanged,
				this, &ApplicationController::psfParameterDescriptorsChanged);
		connect(this->psfModule, &PSFModule::generatorTypeChanged,
				this, &ApplicationController::generatorTypeChanged);

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

void ApplicationController::storeCurrentCoefficients()
{
	if (this->parameterTable == nullptr || this->psfModule == nullptr || this->imageSession == nullptr) {
		return;
	}
	if (!this->hasInputData()) {
		return;
	}
	int frame = this->getCurrentFrame();
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

	// Check per-patch external PSF override (from one-shot "Load PSF from File")
	{
		int frame = this->getCurrentFrame();
		int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
		auto key = qMakePair(frame, patchIdx);
		if (this->externalPSFOverrides.contains(key)) {
			this->psfModule->setExternalPSF(this->externalPSFOverrides[key]);
			return;
		}
	}

	// Custom PSF folder mode: load PSF from file instead of computing from coefficients
	if (this->useCustomPSFFolder && !this->customPSFFolder.isEmpty()) {
		int frame = this->getCurrentFrame();
		int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
		QString baseName = QString("%1_%2").arg(frame).arg(patchIdx);
		QDir dir(this->customPSFFolder);
		QFileInfoList files = dir.entryInfoList(QDir::Files);
		for (const QFileInfo& fi : files) {
			if (fi.baseName() == baseName) {
				this->loadPSFFromFile(fi.absoluteFilePath());
				return;
			}
		}
		LOG_WARNING() << "No PSF file found for" << baseName << "in" << this->customPSFFolder;
		return;
	}

	int frame = this->getCurrentFrame();
	int patchIdx = this->parameterTable->patchIndex(this->getCurrentPatchX(), this->getCurrentPatchY());
	QVector<double> coeffs = this->parameterTable->getCoefficients(frame, patchIdx);
	if (!coeffs.isEmpty()) {
		this->psfModule->setAllCoefficients(coeffs);
		emit coefficientsLoaded(coeffs);
	}
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
	if (!this->buildOptimizationJobs(config)) {
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

bool ApplicationController::buildOptimizationJobs(OptimizationConfig& config)
{
	const int gridCols = this->imageSession->getPatchGridCols();
	const int gridRows = this->imageSession->getPatchGridRows();
	const int totalPatches = gridCols * gridRows;
	const int totalFrames = this->imageSession->getInputFrames();
	const bool hasGroundTruth = this->imageSession->hasGroundTruthData();
	const int coeffCount = this->psfModule->getAllCoefficients().size();

	config.jobs.clear();

	QVector<int> frameList;
	QVector<int> patchList;

	if (config.mode == 0) {
		// Single patch mode: current frame and current patch
		frameList.append(this->getCurrentFrame());
		int linearPatch = this->parameterTable->patchIndex(
			this->getCurrentPatchX(), this->getCurrentPatchY());
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
	for (int frameNr : frameList) {
		for (int linearPatch : patchList) {
			int patchX = linearPatch % gridCols;
			int patchY = linearPatch / gridCols;

			OptimizationJob job;
			job.frameNr = frameNr;
			job.patchX = patchX;
			job.patchY = patchY;

			// Extract input patch data (deep copy for thread safety)
			ImagePatch inputPatch = this->imageSession->getInputPatch(frameNr, patchX, patchY);
			if (!inputPatch.isValid()) {
				LOG_WARNING() << "Invalid input patch at frame" << frameNr
							  << "patch (" << patchX << "," << patchY << "), skipping";
				continue;
			}
			job.inputPatch = inputPatch.data.copy();

			// Extract ground truth patch (deep copy) if available and reference metric requested
			if (hasGroundTruth && config.useReferenceMetric) {
				ImagePatch gtPatch = this->imageSession->getGroundTruthPatch(frameNr, patchX, patchY);
				if (gtPatch.isValid()) {
					job.groundTruthPatch = gtPatch.data.copy();
				}
			}

			// Determine start coefficients
			int patchIdx = this->parameterTable->patchIndex(patchX, patchY);
			switch (config.startCoefficientSource) {
				case 0: {
					// Current stored coefficients
					QVector<double> coeffs = this->parameterTable->getCoefficients(frameNr, patchIdx);
					job.startCoefficients = coeffs.isEmpty()
						? QVector<double>(coeffCount, 0.0) : coeffs;
					break;
				}
				case 1: {
					// Fixed frame
					int sourceFrame = qBound(0, config.sourceParam, totalFrames - 1);
					QVector<double> coeffs = this->parameterTable->getCoefficients(sourceFrame, patchIdx);
					job.startCoefficients = coeffs.isEmpty()
						? QVector<double>(coeffCount, 0.0) : coeffs;
					break;
				}
				case 2: {
					// Offset from current frame
					int sourceFrame = qBound(0, frameNr + config.sourceParam, totalFrames - 1);
					QVector<double> coeffs = this->parameterTable->getCoefficients(sourceFrame, patchIdx);
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
						QVector<double> coeffs = this->parameterTable->getCoefficients(frameNr, patchIdx);
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
					QVector<double> coeffs = this->parameterTable->getCoefficients(frameNr, sourcePatchIdx);
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
	if (this->psfModule == nullptr) {
		return;
	}

	af::array psf = this->psfModule->getCurrentPSF();
	if (psf.isempty()) {
		LOG_WARNING() << "No PSF to save";
		return;
	}

	int height = static_cast<int>(psf.dims(0));  // rows
	int width = static_cast<int>(psf.dims(1));   // cols

	// Copy PSF to host as float
	af::array floatPSF = psf.as(af::dtype::f32);
	float* hostData = floatPSF.host<float>();

	// Write single-page 32-bit float TIFF
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly)) {
		LOG_WARNING() << "Could not write PSF file:" << file.errorString();
		af::freeHost(hostData);
		return;
	}

	QDataStream stream(&file);
	stream.setByteOrder(QDataStream::LittleEndian);

	uint32_t bytesPerFrame = static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * sizeof(float);
	const int numIfdEntries = 10;
	uint32_t ifdSize = 2 + numIfdEntries * 12 + 4; // 126 bytes
	uint32_t headerSize = 8;
	uint32_t dataStart = headerSize + ifdSize;

	auto writeIfdEntry = [&](uint16_t tag, uint16_t type, uint32_t count, uint32_t value) {
		stream << tag << type << count << value;
	};

	// TIFF Header
	stream.writeRawData("II", 2);
	stream << static_cast<uint16_t>(42);
	stream << headerSize; // offset to first IFD

	// IFD
	stream << static_cast<uint16_t>(numIfdEntries);
	writeIfdEntry(256, 4, 1, static_cast<uint32_t>(width));      // ImageWidth
	writeIfdEntry(257, 4, 1, static_cast<uint32_t>(height));     // ImageLength
	writeIfdEntry(258, 3, 1, 32);                                 // BitsPerSample
	writeIfdEntry(259, 3, 1, 1);                                  // Compression = None
	writeIfdEntry(262, 3, 1, 1);                                  // PhotometricInterpretation = MinIsBlack
	writeIfdEntry(273, 4, 1, dataStart);                          // StripOffsets
	writeIfdEntry(277, 3, 1, 1);                                  // SamplesPerPixel
	writeIfdEntry(278, 4, 1, static_cast<uint32_t>(height));     // RowsPerStrip
	writeIfdEntry(279, 4, 1, bytesPerFrame);                      // StripByteCounts
	writeIfdEntry(339, 3, 1, 3);                                  // SampleFormat = IEEE float
	stream << static_cast<uint32_t>(0); // next IFD = none

	// Convert column-major (AF) to row-major (TIFF):
	// AF element (row=y, col=x) lives at hostData[y + x * height]
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			float val = hostData[y + x * height];
			stream.writeRawData(reinterpret_cast<const char*>(&val), sizeof(float));
		}
	}

	af::freeHost(hostData);
	file.close();
	LOG_INFO() << "PSF saved:" << filePath << "(" << width << "x" << height << ", 32-bit float)";
}

void ApplicationController::loadPSFFromFile(const QString& filePath)
{
	if (this->psfModule == nullptr) {
		return;
	}

	QFileInfo fileInfo(filePath);
	QString suffix = fileInfo.suffix().toLower();

	try {
		af::array psf;

		if (suffix == "tif" || suffix == "tiff") {
			// Load TIFF preserving native type
			psf = af::loadImageNative(filePath.toStdString().c_str());
			psf = psf.as(af::dtype::f32);
		} else {
			// Load standard image via QImage, normalize to [0,1]
			QImage img(filePath);
			if (img.isNull()) {
				LOG_WARNING() << "Could not load PSF image:" << filePath;
				return;
			}
			img = img.convertToFormat(QImage::Format_Grayscale8);
			int w = img.width();
			int h = img.height();
			QVector<float> floatData(w * h);
			for (int y = 0; y < h; y++) {
				const uchar* scanLine = img.constScanLine(y);
				for (int x = 0; x < w; x++) {
					floatData[x + y * w] = scanLine[x] / 255.0f;
				}
			}
			psf = af::array(w, h, floatData.constData());
		}

		if (!psf.isempty()) {
			this->psfModule->setExternalPSF(psf);

			// Store per-patch override so it persists across patch switches
			if (this->hasInputData() && this->parameterTable != nullptr) {
				int frame = this->getCurrentFrame();
				int patchIdx = this->parameterTable->patchIndex(
					this->getCurrentPatchX(), this->getCurrentPatchY());
				this->externalPSFOverrides[qMakePair(frame, patchIdx)] = psf;
			}

			LOG_INFO() << "PSF loaded from file:" << filePath
					   << "(" << psf.dims(0) << "x" << psf.dims(1) << ")";
		}
	} catch (af::exception& e) {
		LOG_WARNING() << "Failed to load PSF file:" << e.what();
	}
}

void ApplicationController::setAutoSavePSF(bool enabled)
{
	this->autoSavePSFEnabled = enabled;
	LOG_INFO() << "PSF auto-save" << (enabled ? "enabled" : "disabled");
}

void ApplicationController::setPSFSaveFolder(const QString& folder)
{
	this->psfSaveFolder = folder;
	LOG_INFO() << "PSF save folder set to:" << folder;
}

void ApplicationController::setUseCustomPSFFolder(bool enabled)
{
	this->useCustomPSFFolder = enabled;
	LOG_INFO() << "Custom PSF folder" << (enabled ? "enabled" : "disabled");
}

void ApplicationController::setCustomPSFFolder(const QString& folder)
{
	this->customPSFFolder = folder;
	LOG_INFO() << "Custom PSF folder set to:" << folder;
}

// --- Interpolation ---

void ApplicationController::interpolateCoefficientsInX()
{
	if (this->parameterTable == nullptr || !this->hasInputData()) return;
	this->storeCurrentCoefficients();

	int frame = this->getCurrentFrame();
	int patchX = this->getCurrentPatchX();
	int patchY = this->getCurrentPatchY();
	int width = this->parameterTable->getNumberOfPatchesInX();
	int numCoeffs = this->parameterTable->getCoefficientsPerPatch();

	QVector<InterpolationSlice> slices = this->tableInterpolator->interpolateInX(
		this->parameterTable, frame, patchX, patchY);

	// Build result for plotting
	InterpolationResult result;
	result.slices = slices;
	result.totalCoefficients = numCoeffs;
	result.axisLabel = tr("Patch X");

	// Read the full interpolated row from the table
	result.allPositions.resize(width);
	result.allValues.resize(numCoeffs);
	for (int x = 0; x < width; x++) {
		result.allPositions[x] = x;
	}
	for (int c = 0; c < numCoeffs; c++) {
		result.allValues[c].resize(width);
		for (int x = 0; x < width; x++) {
			int patch = this->parameterTable->patchIndex(x, patchY);
			result.allValues[c][x] = this->parameterTable->getCoefficient(frame, patch, c);
		}
	}

	this->loadCoefficientsForCurrentPatch();
	emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	emit interpolationCompleted(result);
}

void ApplicationController::interpolateCoefficientsInY()
{
	if (this->parameterTable == nullptr || !this->hasInputData()) return;
	this->storeCurrentCoefficients();

	int frame = this->getCurrentFrame();
	int patchX = this->getCurrentPatchX();
	int patchY = this->getCurrentPatchY();
	int height = this->parameterTable->getNumberOfPatchesInY();
	int numCoeffs = this->parameterTable->getCoefficientsPerPatch();

	QVector<InterpolationSlice> slices = this->tableInterpolator->interpolateInY(
		this->parameterTable, frame, patchX, patchY);

	InterpolationResult result;
	result.slices = slices;
	result.totalCoefficients = numCoeffs;
	result.axisLabel = tr("Patch Y");

	result.allPositions.resize(height);
	result.allValues.resize(numCoeffs);
	for (int y = 0; y < height; y++) {
		result.allPositions[y] = y;
	}
	for (int c = 0; c < numCoeffs; c++) {
		result.allValues[c].resize(height);
		for (int y = 0; y < height; y++) {
			int patch = this->parameterTable->patchIndex(patchX, y);
			result.allValues[c][y] = this->parameterTable->getCoefficient(frame, patch, c);
		}
	}

	this->loadCoefficientsForCurrentPatch();
	emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	emit interpolationCompleted(result);
}

void ApplicationController::interpolateCoefficientsInZ()
{
	if (this->parameterTable == nullptr || !this->hasInputData()) return;
	this->storeCurrentCoefficients();

	int patchX = this->getCurrentPatchX();
	int patchY = this->getCurrentPatchY();
	int numFrames = this->parameterTable->getNumberOfFrames();
	int numCoeffs = this->parameterTable->getCoefficientsPerPatch();
	int patch = this->parameterTable->patchIndex(patchX, patchY);

	QVector<InterpolationSlice> slices = this->tableInterpolator->interpolateInZ(
		this->parameterTable, patchX, patchY);

	InterpolationResult result;
	result.slices = slices;
	result.totalCoefficients = numCoeffs;
	result.axisLabel = tr("Frame");

	result.allPositions.resize(numFrames);
	result.allValues.resize(numCoeffs);
	for (int f = 0; f < numFrames; f++) {
		result.allPositions[f] = f;
	}
	for (int c = 0; c < numCoeffs; c++) {
		result.allValues[c].resize(numFrames);
		for (int f = 0; f < numFrames; f++) {
			result.allValues[c][f] = this->parameterTable->getCoefficient(f, patch, c);
		}
	}

	this->loadCoefficientsForCurrentPatch();
	emit coefficientsLoaded(this->psfModule->getAllCoefficients());
	emit interpolationCompleted(result);
}

void ApplicationController::interpolateAllCoefficientsInZ()
{
	if (this->parameterTable == nullptr || !this->hasInputData()) return;
	this->storeCurrentCoefficients();

	this->tableInterpolator->interpolateAllInZ(this->parameterTable);

	this->loadCoefficientsForCurrentPatch();
	emit coefficientsLoaded(this->psfModule->getAllCoefficients());
}

void ApplicationController::setInterpolationPolynomialOrder(int order)
{
	if (this->tableInterpolator != nullptr) {
		this->tableInterpolator->setPolynomialOrder(order);
	}
}
