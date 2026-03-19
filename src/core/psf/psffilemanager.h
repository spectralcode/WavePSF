#ifndef PSFFILEMANAGER_H
#define PSFFILEMANAGER_H

#include <QObject>
#include <QMap>
#include <QPair>
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
	af::array loadPSFFromFile(const QString& filePath);

	// External PSF override management (per-patch loaded PSFs)
	void storeOverride(int frame, int patchIdx, const af::array& psf);
	void removeOverride(int frame, int patchIdx);
	bool hasOverride(int frame, int patchIdx) const;
	af::array getOverride(int frame, int patchIdx) const;
	void clearOverrides();

	// Custom PSF folder: load PSF by frame_patchIdx naming convention
	af::array loadPSFFromFolder(int frame, int patchIdx);

	// Auto-save PSF if enabled
	void autoSaveIfEnabled(int frame, int patchIdx, PSFModule* psfModule);

	// State queries
	bool isCustomFolderMode() const;

public slots:
	void setAutoSavePSF(bool enabled);
	void setPSFSaveFolder(const QString& folder);
	void setUseCustomPSFFolder(bool enabled);
	void setCustomPSFFolder(const QString& folder);

private:
	QMap<QPair<int,int>, af::array> externalPSFOverrides;
	bool autoSavePSFEnabled;
	QString psfSaveFolder;
	bool useCustomPSFFolder;
	QString customPSFFolder;
};

#endif // PSFFILEMANAGER_H
