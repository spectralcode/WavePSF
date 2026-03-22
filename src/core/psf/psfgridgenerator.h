#ifndef PSFGRIDGENERATOR_H
#define PSFGRIDGENERATOR_H

#include <QObject>
#include <QVector>
#include <QImage>
#include <arrayfire.h>

class PSFModule;
class WavefrontParameterTable;

struct PSFGridResult {
	QVector<af::array> rawPSFs;   // cropped float PSFs for TIF export
	QImage mosaicImage;           // composed 8-bit grayscale mosaic for display
	int cols;                     // patch grid columns
	int rows;                     // patch grid rows
	int cellSize;                 // crop size per PSF cell
	int spacing;                  // pixel gap between cells
};

class PSFGridGenerator : public QObject
{
	Q_OBJECT
public:
	explicit PSFGridGenerator(QObject* parent = nullptr);

	PSFGridResult generate(
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable,
		int frame, int cols, int rows,
		int cropSize);

private:
	static QImage composeMosaic(
		const QVector<af::array>& psfs,
		int cols, int rows, int cellSize, int spacing);

	static QImage afArrayToGrayscaleImage(const af::array& arr);
};

#endif // PSFGRIDGENERATOR_H
