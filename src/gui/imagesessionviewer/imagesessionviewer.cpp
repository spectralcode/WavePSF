#include "imagesessionviewer.h"
#include "imagedataviewer.h"
#include "datacrosssectionwidget.h"
#include "displaycontrolbar.h"
#include "controller/imagesession.h"
#include "utils/logging.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QGroupBox>
#include <QSplitter>
#include <QTimer>
#include "gui/verticalscrollarea.h"

namespace {
	const QString SETTINGS_GROUP            = QStringLiteral("image_session_viewer");
	const QString KEY_AUTO_RANGE_MODE       = QStringLiteral("auto_range_mode");
	const QString KEY_DISPLAY_RANGE_MIN     = QStringLiteral("display_range_min");
	const QString KEY_DISPLAY_RANGE_MAX     = QStringLiteral("display_range_max");
	const QString KEY_LOG_SCALE             = QStringLiteral("log_scale");
	const QString KEY_LUT_NAME              = QStringLiteral("lut_name");
	const QString KEY_PATCH_GRID_COLS       = QStringLiteral("patch_grid_cols");
	const QString KEY_PATCH_GRID_ROWS       = QStringLiteral("patch_grid_rows");
	const QString KEY_PATCH_BORDER_EXT      = QStringLiteral("patch_border_extension");
	const QString KEY_RIGHT_SPLITTER_STATE  = QStringLiteral("right_splitter_state");
	const QString KEY_MAIN_SPLITTER_STATE   = QStringLiteral("main_splitter_state");
	const QString KEY_SYNC_VIEWS            = QStringLiteral("sync_views");
	const QString KEY_HISTOGRAM_MODE       = QStringLiteral("histogram_mode");

	const int    DEF_AUTO_RANGE_MODE       = static_cast<int>(AutoRangeMode::PerFrame);
	const double DEF_DISPLAY_RANGE_MIN     = 0.0;
	const double DEF_DISPLAY_RANGE_MAX     = 255.0;
	const bool   DEF_LOG_SCALE             = false;
	const QString DEF_LUT_NAME             = QStringLiteral("Grayscale");
	const bool   DEF_SYNC_VIEWS            = false;
	const int    DEF_HISTOGRAM_MODE        = static_cast<int>(HistogramMode::Off);
	const int    DEF_PATCH_GRID_COLS       = 6;
	const int    DEF_PATCH_GRID_ROWS       = 8;
	const int    DEF_PATCH_BORDER_EXT      = 10;
}

ImageSessionViewer::ImageSessionViewer(QWidget* parent)
	: QWidget(parent), imageSession(nullptr),
	  mainSplitter(nullptr), controlsWidget(nullptr), sidebarLayout(nullptr), rightSplitter(nullptr), viewersWidget(nullptr),
	  frameControlsGroup(nullptr), frameSlider(nullptr), frameSpinBox(nullptr),
	  patchSlider(nullptr), patchSpinBox(nullptr),
	  inputViewer(nullptr), outputViewer(nullptr), activeViewer(nullptr), crossSectionWidget(nullptr), displayControlBar(nullptr), updatingControls(false),
	  viewSyncEnabled(false), crossSectionVisible(false), patchGridVisible(true),
	  connectedInputData(nullptr), connectedOutputData(nullptr)
{
	this->setupUI();
	this->connectSignals();
	//this->updateFrameControls();
}

ImageSessionViewer::~ImageSessionViewer()
{
	// Qt parent-child system handles cleanup
}

void ImageSessionViewer::addSidebarWidget(QWidget* widget)
{
	this->sidebarLayout->addWidget(widget);
}

void ImageSessionViewer::addBottomPanel(QWidget* widget)
{
	this->rightSplitter->addWidget(widget);
	this->rightSplitter->setStretchFactor(0, 3); // viewers
	if (this->crossSectionWidget != nullptr) {
		this->rightSplitter->setStretchFactor(1, 1); // cross-section
		this->rightSplitter->setStretchFactor(2, 1); // bottom panel
	} else {
		this->rightSplitter->setStretchFactor(1, 1); // bottom panel
	}
}

QString ImageSessionViewer::getName() const
{
	return SETTINGS_GROUP;
}

QVariantMap ImageSessionViewer::getSettings() const
{
	QVariantMap settingsMap;
	settingsMap.insert(KEY_AUTO_RANGE_MODE,    static_cast<int>(this->displaySettings.autoRangeMode));
	settingsMap.insert(KEY_DISPLAY_RANGE_MIN,  this->displaySettings.rangeMin);
	settingsMap.insert(KEY_DISPLAY_RANGE_MAX,  this->displaySettings.rangeMax);
	settingsMap.insert(KEY_LOG_SCALE,          this->displaySettings.logScale);
	settingsMap.insert(KEY_LUT_NAME,           this->displaySettings.lutName);
	settingsMap.insert(KEY_SYNC_VIEWS,         this->viewSyncEnabled);
	if (this->displayControlBar) {
		settingsMap.insert(KEY_HISTOGRAM_MODE, static_cast<int>(this->displayControlBar->getHistogramMode()));
	}
	settingsMap.insert(KEY_PATCH_GRID_COLS,    this->imageSession->getPatchGridCols());
	settingsMap.insert(KEY_PATCH_GRID_ROWS,    this->imageSession->getPatchGridRows());
	settingsMap.insert(KEY_PATCH_BORDER_EXT,   this->imageSession->getPatchBorderExtension());
	if (this->rightSplitter) {
		settingsMap.insert(KEY_RIGHT_SPLITTER_STATE, this->rightSplitter->saveState());
	}
	if (this->mainSplitter) {
		settingsMap.insert(KEY_MAIN_SPLITTER_STATE, this->mainSplitter->saveState());
	}

	return settingsMap;
}


void ImageSessionViewer::setSettings(const QVariantMap& settingsMap)
{
	DisplaySettings ds;
	ds.autoRangeMode = static_cast<AutoRangeMode>(settingsMap.value(KEY_AUTO_RANGE_MODE, DEF_AUTO_RANGE_MODE).toInt());
	ds.rangeMin  = settingsMap.value(KEY_DISPLAY_RANGE_MIN, DEF_DISPLAY_RANGE_MIN).toDouble();
	ds.rangeMax  = settingsMap.value(KEY_DISPLAY_RANGE_MAX, DEF_DISPLAY_RANGE_MAX).toDouble();
	ds.logScale  = settingsMap.value(KEY_LOG_SCALE,         DEF_LOG_SCALE).toBool();
	ds.lutName   = settingsMap.value(KEY_LUT_NAME,          DEF_LUT_NAME).toString();
	const bool   syncViews  = settingsMap.value(KEY_SYNC_VIEWS,         DEF_SYNC_VIEWS).toBool();
	const int    cols       = settingsMap.value(KEY_PATCH_GRID_COLS,    DEF_PATCH_GRID_COLS).toInt();
	const int    rows       = settingsMap.value(KEY_PATCH_GRID_ROWS,    DEF_PATCH_GRID_ROWS).toInt();
	const int    border     = settingsMap.value(KEY_PATCH_BORDER_EXT,   DEF_PATCH_BORDER_EXT).toInt();

	this->setDisplaySettings(ds);
	this->setViewSyncEnabled(syncViews);
	if (this->displayControlBar) {
		this->displayControlBar->setHistogramMode(
			static_cast<HistogramMode>(settingsMap.value(KEY_HISTOGRAM_MODE, DEF_HISTOGRAM_MODE).toInt()));
	}
	this->configurePatchGrid(cols, rows, border);
	emit patchGridConfigurationRequested(cols, rows, border);

	if (settingsMap.contains(KEY_RIGHT_SPLITTER_STATE) && this->rightSplitter) {
		this->rightSplitter->restoreState(settingsMap.value(KEY_RIGHT_SPLITTER_STATE).toByteArray());
	}
	if (settingsMap.contains(KEY_MAIN_SPLITTER_STATE) && this->mainSplitter) {
		this->mainSplitter->restoreState(settingsMap.value(KEY_MAIN_SPLITTER_STATE).toByteArray());
	}
}

void ImageSessionViewer::updateImageSession(ImageSession* imageSession)
{
	this->imageSession = imageSession;
	this->updateDataInViewers();
	this->updateFrameControls();
	this->syncViewersToSession();
}

void ImageSessionViewer::refreshOutputViewer()
{
	if (this->outputViewer != nullptr && this->imageSession != nullptr) {
		this->outputViewer->setCurrentFrame(this->imageSession->getCurrentFrame());
	}
}

void ImageSessionViewer::setCurrentFrame(int frame)
{
	if (!this->updatingControls) {
		this->updatingControls = true;

		// Update frame controls
		this->frameSlider->setValue(frame);
		this->frameSpinBox->setValue(frame);

		// Update both viewers to the same frame
		if (this->inputViewer != nullptr) {
			this->inputViewer->setCurrentFrame(frame);
		}
		if (this->outputViewer != nullptr) {
			this->outputViewer->setCurrentFrame(frame);
		}

		this->updatingControls = false;
	}
	this->updateFrameControls();
}

void ImageSessionViewer::highlightPatch(int x, int y)
{
	// Convert x,y coordinates to linear patch ID for the viewers
	int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEF_PATCH_GRID_COLS;
	int rows = this->imageSession != nullptr ? this->imageSession->getPatchGridRows() : DEF_PATCH_GRID_ROWS;
	int patchId = y * cols + x;
	int totalPatches = cols * rows;

	if (this->inputViewer != nullptr) {
		this->inputViewer->highlightPatch(patchId);
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->highlightPatch(patchId);
	}

	// Update patch navigation controls
	this->updatingControls = true;
	this->patchSlider->setMaximum(totalPatches - 1);
	this->patchSpinBox->setMaximum(totalPatches - 1);
	this->patchSlider->setValue(patchId);
	this->patchSpinBox->setValue(patchId);
	this->patchSlider->setEnabled(true);
	this->patchSpinBox->setEnabled(true);
	this->updatingControls = false;
}

void ImageSessionViewer::highlightPatches(const QVector<int>& patchLinearIds)
{
	if (this->inputViewer != nullptr) {
		this->inputViewer->highlightPatches(patchLinearIds);
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->highlightPatches(patchLinearIds);
	}
}

void ImageSessionViewer::configurePatchGrid(int cols, int rows, int borderExtension)
{
	Q_UNUSED(borderExtension);

	if (!this->updatingControls) {
		this->updatingControls = true;

		// Update patch navigation range
		int totalPatches = cols * rows;
		this->patchSlider->setMaximum(totalPatches - 1);
		this->patchSpinBox->setMaximum(totalPatches - 1);
		this->patchSlider->setValue(0);
		this->patchSpinBox->setValue(0);
		this->patchSlider->setEnabled(totalPatches > 0);
		this->patchSpinBox->setEnabled(totalPatches > 0);

		this->updatingControls = false;
	}

	// Update viewers
	if (this->inputViewer != nullptr) {
		this->inputViewer->configurePatchGrid(cols, rows);
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->configurePatchGrid(cols, rows);
	}
}

void ImageSessionViewer::setFrameFromSlider(int frame)
{
	if (!this->updatingControls) {
		emit frameChangeRequested(frame);
	}
}

void ImageSessionViewer::setFrameFromSpinBox(int frame)
{
	if (!this->updatingControls) {
		emit frameChangeRequested(frame);
	}
}

void ImageSessionViewer::setPatchFromSlider(int patchId)
{
	if (!this->updatingControls) {
		int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEF_PATCH_GRID_COLS;
		int x = patchId % cols;
		int y = patchId / cols;
		emit patchChangeRequested(x, y);
	}
}

void ImageSessionViewer::setPatchFromSpinBox(int patchId)
{
	if (!this->updatingControls) {
		int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEF_PATCH_GRID_COLS;
		int x = patchId % cols;
		int y = patchId / cols;
		emit patchChangeRequested(x, y);
	}
}

void ImageSessionViewer::setDisplaySettings(const DisplaySettings& settings)
{
	this->displaySettings = settings;
	if (this->inputViewer != nullptr) {
		this->inputViewer->setDisplaySettings(settings);
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->setDisplaySettings(settings);
	}
	if (this->crossSectionWidget != nullptr) {
		this->crossSectionWidget->setDisplaySettings(settings);
	}
	// Sync control bar UI (block re-emission to avoid feedback loop)
	if (this->displayControlBar != nullptr) {
		this->displayControlBar->blockSignals(true);
		this->displayControlBar->setSettings(settings);
		this->displayControlBar->blockSignals(false);
	}
}

DisplaySettings ImageSessionViewer::getDisplaySettings() const
{
	return this->displaySettings;
}

void ImageSessionViewer::setViewSyncEnabled(bool enabled)
{
	this->viewSyncEnabled = enabled;
	this->inputViewer->setViewSyncActive(enabled);
	this->outputViewer->setViewSyncActive(enabled);
	disconnect(this->viewSyncConn1);
	disconnect(this->viewSyncConn2);
	if (enabled) {
		this->viewSyncConn1 = connect(
			this->inputViewer,  &ImageDataViewer::viewTransformChanged,
			this->outputViewer, &ImageDataViewer::applyViewTransform);
		this->viewSyncConn2 = connect(
			this->outputViewer, &ImageDataViewer::viewTransformChanged,
			this->inputViewer,  &ImageDataViewer::applyViewTransform);
		// Bring output view in line with input view immediately
		this->inputViewer->broadcastViewTransform();
	}
}


ImageDataViewer* ImageSessionViewer::otherViewer() const
{
	return (this->activeViewer == this->inputViewer) ? this->outputViewer : this->inputViewer;
}

void ImageSessionViewer::rotateViewers90()
{
	this->activeViewer->rotate90();
}

void ImageSessionViewer::flipViewersH()
{
	this->activeViewer->flipH();
}

void ImageSessionViewer::flipViewersV()
{
	this->activeViewer->flipV();
}

void ImageSessionViewer::setPatchGridVisible(bool visible)
{
	this->patchGridVisible = visible;
	this->activeViewer->setPatchGridVisible(visible);
	if (this->viewSyncEnabled)
		this->otherViewer()->setPatchGridVisible(visible);
}

void ImageSessionViewer::setAxisOverlayVisible(bool visible)
{
	this->inputViewer->setAxisOverlayVisible(visible);
	this->outputViewer->setAxisOverlayVisible(visible);
}

void ImageSessionViewer::setCrossSectionVisible(bool visible)
{
	if (!this->crossSectionWidget) return;
	if (this->crossSectionVisible == visible) return;
	this->crossSectionVisible = visible;
	this->crossSectionWidget->setVisible(visible);
	this->inputViewer->setYPositionLineVisible(visible);
	this->outputViewer->setYPositionLineVisible(visible);
	// keep splitter proportions reasonable when showing/hiding
	if (visible) {
		this->rightSplitter->setStretchFactor(0, 3);
		this->rightSplitter->setStretchFactor(1, 1);
		this->crossSectionWidget->refreshPanels();
		int y = this->crossSectionWidget->currentYPosition();
		this->inputViewer->setYPositionLineY(y);
		this->outputViewer->setYPositionLineY(y);
	}
	emit crossSectionVisibilityChanged(visible);
}


void ImageSessionViewer::handleInputPatchSelected(int patchId)
{
	// Convert linear patch ID back to x,y coordinates
	int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEF_PATCH_GRID_COLS;
	int x = patchId % cols;
	int y = patchId / cols;

	emit patchChangeRequested(x, y);
}

void ImageSessionViewer::handleOutputPatchSelected(int patchId)
{
	// Convert linear patch ID back to x,y coordinates
	int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEF_PATCH_GRID_COLS;
	int x = patchId % cols;
	int y = patchId / cols;

	emit patchChangeRequested(x, y);
}

void ImageSessionViewer::handleInputFileDropped(const QString &filePath)
{
	emit inputFileDropRequested(filePath);
}

void ImageSessionViewer::handleFrameChangedFromViewer(int frame)
{
	if (!this->updatingControls) {
		emit frameChangeRequested(frame);
	}
}

void ImageSessionViewer::setupUI()
{
	// Main layout
	QHBoxLayout* mainLayout = new QHBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	// Create main splitter
	this->mainSplitter = new QSplitter(Qt::Horizontal, this);

	this->setupFrameControls();
	this->setupImageViewers();

	// Create controls widget in scroll area
	VerticalScrollArea* controlsScrollArea = new VerticalScrollArea();

	this->controlsWidget = new QWidget(controlsScrollArea);

	this->sidebarLayout = new QVBoxLayout(this->controlsWidget);
	this->sidebarLayout->setContentsMargins(3, 3, 3, 0);
	//this->sidebarLayout->setContentsMargins(0, 0, 0, 0);
	this->sidebarLayout->addWidget(this->frameControlsGroup);

	controlsScrollArea->setWidget(this->controlsWidget);

	// Wrap viewers and cross-section in vertical splitter (bottom panel added later via addBottomPanel)
	this->rightSplitter = new QSplitter(Qt::Vertical);
	this->rightSplitter->addWidget(this->viewersWidget);
	if (this->crossSectionWidget != nullptr) {
		this->rightSplitter->addWidget(this->crossSectionWidget);
		this->crossSectionWidget->setVisible(false);
		this->rightSplitter->setStretchFactor(0, 3);
		this->rightSplitter->setStretchFactor(1, 1);
	}

	// Add to main horizontal splitter
	this->mainSplitter->addWidget(controlsScrollArea);
	this->mainSplitter->addWidget(this->rightSplitter);
	this->mainSplitter->setStretchFactor(0, 0); // Sidebar: don't grow on resize
	this->mainSplitter->setStretchFactor(1, 1); // Viewers + bottom panel: take all extra space

	mainLayout->addWidget(this->mainSplitter);
	this->setLayout(mainLayout);
}

void ImageSessionViewer::setupFrameControls()
{
	this->frameControlsGroup = new QGroupBox("Frame Navigation");

	// Frame slider and spinbox
	this->frameSlider = new QSlider(Qt::Horizontal);
	this->frameSlider->setMinimum(0);
	this->frameSlider->setMaximum(0);
	this->frameSlider->setValue(0);
	this->frameSlider->setEnabled(false);

	this->frameSpinBox = new QSpinBox();
	this->frameSpinBox->setMinimum(0);
	this->frameSpinBox->setMaximum(0);
	this->frameSpinBox->setValue(0);
	this->frameSpinBox->setEnabled(false);

	// Patch slider and spinbox
	this->patchSlider = new QSlider(Qt::Horizontal);
	this->patchSlider->setMinimum(0);
	this->patchSlider->setMaximum(0);
	this->patchSlider->setValue(0);
	this->patchSlider->setEnabled(false);

	this->patchSpinBox = new QSpinBox();
	this->patchSpinBox->setMinimum(0);
	this->patchSpinBox->setMaximum(0);
	this->patchSpinBox->setValue(0);
	this->patchSpinBox->setEnabled(false);

	// Layout: label | slider | spinbox per row
	QVBoxLayout* frameLayout = new QVBoxLayout();
	frameLayout->setContentsMargins(3, 3, 3, 3);

	QHBoxLayout* frameRow = new QHBoxLayout();
	frameRow->addWidget(new QLabel(tr("Frame:")));
	frameRow->addWidget(this->frameSlider, 1);
	frameRow->addWidget(this->frameSpinBox, 0);
	frameLayout->addLayout(frameRow);

	QHBoxLayout* patchRow = new QHBoxLayout();
	patchRow->addWidget(new QLabel(tr("Patch:")));
	patchRow->addWidget(this->patchSlider, 1);
	patchRow->addWidget(this->patchSpinBox, 0);
	frameLayout->addLayout(patchRow);

	this->frameControlsGroup->setLayout(frameLayout);

	// Synchronize frame slider and spinbox
	connect(this->frameSlider, &QSlider::valueChanged, this->frameSpinBox, &QSpinBox::setValue);
	connect(this->frameSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this->frameSlider, &QSlider::setValue);

	// Synchronize patch slider and spinbox
	connect(this->patchSlider, &QSlider::valueChanged, this->patchSpinBox, &QSpinBox::setValue);
	connect(this->patchSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this->patchSlider, &QSlider::setValue);
}


void ImageSessionViewer::setupImageViewers()
{
	// Create viewers widget (container for control bar + image viewers)
	this->viewersWidget = new QWidget();
	QVBoxLayout* viewersOuterLayout = new QVBoxLayout(this->viewersWidget);
	viewersOuterLayout->setContentsMargins(0, 0, 0, 0);
	viewersOuterLayout->setSpacing(0);

	// Display control bar (LUT, range slider, auto-range, log)
	this->displayControlBar = new DisplayControlBar(this->viewersWidget);
	viewersOuterLayout->addWidget(this->displayControlBar);

	// Create input and output viewers
	QWidget* viewersPair = new QWidget(this->viewersWidget);
	this->inputViewer  = new ImageDataViewer("Input",  viewersPair);
	this->outputViewer = new ImageDataViewer("Output", viewersPair);
	this->activeViewer = this->inputViewer;
	this->crossSectionWidget = new DataCrossSectionWidget(this);

	// Enable drag and drop on input viewer only
	this->inputViewer->enableInputDataDrops(true);

	QHBoxLayout* viewersLayout = new QHBoxLayout(viewersPair);
	viewersLayout->setContentsMargins(0, 0, 0, 0);
	viewersLayout->addWidget(this->inputViewer, 1);
	viewersLayout->addWidget(this->outputViewer, 1);

	viewersOuterLayout->addWidget(viewersPair, 1);
}

void ImageSessionViewer::connectSignals()
{
	// Connect frame controls to internal slots
	connect(this->frameSlider, &QSlider::valueChanged, this, &ImageSessionViewer::setFrameFromSlider);
	connect(this->frameSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImageSessionViewer::setFrameFromSpinBox);

	// Connect patch navigation controls
	connect(this->patchSlider, &QSlider::valueChanged, this, &ImageSessionViewer::setPatchFromSlider);
	connect(this->patchSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImageSessionViewer::setPatchFromSpinBox);

	// Connect patch selection signals
	connect(this->inputViewer, &ImageDataViewer::patchSelectionChanged, this, &ImageSessionViewer::handleInputPatchSelected);
	connect(this->outputViewer, &ImageDataViewer::patchSelectionChanged, this, &ImageSessionViewer::handleOutputPatchSelected);

	// Connect frame synchronization
	connect(this->inputViewer, &ImageDataViewer::currentFrameChanged, this, &ImageSessionViewer::handleFrameChangedFromViewer);

	// Connect file drop
	connect(this->inputViewer, &ImageDataViewer::inputFileDropRequested, this, &ImageSessionViewer::handleInputFileDropped);

	// Forward coefficient operations from both viewers
	connect(this->inputViewer, &ImageDataViewer::copyPressed, this, &ImageSessionViewer::copyCoefficientsRequested);
	connect(this->inputViewer, &ImageDataViewer::pastePressed, this, &ImageSessionViewer::pasteCoefficientsRequested);
	connect(this->inputViewer, &ImageDataViewer::deletePressed, this, &ImageSessionViewer::resetCoefficientsRequested);
	connect(this->outputViewer, &ImageDataViewer::copyPressed, this, &ImageSessionViewer::copyCoefficientsRequested);
	connect(this->outputViewer, &ImageDataViewer::pastePressed, this, &ImageSessionViewer::pasteCoefficientsRequested);
	connect(this->outputViewer, &ImageDataViewer::deletePressed, this, &ImageSessionViewer::resetCoefficientsRequested);

	// Forward patch navigation from both viewers
	connect(this->inputViewer,  &ImageDataViewer::navigatePatch, this, &ImageSessionViewer::navigatePatch);
	connect(this->outputViewer, &ImageDataViewer::navigatePatch, this, &ImageSessionViewer::navigatePatch);

	// Track active viewer (last clicked)
	connect(this->inputViewer,  &ImageDataViewer::viewActivated, this, [this]() { this->activeViewer = this->inputViewer; });
	connect(this->outputViewer, &ImageDataViewer::viewActivated, this, [this]() { this->activeViewer = this->outputViewer; });

	// Forward view transform changes (for PSF grid orientation sync)
	connect(this->inputViewer, &ImageDataViewer::viewTransformChanged,
	        this, &ImageSessionViewer::viewerTransformChanged);
	connect(this->outputViewer, &ImageDataViewer::viewTransformChanged,
	        this, &ImageSessionViewer::viewerTransformChanged);

	// Y position sync: cross-section ↔ viewers
	connect(this->crossSectionWidget, &DataCrossSectionWidget::yPositionChanged,
	        this->inputViewer, &ImageDataViewer::setYPositionLineY);
	connect(this->crossSectionWidget, &DataCrossSectionWidget::yPositionChanged,
	        this->outputViewer, &ImageDataViewer::setYPositionLineY);
	connect(this->inputViewer, &ImageDataViewer::yPositionLineDragged,
	        this->crossSectionWidget, &DataCrossSectionWidget::setYPosition);
	connect(this->outputViewer, &ImageDataViewer::yPositionLineDragged,
	        this->crossSectionWidget, &DataCrossSectionWidget::setYPosition);

	// Y position line toggle: sync other viewer
	connect(this->inputViewer, &ImageDataViewer::yPositionLineToggled,
	        this->outputViewer, &ImageDataViewer::setYPositionLineVisible);
	connect(this->outputViewer, &ImageDataViewer::yPositionLineToggled,
	        this->inputViewer, &ImageDataViewer::setYPositionLineVisible);

	// Display control bar → display settings
	connect(this->displayControlBar, &DisplayControlBar::settingsChanged,
	        this, &ImageSessionViewer::setDisplaySettings);

	// Forward per-frame data range from viewers → display control bar (for "Fit to Frame" context menu)
	connect(this->inputViewer, &ImageDataViewer::dataRangeComputed,
	        this->displayControlBar, &DisplayControlBar::setInputFrameRange);
	connect(this->outputViewer, &ImageDataViewer::dataRangeComputed,
	        this->displayControlBar, &DisplayControlBar::setOutputFrameRange);

	// Forward per-frame histograms from viewers → display control bar
	connect(this->inputViewer, &ImageDataViewer::histogramComputed,
	        this->displayControlBar, &DisplayControlBar::setInputHistogram);
	connect(this->outputViewer, &ImageDataViewer::histogramComputed,
	        this->displayControlBar, &DisplayControlBar::setOutputHistogram);

	// Histogram mode changed → enable/disable histogram computation in viewers
	connect(this->displayControlBar, &DisplayControlBar::histogramModeChanged,
	        this, [this](HistogramMode mode) {
		this->inputViewer->setComputeHistogram(mode == HistogramMode::InputFrame);
		this->outputViewer->setComputeHistogram(mode == HistogramMode::OutputFrame);
	});

	// "Fit to Input/Output Stack" → apply global data range
	connect(this->displayControlBar, &DisplayControlBar::resetToInputStackRequested, this, [this]() {
		if (this->connectedInputData) {
			auto range = this->connectedInputData->getGlobalRange();
			this->displayControlBar->setSliderRange(range.first, range.second);
		}
	});
	connect(this->displayControlBar, &DisplayControlBar::resetToOutputStackRequested, this, [this]() {
		if (this->connectedOutputData) {
			auto range = this->connectedOutputData->getGlobalRange();
			this->displayControlBar->setSliderRange(range.first, range.second);
		}
	});
}

void ImageSessionViewer::updateFrameControls()
{
	if (this->imageSession != nullptr && this->imageSession->hasInputData()) {
		int totalFrames = this->imageSession->getInputFrames();
		int currentFrame = this->imageSession->getCurrentFrame();

		this->updatingControls = true;

		// Update range
		this->frameSlider->setMaximum(totalFrames - 1);
		this->frameSpinBox->setMaximum(totalFrames - 1);

		// Update values
		this->frameSlider->setValue(currentFrame);
		this->frameSpinBox->setValue(currentFrame);

		// Enable controls
		this->frameSlider->setEnabled(true);
		this->frameSpinBox->setEnabled(true);

		this->updatingControls = false;
	} else {
		// No data - disable controls
		this->frameSlider->setMaximum(0);
		this->frameSpinBox->setMaximum(0);
		this->frameSlider->setValue(0);
		this->frameSpinBox->setValue(0);
		this->frameSlider->setEnabled(false);
		this->frameSpinBox->setEnabled(false);
	}
}

void ImageSessionViewer::syncViewersToSession()
{
	if (this->imageSession != nullptr) {
		// Sync current frame
		if (this->imageSession->hasInputData()) {
			int currentFrame = this->imageSession->getCurrentFrame();
			if (this->inputViewer != nullptr) {
				this->inputViewer->setCurrentFrame(currentFrame);
			}
			if (this->outputViewer != nullptr) {
				this->outputViewer->setCurrentFrame(currentFrame);
			}
			if (this->crossSectionWidget != nullptr) {
				this->crossSectionWidget->setCurrentFrame(currentFrame);
			}
		}

		// Sync patch grid configuration
		int cols = this->imageSession->getPatchGridCols();
		int rows = this->imageSession->getPatchGridRows();

		if (this->inputViewer != nullptr) {
			this->inputViewer->configurePatchGrid(cols, rows);
		}
		if (this->outputViewer != nullptr) {
			this->outputViewer->configurePatchGrid(cols, rows);
		}

		// Sync current patch highlight
		int patchX = this->imageSession->getCurrentPatch().x();
		int patchY = this->imageSession->getCurrentPatch().y();
		if (patchX >= 0 && patchY >= 0) {
			this->highlightPatch(patchX, patchY);
		}

		// Apply current display settings to all viewers
		this->setDisplaySettings(this->displaySettings);
	}
}

void ImageSessionViewer::updateDataInViewers()
{
	if (this->imageSession != nullptr) {
		// Suppress viewer frame-change feedback while reconnecting data sources.
		// connectImageData() calls displayFrame(0) which emits currentFrameChanged(0),
		// which would otherwise propagate as frameChangeRequested(0) and reset
		// imageSession->currentFrame to 0, discarding any preserved frame value.
		this->updatingControls = true;

		// Connect viewers to data (only reconnect if the data pointer changed,
		// to avoid resetting frame position when only ground truth was added)
		if (this->imageSession->hasInputData()) {
			if (this->connectedInputData != this->imageSession->getInputData()) {
				this->connectedInputData = this->imageSession->getInputData();
				this->inputViewer->connectImageData(this->connectedInputData);

				// Set slider range from full volume scan and configure integer/float formatting
				auto range = this->connectedInputData->getGlobalRange();
				this->displayControlBar->setSliderRange(range.first, range.second);
				EnviDataType dt = this->connectedInputData->getDataType();
				this->displayControlBar->setIntegerMode(dt != FLOAT_32BIT && dt != DOUBLE_64BIT);
			}
			this->inputViewer->setPatchGridVisible(this->patchGridVisible);
		} else {
			this->connectedInputData = nullptr;
			this->inputViewer->disconnectImageData();
			this->displayControlBar->clearInputHistogram();
		}

		if (this->imageSession->hasOutputData()) {
			if (this->connectedOutputData != this->imageSession->getOutputData()) {
				this->connectedOutputData = this->imageSession->getOutputData();
				this->outputViewer->connectImageData(this->connectedOutputData);
			}
			this->outputViewer->setPatchGridVisible(this->patchGridVisible);
		} else {
			this->connectedOutputData = nullptr;
			this->outputViewer->disconnectImageData();
			this->displayControlBar->clearOutputHistogram();
		}

		if (this->crossSectionWidget != nullptr) {
			if (this->imageSession->hasInputData()) {
				this->crossSectionWidget->setInputData(this->imageSession->getInputData());
			} else {
				this->crossSectionWidget->setInputData(nullptr);
			}
			if (this->imageSession->hasOutputData()) {
				this->crossSectionWidget->setOutputData(this->imageSession->getOutputData());
			} else {
				this->crossSectionWidget->setOutputData(nullptr);
			}
		}

		this->updatingControls = false;

		// Connect reference data to both viewers
		const ImageData* groundTruthData = nullptr;
		if (this->imageSession->hasGroundTruthData()) {
			groundTruthData = this->imageSession->getGroundTruthData();
		}

		if (this->inputViewer != nullptr) {
			this->inputViewer->connectReferenceImageData(groundTruthData);

			this->inputViewer->showReference(true);
			QTimer::singleShot(1000, this, [this](){
				this->inputViewer->showReference(false);
			});
		}

		if (this->outputViewer != nullptr) {
			this->outputViewer->connectReferenceImageData(groundTruthData);

			this->outputViewer->showReference(true);
			QTimer::singleShot(1000, this, [this](){
				this->outputViewer->showReference(false);
			});
		}

	}
}
