#include "psfsettingsdialog.h"
#include "core/psf/zernikegenerator.h"
#include "core/psf/wavefrontgeneratorfactory.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>


PSFSettingsDialog::PSFSettingsDialog(const PSFSettings& settings,
								   bool autoRange, double displayMin, double displayMax,
								   bool syncViewsEnabled,
								   QWidget* parent)
	: QDialog(parent)
	, initialSettings(settings)
	, currentGeneratorTypeName(settings.generatorTypeName)
{
	this->setWindowTitle(tr("Settings"));
	this->setupUI();
	this->populateFromSettings(settings);

	// Populate display settings
	this->displayAutoRangeCheck->setChecked(autoRange);
	this->displayMinSpin->setValue(displayMin);
	this->displayMaxSpin->setValue(displayMax);
	this->displayMinSpin->setEnabled(!autoRange);
	this->displayMaxSpin->setEnabled(!autoRange);
	this->syncViewsCheck->setChecked(syncViewsEnabled);

	this->updateValidationState();

	// Live PSF update when changing aperture radius or padding factor
	connect(this->apertureRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this]() { emit settingsApplied(this->getSettings()); });
	connect(this->paddingFactorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, [this]() { emit settingsApplied(this->getSettings()); });
	connect(this->apertureGeometryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, [this]() { emit settingsApplied(this->getSettings()); });

	// Restore original settings on Cancel/close (but keep current generator type)
	connect(this, &QDialog::rejected, this, [this]() {
		PSFSettings revert = this->initialSettings;
		revert.generatorTypeName = this->currentGeneratorTypeName;
		emit settingsApplied(revert);
	});
}

PSFSettings PSFSettingsDialog::getSettings() const
{
	PSFSettings s;
	s.generatorTypeName = this->currentGeneratorTypeName;

	// Capture Zernike settings from custom UI
	QVariantMap zernikeGs;
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_NOLL_INDEX_SPEC)] = this->nollIndicesEdit->text().trimmed();
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_GLOBAL_MIN)] = this->globalMinSpin->value();
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_GLOBAL_MAX)] = this->globalMaxSpin->value();
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_STEP)] = this->initialSettings.coefficientStep;
	QVariantMap overrides;
	for (int row = 0; row < this->overrideTable->rowCount(); ++row) {
		QCheckBox* checkBox = qobject_cast<QCheckBox*>(
			this->overrideTable->cellWidget(row, 2));
		if (checkBox && checkBox->isChecked()) {
			int nollIndex = this->overrideTable->item(row, 0)->text().toInt();
			QDoubleSpinBox* minSpin = qobject_cast<QDoubleSpinBox*>(
				this->overrideTable->cellWidget(row, 3));
			QDoubleSpinBox* maxSpin = qobject_cast<QDoubleSpinBox*>(
				this->overrideTable->cellWidget(row, 4));
			if (minSpin && maxSpin) {
				QVariantMap range;
				range[QLatin1String(ZernikeGenerator::KEY_RANGE_MIN)] = minSpin->value();
				range[QLatin1String(ZernikeGenerator::KEY_RANGE_MAX)] = maxSpin->value();
				overrides[QString::number(nollIndex)] = range;
				s.coefficientRangeOverrides[nollIndex] = qMakePair(minSpin->value(), maxSpin->value());
			}
		}
	}
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_RANGE_OVERRIDES)] = overrides;
	s.allGeneratorSettings[QStringLiteral("Zernike")] = zernikeGs;

	// Capture descriptor-based generator settings from auto-generated widgets
	for (auto it = this->generatorSettingWidgets.constBegin();
	     it != this->generatorSettingWidgets.constEnd(); ++it) {
		s.allGeneratorSettings[it.key()] = this->readGeneratorSettingsWidgets(it.key());
	}

	// Active generator's generatorSettings is the matching entry
	s.generatorSettings = s.allGeneratorSettings.value(s.generatorTypeName);

	// Zernike convenience fields (for backward compat with PSFModule)
	if (s.generatorTypeName == QLatin1String("Zernike")) {
		s.nollIndexSpec = zernikeGs[QLatin1String(ZernikeGenerator::KEY_NOLL_INDEX_SPEC)].toString();
		s.globalMinCoefficient = zernikeGs[QLatin1String(ZernikeGenerator::KEY_GLOBAL_MIN)].toDouble();
		s.globalMaxCoefficient = zernikeGs[QLatin1String(ZernikeGenerator::KEY_GLOBAL_MAX)].toDouble();
		s.coefficientStep = this->initialSettings.coefficientStep;
	}

	// PSF calculation (common)
	s.gridSize = this->gridSizeCombo->currentText().toInt();
	s.wavelengthNm = this->initialSettings.wavelengthNm;
	s.apertureRadius = this->apertureRadiusSpin->value();
	s.apertureGeometry = this->apertureGeometryCombo->currentIndex();
	s.normalizationMode = this->normalizationCombo->currentIndex();
	s.paddingFactor = this->paddingFactorCombo->currentText().split(" ").first().toInt();

	return s;
}

void PSFSettingsDialog::onNollIndicesChanged()
{
	this->updateValidationState();

	QVector<int> indices = ZernikeGenerator::parseNollIndexSpec(this->nollIndicesEdit->text());
	if (!indices.isEmpty()) {
		this->rebuildOverrideTable(indices);
	}
}

bool PSFSettingsDialog::getAutoRange() const
{
	return this->displayAutoRangeCheck->isChecked();
}

double PSFSettingsDialog::getDisplayMin() const
{
	return this->displayMinSpin->value();
}

double PSFSettingsDialog::getDisplayMax() const
{
	return this->displayMaxSpin->value();
}

void PSFSettingsDialog::updateGeneratorType(const QString& typeName)
{
	this->currentGeneratorTypeName = typeName;
	this->updateValidationState();
}

void PSFSettingsDialog::onApplyClicked()
{
	if (this->validateSettings()) {
		emit settingsApplied(this->getSettings());
		emit displaySettingsApplied(this->getAutoRange(), this->getDisplayMin(), this->getDisplayMax());
		this->initialSettings = this->getSettings();
	}
}

void PSFSettingsDialog::setupUI()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);

	QTabWidget* tabWidget = new QTabWidget(this);

	// --- Wavefront Generator tab ---
	QWidget* wavefrontTab = new QWidget(tabWidget);
	QVBoxLayout* wavefrontLayout = new QVBoxLayout(wavefrontTab);

	// Zernike Generator GroupBox
	this->zernikeGroupBox = new QGroupBox(tr("Zernike Generator"), wavefrontTab);
	QVBoxLayout* zernikeLayout = new QVBoxLayout(this->zernikeGroupBox);

	QHBoxLayout* nollRow = new QHBoxLayout();
	nollRow->addWidget(new QLabel(tr("Noll Indices:"), this->zernikeGroupBox));
	this->nollIndicesEdit = new QLineEdit(this->zernikeGroupBox);
	this->nollIndicesEdit->setPlaceholderText(tr("e.g. 2-21 or 1-5, 7, 11"));
	this->nollIndicesEdit->setToolTip(tr("Comma-separated Noll indices and ranges.\nExamples: \"2-21\", \"1-5, 7, 11\", \"4, 11, 15-21\""));
	connect(this->nollIndicesEdit, &QLineEdit::textChanged, this, &PSFSettingsDialog::onNollIndicesChanged);
	nollRow->addWidget(this->nollIndicesEdit);
	zernikeLayout->addLayout(nollRow);

	QHBoxLayout* rangeRow = new QHBoxLayout();
	rangeRow->addWidget(new QLabel(tr("Global Range:"), this->zernikeGroupBox));
	this->globalMinSpin = new QDoubleSpinBox(this->zernikeGroupBox);
	this->globalMinSpin->setRange(-100.0, 0.0);
	this->globalMinSpin->setDecimals(3);
	this->globalMinSpin->setSingleStep(0.1);
	rangeRow->addWidget(this->globalMinSpin);
	rangeRow->addWidget(new QLabel(tr("to"), this->zernikeGroupBox));
	this->globalMaxSpin = new QDoubleSpinBox(this->zernikeGroupBox);
	this->globalMaxSpin->setRange(0.0, 100.0);
	this->globalMaxSpin->setDecimals(3);
	this->globalMaxSpin->setSingleStep(0.1);
	rangeRow->addWidget(this->globalMaxSpin);
	zernikeLayout->addLayout(rangeRow);

	zernikeLayout->addWidget(new QLabel(tr("Per-Zernike Range Overrides:"), this->zernikeGroupBox));
	this->overrideTable = new QTableWidget(this->zernikeGroupBox);
	this->overrideTable->setColumnCount(5);
	this->overrideTable->setHorizontalHeaderLabels({tr("Noll#"), tr("Name"), tr("Override"), tr("Min"), tr("Max")});
	this->overrideTable->horizontalHeader()->setStretchLastSection(true);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
	this->overrideTable->setSelectionMode(QAbstractItemView::NoSelection);
	zernikeLayout->addWidget(this->overrideTable);

	wavefrontLayout->addWidget(this->zernikeGroupBox);

	// Auto-generate GroupBoxes for all descriptor-based generators
	for (const QString& typeName : WavefrontGeneratorFactory::availableTypeNames()) {
		IWavefrontGenerator* gen = WavefrontGeneratorFactory::create(typeName, nullptr);
		QVector<WavefrontGeneratorSetting> descriptors = gen->getSettingsDescriptors();
		delete gen;
		if (descriptors.isEmpty()) continue;
		this->buildGeneratorSettingsGroup(typeName, descriptors, wavefrontTab);
		wavefrontLayout->addWidget(this->generatorGroupBoxes.value(typeName));
	}

	tabWidget->addTab(wavefrontTab, tr("Wavefront Generator"));

	// --- PSF Calculation tab ---
	QWidget* psfTab = new QWidget(tabWidget);
	QFormLayout* psfLayout = new QFormLayout(psfTab);

	this->gridSizeCombo = new QComboBox(psfTab);
	this->gridSizeCombo->addItems({QStringLiteral("64"), QStringLiteral("128"), QStringLiteral("256"), QStringLiteral("512")});
	psfLayout->addRow(tr("Grid Size:"), this->gridSizeCombo);

	this->apertureRadiusSpin = new QDoubleSpinBox(psfTab);
	this->apertureRadiusSpin->setRange(0.01, 1.0);
	this->apertureRadiusSpin->setDecimals(3);
	this->apertureRadiusSpin->setSingleStep(0.01);
	psfLayout->addRow(tr("Aperture Radius:"), this->apertureRadiusSpin);

	this->apertureGeometryCombo = new QComboBox(psfTab);
	this->apertureGeometryCombo->addItems({tr("Circle"), tr("Rectangle"), tr("Triangle")});
	psfLayout->addRow(tr("Aperture Geometry:"), this->apertureGeometryCombo);

	this->normalizationCombo = new QComboBox(psfTab);
	this->normalizationCombo->addItems({tr("Sum"), tr("Peak"), tr("None")});
	psfLayout->addRow(tr("Normalization:"), this->normalizationCombo);

	this->paddingFactorCombo = new QComboBox(psfTab);
	this->paddingFactorCombo->addItems({QStringLiteral("1 (None)"), QStringLiteral("2"), QStringLiteral("4"), QStringLiteral("8")});
	this->paddingFactorCombo->setToolTip(tr(
		"Zero-padding factor for the FFT.\n"
		"Higher values produce smoother PSFs by oversampling\n"
		"the diffraction pattern before cropping back to grid size.\n"
		"Increases computation time."));
	psfLayout->addRow(tr("Padding Factor:"), this->paddingFactorCombo);

	tabWidget->addTab(psfTab, tr("PSF Calculation"));

	// --- Display tab ---
	QWidget* displayTab = new QWidget(tabWidget);
	QFormLayout* displayLayout = new QFormLayout(displayTab);

	this->displayAutoRangeCheck = new QCheckBox(tr("Auto Range"), displayTab);
	displayLayout->addRow(this->displayAutoRangeCheck);

	this->displayMinSpin = new QDoubleSpinBox(displayTab);
	this->displayMinSpin->setRange(-99999.0, 99999.0);
	this->displayMinSpin->setDecimals(2);
	displayLayout->addRow(tr("Min:"), this->displayMinSpin);

	this->displayMaxSpin = new QDoubleSpinBox(displayTab);
	this->displayMaxSpin->setRange(-99999.0, 99999.0);
	this->displayMaxSpin->setDecimals(2);
	displayLayout->addRow(tr("Max:"), this->displayMaxSpin);

	this->syncViewsCheck = new QCheckBox(tr("Synchronize view navigation"), displayTab);
	displayLayout->addRow(this->syncViewsCheck);

	connect(this->displayAutoRangeCheck, &QCheckBox::toggled, this, [this](bool checked) {
		this->displayMinSpin->setEnabled(!checked);
		this->displayMaxSpin->setEnabled(!checked);
	});
	connect(this->syncViewsCheck, &QCheckBox::toggled, this, &PSFSettingsDialog::viewSyncChanged);

	tabWidget->addTab(displayTab, tr("Display"));

	mainLayout->addWidget(tabWidget);

	// --- Button box ---
	QDialogButtonBox* buttonBox = new QDialogButtonBox(this);
	this->okButton = buttonBox->addButton(QDialogButtonBox::Ok);
	buttonBox->addButton(QDialogButtonBox::Cancel);
	this->applyButton = buttonBox->addButton(QDialogButtonBox::Apply);

	connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(this->applyButton, &QPushButton::clicked, this, &PSFSettingsDialog::onApplyClicked);

	mainLayout->addWidget(buttonBox);

	this->resize(500, 600);
}

void PSFSettingsDialog::populateFromSettings(const PSFSettings& settings)
{
	// Use allGeneratorSettings from PSFSettings; fall back to active generatorSettings for backward compat
	QMap<QString, QVariantMap> allGenSettings = settings.allGeneratorSettings;
	if (allGenSettings.isEmpty() && !settings.generatorSettings.isEmpty()) {
		allGenSettings[settings.generatorTypeName] = settings.generatorSettings;
	}

	// Populate Zernike fields from the Zernike-specific settings map
	QVariantMap zernikeGs = allGenSettings.value(QStringLiteral("Zernike"));
	QString nollSpec = zernikeGs.value(QLatin1String(ZernikeGenerator::KEY_NOLL_INDEX_SPEC)).toString();
	this->nollIndicesEdit->blockSignals(true);
	this->nollIndicesEdit->setText(nollSpec);
	this->nollIndicesEdit->blockSignals(false);
	this->globalMinSpin->setValue(zernikeGs.value(QLatin1String(ZernikeGenerator::KEY_GLOBAL_MIN)).toDouble());
	this->globalMaxSpin->setValue(zernikeGs.value(QLatin1String(ZernikeGenerator::KEY_GLOBAL_MAX)).toDouble());

	QVector<int> indices = ZernikeGenerator::parseNollIndexSpec(nollSpec);
	this->rebuildOverrideTable(indices);

	QVariantMap overrides = zernikeGs.value(QLatin1String(ZernikeGenerator::KEY_RANGE_OVERRIDES)).toMap();
	for (int row = 0; row < this->overrideTable->rowCount(); ++row) {
		int nollIndex = this->overrideTable->item(row, 0)->text().toInt();
		QString key = QString::number(nollIndex);
		if (overrides.contains(key)) {
			QVariantMap range = overrides[key].toMap();
			QCheckBox* checkBox = qobject_cast<QCheckBox*>(
				this->overrideTable->cellWidget(row, 2));
			QDoubleSpinBox* minSpin = qobject_cast<QDoubleSpinBox*>(
				this->overrideTable->cellWidget(row, 3));
			QDoubleSpinBox* maxSpin = qobject_cast<QDoubleSpinBox*>(
				this->overrideTable->cellWidget(row, 4));
			if (checkBox && minSpin && maxSpin) {
				checkBox->setChecked(true);
				minSpin->setValue(range[QLatin1String(ZernikeGenerator::KEY_RANGE_MIN)].toDouble());
				maxSpin->setValue(range[QLatin1String(ZernikeGenerator::KEY_RANGE_MAX)].toDouble());
				minSpin->setEnabled(true);
				maxSpin->setEnabled(true);
			}
		}
	}

	// Populate descriptor-based generator settings from auto-generated widgets
	for (auto it = this->generatorSettingWidgets.constBegin();
	     it != this->generatorSettingWidgets.constEnd(); ++it) {
		this->populateGeneratorSettingsWidgets(it.key(), allGenSettings.value(it.key()));
	}

	// Common PSF calculation fields
	int gridIdx = this->gridSizeCombo->findText(QString::number(settings.gridSize));
	if (gridIdx >= 0) {
		this->gridSizeCombo->setCurrentIndex(gridIdx);
	}

	this->apertureRadiusSpin->setValue(settings.apertureRadius);
	this->apertureGeometryCombo->setCurrentIndex(settings.apertureGeometry);
	this->normalizationCombo->setCurrentIndex(settings.normalizationMode);

	// Padding factor combo: find matching item by numeric prefix
	QString paddingStr = QString::number(settings.paddingFactor);
	for (int i = 0; i < this->paddingFactorCombo->count(); ++i) {
		if (this->paddingFactorCombo->itemText(i).startsWith(paddingStr)) {
			this->paddingFactorCombo->setCurrentIndex(i);
			break;
		}
	}
}

void PSFSettingsDialog::rebuildOverrideTable(const QVector<int>& indices)
{
	// Save existing overrides before rebuild
	QMap<int, QPair<double,double>> existingOverrides;
	QSet<int> checkedIndices;
	for (int row = 0; row < this->overrideTable->rowCount(); ++row) {
		int nollIndex = this->overrideTable->item(row, 0)->text().toInt();
		QCheckBox* checkBox = qobject_cast<QCheckBox*>(
			this->overrideTable->cellWidget(row, 2));
		QDoubleSpinBox* minSpin = qobject_cast<QDoubleSpinBox*>(
			this->overrideTable->cellWidget(row, 3));
		QDoubleSpinBox* maxSpin = qobject_cast<QDoubleSpinBox*>(
			this->overrideTable->cellWidget(row, 4));
		if (checkBox && checkBox->isChecked() && minSpin && maxSpin) {
			checkedIndices.insert(nollIndex);
			existingOverrides[nollIndex] = qMakePair(minSpin->value(), maxSpin->value());
		}
	}

	// Clear old rows (removes cell widgets properly)
	this->overrideTable->setRowCount(0);
	this->overrideTable->setRowCount(indices.size());

	for (int i = 0; i < indices.size(); ++i) {
		int nollIndex = indices[i];

		// Noll# (read-only)
		QTableWidgetItem* nollItem = new QTableWidgetItem(QString::number(nollIndex));
		nollItem->setFlags(nollItem->flags() & ~Qt::ItemIsEditable);
		this->overrideTable->setItem(i, 0, nollItem);

		// Name (read-only)
		QTableWidgetItem* nameItem = new QTableWidgetItem(ZernikeGenerator::getName(nollIndex));
		nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
		this->overrideTable->setItem(i, 1, nameItem);

		// Override checkbox
		QCheckBox* checkBox = new QCheckBox(this->overrideTable);
		this->overrideTable->setCellWidget(i, 2, checkBox);

		// Min spinbox
		QDoubleSpinBox* minSpin = new QDoubleSpinBox(this->overrideTable);
		minSpin->setRange(-100.0, 100.0);
		minSpin->setDecimals(3);
		minSpin->setSingleStep(0.1);
		minSpin->setValue(this->globalMinSpin->value());
		minSpin->setEnabled(false);
		this->overrideTable->setCellWidget(i, 3, minSpin);

		// Max spinbox
		QDoubleSpinBox* maxSpin = new QDoubleSpinBox(this->overrideTable);
		maxSpin->setRange(-100.0, 100.0);
		maxSpin->setDecimals(3);
		maxSpin->setSingleStep(0.1);
		maxSpin->setValue(this->globalMaxSpin->value());
		maxSpin->setEnabled(false);
		this->overrideTable->setCellWidget(i, 4, maxSpin);

		// Enable/disable min/max when checkbox toggled
		connect(checkBox, &QCheckBox::toggled, minSpin, &QDoubleSpinBox::setEnabled);
		connect(checkBox, &QCheckBox::toggled, maxSpin, &QDoubleSpinBox::setEnabled);

		// Restore existing override if present
		if (checkedIndices.contains(nollIndex) && existingOverrides.contains(nollIndex)) {
			checkBox->setChecked(true);
			minSpin->setValue(existingOverrides[nollIndex].first);
			maxSpin->setValue(existingOverrides[nollIndex].second);
		}
	}
}

void PSFSettingsDialog::buildGeneratorSettingsGroup(const QString& typeName,
                                                     const QVector<WavefrontGeneratorSetting>& descriptors,
                                                     QWidget* parent)
{
	QGroupBox* group = new QGroupBox(typeName, parent);
	QFormLayout* layout = new QFormLayout(group);

	auto emitSettings = [this]() { emit settingsApplied(this->getSettings()); };
	QMap<QString, QWidget*> widgets;

	for (const WavefrontGeneratorSetting& desc : descriptors) {
		if (desc.decimals == 0) {
			QSpinBox* spin = new QSpinBox(group);
			spin->setRange(static_cast<int>(desc.minValue), static_cast<int>(desc.maxValue));
			spin->setSingleStep(static_cast<int>(desc.step));
			spin->setValue(static_cast<int>(desc.defaultValue));
			if (!desc.tooltip.isEmpty()) spin->setToolTip(desc.tooltip);
			layout->addRow(desc.name + QStringLiteral(":"), spin);
			connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, emitSettings);
			widgets[desc.key] = spin;
		} else {
			QDoubleSpinBox* spin = new QDoubleSpinBox(group);
			spin->setRange(desc.minValue, desc.maxValue);
			spin->setDecimals(desc.decimals);
			spin->setSingleStep(desc.step);
			spin->setValue(desc.defaultValue);
			if (!desc.tooltip.isEmpty()) spin->setToolTip(desc.tooltip);
			layout->addRow(desc.name + QStringLiteral(":"), spin);
			connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, emitSettings);
			widgets[desc.key] = spin;
		}
	}

	this->generatorGroupBoxes[typeName] = group;
	this->generatorSettingWidgets[typeName] = widgets;
}

QVariantMap PSFSettingsDialog::readGeneratorSettingsWidgets(const QString& typeName) const
{
	QVariantMap map;
	const QMap<QString, QWidget*>& widgets = this->generatorSettingWidgets.value(typeName);
	for (auto it = widgets.constBegin(); it != widgets.constEnd(); ++it) {
		QDoubleSpinBox* dSpin = qobject_cast<QDoubleSpinBox*>(it.value());
		if (dSpin) { map[it.key()] = dSpin->value(); continue; }
		QSpinBox* iSpin = qobject_cast<QSpinBox*>(it.value());
		if (iSpin) { map[it.key()] = iSpin->value(); }
	}
	return map;
}

void PSFSettingsDialog::populateGeneratorSettingsWidgets(const QString& typeName, const QVariantMap& gs)
{
	const QMap<QString, QWidget*>& widgets = this->generatorSettingWidgets.value(typeName);
	for (auto it = widgets.constBegin(); it != widgets.constEnd(); ++it) {
		if (!gs.contains(it.key())) continue;
		QDoubleSpinBox* dSpin = qobject_cast<QDoubleSpinBox*>(it.value());
		if (dSpin) { dSpin->setValue(gs[it.key()].toDouble()); continue; }
		QSpinBox* iSpin = qobject_cast<QSpinBox*>(it.value());
		if (iSpin) { iSpin->setValue(gs[it.key()].toInt()); }
	}
}

bool PSFSettingsDialog::validateSettings() const
{
	if (this->currentGeneratorTypeName == QLatin1String("Zernike")) {
		QVector<int> indices = ZernikeGenerator::parseNollIndexSpec(this->nollIndicesEdit->text());
		return !indices.isEmpty();
	}
	return true; // descriptor-based generators always valid (spin boxes enforce ranges)
}

void PSFSettingsDialog::updateValidationState()
{
	bool valid = this->validateSettings();

	// Visual feedback for Zernike mode
	if (this->currentGeneratorTypeName == QLatin1String("Zernike")) {
		if (valid) {
			this->nollIndicesEdit->setStyleSheet(QString());
		} else {
			this->nollIndicesEdit->setStyleSheet(QStringLiteral("border: 1px solid red;"));
		}
	}

	this->okButton->setEnabled(valid);
	this->applyButton->setEnabled(valid);
}
