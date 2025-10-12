#include "supportedfilechecker.h"
#include <QFileInfo>
#include <QDir>

bool SupportedFileChecker::isEnviFile(const QString& filePath)
{
	if (filePath.isEmpty()) {
		return false;
	}

	QFileInfo fileInfo(filePath);
	QString suffix = fileInfo.suffix().toLower();
	QString baseName = fileInfo.baseName();
	QString dirPath = fileInfo.absolutePath();

	if (suffix == "hdr") {
		// For .hdr files, check if corresponding data file exists
		QStringList dataExtensions = {"img", "raw", "dat"};
		for (const QString& ext : dataExtensions) {
			QString dataPath = QDir(dirPath).filePath(baseName + "." + ext);
			if (QFileInfo::exists(dataPath)) {
				return true;
			}
		}
		return false;
	}
	else if (suffix == "img" || suffix == "raw" || suffix == "dat") {
		// For data files, check if corresponding .hdr file exists
		QString hdrPath = QDir(dirPath).filePath(baseName + ".hdr");
		return QFileInfo::exists(hdrPath);
	}

	return false;
}

bool SupportedFileChecker::isStandardImageFile(const QString& filePath)
{
	QMap<QString, QStringList> formats = getSupportedImageFormats();
	return hasExtension(filePath, formats["standard"]);
}

bool SupportedFileChecker::isValidImageFile(const QString& filePath)
{
	return isEnviFile(filePath) || isStandardImageFile(filePath);
}

QMap<QString, QStringList> SupportedFileChecker::getSupportedImageFormats()
{
	static const QMap<QString, QStringList> formats = {
		{"envi", {".hdr", ".img", ".raw", ".dat"}},
		{"standard", {".tif", ".tiff", ".png", ".jpg", ".jpeg", ".bmp"}}
	};
	return formats;
}

QStringList SupportedFileChecker::getAllExtensions()
{
	QStringList allExtensions;
	QMap<QString, QStringList> formats = getSupportedImageFormats();
	
	for (auto it = formats.begin(); it != formats.end(); ++it) {
		allExtensions.append(it.value());
	}
	
	return allExtensions;
}

QString SupportedFileChecker::getFileDialogFilters()
{
	QMap<QString, QStringList> formats = getSupportedImageFormats();
	QStringList filterParts;
	
	// All supported files filter
	QStringList allExts = getAllExtensions();
	QString allFilter = QString("All Supported Files (%1)").arg(formatExtensionsForFilter(allExts));
	filterParts.append(allFilter);
	
	// Individual format filters
	filterParts.append(QString("ENVI Files (%1)").arg(formatExtensionsForFilter(formats["envi"])));
	filterParts.append(QString("Standard Images (%1)").arg(formatExtensionsForFilter(formats["standard"])));
	
	// All files
	filterParts.append("All Files (*)");
	
	return filterParts.join(";;");
}

bool SupportedFileChecker::hasExtension(const QString& filePath, const QStringList& extensions)
{
	if (filePath.isEmpty()) {
		return false;
	}

	QFileInfo fileInfo(filePath);
	QString suffix = "." + fileInfo.suffix().toLower();
	
	for (const QString& ext : extensions) {
		if (suffix == ext.toLower()) {
			return true;
		}
	}
	
	return false;
}

QString SupportedFileChecker::formatExtensionsForFilter(const QStringList& extensions)
{
	QStringList formatted;
	for (const QString& ext : extensions) {
		formatted.append("*" + ext);
	}
	return formatted.join(" ");
}