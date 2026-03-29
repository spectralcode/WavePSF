#include "psffilemanager.h"
#include "psfmodule.h"
#include "utils/logging.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QImage>

PSFFileManager::PSFFileManager(QObject* parent)
	: QObject(parent)
	, autoSavePSFEnabled(false)
{
}

void PSFFileManager::savePSFToFile(const QString& filePath, PSFModule* psfModule)
{
	if (psfModule == nullptr) {
		return;
	}

	af::array psf = psfModule->getCurrentPSF();
	if (psf.isempty()) {
		LOG_WARNING() << "No PSF to save";
		return;
	}

	// Extract focal plane if PSF is 3D (multi-page TIFF deferred to V2)
	psf = PSFModule::focalSlice(psf);

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

af::array PSFFileManager::loadPSFFromFile(const QString& filePath)
{
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
				return af::array();
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
			LOG_INFO() << "PSF loaded from file:" << filePath
					   << "(" << psf.dims(0) << "x" << psf.dims(1) << ")";
		}
		return psf;
	} catch (af::exception& e) {
		LOG_WARNING() << "Failed to load PSF file:" << e.what();
		return af::array();
	}
}

void PSFFileManager::autoSaveIfEnabled(int frame, int patchIdx, PSFModule* psfModule)
{
	if (!this->autoSavePSFEnabled || this->psfSaveFolder.isEmpty()) {
		return;
	}
	QString name = QString("%1_%2.tif").arg(frame).arg(patchIdx);
	this->savePSFToFile(this->psfSaveFolder + "/" + name, psfModule);
}

void PSFFileManager::setAutoSavePSF(bool enabled)
{
	this->autoSavePSFEnabled = enabled;
	LOG_INFO() << "PSF auto-save" << (enabled ? "enabled" : "disabled");
}

void PSFFileManager::setPSFSaveFolder(const QString& folder)
{
	this->psfSaveFolder = folder;
	LOG_INFO() << "PSF save folder set to:" << folder;
}
