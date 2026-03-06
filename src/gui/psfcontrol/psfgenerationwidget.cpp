#include "psfgenerationwidget.h"
#include "coefficienteditorwidget.h"
#include "wavefrontplotwidget.h"
#include "psfpreviewwidget.h"
#include "core/psf/psfsettings.h"
#include "core/psf/wavefrontgeneratorfactory.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>

namespace {
	const QString SETTINGS_GROUP     = QStringLiteral("psf_generation");
	const QString KEY_PSF_SETTINGS   = QStringLiteral("psf_settings");
	const QString KEY_WAVEFRONT_PLOT = QStringLiteral("wavefront_plot");
}


PSFGenerationWidget::PSFGenerationWidget(QWidget* parent)
	: QGroupBox(tr("PSF Generator"), parent)
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(3, 3, 3, 3);

	// Generator type selector
	QHBoxLayout* genTypeLayout = new QHBoxLayout();
	genTypeLayout->addWidget(new QLabel(tr("Generator:"), this));
	this->generatorTypeCombo = new QComboBox(this);
	this->generatorTypeCombo->addItems(WavefrontGeneratorFactory::availableTypeNames());
	genTypeLayout->addWidget(this->generatorTypeCombo);
	genTypeLayout->addStretch();
	mainLayout->addLayout(genTypeLayout);

	// Content: coefficients (left) | wavefront + PSF (right)
	QHBoxLayout* contentLayout = new QHBoxLayout();

	this->coeffEditor = new CoefficientEditorWidget(this);
	contentLayout->addWidget(this->coeffEditor, 1);

	// Right column: wavefront plot (top) + PSF preview (bottom)
	QVBoxLayout* rightColumn = new QVBoxLayout();
	this->wavefrontPlot = new WavefrontPlotWidget(this);
	this->psfPreview = new PSFPreviewWidget(this);
	rightColumn->addWidget(new QLabel(tr("Wavefront"), this));
	rightColumn->addWidget(this->wavefrontPlot, 1);
	rightColumn->addWidget(new QLabel(tr("PSF"), this));
	rightColumn->addWidget(this->psfPreview, 1);
	contentLayout->addLayout(rightColumn, 4);

	mainLayout->addLayout(contentLayout, 1);

	// Connect signals
	connect(this->generatorTypeCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
			this, &PSFGenerationWidget::generatorTypeChangeRequested);
	connect(this->coeffEditor, &CoefficientEditorWidget::coefficientChanged,
			this, &PSFGenerationWidget::coefficientChanged);
	connect(this->coeffEditor, &CoefficientEditorWidget::resetRequested,
			this, &PSFGenerationWidget::resetRequested);
}

PSFGenerationWidget::~PSFGenerationWidget()
{
}

CoefficientEditorWidget* PSFGenerationWidget::coefficientEditor() const
{
	return this->coeffEditor;
}

QString PSFGenerationWidget::getName() const
{
	return SETTINGS_GROUP;
}

QVariantMap PSFGenerationWidget::getSettings() const
{
	QVariantMap settings;
	settings.insert(KEY_PSF_SETTINGS,   serializePSFSettings(this->currentSettings));
	settings.insert(KEY_WAVEFRONT_PLOT, this->wavefrontPlot->getSettings());
	return settings;
}

void PSFGenerationWidget::setSettings(const QVariantMap& settings)
{
	this->currentSettings = deserializePSFSettings(settings.value(KEY_PSF_SETTINGS).toMap());
	this->wavefrontPlot->setSettings(settings.value(KEY_WAVEFRONT_PLOT).toMap());
}

PSFSettings PSFGenerationWidget::getPSFSettings() const
{
	return this->currentSettings;
}

void PSFGenerationWidget::setParameterDescriptors(QVector<WavefrontParameter> descriptors)
{
	this->coeffEditor->setParameterDescriptors(descriptors);
}

void PSFGenerationWidget::setCoefficients(const QVector<double>& values)
{
	this->coeffEditor->setValues(values);
}

void PSFGenerationWidget::updateWavefront(af::array wavefront)
{
	this->wavefrontPlot->updatePlot(wavefront);
}

void PSFGenerationWidget::updatePSF(af::array psf)
{
	this->psfPreview->updateImage(psf);
}

void PSFGenerationWidget::setPSFSettings(const PSFSettings& settings)
{
	this->currentSettings = settings;
	// Sync combo box without triggering signal
	this->generatorTypeCombo->blockSignals(true);
	int idx = this->generatorTypeCombo->findText(settings.generatorTypeName);
	if (idx >= 0) {
		this->generatorTypeCombo->setCurrentIndex(idx);
	}
	this->generatorTypeCombo->blockSignals(false);

	// Forward aperture settings to wavefront plot
	this->wavefrontPlot->setAperture(settings.apertureGeometry, settings.apertureRadius);
}

void PSFGenerationWidget::setGeneratorType(const QString& typeName)
{
	this->generatorTypeCombo->blockSignals(true);
	int idx = this->generatorTypeCombo->findText(typeName);
	if (idx >= 0) {
		this->generatorTypeCombo->setCurrentIndex(idx);
	}
	this->generatorTypeCombo->blockSignals(false);
}
