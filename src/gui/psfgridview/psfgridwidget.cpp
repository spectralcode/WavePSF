#include "psfgridwidget.h"
#include "utils/logging.h"

#include <cmath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QFileDialog>
#include <QFile>
#include <QTimer>
#include <QDataStream>

namespace {
	const QString SETTINGS_GROUP    = QStringLiteral("psf_grid_widget");
	const QString KEY_CROP_SIZE      = QStringLiteral("crop_size");
	const QString KEY_LIVE_UPDATE    = QStringLiteral("live_update");
	const QString KEY_SPLITTER_STATE = QStringLiteral("splitter_state");
	const int     DEF_CROP_SIZE      = 32;
	const bool    DEF_LIVE_UPDATE    = true;
}

PSFGridWidget::PSFGridWidget(QWidget* parent)
	: QWidget(parent)
	, mosaicItem(nullptr)
	, highlightRect(nullptr)
	, currentPatchX(0)
	, currentPatchY(0)
	, currentFrame(0)
	, patchCols(1)
	, patchRows(1)
	, syncActive(false)
{
	this->setupUI();
}

QString PSFGridWidget::getName() const
{
	return SETTINGS_GROUP;
}

QVariantMap PSFGridWidget::getSettings() const
{
	QVariantMap settings;
	settings[KEY_CROP_SIZE] = this->cropSizeSpinBox->value();
	settings[KEY_LIVE_UPDATE] = this->liveUpdateCheckBox->isChecked();
	settings[KEY_SPLITTER_STATE] = this->splitter->saveState();
	return settings;
}

void PSFGridWidget::setSettings(const QVariantMap& settings)
{
	this->cropSizeSpinBox->setValue(settings.value(KEY_CROP_SIZE, DEF_CROP_SIZE).toInt());
	this->liveUpdateCheckBox->setChecked(settings.value(KEY_LIVE_UPDATE, DEF_LIVE_UPDATE).toBool());
	if (settings.contains(KEY_SPLITTER_STATE)) {
		this->splitter->restoreState(settings.value(KEY_SPLITTER_STATE).toByteArray());
	}
}

void PSFGridWidget::setupUI()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	this->splitter = new QSplitter(Qt::Vertical, this);

	// Graphics view (top)
	this->graphicsScene = new QGraphicsScene(this);
	this->graphicsView = new QGraphicsView(this->graphicsScene, this);
	this->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
	this->graphicsView->setRenderHint(QPainter::SmoothPixmapTransform, false);
	this->graphicsView->setBackgroundBrush(QBrush(QColor(30, 30, 30)));
	this->graphicsView->setContextMenuPolicy(Qt::CustomContextMenu);
	this->graphicsView->viewport()->installEventFilter(this);
	this->graphicsView->installEventFilter(this);
	this->splitter->addWidget(this->graphicsView);

	// Controls (bottom)
	QWidget* controlsWidget = new QWidget(this);
	QHBoxLayout* controlsLayout = new QHBoxLayout(controlsWidget);
	controlsLayout->setContentsMargins(4, 4, 4, 4);

	this->generateButton = new QPushButton(tr("Generate"), this);
	controlsLayout->addWidget(this->generateButton);

	this->liveUpdateCheckBox = new QCheckBox(tr("Auto-update"), this);
	this->liveUpdateCheckBox->setChecked(DEF_LIVE_UPDATE);
	this->liveUpdateCheckBox->setToolTip(tr("Automatically update the grid when coefficients change"));
	controlsLayout->addWidget(this->liveUpdateCheckBox);

	controlsLayout->addStretch();

	controlsLayout->addWidget(new QLabel(tr("Crop:"), this));
	this->cropSizeSpinBox = new QSpinBox(this);
	this->cropSizeSpinBox->setRange(8, 1024);
	this->cropSizeSpinBox->setValue(DEF_CROP_SIZE);
	this->cropSizeSpinBox->setSuffix(tr(" px"));
	controlsLayout->addWidget(this->cropSizeSpinBox);

	controlsLayout->addStretch();

	this->infoLabel = new QLabel(this);
	controlsLayout->addWidget(this->infoLabel);

	this->splitter->addWidget(controlsWidget);

	this->splitter->setStretchFactor(0, 3);
	this->splitter->setStretchFactor(1, 0);

	mainLayout->addWidget(this->splitter);

	// Connections
	connect(this->generateButton, &QPushButton::clicked,
	        this, &PSFGridWidget::onGenerateClicked);
	connect(this->graphicsView, &QWidget::customContextMenuRequested,
	        this, &PSFGridWidget::showContextMenu);
	connect(this->liveUpdateCheckBox, &QCheckBox::toggled,
	        this->generateButton, &QPushButton::setDisabled);
	this->generateButton->setDisabled(this->liveUpdateCheckBox->isChecked());
}

void PSFGridWidget::onGenerateClicked()
{
	emit generateRequested(this->currentFrame, this->cropSizeSpinBox->value());
}

void PSFGridWidget::displayPSFGrid(const PSFGridResult& result)
{
	this->lastResult = result;

	// Clear existing items
	this->graphicsScene->clear();
	this->mosaicItem = nullptr;
	this->highlightRect = nullptr;

	if (result.mosaicImage.isNull()) {
		return;
	}

	// Add mosaic pixmap
	this->mosaicItem = this->graphicsScene->addPixmap(
		QPixmap::fromImage(result.mosaicImage));

	// Create highlight rectangle
	QPen highlightPen(QColor(255, 200, 0), 2);
	highlightPen.setCosmetic(true);
	this->highlightRect = this->graphicsScene->addRect(0, 0, 1, 1, highlightPen);
	this->highlightRect->setZValue(1);

	this->graphicsScene->setSceneRect(result.mosaicImage.rect());

	this->updateHighlight();

	this->infoLabel->setText(QString("%1x%2 patches, %3 px crop")
		.arg(result.cols).arg(result.rows).arg(result.cellSize));

	// Fit view to scene, preserving current orientation
	this->graphicsView->resetTransform();
	this->graphicsView->fitInView(this->graphicsScene->sceneRect(), Qt::KeepAspectRatio);
	if (!this->viewOrientation.isIdentity()) {
		this->graphicsView->setTransform(
			this->graphicsView->transform() * this->viewOrientation);
	}
}

void PSFGridWidget::setCurrentPatch(int x, int y)
{
	this->currentPatchX = x;
	this->currentPatchY = y;
	this->updateHighlight();
}

void PSFGridWidget::setPatchGridDimensions(int cols, int rows, int borderExtension)
{
	Q_UNUSED(borderExtension);
	this->patchCols = cols;
	this->patchRows = rows;
	if (this->liveUpdateCheckBox->isChecked() && !this->lastResult.mosaicImage.isNull()) {
		// Defer to next event loop iteration so parameter table is resized first
		QTimer::singleShot(0, this, [this]() {
			emit generateRequested(this->currentFrame, this->cropSizeSpinBox->value());
		});
	}
}

void PSFGridWidget::setCurrentFrame(int frame)
{
	this->currentFrame = frame;
}

void PSFGridWidget::rotate90()
{
	if (this->syncActive) return;
	this->viewOrientation = this->viewOrientation * QTransform().rotate(-90);
	QTransform current = this->graphicsView->transform();
	double myScale = std::sqrt(current.m11() * current.m11()
	                         + current.m21() * current.m21());
	this->graphicsView->setTransform(
		QTransform::fromScale(myScale, myScale) * this->viewOrientation);
}

void PSFGridWidget::flipH()
{
	if (this->syncActive) return;
	this->viewOrientation = this->viewOrientation * QTransform(-1, 0, 0, 1, 0, 0);
	QTransform current = this->graphicsView->transform();
	double myScale = std::sqrt(current.m11() * current.m11()
	                         + current.m21() * current.m21());
	this->graphicsView->setTransform(
		QTransform::fromScale(myScale, myScale) * this->viewOrientation);
}

void PSFGridWidget::flipV()
{
	if (this->syncActive) return;
	this->viewOrientation = this->viewOrientation * QTransform(1, 0, 0, -1, 0, 0);
	QTransform current = this->graphicsView->transform();
	double myScale = std::sqrt(current.m11() * current.m11()
	                         + current.m21() * current.m21());
	this->graphicsView->setTransform(
		QTransform::fromScale(myScale, myScale) * this->viewOrientation);
}

void PSFGridWidget::applyViewTransform(QTransform viewTransform, QPointF)
{
	if (!this->syncActive) return;

	// Extract rotation/flip by normalizing out the scale component
	double scale = std::sqrt(viewTransform.m11() * viewTransform.m11()
	                       + viewTransform.m21() * viewTransform.m21());
	if (scale < 1e-10) return;

	QTransform newOrientation(
		viewTransform.m11() / scale, viewTransform.m12() / scale,
		viewTransform.m21() / scale, viewTransform.m22() / scale, 0, 0);

	// Only update if orientation actually changed (ignore zoom/pan changes)
	if (newOrientation == this->viewOrientation) return;
	this->viewOrientation = newOrientation;

	// Preserve PSF grid's own zoom, apply new orientation
	QTransform current = this->graphicsView->transform();
	double myScale = std::sqrt(current.m11() * current.m11()
	                         + current.m21() * current.m21());
	this->graphicsView->setTransform(
		QTransform::fromScale(myScale, myScale) * this->viewOrientation);
}

void PSFGridWidget::setSyncActive(bool active)
{
	this->syncActive = active;
}

void PSFGridWidget::updateSinglePSF(af::array psf, int patchX, int patchY)
{
	if (!this->liveUpdateCheckBox->isChecked()) {
		return;
	}
	if (!this->isVisible()) {
		return;
	}
	if (this->lastResult.mosaicImage.isNull() || this->mosaicItem == nullptr) {
		// No mosaic yet — trigger full generation
		emit generateRequested(this->currentFrame, this->cropSizeSpinBox->value());
		return;
	}

	// Bounds check
	if (patchX < 0 || patchX >= this->lastResult.cols ||
	    patchY < 0 || patchY >= this->lastResult.rows) {
		return;
	}

	// Extract focal plane if PSF is 3D
	if (psf.numdims() > 2 && psf.dims(2) > 1) {
		int centerZ = static_cast<int>(psf.dims(2)) / 2;
		psf = psf(af::span, af::span, centerZ);
	}

	int cellSize = this->lastResult.cellSize;

	// Center-crop (same logic as PSFGridGenerator::generate)
	int psfSize = static_cast<int>(psf.dims(0));
	if (cellSize < psfSize) {
		int offset = (psfSize - cellSize) / 2;
		psf = psf(af::seq(offset, offset + cellSize - 1),
		          af::seq(offset, offset + cellSize - 1));
	}

	// Transpose for correct display orientation
	psf = af::transpose(psf);

	// Store updated PSF
	int patchIdx = patchY * this->lastResult.cols + patchX;
	this->lastResult.rawPSFs[patchIdx] = psf;

	// Convert to grayscale (same logic as PSFGridGenerator::afArrayToGrayscaleImage)
	int h = static_cast<int>(psf.dims(0));
	int w = static_cast<int>(psf.dims(1));
	af::array floatArr = psf.as(af::dtype::f32);
	float* hostData = floatArr.host<float>();

	float peak = 0.0f;
	for (int i = 0; i < w * h; i++) {
		if (hostData[i] > peak) {
			peak = hostData[i];
		}
	}
	float scale = (peak > 0.0f) ? 255.0f / peak : 0.0f;

	// Paint cell into mosaic
	int stride = cellSize + this->lastResult.spacing;
	int destX = patchX * stride;
	int destY = patchY * stride;

	for (int cy = 0; cy < h && (destY + cy) < this->lastResult.mosaicImage.height(); cy++) {
		uchar* dstLine = this->lastResult.mosaicImage.scanLine(destY + cy);
		for (int cx = 0; cx < w && (destX + cx) < this->lastResult.mosaicImage.width(); cx++) {
			float val = hostData[cy + cx * h];
			dstLine[destX + cx] = static_cast<uchar>(val * scale);
		}
	}

	af::freeHost(hostData);

	// Update display
	this->mosaicItem->setPixmap(QPixmap::fromImage(this->lastResult.mosaicImage));
}

void PSFGridWidget::updateHighlight()
{
	if (this->highlightRect == nullptr || this->lastResult.mosaicImage.isNull()) {
		return;
	}

	int stride = this->lastResult.cellSize + this->lastResult.spacing;
	int x = this->currentPatchX * stride;
	int y = this->currentPatchY * stride;
	this->highlightRect->setRect(x, y, this->lastResult.cellSize, this->lastResult.cellSize);
}

QPair<int, int> PSFGridWidget::cellAtScenePos(QPointF scenePos) const
{
	if (this->lastResult.mosaicImage.isNull()) {
		return qMakePair(-1, -1);
	}

	int stride = this->lastResult.cellSize + this->lastResult.spacing;
	int col = static_cast<int>(scenePos.x()) / stride;
	int row = static_cast<int>(scenePos.y()) / stride;

	// Check within cell bounds (not in spacing)
	int localX = static_cast<int>(scenePos.x()) % stride;
	int localY = static_cast<int>(scenePos.y()) % stride;
	if (localX >= this->lastResult.cellSize || localY >= this->lastResult.cellSize) {
		return qMakePair(-1, -1);
	}

	if (col < 0 || col >= this->lastResult.cols || row < 0 || row >= this->lastResult.rows) {
		return qMakePair(-1, -1);
	}

	return qMakePair(col, row);
}

bool PSFGridWidget::eventFilter(QObject* obj, QEvent* event)
{
	if (obj == this->graphicsView && event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		switch (keyEvent->key()) {
		case Qt::Key_R:
			this->rotate90();
			return true;
		case Qt::Key_H:
			this->flipH();
			return true;
		case Qt::Key_V:
			if (!(keyEvent->modifiers() & Qt::ControlModifier)) {
				this->flipV();
				return true;
			}
			break;
		}
	}

	if (obj == this->graphicsView->viewport()) {
		if (event->type() == QEvent::Wheel) {
			QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
			double factor = (wheelEvent->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
			this->graphicsView->scale(factor, factor);
			return true;
		}

		if (event->type() == QEvent::MouseButtonDblClick) {
			if (!this->lastResult.mosaicImage.isNull()) {
				this->graphicsView->resetTransform();
				this->graphicsView->fitInView(this->graphicsScene->sceneRect(), Qt::KeepAspectRatio);
				if (!this->viewOrientation.isIdentity()) {
					this->graphicsView->setTransform(
						this->graphicsView->transform() * this->viewOrientation);
				}
			}
			return true;
		}

		if (event->type() == QEvent::MouseButtonPress) {
			QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->button() == Qt::LeftButton &&
			    !(mouseEvent->modifiers() & Qt::ControlModifier)) {
				QPointF scenePos = this->graphicsView->mapToScene(mouseEvent->pos());
				QPair<int, int> cell = this->cellAtScenePos(scenePos);
				if (cell.first >= 0 && cell.second >= 0) {
					emit patchClicked(cell.first, cell.second);
				}
			}
		}
	}

	return QWidget::eventFilter(obj, event);
}

void PSFGridWidget::showContextMenu(const QPoint& pos)
{
	QMenu menu(this);

	QAction* saveTifAction = menu.addAction(tr("Save as TIF..."));
	QAction* savePngAction = menu.addAction(tr("Save as PNG..."));
	menu.addSeparator();
	QAction* resetViewAction = menu.addAction(tr("Reset View"));

	bool hasMosaic = !this->lastResult.mosaicImage.isNull();
	saveTifAction->setEnabled(hasMosaic);
	savePngAction->setEnabled(hasMosaic);

	QAction* chosen = menu.exec(this->graphicsView->mapToGlobal(pos));
	if (chosen == saveTifAction) {
		this->saveMosaicAs("tif");
	} else if (chosen == savePngAction) {
		this->saveMosaicAs("png");
	} else if (chosen == resetViewAction) {
		if (!this->lastResult.mosaicImage.isNull()) {
			this->graphicsView->resetTransform();
			this->graphicsView->fitInView(this->graphicsScene->sceneRect(), Qt::KeepAspectRatio);
			if (!this->viewOrientation.isIdentity()) {
				this->graphicsView->setTransform(
					this->graphicsView->transform() * this->viewOrientation);
			}
		}
	}
}

void PSFGridWidget::saveMosaicAs(const QString& format)
{
	if (this->lastResult.mosaicImage.isNull()) {
		return;
	}

	QString filter;
	if (format == "tif") {
		filter = tr("TIFF Image (*.tif)");
	} else {
		filter = tr("PNG Image (*.png)");
	}

	QString filePath = QFileDialog::getSaveFileName(this, tr("Save PSF Grid"), QString(), filter);
	if (filePath.isEmpty()) {
		return;
	}

	if (format == "tif") {
		this->saveMosaicAsTif(filePath);
	} else {
		this->lastResult.mosaicImage.save(filePath);
		LOG_INFO() << "PSF grid mosaic saved as PNG:" << filePath;
	}
}

void PSFGridWidget::saveMosaicAsTif(const QString& filePath)
{
	// Compose float32 mosaic from raw PSFs
	int cols = this->lastResult.cols;
	int rows = this->lastResult.rows;
	int cellSize = this->lastResult.cellSize;
	int spacing = this->lastResult.spacing;
	int mosaicWidth = cols * cellSize + (cols - 1) * spacing;
	int mosaicHeight = rows * cellSize + (rows - 1) * spacing;
	int stride = cellSize + spacing;

	QVector<float> mosaicData(mosaicWidth * mosaicHeight, 0.0f);

	for (int py = 0; py < rows; py++) {
		for (int px = 0; px < cols; px++) {
			int patchIdx = py * cols + px;
			if (patchIdx >= this->lastResult.rawPSFs.size() || this->lastResult.rawPSFs[patchIdx].isempty()) {
				continue;
			}

			af::array floatArr = this->lastResult.rawPSFs[patchIdx].as(af::dtype::f32);
			float* hostData = floatArr.host<float>();
			int h = static_cast<int>(floatArr.dims(0));
			int w = static_cast<int>(floatArr.dims(1));

			int destX = px * stride;
			int destY = py * stride;

			// AF column-major → row-major mosaic
			for (int cy = 0; cy < h && (destY + cy) < mosaicHeight; cy++) {
				for (int cx = 0; cx < w && (destX + cx) < mosaicWidth; cx++) {
					mosaicData[(destY + cy) * mosaicWidth + (destX + cx)] = hostData[cy + cx * h];
				}
			}

			af::freeHost(hostData);
		}
	}

	// Write single-page 32-bit float TIFF (same pattern as PSFFileManager)
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly)) {
		LOG_WARNING() << "Could not write PSF grid TIF:" << file.errorString();
		return;
	}

	QDataStream stream(&file);
	stream.setByteOrder(QDataStream::LittleEndian);

	uint32_t bytesPerFrame = static_cast<uint32_t>(mosaicWidth) * static_cast<uint32_t>(mosaicHeight) * sizeof(float);
	const int numIfdEntries = 10;
	uint32_t ifdSize = 2 + numIfdEntries * 12 + 4;
	uint32_t headerSize = 8;
	uint32_t dataStart = headerSize + ifdSize;

	auto writeIfdEntry = [&](uint16_t tag, uint16_t type, uint32_t count, uint32_t value) {
		stream << tag << type << count << value;
	};

	// TIFF Header
	stream.writeRawData("II", 2);
	stream << static_cast<uint16_t>(42);
	stream << headerSize;

	// IFD
	stream << static_cast<uint16_t>(numIfdEntries);
	writeIfdEntry(256, 4, 1, static_cast<uint32_t>(mosaicWidth));
	writeIfdEntry(257, 4, 1, static_cast<uint32_t>(mosaicHeight));
	writeIfdEntry(258, 3, 1, 32);
	writeIfdEntry(259, 3, 1, 1);
	writeIfdEntry(262, 3, 1, 1);
	writeIfdEntry(273, 4, 1, dataStart);
	writeIfdEntry(277, 3, 1, 1);
	writeIfdEntry(278, 4, 1, static_cast<uint32_t>(mosaicHeight));
	writeIfdEntry(279, 4, 1, bytesPerFrame);
	writeIfdEntry(339, 3, 1, 3);
	stream << static_cast<uint32_t>(0);

	// Row-major data is already in the correct order
	stream.writeRawData(reinterpret_cast<const char*>(mosaicData.constData()),
	                    static_cast<int>(bytesPerFrame));

	file.close();
	LOG_INFO() << "PSF grid mosaic saved as TIF:" << filePath
			   << "(" << mosaicWidth << "x" << mosaicHeight << ", 32-bit float)";
}
