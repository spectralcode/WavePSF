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
#include <QPushButton>
#include <QFileDialog>
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

	// File browser widget (visible only in "From File" mode)
	this->fileBrowserWidget = new QWidget(this);
	QVBoxLayout* fbLayout = new QVBoxLayout(this->fileBrowserWidget);
	fbLayout->setContentsMargins(0, 0, 0, 0);
	this->fileSourceLabel = new QLabel(tr("No source selected"), this);
	this->fileSourceLabel->setWordWrap(true);
	fbLayout->addWidget(this->fileSourceLabel);
	QHBoxLayout* fbButtonLayout = new QHBoxLayout();
	QPushButton* browseFolderBtn = new QPushButton(tr("Browse Folder..."), this);
	QPushButton* browseFileBtn = new QPushButton(tr("Browse File..."), this);
	fbButtonLayout->addWidget(browseFolderBtn);
	fbButtonLayout->addWidget(browseFileBtn);
	fbButtonLayout->addStretch();
	fbLayout->addLayout(fbButtonLayout);
	fbLayout->addStretch();
	this->fileBrowserWidget->setVisible(false);
	mainLayout->addWidget(this->fileBrowserWidget);

	connect(browseFolderBtn, &QPushButton::clicked, this, &PSFGenerationWidget::browseForFolder);
	connect(browseFileBtn, &QPushButton::clicked, this, &PSFGenerationWidget::browseForFile);

	// Content: coefficients (left) | wavefront + PSF (right)
	QHBoxLayout* contentLayout = new QHBoxLayout();

	this->coefficientContainer = new QWidget(this);
	QVBoxLayout* coeffLayout = new QVBoxLayout(this->coefficientContainer);
	coeffLayout->setContentsMargins(0, 0, 0, 0);
	this->coeffEditor = new CoefficientEditorWidget(this);
	coeffLayout->addWidget(this->coeffEditor);
	contentLayout->addWidget(this->coefficientContainer, 1);

	// Right column: wavefront plot (top) + PSF preview (bottom), resizable via splitter
	QSplitter* rightSplitter = new QSplitter(Qt::Vertical, this);

	this->wavefrontContainer = new QWidget(this);
	QVBoxLayout* wfLayout = new QVBoxLayout(this->wavefrontContainer);
	wfLayout->setContentsMargins(0, 0, 0, 0);
	this->wavefrontPlot = new WavefrontPlotWidget(this);
	wfLayout->addWidget(new QLabel(tr("Wavefront"), this));
	wfLayout->addWidget(this->wavefrontPlot, 1);
	rightSplitter->addWidget(this->wavefrontContainer);

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
			this, &PSFGenerationWidget::inlineSettingsChanged);
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
	bool isFileMode = (this->currentSettings.generatorTypeName == QStringLiteral("From File"));
	bool is3D = (psf.numdims() > 2 && psf.dims(2) > 1);
	if (isFileMode || is3D) {
		this->psfPreviewStack->setCurrentIndex(1);
		this->psf3DPreview->updatePSF(psf);
	} else {
		this->psfPreviewStack->setCurrentIndex(0);
		this->psfPreview->updateImage(psf);
	}
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

	// Read aperture and RW settings from the active generator's composed settings
	QVariantMap activeSettings = settings.allGeneratorSettings.value(settings.generatorTypeName);
	QVariantMap propSettings = activeSettings.value(QStringLiteral("propagator_settings")).toMap();

	int apertureGeometry = propSettings.value(QStringLiteral("aperture_geometry"), 0).toInt();
	double apertureRadius = propSettings.value(QStringLiteral("aperture_radius"), 1.0).toDouble();
	this->wavefrontPlot->setAperture(apertureGeometry, apertureRadius);

	// Show/hide RW settings widget (preview stack is switched in updatePSF() based on data)
	bool is3D = (settings.generatorTypeName == QStringLiteral("3D PSF Microscopy"));
	this->rwSettingsWidget->setVisible(is3D);
	if (is3D) {
		this->rwSettingsWidget->setSettings(propSettings);
	}

	// Toggle coefficient editor / wavefront vs file browser
	bool isFileMode = (settings.generatorTypeName == QStringLiteral("From File"));
	this->coefficientContainer->setVisible(!isFileMode);
	this->wavefrontContainer->setVisible(!isFileMode);
	this->fileBrowserWidget->setVisible(isFileMode);
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

void PSFGenerationWidget::setCurrentFrame(int frame)
{
	this->psf3DPreview->setFrameIndex(frame);
}

void PSFGenerationWidget::browseForFolder()
{
	QString folder = QFileDialog::getExistingDirectory(this, tr("Select PSF Folder"));
	if (!folder.isEmpty()) {
		this->fileSourceLabel->setText(folder);
		emit filePSFSourceSelected(folder);
	}
}

void PSFGenerationWidget::browseForFile()
{
	QString file = QFileDialog::getOpenFileName(this, tr("Select PSF File"),
		QString(), tr("Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp)"));
	if (!file.isEmpty()) {
		this->fileSourceLabel->setText(file);
		emit filePSFSourceSelected(file);
	}
}
