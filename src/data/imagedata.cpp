#include "imagedata.h"
#include "utils/logging.h"
#include <QImage>
#include <QFile>
#include <QtMath>
#include <cstring>
#include <algorithm>

ImageData::ImageData(void* data, int width, int height, int frames, EnviDataType dataType,
					 const QVector<qreal>& wavelengths, const QString& wavelengthUnit, QObject* parent)
	: QObject(parent), width(width), height(height), frames(frames), dataType(dataType),
	  data(data), wavelengths(wavelengths), wavelengthUnit(wavelengthUnit)
{
	// Generate frame names for HSI data
	this->frameNames.reserve(this->frames);
	for (int i = 0; i < this->frames; ++i) {
		if (i < this->wavelengths.size()) {
			this->frameNames.append(QString::number(this->wavelengths[i]) + " " + this->wavelengthUnit);
		} else {
			this->frameNames.append(QString("Frame %1").arg(i));
		}
	}
}

ImageData::ImageData(const QImage& image, QObject* parent)
	: QObject(parent), width(image.width()), height(image.height()),
	  dataType(UNSIGNED_CHAR_8BIT), data(nullptr)
{
	if (image.isNull() || this->width <= 0 || this->height <= 0) {
		LOG_WARNING() << "Invalid image provided";
		return;
	}

	// Detect if image is grayscale
	bool isGrayscale = this->isGrayscaleImage(image);

	if (isGrayscale) {
		this->frames = 1;
		this->initializeGrayscaleMetadata();
		this->convertGrayscaleToFrame(image);
		LOG_DEBUG() << "Created single-frame grayscale ImageData";
	} else {
		this->frames = 3;
		this->initializeRGBMetadata();
		this->convertRGBToFrames(image);
		LOG_DEBUG() << "Created 3-frame RGB ImageData";
	}
}

ImageData::ImageData(const ImageData& other)
	: QObject(other.parent()), width(other.width), height(other.height), frames(other.frames),
	  dataType(other.dataType), data(nullptr), wavelengths(other.wavelengths),
	  wavelengthUnit(other.wavelengthUnit), frameNames(other.frameNames)
{
	// Deep copy data
	size_t totalBytes = static_cast<size_t>(this->width) * static_cast<size_t>(this->height) * static_cast<size_t>(this->frames) * this->calculateBytesPerSample();
	this->allocateDataBuffer(totalBytes);
	if (this->data != nullptr && other.data != nullptr) {
		std::memcpy(this->data, other.data, totalBytes);
	}
}

ImageData::~ImageData()
{
	if (this->data != nullptr) {
		free(this->data);
		this->data = nullptr;
	}
}

int ImageData::getWidth() const
{
	return this->width;
}

int ImageData::getHeight() const
{
	return this->height;
}

int ImageData::getFrames() const
{
	return this->frames;
}

EnviDataType ImageData::getDataType() const
{
	return this->dataType;
}

size_t ImageData::getBytesPerSample() const
{
	return this->calculateBytesPerSample();
}

void* ImageData::getData() const
{
	return this->data;
}

void* ImageData::getData(int frameNr) const
{
	if (frameNr < 0 || frameNr >= this->frames || this->data == nullptr) {
		return nullptr;
	}

	size_t bytesPerSample = this->calculateBytesPerSample();
	size_t bytesPerFrame = bytesPerSample * static_cast<size_t>(this->width) * static_cast<size_t>(this->height);
	return &static_cast<char*>(this->data)[bytesPerFrame * frameNr];
}

void ImageData::writeSingleFrame(int frameNr, void* data)
{
	if (frameNr < 0 || frameNr >= this->frames || this->data == nullptr || data == nullptr) {
		LOG_WARNING() << "Invalid frame write parameters - frameNr:" << frameNr << "frames:" << this->frames;
		return;
	}

	size_t bytesPerSample = this->calculateBytesPerSample();
	size_t bytesPerFrame = bytesPerSample * static_cast<size_t>(this->width) * static_cast<size_t>(this->height);
	void* framePosition = &static_cast<char*>(this->data)[bytesPerFrame * frameNr];
	std::memcpy(framePosition, data, bytesPerFrame);
	emit dataChanged();
}

void ImageData::writeSingleSubFrame(int frameNr, int x, int y, int width, int height, void* data)
{
	if (frameNr < 0 || frameNr >= this->frames || this->data == nullptr || data == nullptr) {
		LOG_WARNING() << "Invalid sub-frame write parameters - frameNr:" << frameNr << "frames:" << this->frames;
		return;
	}

	if (x < 0 || y < 0 || x + width > this->width || y + height > this->height) {
		LOG_WARNING() << "Sub-frame coordinates out of bounds - x:" << x << "y:" << y << "w:" << width << "h:" << height;
		return;
	}

	size_t bytesPerSample = this->calculateBytesPerSample();
	size_t bytesPerFrame = bytesPerSample * static_cast<size_t>(this->width) * static_cast<size_t>(this->height);
	char* frameStart = &static_cast<char*>(this->data)[bytesPerFrame * frameNr];

	size_t bytesPerFullLine = bytesPerSample * static_cast<size_t>(this->width);
	char* subFrameStart = &frameStart[bytesPerFullLine * y + x * bytesPerSample];

	size_t bytesPerSubLine = bytesPerSample * static_cast<size_t>(width);
	for (int i = 0; i < height; ++i) {
		std::memcpy(&subFrameStart[bytesPerFullLine * i],
					&static_cast<char*>(data)[bytesPerSubLine * i],
					bytesPerSubLine);
	}
	emit dataChanged();
}

QVector<qreal> ImageData::getWavelengths() const
{
	return this->wavelengths;
}

QString ImageData::getWavelengthUnit() const
{
	return this->wavelengthUnit;
}

QStringList ImageData::getFrameNames() const
{
	return this->frameNames;
}

// todo: check performance and maybe use openmp
// todo: check if data conversion is correct for each datatype
double ImageData::getMaxPixelValue()
{
	if (this->data == nullptr) {
		return 0.0;
	}

	double maxValue = 0.0;
	size_t totalSamples = static_cast<size_t>(this->width) * static_cast<size_t>(this->height) * static_cast<size_t>(this->frames);

	switch (this->dataType) {
		case UNSIGNED_CHAR_8BIT: {
			auto* typedData = static_cast<unsigned char*>(this->data);
			for (size_t i = 0; i < totalSamples; ++i) {
				maxValue = std::max(maxValue, static_cast<double>(typedData[i]));
			}
			break;
		}
		case SIGNED_SHORT_16BIT: {
			auto* typedData = static_cast<short*>(this->data);
			for (size_t i = 0; i < totalSamples; ++i) {
				maxValue = std::max(maxValue, static_cast<double>(typedData[i]));
			}
			break;
		}
		case UNSIGNED_SHORT_16BIT: {
			auto* typedData = static_cast<unsigned short*>(this->data);
			for (size_t i = 0; i < totalSamples; ++i) {
				maxValue = std::max(maxValue, static_cast<double>(typedData[i]));
			}
			break;
		}
		case SIGNED_INT_32BIT: {
			auto* typedData = static_cast<int*>(this->data);
			for (size_t i = 0; i < totalSamples; ++i) {
				maxValue = std::max(maxValue, static_cast<double>(typedData[i]));
			}
			break;
		}
		case UNSIGNED_INT_32BIT: {
			auto* typedData = static_cast<unsigned int*>(this->data);
			for (size_t i = 0; i < totalSamples; ++i) {
				maxValue = std::max(maxValue, static_cast<double>(typedData[i]));
			}
			break;
		}
		case FLOAT_32BIT: {
			auto* typedData = static_cast<float*>(this->data);
			for (size_t i = 0; i < totalSamples; ++i) {
				maxValue = std::max(maxValue, static_cast<double>(typedData[i]));
			}
			break;
		}
		case DOUBLE_64BIT: {
			auto* typedData = static_cast<double*>(this->data);
			for (size_t i = 0; i < totalSamples; ++i) {
				maxValue = std::max(maxValue, typedData[i]);
			}
			break;
		}
		case SIGNED_LONG_INT_64BIT: {
			auto* typedData = static_cast<long long*>(this->data);
			for (size_t i = 0; i < totalSamples; ++i) {
				maxValue = std::max(maxValue, static_cast<double>(typedData[i]));
			}
			break;
		}
		case UNSIGNED_LONG_INT_64BIT: {
			auto* typedData = static_cast<unsigned long long*>(this->data);
			for (size_t i = 0; i < totalSamples; ++i) {
				maxValue = std::max(maxValue, static_cast<double>(typedData[i]));
			}
			break;
		}
		default:
			LOG_WARNING() << "Unsupported data type for getMaxPixelValue:" << this->dataType;
			break;
	}

	return maxValue;
}

void ImageData::saveDataToDisk(const QString& filePath)
{
	if (this->data == nullptr) {
		LOG_WARNING() << "No data to save";
		return;
	}

	QFile outputFile(filePath);
	if (!outputFile.open(QIODevice::WriteOnly)) {
		LOG_WARNING() << "Could not write file to disk:" << outputFile.errorString();
		return;
	}

	size_t bytesPerSample = this->calculateBytesPerSample();
	size_t totalBytes = bytesPerSample * static_cast<size_t>(this->frames) * static_cast<size_t>(this->width) * static_cast<size_t>(this->height);
	qint64 bytesWritten = outputFile.write(static_cast<char*>(this->data), totalBytes);

	if (bytesWritten != static_cast<qint64>(totalBytes)) {
		LOG_WARNING() << "Failed to write all data to disk";
	} else {
		LOG_INFO() << "Data written to disk:" << filePath;
	}

	outputFile.close();
}

void ImageData::convertRGBToFrames(const QImage& rgbImage)
{
	// Convert to RGB32 format for consistent pixel access
	QImage convertedImage = rgbImage.convertToFormat(QImage::Format_RGB32);

	size_t samplesPerFrame = static_cast<size_t>(this->width) * static_cast<size_t>(this->height);
	size_t bytesPerSample = this->calculateBytesPerSample();
	size_t totalBytes = samplesPerFrame * 3 * bytesPerSample; // 3 frames (R, G, B)

	this->allocateDataBuffer(totalBytes);
	if (this->data == nullptr) {
		return;
	}

	auto* outputData = static_cast<unsigned char*>(this->data);
	const QRgb* pixels = reinterpret_cast<const QRgb*>(convertedImage.constBits());

	// Split RGB into separate frames
	for (int i = 0; i < this->width * this->height; ++i) {
		QRgb pixel = pixels[i];
		outputData[i] = qRed(pixel);                        // R frame
		outputData[samplesPerFrame + i] = qGreen(pixel);    // G frame
		outputData[2 * samplesPerFrame + i] = qBlue(pixel); // B frame
	}
}

void ImageData::convertGrayscaleToFrame(const QImage& grayscaleImage)
{
	// Validate that this method is only called for 8-bit data
	if (this->dataType != UNSIGNED_CHAR_8BIT) {
		LOG_WARNING() << "convertGrayscaleToFrame called with non-8-bit data type:" << this->dataType;
		LOG_WARNING() << "Use ArrayFire loading path for 16-bit images";
		return;
	}

	// Convert to grayscale format for consistent pixel access
	QImage convertedImage = grayscaleImage.convertToFormat(QImage::Format_Grayscale8);

	size_t samplesPerFrame = static_cast<size_t>(this->width) * static_cast<size_t>(this->height);
	size_t bytesPerSample = this->calculateBytesPerSample();
	size_t totalBytes = samplesPerFrame * bytesPerSample; // Single frame

	// Validate expected size
	if (bytesPerSample != 1) {
		LOG_WARNING() << "Expected 1 byte per sample for 8-bit data, got:" << bytesPerSample;
		return;
	}

	this->allocateDataBuffer(totalBytes);
	if (this->data == nullptr) {
		return;
	}

	auto* outputData = static_cast<unsigned char*>(this->data);
	const unsigned char* pixels = convertedImage.constBits();

	// Validate Qt image data size matches our expectations
	qint64 qtImageBytes = convertedImage.sizeInBytes();
	if (static_cast<size_t>(qtImageBytes) != totalBytes) {
		LOG_WARNING() << "Qt image size mismatch - expected:" << totalBytes << "got:" << qtImageBytes;
		return;
	}

	// Copy grayscale data directly
	std::memcpy(outputData, pixels, totalBytes);
}

void ImageData::initializeRGBMetadata()
{
	this->wavelengthUnit = "RGB";
	this->wavelengths = {0.0, 1.0, 2.0}; // Symbolic wavelengths for R, G, B
	this->frameNames = QStringList() << "Red" << "Green" << "Blue";
}

void ImageData::initializeGrayscaleMetadata()
{
	this->wavelengthUnit = "Grayscale";
	this->wavelengths = {0.0}; // Single frame
	this->frameNames = QStringList() << "Grayscale";
}

void ImageData::allocateDataBuffer(size_t totalBytes)
{
	if (this->data != nullptr) {
		free(this->data);
		this->data = nullptr;
	}

	this->data = malloc(totalBytes);
	if (this->data == nullptr) {
		LOG_ERROR() << "Failed to allocate memory for image data, size:" << totalBytes << "bytes";
	}
}

size_t ImageData::calculateBytesPerSample() const
{
	switch (this->dataType) {
		case UNSIGNED_CHAR_8BIT:
			return 1;
		case SIGNED_SHORT_16BIT:
		case UNSIGNED_SHORT_16BIT:
			return 2;
		case SIGNED_INT_32BIT:
		case UNSIGNED_INT_32BIT:
		case FLOAT_32BIT:
			return 4;
		case DOUBLE_64BIT:
		case SIGNED_LONG_INT_64BIT:
		case UNSIGNED_LONG_INT_64BIT:
			return 8;
		case COMPLEX_FLOAT_2X32BIT:
			return 8;
		case COMPLEX_DOUBLE_2X64BIT:
			return 16;
		default:
			LOG_WARNING() << "Unknown ENVI data type:" << this->dataType;
			return 4; // Default to float
	}
}

bool ImageData::isGrayscaleImage(const QImage& image) const
{
	if (image.isNull()) {
		return false;
	}

	// Check format first - some formats are inherently grayscale
	QImage::Format format = image.format();
	if (format == QImage::Format_Grayscale8 || format == QImage::Format_Mono ||
		format == QImage::Format_MonoLSB) {
		return true;
	}

	// For indexed formats, check if color table is grayscale
	if (format == QImage::Format_Indexed8) {
		QVector<QRgb> colorTable = image.colorTable();
		for (int i = 0; i < colorTable.size(); ++i) {
			QRgb color = colorTable[i];
			if (qRed(color) != qGreen(color) || qGreen(color) != qBlue(color)) {
				return false; // Found non-grayscale color
			}
		}
		return true; // All colors in table are grayscale
	}

	// For RGB formats, sample pixels to detect if image is effectively grayscale
	// Sample every 16th pixel for performance
	int sampleStep = 16;
	int width = image.width();
	int height = image.height();

	for (int y = 0; y < height; y += sampleStep) {
		for (int x = 0; x < width; x += sampleStep) {
			QRgb pixel = image.pixel(x, y);
			if (qRed(pixel) != qGreen(pixel) || qGreen(pixel) != qBlue(pixel)) {
				return false; // Found color pixel
			}
		}
	}

	return true; // All sampled pixels are grayscale
}
