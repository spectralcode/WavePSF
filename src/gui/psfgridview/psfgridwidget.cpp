#include "psfgridwidget.h"
#include "utils/logging.h"

#include <cmath>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QCheckBox>
#include <QPushButton>
#include <QProgressBar>
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
	const QString KEY_AUTO_PATCH     = QStringLiteral("live_update");
	const QString KEY_AUTO_FRAME     = QStringLiteral("auto_update_frame");
	const QString KEY_SPLITTER_STATE = QStringLiteral("splitter_state");
	const int     DEF_CROP_SIZE      = 32;
	const bool    DEF_AUTO_PATCH     = true;
	const bool    DEF_AUTO_FRAME     = false;
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
	, generationInProgress(false)
	, discardPendingResult(false)
	, frameUpdatePending(false)
	, manualPatchUpdate(false)
	, gridOutdated(true)
	, resetGridOnNextPatchUpdate(false)
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
	settings[KEY_AUTO_PATCH] = this->autoUpdatePatchCheckBox->isChecked();
	settings[KEY_AUTO_FRAME] = this->autoUpdateFrameCheckBox->isChecked();
	settings[KEY_SPLITTER_STATE] = this->splitter->saveState();
	return settings;
}

void PSFGridWidget::setSettings(const QVariantMap& settings)
{
	this->cropSizeSpinBox->setValue(settings.value(KEY_CROP_SIZE, DEF_CROP_SIZE).toInt());
	this->autoUpdatePatchCheckBox->setChecked(
		settings.value(KEY_AUTO_PATCH, DEF_AUTO_PATCH).toBool());
	this->autoUpdateFrameCheckBox->setChecked(
		settings.value(KEY_AUTO_FRAME, DEF_AUTO_FRAME).toBool());
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
	QGridLayout* controlsLayout = new QGridLayout(controlsWidget);
	controlsLayout->setContentsMargins(4, 4, 4, 4);
	controlsLayout->setHorizontalSpacing(6);
	controlsLayout->setVerticalSpacing(2);

	this->updatePatchButton = new QPushButton(tr("Update patch"), this);
	this->updatePatchButton->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Fixed);
	controlsLayout->addWidget(this->updatePatchButton, 0, 0);

	this->autoUpdatePatchCheckBox = new QCheckBox(tr("Auto update patch"), this);
	this->autoUpdatePatchCheckBox->setChecked(DEF_AUTO_PATCH);
	controlsLayout->addWidget(this->autoUpdatePatchCheckBox, 0, 1);

	controlsLayout->addWidget(new QLabel(tr("Crop:"), this), 0, 2);
	this->cropSizeSpinBox = new QSpinBox(this);
	this->cropSizeSpinBox->setRange(8, 1024);
	this->cropSizeSpinBox->setValue(DEF_CROP_SIZE);
	this->cropSizeSpinBox->setSuffix(tr(" px"));
	controlsLayout->addWidget(this->cropSizeSpinBox, 0, 3);

	this->updateFrameButton = new QPushButton(tr("Update frame"), this);
	this->updateFrameButton->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Fixed);
	controlsLayout->addWidget(this->updateFrameButton, 1, 0);

	this->autoUpdateFrameCheckBox = new QCheckBox(tr("Auto update frame"), this);
	this->autoUpdateFrameCheckBox->setChecked(DEF_AUTO_FRAME);
	controlsLayout->addWidget(this->autoUpdateFrameCheckBox, 1, 1);

	this->progressBar = new QProgressBar(this);
	this->progressBar->setRange(0, 1);
	this->progressBar->setValue(0);
	this->progressBar->setFormat(QString());
	this->progressBar->setAlignment(Qt::AlignCenter);
	controlsLayout->addWidget(this->progressBar, 1, 2, 1, 2);
	controlsLayout->setColumnStretch(3, 1);

	this->splitter->addWidget(controlsWidget);
	this->splitter->setCollapsible(1, false);

	this->splitter->setStretchFactor(0, 3);
	this->splitter->setStretchFactor(1, 0);

	mainLayout->addWidget(this->splitter);

	// Connections
	connect(this->updatePatchButton, &QPushButton::clicked,
	        this, &PSFGridWidget::onUpdatePatchClicked);
	connect(this->updateFrameButton, &QPushButton::clicked,
	        this, &PSFGridWidget::onUpdateFrameClicked);
	connect(this->graphicsView, &QWidget::customContextMenuRequested,
	        this, &PSFGridWidget::showContextMenu);
	connect(this->cropSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
	        this, &PSFGridWidget::invalidateGrid);
	connect(this->autoUpdatePatchCheckBox, &QCheckBox::toggled,
	        this, [this](bool checked) {
		        if (checked) {
			        this->handleVisibilityChanged(this->isVisible());
		        }
	        });
	connect(this->autoUpdateFrameCheckBox, &QCheckBox::toggled,
	        this, [this](bool checked) {
		        if (checked && this->isVisible() && this->gridOutdated) {
			        this->handleVisibilityChanged(true);
		        }
	        });
}

void PSFGridWidget::onUpdatePatchClicked()
{
	this->manualPatchUpdate = true;
	emit this->updatePatchRequested();
	this->manualPatchUpdate = false;
}

void PSFGridWidget::onUpdateFrameClicked()
{
	if (this->generationInProgress) {
		this->frameUpdatePending = false;
		this->updateFrameButton->setEnabled(false);
		this->setStatus(tr("Cancelling..."));
		emit this->cancelGenerationRequested();
		return;
	}
	this->frameUpdatePending = false;
	emit generateRequested(this->currentFrame, this->cropSizeSpinBox->value());
}

void PSFGridWidget::displayPSFGrid(const PSFGridResult& result)
{
	this->finishGeneration();
	const QString cancelledStatus = this->lastResult.mosaicImage.isNull()
		? tr("Cancelled.")
		: tr("Cancelled - previous grid retained");
	if (this->discardPendingResult) {
		this->discardPendingResult = false;
		this->setStatus(this->gridOutdated
			? tr("Out of date - Update frame")
			: cancelledStatus);
		this->startPendingFrameUpdate();
		return;
	}

	if (result.status == RunStatus::CANCELLED) {
		this->setStatus(cancelledStatus);
		this->startPendingFrameUpdate();
		return;
	}
	const bool failed = result.status == RunStatus::FAILED;
	if (failed && result.mosaicImage.isNull()) {
		this->setStatus(result.message);
		this->startPendingFrameUpdate();
		return;
	}

	this->lastResult = result;
	this->gridOutdated = failed;
	this->resetGridOnNextPatchUpdate = false;

	// Clear existing items
	this->graphicsScene->clear();
	this->mosaicItem = nullptr;
	this->highlightRect = nullptr;

	if (result.mosaicImage.isNull()) {
		this->setStatus(tr("No PSF grid available."));
		this->startPendingFrameUpdate();
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

	this->setStatus(failed ? result.message : tr("Up to date"));

	// Fit view to scene, preserving current orientation
	this->graphicsView->resetTransform();
	this->graphicsView->fitInView(this->graphicsScene->sceneRect(), Qt::KeepAspectRatio);
	if (!this->viewOrientation.isIdentity()) {
		this->graphicsView->setTransform(
			this->graphicsView->transform() * this->viewOrientation);
	}
	this->startPendingFrameUpdate();
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
	if (this->patchCols == cols && this->patchRows == rows) {
		return;
	}
	this->patchCols = cols;
	this->patchRows = rows;
	this->invalidateGrid();
}

void PSFGridWidget::setCurrentFrame(int frame)
{
	if (this->currentFrame == frame) {
		return;
	}
	this->currentFrame = frame;
	this->invalidateGrid();
}

void PSFGridWidget::invalidateGrid()
{
	this->gridOutdated = true;
	this->resetGridOnNextPatchUpdate = true;
	if (this->generationInProgress && !this->discardPendingResult) {
		this->discardPendingResult = true;
		this->updateFrameButton->setEnabled(false);
		this->setStatus(tr("Stopping outdated generation..."));
		emit this->cancelGenerationRequested();
	}
	if (!this->generationInProgress
		&& !this->lastResult.mosaicImage.isNull()) {
		this->setStatus(tr("Out of date - Update frame"));
	}
	if (this->autoUpdateFrameCheckBox->isChecked()) {
		this->frameUpdatePending = true;
		QTimer::singleShot(
			0, this, &PSFGridWidget::startPendingFrameUpdate);
	}
}

void PSFGridWidget::generationStarted(int totalPatches)
{
	this->generationInProgress = true;
	this->discardPendingResult = false;
	this->updatePatchButton->setEnabled(false);
	this->updateFrameButton->setText(tr("Cancel"));
	this->updateFrameButton->setEnabled(true);
	this->cropSizeSpinBox->setEnabled(false);
	this->progressBar->setRange(0, qMax(0, totalPatches));
	this->progressBar->setValue(0);
	this->progressBar->setFormat(tr("%v / %m patches"));
	this->progressBar->setToolTip(tr("Generating PSF grid..."));
}

void PSFGridWidget::generationProgressUpdated(
	int completedPatches, int totalPatches)
{
	if (!this->generationInProgress) {
		return;
	}
	this->progressBar->setRange(0, qMax(0, totalPatches));
	this->progressBar->setValue(qBound(
		0, completedPatches, totalPatches));
}

void PSFGridWidget::handleVisibilityChanged(bool visible)
{
	if (!visible) {
		this->frameUpdatePending = false;
		if (this->generationInProgress) {
			this->discardPendingResult = true;
			emit this->cancelGenerationRequested();
		}
		return;
	}

	if (this->autoUpdateFrameCheckBox->isChecked()
		&& this->gridOutdated) {
		this->frameUpdatePending = true;
		QTimer::singleShot(
			0, this, &PSFGridWidget::startPendingFrameUpdate);
	} else if (this->autoUpdatePatchCheckBox->isChecked()) {
		emit this->updatePatchRequested();
	}
}

void PSFGridWidget::finishGeneration()
{
	this->generationInProgress = false;
	this->updatePatchButton->setEnabled(true);
	this->updateFrameButton->setText(tr("Update frame"));
	this->updateFrameButton->setEnabled(true);
	this->cropSizeSpinBox->setEnabled(true);
	this->progressBar->setRange(0, 1);
	this->progressBar->setValue(0);
}

void PSFGridWidget::startPendingFrameUpdate()
{
	if (!this->frameUpdatePending || this->generationInProgress) {
		return;
	}
	if (!this->isVisible()
		|| !this->autoUpdateFrameCheckBox->isChecked()) {
		this->frameUpdatePending = false;
		return;
	}
	this->frameUpdatePending = false;
	emit this->generateRequested(
		this->currentFrame, this->cropSizeSpinBox->value());
}

void PSFGridWidget::setStatus(const QString& text)
{
	this->progressBar->setFormat(text);
	this->progressBar->setToolTip(text);
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
	if (!this->autoUpdatePatchCheckBox->isChecked()
		&& !this->manualPatchUpdate) {
		return;
	}
	if (!this->isVisible()) {
		return;
	}
	const bool geometryChanged =
		this->lastResult.cols != this->patchCols
		|| this->lastResult.rows != this->patchRows
		|| this->lastResult.cellSize != this->cropSizeSpinBox->value();
	if (this->lastResult.mosaicImage.isNull()
		|| this->mosaicItem == nullptr || geometryChanged
		|| this->resetGridOnNextPatchUpdate) {
		if (this->generationInProgress
			|| this->patchCols <= 0 || this->patchRows <= 0) {
			return;
		}

		PSFGridResult emptyGrid = PSFGridGenerator::createEmptyGrid(
			this->patchCols, this->patchRows,
			this->cropSizeSpinBox->value());
		this->displayPSFGrid(emptyGrid);
		this->gridOutdated = true;
		this->setStatus(tr("Selected patch only"));
	}

	if (!PSFGridGenerator::updatePatch(
		this->lastResult, psf, this->currentFrame,
		patchX, patchY)) {
		return;
	}

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

	bool hasMosaic = !this->gridOutdated
		&& !this->lastResult.mosaicImage.isNull();
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
	if (this->gridOutdated || this->lastResult.mosaicImage.isNull()) {
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
