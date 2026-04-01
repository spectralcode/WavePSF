#ifndef FILEPSFGENERATOR_H
#define FILEPSFGENERATOR_H

#include <QObject>
#include <QMap>
#include <arrayfire.h>
#include "ipsfgenerator.h"
#include "psffileinfo.h"

class FilePSFGenerator : public QObject, public IPSFGenerator
{
	Q_OBJECT
public:
	explicit FilePSFGenerator(QObject* parent = nullptr);

	// IPSFGenerator — identity
	QString typeName() const override;
	QVariantMap serializeSettings() const override;
	void deserializeSettings(const QVariantMap& settings) override;

	// Coefficients — not supported
	bool supportsCoefficients() const override { return false; }

	// PSF generation
	af::array generatePSF(const PSFRequest& request) override;

	// Capabilities
	bool is3D() const override;
	bool isFileBased() const override { return true; }

	// Cache management
	void invalidateCache() override;

	// Source selection
	void setSource(const QString& path);
	QString source() const;
	PSFFileInfo getFileInfo() const;

private:
	void loadFolder(const QString& folderPath);
	void loadSingleFile(const QString& filePath);

	void computeArrayStats(const af::array& data, double& outMin, double& outMax, double& outSum);

	QString sourcePath;
	QMap<int, af::array> patchVolumes;
	af::array singlePSF;
	bool singleMode;
	bool volumetric;
	PSFFileInfo fileInfo;
};

#endif // FILEPSFGENERATOR_H
