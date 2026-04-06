#ifndef PSFFILECONTROLLER_H
#define PSFFILECONTROLLER_H

#include <QObject>
#include <QString>
#include "core/psf/psffileinfo.h"

class PSFModule;
class PSFFileManager;

class PSFFileController : public QObject
{
	Q_OBJECT

public:
	explicit PSFFileController(PSFModule* psfModule, QObject* parent = nullptr);

public slots:
	void savePSFToFile(const QString& filePath);
	void setAutoSavePSF(bool enabled);
	void setPSFSaveFolder(const QString& folder);
	void autoSaveIfEnabled(int frame, int patchIdx);
	void setFilePSFSource(const QString& path);
	void refreshFileInfo();

signals:
	void filePSFInfoUpdated(PSFFileInfo info);

private:
	PSFModule* psfModule;
	PSFFileManager* psfFileManager;
};

#endif // PSFFILECONTROLLER_H
