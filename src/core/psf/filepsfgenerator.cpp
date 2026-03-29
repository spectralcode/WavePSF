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

	auto key = qMakePair(request.frame, request.patchIdx);
	if (this->psfCache.contains(key)) {
		return this->psfCache.value(key);
	}

	return af::array();
}

bool FilePSFGenerator::is3D() const
{
	return this->volumetric;
}

void FilePSFGenerator::invalidateCache()
{
	this->psfCache.clear();
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

	// Load all files, try frame_patchIdx naming convention first
	for (const QFileInfo& fi : qAsConst(imageFiles)) {
		af::array psf = PSFFileManager::loadPSFFromFile(fi.absoluteFilePath());
		if (psf.isempty()) {
			continue;
		}

		// Try to parse "frame_patchIdx" from basename
		QStringList parts = fi.baseName().split('_');
		if (parts.size() >= 2) {
			bool frameOk = false, patchOk = false;
			int frame = parts[0].toInt(&frameOk);
			int patchIdx = parts[1].toInt(&patchOk);
			if (frameOk && patchOk) {
				this->psfCache[qMakePair(frame, patchIdx)] = psf;
				continue;
			}
		}

		// Fallback: use alphabetical index as frame, patchIdx = 0
		int idx = imageFiles.indexOf(fi);
		this->psfCache[qMakePair(idx, 0)] = psf;
	}

	// Detect 3D from first loaded PSF
	if (!this->psfCache.isEmpty()) {
		af::array first = this->psfCache.constBegin().value();
		this->volumetric = (first.numdims() > 2 && first.dims(2) > 1);
	}

	LOG_INFO() << "FilePSFGenerator: loaded" << this->psfCache.size()
			   << "PSFs from folder:" << folderPath;
}
