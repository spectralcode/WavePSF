#include "imagesessionviewer.h"
#include "imagedataviewer.h"
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
	const char* SETTINGS_GROUP = "image_session_viewer";
	const char* AUTO_RANGE_ENABLED_KEY = "auto_range_enabled";
	const char* DISPLAY_RANGE_MIN_KEY = "display_range_min";
	const char* DISPLAY_RANGE_MAX_KEY = "display_range_max";
	const char* PATCH_GRID_COLS_KEY = "patchGridCols";
	const char* PATCH_GRID_ROWS_KEY = "patchGridRows";
	const char* PATCH_BORDER_EXTENSION_KEY = "patchBorderExtension";
	const char* RIGHT_SPLITTER_STATE_KEY = "right_splitter_state";

	const bool	DEFAULT_AUTO_RANGE = true;
	const double DEFAULT_MIN = 0.0;
	const double DEFAULT_MAX = 255.0;
	const int DEFAULT_PATCH_COLS = 4;
	const int DEFAULT_PATCH_ROWS = 4;
	const int DEFAULT_BORDER_EXTENSION = 10;
}

ImageSessionViewer::ImageSessionViewer(QWidget* parent)
	: QWidget(parent), imageSession(nullptr),
	  displayRangeMin(0.0), displayRangeMax(255.0), autoRangeEnabled(true),
	  mainSplitter(nullptr), controlsWidget(nullptr), sidebarLayout(nullptr), rightSplitter(nullptr), viewersWidget(nullptr),
	  frameControlsGroup(nullptr), frameSlider(nullptr), frameSpinBox(nullptr),
	  patchSlider(nullptr), patchSpinBox(nullptr),
	  inputViewer(nullptr), outputViewer(nullptr), updatingControls(false)
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
	this->rightSplitter->setStretchFactor(1, 1); // bottom panel
}

QString ImageSessionViewer::getName() const
{
	return QLatin1String(SETTINGS_GROUP);
}

QVariantMap ImageSessionViewer::getSettings() const
{
	QVariantMap settingsMap;
	settingsMap.insert(AUTO_RANGE_ENABLED_KEY, this->autoRangeEnabled);
	settingsMap.insert(DISPLAY_RANGE_MIN_KEY, this->displayRangeMin);
	settingsMap.insert(DISPLAY_RANGE_MAX_KEY, this->displayRangeMax);
	settingsMap.insert(PATCH_GRID_COLS_KEY, this->imageSession->getPatchGridCols());
	settingsMap.insert(PATCH_GRID_ROWS_KEY, this->imageSession->getPatchGridRows());
	settingsMap.insert(PATCH_BORDER_EXTENSION_KEY, this->imageSession->getPatchBorderExtension());
	if (this->rightSplitter) {
		settingsMap.insert(RIGHT_SPLITTER_STATE_KEY, this->rightSplitter->saveState());
	}

	return settingsMap;
}


void ImageSessionViewer::setSettings(const QVariantMap& settingsMap)
{
	//read settings, use default values if settingsMap empty
	const bool autoRange = settingsMap.value(AUTO_RANGE_ENABLED_KEY, DEFAULT_AUTO_RANGE).toBool();
	const double minV = settingsMap.value(DISPLAY_RANGE_MIN_KEY, DEFAULT_MIN).toDouble();
	const double maxV = settingsMap.value(DISPLAY_RANGE_MAX_KEY, DEFAULT_MAX).toDouble();
	const int cols = settingsMap.value(PATCH_GRID_COLS_KEY, DEFAULT_PATCH_COLS).toInt();
	const int rows = settingsMap.value(PATCH_GRID_ROWS_KEY, DEFAULT_PATCH_ROWS).toInt();
	const int border = settingsMap.value(PATCH_BORDER_EXTENSION_KEY, DEFAULT_BORDER_EXTENSION).toInt();

	//update internal state and apply settings
	this->setDisplaySettings(autoRange, minV, maxV);
	this->configurePatchGrid(cols, rows, border);

	if (settingsMap.contains(RIGHT_SPLITTER_STATE_KEY) && this->rightSplitter) {
		this->rightSplitter->restoreState(settingsMap.value(RIGHT_SPLITTER_STATE_KEY).toByteArray());
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
	int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEFAULT_PATCH_COLS;
	int rows = this->imageSession != nullptr ? this->imageSession->getPatchGridRows() : DEFAULT_PATCH_ROWS;
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
		int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEFAULT_PATCH_COLS;
		int x = patchId % cols;
		int y = patchId / cols;
		emit patchChangeRequested(x, y);
	}
}

void ImageSessionViewer::setPatchFromSpinBox(int patchId)
{
	if (!this->updatingControls) {
		int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEFAULT_PATCH_COLS;
		int x = patchId % cols;
		int y = patchId / cols;
		emit patchChangeRequested(x, y);
	}
}

void ImageSessionViewer::setDisplaySettings(bool autoRange, double min, double max)
{
	this->autoRangeEnabled = autoRange;
	this->displayRangeMin = min;
	this->displayRangeMax = max;

	if (this->inputViewer != nullptr) {
		this->inputViewer->setAutoRange(autoRange);
		if (!autoRange) {
			this->inputViewer->setDisplayRange(min, max);
		}
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->setAutoRange(autoRange);
		if (!autoRange) {
			this->outputViewer->setDisplayRange(min, max);
		}
	}
}

bool ImageSessionViewer::getAutoRangeEnabled() const
{
	return this->autoRangeEnabled;
}

double ImageSessionViewer::getDisplayRangeMin() const
{
	return this->displayRangeMin;
}

double ImageSessionViewer::getDisplayRangeMax() const
{
	return this->displayRangeMax;
}

void ImageSessionViewer::handleInputPatchSelected(int patchId)
{
	// Convert linear patch ID back to x,y coordinates
	int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEFAULT_PATCH_COLS;
	int x = patchId % cols;
	int y = patchId / cols;

	emit patchChangeRequested(x, y);
}

void ImageSessionViewer::handleOutputPatchSelected(int patchId)
{
	// Convert linear patch ID back to x,y coordinates
	int cols = this->imageSession != nullptr ? this->imageSession->getPatchGridCols() : DEFAULT_PATCH_COLS;
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

	// Wrap viewers in vertical splitter (bottom panel added later via addBottomPanel)
	this->rightSplitter = new QSplitter(Qt::Vertical);
	this->rightSplitter->addWidget(this->viewersWidget);

	// Add to main horizontal splitter
	this->mainSplitter->addWidget(controlsScrollArea);
	this->mainSplitter->addWidget(this->rightSplitter);
	this->mainSplitter->setStretchFactor(0, 1); // Sidebar
	this->mainSplitter->setStretchFactor(1, 2); // Viewers + bottom panel

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
	// Create viewers widget
	this->viewersWidget = new QWidget();

	// Create input and output viewers
	this->inputViewer = new ImageDataViewer("Input", this->viewersWidget);
	this->outputViewer = new ImageDataViewer("Output", this->viewersWidget);

	// Enable drag and drop on input viewer only
	this->inputViewer->enableInputDataDrops(true);

	// Create layout for viewers
	QHBoxLayout* viewersLayout = new QHBoxLayout(this->viewersWidget);
	viewersLayout->setContentsMargins(0, 0, 0, 0);
	viewersLayout->addWidget(this->inputViewer, 1);
	viewersLayout->addWidget(this->outputViewer, 1);
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

		// Apply current display range settings to both viewers
		if (this->inputViewer != nullptr) {
			this->inputViewer->setAutoRange(this->autoRangeEnabled);
			if (!this->autoRangeEnabled) {
				this->inputViewer->setDisplayRange(this->displayRangeMin, this->displayRangeMax);
			}
		}
		if (this->outputViewer != nullptr) {
			this->outputViewer->setAutoRange(this->autoRangeEnabled);
			if (!this->autoRangeEnabled) {
				this->outputViewer->setDisplayRange(this->displayRangeMin, this->displayRangeMax);
			}
		}
	}
}

void ImageSessionViewer::updateDataInViewers()
{
	if (this->imageSession != nullptr) {
		// Connect viewers to data (only reconnect if the data pointer changed,
		// to avoid resetting frame position when only ground truth was added)
		if (this->imageSession->hasInputData()) {
			if (this->inputViewer->getImageData() != this->imageSession->getInputData()) {
				this->inputViewer->connectImageData(this->imageSession->getInputData());
			}
			this->inputViewer->setPatchGridVisible(true);
		} else {
			this->inputViewer->disconnectImageData();
		}

		if (this->imageSession->hasOutputData()) {
			if (this->outputViewer->getImageData() != this->imageSession->getOutputData()) {
				this->outputViewer->connectImageData(this->imageSession->getOutputData());
			}
			this->outputViewer->setPatchGridVisible(true);
		} else {
			this->outputViewer->disconnectImageData();
		}

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

		LOG_INFO() << "Data connected to viewers";
	}
}
