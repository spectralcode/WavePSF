#include "processingcontrolwidget.h"
#include "deconvolutionsettingswidget.h"
#include "optimizationwidget.h"
#include "interpolationwidget.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QLabel>
#include <QSpinBox>

namespace {
	const char* SETTINGS_GROUP = "psf_control";
	const int DEFAULT_PATCH_COLS = 4;
	const int DEFAULT_PATCH_ROWS = 4;
	const int DEFAULT_BORDER_EXTENSION = 10;
}


ProcessingControlWidget::ProcessingControlWidget(QWidget* parent)
	: QWidget(parent)
	, updatingPatchGrid(false)
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	this->tabWidget = new QTabWidget(this);
	mainLayout->addWidget(this->tabWidget);

	// Deconvolution settings tab
	this->deconvSettings = new DeconvolutionSettingsWidget(this->tabWidget);
	this->tabWidget->addTab(this->deconvSettings, tr("Deconvolution"));

	// Optimization tab
	this->optimizationWidget = new OptimizationWidget(this->tabWidget);
	this->tabWidget->addTab(this->optimizationWidget, tr("Optimization"));

	// Interpolation tab
	this->interpolationWidget = new InterpolationWidget(this->tabWidget);
	this->tabWidget->addTab(this->interpolationWidget, tr("Interpolation"));

	// Patch Grid tab
	QWidget* patchGridTab = new QWidget(this->tabWidget);
	QVBoxLayout* patchLayout = new QVBoxLayout(patchGridTab);

	this->patchGridInfoLabel = new QLabel(tr("Current patch: (0, 0)"), patchGridTab);
	patchLayout->addWidget(this->patchGridInfoLabel);

	QGridLayout* gridControlsLayout = new QGridLayout();
	gridControlsLayout->addWidget(new QLabel(tr("Columns:"), patchGridTab), 0, 0);
	this->patchColsSpinBox = new QSpinBox(patchGridTab);
	this->patchColsSpinBox->setMinimum(1);
	this->patchColsSpinBox->setMaximum(32);
	this->patchColsSpinBox->setValue(DEFAULT_PATCH_COLS);
	gridControlsLayout->addWidget(this->patchColsSpinBox, 0, 1);

	gridControlsLayout->addWidget(new QLabel(tr("Rows:"), patchGridTab), 1, 0);
	this->patchRowsSpinBox = new QSpinBox(patchGridTab);
	this->patchRowsSpinBox->setMinimum(1);
	this->patchRowsSpinBox->setMaximum(32);
	this->patchRowsSpinBox->setValue(DEFAULT_PATCH_ROWS);
	gridControlsLayout->addWidget(this->patchRowsSpinBox, 1, 1);

	gridControlsLayout->addWidget(new QLabel(tr("Border Extension:"), patchGridTab), 2, 0);
	this->borderExtensionSpinBox = new QSpinBox(patchGridTab);
	this->borderExtensionSpinBox->setMinimum(0);
	this->borderExtensionSpinBox->setMaximum(100);
	this->borderExtensionSpinBox->setValue(DEFAULT_BORDER_EXTENSION);
	this->borderExtensionSpinBox->setSuffix(tr(" px"));
	gridControlsLayout->addWidget(this->borderExtensionSpinBox, 2, 1);

	patchLayout->addLayout(gridControlsLayout);
	patchLayout->addStretch();

	this->tabWidget->addTab(patchGridTab, tr("Patch Grid"));

	// Connect patch grid spinboxes
	connect(this->patchColsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &ProcessingControlWidget::applyPatchGridSettings);
	connect(this->patchRowsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &ProcessingControlWidget::applyPatchGridSettings);
	connect(this->borderExtensionSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &ProcessingControlWidget::applyPatchGridSettings);

	// Forward signals from deconvolution settings
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::algorithmChanged,
			this, &ProcessingControlWidget::deconvAlgorithmChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::iterationsChanged,
			this, &ProcessingControlWidget::deconvIterationsChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::relaxationFactorChanged,
			this, &ProcessingControlWidget::deconvRelaxationFactorChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::regularizationFactorChanged,
			this, &ProcessingControlWidget::deconvRegularizationFactorChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::noiseToSignalFactorChanged,
			this, &ProcessingControlWidget::deconvNoiseToSignalFactorChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::liveModeChanged,
			this, &ProcessingControlWidget::deconvLiveModeChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::deconvolutionRequested,
			this, &ProcessingControlWidget::deconvolutionRequested);

	// Forward signals from optimization widget
	connect(this->optimizationWidget, &OptimizationWidget::optimizationRequested,
			this, &ProcessingControlWidget::optimizationRequested);
	connect(this->optimizationWidget, &OptimizationWidget::optimizationCancelRequested,
			this, &ProcessingControlWidget::optimizationCancelRequested);
	connect(this->optimizationWidget, &OptimizationWidget::patchSelectionChanged,
			this, &ProcessingControlWidget::optimizationPatchSelectionChanged);
	connect(this->optimizationWidget, &OptimizationWidget::livePreviewSettingsChanged,
			this, &ProcessingControlWidget::optimizationLivePreviewChanged);
	connect(this->optimizationWidget, &OptimizationWidget::algorithmParametersChanged,
			this, &ProcessingControlWidget::optimizationAlgorithmParametersChanged);

	// Forward signals from interpolation widget
	connect(this->interpolationWidget, &InterpolationWidget::interpolateInXRequested,
			this, &ProcessingControlWidget::interpolateInXRequested);
	connect(this->interpolationWidget, &InterpolationWidget::interpolateInYRequested,
			this, &ProcessingControlWidget::interpolateInYRequested);
	connect(this->interpolationWidget, &InterpolationWidget::interpolateInZRequested,
			this, &ProcessingControlWidget::interpolateInZRequested);
	connect(this->interpolationWidget, &InterpolationWidget::interpolateAllInZRequested,
			this, &ProcessingControlWidget::interpolateAllInZRequested);
	connect(this->interpolationWidget, &InterpolationWidget::polynomialOrderChanged,
			this, &ProcessingControlWidget::interpolationPolynomialOrderChanged);
}

ProcessingControlWidget::~ProcessingControlWidget()
{
}

QString ProcessingControlWidget::getName() const
{
	return QLatin1String(SETTINGS_GROUP);
}

QVariantMap ProcessingControlWidget::getSettings() const
{
	QVariantMap settings;
	settings.insert("deconvolution", this->deconvSettings->getSettings());
	settings.insert("optimization", this->optimizationWidget->getSettings());
	return settings;
}

void ProcessingControlWidget::setSettings(const QVariantMap& settings)
{
	this->deconvSettings->setSettings(settings.value("deconvolution").toMap());
	this->optimizationWidget->setSettings(settings.value("optimization").toMap());
}

void ProcessingControlWidget::setParameterDescriptors(QVector<WavefrontParameter> descriptors)
{
	this->optimizationWidget->setParameterDescriptors(descriptors);
}

void ProcessingControlWidget::setGroundTruthAvailable(bool available)
{
	this->optimizationWidget->setGroundTruthAvailable(available);
}

void ProcessingControlWidget::setCurrentFrame(int frame)
{
	this->optimizationWidget->setCurrentFrame(frame);
}

void ProcessingControlWidget::updateOptimizationProgress(const OptimizationProgress& progress)
{
	this->optimizationWidget->updateProgress(progress);
}

void ProcessingControlWidget::onOptimizationFinished(const OptimizationResult& result)
{
	this->optimizationWidget->onOptimizationFinished(result);
}

void ProcessingControlWidget::onOptimizationStarted()
{
	this->optimizationWidget->onOptimizationStarted();
}

void ProcessingControlWidget::updateInterpolationResult(const InterpolationResult& result)
{
	this->interpolationWidget->updateInterpolationResult(result);
}

void ProcessingControlWidget::updateCurrentPatch(int x, int y)
{
	this->patchGridInfoLabel->setText(QString("Current patch: (%1, %2)").arg(x).arg(y));
}

void ProcessingControlWidget::setPatchGridConfiguration(int cols, int rows, int borderExtension)
{
	this->updatingPatchGrid = true;
	this->patchColsSpinBox->setValue(cols);
	this->patchRowsSpinBox->setValue(rows);
	this->borderExtensionSpinBox->setValue(borderExtension);
	this->updatingPatchGrid = false;
}

void ProcessingControlWidget::applyPatchGridSettings()
{
	if (!this->updatingPatchGrid) {
		emit patchGridConfigurationRequested(
			this->patchColsSpinBox->value(),
			this->patchRowsSpinBox->value(),
			this->borderExtensionSpinBox->value());
	}
}
