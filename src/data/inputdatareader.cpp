#include "inputdatareader.h"
#include "utils/logging.h"
#ifdef WAVEPSF_LIBTIFF_BACKEND
#include "tiffio.h"
#endif
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QDataStream>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <cstring>
#include "utils/supportedfilechecker.h"
#include "omp.h"

InputDataReader::InputDataReader(QObject* parent)
	: QObject(parent)
{
}

InputDataReader::~InputDataReader()
{
}

ImageData* InputDataReader::loadFile(const QString& filePath)
{
	QFileInfo fileInfo(filePath);
	if (!fileInfo.exists() || !fileInfo.isReadable()) {
		LOG_WARNING() << "File does not exist or is not readable:" << filePath;
		return nullptr;
	}

	ImageData* result = nullptr;

	if (SupportedFileChecker::isEnviFile(filePath)) {
		LOG_INFO() << tr("Loading ENVI file: ") << filePath;
		// For ENVI data files, we need to load via the .hdr file
		QString hdrFilePath = filePath;
		if (!filePath.endsWith(".hdr", Qt::CaseInsensitive)) {
			// Convert data file path to .hdr file path
			hdrFilePath = QDir(fileInfo.absolutePath()).filePath(fileInfo.baseName()) + ".hdr";
		}
		result = this->loadEnviFiles(hdrFilePath);
	}
	else if (SupportedFileChecker::isStandardImageFile(filePath)) {
		LOG_INFO() << tr("Loading standard image: ") << filePath;
		result = this->loadStandardImage(filePath);
	}
	else {
		LOG_ERROR() << tr("Unsupported file format: ") << fileInfo.suffix();
		return nullptr;
	}

	if (result != nullptr) {
		emit fileLoaded(filePath);
		LOG_INFO() << tr("Successfully loaded file: ") << filePath;
	} else {
		LOG_ERROR() << tr("Failed to load file: ") << filePath;
	}

	return result;
}

ImageData* InputDataReader::loadEnviFiles(const QString& hdrFilePath)
{
	// Parse ENVI header file
	QMap<QString, QString> enviMap = this->parseEnviHeader(hdrFilePath);
	if (enviMap.isEmpty()) {
		LOG_WARNING() << ": Failed to parse ENVI header:" << hdrFilePath;
		return nullptr;
	}

	// Find corresponding data file
	QString dataFilePath = this->detectDataFile(hdrFilePath);
	if (dataFilePath.isEmpty()) {
		LOG_WARNING() << ": Could not find corresponding data file for:" << hdrFilePath;
		return nullptr;
	}

	return this->loadEnviData(dataFilePath, enviMap);
}

QMap<QString, QString> InputDataReader::parseEnviHeader(const QString& hdrFilePath)
{
	QMap<QString, QString> enviMap;

	QFile file(hdrFilePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		LOG_WARNING() << ": Could not open ENVI header file:" << hdrFilePath;
		return enviMap;
	}

	QTextStream in(&file);
	while (!in.atEnd()) {
		QString line = in.readLine();

		// Replace duplicate whitespaces with single whitespace and remove white spaces from beginning and end
		line = line.simplified();

		// Separate string into substrings to obtain field name (key) and its value
		QStringList fields = line.split("=");

		if (fields.size() < 2) {
			continue;
		}

		// Element to the left of the "=" is the field name. Remove unnecessary whitespaces and convert to lower case
		QString key = fields.at(0).trimmed().toLower();

		// Everything after the "=" is the value and needs to be combined into a single string
		QString value = "";
		for (int i = 1; i < fields.size(); i++) {
			value += fields.at(i);
		}

		// Check if there is a curly bracket and continue reading data until a closing bracket is found
		// todo: handle multiple brackets and handle situation with missing closing bracket
		if (value.contains("{") && !value.contains("}")) {
			QString nextLine = "";
			while (!nextLine.contains("}") && !in.atEnd()) {
				nextLine = in.readLine();
				value += nextLine.trimmed();
			}
		}

		// Insert field names and corresponding values into map
		enviMap.insert(key, value);
	}

	file.close();
	return enviMap;
}

ImageData* InputDataReader::loadEnviData(const QString& dataFilePath, const QMap<QString, QString>& enviMap)
{
	QFile file(dataFilePath);
	if (!file.open(QIODevice::ReadOnly)) {
		LOG_WARNING() << ": Could not open ENVI data file:" << dataFilePath;
		return nullptr;
	}

	QDataStream in(&file);

	// Parse ENVI parameters
	QString interleave = enviMap.value(INTERLEAVE, "bsq").toLower().trimmed();
	int dataType = enviMap.value(DATA_TYPE, "1").toInt();
	size_t bytesPerSample = this->getBytesPerSample(static_cast<EnviDataType>(dataType));
	int width = enviMap.value(SAMPLES, "0").toInt();
	int height = enviMap.value(LINES, "0").toInt();
	int frames = enviMap.value(BANDS, "1").toInt();

	if (width <= 0 || height <= 0 || frames <= 0 || bytesPerSample == 0) {
		LOG_WARNING() << ": Invalid ENVI parameters - width:" << width << "height:" << height << "frames:" << frames;
		return nullptr;
	}

	size_t samplesPerFrame = width * height;
	size_t bytesPerFrame = samplesPerFrame * bytesPerSample;
	size_t samplesPerSpectralFrame = width * frames;
	size_t bytesPerSpectralFrame = samplesPerSpectralFrame * bytesPerSample;
	size_t samples = width * height * frames;
	size_t dataSizeInBytes = samples * bytesPerSample;
	size_t bytesPerLine = width * bytesPerSample;

	QString wavelengthsString = enviMap.value(WAVELENGTH, "");
	wavelengthsString = wavelengthsString.remove("{", Qt::CaseInsensitive);
	wavelengthsString = wavelengthsString.remove("}", Qt::CaseInsensitive);
	wavelengthsString = wavelengthsString.simplified();
	QStringList wavelengthStrings = wavelengthsString.split(",");
	QVector<qreal> wavelengths;
	for (const auto& string : qAsConst(wavelengthStrings)) {
		wavelengths.append(string.simplified().toDouble());
	}

	while (wavelengths.size() < frames) {
		wavelengths.append(wavelengths.size());
	}

	QString wavelengthUnit = enviMap.value(WAVELENGTH_UNITS, "nm").simplified();
	if (wavelengthUnit.isEmpty()) {
		wavelengthUnit = "nm";
	}

	char* data = static_cast<char*>(malloc(dataSizeInBytes));
	if (data == nullptr) {
		LOG_ERROR() << ": Failed to allocate memory for ENVI data, size:" << dataSizeInBytes;
		return nullptr;
	}

	char* tmpFrame = nullptr;
	bool success = false;

	//reorder/reslice data according to interleave parameter
	if (interleave == "bip") {
		tmpFrame = static_cast<char*>(malloc(bytesPerFrame));
		if (tmpFrame != nullptr) {
			success = true;
			for (int i = 0; i < frames && success; i++) {
				int readResult = in.readRawData(&tmpFrame[0], static_cast<int>(bytesPerFrame));
				if (readResult == -1) {
					success = false;
					break;
				}
#pragma omp parallel for
				for (int j = 0; j < samplesPerFrame; j++) {
					int index = i * static_cast<int>(samplesPerFrame) + j;
					int pos = ((index / frames) + (index % frames) * static_cast<int>(samplesPerFrame)) * static_cast<int>(bytesPerSample);
					memcpy(&data[pos], &tmpFrame[j * bytesPerSample], bytesPerSample);
				}
			}
		}
	}
	else if (interleave == "bil") {
		tmpFrame = static_cast<char*>(malloc(bytesPerSpectralFrame));
		if (tmpFrame != nullptr) {
			success = true;
			for (int i = 0; i < height && success; i++) {
				int readResult = in.readRawData(&tmpFrame[0], static_cast<int>(bytesPerSpectralFrame));
				if (readResult == -1) {
					success = false;
					break;
				}
#pragma omp parallel for
				for (int j = 0; j < frames; j++) {
					memcpy(&data[i * bytesPerLine + j * bytesPerFrame], &tmpFrame[j * bytesPerLine], bytesPerLine);
				}
			}
		}
	}
	else if (interleave == "bsq") {
		int readResult = in.readRawData(&data[0], static_cast<int>(dataSizeInBytes));
		if (readResult == -1) {
			LOG_WARNING() << ": Could not read input data!";
		} else {
			success = true;
		}
	}
	else {
		LOG_WARNING() << ": Unsupported interleave format:" << interleave;
	}

	// Clean up temporary frame
	if (tmpFrame != nullptr) {
		free(tmpFrame);
	}

	file.close();

	if (!success) {
		LOG_WARNING() << ": Failed to read ENVI data";
		free(data);
		return nullptr;
	}

	return new ImageData(data, width, height, frames, static_cast<EnviDataType>(dataType), wavelengths, wavelengthUnit);
}

ImageData* InputDataReader::loadStandardImage(const QString& imagePath)
{
	QFileInfo fileInfo(imagePath);
	QString suffix = fileInfo.suffix().toLower();

	if (suffix == "tiff" || suffix == "tif") {
#ifdef WAVEPSF_LIBTIFF_BACKEND
		return this->loadTiffWithLibtiff(imagePath);
#else
		return this->loadTiffWithArrayFire(imagePath);
#endif
	}

	// Use Qt for other standard formats
	QImageReader reader(imagePath);
	if (!reader.canRead()) {
		LOG_WARNING() << ": Cannot read image file:" << imagePath;
		return nullptr;
	}

	QImage image = reader.read();
	if (image.isNull()) {
		LOG_WARNING() << ": Failed to load image:" << imagePath;
		return nullptr;
	}

	return new ImageData(image);
}

#ifdef WAVEPSF_LIBTIFF_BACKEND
ImageData* InputDataReader::loadTiffWithLibtiff(const QString& imagePath)
{
	TIFF* tif = TIFFOpen(imagePath.toLocal8Bit().constData(), "r");
	if (!tif) {
		LOG_WARNING() << ": libtiff failed to open:" << imagePath;
		return nullptr;
	}

	// Count frames (IFDs)
	int frameCount = 0;
	do { frameCount++; } while (TIFFReadDirectory(tif));
	TIFFSetDirectory(tif, 0);

	// Read metadata from first frame
	uint32 width = 0, height = 0;
	uint16 bitsPerSample = 8, samplesPerPixel = 1, sampleFormat = SAMPLEFORMAT_UINT;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
	TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
	TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
	TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);

	// Map to EnviDataType
	EnviDataType enviType = UNSIGNED_CHAR_8BIT;
	if (sampleFormat == SAMPLEFORMAT_IEEEFP) {
		enviType = (bitsPerSample == 64) ? DOUBLE_64BIT : FLOAT_32BIT;
	} else {
		switch (bitsPerSample) {
			case 8:  enviType = UNSIGNED_CHAR_8BIT;   break;
			case 16: enviType = UNSIGNED_SHORT_16BIT; break;
			case 32: enviType = SIGNED_INT_32BIT;     break;
			default: enviType = UNSIGNED_CHAR_8BIT;   break;
		}
	}

	size_t bytesPerSample = this->getBytesPerSample(enviType);
	size_t rowBytes   = static_cast<size_t>(width) * bytesPerSample;
	size_t frameBytes = rowBytes * static_cast<size_t>(height);

	char* data = static_cast<char*>(malloc(frameBytes * static_cast<size_t>(frameCount)));
	if (!data) {
		LOG_ERROR() << ": Failed to allocate memory for TIFF data";
		TIFFClose(tif);
		return nullptr;
	}

	tsize_t scanlineSize = TIFFScanlineSize(tif);
	char* rowBuf = static_cast<char*>(_TIFFmalloc(scanlineSize));
	if (!rowBuf) {
		free(data);
		TIFFClose(tif);
		return nullptr;
	}

	for (int f = 0; f < frameCount; ++f) {
		TIFFSetDirectory(tif, static_cast<tdir_t>(f));
		char* frameDest = data + static_cast<ptrdiff_t>(f) * static_cast<ptrdiff_t>(frameBytes);
		for (uint32 y = 0; y < height; ++y) {
			TIFFReadScanline(tif, rowBuf, y, 0);
			if (samplesPerPixel == 1) {
				memcpy(frameDest + y * rowBytes, rowBuf, rowBytes);
			} else {
				// Multi-channel per IFD: extract first channel only (HSI bands are single-channel)
				for (uint32 x = 0; x < width; ++x) {
					memcpy(frameDest + y * rowBytes + x * bytesPerSample,
					       rowBuf + x * samplesPerPixel * bytesPerSample,
					       bytesPerSample);
				}
			}
		}
	}

	_TIFFfree(rowBuf);
	TIFFClose(tif);

	QVector<qreal> wavelengths;
	for (int i = 0; i < frameCount; ++i) {
		wavelengths.append(static_cast<qreal>(i));
	}

	LOG_INFO() << ": Loaded TIFF:" << width << "x" << height
	           << "frames:" << frameCount << "bits:" << bitsPerSample;

	return new ImageData(data, static_cast<int>(width), static_cast<int>(height),
	                     frameCount, enviType, wavelengths, "Frame");
}
#endif // WAVEPSF_LIBTIFF_BACKEND

ImageData* InputDataReader::loadTiffWithArrayFire(const QString& imagePath)
{
	try {
		// Use ArrayFire to load TIFF with native bit depth support
		af::array afImage = af::loadImageNative(imagePath.toStdString().c_str());

		if (afImage.isempty()) {
			LOG_WARNING() << ": ArrayFire failed to load TIFF:" << imagePath;
			return nullptr;
		}

		// Get dimensions and data type from ArrayFire
		af::dim4 dims = afImage.dims();
		int width = static_cast<int>(dims[0]);
		int height = static_cast<int>(dims[1]);
		int channels = (dims[2] > 1) ? static_cast<int>(dims[2]) : 1;
		int frames_dim3 = (dims[3] > 1) ? static_cast<int>(dims[3]) : 1;

		// Determine if this is a multi-frame TIFF (HSI data) or single image
		int totalFrames = channels * frames_dim3;
		bool isMultiFrame = (totalFrames > 3);

		af::dtype afType = afImage.type();

		// Map ArrayFire data type to ENVI data type
		EnviDataType enviType;
		switch (afType) {
			case af::dtype::u8:
				enviType = UNSIGNED_CHAR_8BIT;
				break;
			case af::dtype::u16:
				enviType = UNSIGNED_SHORT_16BIT;
				break;
			case af::dtype::s16:
				enviType = SIGNED_SHORT_16BIT;
				break;
			case af::dtype::u32:
				enviType = UNSIGNED_INT_32BIT;
				break;
			case af::dtype::s32:
				enviType = SIGNED_INT_32BIT;
				break;
			case af::dtype::f32:
				enviType = FLOAT_32BIT;
				break;
			case af::dtype::f64:
				enviType = DOUBLE_64BIT;
				break;
			default:
				LOG_WARNING() << ": Unsupported ArrayFire data type, using float";
				enviType = FLOAT_32BIT;
				afImage = afImage.as(af::dtype::f32);
				break;
		}

		// Allocate buffer for ImageData
		size_t bytesPerSample = this->getBytesPerSample(enviType);
		size_t totalBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(totalFrames) * bytesPerSample;
		void* data = malloc(totalBytes);

		if (data == nullptr) {
			LOG_ERROR() << ": Failed to allocate memory for TIFF data:" << totalBytes << "bytes";
			return nullptr;
		}

		// Copy data from ArrayFire to buffer
		this->copyArrayFireDataToBuffer(afImage, data, enviType, width, height, totalFrames);

		// Create metadata
		QVector<qreal> wavelengths;
		QString wavelengthUnit;

		if (isMultiFrame) {
			// Multi-frame TIFF - treat as HSI data
			LOG_INFO() << ": Loading multi-frame TIFF as HSI data:" << width << "x" << height << "x" << totalFrames;
			wavelengthUnit = "Frame";
			for (int i = 0; i < totalFrames; ++i) {
				wavelengths.append(static_cast<qreal>(i));
			}
		} else if (totalFrames == 1) {
			// Single-frame grayscale
			LOG_INFO() << ": Loading single-frame TIFF as grayscale:" << width << "x" << height;
			wavelengthUnit = "Grayscale";
			wavelengths.append(0.0);
		} else {
			// RGB/multi-channel single image
			LOG_INFO() << ": Loading TIFF as RGB:" << width << "x" << height << "x" << totalFrames;
			wavelengthUnit = "RGB";
			for (int i = 0; i < totalFrames; ++i) {
				wavelengths.append(static_cast<qreal>(i));
			}
		}

		// Create ImageData object
		ImageData* result = new ImageData(data, width, height, totalFrames, enviType, wavelengths, wavelengthUnit);
		return result;

	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception loading TIFF:" << imagePath << "-" << e.what();
		return nullptr;
	}
}

size_t InputDataReader::getBytesPerSample(EnviDataType dataType)
{
	switch (dataType) {
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
			LOG_WARNING() << ": Unknown ENVI data type:" << dataType;
			return 4; // Default to float
	}
}

QString InputDataReader::detectDataFile(const QString& hdrFilePath)
{
	QFileInfo fileInfo(hdrFilePath);
	QString baseName = fileInfo.baseName();
	QString dirPath = fileInfo.absolutePath();
	QStringList dataExtensions = {"img", "raw", "dat"};

	for (const QString& ext : dataExtensions) {
		QString dataPath = QDir(dirPath).filePath(baseName + "." + ext);
		if (QFileInfo::exists(dataPath)) {
			return dataPath;
		}
	}

	return QString();
}

void InputDataReader::copyArrayFireDataToBuffer(const af::array& afData, void* buffer, EnviDataType enviType, int width, int height, int frames)
{
	if (buffer == nullptr || afData.isempty()) {
		LOG_WARNING() << ": Invalid parameters for ArrayFire data copy";
		return;
	}

	try {
		size_t totalElements = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(frames);

		// Reshape ArrayFire array to linear format if needed
		af::array linearData = af::moddims(afData, totalElements);

		// Copy data based on target ENVI data type
		switch (enviType) {
			case UNSIGNED_CHAR_8BIT: {
				af::array converted = af::clamp(linearData, 0.0, 255.0).as(af::dtype::u8);
				unsigned char* hostData = converted.host<unsigned char>();
				std::memcpy(buffer, hostData, totalElements * sizeof(unsigned char));
				af::freeHost(hostData);
				break;
			}

			case SIGNED_SHORT_16BIT: {
				af::array converted = af::clamp(linearData, -32768.0, 32767.0).as(af::dtype::s16);
				short* hostData = converted.host<short>();
				std::memcpy(buffer, hostData, totalElements * sizeof(short));
				af::freeHost(hostData);
				break;
			}

			case UNSIGNED_SHORT_16BIT: {
				af::array converted = af::clamp(linearData, 0.0, 65535.0).as(af::dtype::u16);
				unsigned short* hostData = converted.host<unsigned short>();
				std::memcpy(buffer, hostData, totalElements * sizeof(unsigned short));
				af::freeHost(hostData);
				break;
			}

			case SIGNED_INT_32BIT: {
				af::array converted = linearData.as(af::dtype::s32);
				int* hostData = converted.host<int>();
				std::memcpy(buffer, hostData, totalElements * sizeof(int));
				af::freeHost(hostData);
				break;
			}

			case UNSIGNED_INT_32BIT: {
				af::array converted = linearData.as(af::dtype::u32);
				unsigned int* hostData = converted.host<unsigned int>();
				std::memcpy(buffer, hostData, totalElements * sizeof(unsigned int));
				af::freeHost(hostData);
				break;
			}

			case FLOAT_32BIT: {
				af::array converted = linearData.as(af::dtype::f32);
				float* hostData = converted.host<float>();
				std::memcpy(buffer, hostData, totalElements * sizeof(float));
				af::freeHost(hostData);
				break;
			}

			case DOUBLE_64BIT: {
				af::array converted = linearData.as(af::dtype::f64);
				double* hostData = converted.host<double>();
				std::memcpy(buffer, hostData, totalElements * sizeof(double));
				af::freeHost(hostData);
				break;
			}

			default:
				LOG_WARNING() << ": Unsupported ENVI data type for buffer copy:" << enviType;
				break;
		}

		LOG_DEBUG() << ": Copied" << totalElements << "elements from ArrayFire to buffer";

	} catch (const af::exception& e) {
		LOG_WARNING() << ": ArrayFire exception during buffer copy:" << e.what();
	}
}
