#include "psfgridgenerator.h"
#include "psfmodule.h"
#include "data/wavefrontparametertable.h"
#include "utils/logging.h"

PSFGridGenerator::PSFGridGenerator(QObject* parent)
	: QObject(parent)
{
}

PSFGridResult PSFGridGenerator::generate(
	PSFModule* psfModule,
	WavefrontParameterTable* parameterTable,
	int frame, int cols, int rows,
	int cropSize)
{
	PSFGridResult result;
	result.cols = cols;
	result.rows = rows;
	result.cellSize = cropSize;
	result.spacing = 1;

	int totalPatches = cols * rows;
	result.rawPSFs.resize(totalPatches);

	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			int patchIdx = parameterTable->patchIndex(x, y);
			QVector<double> coeffs = parameterTable->getCoefficients(frame, patchIdx);
			af::array psf = psfModule->computePSFFromCoefficients(coeffs);

			// Center-crop
			int psfSize = static_cast<int>(psf.dims(0));
			if (cropSize < psfSize) {
				int offset = (psfSize - cropSize) / 2;
				psf = psf(af::seq(offset, offset + cropSize - 1),
				          af::seq(offset, offset + cropSize - 1));
			}
			result.rawPSFs[patchIdx] = psf;
		}
	}

	result.mosaicImage = composeMosaic(
		result.rawPSFs, cols, rows, cropSize, result.spacing);

	LOG_INFO() << "PSF grid generated:" << cols << "x" << rows
			   << "patches, crop=" << cropSize;
	return result;
}

QImage PSFGridGenerator::composeMosaic(
	const QVector<af::array>& psfs,
	int cols, int rows, int cellSize, int spacing)
{
	int mosaicWidth = cols * cellSize + (cols - 1) * spacing;
	int mosaicHeight = rows * cellSize + (rows - 1) * spacing;

	QImage mosaic(mosaicWidth, mosaicHeight, QImage::Format_Grayscale8);
	mosaic.fill(0);

	int stride = cellSize + spacing;

	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			int patchIdx = y * cols + x;
			if (patchIdx >= psfs.size() || psfs[patchIdx].isempty()) {
				continue;
			}

			QImage cellImage = afArrayToGrayscaleImage(psfs[patchIdx]);

			int destX = x * stride;
			int destY = y * stride;

			for (int cy = 0; cy < cellImage.height() && (destY + cy) < mosaicHeight; cy++) {
				const uchar* srcLine = cellImage.constScanLine(cy);
				uchar* dstLine = mosaic.scanLine(destY + cy);
				for (int cx = 0; cx < cellImage.width() && (destX + cx) < mosaicWidth; cx++) {
					dstLine[destX + cx] = srcLine[cx];
				}
			}
		}
	}

	return mosaic;
}

QImage PSFGridGenerator::afArrayToGrayscaleImage(const af::array& arr)
{
	int height = static_cast<int>(arr.dims(0));
	int width = static_cast<int>(arr.dims(1));

	af::array floatArr = arr.as(af::dtype::f32);
	float* hostData = floatArr.host<float>();

	QImage image(width, height, QImage::Format_Grayscale8);

	// Find peak for normalization
	float peak = 0.0f;
	for (int i = 0; i < width * height; i++) {
		if (hostData[i] > peak) {
			peak = hostData[i];
		}
	}

	float scale = (peak > 0.0f) ? 255.0f / peak : 0.0f;

	// AF is column-major: element(row, col) at hostData[row + col * height]
	for (int y = 0; y < height; y++) {
		uchar* scanLine = image.scanLine(y);
		for (int x = 0; x < width; x++) {
			float val = hostData[y + x * height];
			scanLine[x] = static_cast<uchar>(val * scale);
		}
	}

	af::freeHost(hostData);
	return image;
}
