#include "filepsfgenerator.h"
#include "psffilemanager.h"
#include "utils/logging.h"
#include <QDir>
#include <QFileInfo>

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

void FilePSFGenerator::setSource(const QString& path)
{
	this->sourcePath = path;
	this->invalidateCache();
	this->singleMode = false;
	this->volumetric = false;

	if (path.isEmpty()) {
		return;
	}

	QFileInfo info(path);
	if (info.isFile()) {
		this->loadSingleFile(path);
	} else if (info.isDir()) {
		this->loadFolder(path);
	}
}

QString FilePSFGenerator::source() const
{
	return this->sourcePath;
}

void FilePSFGenerator::loadSingleFile(const QString& filePath)
{
	af::array psf = PSFFileManager::loadPSFFromFile(filePath);
	if (psf.isempty()) {
		return;
	}

	this->singlePSF = psf;
	this->singleMode = true;
	this->volumetric = (psf.numdims() > 2 && psf.dims(2) > 1);
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
		return;
	}

	// Load all files and group frames by patch index.
	// frame_patchIdx naming: "0_0.tif" = frame 0 patch 0. Frames stacked as Z per patch.
	// Plain naming: all files stacked as one volume (patch 0).
	QMap<int, QMap<int, af::array>> patchFrames; // patchIdx → (frame → array)
	bool hasFramePatchNaming = false;

	for (const QFileInfo& fi : qAsConst(imageFiles)) {
		af::array psf = PSFFileManager::loadPSFFromFile(fi.absoluteFilePath());
		if (psf.isempty()) {
			continue;
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
}
