#include "imagedataaccessor.h"
#include "patchlayout.h"
#include "patchextractor.h"
#include "utils/logging.h"
#include <QDebug>
#include <QRect>
#include <algorithm>

ImageDataAccessor::ImageDataAccessor(ImageData* imageData, bool readOnly, QObject* parent)
	: QObject(parent), imageData(imageData), readOnly(readOnly),
	  cachedFrameNumber(-1), frameModified(false),
	  patchGridCols(1), patchGridRows(1), patchBorderExtension(0),
	  tempBuffer(nullptr), tempBufferSize(0)
{
	if (this->imageData == nullptr) {
		LOG_WARNING() << ": Initialized with null ImageData";
	}
}

ImageDataAccessor::~ImageDataAccessor()
{
	// Write back any cached changes before destruction
	if (this->frameModified && !this->readOnly) {
		this->writeFrameFromCache();
	}

	// Clean up temporary buffer
	if (this->tempBuffer != nullptr) {
		free(this->tempBuffer);
		this->tempBuffer = nullptr;
	}
}

af::array ImageDataAccessor::getFrame(int frameNr)
{
	if (!this->isValid()) {
		LOG_WARNING() << ": Cannot get frame - invalid state";
		return af::array();
	}

	if (!this->isValidFrame(frameNr)) {
		LOG_WARNING() << ": Invalid frame number:" << frameNr;
		return af::array();
	}

	// Check if frame is already cached
	if (this->cachedFrameNumber == frameNr && !this->cachedFrame.isempty()) {
		return this->cachedFrame;
	}

	// Write back previous frame if modified
	if (this->frameModified && !this->readOnly) {
		this->writeFrameFromCache();
	}

	// Load new frame to cache
	this->loadFrameToCache(frameNr);
	return this->cachedFrame;
}

ImagePatch ImageDataAccessor::getExtendedPatch(int patchX, int patchY, int frameNr)
{
	if (!this->isValidPatchCoordinate(patchX, patchY)) {
		LOG_WARNING() << ": Invalid patch coordinates:" << patchX << "," << patchY;
		return ImagePatch();
	}

	af::array frame = this->getFrame(frameNr);
	if (frame.isempty()) {
		return ImagePatch();
	}

	const PatchLayout layout{
		this->imageData->getWidth(),
		this->imageData->getHeight(),
		this->patchGridCols,
		this->patchGridRows
	};
	return PatchExtractor::extractExtendedPatch(
		frame,
		layout,
		patchX,
		patchY,
		this->patchBorderExtension);
}

void ImageDataAccessor::writeFrame(int frameNr, const af::array& frameData)
{
	if (this->readOnly) {
		LOG_WARNING() << ": Cannot write frame - accessor is read-only";
		return;
	}

	if (!this->isValid()) {
		LOG_WARNING() << ": Cannot write frame - invalid state";
		return;
	}

	if (!this->isValidFrame(frameNr)) {
		LOG_WARNING() << ": Invalid frame number:" << frameNr;
		return;
	}

	// Check frame dimensions
	if (frameData.dims(0) != this->imageData->getWidth() || frameData.dims(1) != this->imageData->getHeight()) {
		LOG_WARNING() << ": Frame data dimensions don't match image dimensions";
		return;
	}

	try {
		// Update cache
		this->cachedFrame = frameData.copy();
		this->cachedFrameNumber = frameNr;
		this->frameModified = true;
		this->writeFrameFromCache();

		emit dataWritten(frameNr);
	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception during frame write:" << e.what();
	}
}

void ImageDataAccessor::writePatchResult(const ImagePatch& originalPatch, const af::array& processedData)
{
	if (!originalPatch.isValid()) {
		LOG_WARNING() << ": Cannot write patch result - invalid ImagePatch";
		return;
	}

	if (this->readOnly) {
		LOG_WARNING() << ": Cannot write patch result - accessor is read-only";
		return;
	}

	if (this->cachedFrameNumber < 0) {
		LOG_WARNING() << ": No cached frame available for patch result write";
		return;
	}

	if (!this->writePatchResultToCache(originalPatch, processedData)) {
		return;
	}

	this->writeFrameFromCache();
}

void ImageDataAccessor::writeMultiplePatches(int frameNr, const QList<QPoint>& patchCoords, const QList<af::array>& patchData)
{
	if (this->readOnly) {
		LOG_WARNING() << ": Cannot write patches - accessor is read-only";
		return;
	}

	if (patchCoords.size() != patchData.size()) {
		LOG_WARNING() << ": Patch coordinates and data count mismatch";
		return;
	}

	// Get current frame to cache
	af::array frame = this->getFrame(frameNr);
	if (frame.isempty()) {
		return;
	}

	try {
		// Update all patches in the cached frame
		for (int i = 0; i < patchCoords.size(); ++i) {
			const QPoint& coord = patchCoords[i];
			const af::array& patch = patchData[i];

			if (!this->isValidPatchCoordinate(coord.x(), coord.y())) {
				LOG_WARNING() << ": Skipping invalid patch coordinate:" << coord.x() << "," << coord.y();
				continue;
			}

			QRect coreBounds = this->calculateCorePatchBounds(coord.x(), coord.y());

			if (patch.dims(0) == coreBounds.width() && patch.dims(1) == coreBounds.height()) {
				this->cachedFrame(af::seq(coreBounds.x(), coreBounds.x() + coreBounds.width() - 1),
								  af::seq(coreBounds.y(), coreBounds.y() + coreBounds.height() - 1)) = patch;

				emit patchWritten(coord.x(), coord.y(), frameNr);
			} else {
				LOG_WARNING() << ": Skipping patch with incorrect dimensions at:" << coord.x() << "," << coord.y();
			}
		}

		this->frameModified = true;
		this->writeFrameFromCache();
	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception during multi-patch write:" << e.what();
	}
}

void ImageDataAccessor::writeMultiplePatchResults(
	int frameNr,
	const QList<ImagePatch>& originalPatches,
	const QList<af::array>& processedData)
{
	if (this->readOnly) {
		LOG_WARNING() << ": Cannot write patch results - accessor is read-only";
		return;
	}

	if (originalPatches.size() != processedData.size()) {
		LOG_WARNING() << ": Patch templates and processed data count mismatch";
		return;
	}

	af::array frame = this->getFrame(frameNr);
	if (frame.isempty()) {
		return;
	}

	for (int i = 0; i < originalPatches.size(); ++i) {
		if (!this->writePatchResultToCache(originalPatches[i], processedData[i])) {
			continue;
		}

		const QRect& patchBounds = originalPatches[i].imagePosition;
		emit patchWritten(patchBounds.x(), patchBounds.y(), frameNr);
	}

	this->writeFrameFromCache();
}

void ImageDataAccessor::flushFrame(int frameNr)
{
	if (this->readOnly || !this->frameModified) {
		return;
	}

	if (this->cachedFrameNumber == frameNr && !this->cachedFrame.isempty()) {
		this->writeFrameFromCache();
	}
}

void ImageDataAccessor::configurePatchGrid(int cols, int rows, int borderExtension)
{
	if (cols <= 0 || rows <= 0) {
		LOG_WARNING() << ": Invalid patch grid dimensions:" << cols << "x" << rows;
		return;
	}

	if (borderExtension < 0) {
		LOG_WARNING() << ": Border extension cannot be negative:" << borderExtension;
		return;
	}

	this->patchGridCols = cols;
	this->patchGridRows = rows;
	this->patchBorderExtension = borderExtension;
}

void ImageDataAccessor::clearCache()
{
	// Write back changes if any
	if (this->frameModified && !this->readOnly) {
		this->writeFrameFromCache();
	}

	this->cachedFrame = af::array();
	this->cachedFrameNumber = -1;
	this->frameModified = false;
}

bool ImageDataAccessor::isFrameCached(int frameNr) const
{
	return this->cachedFrameNumber == frameNr && !this->cachedFrame.isempty();
}

void ImageDataAccessor::loadFrameToCache(int frameNr)
{
	void* frameData = this->imageData->getData(frameNr);
	if (frameData == nullptr) {
		LOG_WARNING() << ": Failed to get frame data for frame:" << frameNr;
		return;
	}

	try {
		this->cachedFrame = this->convertToArrayFire(frameData, this->imageData->getWidth(), this->imageData->getHeight());
		this->cachedFrameNumber = frameNr;
		this->frameModified = false;

		emit frameLoaded(frameNr);
	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception during frame loading:" << e.what();
		this->cachedFrame = af::array();
		this->cachedFrameNumber = -1;
	}
}

void ImageDataAccessor::writeFrameFromCache()
{
	if (this->cachedFrame.isempty() || this->cachedFrameNumber < 0 || !this->frameModified) {
		return;
	}

	int width = this->imageData->getWidth();
	int height = this->imageData->getHeight();
	size_t frameSize = static_cast<size_t>(width) * static_cast<size_t>(height) * this->imageData->getBytesPerSample();

	// Ensure temp buffer is large enough
	this->ensureTempBuffer(frameSize);

	try {
		// Convert from ArrayFire to native format
		this->convertFromArrayFire(this->cachedFrame, this->tempBuffer, width, height);

		// Write to ImageData
		this->imageData->writeSingleFrame(this->cachedFrameNumber, this->tempBuffer);
		this->frameModified = false;

		LOG_DEBUG() << ": Flushed frame" << this->cachedFrameNumber << "to ImageData";
	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception during frame flush:" << e.what();
	}
}

void ImageDataAccessor::ensureTempBuffer(size_t requiredSize)
{
	if (this->tempBufferSize < requiredSize) {
		if (this->tempBuffer != nullptr) {
			free(this->tempBuffer);
		}

		this->tempBuffer = malloc(requiredSize);
		if (this->tempBuffer == nullptr) {
			LOG_ERROR() << ": Failed to allocate temporary buffer, size:" << requiredSize;
			this->tempBufferSize = 0;
		} else {
			this->tempBufferSize = requiredSize;
		}
	}
}

QRect ImageDataAccessor::calculateCorePatchBounds(int patchX, int patchY) const
{
	if (!this->isValid()) return QRect();
	const PatchLayout layout{ this->imageData->getWidth(), this->imageData->getHeight(),
	                          this->patchGridCols, this->patchGridRows };
	return layout.patchBounds(patchX, patchY);
}

void ImageDataAccessor::writePatchAtPosition(const QRect& imagePos, const af::array& patchData)
{
	// Get current cached frame
	af::array frame = this->getFrame(this->cachedFrameNumber);
	if (frame.isempty()) {
		LOG_WARNING() << ": No cached frame available for patch write";
		return;
	}

	// Validate position
	int frameWidth = static_cast<int>(frame.dims(0));
	int frameHeight = static_cast<int>(frame.dims(1));

	if (imagePos.x() < 0 || imagePos.y() < 0 ||
		imagePos.x() + imagePos.width() > frameWidth ||
		imagePos.y() + imagePos.height() > frameHeight) {
		LOG_WARNING() << ": Patch position out of bounds:" << imagePos << "frame size:" << frameWidth << "x" << frameHeight;
		return;
	}

	// Check patch dimensions
	if (patchData.dims(0) != imagePos.width() || patchData.dims(1) != imagePos.height()) {
		LOG_WARNING() << ": Patch data dimensions don't match position:"
					  << patchData.dims(0) << "x" << patchData.dims(1) << "vs" << imagePos.width() << "x" << imagePos.height();
		return;
	}

	try {
		// Update the cached frame at the specified position
		this->cachedFrame(af::seq(imagePos.x(), imagePos.x() + imagePos.width() - 1),
						  af::seq(imagePos.y(), imagePos.y() + imagePos.height() - 1)) = patchData;

		this->frameModified = true;

	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception during position-based patch write:" << e.what();
	}
}

bool ImageDataAccessor::writePatchResultToCache(
	const ImagePatch& originalPatch,
	const af::array& processedData)
{
	if (!originalPatch.isValid() || processedData.isempty()) {
		return false;
	}

	const BorderExtension& borders = originalPatch.borders;
	const int expansion = borders.extensionSize;

	const long long startX = borders.leftExtended ? expansion : 0;
	const long long startY = borders.topExtended ? expansion : 0;
	const long long endX = processedData.dims(0) - (borders.rightExtended ? expansion : 0);
	const long long endY = processedData.dims(1) - (borders.bottomExtended ? expansion : 0);

	if (startX >= endX || startY >= endY) {
		LOG_WARNING() << ": Invalid processed patch bounds for write:" << originalPatch.imagePosition;
		return false;
	}

	try {
		af::array coreResult = processedData(
			af::seq(startX, endX - 1),
			af::seq(startY, endY - 1));
		this->writePatchAtPosition(originalPatch.imagePosition, coreResult);

		LOG_DEBUG() << ": Wrote patch result - core area" << originalPatch.imagePosition
					<< " from processed size:" << processedData.dims(0) << "x" << processedData.dims(1)
					<< " extracted:" << startX << "," << startY << " to " << endX - 1 << "," << endY - 1;
		return true;
	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception during patch result write:" << e.what();
		return false;
	}
}

af::array ImageDataAccessor::convertToArrayFire(void* data, int width, int height) const
{
	if (data == nullptr || width <= 0 || height <= 0 || !this->isValid()) {
		LOG_WARNING() << ": Invalid parameters for ArrayFire conversion";
		return af::array();
	}

	try {
		EnviDataType dataType = this->imageData->getDataType();

		// Optimized type conversion based on ENVI data type
		switch (dataType) {
			case UNSIGNED_CHAR_8BIT:
				return af::array(width, height, static_cast<unsigned char*>(data)).as(af::dtype::f32);

			case SIGNED_SHORT_16BIT:
				return af::array(width, height, static_cast<short*>(data)).as(af::dtype::f32);

			case UNSIGNED_SHORT_16BIT:
				return af::array(width, height, static_cast<unsigned short*>(data)).as(af::dtype::f32);

			case SIGNED_INT_32BIT:
				return af::array(width, height, static_cast<int*>(data)).as(af::dtype::f32);

			case UNSIGNED_INT_32BIT:
				return af::array(width, height, static_cast<unsigned int*>(data)).as(af::dtype::f32);

			case FLOAT_32BIT:
				return af::array(width, height, static_cast<float*>(data));

			case DOUBLE_64BIT:
				return af::array(width, height, static_cast<double*>(data)).as(af::dtype::f32);

			case SIGNED_LONG_INT_64BIT:
				return af::array(width, height, static_cast<long long*>(data)).as(af::dtype::f32);

			case UNSIGNED_LONG_INT_64BIT:
				return af::array(width, height, static_cast<unsigned long long*>(data)).as(af::dtype::f32);

			default:
				LOG_WARNING() << ": Unsupported ENVI data type:" << dataType;
				return af::array();
		}
	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception during conversion:" << e.what();
		return af::array();
	}
}

void ImageDataAccessor::convertFromArrayFire(const af::array& afData, void* data, int width, int height) const
{
	if (data == nullptr || width <= 0 || height <= 0 || !this->isValid()) {
		LOG_WARNING() << ": Invalid parameters for ArrayFire back-conversion";
		return;
	}

	try {
		EnviDataType dataType = this->imageData->getDataType();
		size_t arraySize = static_cast<size_t>(width) * static_cast<size_t>(height);

		// Optimized back-conversion with proper clamping
		switch (dataType) {
			case UNSIGNED_CHAR_8BIT: {
				af::array converted = af::clamp(afData, 0.0, 255.0).as(af::dtype::u8);
				unsigned char* hostData = converted.host<unsigned char>();
				std::memcpy(data, hostData, arraySize * sizeof(unsigned char));
				af::freeHost(hostData);
				break;
			}

			case SIGNED_SHORT_16BIT: {
				af::array converted = af::clamp(afData, -32768.0, 32767.0).as(af::dtype::s16);
				short* hostData = converted.host<short>();
				std::memcpy(data, hostData, arraySize * sizeof(short));
				af::freeHost(hostData);
				break;
			}

			case UNSIGNED_SHORT_16BIT: {
				af::array converted = af::clamp(afData, 0.0, 65535.0).as(af::dtype::u16);
				unsigned short* hostData = converted.host<unsigned short>();
				std::memcpy(data, hostData, arraySize * sizeof(unsigned short));
				af::freeHost(hostData);
				break;
			}

			case SIGNED_INT_32BIT: {
				af::array converted = afData.as(af::dtype::s32);
				int* hostData = converted.host<int>();
				std::memcpy(data, hostData, arraySize * sizeof(int));
				af::freeHost(hostData);
				break;
			}

			case UNSIGNED_INT_32BIT: {
				af::array converted = afData.as(af::dtype::u32);
				unsigned int* hostData = converted.host<unsigned int>();
				std::memcpy(data, hostData, arraySize * sizeof(unsigned int));
				af::freeHost(hostData);
				break;
			}

			case FLOAT_32BIT: {
				af::array converted = afData.as(af::dtype::f32);
				float* hostData = converted.host<float>();
				std::memcpy(data, hostData, arraySize * sizeof(float));
				af::freeHost(hostData);
				break;
			}

			case DOUBLE_64BIT: {
				af::array converted = afData.as(af::dtype::f64);
				double* hostData = converted.host<double>();
				std::memcpy(data, hostData, arraySize * sizeof(double));
				af::freeHost(hostData);
				break;
			}

			case SIGNED_LONG_INT_64BIT: {
				af::array converted = afData.as(af::dtype::s64);
				long long* hostData = converted.host<long long>();
				std::memcpy(data, hostData, arraySize * sizeof(long long));
				af::freeHost(hostData);
				break;
			}

			case UNSIGNED_LONG_INT_64BIT: {
				af::array converted = afData.as(af::dtype::u64);
				unsigned long long* hostData = converted.host<unsigned long long>();
				std::memcpy(data, hostData, arraySize * sizeof(unsigned long long));
				af::freeHost(hostData);
				break;
			}

			default:
				LOG_WARNING() << ": Unsupported ENVI data type for back-conversion:" << dataType;
				break;
		}
	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception during back-conversion:" << e.what();
	}
}

// Getters implementation
int ImageDataAccessor::getPatchGridCols() const { return this->patchGridCols; }
int ImageDataAccessor::getPatchGridRows() const { return this->patchGridRows; }
int ImageDataAccessor::getPatchBorderExtension() const { return this->patchBorderExtension; }

QRect ImageDataAccessor::getCorePatchBounds(int patchX, int patchY) const
{
	return this->calculateCorePatchBounds(patchX, patchY);
}

int ImageDataAccessor::getWidth() const { return this->isValid() ? this->imageData->getWidth() : 0; }
int ImageDataAccessor::getHeight() const { return this->isValid() ? this->imageData->getHeight() : 0; }
int ImageDataAccessor::getFrames() const { return this->isValid() ? this->imageData->getFrames() : 0; }
bool ImageDataAccessor::isReadOnly() const { return this->readOnly; }
bool ImageDataAccessor::isValid() const { return this->imageData != nullptr; }

bool ImageDataAccessor::isValidPatchCoordinate(int patchX, int patchY) const
{
	return patchX >= 0 && patchX < this->patchGridCols &&
		   patchY >= 0 && patchY < this->patchGridRows;
}

bool ImageDataAccessor::isValidFrame(int frameNr) const
{
	return this->isValid() && frameNr >= 0 && frameNr < this->imageData->getFrames();
}
