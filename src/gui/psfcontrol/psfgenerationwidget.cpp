#include "psfgenerationwidget.h"
#include "coefficienteditorwidget.h"
#include "wavefrontplotwidget.h"
#include "psfpreviewwidget.h"
#include "psf3dpreviewwidget.h"
#include "rwsettingswidget.h"
#include "core/psf/psfsettings.h"
#include "core/psf/psfmodule.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QComboBox>
#include <QLabel>
#include <QSplitter>

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
	this->generatorTypeCombo->addItems(PSFModule::availablePSFModes());
	genTypeLayout->addWidget(this->generatorTypeCombo);
	genTypeLayout->addStretch();
	mainLayout->addLayout(genTypeLayout);

	// Inline Richards-Wolf settings (visible only in 3D Microscopy mode)
	this->rwSettingsWidget = new RWSettingsWidget(this);
	this->rwSettingsWidget->setVisible(false);
	mainLayout->addWidget(this->rwSettingsWidget);

	// Content: coefficients (left) | wavefront + PSF (right)
	QHBoxLayout* contentLayout = new QHBoxLayout();

	this->coeffEditor = new CoefficientEditorWidget(this);
	contentLayout->addWidget(this->coeffEditor, 1);

	// Right column: wavefront plot (top) + PSF preview (bottom), resizable via splitter
	QSplitter* rightSplitter = new QSplitter(Qt::Vertical, this);

	QWidget* wavefrontContainer = new QWidget(this);
	QVBoxLayout* wfLayout = new QVBoxLayout(wavefrontContainer);
	wfLayout->setContentsMargins(0, 0, 0, 0);
	this->wavefrontPlot = new WavefrontPlotWidget(this);
	wfLayout->addWidget(new QLabel(tr("Wavefront"), this));
	wfLayout->addWidget(this->wavefrontPlot, 1);
	rightSplitter->addWidget(wavefrontContainer);

	// PSF preview: stacked widget for 2D/3D modes
	QWidget* psfContainer = new QWidget(this);
	QVBoxLayout* psfLayout = new QVBoxLayout(psfContainer);
	psfLayout->setContentsMargins(0, 0, 0, 0);
	psfLayout->addWidget(new QLabel(tr("PSF"), this));
	this->psfPreviewStack = new QStackedWidget(this);
	this->psfPreview = new PSFPreviewWidget(this);
	this->psfPreviewStack->addWidget(this->psfPreview);       // page 0: 2D
	this->psf3DPreview = new PSF3DPreviewWidget(this);
	this->psfPreviewStack->addWidget(this->psf3DPreview);     // page 1: 3D
	psfLayout->addWidget(this->psfPreviewStack, 1);
	rightSplitter->addWidget(psfContainer);

	contentLayout->addWidget(rightSplitter, 4);

	mainLayout->addLayout(contentLayout, 1);

	// Connect signals
	connect(this->generatorTypeCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
			this, &PSFGenerationWidget::psfModeChangeRequested);
	connect(this->coeffEditor, &CoefficientEditorWidget::coefficientChanged,
			this, &PSFGenerationWidget::coefficientChanged);
	connect(this->coeffEditor, &CoefficientEditorWidget::resetRequested,
			this, &PSFGenerationWidget::resetRequested);
	connect(this->rwSettingsWidget, &RWSettingsWidget::settingChanged,
			this, &PSFGenerationWidget::rwSettingsChanged);
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
	if (psf.numdims() > 2 && psf.dims(2) > 1) {
		this->psf3DPreview->updatePSF(psf);
	} else {
		this->psfPreview->updateImage(psf);
	}
}

void PSFGenerationWidget::setPSFSettings(const PSFSettings& settings)
{
	this->currentSettings = settings;

	// Determine the mode name for the combo box
	QString modeName;
	if (settings.psfModel == 1) {
		modeName = QStringLiteral("3D PSF Microscopy");
	} else {
		modeName = settings.generatorTypeName;
	}

	// Sync combo box without triggering signal
	this->generatorTypeCombo->blockSignals(true);
	int idx = this->generatorTypeCombo->findText(modeName);
	if (idx >= 0) {
		this->generatorTypeCombo->setCurrentIndex(idx);
	}
	this->generatorTypeCombo->blockSignals(false);

	// Forward aperture settings to wavefront plot
	this->wavefrontPlot->setAperture(settings.apertureGeometry, settings.apertureRadius);

	// Update inline RW settings and visibility
	this->rwSettingsWidget->setSettings(settings.rwSettings);
	this->onPSFModelChanged(settings.psfModel);
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

void PSFGenerationWidget::setPSFMode(const QString& modeName)
{
	this->generatorTypeCombo->blockSignals(true);
	int idx = this->generatorTypeCombo->findText(modeName);
	if (idx >= 0) {
		this->generatorTypeCombo->setCurrentIndex(idx);
	}
	this->generatorTypeCombo->blockSignals(false);
}

void PSFGenerationWidget::onPSFModelChanged(int model)
{
	bool is3D = (model == 1);
	this->rwSettingsWidget->setVisible(is3D);
	this->psfPreviewStack->setCurrentIndex(is3D ? 1 : 0);
}
