#ifndef FILEPSFGENERATOR_H
#define FILEPSFGENERATOR_H

#include <QObject>
#include <QMap>
#include <QPair>
#include <arrayfire.h>
#include "ipsfgenerator.h"

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

	// Cache management
	void invalidateCache() override;

	// Source selection
	void setSource(const QString& path);
	QString source() const;

private:
	void loadFolder(const QString& folderPath);
	void loadSingleFile(const QString& filePath);

	QString sourcePath;
	QMap<QPair<int,int>, af::array> psfCache;
	af::array singlePSF;
	bool singleMode;
	bool volumetric;
};

#endif // FILEPSFGENERATOR_H
