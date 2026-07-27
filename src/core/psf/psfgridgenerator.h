#ifndef PSFGRIDGENERATOR_H
#define PSFGRIDGENERATOR_H

#include <QObject>
#include <QVector>
#include <QImage>
#include <QAtomicInt>
#include <QFutureWatcher>
#include <arrayfire.h>
#include "psfsettings.h"
#include "utils/operationresult.h"

struct PSFGridRequest {
	PSFSettings psfSettings;
	QVector<QVector<double>> coefficients;
	QVector<af::array> loadedPSFs;
	int afBackend = 0;
	int afDeviceId = 0;
	int frame = 0;
	int cols = 0;
	int rows = 0;
	int cropSize = 0;
};

struct PSFGridResult {
	QVector<af::array> rawPSFs;   // cropped float PSFs for TIF export
	QImage mosaicImage;           // composed 8-bit grayscale mosaic for display
	int cols = 0;                 // patch grid columns
	int rows = 0;                 // patch grid rows
	int cellSize = 0;             // crop size per PSF cell
	int spacing = 1;              // pixel gap between cells
	RunStatus status = RunStatus::COMPLETED;
	QString message;
};

class PSFGridGenerator : public QObject
{
	Q_OBJECT
public:
	explicit PSFGridGenerator(QObject* parent = nullptr);
	~PSFGridGenerator() override;

	void generate(const PSFGridRequest& request);
	void cancel();

	static PSFGridResult createEmptyGrid(int cols, int rows, int cellSize);
	static bool updatePatch(
		PSFGridResult& grid, af::array psf,
		int frame, int patchX, int patchY);

signals:
	void started(int totalPatches);
	void progressUpdated(int completedPatches, int totalPatches);
	void finished(PSFGridResult result);

private:
	PSFGridResult generateGrid(const PSFGridRequest& request);
	static QImage afArrayToGrayscaleImage(const af::array& arr);

	QFutureWatcher<PSFGridResult> watcher;
	QAtomicInt cancelRequested;
};

#endif // PSFGRIDGENERATOR_H
