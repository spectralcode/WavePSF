#include "psfcontrolwidget.h"
#include "coefficienteditorwidget.h"
#include "wavefrontplotwidget.h"
#include "psfpreviewwidget.h"
#include "deconvolutionsettingswidget.h"
#include "optimizationwidget.h"
#include "interpolationwidget.h"
#include "core/psf/wavefrontgeneratorfactory.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QLabel>

namespace {
	const char* SETTINGS_GROUP = "psf_control";
}


PSFControlWidget::PSFControlWidget(QWidget* parent)
	: QGroupBox(tr("PSF Control"), parent)
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);

	this->tabWidget = new QTabWidget(this);
	mainLayout->addWidget(this->tabWidget);

	// PSF Generation tab
	QWidget* generationTab = new QWidget(this->tabWidget);
	QVBoxLayout* genMainLayout = new QVBoxLayout(generationTab);

	// Generator type selector
	QHBoxLayout* genTypeLayout = new QHBoxLayout();
	genTypeLayout->addWidget(new QLabel(tr("Generator:"), generationTab));
	this->generatorTypeCombo = new QComboBox(generationTab);
	this->generatorTypeCombo->addItems(WavefrontGeneratorFactory::availableTypeNames());
	genTypeLayout->addWidget(this->generatorTypeCombo);
	genTypeLayout->addStretch();
	genMainLayout->addLayout(genTypeLayout);

	// Widgets row
	QHBoxLayout* genLayout = new QHBoxLayout();

	this->coeffEditor = new CoefficientEditorWidget(generationTab);
	this->wavefrontPlot = new WavefrontPlotWidget(generationTab);
	this->psfPreview = new PSFPreviewWidget(generationTab);

	genLayout->addWidget(this->coeffEditor, 1);
	genLayout->addWidget(this->wavefrontPlot, 1);
	genLayout->addWidget(this->psfPreview, 1);
	genMainLayout->addLayout(genLayout, 1);

	this->tabWidget->addTab(generationTab, tr("PSF Generation"));

	// Deconvolution settings tab
	this->deconvSettings = new DeconvolutionSettingsWidget(this->tabWidget);
	this->tabWidget->addTab(this->deconvSettings, tr("Deconvolution"));

	// Optimization tab
	this->optimizationWidget = new OptimizationWidget(this->tabWidget);
	this->tabWidget->addTab(this->optimizationWidget, tr("Optimization"));

	// Interpolation tab
	this->interpolationWidget = new InterpolationWidget(this->tabWidget);
	this->tabWidget->addTab(this->interpolationWidget, tr("Interpolation"));

	// Generator type change
	connect(this->generatorTypeCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
			this, &PSFControlWidget::generatorTypeChangeRequested);

	// Forward signals from coefficient editor
	connect(this->coeffEditor, &CoefficientEditorWidget::coefficientChanged,
			this, &PSFControlWidget::coefficientChanged);
	connect(this->coeffEditor, &CoefficientEditorWidget::resetRequested,
			this, &PSFControlWidget::resetRequested);

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

PSFSettings PSFControlWidget::getPSFSettings() const
{
	return this->currentSettings;
}

QVariantMap PSFControlWidget::getSettings() const
{
	QVariantMap settings;
	settings.insert("coefficients", this->coeffEditor->getSettings());
	settings.insert("deconvolution", this->deconvSettings->getSettings());
	settings.insert("optimization", this->optimizationWidget->getSettings());
	settings.insert("psf_settings", serializePSFSettings(this->currentSettings));
	return settings;
}

void PSFControlWidget::setSettings(const QVariantMap& settings)
{
	if (settings.contains("coefficients")) {
		this->coeffEditor->setSettings(settings.value("coefficients").toMap());
	}
	if (settings.contains("deconvolution")) {
		this->deconvSettings->setSettings(settings.value("deconvolution").toMap());
	}
	if (settings.contains("optimization")) {
		this->optimizationWidget->setSettings(settings.value("optimization").toMap());
	}
	if (settings.contains("psf_settings")) {
		this->currentSettings = deserializePSFSettings(settings.value("psf_settings").toMap());
	}
}

void PSFControlWidget::setParameterDescriptors(QVector<WavefrontParameter> descriptors)
{
	this->coeffEditor->setParameterDescriptors(descriptors);
	this->optimizationWidget->setParameterDescriptors(descriptors);
}

void PSFControlWidget::setCoefficients(const QVector<double>& values)
{
	this->coeffEditor->setValues(values);
}

void PSFControlWidget::updateWavefront(af::array wavefront)
{
	this->wavefrontPlot->updatePlot(wavefront);
}

void PSFControlWidget::updatePSF(af::array psf)
{
	this->psfPreview->updateImage(psf);
}

void PSFControlWidget::setPSFSettings(const PSFSettings& settings)
{
	this->currentSettings = settings;
	// Sync combo box without triggering signal
	this->generatorTypeCombo->blockSignals(true);
	int idx = this->generatorTypeCombo->findText(settings.generatorTypeName);
	if (idx >= 0) {
		this->generatorTypeCombo->setCurrentIndex(idx);
	}
	this->generatorTypeCombo->blockSignals(false);
}

void PSFControlWidget::setGeneratorType(const QString& typeName)
{
	this->generatorTypeCombo->blockSignals(true);
	int idx = this->generatorTypeCombo->findText(typeName);
	if (idx >= 0) {
		this->generatorTypeCombo->setCurrentIndex(idx);
	}
	this->generatorTypeCombo->blockSignals(false);
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
