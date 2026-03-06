#include "psfsettingsdialog.h"
#include "core/psf/zernikegenerator.h"
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

	// Always capture settings from BOTH generator GroupBoxes
	QVariantMap zernikeGs;
	zernikeGs["noll_index_spec"] = this->nollIndicesEdit->text().trimmed();
	zernikeGs["global_min"] = this->globalMinSpin->value();
	zernikeGs["global_max"] = this->globalMaxSpin->value();
	zernikeGs["step"] = this->initialSettings.coefficientStep;
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
				range["min"] = minSpin->value();
				range["max"] = maxSpin->value();
				overrides[QString::number(nollIndex)] = range;
				s.coefficientRangeOverrides[nollIndex] = qMakePair(minSpin->value(), maxSpin->value());
			}
		}
	}
	zernikeGs["range_overrides"] = overrides;
	s.allGeneratorSettings[QStringLiteral("Zernike")] = zernikeGs;

	QVariantMap dmGs;
	dmGs["actuator_rows"] = this->dmRowsSpin->value();
	dmGs["actuator_cols"] = this->dmColsSpin->value();
	dmGs["coupling_coefficient"] = this->dmCouplingSpin->value();
	dmGs["gaussian_index"] = this->dmGaussianIndexSpin->value();
	dmGs["command_min"] = this->dmCommandMinSpin->value();
	dmGs["command_max"] = this->dmCommandMaxSpin->value();
	dmGs["command_step"] = this->dmCommandStepSpin->value();
	s.allGeneratorSettings[QStringLiteral("Deformable Mirror")] = dmGs;

	// Active generator's generatorSettings is the matching entry
	s.generatorSettings = s.allGeneratorSettings.value(s.generatorTypeName);

	// Zernike convenience fields (for backward compat with PSFModule)
	if (s.generatorTypeName == QLatin1String("Zernike")) {
		s.nollIndexSpec = zernikeGs["noll_index_spec"].toString();
		s.globalMinCoefficient = zernikeGs["global_min"].toDouble();
		s.globalMaxCoefficient = zernikeGs["global_max"].toDouble();
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

	// DM Simulator GroupBox
	this->dmGroupBox = new QGroupBox(tr("DM Simulator"), wavefrontTab);
	QFormLayout* dmLayout = new QFormLayout(this->dmGroupBox);

	this->dmRowsSpin = new QSpinBox(this->dmGroupBox);
	this->dmRowsSpin->setRange(2, 64);
	this->dmRowsSpin->setValue(8);
	this->dmRowsSpin->setToolTip(tr("Number of actuator rows in the grid.\nChanging this resets all actuator coefficients."));
	dmLayout->addRow(tr("Actuator Rows:"), this->dmRowsSpin);

	this->dmColsSpin = new QSpinBox(this->dmGroupBox);
	this->dmColsSpin->setRange(2, 64);
	this->dmColsSpin->setValue(8);
	this->dmColsSpin->setToolTip(tr("Number of actuator columns in the grid.\nChanging this resets all actuator coefficients."));
	dmLayout->addRow(tr("Actuator Columns:"), this->dmColsSpin);

	this->dmCouplingSpin = new QDoubleSpinBox(this->dmGroupBox);
	this->dmCouplingSpin->setRange(0.01, 1.00);
	this->dmCouplingSpin->setDecimals(3);
	this->dmCouplingSpin->setSingleStep(0.01);
	this->dmCouplingSpin->setValue(0.25);
	this->dmCouplingSpin->setToolTip(tr(
		"Inter-actuator coupling (typical 0.05-0.30).\n"
		"Fraction of peak influence reaching the adjacent actuator.\n"
		"Higher = smoother wavefront, more overlap between actuators.\n"
		"Lower = sharper, more localized actuator influence."));
	dmLayout->addRow(tr("Coupling Coefficient:"), this->dmCouplingSpin);

	this->dmGaussianIndexSpin = new QDoubleSpinBox(this->dmGroupBox);
	this->dmGaussianIndexSpin->setRange(0.01, 10.0);
	this->dmGaussianIndexSpin->setDecimals(2);
	this->dmGaussianIndexSpin->setSingleStep(0.1);
	this->dmGaussianIndexSpin->setValue(1.5);
	this->dmGaussianIndexSpin->setToolTip(tr(
		"Shape exponent of the influence function (typical 1.5-2.5).\n"
		"2.0 = standard Gaussian shape.\n"
		"< 1.0 = very broad.\n"
		"Higher = sharper."));
	dmLayout->addRow(tr("Gaussian Index:"), this->dmGaussianIndexSpin);

	this->dmCommandMinSpin = new QDoubleSpinBox(this->dmGroupBox);
	this->dmCommandMinSpin->setRange(-100.0, 0.0);
	this->dmCommandMinSpin->setDecimals(3);
	this->dmCommandMinSpin->setSingleStep(0.1);
	this->dmCommandMinSpin->setValue(-1.0);
	this->dmCommandMinSpin->setToolTip(tr("Minimum actuator command value (slider lower bound)."));
	dmLayout->addRow(tr("Command Min:"), this->dmCommandMinSpin);

	this->dmCommandMaxSpin = new QDoubleSpinBox(this->dmGroupBox);
	this->dmCommandMaxSpin->setRange(0.0, 100.0);
	this->dmCommandMaxSpin->setDecimals(3);
	this->dmCommandMaxSpin->setSingleStep(0.1);
	this->dmCommandMaxSpin->setValue(1.0);
	this->dmCommandMaxSpin->setToolTip(tr("Maximum actuator command value (slider upper bound)."));
	dmLayout->addRow(tr("Command Max:"), this->dmCommandMaxSpin);

	this->dmCommandStepSpin = new QDoubleSpinBox(this->dmGroupBox);
	this->dmCommandStepSpin->setRange(0.0001, 1.0);
	this->dmCommandStepSpin->setDecimals(4);
	this->dmCommandStepSpin->setSingleStep(0.001);
	this->dmCommandStepSpin->setValue(0.01);
	this->dmCommandStepSpin->setToolTip(tr("Actuator command slider step size."));
	dmLayout->addRow(tr("Command Step:"), this->dmCommandStepSpin);

	// Auto-apply DM settings on any value change for live preview
	auto emitSettings = [this]() { emit settingsApplied(this->getSettings()); };
	connect(this->dmRowsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, emitSettings);
	connect(this->dmColsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, emitSettings);
	connect(this->dmCouplingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, emitSettings);
	connect(this->dmGaussianIndexSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, emitSettings);
	connect(this->dmCommandMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, emitSettings);
	connect(this->dmCommandMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, emitSettings);
	connect(this->dmCommandStepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, emitSettings);

	wavefrontLayout->addWidget(this->dmGroupBox);

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
	QString nollSpec = zernikeGs.value("noll_index_spec").toString();
	this->nollIndicesEdit->blockSignals(true);
	this->nollIndicesEdit->setText(nollSpec);
	this->nollIndicesEdit->blockSignals(false);
	this->globalMinSpin->setValue(zernikeGs.value("global_min").toDouble());
	this->globalMaxSpin->setValue(zernikeGs.value("global_max").toDouble());

	QVector<int> indices = ZernikeGenerator::parseNollIndexSpec(nollSpec);
	this->rebuildOverrideTable(indices);

	QVariantMap overrides = zernikeGs.value("range_overrides").toMap();
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
				minSpin->setValue(range["min"].toDouble());
				maxSpin->setValue(range["max"].toDouble());
				minSpin->setEnabled(true);
				maxSpin->setEnabled(true);
			}
		}
	}

	// Populate DM fields from the DM-specific settings map
	QVariantMap dmGs = allGenSettings.value(QStringLiteral("Deformable Mirror"));
	this->dmRowsSpin->setValue(dmGs.value("actuator_rows").toInt());
	this->dmColsSpin->setValue(dmGs.value("actuator_cols").toInt());
	this->dmCouplingSpin->setValue(dmGs.value("coupling_coefficient").toDouble());
	this->dmGaussianIndexSpin->setValue(dmGs.value("gaussian_index").toDouble());
	this->dmCommandMinSpin->setValue(dmGs.value("command_min").toDouble());
	this->dmCommandMaxSpin->setValue(dmGs.value("command_max").toDouble());
	this->dmCommandStepSpin->setValue(dmGs.value("command_step").toDouble());

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

bool PSFSettingsDialog::validateSettings() const
{
	if (this->currentGeneratorTypeName == QLatin1String("Zernike")) {
		QVector<int> indices = ZernikeGenerator::parseNollIndexSpec(this->nollIndicesEdit->text());
		return !indices.isEmpty();
	}
	return true; // DM settings always valid (spin boxes enforce ranges)
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
