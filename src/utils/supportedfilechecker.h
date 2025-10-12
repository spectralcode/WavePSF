#ifndef SUPPORTEDFILECHECKER_H
#define SUPPORTEDFILECHECKER_H

#include <QString>
#include <QStringList>
#include <QMap>

class SupportedFileChecker
{
public:
	static bool isEnviFile(const QString& filePath);
	static bool isStandardImageFile(const QString& filePath);
	static bool isValidImageFile(const QString& filePath);
	
	static QMap<QString, QStringList> getSupportedImageFormats();
	static QStringList getAllExtensions();
	static QString getFileDialogFilters();

private:
	SupportedFileChecker() = default; //private constructor prevents instantiation. use static methods only.
	static bool hasExtension(const QString& filePath, const QStringList& extensions);
	static QString formatExtensionsForFilter(const QStringList& extensions);
};

#endif // SUPPORTEDFILECHECKER_H