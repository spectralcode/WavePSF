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
#include <QGridLayout>
#include <cmath>

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

	// Browse buttons at top
	QHBoxLayout* fbButtonLayout = new QHBoxLayout();
	QPushButton* browseFolderBtn = new QPushButton(tr("Open Folder..."), this);
	QPushButton* browseFileBtn = new QPushButton(tr("Open File..."), this);
	fbButtonLayout->addWidget(browseFolderBtn);
	fbButtonLayout->addWidget(browseFileBtn);
	fbButtonLayout->addStretch();
	fbLayout->addLayout(fbButtonLayout);

	// Key-value info grid
	QString keyStyle = QStringLiteral("font-weight: bold;");
	QGridLayout* infoGrid = new QGridLayout();
	infoGrid->setContentsMargins(0, 4, 0, 0);
	infoGrid->setHorizontalSpacing(8);
	infoGrid->setVerticalSpacing(2);

	QLabel* sourceKey = new QLabel(tr("Source:"), this);
	sourceKey->setStyleSheet(keyStyle);
	sourceKey->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	this->fileSourceValue = new QLabel(tr("No source selected"), this);
	this->fileSourceValue->setWordWrap(true);
	infoGrid->addWidget(sourceKey, 0, 0, Qt::AlignTop);
	infoGrid->addWidget(this->fileSourceValue, 0, 1);

	QLabel* typeKey = new QLabel(tr("Type:"), this);
	typeKey->setStyleSheet(keyStyle);
	this->fileTypeValue = new QLabel(this);
	infoGrid->addWidget(typeKey, 1, 0, Qt::AlignTop);
	infoGrid->addWidget(this->fileTypeValue, 1, 1);

	QLabel* filesKey = new QLabel(tr("Files:"), this);
	filesKey->setStyleSheet(keyStyle);
	this->fileFilesValue = new QLabel(this);
	infoGrid->addWidget(filesKey, 2, 0, Qt::AlignTop);
	infoGrid->addWidget(this->fileFilesValue, 2, 1);

	QLabel* rangeKey = new QLabel(tr("Range:"), this);
	rangeKey->setStyleSheet(keyStyle);
	this->fileRangeValue = new QLabel(this);
	this->fileRangeValue->setWordWrap(true);
	infoGrid->addWidget(rangeKey, 3, 0, Qt::AlignTop);
	infoGrid->addWidget(this->fileRangeValue, 3, 1);

	infoGrid->setColumnStretch(1, 1);

	// Wrap info rows in a widget so we can show/hide them together
	this->fileInfoWidget = new QWidget(this);
	this->fileInfoWidget->setLayout(infoGrid);
	this->fileInfoWidget->setVisible(false);
	fbLayout->addWidget(this->fileInfoWidget);

	fbLayout->addStretch();
	this->fileBrowserWidget->setVisible(false);
	mainLayout->addWidget(this->fileBrowserWidget);

	connect(browseFolderBtn, &QPushButton::clicked, this, &PSFGenerationWidget::browseForFolder);
	connect(browseFileBtn, &QPushButton::clicked, this, &PSFGenerationWidget::browseForFile);

	// Content: top row (coefficients + wavefront) | bottom (PSF preview)
	this->contentSplitter = new QSplitter(Qt::Vertical, this);

	// Top row: coefficients (left) + wavefront (right)
	QWidget* topRow = new QWidget(this);
	QHBoxLayout* topRowLayout = new QHBoxLayout(topRow);
	topRowLayout->setContentsMargins(0, 0, 0, 0);

	this->coefficientContainer = new QWidget(this);
	QVBoxLayout* coeffLayout = new QVBoxLayout(this->coefficientContainer);
	coeffLayout->setContentsMargins(0, 0, 0, 0);
	this->coeffEditor = new CoefficientEditorWidget(this);
	coeffLayout->addWidget(this->coeffEditor);
	topRowLayout->addWidget(this->coefficientContainer, 1);

	this->wavefrontContainer = new QWidget(this);
	QVBoxLayout* wfLayout = new QVBoxLayout(this->wavefrontContainer);
	wfLayout->setContentsMargins(0, 0, 0, 0);
	this->wavefrontPlot = new WavefrontPlotWidget(this);
	wfLayout->addWidget(new QLabel(tr("Wavefront"), this));
	wfLayout->addWidget(this->wavefrontPlot, 1);
	topRowLayout->addWidget(this->wavefrontContainer, 2);

	this->contentSplitter->addWidget(topRow);

	// Bottom: PSF preview (stacked widget for 2D/3D modes)
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
	this->contentSplitter->addWidget(psfContainer);

	this->contentSplitter->setStretchFactor(0, 1);
	this->contentSplitter->setStretchFactor(1, 1);
	mainLayout->addWidget(this->contentSplitter, 1);

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
	bool show3D = this->currentSettings.isFileBased || this->currentSettings.is3D
	           || (psf.numdims() > 2 && psf.dims(2) > 1);

	this->psfPreviewStack->setCurrentIndex(show3D ? 1 : 0);

	if (psf.isempty()) {
		this->psfPreview->clearPreview();
		this->psf3DPreview->clearPreview();
		return;
	}

	if (show3D) {
		this->psf3DPreview->updatePSF(psf);
	} else {
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

	this->wavefrontPlot->setAperture(settings.apertureGeometry, settings.apertureRadius);

	// Show/hide inline settings widget (preview stack is switched in updatePSF() based on data)
	this->rwSettingsWidget->setVisible(settings.hasInlineSettings);
	if (settings.hasInlineSettings) {
		this->rwSettingsWidget->setSettings(settings.inlineSettingsValues);
	}

	// Toggle coefficient editor / wavefront vs file browser
	this->coefficientContainer->setVisible(!settings.isFileBased);
	this->wavefrontContainer->setVisible(!settings.isFileBased);
	this->fileBrowserWidget->setVisible(settings.isFileBased);
	if (!settings.isFileBased) {
		this->fileInfoWidget->setVisible(false);
	}
	if (settings.isFileBased) {
		QVariantMap fileSettings = settings.allGeneratorSettings.value(settings.generatorTypeName);
		QString sourcePath = fileSettings.value(QStringLiteral("source_path")).toString();
		this->fileSourceValue->setText(sourcePath.isEmpty() ? tr("No source selected") : sourcePath);
	}
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

void PSFGenerationWidget::setFilePSFInfo(const PSFFileInfo& info)
{
	if (!info.valid) {
		this->fileInfoWidget->setVisible(false);
		return;
	}

	// Source path (updated via browse callbacks and setPSFSettings, but also refresh here)
	// fileSourceValue is already set by browse/setPSFSettings

	// Type: PSF classification and dimensions
	QChar times(0x00D7); // × multiplication sign
	QString typeStr;
	if (info.patchCount == 1 && !info.volumetric) {
		typeStr = QString("Single 2D PSF (%1%2%3)").arg(info.width).arg(times).arg(info.height);
	} else if (info.patchCount == 1 && info.volumetric) {
		typeStr = QString("3D PSF volume (%1%2%3%4%5)")
			.arg(info.width).arg(times).arg(info.height).arg(times).arg(info.depth);
	} else if (info.patchCount > 1 && !info.volumetric) {
		typeStr = QString("2D PSFs (%1 patches, %2%3%4)")
			.arg(info.patchCount).arg(info.width).arg(times).arg(info.height);
	} else {
		typeStr = QString("3D PSFs (%1 patches, %2%3%4%5%6)")
			.arg(info.patchCount).arg(info.width).arg(times).arg(info.height).arg(times).arg(info.depth);
	}
	this->fileTypeValue->setText(typeStr);

	// Files: count, format, bit depth
	QString fileWord = (info.fileCount == 1) ? tr("file") : tr("files");
	QString bitDepthStr = (info.bitDepth > 0) ? QString("%1-bit").arg(info.bitDepth) : tr("mixed bit depth");
	this->fileFilesValue->setText(QString("%1 %2 %3, %4")
		.arg(info.fileCount).arg(info.fileFormat).arg(fileWord).arg(bitDepthStr));

	// Range: value range and normalization
	bool isNormalized = (std::fabs(info.sum - 1.0) < 1e-3) && (info.minValue >= 0.0);
	QString sumStr = isNormalized
		? QString("sum %1 1.000 (normalized)").arg(QChar(0x2248)) // ≈
		: QString("sum = %1").arg(info.sum, 0, 'f', 3);
	this->fileRangeValue->setText(QString("[%1, %2], %3")
		.arg(info.minValue, 0, 'f', 3)
		.arg(info.maxValue, 0, 'f', 3)
		.arg(sumStr));

	this->fileInfoWidget->setVisible(true);
}

void PSFGenerationWidget::browseForFolder()
{
	QString folder = QFileDialog::getExistingDirectory(this, tr("Select PSF Folder"));
	if (!folder.isEmpty()) {
		this->fileSourceValue->setText(folder);
		emit filePSFSourceSelected(folder);
	}
}

void PSFGenerationWidget::browseForFile()
{
	QString file = QFileDialog::getOpenFileName(this, tr("Select PSF File"),
		QString(), tr("Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp)"));
	if (!file.isEmpty()) {
		this->fileSourceValue->setText(file);
		emit filePSFSourceSelected(file);
	}
}
