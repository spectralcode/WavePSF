#include "filepsfgenerator.h"
#include "psffilemanager.h"
#include "utils/logging.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <limits>

namespace {
	const QString KEY_SOURCE_PATH = QStringLiteral("source_path");
}


FilePSFGenerator::FilePSFGenerator(QObject* parent)
	: QObject(parent)
	, singleMode(false)
	, volumetric(false)
{
}

QString FilePSFGenerator::typeName() const
{
	return QStringLiteral("From File");
}

QVariantMap FilePSFGenerator::serializeSettings() const
{
	QVariantMap m;
	m[KEY_SOURCE_PATH] = this->sourcePath;
	return m;
}

void FilePSFGenerator::deserializeSettings(const QVariantMap& settings)
{
	QString path = settings.value(KEY_SOURCE_PATH).toString();
	if (!path.isEmpty() && path != this->sourcePath) {
		this->setSource(path);
	}
}

af::array FilePSFGenerator::generatePSF(const PSFRequest& request)
{
	if (this->singleMode) {
		return this->singlePSF;
	}

	if (this->patchVolumes.contains(request.patchIdx)) {
		return this->patchVolumes.value(request.patchIdx);
	}

	return af::array();
}

bool FilePSFGenerator::is3D() const
{
	return this->volumetric;
}

void FilePSFGenerator::invalidateCache()
{
	this->patchVolumes.clear();
	this->singlePSF = af::array();
}

PSFFileInfo FilePSFGenerator::getFileInfo() const
{
	return this->fileInfo;
}

void FilePSFGenerator::computeArrayStats(const af::array& data, double& outMin, double& outMax, double& outSum)
{
	outMin = af::min<double>(af::flat(data));
	outMax = af::max<double>(af::flat(data));
	outSum = af::sum<double>(af::flat(data));
}

void FilePSFGenerator::setSource(const QString& path)
{
	this->sourcePath = path;
	this->invalidateCache();
	this->singleMode = false;
	this->volumetric = false;
	this->fileInfo = PSFFileInfo();

	if (path.isEmpty()) {
		return;
	}

	QFileInfo info(path);
	if (info.isFile()) {
		this->loadSingleFile(path);
	} else if (info.isDir()) {
		this->loadFolder(path);
	} else {
		LOG_WARNING() << "FilePSFGenerator: source path does not exist:" << path;
		this->sourcePath.clear();
	}
}

QString FilePSFGenerator::source() const
{
	return this->sourcePath;
}

void FilePSFGenerator::loadSingleFile(const QString& filePath)
{
	int bitDepth = 0;
	af::array psf = PSFFileManager::loadPSFFromFile(filePath, &bitDepth);
	if (psf.isempty()) {
		return;
	}

	this->singlePSF = psf;
	this->singleMode = true;
	this->volumetric = (psf.numdims() > 2 && psf.dims(2) > 1);

	// Populate metadata
	QFileInfo fi(filePath);
	QString suffix = fi.suffix().toLower();
	this->fileInfo.valid = true;
	this->fileInfo.isFolder = false;
	this->fileInfo.fileCount = 1;
	this->fileInfo.fileFormat = (suffix == "tif" || suffix == "tiff") ? QStringLiteral("TIFF")
		: (suffix == "png") ? QStringLiteral("PNG")
		: (suffix == "jpg" || suffix == "jpeg") ? QStringLiteral("JPEG")
		: (suffix == "bmp") ? QStringLiteral("BMP")
		: suffix.toUpper();
	this->fileInfo.width = static_cast<int>(psf.dims(1));
	this->fileInfo.height = static_cast<int>(psf.dims(0));
	this->fileInfo.depth = this->volumetric ? static_cast<int>(psf.dims(2)) : 1;
	this->fileInfo.patchCount = 1;
	this->fileInfo.volumetric = this->fileInfo.depth > 1;
	this->fileInfo.bitDepth = bitDepth;
	this->computeArrayStats(psf, this->fileInfo.minValue, this->fileInfo.maxValue, this->fileInfo.sum);

	LOG_INFO() << "FilePSFGenerator: loaded single file:" << filePath
			   << "(" << psf.dims(0) << "x" << psf.dims(1)
			   << (this->volumetric ? QString(", %1 planes").arg(psf.dims(2)) : QString()) << ")";
}

void FilePSFGenerator::loadFolder(const QString& folderPath)
{
	QDir dir(folderPath);
	QStringList filters;
	filters << "*.tif" << "*.tiff" << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp";
	QFileInfoList imageFiles = dir.entryInfoList(filters, QDir::Files, QDir::Name);

	if (imageFiles.isEmpty()) {
		LOG_WARNING() << "FilePSFGenerator: no image files found in" << folderPath;
		return;
	}

	if (imageFiles.size() == 1) {
		this->loadSingleFile(imageFiles.first().absoluteFilePath());
		if (this->fileInfo.valid) {
			this->fileInfo.isFolder = true;
		}
		return;
	}

	// Track file formats and bit depths for metadata
	QSet<QString> formats;
	int firstBitDepth = 0;
	bool mixedBitDepth = false;

	// Load all files and group frames by patch index.
	// frame_patchIdx naming: "0_0.tif" = frame 0 patch 0. Frames stacked as Z per patch.
	// Plain naming: all files stacked as one volume (patch 0).
	QMap<int, QMap<int, af::array>> patchFrames; // patchIdx → (frame → array)
	bool hasFramePatchNaming = false;

	for (const QFileInfo& fi : qAsConst(imageFiles)) {
		int bitDepth = 0;
		af::array psf = PSFFileManager::loadPSFFromFile(fi.absoluteFilePath(), &bitDepth);
		if (psf.isempty()) {
			continue;
		}

		// Track format and bit depth
		QString suffix = fi.suffix().toLower();
		if (suffix == "tif" || suffix == "tiff") formats.insert(QStringLiteral("TIFF"));
		else if (suffix == "png") formats.insert(QStringLiteral("PNG"));
		else if (suffix == "jpg" || suffix == "jpeg") formats.insert(QStringLiteral("JPEG"));
		else if (suffix == "bmp") formats.insert(QStringLiteral("BMP"));
		else formats.insert(suffix.toUpper());

		if (firstBitDepth == 0) {
			firstBitDepth = bitDepth;
		} else if (bitDepth != firstBitDepth) {
			mixedBitDepth = true;
		}

		QStringList parts = fi.baseName().split('_');
		if (parts.size() >= 2) {
			bool frameOk = false, patchOk = false;
			int frame = parts[0].toInt(&frameOk);
			int patchIdx = parts[1].toInt(&patchOk);
			if (frameOk && patchOk) {
				patchFrames[patchIdx][frame] = psf;
				hasFramePatchNaming = true;
				continue;
			}
		}

		if (!hasFramePatchNaming) {
			int idx = imageFiles.indexOf(fi);
			patchFrames[0][idx] = psf;
		}
	}

	// Stack frames into a 3D volume per patch (skip dimension mismatches)
	int totalSlices = 0;
	for (auto it = patchFrames.constBegin(); it != patchFrames.constEnd(); ++it) {
		const QMap<int, af::array>& frames = it.value();
		af::array volume;
		for (auto fi = frames.constBegin(); fi != frames.constEnd(); ++fi) {
			if (volume.isempty()) {
				volume = fi.value();
			} else if (fi.value().dims(0) == volume.dims(0) &&
					   fi.value().dims(1) == volume.dims(1)) {
				volume = af::join(2, volume, fi.value());
			}
		}
		this->patchVolumes[it.key()] = volume;
		totalSlices += frames.size();
	}

	this->volumetric = true;
	LOG_INFO() << "FilePSFGenerator: loaded" << totalSlices << "slices into"
			   << this->patchVolumes.size() << "patch volume(s) from folder:" << folderPath;

	// Populate metadata from all loaded volumes
	if (!this->patchVolumes.isEmpty()) {
		af::array firstVol = this->patchVolumes.first();
		this->fileInfo.valid = true;
		this->fileInfo.isFolder = true;
		this->fileInfo.fileCount = imageFiles.size();
		this->fileInfo.fileFormat = (formats.size() == 1) ? *formats.begin() : QStringLiteral("Mixed");
		this->fileInfo.width = static_cast<int>(firstVol.dims(1));
		this->fileInfo.height = static_cast<int>(firstVol.dims(0));
		this->fileInfo.depth = (firstVol.numdims() > 2 && firstVol.dims(2) > 1)
			? static_cast<int>(firstVol.dims(2)) : 1;
		this->fileInfo.patchCount = this->patchVolumes.size();
		this->fileInfo.volumetric = this->fileInfo.depth > 1;
		this->fileInfo.bitDepth = mixedBitDepth ? 0 : firstBitDepth;

		// Aggregate min/max/sum across all patches
		double globalMin = (std::numeric_limits<double>::max)();
		double globalMax = -(std::numeric_limits<double>::max)();
		double globalSum = 0.0;
		for (auto it = this->patchVolumes.constBegin(); it != this->patchVolumes.constEnd(); ++it) {
			double pMin, pMax, pSum;
			this->computeArrayStats(it.value(), pMin, pMax, pSum);
			if (pMin < globalMin) globalMin = pMin;
			if (pMax > globalMax) globalMax = pMax;
			globalSum += pSum;
		}
		this->fileInfo.minValue = globalMin;
		this->fileInfo.maxValue = globalMax;
		this->fileInfo.sum = globalSum;
	}
}
