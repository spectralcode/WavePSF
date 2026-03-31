#ifndef PSFFILEMANAGER_H
#define PSFFILEMANAGER_H

#include <QObject>
#include <QString>
#include <arrayfire.h>

class PSFModule;

class PSFFileManager : public QObject
{
	Q_OBJECT
public:
	explicit PSFFileManager(QObject* parent = nullptr);

	// PSF file I/O
	void savePSFToFile(const QString& filePath, PSFModule* psfModule);
	static af::array loadPSFFromFile(const QString& filePath, int* outBitDepth = nullptr);

	// Auto-save PSF if enabled
	void autoSaveIfEnabled(int frame, int patchIdx, PSFModule* psfModule);

public slots:
	void setAutoSavePSF(bool enabled);
	void setPSFSaveFolder(const QString& folder);

private:
	bool autoSavePSFEnabled;
	QString psfSaveFolder;
};

#endif // PSFFILEMANAGER_H
