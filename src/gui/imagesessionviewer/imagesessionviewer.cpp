#include "imagesessionviewer.h"
#include "imagedataviewer.h"
#include "controller/imagesession.h"
#include "utils/logging.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QSplitter>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QTimer>

namespace {
	const char* SETTINGS_GROUP = "image_session_viewer";
	const char* AUTO_RANGE_ENABLED_KEY = "auto_range_enabled";
	const char* DISPLAY_RANGE_MIN_KEY = "display_range_min";
	const char* DISPLAY_RANGE_MAX_KEY = "display_range_max";
	const char* PATCH_GRID_COLS_KEY = "patchGridCols";
	const char* PATCH_GRID_ROWS_KEY = "patchGridRows";
	const char* PATCH_BORDER_EXTENSION_KEY = "patchBorderExtension";

	const bool	DEFAULT_AUTO_RANGE = true;
	const double DEFAULT_MIN = 0.0;
	const double DEFAULT_MAX = 255.0;
	const int DEFAULT_PATCH_COLS = 4;
	const int DEFAULT_PATCH_ROWS = 4;
	const int DEFAULT_BORDER_EXTENSION = 10;
	const int CONTROLS_MIN_WIDTH = 250;
	const int CONTROLS_MAX_WIDTH = 350;
}

ImageSessionViewer::ImageSessionViewer(QWidget* parent)
	: QWidget(parent), imageSession(nullptr),
	  displayRangeMin(0.0), displayRangeMax(255.0), autoRangeEnabled(true),
	  mainSplitter(nullptr), controlsWidget(nullptr), viewersWidget(nullptr),
	  frameControlsGroup(nullptr), frameInfoLabel(nullptr), frameSlider(nullptr), frameSpinBox(nullptr),
	  displayRangeGroup(nullptr), autoRangeCheckBox(nullptr), minValueSpinBox(nullptr), maxValueSpinBox(nullptr),
	  patchGridGroup(nullptr), patchGridInfoLabel(nullptr), patchColsSpinBox(nullptr),
	  patchRowsSpinBox(nullptr), borderExtensionSpinBox(nullptr),
	  inputViewer(nullptr), outputViewer(nullptr), updatingControls(false)
{
	this->setupUI();
	this->connectSignals();
	//this->updateFrameControls();
	//this->updatePatchGridControls();
}

ImageSessionViewer::~ImageSessionViewer()
{
	// Qt parent-child system handles cleanup
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
	this->autoRangeEnabled = autoRange;
	this->displayRangeMin = minV;
	this->displayRangeMax = maxV;
	this->autoRangeCheckBox->setChecked(this->autoRangeEnabled);
	this->minValueSpinBox->setValue(this->displayRangeMin);
	this->maxValueSpinBox->setValue(this->displayRangeMax);
	this->configurePatchGrid(cols, rows, border);
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
	int patchId = y * cols + x;

	if (this->inputViewer != nullptr) {
		this->inputViewer->highlightPatch(patchId);
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->highlightPatch(patchId);
	}
	this->updatePatchGridControls();
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
	if (!this->updatingControls) {
		this->updatingControls = true;

		// Update patch grid controls
		this->patchColsSpinBox->setValue(cols);
		this->patchRowsSpinBox->setValue(rows);
		this->borderExtensionSpinBox->setValue(borderExtension);

		this->updatingControls = false;
	}

	// Update viewers
	if (this->inputViewer != nullptr) {
		this->inputViewer->configurePatchGrid(cols, rows);
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->configurePatchGrid(cols, rows);
	}

	this->updatePatchGridControls();
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

void ImageSessionViewer::setAutoRange(bool enabled)
{
	this->autoRangeEnabled = enabled;

	// Apply to both viewers
	if (this->inputViewer != nullptr) {
		this->inputViewer->setAutoRange(enabled);
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->setAutoRange(enabled);
	}

	// Enable/disable manual controls
	this->minValueSpinBox->setEnabled(!enabled);
	this->maxValueSpinBox->setEnabled(!enabled);
}

void ImageSessionViewer::setMinValue(double value)
{
	this->displayRangeMin = value;

	// Apply to both viewers
	if (this->inputViewer != nullptr) {
		this->inputViewer->setDisplayRange(value, this->maxValueSpinBox->value());
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->setDisplayRange(value, this->maxValueSpinBox->value());
	}
}

void ImageSessionViewer::setMaxValue(double value)
{
	this->displayRangeMax = value;

	// Apply to both viewers
	if (this->inputViewer != nullptr) {
		this->inputViewer->setDisplayRange(this->minValueSpinBox->value(), value);
	}
	if (this->outputViewer != nullptr) {
		this->outputViewer->setDisplayRange(this->minValueSpinBox->value(), value);
	}
}

void ImageSessionViewer::applyPatchGridSettings()
{
	int cols = this->patchColsSpinBox->value();
	int rows = this->patchRowsSpinBox->value();
	int border = this->borderExtensionSpinBox->value();
	emit patchGridConfigurationRequested(cols, rows, border);
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
	this->setupDisplayRangeControls();
	this->setupPatchGridControls();
	this->setupImageViewers();

	// Create controls widget
	this->controlsWidget = new QWidget();
	this->controlsWidget->setMinimumWidth(CONTROLS_MIN_WIDTH);
	this->controlsWidget->setMaximumWidth(CONTROLS_MAX_WIDTH);

	QVBoxLayout* controlsLayout = new QVBoxLayout(this->controlsWidget);
	controlsLayout->addWidget(this->frameControlsGroup);
	controlsLayout->addWidget(this->displayRangeGroup);
	controlsLayout->addWidget(this->patchGridGroup);
	controlsLayout->addStretch();

	// Add to splitter
	this->mainSplitter->addWidget(this->controlsWidget);
	this->mainSplitter->addWidget(this->viewersWidget);
	this->mainSplitter->setStretchFactor(0, 0); // Controls don't stretch
	this->mainSplitter->setStretchFactor(1, 1); // Viewers stretch

	mainLayout->addWidget(this->mainSplitter);
	this->setLayout(mainLayout);
}

void ImageSessionViewer::setupFrameControls()
{
	this->frameControlsGroup = new QGroupBox("Frame Navigation");

	// Frame info label
	this->frameInfoLabel = new QLabel("No data loaded");

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

	// Layout
	QVBoxLayout* frameLayout = new QVBoxLayout();
	frameLayout->addWidget(this->frameInfoLabel);

	QHBoxLayout* sliderLayout = new QHBoxLayout();
	sliderLayout->addWidget(this->frameSlider, 1);
	sliderLayout->addWidget(this->frameSpinBox, 0);
	frameLayout->addLayout(sliderLayout);

	this->frameControlsGroup->setLayout(frameLayout);

	// Synchronize slider and spinbox
	connect(this->frameSlider, &QSlider::valueChanged, this->frameSpinBox, &QSpinBox::setValue);
	connect(this->frameSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this->frameSlider, &QSlider::setValue);
}

void ImageSessionViewer::setupDisplayRangeControls()
{
	this->displayRangeGroup = new QGroupBox(tr("Display Range"));

	// Auto range checkbox
	this->autoRangeCheckBox = new QCheckBox(tr("Auto Range"));
	this->autoRangeCheckBox->setChecked(this->autoRangeEnabled);

	// Min value control
	QLabel* minLabel = new QLabel(tr("Min:"));
	this->minValueSpinBox = new QDoubleSpinBox();
	this->minValueSpinBox->setRange(-99999.0, 99999.0);
	this->minValueSpinBox->setDecimals(2);
	this->minValueSpinBox->setValue(this->displayRangeMin);
	this->minValueSpinBox->setEnabled(!this->autoRangeEnabled);

	// Max value control
	QLabel* maxLabel = new QLabel(tr("Max:"));
	this->maxValueSpinBox = new QDoubleSpinBox();
	this->maxValueSpinBox->setRange(-99999.0, 99999.0);
	this->maxValueSpinBox->setDecimals(2);
	this->maxValueSpinBox->setValue(this->displayRangeMax);
	this->maxValueSpinBox->setEnabled(!this->autoRangeEnabled);

	// Layout
	QVBoxLayout* rangeLayout = new QVBoxLayout();
	rangeLayout->addWidget(this->autoRangeCheckBox);

	QGridLayout* valuesLayout = new QGridLayout();
	valuesLayout->addWidget(minLabel, 0, 0);
	valuesLayout->addWidget(this->minValueSpinBox, 0, 1);
	valuesLayout->addWidget(maxLabel, 1, 0);
	valuesLayout->addWidget(this->maxValueSpinBox, 1, 1);

	rangeLayout->addLayout(valuesLayout);
	this->displayRangeGroup->setLayout(rangeLayout);
}

void ImageSessionViewer::setupPatchGridControls()
{
	this->patchGridGroup = new QGroupBox("Patch Grid Configuration");

	// Info label
	this->patchGridInfoLabel = new QLabel("Current patch: (0, 0)");

	// Grid configuration controls
	QLabel* colsLabel = new QLabel("Columns:");
	this->patchColsSpinBox = new QSpinBox();
	this->patchColsSpinBox->setMinimum(1);
	this->patchColsSpinBox->setMaximum(32);
	this->patchColsSpinBox->setValue(DEFAULT_PATCH_COLS);

	QLabel* rowsLabel = new QLabel("Rows:");
	this->patchRowsSpinBox = new QSpinBox();
	this->patchRowsSpinBox->setMinimum(1);
	this->patchRowsSpinBox->setMaximum(32);
	this->patchRowsSpinBox->setValue(DEFAULT_PATCH_ROWS);

	QLabel* borderLabel = new QLabel("Border Extension:");
	this->borderExtensionSpinBox = new QSpinBox();
	this->borderExtensionSpinBox->setMinimum(0);
	this->borderExtensionSpinBox->setMaximum(100);
	this->borderExtensionSpinBox->setValue(DEFAULT_BORDER_EXTENSION);
	this->borderExtensionSpinBox->setSuffix(" px");

	// Layout
	QVBoxLayout* patchLayout = new QVBoxLayout();
	patchLayout->addWidget(this->patchGridInfoLabel);

	QGridLayout* gridControlsLayout = new QGridLayout();
	gridControlsLayout->addWidget(colsLabel, 0, 0);
	gridControlsLayout->addWidget(this->patchColsSpinBox, 0, 1);
	gridControlsLayout->addWidget(rowsLabel, 1, 0);
	gridControlsLayout->addWidget(this->patchRowsSpinBox, 1, 1);
	gridControlsLayout->addWidget(borderLabel, 2, 0);
	gridControlsLayout->addWidget(this->borderExtensionSpinBox, 2, 1);

	patchLayout->addLayout(gridControlsLayout);

	this->patchGridGroup->setLayout(patchLayout);
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

	// Connect display range controls
	connect(this->autoRangeCheckBox, &QCheckBox::toggled, this, &ImageSessionViewer::setAutoRange);
	connect(this->minValueSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImageSessionViewer::setMinValue);
	connect(this->maxValueSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImageSessionViewer::setMaxValue);

	// Connect patch grid controls for immediate updates
	connect(this->patchColsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImageSessionViewer::applyPatchGridSettings);
	connect(this->patchRowsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImageSessionViewer::applyPatchGridSettings);
	connect(this->borderExtensionSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImageSessionViewer::applyPatchGridSettings);

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

		// Update info
		this->frameInfoLabel->setText(QString("Frame %1 of %2").arg(currentFrame + 1).arg(totalFrames));

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
		this->frameInfoLabel->setText("No data loaded");
	}
}

void ImageSessionViewer::updatePatchGridControls()
{
	if (this->imageSession != nullptr) {
		int currentPatchX = this->imageSession->getCurrentPatch().x();
		int currentPatchY = this->imageSession->getCurrentPatch().y();

		if (currentPatchX >= 0 && currentPatchY >= 0) {
			this->patchGridInfoLabel->setText(QString("Current patch: (%1, %2)").arg(currentPatchX).arg(currentPatchY));
		} else {
			this->patchGridInfoLabel->setText("Current patch: (0, 0)");
		}
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
		bool autoRange = this->autoRangeCheckBox->isChecked();
		if (this->inputViewer != nullptr) {
			this->inputViewer->setAutoRange(autoRange);
			if (!autoRange) {
				this->inputViewer->setDisplayRange(this->minValueSpinBox->value(), this->maxValueSpinBox->value());
			}
		}
		if (this->outputViewer != nullptr) {
			this->outputViewer->setAutoRange(autoRange);
			if (!autoRange) {
				this->outputViewer->setDisplayRange(this->minValueSpinBox->value(), this->maxValueSpinBox->value());
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
