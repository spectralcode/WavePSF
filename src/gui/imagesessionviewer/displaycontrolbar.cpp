#include "displaycontrolbar.h"
#include "rangeslider.h"
#include "gui/lut.h"

#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QEvent>

namespace {

// QComboBox subclass that shows icons only when the dropdown is open
class LutComboBox : public QComboBox
{
public:
	explicit LutComboBox(QWidget* parent = nullptr) : QComboBox(parent)
	{
		setIconSize(QSize(0, 0));
	}

	void showPopup() override
	{
		setIconSize(QSize(64, 12));
		QComboBox::showPopup();
	}

	void hidePopup() override
	{
		QComboBox::hidePopup();
		setIconSize(QSize(0, 0));
	}
};

} // namespace

DisplayControlBar::DisplayControlBar(QWidget* parent)
	: QWidget(parent)
	, blockEmit(false)
	, lastInputFrameMin(0.0)
	, lastInputFrameMax(255.0)
	, lastOutputFrameMin(0.0)
	, lastOutputFrameMax(255.0)
	, histogramMode(HistogramMode::Off)
{
	QHBoxLayout* layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	// Projection mode combo
	this->projectionCombo = new QComboBox(this);
	this->projectionCombo->addItem(tr("Slice"));
	this->projectionCombo->addItem(tr("MIP"));
	this->projectionCombo->addItem(tr("Min"));
	this->projectionCombo->addItem(tr("Average"));
	this->projectionCombo->setCurrentIndex(0);
	this->projectionCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	this->projectionCombo->setToolTip(tr("Slice: display a single slice of the stack.\n"
	                                     "MIP: Maximum Intensity Projection across all frames.\n"
	                                     "Min: Minimum Intensity Projection across all frames.\n"
	                                     "Average: Mean projection across all frames."));
	layout->addWidget(this->projectionCombo);

	// LUT combo (text-only when closed, icons when dropdown is open)
	this->lutCombo = new LutComboBox(this);
	this->populateLutCombo();
	this->lutCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout->addWidget(this->lutCombo);

	// Range slider (dual-handle with gradient)
	this->rangeSlider = new RangeSlider(this);
	this->rangeSlider->setRange(0.0, 255.0);
	this->rangeSlider->setValues(0.0, 255.0);
	this->rangeSlider->setGradient(LUT::get(QStringLiteral("Grayscale")));
	layout->addWidget(this->rangeSlider, 1);

	// Auto-range combo
	this->autoRangeCombo = new QComboBox(this);
	this->autoRangeCombo->addItem(tr("Auto (Frame)"));
	this->autoRangeCombo->addItem(tr("Auto (Stack)"));
	this->autoRangeCombo->addItem(tr("Manual"));
	this->autoRangeCombo->setCurrentIndex(0);
	this->autoRangeCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	this->autoRangeCombo->setToolTip(tr("Auto (Frame): scale to min/max of each frame.\n"
	                                    "Auto (Stack): scale to min/max of entire stack.\n"
	                                    "Manual: set display range by hand.\n\n"
	                                    "Auto modes scale each viewer independently.\n"
	                                    "Manual mode applies the same range to both viewers.\n"
	                                    "Double-click slider to toggle. Right-click for fit options."));
	layout->addWidget(this->autoRangeCombo);

	// Log checkbox
	this->logCheck = new QCheckBox(tr("Log"), this);
	this->logCheck->setToolTip(tr("Apply logarithmic scaling to pixel values."));
	layout->addWidget(this->logCheck);

	this->updateManualControlsEnabled();

	// ---- Connections ----

	connect(this->lutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		this->rangeSlider->setGradient(LUT::get(this->lutCombo->currentText()));
		this->emitSettings();
	});

	connect(this->rangeSlider, &RangeSlider::valuesChanged, this, [this]() {
		if (!this->blockEmit) this->emitSettings();
	});

	connect(this->autoRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		this->updateManualControlsEnabled();
		this->emitSettings();
	});

	connect(this->logCheck, &QCheckBox::toggled, this, [this]() { this->emitSettings(); });

	connect(this->projectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		this->emitSettings();
	});

	// Catch double-click on disabled slider to switch to Manual mode
	this->rangeSlider->installEventFilter(this);
}

void DisplayControlBar::populateLutCombo()
{
	QStringList names = LUT::availableNames();
	for (const QString& name : names) {
		QPixmap px = LUT::getPreviewPixmap(name, 64, 12);
		this->lutCombo->addItem(QIcon(px), name);
	}
}

DisplaySettings DisplayControlBar::getSettings() const
{
	DisplaySettings s;
	switch (this->autoRangeCombo->currentIndex()) {
		case 0:  s.autoRangeMode = AutoRangeMode::PerFrame; break;
		case 1:  s.autoRangeMode = AutoRangeMode::PerVolume; break;
		default: s.autoRangeMode = AutoRangeMode::Off; break;
	}
	s.rangeMin = this->rangeSlider->low();
	s.rangeMax = this->rangeSlider->high();
	s.logScale = this->logCheck->isChecked();
	s.lutName = this->lutCombo->currentText();
	switch (this->projectionCombo->currentIndex()) {
		case 1:  s.projectionMode = ProjectionMode::MaxIntensity; break;
		case 2:  s.projectionMode = ProjectionMode::MinIntensity; break;
		case 3:  s.projectionMode = ProjectionMode::Average; break;
		default: s.projectionMode = ProjectionMode::Normal; break;
	}
	return s;
}

void DisplayControlBar::setSettings(const DisplaySettings& settings)
{
	this->blockEmit = true;

	int idx = this->lutCombo->findText(settings.lutName);
	if (idx >= 0) this->lutCombo->setCurrentIndex(idx);

	// Expand slider range to encompass saved handle values (data may not be loaded yet)
	double newLo = qMin(this->rangeSlider->rangeMinimum(), settings.rangeMin);
	double newHi = qMax(this->rangeSlider->rangeMaximum(), settings.rangeMax);
	this->rangeSlider->setRange(newLo, newHi);

	this->rangeSlider->setValues(settings.rangeMin, settings.rangeMax);
	this->rangeSlider->setGradient(LUT::get(settings.lutName));

	switch (settings.autoRangeMode) {
		case AutoRangeMode::PerFrame:  this->autoRangeCombo->setCurrentIndex(0); break;
		case AutoRangeMode::PerVolume: this->autoRangeCombo->setCurrentIndex(1); break;
		case AutoRangeMode::Off:       this->autoRangeCombo->setCurrentIndex(2); break;
	}
	this->logCheck->setChecked(settings.logScale);

	switch (settings.projectionMode) {
		case ProjectionMode::MaxIntensity: this->projectionCombo->setCurrentIndex(1); break;
		case ProjectionMode::MinIntensity: this->projectionCombo->setCurrentIndex(2); break;
		case ProjectionMode::Average:      this->projectionCombo->setCurrentIndex(3); break;
		default:                           this->projectionCombo->setCurrentIndex(0); break;
	}

	this->updateManualControlsEnabled();
	this->blockEmit = false;
}

void DisplayControlBar::setSliderRange(double min, double max)
{
	this->blockEmit = true;
	this->rangeSlider->setRange(min, max);
	this->rangeSlider->setValues(min, max);
	this->blockEmit = false;
	this->emitSettings();
}

void DisplayControlBar::setInputFrameRange(double min, double max)
{
	this->lastInputFrameMin = min;
	this->lastInputFrameMax = max;
}

void DisplayControlBar::setOutputFrameRange(double min, double max)
{
	this->lastOutputFrameMin = min;
	this->lastOutputFrameMax = max;
}

void DisplayControlBar::setIntegerMode(bool intMode)
{
	this->rangeSlider->setIntegerMode(intMode);
}

void DisplayControlBar::setInputHistogram(const HistogramData& hist)
{
	this->inputHistogram = hist;
	if (this->histogramMode == HistogramMode::InputFrame) {
		this->rangeSlider->setHistogram(hist);
	}
}

void DisplayControlBar::setOutputHistogram(const HistogramData& hist)
{
	this->outputHistogram = hist;
	if (this->histogramMode == HistogramMode::OutputFrame) {
		this->rangeSlider->setHistogram(hist);
	}
}

void DisplayControlBar::clearInputHistogram()
{
	this->inputHistogram.bins.clear();
	if (this->histogramMode == HistogramMode::InputFrame) {
		this->rangeSlider->clearHistogram();
	}
}

void DisplayControlBar::clearOutputHistogram()
{
	this->outputHistogram.bins.clear();
	if (this->histogramMode == HistogramMode::OutputFrame) {
		this->rangeSlider->clearHistogram();
	}
}

void DisplayControlBar::setHistogramMode(HistogramMode mode)
{
	if (mode == this->histogramMode) return;
	this->histogramMode = mode;
	switch (mode) {
		case HistogramMode::InputFrame:
			if (!this->inputHistogram.bins.isEmpty()) {
				this->rangeSlider->setHistogram(this->inputHistogram);
			} else {
				this->rangeSlider->clearHistogram();
			}
			break;
		case HistogramMode::OutputFrame:
			if (!this->outputHistogram.bins.isEmpty()) {
				this->rangeSlider->setHistogram(this->outputHistogram);
			} else {
				this->rangeSlider->clearHistogram();
			}
			break;
		default:
			this->rangeSlider->clearHistogram();
			break;
	}
	emit this->histogramModeChanged(mode);
}

HistogramMode DisplayControlBar::getHistogramMode() const
{
	return this->histogramMode;
}

void DisplayControlBar::setSliderExpanded(bool expanded)
{
	this->rangeSlider->setExpanded(expanded);
}

bool DisplayControlBar::isSliderExpanded() const
{
	return this->rangeSlider->isExpanded();
}

void DisplayControlBar::contextMenuEvent(QContextMenuEvent* event)
{
	QMenu menu(this);
	QAction* inputFrame  = menu.addAction(tr("Fit to Input Frame"));
	QAction* outputFrame = menu.addAction(tr("Fit to Output Frame"));
	menu.addSeparator();
	QAction* inputStack  = menu.addAction(tr("Fit to Input Stack"));
	QAction* outputStack = menu.addAction(tr("Fit to Output Stack"));

	menu.addSeparator();
	QMenu* histMenu = menu.addMenu(tr("Histogram"));
	QAction* histOff    = histMenu->addAction(tr("Off"));
	QAction* histInput  = histMenu->addAction(tr("Input Frame"));
	QAction* histOutput = histMenu->addAction(tr("Output Frame"));
	histOff->setCheckable(true);
	histInput->setCheckable(true);
	histOutput->setCheckable(true);
	histOff->setChecked(this->histogramMode == HistogramMode::Off);
	histInput->setChecked(this->histogramMode == HistogramMode::InputFrame);
	histOutput->setChecked(this->histogramMode == HistogramMode::OutputFrame);

	menu.addSeparator();
	QAction* enlargedSlider = menu.addAction(tr("Enlarged Slider"));
	enlargedSlider->setCheckable(true);
	enlargedSlider->setChecked(this->rangeSlider->isExpanded());

	QAction* chosen = menu.exec(event->globalPos());
	if (chosen == inputFrame) {
		this->blockEmit = true;
		this->autoRangeCombo->setCurrentIndex(2); // Switch to Manual
		this->updateManualControlsEnabled();
		this->rangeSlider->setRange(this->lastInputFrameMin, this->lastInputFrameMax);
		this->rangeSlider->setValues(this->lastInputFrameMin, this->lastInputFrameMax);
		this->blockEmit = false;
		this->emitSettings();
	} else if (chosen == outputFrame) {
		this->blockEmit = true;
		this->autoRangeCombo->setCurrentIndex(2); // Switch to Manual
		this->updateManualControlsEnabled();
		this->rangeSlider->setRange(this->lastOutputFrameMin, this->lastOutputFrameMax);
		this->rangeSlider->setValues(this->lastOutputFrameMin, this->lastOutputFrameMax);
		this->blockEmit = false;
		this->emitSettings();
	} else if (chosen == inputStack) {
		this->blockEmit = true;
		this->autoRangeCombo->setCurrentIndex(2); // Switch to Manual
		this->updateManualControlsEnabled();
		this->blockEmit = false;
		emit this->resetToInputStackRequested();
	} else if (chosen == outputStack) {
		this->blockEmit = true;
		this->autoRangeCombo->setCurrentIndex(2); // Switch to Manual
		this->updateManualControlsEnabled();
		this->blockEmit = false;
		emit this->resetToOutputStackRequested();
	} else if (chosen == histOff) {
		this->setHistogramMode(HistogramMode::Off);
	} else if (chosen == histInput) {
		this->setHistogramMode(HistogramMode::InputFrame);
	} else if (chosen == histOutput) {
		this->setHistogramMode(HistogramMode::OutputFrame);
	} else if (chosen == enlargedSlider) {
		this->rangeSlider->setExpanded(!this->rangeSlider->isExpanded());
	}
}

void DisplayControlBar::updateManualControlsEnabled()
{
	bool manual = (this->autoRangeCombo->currentIndex() == 2);
	this->rangeSlider->setEnabled(manual);
}

bool DisplayControlBar::eventFilter(QObject* obj, QEvent* event)
{
	if (obj == this->rangeSlider && event->type() == QEvent::MouseButtonDblClick) {
		QMouseEvent* me = static_cast<QMouseEvent*>(event);
		if (this->rangeSlider->isNearHandle(me->pos().x())) {
			return false; // Let RangeSlider open inline editor
		}
		if (!this->rangeSlider->isEnabled()) {
			// Double-click on disabled slider → switch to Manual
			this->autoRangeCombo->setCurrentIndex(2);
			return true;
		} else {
			// Double-click on enabled slider → switch to Auto (Frame)
			this->autoRangeCombo->setCurrentIndex(0);
			return true;
		}
	}
	return QWidget::eventFilter(obj, event);
}

void DisplayControlBar::emitSettings()
{
	if (!this->blockEmit) {
		emit settingsChanged(this->getSettings());
	}
}
