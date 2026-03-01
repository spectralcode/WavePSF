#include "applicationcontroller.h"
#include "imagesession.h"
#include "data/inputdatareader.h"
#include "data/imagedata.h"
#include "data/wavefrontparametertable.h"
#include "core/psf/psfmodule.h"
#include "utils/logging.h"
#include "utils/settingsfilemanager.h"
#include <QFileInfo>
#include <QRandomGenerator>

ApplicationController::ApplicationController(QObject* parent)
	: QObject(parent), imageSession(nullptr), inputDataReader(nullptr), psfModule(nullptr)
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

	qRegisterMetaType<OptimizationConfig>("OptimizationConfig");
	qRegisterMetaType<OptimizationProgress>("OptimizationProgress");
	qRegisterMetaType<OptimizationResult>("OptimizationResult");
	qRegisterMetaType<InterpolationResult>("InterpolationResult");
}

ApplicationController::~ApplicationController()
{
	if (this->optimizationThread) {
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

void ApplicationController::configurePatchGrid(int cols, int rows, int borderExtension)
{
	if (this->imageSession != nullptr) {
		this->imageSession->configurePatchGrid(cols, rows, borderExtension);
		this->resizeParameterTable();
	}
}

void ApplicationController::setPSFCoefficient(int id, double value)
{
	if (this->psfModule != nullptr) {
		this->psfModule->setCoefficient(id, value);
	}
	this->storeCurrentCoefficients();
}

void ApplicationController::resetPSFCoefficients()
{
	if (this->psfModule != nullptr) {
		this->psfModule->resetCoefficients();
	}
	this->storeCurrentCoefficients();
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
		}
	}
}

void ApplicationController::applyPSFSettings(const PSFSettings& settings)
{
	if (this->psfModule != nullptr) {
		this->psfModule->applyPSFSettings(settings);
	}
	emit psfSettingsUpdated(settings);
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
	// Emit ImageSession changed so viewers can update their data connections
	emit imageSessionChanged(this->imageSession);

	// Resize parameter table to match new data dimensions
	this->resizeParameterTable();
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
	this->imageSession = new ImageSession(this);
	this->inputDataReader = new InputDataReader(this);
	this->psfModule = new PSFModule(this);
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
		// When patch or frame changes and live mode is on, re-deconvolve
		connect(this->imageSession, &ImageSession::patchChanged,
				this, [this](int, int) {
					if (this->deconvolutionLiveMode) {
						this->runDeconvolutionOnCurrentPatch();
					}
				});
		connect(this->imageSession, &ImageSession::frameChanged,
				this, [this](int) {
					if (this->deconvolutionLiveMode) {
						this->runDeconvolutionOnCurrentPatch();
					}
				});

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
	this->parameterTable->resize(frames, cols, rows, coeffs);
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

	// Store all job results into parameter table
	for (const OptimizationJobResult& jobResult : result.jobResults) {
		int patchIdx = this->parameterTable->patchIndex(jobResult.patchX, jobResult.patchY);
		this->parameterTable->setCoefficients(jobResult.frameNr, patchIdx, jobResult.bestCoefficients);
	}

	// Apply the last job's coefficients to PSFModule for display
	if (!result.jobResults.isEmpty()) {
		const OptimizationJobResult& lastJob = result.jobResults.last();
		this->psfModule->setAllCoefficients(lastJob.bestCoefficients);
		emit coefficientsLoaded(lastJob.bestCoefficients);

		// Run deconvolution on current patch to show result
		this->runDeconvolutionOnCurrentPatch();
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
