#include "psfgridgenerator.h"
#include "ipsfgenerator.h"
#include "psfgeneratorfactory.h"
#include "core/processing/patchdeconvolutionprocessor.h"
#include "utils/afdevicemanager.h"
#include "utils/logging.h"
#include <QtConcurrent>
#include <QScopedPointer>
#include <cstring>
#include <exception>

PSFGridGenerator::PSFGridGenerator(QObject* parent)
	: QObject(parent)
{
	this->cancelRequested.storeRelease(0);
	connect(&this->watcher, &QFutureWatcher<PSFGridResult>::finished,
			this, [this]() {
				emit this->finished(this->watcher.result());
			});
}

PSFGridGenerator::~PSFGridGenerator()
{
	this->cancel();
	this->watcher.waitForFinished();
}

void PSFGridGenerator::generate(const PSFGridRequest& request)
{
	if (this->watcher.isRunning()) {
		return;
	}

	this->cancelRequested.storeRelease(0);
	emit this->started(request.cols * request.rows);
	this->watcher.setFuture(QtConcurrent::run([this, request]() {
		return this->generateGrid(request);
	}));
}

void PSFGridGenerator::cancel()
{
	this->cancelRequested.storeRelease(1);
}

PSFGridResult PSFGridGenerator::generateGrid(
	const PSFGridRequest& request)
{
	PSFGridResult result;
	result.cols = request.cols;
	result.rows = request.rows;
	result.cellSize = request.cropSize;

	try {
		const int totalPatches = request.cols * request.rows;
		const bool usesLoadedPSFs = !request.loadedPSFs.isEmpty();
		if (request.cols <= 0 || request.rows <= 0 || request.cropSize <= 0
			|| (usesLoadedPSFs
				? request.loadedPSFs.size() != totalPatches
				: request.coefficients.size() != totalPatches)) {
			result.status = RunStatus::FAILED;
			result.message = tr("Invalid PSF grid request.");
			return result;
		}

		AFDeviceManager::setDeviceForCurrentThread(
			request.afBackend, request.afDeviceId);

		QScopedPointer<IPSFGenerator> generator;
		if (!usesLoadedPSFs) {
			generator.reset(PSFGeneratorFactory::create(
				request.psfSettings.generatorTypeName, nullptr));
			if (generator.isNull()) {
				result.status = RunStatus::FAILED;
				result.message = tr("Unknown PSF generator: %1")
					.arg(request.psfSettings.generatorTypeName);
				return result;
			}

			const QVariantMap settings =
				request.psfSettings.allGeneratorSettings.value(
					request.psfSettings.generatorTypeName);
			if (!settings.isEmpty()) {
				generator->deserializeSettings(settings);
			}
		}

		result = createEmptyGrid(
			request.cols, request.rows, request.cropSize);
		int missingPatches = 0;
		for (int patchIdx = 0; patchIdx < totalPatches; ++patchIdx) {
			if (this->cancelRequested.loadAcquire() != 0) {
				result.status = RunStatus::CANCELLED;
				return result;
			}

			af::array psf;
			if (usesLoadedPSFs) {
				psf = request.loadedPSFs.at(patchIdx);
			} else {
				generator->setAllCoefficients(
					request.coefficients.at(patchIdx));
				PSFRequest psfRequest;
				psfRequest.gridSize = request.psfSettings.gridSize;
				psfRequest.frame = request.frame;
				psfRequest.patchIdx = patchIdx;
				psf = generator->generatePSF(psfRequest);
			}

			if (!updatePatch(
				result, psf, request.frame,
				patchIdx % request.cols, patchIdx / request.cols)) {
				++missingPatches;
			}

			emit this->progressUpdated(patchIdx + 1, totalPatches);
		}

		if (this->cancelRequested.loadAcquire() != 0) {
			result.status = RunStatus::CANCELLED;
			return result;
		}
		if (missingPatches > 0) {
			result.status = RunStatus::FAILED;
			result.message = tr("No PSF is available for %1 of %2 patches.")
				.arg(missingPatches)
				.arg(totalPatches);
			return result;
		}

		LOG_INFO() << "PSF grid generated:" << request.cols << "x" << request.rows
				   << "patches, crop=" << request.cropSize;
	} catch (const af::exception& e) {
		result.status = RunStatus::FAILED;
		result.message = tr("PSF grid generation failed: %1")
			.arg(QString::fromUtf8(e.what()));
	} catch (const std::exception& e) {
		result.status = RunStatus::FAILED;
		result.message = tr("PSF grid generation failed: %1")
			.arg(QString::fromUtf8(e.what()));
	} catch (...) {
		result.status = RunStatus::FAILED;
		result.message = tr("PSF grid generation failed with an unknown error.");
	}
	return result;
}

PSFGridResult PSFGridGenerator::createEmptyGrid(
	int cols, int rows, int cellSize)
{
	PSFGridResult result;
	result.cols = cols;
	result.rows = rows;
	result.cellSize = cellSize;
	if (cols <= 0 || rows <= 0 || cellSize <= 0) {
		return result;
	}

	result.rawPSFs.resize(cols * rows);
	result.mosaicImage = QImage(
		cols * cellSize + (cols - 1) * result.spacing,
		rows * cellSize + (rows - 1) * result.spacing,
		QImage::Format_Grayscale8);
	result.mosaicImage.fill(0);
	return result;
}

bool PSFGridGenerator::updatePatch(
	PSFGridResult& grid, af::array psf,
	int frame, int patchX, int patchY)
{
	if (psf.isempty() || grid.mosaicImage.isNull()
		|| patchX < 0 || patchX >= grid.cols
		|| patchY < 0 || patchY >= grid.rows) {
		return false;
	}

	psf = PatchDeconvolutionProcessor::extractPSFFrame(psf, frame);
	if (psf.isempty()) {
		return false;
	}

	const int psfSize = static_cast<int>(psf.dims(0));
	if (grid.cellSize < psfSize) {
		const int offset = (psfSize - grid.cellSize) / 2;
		psf = psf(
			af::seq(offset, offset + grid.cellSize - 1),
			af::seq(offset, offset + grid.cellSize - 1));
	}
	psf = af::transpose(psf);

	const int patchIdx = patchY * grid.cols + patchX;
	if (patchIdx >= grid.rawPSFs.size()) {
		return false;
	}
	grid.rawPSFs[patchIdx] = psf;

	const QImage cellImage = afArrayToGrayscaleImage(psf);
	const int destX = patchX * (grid.cellSize + grid.spacing);
	const int destY = patchY * (grid.cellSize + grid.spacing);
	const int copyWidth = qMin(cellImage.width(), grid.cellSize);
	const int copyHeight = qMin(cellImage.height(), grid.cellSize);
	for (int y = 0; y < copyHeight; ++y) {
		std::memcpy(
			grid.mosaicImage.scanLine(destY + y) + destX,
			cellImage.constScanLine(y),
			static_cast<size_t>(copyWidth));
	}
	return true;
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
