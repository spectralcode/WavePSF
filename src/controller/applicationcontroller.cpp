#include "applicationcontroller.h"
#include "imagesession.h"
#include "data/inputdatareader.h"
#include "data/imagedata.h"
#include "utils/logging.h"
#include "utils/settingsfilemanager.h"
#include <QFileInfo>

ApplicationController::ApplicationController(QObject* parent)
	: QObject(parent), imageSession(nullptr), inputDataReader(nullptr)
{
	this->initializeComponents();
	this->connectSessionSignals();
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
		this->imageSession->setCurrentFrame(frame);
	}
}

void ApplicationController::setCurrentPatch(int x, int y)
{
	if (this->imageSession != nullptr) {
		this->imageSession->setCurrentPatch(x, y);
	}
}

void ApplicationController::configurePatchGrid(int cols, int rows, int borderExtension)
{
	if (this->imageSession != nullptr) {
		this->imageSession->configurePatchGrid(cols, rows, borderExtension);
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
