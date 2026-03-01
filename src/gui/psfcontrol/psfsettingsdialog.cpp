#include "psfsettingsdialog.h"
#include "core/psf/zernikegenerator.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QStackedWidget>
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


PSFSettingsDialog::PSFSettingsDialog(const PSFSettings& settings, QWidget* parent)
	: QDialog(parent)
	, initialSettings(settings)
{
	this->setWindowTitle(tr("Settings"));
	this->setupUI();
	this->populateFromSettings(settings);
	this->updateValidationState();
}

PSFSettings PSFSettingsDialog::getSettings() const
{
	PSFSettings s;
	s.generatorTypeName = this->initialSettings.generatorTypeName;

	if (s.generatorTypeName == QLatin1String("Zernike")) {
		// Zernike settings
		s.nollIndexSpec = this->nollIndicesEdit->text().trimmed();
		s.globalMinCoefficient = this->globalMinSpin->value();
		s.globalMaxCoefficient = this->globalMaxSpin->value();
		s.coefficientStep = this->initialSettings.coefficientStep;

		// Per-Zernike overrides from table
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
					s.coefficientRangeOverrides[nollIndex] = qMakePair(minSpin->value(), maxSpin->value());
				}
			}
		}

		// Serialize Zernike-specific settings into generatorSettings
		QVariantMap gs;
		gs["noll_index_spec"] = s.nollIndexSpec;
		gs["global_min"] = s.globalMinCoefficient;
		gs["global_max"] = s.globalMaxCoefficient;
		gs["step"] = s.coefficientStep;
		QVariantMap overrides;
		for (auto it = s.coefficientRangeOverrides.constBegin();
			 it != s.coefficientRangeOverrides.constEnd(); ++it) {
			QVariantMap range;
			range["min"] = it.value().first;
			range["max"] = it.value().second;
			overrides[QString::number(it.key())] = range;
		}
		gs["range_overrides"] = overrides;
		s.generatorSettings = gs;
	} else {
		// Deformable Mirror settings
		QVariantMap gs;
		gs["actuator_rows"] = this->dmRowsSpin->value();
		gs["actuator_cols"] = this->dmColsSpin->value();
		gs["coupling_coefficient"] = this->dmCouplingSpin->value();
		gs["gaussian_index"] = this->dmGaussianIndexSpin->value();
		gs["command_min"] = this->dmCommandMinSpin->value();
		gs["command_max"] = this->dmCommandMaxSpin->value();
		gs["command_step"] = this->dmCommandStepSpin->value();
		s.generatorSettings = gs;
	}

	// PSF calculation (common)
	s.gridSize = this->gridSizeCombo->currentText().toInt();
	s.wavelengthNm = this->initialSettings.wavelengthNm;
	s.apertureRadius = this->apertureRadiusSpin->value();
	s.normalizationMode = this->normalizationCombo->currentIndex();

	return s;
}

void PSFSettingsDialog::onNollIndicesChanged()
{
	this->updateValidationState();

	QVector<int> indices = parseNollIndexSpec(this->nollIndicesEdit->text());
	if (!indices.isEmpty()) {
		this->rebuildOverrideTable(indices);
	}
}

void PSFSettingsDialog::onApplyClicked()
{
	if (this->validateSettings()) {
		emit settingsApplied(this->getSettings());
	}
}

void PSFSettingsDialog::setupUI()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);

	QTabWidget* tabWidget = new QTabWidget(this);

	// --- Wavefront Generator tab ---
	QWidget* wavefrontTab = new QWidget(tabWidget);
	QVBoxLayout* wavefrontLayout = new QVBoxLayout(wavefrontTab);

	this->generatorStack = new QStackedWidget(wavefrontTab);

	// Page 0: Zernike settings
	QWidget* zernikePage = new QWidget(this->generatorStack);
	QVBoxLayout* zernikeLayout = new QVBoxLayout(zernikePage);

	// Noll indices
	QHBoxLayout* nollRow = new QHBoxLayout();
	nollRow->addWidget(new QLabel(tr("Noll Indices:"), zernikePage));
	this->nollIndicesEdit = new QLineEdit(zernikePage);
	this->nollIndicesEdit->setPlaceholderText(tr("e.g. 2-21 or 1-5, 7, 11"));
	this->nollIndicesEdit->setToolTip(tr("Comma-separated Noll indices and ranges.\nExamples: \"2-21\", \"1-5, 7, 11\", \"4, 11, 15-21\""));
	connect(this->nollIndicesEdit, &QLineEdit::textChanged, this, &PSFSettingsDialog::onNollIndicesChanged);
	nollRow->addWidget(this->nollIndicesEdit);
	zernikeLayout->addLayout(nollRow);

	// Global coefficient range
	QHBoxLayout* rangeRow = new QHBoxLayout();
	rangeRow->addWidget(new QLabel(tr("Global Range:"), zernikePage));
	this->globalMinSpin = new QDoubleSpinBox(zernikePage);
	this->globalMinSpin->setRange(-100.0, 0.0);
	this->globalMinSpin->setDecimals(3);
	this->globalMinSpin->setSingleStep(0.1);
	rangeRow->addWidget(this->globalMinSpin);
	rangeRow->addWidget(new QLabel(tr("to"), zernikePage));
	this->globalMaxSpin = new QDoubleSpinBox(zernikePage);
	this->globalMaxSpin->setRange(0.0, 100.0);
	this->globalMaxSpin->setDecimals(3);
	this->globalMaxSpin->setSingleStep(0.1);
	rangeRow->addWidget(this->globalMaxSpin);
	zernikeLayout->addLayout(rangeRow);

	// Per-Zernike override table
	zernikeLayout->addWidget(new QLabel(tr("Per-Zernike Range Overrides:"), zernikePage));
	this->overrideTable = new QTableWidget(zernikePage);
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

	this->generatorStack->addWidget(zernikePage);

	// Page 1: Deformable Mirror settings
	QWidget* dmPage = new QWidget(this->generatorStack);
	QFormLayout* dmLayout = new QFormLayout(dmPage);

	this->dmRowsSpin = new QSpinBox(dmPage);
	this->dmRowsSpin->setRange(2, 64);
	this->dmRowsSpin->setValue(8);
	this->dmRowsSpin->setToolTip(tr("Number of actuator rows in the grid.\nChanging this resets all actuator coefficients."));
	dmLayout->addRow(tr("Actuator Rows:"), this->dmRowsSpin);

	this->dmColsSpin = new QSpinBox(dmPage);
	this->dmColsSpin->setRange(2, 64);
	this->dmColsSpin->setValue(8);
	this->dmColsSpin->setToolTip(tr("Number of actuator columns in the grid.\nChanging this resets all actuator coefficients."));
	dmLayout->addRow(tr("Actuator Columns:"), this->dmColsSpin);

	this->dmCouplingSpin = new QDoubleSpinBox(dmPage);
	this->dmCouplingSpin->setRange(0.01, 0.50);
	this->dmCouplingSpin->setDecimals(3);
	this->dmCouplingSpin->setSingleStep(0.01);
	this->dmCouplingSpin->setValue(0.15);
	this->dmCouplingSpin->setToolTip(tr(
		"Inter-actuator coupling (typical 0.05-0.15).\n"
		"Fraction of peak influence reaching the adjacent actuator.\n"
		"Higher = smoother wavefront, more overlap between actuators.\n"
		"Lower = sharper, more localized actuator influence."));
	dmLayout->addRow(tr("Coupling Coefficient:"), this->dmCouplingSpin);

	this->dmGaussianIndexSpin = new QDoubleSpinBox(dmPage);
	this->dmGaussianIndexSpin->setRange(1.0, 4.0);
	this->dmGaussianIndexSpin->setDecimals(2);
	this->dmGaussianIndexSpin->setSingleStep(0.1);
	this->dmGaussianIndexSpin->setValue(2.0);
	this->dmGaussianIndexSpin->setToolTip(tr(
		"Shape exponent of the influence function (typical 1.5-2.5).\n"
		"2.0 = standard Gaussian shape.\n"
		"Lower = broader, flatter influence shoulders.\n"
		"Higher = sharper, more peaked influence."));
	dmLayout->addRow(tr("Gaussian Index:"), this->dmGaussianIndexSpin);

	this->dmCommandMinSpin = new QDoubleSpinBox(dmPage);
	this->dmCommandMinSpin->setRange(-100.0, 0.0);
	this->dmCommandMinSpin->setDecimals(3);
	this->dmCommandMinSpin->setSingleStep(0.1);
	this->dmCommandMinSpin->setValue(-1.0);
	this->dmCommandMinSpin->setToolTip(tr("Minimum actuator command value (slider lower bound)."));
	dmLayout->addRow(tr("Command Min:"), this->dmCommandMinSpin);

	this->dmCommandMaxSpin = new QDoubleSpinBox(dmPage);
	this->dmCommandMaxSpin->setRange(0.0, 100.0);
	this->dmCommandMaxSpin->setDecimals(3);
	this->dmCommandMaxSpin->setSingleStep(0.1);
	this->dmCommandMaxSpin->setValue(1.0);
	this->dmCommandMaxSpin->setToolTip(tr("Maximum actuator command value (slider upper bound)."));
	dmLayout->addRow(tr("Command Max:"), this->dmCommandMaxSpin);

	this->dmCommandStepSpin = new QDoubleSpinBox(dmPage);
	this->dmCommandStepSpin->setRange(0.0001, 1.0);
	this->dmCommandStepSpin->setDecimals(4);
	this->dmCommandStepSpin->setSingleStep(0.001);
	this->dmCommandStepSpin->setValue(0.01);
	this->dmCommandStepSpin->setToolTip(tr("Actuator command slider step size."));
	dmLayout->addRow(tr("Command Step:"), this->dmCommandStepSpin);

	// Auto-apply DM settings on any value change for live preview
	connect(this->dmRowsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &PSFSettingsDialog::onApplyClicked);
	connect(this->dmColsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &PSFSettingsDialog::onApplyClicked);
	connect(this->dmCouplingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &PSFSettingsDialog::onApplyClicked);
	connect(this->dmGaussianIndexSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &PSFSettingsDialog::onApplyClicked);
	connect(this->dmCommandMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &PSFSettingsDialog::onApplyClicked);
	connect(this->dmCommandMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &PSFSettingsDialog::onApplyClicked);
	connect(this->dmCommandStepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &PSFSettingsDialog::onApplyClicked);

	this->generatorStack->addWidget(dmPage);

	wavefrontLayout->addWidget(this->generatorStack);
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

	this->normalizationCombo = new QComboBox(psfTab);
	this->normalizationCombo->addItems({tr("Sum"), tr("Peak"), tr("None")});
	psfLayout->addRow(tr("Normalization:"), this->normalizationCombo);

	tabWidget->addTab(psfTab, tr("PSF Calculation"));

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
	// Show correct generator page
	if (settings.generatorTypeName == QLatin1String("Deformable Mirror")) {
		this->generatorStack->setCurrentIndex(1);

		// Populate DM fields
		QVariantMap gs = settings.generatorSettings;
		this->dmRowsSpin->setValue(gs.value("actuator_rows", 8).toInt());
		this->dmColsSpin->setValue(gs.value("actuator_cols", 8).toInt());
		this->dmCouplingSpin->setValue(gs.value("coupling_coefficient", 0.15).toDouble());
		this->dmGaussianIndexSpin->setValue(gs.value("gaussian_index", 2.0).toDouble());
		this->dmCommandMinSpin->setValue(gs.value("command_min", -1.0).toDouble());
		this->dmCommandMaxSpin->setValue(gs.value("command_max", 1.0).toDouble());
		this->dmCommandStepSpin->setValue(gs.value("command_step", 0.01).toDouble());
	} else {
		this->generatorStack->setCurrentIndex(0);

		// Populate Zernike fields
		this->nollIndicesEdit->blockSignals(true);
		this->nollIndicesEdit->setText(settings.nollIndexSpec);
		this->nollIndicesEdit->blockSignals(false);
		this->globalMinSpin->setValue(settings.globalMinCoefficient);
		this->globalMaxSpin->setValue(settings.globalMaxCoefficient);

		// Build override table from Noll indices
		QVector<int> indices = parseNollIndexSpec(settings.nollIndexSpec);
		this->rebuildOverrideTable(indices);

		// Apply stored overrides
		for (int row = 0; row < this->overrideTable->rowCount(); ++row) {
			int nollIndex = this->overrideTable->item(row, 0)->text().toInt();
			if (settings.coefficientRangeOverrides.contains(nollIndex)) {
				QCheckBox* checkBox = qobject_cast<QCheckBox*>(
					this->overrideTable->cellWidget(row, 2));
				QDoubleSpinBox* minSpin = qobject_cast<QDoubleSpinBox*>(
					this->overrideTable->cellWidget(row, 3));
				QDoubleSpinBox* maxSpin = qobject_cast<QDoubleSpinBox*>(
					this->overrideTable->cellWidget(row, 4));
				if (checkBox && minSpin && maxSpin) {
					checkBox->setChecked(true);
					minSpin->setValue(settings.coefficientRangeOverrides[nollIndex].first);
					maxSpin->setValue(settings.coefficientRangeOverrides[nollIndex].second);
					minSpin->setEnabled(true);
					maxSpin->setEnabled(true);
				}
			}
		}
	}

	// Grid size combo
	int gridIdx = this->gridSizeCombo->findText(QString::number(settings.gridSize));
	if (gridIdx >= 0) {
		this->gridSizeCombo->setCurrentIndex(gridIdx);
	}

	this->apertureRadiusSpin->setValue(settings.apertureRadius);
	this->normalizationCombo->setCurrentIndex(settings.normalizationMode);
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
	if (this->initialSettings.generatorTypeName == QLatin1String("Zernike")) {
		QVector<int> indices = parseNollIndexSpec(this->nollIndicesEdit->text());
		return !indices.isEmpty();
	}
	return true; // DM settings always valid (spin boxes enforce ranges)
}

void PSFSettingsDialog::updateValidationState()
{
	bool valid = this->validateSettings();

	// Visual feedback for Zernike mode
	if (this->initialSettings.generatorTypeName == QLatin1String("Zernike")) {
		if (valid) {
			this->nollIndicesEdit->setStyleSheet(QString());
		} else {
			this->nollIndicesEdit->setStyleSheet(QStringLiteral("border: 1px solid red;"));
		}
	}

	this->okButton->setEnabled(valid);
	this->applyButton->setEnabled(valid);
}
