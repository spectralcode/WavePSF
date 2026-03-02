#include "psfcontrolwidget.h"
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


PSFControlWidget::PSFControlWidget(QWidget* parent)
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
			this, &PSFControlWidget::applyPatchGridSettings);
	connect(this->patchRowsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &PSFControlWidget::applyPatchGridSettings);
	connect(this->borderExtensionSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &PSFControlWidget::applyPatchGridSettings);

	// Forward signals from deconvolution settings
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::algorithmChanged,
			this, &PSFControlWidget::deconvAlgorithmChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::iterationsChanged,
			this, &PSFControlWidget::deconvIterationsChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::relaxationFactorChanged,
			this, &PSFControlWidget::deconvRelaxationFactorChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::regularizationFactorChanged,
			this, &PSFControlWidget::deconvRegularizationFactorChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::noiseToSignalFactorChanged,
			this, &PSFControlWidget::deconvNoiseToSignalFactorChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::liveModeChanged,
			this, &PSFControlWidget::deconvLiveModeChanged);
	connect(this->deconvSettings, &DeconvolutionSettingsWidget::deconvolutionRequested,
			this, &PSFControlWidget::deconvolutionRequested);

	// Forward signals from optimization widget
	connect(this->optimizationWidget, &OptimizationWidget::optimizationRequested,
			this, &PSFControlWidget::optimizationRequested);
	connect(this->optimizationWidget, &OptimizationWidget::optimizationCancelRequested,
			this, &PSFControlWidget::optimizationCancelRequested);
	connect(this->optimizationWidget, &OptimizationWidget::patchSelectionChanged,
			this, &PSFControlWidget::optimizationPatchSelectionChanged);
	connect(this->optimizationWidget, &OptimizationWidget::livePreviewSettingsChanged,
			this, &PSFControlWidget::optimizationLivePreviewChanged);
	connect(this->optimizationWidget, &OptimizationWidget::saParametersChanged,
			this, &PSFControlWidget::optimizationSAParametersChanged);

	// Forward signals from interpolation widget
	connect(this->interpolationWidget, &InterpolationWidget::interpolateInXRequested,
			this, &PSFControlWidget::interpolateInXRequested);
	connect(this->interpolationWidget, &InterpolationWidget::interpolateInYRequested,
			this, &PSFControlWidget::interpolateInYRequested);
	connect(this->interpolationWidget, &InterpolationWidget::interpolateInZRequested,
			this, &PSFControlWidget::interpolateInZRequested);
	connect(this->interpolationWidget, &InterpolationWidget::interpolateAllInZRequested,
			this, &PSFControlWidget::interpolateAllInZRequested);
	connect(this->interpolationWidget, &InterpolationWidget::polynomialOrderChanged,
			this, &PSFControlWidget::interpolationPolynomialOrderChanged);
}

PSFControlWidget::~PSFControlWidget()
{
}

QString PSFControlWidget::getName() const
{
	return QLatin1String(SETTINGS_GROUP);
}

QVariantMap PSFControlWidget::getSettings() const
{
	QVariantMap settings;
	settings.insert("deconvolution", this->deconvSettings->getSettings());
	settings.insert("optimization", this->optimizationWidget->getSettings());
	return settings;
}

void PSFControlWidget::setSettings(const QVariantMap& settings)
{
	this->deconvSettings->setSettings(settings.value("deconvolution").toMap());
	this->optimizationWidget->setSettings(settings.value("optimization").toMap());
}

void PSFControlWidget::setParameterDescriptors(QVector<WavefrontParameter> descriptors)
{
	this->optimizationWidget->setParameterDescriptors(descriptors);
}

void PSFControlWidget::setGroundTruthAvailable(bool available)
{
	this->optimizationWidget->setGroundTruthAvailable(available);
}

void PSFControlWidget::updateOptimizationProgress(const OptimizationProgress& progress)
{
	this->optimizationWidget->updateProgress(progress);
}

void PSFControlWidget::onOptimizationFinished(const OptimizationResult& result)
{
	this->optimizationWidget->onOptimizationFinished(result);
}

void PSFControlWidget::onOptimizationStarted()
{
	this->optimizationWidget->onOptimizationStarted();
}

void PSFControlWidget::updateInterpolationResult(const InterpolationResult& result)
{
	this->interpolationWidget->updateInterpolationResult(result);
}

void PSFControlWidget::updateCurrentPatch(int x, int y)
{
	this->patchGridInfoLabel->setText(QString("Current patch: (%1, %2)").arg(x).arg(y));
}

void PSFControlWidget::setPatchGridConfiguration(int cols, int rows, int borderExtension)
{
	this->updatingPatchGrid = true;
	this->patchColsSpinBox->setValue(cols);
	this->patchRowsSpinBox->setValue(rows);
	this->borderExtensionSpinBox->setValue(borderExtension);
	this->updatingPatchGrid = false;
}

void PSFControlWidget::applyPatchGridSettings()
{
	if (!this->updatingPatchGrid) {
		emit patchGridConfigurationRequested(
			this->patchColsSpinBox->value(),
			this->patchRowsSpinBox->value(),
			this->borderExtensionSpinBox->value());
	}
}
