#include "psf3dpreviewwidget.h"
#include "sliceviewerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QDataStream>
#include <QtMath>


PSF3DPreviewWidget::PSF3DPreviewWidget(QWidget* parent)
	: QWidget(parent)
	, volumeMaxVal(0.0f)
	, normalizePerSlice(true)
	, logScale(true)
	, syncToDataFrame(false)
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* panelsLayout = new QHBoxLayout();
	this->xyPanel = new SliceViewerWidget(tr("Z"), tr("XY Slice"), this);
	this->xzPanel = new SliceViewerWidget(tr("Y"), tr("XZ Section"), this);
	panelsLayout->addWidget(this->xyPanel, 1);
	panelsLayout->addWidget(this->xzPanel, 1);
	mainLayout->addLayout(panelsLayout, 1);

	connect(this->xyPanel, &SliceViewerWidget::sliceChanged,
			this, &PSF3DPreviewWidget::renderXYSlice);
	connect(this->xzPanel, &SliceViewerWidget::sliceChanged,
			this, &PSF3DPreviewWidget::renderXZSection);

	// Context menu actions shared by both panels
	QAction* normalizeAction = new QAction(tr("Normalize per slice"), this);
	normalizeAction->setCheckable(true);
	normalizeAction->setChecked(true);
	connect(normalizeAction, &QAction::toggled, this, [this](bool checked) {
		this->normalizePerSlice = checked;
		this->reRenderAll();
	});

	QAction* logScaleAction = new QAction(tr("Log scale"), this);
	logScaleAction->setCheckable(true);
	logScaleAction->setChecked(true);
	connect(logScaleAction, &QAction::toggled, this, [this](bool checked) {
		this->logScale = checked;
		this->reRenderAll();
	});

	QAction* saveVolumeAction = new QAction(tr("Save PSF volume..."), this);
	connect(saveVolumeAction, &QAction::triggered,
			this, &PSF3DPreviewWidget::saveVolume);

	QAction* syncFrameAction = new QAction(tr("Sync to Data Frame"), this);
	syncFrameAction->setCheckable(true);
	syncFrameAction->setChecked(false);
	connect(syncFrameAction, &QAction::toggled, this, [this](bool checked) {
		this->syncToDataFrame = checked;
	});

	this->xyPanel->addContextMenuAction(normalizeAction);
	this->xyPanel->addContextMenuAction(logScaleAction);
	this->xyPanel->addContextMenuAction(saveVolumeAction);
	this->xyPanel->addContextMenuAction(syncFrameAction);
	this->xzPanel->addContextMenuAction(normalizeAction);
	this->xzPanel->addContextMenuAction(logScaleAction);
	this->xzPanel->addContextMenuAction(saveVolumeAction);
	this->xzPanel->addContextMenuAction(syncFrameAction);
}

void PSF3DPreviewWidget::updatePSF(af::array psf3D)
{
	if (psf3D.isempty()) return;

	this->psfVolume = psf3D;
	this->volumeMaxVal = af::max<float>(af::flat(psf3D));

	if (psf3D.numdims() > 2 && psf3D.dims(2) > 1) {
		int numZ = static_cast<int>(psf3D.dims(2));
		int numY = static_cast<int>(psf3D.dims(0));

		bool dimsChanged = (this->xyPanel->sliderMaximum() != numZ - 1) ||
		                   (this->xzPanel->sliderMaximum() != numY - 1);

		this->xyPanel->setSliderRange(0, numZ - 1);
		this->xzPanel->setSliderRange(0, numY - 1);
		if (dimsChanged) {
			this->xyPanel->setSliderValue(numZ / 2);
			this->xzPanel->setSliderValue(numY / 2);
		}

		this->renderXYSlice(this->xyPanel->sliderValue());
		this->renderXZSection(this->xzPanel->sliderValue());
	} else {
		// 2D fallback: show single slice
		float maxArg = this->normalizePerSlice ? 0.0f : this->volumeMaxVal;
		QImage img = this->renderSliceToImage(psf3D, maxArg);
		this->xyPanel->setImage(img);
		this->xyPanel->setTitle(tr("XY Slice"));
	}
}

void PSF3DPreviewWidget::renderXYSlice(int zIndex)
{
	if (this->psfVolume.isempty()) return;

	int numZ = static_cast<int>(this->psfVolume.dims(2));
	if (zIndex < 0 || zIndex >= numZ) return;

	af::array slice = this->psfVolume(af::span, af::span, zIndex);
	float maxArg = this->normalizePerSlice ? 0.0f : this->volumeMaxVal;
	QImage img = this->renderSliceToImage(slice, maxArg);
	this->xyPanel->setImage(img);

	this->xyPanel->setTitle(tr("XY (z=%1/%2)").arg(zIndex).arg(numZ - 1));
	this->xyPanel->setValueLabel(QStringLiteral("%1/%2").arg(zIndex).arg(numZ - 1));
}

void PSF3DPreviewWidget::renderXZSection(int yIndex)
{
	if (this->psfVolume.isempty()) return;
	if (this->psfVolume.numdims() < 3 || this->psfVolume.dims(2) <= 1) return;

	int numY = static_cast<int>(this->psfVolume.dims(0));
	if (yIndex < 0 || yIndex >= numY) return;

	// Extract XZ section at given y-row: psfVolume(yIndex, :, :) → [1, W, Z]
	// Reorder to [Z, W] for rendering as image (rows=z, cols=x)
	af::array section = this->psfVolume(yIndex, af::span, af::span);
	section = af::reorder(section, 2, 1, 0); // [Z, W, 1]

	float maxArg = this->normalizePerSlice ? 0.0f : this->volumeMaxVal;
	QImage img = this->renderSliceToImage(section, maxArg);
	this->xzPanel->setImage(img);

	this->xzPanel->setTitle(tr("XZ (y=%1/%2)").arg(yIndex).arg(numY - 1));
	this->xzPanel->setValueLabel(QStringLiteral("%1/%2").arg(yIndex).arg(numY - 1));
}

QImage PSF3DPreviewWidget::renderSliceToImage(const af::array& slice2D, float globalMax)
{
	af::array data = slice2D.as(f32);
	int rows = data.dims(0);
	int cols = data.dims(1);

	QVector<float> hostData(rows * cols);
	data.host(hostData.data());

	// Use global max if provided, otherwise compute per-slice max
	float maxVal = globalMax;
	if (maxVal <= 0.0f) {
		for (float v : qAsConst(hostData)) {
			if (v > maxVal) maxVal = v;
		}
	}

	if (maxVal <= 0.0f) {
		return QImage(cols, rows, QImage::Format_Grayscale8);
	}

	QImage img(cols, rows, QImage::Format_Grayscale8);

	if (this->logScale) {
		const float alpha = 1000.0f;
		float logAlpha = qLn(1.0f + alpha);
		for (int y = 0; y < rows; ++y) {
			uchar* scanLine = img.scanLine(y);
			for (int x = 0; x < cols; ++x) {
				float val = qMax(0.0f, hostData[y + x * rows]); // column-major
				float normalized = val / maxVal;
				scanLine[x] = static_cast<uchar>(255.0f * qLn(1.0f + alpha * normalized) / logAlpha);
			}
		}
	} else {
		for (int y = 0; y < rows; ++y) {
			uchar* scanLine = img.scanLine(y);
			for (int x = 0; x < cols; ++x) {
				float val = qMax(0.0f, hostData[y + x * rows]); // column-major
				scanLine[x] = static_cast<uchar>(255.0f * val / maxVal);
			}
		}
	}

	return img;
}

void PSF3DPreviewWidget::setFrameIndex(int frame)
{
	if (!this->syncToDataFrame) return;
	if (this->psfVolume.isempty()) return;

	int maxZ = static_cast<int>(this->psfVolume.dims(2)) - 1;
	if (maxZ < 1) return;

	int z = qBound(0, frame, maxZ);
	this->xyPanel->setSliderValue(z);
	this->renderXYSlice(z);
}

void PSF3DPreviewWidget::clearPreview()
{
	this->psfVolume = af::array();
	this->volumeMaxVal = 0.0f;

	this->xyPanel->setImage(QImage());
	this->xzPanel->setImage(QImage());

	this->xyPanel->setSliderRange(0, 0);
	this->xzPanel->setSliderRange(0, 0);
	this->xyPanel->setSliderValue(0);
	this->xzPanel->setSliderValue(0);

	this->xyPanel->setTitle(tr("XY Slice"));
	this->xzPanel->setTitle(tr("XZ Section"));
}

void PSF3DPreviewWidget::reRenderAll()
{
	if (this->psfVolume.isempty()) return;
	this->renderXYSlice(this->xyPanel->sliderValue());
	this->renderXZSection(this->xzPanel->sliderValue());
}

void PSF3DPreviewWidget::saveVolume()
{
	if (this->psfVolume.isempty()) return;

	QString fileName = QFileDialog::getSaveFileName(this, tr("Save PSF Volume"),
		QStringLiteral("psf_volume"), tr("TIFF Image (*.tiff *.tif)"));
	if (fileName.isEmpty()) return;

	af::array vol = this->psfVolume.as(f32);
	int height = static_cast<int>(vol.dims(0));
	int width = static_cast<int>(vol.dims(1));
	int numFrames = (vol.numdims() > 2) ? static_cast<int>(vol.dims(2)) : 1;

	// Copy column-major AF data to row-major host buffer for TIFF
	QVector<float> rowMajor(width * height * numFrames);
	float* hostData = vol.host<float>();
	for (int f = 0; f < numFrames; ++f) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				rowMajor[f * width * height + y * width + x] =
					hostData[f * width * height + y + x * height]; // col-major → row-major
			}
		}
	}
	af::freeHost(hostData);

	// Write multi-page 32-bit float TIFF (same pattern as ImageData::saveAsTiff)
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) return;

	QDataStream stream(&file);
	stream.setByteOrder(QDataStream::LittleEndian);

	uint32_t bytesPerFrame = static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * sizeof(float);
	const int numIfdEntries = 10;
	const uint32_t ifdSize = 2 + numIfdEntries * 12 + 4;
	const uint32_t headerSize = 8;
	uint32_t dataStart = headerSize + static_cast<uint32_t>(numFrames) * ifdSize;

	auto writeIfdEntry = [&](uint16_t tag, uint16_t type, uint32_t count, uint32_t value) {
		stream << tag << type << count << value;
	};

	// TIFF Header
	stream.writeRawData("II", 2);
	stream << static_cast<uint16_t>(42);
	stream << headerSize;

	// Write all IFDs
	for (int f = 0; f < numFrames; ++f) {
		uint32_t frameDataOffset = dataStart + static_cast<uint32_t>(f) * bytesPerFrame;
		uint32_t nextIfdOffset = (f < numFrames - 1)
			? (headerSize + static_cast<uint32_t>(f + 1) * ifdSize) : 0;

		stream << static_cast<uint16_t>(numIfdEntries);
		writeIfdEntry(256, 4, 1, static_cast<uint32_t>(width));
		writeIfdEntry(257, 4, 1, static_cast<uint32_t>(height));
		writeIfdEntry(258, 3, 1, 32);
		writeIfdEntry(259, 3, 1, 1);
		writeIfdEntry(262, 3, 1, 1);
		writeIfdEntry(273, 4, 1, frameDataOffset);
		writeIfdEntry(277, 3, 1, 1);
		writeIfdEntry(278, 4, 1, static_cast<uint32_t>(height));
		writeIfdEntry(279, 4, 1, bytesPerFrame);
		writeIfdEntry(339, 3, 1, 3);
		stream << nextIfdOffset;
	}

	// Write frame data
	const char* rawData = reinterpret_cast<const char*>(rowMajor.constData());
	for (int f = 0; f < numFrames; ++f) {
		stream.writeRawData(rawData + f * bytesPerFrame, static_cast<int>(bytesPerFrame));
	}

	file.close();
}
