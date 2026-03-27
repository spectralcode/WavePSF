#ifndef VOLUMETRICPROCESSOR_H
#define VOLUMETRICPROCESSOR_H

#include <QObject>
#include <QString>
#include <arrayfire.h>

class ImageSession;
class QWidget;

class VolumetricProcessor : public QObject
{
	Q_OBJECT
public:
	explicit VolumetricProcessor(QObject* parent = nullptr);

	// Full 3D deconvolution pipeline:
	// 1. GPU memory check with user confirmation
	// 2. Load input volume (all frames)
	// 3. Load 3D PSF from folder of TIFF slices
	// 4. Run 3D Richardson-Lucy
	// 5. Write result back to output
	bool execute(ImageSession* imageSession,
				 const QString& psfFolderPath,
				 int iterations,
				 QWidget* parentWidget = nullptr);

signals:
	void error(QString message);

private:
	af::array loadVolume(ImageSession* imageSession);
	af::array loadPSFVolume(const QString& folderPath);
	void writeVolumeToOutput(ImageSession* imageSession, const af::array& volume);
};

#endif // VOLUMETRICPROCESSOR_H
