#include "optimizationwidget.h"
#include "core/optimization/imagemetriccalculator.h"
#include "core/psf/psfsettings.h"
#include "qcustomplot.h"
#include "gui/qcppaletteobserver.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFrame>
#include <QCheckBox>
#include "gui/verticalscrollarea.h"
#include <QEvent>
#include <QMenu>
#include "gui/plotutils.h"

namespace {
	const char* SETTINGS_GROUP = "optimization";
}

OptimizationWidget::OptimizationWidget(QWidget* parent)
	: QWidget(parent),
	  groundTruthAvailable(false),
	  isRunning(false)
{
	this->setupUI();
}

OptimizationWidget::~OptimizationWidget()
{
}

void OptimizationWidget::setupUI()
{
	QHBoxLayout* mainLayout = new QHBoxLayout(this);

	// Left side: controls in scroll area
	VerticalScrollArea* scrollArea = new VerticalScrollArea(this);

	QWidget* scrollContent = new QWidget(scrollArea);
	QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);

	this->setupModeSection(contentLayout);
	this->setupBatchSection(contentLayout);
	this->setupInitialValuesSection(contentLayout);
	this->setupSAParametersSection(contentLayout);
	this->setupMetricSection(contentLayout);
	this->setupCoefficientSection(contentLayout);
	this->setupControlSection(contentLayout);
	this->setupStatusSection(contentLayout);

	contentLayout->addStretch();
	scrollArea->setWidget(scrollContent);

	// Right side: metric plot
	this->setupPlotSection();

	mainLayout->addWidget(scrollArea, 0);
	mainLayout->addWidget(this->metricPlot, 1);
}

void OptimizationWidget::setupModeSection(QVBoxLayout* layout)
{
	QHBoxLayout* modeLayout = new QHBoxLayout();
	modeLayout->addWidget(new QLabel(tr("Mode:"), this));

	this->modeComboBox = new QComboBox(this);
	this->modeComboBox->addItem(tr("Single Patch"));
	this->modeComboBox->addItem(tr("Batch"));
	this->installScrollGuard(this->modeComboBox);
	modeLayout->addWidget(this->modeComboBox);
	modeLayout->addStretch();

	layout->addLayout(modeLayout);

	connect(this->modeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &OptimizationWidget::onModeChanged);
}

void OptimizationWidget::setupBatchSection(QVBoxLayout* layout)
{
	this->batchGroup = new QGroupBox(tr("Batch Settings"), this);
	QFormLayout* batchForm = new QFormLayout(this->batchGroup);

	this->patchesLineEdit = new QLineEdit(this->batchGroup);
	this->patchesLineEdit->setPlaceholderText(tr("e.g. 0, 2, 4-7"));
	this->patchesLineEdit->setToolTip(tr("Patch indices to optimize (comma-separated, ranges with dash)"));
	batchForm->addRow(tr("Patches:"), this->patchesLineEdit);

	this->framesLineEdit = new QLineEdit(this->batchGroup);
	this->framesLineEdit->setPlaceholderText(tr("e.g. 0-500:50"));
	this->framesLineEdit->setToolTip(tr("Frame specification (ranges with dash, step with colon)"));
	batchForm->addRow(tr("Frames:"), this->framesLineEdit);

	layout->addWidget(this->batchGroup);
	this->batchGroup->setVisible(false);

	connect(this->patchesLineEdit, &QLineEdit::textChanged,
			this, &OptimizationWidget::onPatchTextChanged);

	// Install event filter to highlight patches on focus
	this->patchesLineEdit->installEventFilter(this);
}

void OptimizationWidget::setupInitialValuesSection(QVBoxLayout* layout)
{
	this->initialValuesGroup = new QGroupBox(tr("Initial Values"), this);
	QFormLayout* form = new QFormLayout(this->initialValuesGroup);

	this->startCoeffSourceComboBox = new QComboBox(this->initialValuesGroup);
	this->startCoeffSourceComboBox->addItem(tr("Current stored"));
	this->startCoeffSourceComboBox->addItem(tr("From specific frame"));
	this->startCoeffSourceComboBox->addItem(tr("Offset from current"));
	this->startCoeffSourceComboBox->addItem(tr("Random"));
	this->startCoeffSourceComboBox->addItem(tr("Zero"));
	this->startCoeffSourceComboBox->addItem(tr("From previous patch result"));
	this->startCoeffSourceComboBox->addItem(tr("From specific patch"));
	this->installScrollGuard(this->startCoeffSourceComboBox);
	form->addRow(tr("Source:"), this->startCoeffSourceComboBox);

	this->sourceParamLabel = new QLabel(tr("Frame #:"), this->initialValuesGroup);
	this->sourceParamSpinBox = new QSpinBox(this->initialValuesGroup);
	this->sourceParamSpinBox->setRange(-100000, 100000);
	this->sourceParamSpinBox->setValue(0);
	this->installScrollGuard(this->sourceParamSpinBox);
	form->addRow(this->sourceParamLabel, this->sourceParamSpinBox);

	layout->addWidget(this->initialValuesGroup);

	connect(this->startCoeffSourceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &OptimizationWidget::onStartCoefficientSourceChanged);

	// Initial visibility for source param
	this->onStartCoefficientSourceChanged(0);
}

void OptimizationWidget::setupSAParametersSection(QVBoxLayout* layout)
{
	QGroupBox* saGroup = new QGroupBox(tr("Simulated Annealing"), this);
	QFormLayout* saForm = new QFormLayout(saGroup);

	this->startTempSpinBox = new QDoubleSpinBox(saGroup);
	this->startTempSpinBox->setRange(0.001, 1000.0);
	this->startTempSpinBox->setDecimals(3);
	this->startTempSpinBox->setValue(1.0);
	this->startTempSpinBox->setSingleStep(0.1);
	this->installScrollGuard(this->startTempSpinBox);
	saForm->addRow(tr("Start Temperature:"), this->startTempSpinBox);

	this->endTempSpinBox = new QDoubleSpinBox(saGroup);
	this->endTempSpinBox->setRange(0.00001, 100.0);
	this->endTempSpinBox->setDecimals(5);
	this->endTempSpinBox->setValue(0.001);
	this->endTempSpinBox->setSingleStep(0.001);
	this->installScrollGuard(this->endTempSpinBox);
	saForm->addRow(tr("End Temperature:"), this->endTempSpinBox);

	this->coolingFactorSpinBox = new QDoubleSpinBox(saGroup);
	this->coolingFactorSpinBox->setRange(0.01, 0.9999);
	this->coolingFactorSpinBox->setDecimals(4);
	this->coolingFactorSpinBox->setValue(0.95);
	this->coolingFactorSpinBox->setSingleStep(0.01);
	this->installScrollGuard(this->coolingFactorSpinBox);
	saForm->addRow(tr("Cooling Factor:"), this->coolingFactorSpinBox);

	this->startPerturbanceSpinBox = new QDoubleSpinBox(saGroup);
	this->startPerturbanceSpinBox->setRange(0.0001, 10.0);
	this->startPerturbanceSpinBox->setDecimals(4);
	this->startPerturbanceSpinBox->setValue(0.05);
	this->startPerturbanceSpinBox->setSingleStep(0.01);
	this->installScrollGuard(this->startPerturbanceSpinBox);
	saForm->addRow(tr("Start Perturbance:"), this->startPerturbanceSpinBox);

	this->endPerturbanceSpinBox = new QDoubleSpinBox(saGroup);
	this->endPerturbanceSpinBox->setRange(0.0001, 10.0);
	this->endPerturbanceSpinBox->setDecimals(4);
	this->endPerturbanceSpinBox->setValue(0.001);
	this->endPerturbanceSpinBox->setSingleStep(0.001);
	this->installScrollGuard(this->endPerturbanceSpinBox);
	saForm->addRow(tr("End Perturbance:"), this->endPerturbanceSpinBox);

	this->itersPerTempSpinBox = new QSpinBox(saGroup);
	this->itersPerTempSpinBox->setRange(1, 10000);
	this->itersPerTempSpinBox->setValue(10);
	this->installScrollGuard(this->itersPerTempSpinBox);
	saForm->addRow(tr("Iterations/Temperature:"), this->itersPerTempSpinBox);

	layout->addWidget(saGroup);

	// Live SA parameter updates during optimization
	connect(this->endTempSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &OptimizationWidget::emitSAParametersChanged);
	connect(this->coolingFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &OptimizationWidget::emitSAParametersChanged);
	connect(this->startPerturbanceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &OptimizationWidget::emitSAParametersChanged);
	connect(this->endPerturbanceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &OptimizationWidget::emitSAParametersChanged);
	connect(this->itersPerTempSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &OptimizationWidget::emitSAParametersChanged);
}

void OptimizationWidget::setupMetricSection(QVBoxLayout* layout)
{
	QGroupBox* metricGroup = new QGroupBox(tr("Metric"), this);
	QFormLayout* metricForm = new QFormLayout(metricGroup);

	this->metricModeComboBox = new QComboBox(metricGroup);
	this->metricModeComboBox->addItem(tr("Single Image"));
	this->metricModeComboBox->addItem(tr("Reference Comparison"));
	this->installScrollGuard(this->metricModeComboBox);
	metricForm->addRow(tr("Mode:"), this->metricModeComboBox);

	this->metricTypeComboBox = new QComboBox(metricGroup);
	this->metricTypeComboBox->addItems(ImageMetricCalculator::imageMetricNames());
	this->installScrollGuard(this->metricTypeComboBox);
	metricForm->addRow(tr("Type:"), this->metricTypeComboBox);

	this->metricMultiplierSpinBox = new QDoubleSpinBox(metricGroup);
	this->metricMultiplierSpinBox->setRange(-10000.0, 10000.0);
	this->metricMultiplierSpinBox->setValue(1.0);
	this->metricMultiplierSpinBox->setDecimals(2);
	this->metricMultiplierSpinBox->setSingleStep(1.0);
	this->installScrollGuard(this->metricMultiplierSpinBox);
	metricForm->addRow(tr("Multiplier:"), this->metricMultiplierSpinBox);

	layout->addWidget(metricGroup);

	connect(this->metricModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &OptimizationWidget::onMetricModeChanged);
}

void OptimizationWidget::setupCoefficientSection(QVBoxLayout* layout)
{
	QGroupBox* coeffGroup = new QGroupBox(tr("Coefficients to Optimize"), this);
	QFormLayout* coeffForm = new QFormLayout(coeffGroup);

	this->coefficientSpecLineEdit = new QLineEdit(coeffGroup);
	this->coefficientSpecLineEdit->setPlaceholderText(tr("e.g. 2-8, 11"));
	this->coefficientSpecLineEdit->setToolTip(
		tr("Parameter IDs to optimize (e.g. Noll indices for Zernike, comma-separated, ranges with dash)"));
	coeffForm->addRow(tr("IDs:"), this->coefficientSpecLineEdit);

	layout->addWidget(coeffGroup);
}

void OptimizationWidget::setupControlSection(QVBoxLayout* layout)
{
	QHBoxLayout* controlLayout = new QHBoxLayout();

	this->startButton = new QPushButton(tr("Start Optimization"), this);
	this->cancelButton = new QPushButton(tr("Cancel"), this);
	this->cancelButton->setEnabled(false);

	controlLayout->addWidget(this->startButton);
	controlLayout->addWidget(this->cancelButton);
	controlLayout->addStretch();

	layout->addLayout(controlLayout);

	// Live preview
	QHBoxLayout* previewLayout = new QHBoxLayout();

	this->livePreviewCheckBox = new QCheckBox(tr("Live Preview"), this);
	previewLayout->addWidget(this->livePreviewCheckBox);

	previewLayout->addWidget(new QLabel(tr("every"), this));
	this->livePreviewIntervalSpinBox = new QSpinBox(this);
	this->livePreviewIntervalSpinBox->setRange(1, 100);
	this->livePreviewIntervalSpinBox->setValue(10);
	this->livePreviewIntervalSpinBox->setEnabled(false);
	this->installScrollGuard(this->livePreviewIntervalSpinBox);
	previewLayout->addWidget(this->livePreviewIntervalSpinBox);
	previewLayout->addWidget(new QLabel(tr("iterations"), this));
	previewLayout->addStretch();

	layout->addLayout(previewLayout);

	connect(this->startButton, &QPushButton::clicked,
			this, &OptimizationWidget::onStartClicked);
	connect(this->cancelButton, &QPushButton::clicked,
			this, &OptimizationWidget::onCancelClicked);
	connect(this->livePreviewCheckBox, &QCheckBox::toggled,
			this->livePreviewIntervalSpinBox, &QSpinBox::setEnabled);
	connect(this->livePreviewCheckBox, &QCheckBox::toggled,
			this, &OptimizationWidget::emitLivePreviewSettingsChanged);
	connect(this->livePreviewIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &OptimizationWidget::emitLivePreviewSettingsChanged);
}

void OptimizationWidget::setupStatusSection(QVBoxLayout* layout)
{
	QGroupBox* statusGroup = new QGroupBox(tr("Status"), this);
	QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);

	this->statusLabel = new QLabel(tr("Idle"), statusGroup);
	this->statusLabel->setStyleSheet("font-weight: bold;");
	statusLayout->addWidget(this->statusLabel);

	QHBoxLayout* detailLayout = new QHBoxLayout();
	this->temperatureLabel = new QLabel(tr("T: ---"), statusGroup);
	this->iterationLabel = new QLabel(tr("Iter: ---"), statusGroup);
	this->bestMetricLabel = new QLabel(tr("Best: ---"), statusGroup);
	detailLayout->addWidget(this->temperatureLabel);
	detailLayout->addWidget(this->iterationLabel);
	detailLayout->addWidget(this->bestMetricLabel);
	detailLayout->addStretch();
	statusLayout->addLayout(detailLayout);

	this->batchStatusLabel = new QLabel(statusGroup);
	this->batchStatusLabel->setVisible(false);
	statusLayout->addWidget(this->batchStatusLabel);

	layout->addWidget(statusGroup);
}

void OptimizationWidget::setupPlotSection()
{
	this->metricPlot = new QCustomPlot(this);
	this->metricPlot->addGraph();
	this->metricPlot->xAxis->setLabel(tr("Iteration"));
	this->metricPlot->yAxis->setLabel(tr("Metric"));
	this->metricPlot->graph(0)->setPen(QPen(QColor(0, 120, 215), 1.5));

	// Enable zoom and pan
	this->metricPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

	// Double-click to reset view
	connect(this->metricPlot, &QCustomPlot::mouseDoubleClick,
			this, &OptimizationWidget::resetPlotView);

	// Context menu
	this->metricPlot->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this->metricPlot, &QWidget::customContextMenuRequested,
			this, &OptimizationWidget::showPlotContextMenu);

	// Palette-aware theming
	new QCPPaletteObserver(this->metricPlot);
}

void OptimizationWidget::onModeChanged(int index)
{
	this->batchGroup->setVisible(index == 1);
}

void OptimizationWidget::onMetricModeChanged(int index)
{
	this->metricTypeComboBox->clear();
	if (index == 0) {
		this->metricTypeComboBox->addItems(ImageMetricCalculator::imageMetricNames());
	} else {
		this->metricTypeComboBox->addItems(ImageMetricCalculator::referenceMetricNames());
	}
}

void OptimizationWidget::onStartCoefficientSourceChanged(int index)
{
	// 0=Current stored, 1=From specific frame, 2=Offset, 3=Random, 4=Zero,
	// 5=Previous patch result, 6=From specific patch
	const bool showParam = (index == 1 || index == 2 || index == 6);
	this->sourceParamLabel->setVisible(showParam);
	this->sourceParamSpinBox->setVisible(showParam);

	if (index == 1) {
		this->sourceParamLabel->setText(tr("Frame #:"));
		this->sourceParamSpinBox->setMinimum(0);
	} else if (index == 2) {
		this->sourceParamLabel->setText(tr("Offset:"));
		this->sourceParamSpinBox->setMinimum(-100000);
	} else if (index == 6) {
		this->sourceParamLabel->setText(tr("Patch #:"));
		this->sourceParamSpinBox->setMinimum(0);
	}
}

void OptimizationWidget::onStartClicked()
{
	OptimizationConfig config = this->buildConfig();
	emit optimizationRequested(config);
}

void OptimizationWidget::onCancelClicked()
{
	emit optimizationCancelRequested();
}

void OptimizationWidget::onPatchTextChanged(const QString& text)
{
	QVector<int> patchIds = parseIndexSpec(text);
	emit patchSelectionChanged(patchIds);
}

void OptimizationWidget::resetPlotView()
{
	this->metricPlot->rescaleAxes();
	this->metricPlot->replot();
}

void OptimizationWidget::showPlotContextMenu(const QPoint& pos)
{
	QMenu menu(this);
	QAction* saveAction = menu.addAction(tr("Save Plot as..."));
	connect(saveAction, &QAction::triggered, this, [this]() {
		PlotUtils::savePlotToDisk(this->metricPlot, this);
	});
	menu.addSeparator();
	QAction* resetAction = menu.addAction(tr("Reset View"));
	connect(resetAction, &QAction::triggered, this, &OptimizationWidget::resetPlotView);
	menu.exec(this->metricPlot->mapToGlobal(pos));
}

void OptimizationWidget::setParameterDescriptors(const QVector<WavefrontParameter>& descriptors)
{
	this->parameterDescriptors = descriptors;

	// Update placeholder with valid ID range
	if (!descriptors.isEmpty()) {
		this->coefficientSpecLineEdit->setPlaceholderText(
			QString("e.g. %1-%2").arg(descriptors.first().id).arg(descriptors.last().id));
	}

	// If the line edit is empty, default to all coefficients (by ID)
	if (this->coefficientSpecLineEdit->text().isEmpty()) {
		this->coefficientSpecLineEdit->setText(
			QString("%1-%2").arg(descriptors.first().id).arg(descriptors.last().id));
	}
}

void OptimizationWidget::setGroundTruthAvailable(bool available)
{
	this->groundTruthAvailable = available;
	// Enable/disable reference comparison option
	// Index 1 is "Reference Comparison"
	if (!available && this->metricModeComboBox->currentIndex() == 1) {
		this->metricModeComboBox->setCurrentIndex(0);
	}
}

void OptimizationWidget::updateProgress(const OptimizationProgress& progress)
{
	this->temperatureLabel->setText(QString("T: %1").arg(progress.temperature, 0, 'g', 4));
	this->iterationLabel->setText(QString("Iter: %1").arg(progress.outerIteration));
	this->bestMetricLabel->setText(QString("Best: %1").arg(progress.bestMetric, 0, 'g', 6));

	if (progress.totalJobs > 1) {
		this->batchStatusLabel->setVisible(true);
		this->batchStatusLabel->setText(
			QString("Job %1/%2  Frame: %3  Patch: (%4, %5)")
				.arg(progress.currentJobIndex + 1)
				.arg(progress.totalJobs)
				.arg(progress.currentFrameNr)
				.arg(progress.currentPatchX)
				.arg(progress.currentPatchY));
	}

	// Accumulate plot data (cheap)
	this->plotIterations.append(this->plotIterations.size());
	this->plotMetricValues.append(progress.bestMetric);
	this->metricPlot->graph(0)->setData(this->plotIterations, this->plotMetricValues);

	// Throttle the expensive replot() to ~5/sec to keep GUI responsive
	if (this->replotTimer.elapsed() >= 200) {
		this->metricPlot->rescaleAxes();
		this->metricPlot->replot();
		this->replotTimer.restart();
	}
}

void OptimizationWidget::onOptimizationFinished(const OptimizationResult& result)
{
	this->setRunning(false);

	if (result.wasCancelled) {
		this->statusLabel->setText(tr("Cancelled"));
	} else {
		this->statusLabel->setText(
			QString(tr("Finished (%1 jobs, %2 iterations)"))
				.arg(result.jobResults.size())
				.arg(result.totalOuterIterations));
	}

	// Final replot to show all accumulated data points
	this->metricPlot->rescaleAxes();
	this->metricPlot->replot();
}

void OptimizationWidget::onOptimizationStarted()
{
	this->setRunning(true);
	this->statusLabel->setText(tr("Running..."));

	// Clear plot
	this->plotIterations.clear();
	this->plotMetricValues.clear();
	this->metricPlot->graph(0)->setData(this->plotIterations, this->plotMetricValues);
	this->metricPlot->replot();
	this->replotTimer.start();
}

OptimizationConfig OptimizationWidget::buildConfig() const
{
	OptimizationConfig config;

	// SA parameters
	config.startTemperature = this->startTempSpinBox->value();
	config.endTemperature = this->endTempSpinBox->value();
	config.coolingFactor = this->coolingFactorSpinBox->value();
	config.startPerturbance = this->startPerturbanceSpinBox->value();
	config.endPerturbance = this->endPerturbanceSpinBox->value();
	config.iterationsPerTemperature = this->itersPerTempSpinBox->value();

	// Convert user-entered parameter IDs to 0-based array indices
	QVector<int> requestedIds = parseIndexSpec(this->coefficientSpecLineEdit->text());
	for (int id : requestedIds) {
		for (int i = 0; i < this->parameterDescriptors.size(); ++i) {
			if (this->parameterDescriptors[i].id == id) {
				config.selectedCoefficientIndices.append(i);
				break;
			}
		}
	}

	// Metric
	config.useReferenceMetric = (this->metricModeComboBox->currentIndex() == 1);
	if (config.useReferenceMetric) {
		config.referenceMetric = this->metricTypeComboBox->currentIndex();
	} else {
		config.imageMetric = this->metricTypeComboBox->currentIndex();
	}
	config.metricMultiplier = this->metricMultiplierSpinBox->value();

	// Mode and batch spec
	config.mode = this->modeComboBox->currentIndex();
	config.patchSpec = this->patchesLineEdit->text();
	config.frameSpec = this->framesLineEdit->text();

	// Initial values (always available)
	config.startCoefficientSource = this->startCoeffSourceComboBox->currentIndex();
	config.sourceParam = this->sourceParamSpinBox->value();

	// Live preview
	config.livePreview = this->livePreviewCheckBox->isChecked();
	config.livePreviewInterval = this->livePreviewIntervalSpinBox->value();

	return config;
}

void OptimizationWidget::setRunning(bool running)
{
	this->isRunning = running;
	this->startButton->setEnabled(!running);
	this->cancelButton->setEnabled(running);
	this->modeComboBox->setEnabled(!running);
	this->batchGroup->setEnabled(!running);
	this->initialValuesGroup->setEnabled(!running);
}

void OptimizationWidget::emitSAParametersChanged()
{
	if (this->isRunning) {
		emit saParametersChanged(
			this->endTempSpinBox->value(),
			this->coolingFactorSpinBox->value(),
			this->startPerturbanceSpinBox->value(),
			this->endPerturbanceSpinBox->value(),
			this->itersPerTempSpinBox->value());
	}
}

void OptimizationWidget::emitLivePreviewSettingsChanged()
{
	if (this->isRunning) {
		emit livePreviewSettingsChanged(
			this->livePreviewCheckBox->isChecked(),
			this->livePreviewIntervalSpinBox->value());
	}
}

QString OptimizationWidget::getName() const
{
	return QLatin1String(SETTINGS_GROUP);
}

QVariantMap OptimizationWidget::getSettings() const
{
	QVariantMap settings;
	settings.insert("mode", this->modeComboBox->currentIndex());
	settings.insert("startTemperature", this->startTempSpinBox->value());
	settings.insert("endTemperature", this->endTempSpinBox->value());
	settings.insert("coolingFactor", this->coolingFactorSpinBox->value());
	settings.insert("startPerturbance", this->startPerturbanceSpinBox->value());
	settings.insert("endPerturbance", this->endPerturbanceSpinBox->value());
	settings.insert("itersPerTemp", this->itersPerTempSpinBox->value());
	settings.insert("metricMode", this->metricModeComboBox->currentIndex());
	settings.insert("metricType", this->metricTypeComboBox->currentIndex());
	settings.insert("metricMultiplier", this->metricMultiplierSpinBox->value());
	settings.insert("patches", this->patchesLineEdit->text());
	settings.insert("frames", this->framesLineEdit->text());
	settings.insert("startCoeffSource", this->startCoeffSourceComboBox->currentIndex());
	settings.insert("sourceParam", this->sourceParamSpinBox->value());
	settings.insert("coefficientSpec", this->coefficientSpecLineEdit->text());
	settings.insert("livePreview", this->livePreviewCheckBox->isChecked());
	settings.insert("livePreviewInterval", this->livePreviewIntervalSpinBox->value());
	return settings;
}

void OptimizationWidget::setSettings(const QVariantMap& settings)
{
	this->modeComboBox->setCurrentIndex(settings.value("mode", 0).toInt());
	this->startTempSpinBox->setValue(settings.value("startTemperature", 0.1).toDouble());
	this->endTempSpinBox->setValue(settings.value("endTemperature", 0.001).toDouble());
	this->coolingFactorSpinBox->setValue(settings.value("coolingFactor", 0.95).toDouble());
	if (settings.contains("startPerturbance")) {
		this->startPerturbanceSpinBox->setValue(settings.value("startPerturbance").toDouble());
	} else {
		this->startPerturbanceSpinBox->setValue(settings.value("perturbance", 0.05).toDouble());
	}
	this->endPerturbanceSpinBox->setValue(settings.value("endPerturbance", 0.001).toDouble());
	this->itersPerTempSpinBox->setValue(settings.value("itersPerTemp", 1).toInt());
	this->metricModeComboBox->setCurrentIndex(settings.value("metricMode", 1).toInt());
	this->metricTypeComboBox->setCurrentIndex(settings.value("metricType", 1).toInt());
	this->metricMultiplierSpinBox->setValue(settings.value("metricMultiplier", -100.0).toDouble());
	if (settings.contains("patches")) {
		this->patchesLineEdit->setText(settings.value("patches").toString());
	}
	if (settings.contains("frames")) {
		this->framesLineEdit->setText(settings.value("frames").toString());
	}
	this->startCoeffSourceComboBox->setCurrentIndex(settings.value("startCoeffSource", 0).toInt());
	this->sourceParamSpinBox->setValue(settings.value("sourceParam", 0).toInt());
	this->coefficientSpecLineEdit->setText(settings.value("coefficientSpec", "2-8").toString());
	this->livePreviewCheckBox->setChecked(settings.value("livePreview", true).toBool());
	this->livePreviewIntervalSpinBox->setValue(settings.value("livePreviewInterval", 10).toInt());
}

void OptimizationWidget::installScrollGuard(QWidget* widget)
{
	widget->setFocusPolicy(Qt::StrongFocus);
	widget->installEventFilter(this);
}

bool OptimizationWidget::eventFilter(QObject* obj, QEvent* event)
{
	if (event->type() == QEvent::Wheel) {
		QWidget* widget = qobject_cast<QWidget*>(obj);
		if (widget && !widget->hasFocus()) {
			event->ignore();
			return true;
		}
	}

	// Highlight patches when the patches field gains focus
	if (obj == this->patchesLineEdit) {
		if (event->type() == QEvent::FocusIn) {
			QVector<int> patchIds = parseIndexSpec(this->patchesLineEdit->text());
			emit patchSelectionChanged(patchIds);
		} else if (event->type() == QEvent::FocusOut) {
			emit patchSelectionChanged(QVector<int>());
		}
	}

	return QWidget::eventFilter(obj, event);
}
