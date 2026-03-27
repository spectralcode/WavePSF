#define NOMINMAX
#include "volumetricprocessor.h"
#include "volumetricdeconvolver.h"
#include "controller/imagesession.h"
#include "utils/logging.h"
#include <QDir>
#include <QFileInfo>
#include <QProgressDialog>
#include <QMessageBox>
#include <QApplication>

VolumetricProcessor::VolumetricProcessor(QObject* parent)
	: QObject(parent)
{
}

af::array VolumetricProcessor::loadVolume(ImageSession* imageSession)
{
	int frames = imageSession->getInputFrames();
	int H = imageSession->getInputHeight();
	int W = imageSession->getInputWidth();

	if (frames == 0 || H == 0 || W == 0) {
		return af::array();
	}

	af::array volume = af::constant(0.0f, W, H, frames);
	for (int f = 0; f < frames; ++f) {
		af::array frame = imageSession->getInputFrame(f);
		if (frame.isempty()) {
			LOG_WARNING() << "Failed to load frame" << f << "for 3D volume";
			return af::array();
		}
		volume(af::span, af::span, f) = frame.as(f32);
	}
	return volume;
}

af::array VolumetricProcessor::loadPSFVolume(const QString& folderPath)
{
	QDir dir(folderPath);
	QStringList filters;
	filters << "*.tif" << "*.tiff";
	QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

	if (files.isEmpty()) {
		emit error(QString("No TIFF files found in PSF folder: %1").arg(folderPath));
		return af::array();
	}

	// Load first file to get dimensions
	af::array first = af::loadImageNative(
		files[0].absoluteFilePath().toStdString().c_str()).as(f32);
	if (first.isempty()) {
		emit error(QString("Failed to load first PSF slice: %1")
			.arg(files[0].absoluteFilePath()));
		return af::array();
	}

	int pH = first.dims(0);
	int pW = first.dims(1);
	int pD = files.size();

	af::array psfVolume = af::constant(0.0f, pH, pW, pD);
	psfVolume(af::span, af::span, 0) = first;

	for (int i = 1; i < pD; ++i) {
		af::array slice = af::loadImageNative(
			files[i].absoluteFilePath().toStdString().c_str()).as(f32);
		if (slice.dims(0) != pH || slice.dims(1) != pW) {
			emit error(QString("PSF slice %1 has different dimensions (%2x%3) than first slice (%4x%5)")
				.arg(files[i].fileName())
				.arg(slice.dims(0)).arg(slice.dims(1))
				.arg(pH).arg(pW));
			return af::array();
		}
		psfVolume(af::span, af::span, i) = slice;
	}

	// af::loadImageNative returns (height, width) but the input volume
	// uses (width, height) convention. Transpose to match.
	psfVolume = af::reorder(psfVolume, 1, 0, 2);

	LOG_INFO() << "Loaded 3D PSF:" << psfVolume.dims(0) << "x"
			   << psfVolume.dims(1) << "x" << psfVolume.dims(2)
			   << "from" << folderPath;
	return psfVolume;
}

void VolumetricProcessor::writeVolumeToOutput(ImageSession* imageSession,
	const af::array& volume)
{
	int frames = volume.dims(2);
	for (int f = 0; f < frames; ++f) {
		af::array slice = volume(af::span, af::span, f);
		imageSession->setOutputFrame(f, slice);
	}
}

bool VolumetricProcessor::execute(ImageSession* imageSession,
	const QString& psfFolderPath, int iterations, QWidget* parentWidget)
{
	if (imageSession == nullptr || !imageSession->hasInputData()) {
		emit error("No input data loaded.");
		return false;
	}

	int H = imageSession->getInputHeight();
	int W = imageSession->getInputWidth();
	int D = imageSession->getInputFrames();

	// GPU memory check
	size_t requiredBytes = VolumetricDeconvolver::estimateGPUMemory(W, H, D);
	double requiredMB = static_cast<double>(requiredBytes) / (1024.0 * 1024.0);

	QString memMsg = QString("3D Deconvolution requires approximately %1 MB of GPU memory.\n"
		"Volume: %2 x %3 x %4\nIterations: %5\n\nProceed?")
		.arg(requiredMB, 0, 'f', 0).arg(H).arg(W).arg(D).arg(iterations);

	if (QMessageBox::question(parentWidget, "3D Deconvolution",
		memMsg, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
		return false;
	}

	// Progress dialog
	QProgressDialog progress("Loading image volume...", "Cancel", 0, 0, parentWidget);
	progress.setWindowTitle("3D Deconvolution");
	progress.setWindowModality(Qt::ApplicationModal);
	progress.setMinimumDuration(0);
	progress.show();
	QApplication::processEvents();

	// Load input volume
	af::array volume = this->loadVolume(imageSession);
	if (volume.isempty()) {
		emit error("Failed to load image volume.");
		return false;
	}
	if (progress.wasCanceled()) return false;

	// Load 3D PSF
	progress.setLabelText("Loading 3D PSF...");
	QApplication::processEvents();

	af::array psf = this->loadPSFVolume(psfFolderPath);
	if (psf.isempty()) return false;
	if (progress.wasCanceled()) return false;

	// Run 3D Richardson-Lucy
	progress.setMaximum(iterations);
	progress.setLabelText("Running 3D Richardson-Lucy...");
	progress.setValue(0);

	VolumetricDeconvolver deconvolver(this);
	connect(&deconvolver, &VolumetricDeconvolver::iterationCompleted,
		&progress, [&progress](int current, int total) {
			progress.setValue(current);
			progress.setLabelText(
				QString("3D Richardson-Lucy: iteration %1 / %2")
					.arg(current).arg(total));
			QApplication::processEvents();
		});

	af::array result = deconvolver.deconvolve(volume, psf, iterations);
	if (result.isempty()) return false;
	if (progress.wasCanceled()) return false;

	// Write result
	progress.setLabelText("Writing deconvolved volume...");
	progress.setMaximum(0);
	QApplication::processEvents();

	this->writeVolumeToOutput(imageSession, result);

	progress.close();

	LOG_INFO() << "3D deconvolution completed:" << H << "x" << W << "x" << D
			   << "," << iterations << "iterations";
	return true;
}
