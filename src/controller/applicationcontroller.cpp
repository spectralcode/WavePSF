#include "applicationcontroller.h"
#include "imagesession.h"
#include "data/inputdatareader.h"
#include "data/imagedata.h"
#include "data/wavefrontparametertable.h"
#include "core/psf/psfmodule.h"
#include "utils/logging.h"
#include "utils/settingsfilemanager.h"
#include <QFileInfo>

ApplicationController::ApplicationController(QObject* parent)
	: QObject(parent), imageSession(nullptr), inputDataReader(nullptr), psfModule(nullptr)
	, parameterTable(nullptr), deconvolutionLiveMode(false)
{
	this->initializeComponents();
	this->connectSessionSignals();
	this->connectPSFModuleSignals();
	this->connectDeconvolutionSignals();
}

ApplicationController::~ApplicationController()
{
	// Qt parent-child system will handle cleanup
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
	if (this->deconvolutionLiveMode) {
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
