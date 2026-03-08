#include "imagesession.h"
#include "utils/afdevicemanager.h"
#include "utils/logging.h"
#include <QFileInfo>

namespace {
	const int DEFAULT_PATCH_GRID_COLS = 1;
	const int DEFAULT_PATCH_GRID_ROWS = 1;
	const int DEFAULT_BORDER_EXTENSION = 10;
	const int INVALID_FRAME = -1;
	const int INVALID_PATCH_COORD = -1;
}

ImageSession::ImageSession(AFDeviceManager* afDeviceManager, QObject* parent)
	: QObject(parent),
	  inputData(nullptr), outputData(nullptr), groundTruthData(nullptr),
	  inputAccessor(nullptr), outputAccessor(nullptr), groundTruthAccessor(nullptr),
	  currentFrame(INVALID_FRAME), currentPatch(INVALID_PATCH_COORD, INVALID_PATCH_COORD),
	  patchGridCols(DEFAULT_PATCH_GRID_COLS), patchGridRows(DEFAULT_PATCH_GRID_ROWS),
	  patchBorderExtension(DEFAULT_BORDER_EXTENSION)
{
	connect(afDeviceManager, &AFDeviceManager::aboutToChangeDevice,
			this, &ImageSession::clearAFCaches);
}

ImageSession::~ImageSession()
{
	this->clearAllData();
}

void ImageSession::setInputData(ImageData* inputData)
{
	if (inputData == nullptr) {
		LOG_WARNING() << "Attempted to set null input data";
		return;
	}

	LOG_INFO() << "Setting new input data" << inputData->getWidth() << "x" << inputData->getHeight() << "x" << inputData->getFrames();

	// Validate compatibility with existing ground truth
	if (this->groundTruthData != nullptr) {
		try {
			this->validateDataCompatibility(inputData, "input (checking ground truth compatibility)");
		} catch (const QString& error) {
			LOG_WARNING() << "Input data incompatible with existing ground truth:" << error;
			// Continue anyway - user might want to replace ground truth later
		}
	}

	int previousFrame = this->currentFrame;

	// Delete old input and output data
	this->deleteInputAndOutputData();

	// Set new input data with Qt ownership
	this->inputData = inputData;
	this->inputData->setParent(this);

	// Create output data as copy of input
	this->createOutputDataFromInput();

	// Create new accessors
	this->inputAccessor = new ImageDataAccessor(this->inputData, true, this);  // read-only
	this->outputAccessor = new ImageDataAccessor(this->outputData, false, this);  // writable

	// Apply current patch grid configuration
	this->updateAccessorConfigurations();

	// Preserve the current frame if the new data has enough frames; otherwise reset to 0.
	if (this->inputData->getFrames() == 0) {
		this->currentFrame = INVALID_FRAME;
	} else if (previousFrame > 0 && previousFrame < this->inputData->getFrames()) {
		this->currentFrame = previousFrame;
	} else {
		this->currentFrame = 0;
	}
	if (!this->isValidPatch(this->currentPatch.x(), this->currentPatch.y())) {
		this->currentPatch = QPoint(0, 0);
	}

	emit inputDataChanged();
	emit outputDataChanged();
	if (this->currentFrame != INVALID_FRAME) {
		emit frameChanged(this->currentFrame);
	}
	emit patchChanged(this->currentPatch.x(), this->currentPatch.y());

	LOG_INFO() << "Input data set successfully, current frame:" << this->currentFrame;
}

void ImageSession::setGroundTruthData(ImageData* groundTruthData)
{
	if (groundTruthData == nullptr) {
		LOG_ERROR() << "Attempted to set null ground truth data";
		return;
	}

	LOG_INFO() << "Setting ground truth data" << groundTruthData->getWidth() << "x" << groundTruthData->getHeight() << "x" << groundTruthData->getFrames();

	// Validate compatibility with existing input data
	if (this->inputData != nullptr) {
		try {
			this->validateDataCompatibility(groundTruthData, "ground truth");
		} catch (const QString& error) {
			QString message = tr("Ground truth data incompatible with input data: %1\n\nInput: %2x%3x%4 frames\nGround truth: %5x%6x%7 frames")
								.arg(error)
								.arg(this->inputData->getWidth())
								.arg(this->inputData->getHeight())
								.arg(this->inputData->getFrames())
								.arg(groundTruthData->getWidth())
								.arg(groundTruthData->getHeight())
								.arg(groundTruthData->getFrames());
			LOG_ERROR() << message;
			return;
		}
	}

	// Delete old ground truth data
	if (this->groundTruthData != nullptr) {
		this->groundTruthData->deleteLater();
		this->groundTruthData = nullptr;
	}
	if (this->groundTruthAccessor != nullptr) {
		this->groundTruthAccessor->deleteLater();
		this->groundTruthAccessor = nullptr;
	}

	// Set new ground truth data
	this->groundTruthData = groundTruthData;
	this->groundTruthData->setParent(this);

	// Create ground truth accessor (read-only)
	this->groundTruthAccessor = new ImageDataAccessor(this->groundTruthData, true, this);

	// Apply current patch grid configuration
	if (this->inputData != nullptr) {
		this->updateAccessorConfigurations();
	}

	emit groundTruthDataChanged();
	LOG_INFO() << "Ground truth data set successfully";
}

void ImageSession::clearAllData()
{
	// Clear accessors first
	if (this->inputAccessor != nullptr) {
		this->inputAccessor->deleteLater();
		this->inputAccessor = nullptr;
	}
	if (this->outputAccessor != nullptr) {
		this->outputAccessor->deleteLater();
		this->outputAccessor = nullptr;
	}
	if (this->groundTruthAccessor != nullptr) {
		this->groundTruthAccessor->deleteLater();
		this->groundTruthAccessor = nullptr;
	}

	// Clear data objects (Qt parent-child will handle deletion)
	if (this->inputData != nullptr) {
		this->inputData->deleteLater();
		this->inputData = nullptr;
	}
	if (this->outputData != nullptr) {
		this->outputData->deleteLater();
		this->outputData = nullptr;
	}
	if (this->groundTruthData != nullptr) {
		this->groundTruthData->deleteLater();
		this->groundTruthData = nullptr;
	}

	// Reset state
	this->currentFrame = INVALID_FRAME;
	this->currentPatch = QPoint(INVALID_PATCH_COORD, INVALID_PATCH_COORD);
}

void ImageSession::setCurrentFrame(int frame)
{
	if (!this->isValidFrame(frame)) {
		LOG_WARNING() << "Invalid frame:" << frame << "valid range: 0 -" << (this->hasInputData() ? this->inputData->getFrames() - 1 : -1);
		return;
	}

	if (this->currentFrame != frame) {
		this->currentFrame = frame;
		emit frameChanged(frame);
	}
}

void ImageSession::setCurrentPatch(int x, int y)
{
	if (!this->isValidPatch(x, y)) {
		LOG_WARNING() << "Invalid patch coordinates:" << x << "," << y << "valid range: 0-" << (this->patchGridCols - 1) << ", 0-" << (this->patchGridRows - 1);
		return;
	}

	QPoint newPatch(x, y);
	if (this->currentPatch != newPatch) {
		this->currentPatch = newPatch;
		emit patchChanged(x, y);
	}
}

void ImageSession::configurePatchGrid(int cols, int rows, int borderExtension)
{
	if (cols <= 0 || rows <= 0) {
		LOG_WARNING() << "Invalid patch grid dimensions:" << cols << "x" << rows;
		return;
	}

	if (borderExtension < 0) {
		LOG_WARNING() << "Border extension cannot be negative:" << borderExtension;
		return;
	}

	this->patchGridCols = cols;
	this->patchGridRows = rows;
	this->patchBorderExtension = borderExtension;

	// Update accessor configurations
	this->updateAccessorConfigurations();

	// Validate current patch is still valid
	if (!this->isValidPatch(this->currentPatch.x(), this->currentPatch.y())) {
		this->currentPatch = QPoint(0, 0);
		emit patchChanged(0, 0);
	}

	emit patchGridConfigured(cols, rows, borderExtension);
}

ImagePatch ImageSession::getCurrentInputPatch()
{
	if (!this->hasInputData() || this->inputAccessor == nullptr) {
		LOG_WARNING() << "No input data available for patch access";
		return ImagePatch();
	}

	if (!this->isValidFrame(this->currentFrame) || !this->isValidPatch(this->currentPatch.x(), this->currentPatch.y())) {
		LOG_WARNING() << "Invalid current frame or patch for input access";
		return ImagePatch();
	}

	return this->inputAccessor->getExtendedPatch(this->currentPatch.x(), this->currentPatch.y(), this->currentFrame);
}

ImagePatch ImageSession::getCurrentOutputPatch()
{
	if (!this->hasOutputData() || this->outputAccessor == nullptr) {
		LOG_WARNING() << "No output data available for patch access";
		return ImagePatch();
	}

	if (!this->isValidFrame(this->currentFrame) || !this->isValidPatch(this->currentPatch.x(), this->currentPatch.y())) {
		LOG_WARNING() << "Invalid current frame or patch for output access";
		return ImagePatch();
	}

	return this->outputAccessor->getExtendedPatch(this->currentPatch.x(), this->currentPatch.y(), this->currentFrame);
}

ImagePatch ImageSession::getCurrentGroundTruthPatch()
{
	if (!this->hasGroundTruthData() || this->groundTruthAccessor == nullptr) {
		LOG_WARNING() << "No ground truth data available for patch access";
		return ImagePatch();
	}

	if (!this->isValidPatch(this->currentPatch.x(), this->currentPatch.y())) {
		LOG_WARNING() << "Invalid current patch for ground truth access";
		return ImagePatch();
	}

	int groundTruthFrame = this->getGroundTruthFrameForCurrentFrame();
	return this->groundTruthAccessor->getExtendedPatch(this->currentPatch.x(), this->currentPatch.y(), groundTruthFrame);
}

ImagePatch ImageSession::getInputPatch(int frameNr, int patchX, int patchY)
{
	if (!this->hasInputData() || this->inputAccessor == nullptr) {
		return ImagePatch();
	}
	if (!this->isValidFrame(frameNr) || !this->isValidPatch(patchX, patchY)) {
		return ImagePatch();
	}
	return this->inputAccessor->getExtendedPatch(patchX, patchY, frameNr);
}

ImagePatch ImageSession::getGroundTruthPatch(int frameNr, int patchX, int patchY)
{
	if (!this->hasGroundTruthData() || this->groundTruthAccessor == nullptr) {
		return ImagePatch();
	}
	if (!this->isValidPatch(patchX, patchY)) {
		return ImagePatch();
	}
	// Map frame to ground truth frame (single-frame GT → always 0)
	int gtFrames = this->groundTruthData->getFrames();
	int gtFrame = (gtFrames == 1) ? 0 : qBound(0, frameNr, gtFrames - 1);
	return this->groundTruthAccessor->getExtendedPatch(patchX, patchY, gtFrame);
}

void ImageSession::setCurrentOutputPatch(const af::array& data)
{
	if (!this->hasOutputData() || this->outputAccessor == nullptr) {
		LOG_WARNING() << "No output data available for patch writing";
		return;
	}

	if (!this->isValidFrame(this->currentFrame) || !this->isValidPatch(this->currentPatch.x(), this->currentPatch.y())) {
		LOG_WARNING() << "Invalid current frame or patch for output writing";
		return;
	}

	// Get current output patch to use its border information
	ImagePatch currentPatch = this->getCurrentOutputPatch();
	if (!currentPatch.isValid()) {
		LOG_WARNING() << "Could not get current output patch for writing";
		return;
	}

	// Write the processed data back
	this->outputAccessor->writePatchResult(currentPatch, data);
	LOG_DEBUG() << "Current output patch updated";
	emit outputPatchUpdated();
}

void ImageSession::setOutputPatch(int frameNr, int patchX, int patchY, const af::array& processedExtendedData)
{
	if (!this->hasOutputData() || !this->hasInputData() || this->outputAccessor == nullptr || this->inputAccessor == nullptr) {
		LOG_WARNING() << "No data available for batch output writing";
		return;
	}

	if (!this->isValidFrame(frameNr) || !this->isValidPatch(patchX, patchY)) {
		LOG_WARNING() << "Invalid frame or patch for output writing:" << frameNr << patchX << patchY;
		return;
	}

	// Get input patch to obtain border info and absolute position
	ImagePatch inputPatch = this->inputAccessor->getExtendedPatch(patchX, patchY, frameNr);
	if (!inputPatch.isValid()) {
		LOG_WARNING() << "Could not get input patch for batch output writing";
		return;
	}

	// Ensure the output accessor has the correct frame cached
	// (getFrame flushes any previously modified frame automatically)
	this->outputAccessor->getFrame(frameNr);

	// Write the processed data using input patch's position/border info
	this->outputAccessor->writePatchResult(inputPatch, processedExtendedData);
}

void ImageSession::flushOutput()
{
}

af::array ImageSession::getCurrentInputFrame()
{
	if (!this->hasInputData() || this->inputAccessor == nullptr) {
		LOG_WARNING() << "No input data available for frame access";
		return af::array();
	}

	if (!this->isValidFrame(this->currentFrame)) {
		LOG_WARNING() << "Invalid current frame for input access";
		return af::array();
	}

	return this->inputAccessor->getFrame(this->currentFrame);
}

af::array ImageSession::getCurrentOutputFrame()
{
	if (!this->hasOutputData() || this->outputAccessor == nullptr) {
		LOG_WARNING() << "No output data available for frame access";
		return af::array();
	}

	if (!this->isValidFrame(this->currentFrame)) {
		LOG_WARNING() << "Invalid current frame for output access";
		return af::array();
	}

	return this->outputAccessor->getFrame(this->currentFrame);
}

af::array ImageSession::getCurrentGroundTruthFrame()
{
	if (!this->hasGroundTruthData() || this->groundTruthAccessor == nullptr) {
		LOG_WARNING() << "No ground truth data available for frame access";
		return af::array();
	}

	int groundTruthFrame = this->getGroundTruthFrameForCurrentFrame();
	return this->groundTruthAccessor->getFrame(groundTruthFrame);
}

void ImageSession::setCurrentOutputFrame(const af::array& frameData)
{
	if (!this->hasOutputData() || this->outputAccessor == nullptr) {
		LOG_WARNING() << "No output data available for frame writing";
		return;
	}

	if (!this->isValidFrame(this->currentFrame)) {
		LOG_WARNING() << "Invalid current frame for output writing";
		return;
	}

	this->outputAccessor->writeFrame(this->currentFrame, frameData);
	LOG_DEBUG() << "Current output frame updated";
}

// State getters
int ImageSession::getCurrentFrame() const { return this->currentFrame; }
QPoint ImageSession::getCurrentPatch() const { return this->currentPatch; }
int ImageSession::getPatchGridCols() const { return this->patchGridCols; }
int ImageSession::getPatchGridRows() const { return this->patchGridRows; }
int ImageSession::getPatchBorderExtension() const { return this->patchBorderExtension; }

// Data information
bool ImageSession::hasInputData() const { return this->inputData != nullptr; }
bool ImageSession::hasOutputData() const { return this->outputData != nullptr; }
bool ImageSession::hasGroundTruthData() const { return this->groundTruthData != nullptr; }

// Direct data access for viewers
const ImageData* ImageSession::getInputData() const { return this->inputData; }
const ImageData* ImageSession::getOutputData() const { return this->outputData; }
const ImageData* ImageSession::getGroundTruthData() const { return this->groundTruthData; }

int ImageSession::getInputWidth() const { return this->hasInputData() ? this->inputData->getWidth() : 0; }
int ImageSession::getInputHeight() const { return this->hasInputData() ? this->inputData->getHeight() : 0; }
int ImageSession::getInputFrames() const { return this->hasInputData() ? this->inputData->getFrames() : 0; }

int ImageSession::getOutputWidth() const { return this->hasOutputData() ? this->outputData->getWidth() : 0; }
int ImageSession::getOutputHeight() const { return this->hasOutputData() ? this->outputData->getHeight() : 0; }
int ImageSession::getOutputFrames() const { return this->hasOutputData() ? this->outputData->getFrames() : 0; }

int ImageSession::getGroundTruthWidth() const { return this->hasGroundTruthData() ? this->groundTruthData->getWidth() : 0; }
int ImageSession::getGroundTruthHeight() const { return this->hasGroundTruthData() ? this->groundTruthData->getHeight() : 0; }
int ImageSession::getGroundTruthFrames() const { return this->hasGroundTruthData() ? this->groundTruthData->getFrames() : 0; }

void ImageSession::saveOutputToFile(const QString& filePath, int currentFrame)
{
	if (this->outputData == nullptr) {
		LOG_WARNING() << "No output data to save";
		return;
	}

	// Detect format from extension
	QFileInfo fileInfo(filePath);
	QString suffix = fileInfo.suffix().toLower();

	if (suffix == "dat" || suffix == "raw" || suffix == "img" || suffix == "bin" || suffix == "hdr") {
		this->outputData->saveAsEnvi(filePath);
	} else if (suffix == "tif" || suffix == "tiff") {
		this->outputData->saveAsTiff(filePath);
	} else {
		// Standard image format (png, bmp, jpg, etc.) single frame, 8-bit
		this->outputData->saveFrameAsImage(filePath, currentFrame);
	}
}

void ImageSession::clearAFCaches()
{
	if (this->inputAccessor) this->inputAccessor->clearCache();
	if (this->outputAccessor) this->outputAccessor->clearCache();
	if (this->groundTruthAccessor) this->groundTruthAccessor->clearCache();
}

bool ImageSession::isValidFrame(int frame) const
{
	return this->hasInputData() && frame >= 0 && frame < this->inputData->getFrames();
}

bool ImageSession::isValidPatch(int x, int y) const
{
	return x >= 0 && x < this->patchGridCols && y >= 0 && y < this->patchGridRows;
}

void ImageSession::createOutputDataFromInput()
{
	if (this->inputData == nullptr) {
		LOG_WARNING() << "Cannot create output data - no input data available";
		return;
	}

	// Create output data as copy of input data
	this->outputData = new ImageData(*this->inputData);
	this->outputData->setParent(this);

	LOG_DEBUG() << "Output data created as copy of input data";
}

void ImageSession::deleteInputAndOutputData()
{
	// Delete accessors first
	if (this->inputAccessor != nullptr) {
		this->inputAccessor->deleteLater();
		this->inputAccessor = nullptr;
	}
	if (this->outputAccessor != nullptr) {
		this->outputAccessor->deleteLater();
		this->outputAccessor = nullptr;
	}

	// Delete data objects
	if (this->inputData != nullptr) {
		this->inputData->deleteLater();
		this->inputData = nullptr;
	}
	if (this->outputData != nullptr) {
		this->outputData->deleteLater();
		this->outputData = nullptr;
	}

	LOG_DEBUG() << "Input and output data deleted";
}

void ImageSession::validateDataCompatibility(ImageData* newData, const QString& dataType) const
{
	if (newData == nullptr) {
		throw QString("Data is null");
	}

	// If we have input data, validate dimensions
	if (this->inputData != nullptr) {
		if (newData->getWidth() != this->inputData->getWidth() ||
			newData->getHeight() != this->inputData->getHeight()) {
			throw QString("Dimensions mismatch.\nExpected %1x%2, got %3x%4")
				.arg(this->inputData->getWidth())
				.arg(this->inputData->getHeight())
				.arg(newData->getWidth())
				.arg(newData->getHeight());
		}

		// For ground truth, allow single frame or same frame count
		if (dataType.contains("ground truth")) {
			int newFrames = newData->getFrames();
			int inputFrames = this->inputData->getFrames();
			if (newFrames != 1 && newFrames != inputFrames) {
				throw QString("Frame count mismatch - expected 1 or %1, got %2")
					.arg(inputFrames)
					.arg(newFrames);
			}
		} else {
			// For input data, frames must match exactly if we have ground truth
			if (this->groundTruthData != nullptr) {
				int groundTruthFrames = this->groundTruthData->getFrames();
				if (groundTruthFrames != 1 && groundTruthFrames != newData->getFrames()) {
					throw QString("Frame count incompatible with ground truth - ground truth has %1 frames, new data has %2")
						.arg(groundTruthFrames)
						.arg(newData->getFrames());
				}
			}
		}
	}

	LOG_DEBUG() << "Data compatibility validated for" << dataType;
}

void ImageSession::updateAccessorConfigurations()
{
	if (this->inputAccessor != nullptr) {
		this->inputAccessor->configurePatchGrid(this->patchGridCols, this->patchGridRows, this->patchBorderExtension);
	}
	if (this->outputAccessor != nullptr) {
		this->outputAccessor->configurePatchGrid(this->patchGridCols, this->patchGridRows, this->patchBorderExtension);
	}
	if (this->groundTruthAccessor != nullptr) {
		this->groundTruthAccessor->configurePatchGrid(this->patchGridCols, this->patchGridRows, this->patchBorderExtension);
	}

	LOG_DEBUG() << "Accessor configurations updated";
}

int ImageSession::getGroundTruthFrameForCurrentFrame() const
{
	if (!this->hasGroundTruthData()) {
		return 0;
	}

	int groundTruthFrames = this->groundTruthData->getFrames();

	// If ground truth has only one frame, always use frame 0
	if (groundTruthFrames == 1) {
		return 0;
	}

	// If ground truth has same number of frames as input, use current frame
	if (this->hasInputData() && groundTruthFrames == this->inputData->getFrames()) {
		return qBound(0, this->currentFrame, groundTruthFrames - 1);
	}

	// Default to frame 0
	return 0;
}