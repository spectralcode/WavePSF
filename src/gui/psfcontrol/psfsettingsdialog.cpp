#include "psfsettingsdialog.h"
#include "core/psf/zernikegenerator.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
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

	// Wavefront generator
	s.nollIndexSpec = this->nollIndicesEdit->text().trimmed();
	s.globalMinCoefficient = this->globalMinSpin->value();
	s.globalMaxCoefficient = this->globalMaxSpin->value();
	s.coefficientStep = this->initialSettings.coefficientStep; // controlled by CoefficientEditorWidget

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

	// PSF calculation
	s.gridSize = this->gridSizeCombo->currentText().toInt();
	s.wavelengthNm = this->initialSettings.wavelengthNm; // not exposed in UI
	s.apertureRadius = this->apertureRadiusSpin->value();
	s.normalizationMode = this->normalizationCombo->currentIndex();

	return s;
}

void PSFSettingsDialog::onNollIndicesChanged()
{
	this->updateValidationState();

	if (this->validateNollSpec()) {
		QVector<int> indices = parseNollIndexSpec(this->nollIndicesEdit->text());
		this->rebuildOverrideTable(indices);
	}
}

void PSFSettingsDialog::onApplyClicked()
{
	if (this->validateNollSpec()) {
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

	// Noll indices
	QHBoxLayout* nollRow = new QHBoxLayout();
	nollRow->addWidget(new QLabel(tr("Noll Indices:"), wavefrontTab));
	this->nollIndicesEdit = new QLineEdit(wavefrontTab);
	this->nollIndicesEdit->setPlaceholderText(tr("e.g. 2-21 or 1-5, 7, 11"));
	this->nollIndicesEdit->setToolTip(tr("Comma-separated Noll indices and ranges.\nExamples: \"2-21\", \"1-5, 7, 11\", \"4, 11, 15-21\""));
	connect(this->nollIndicesEdit, &QLineEdit::textChanged, this, &PSFSettingsDialog::onNollIndicesChanged);
	nollRow->addWidget(this->nollIndicesEdit);
	wavefrontLayout->addLayout(nollRow);

	// Global coefficient range
	QHBoxLayout* rangeRow = new QHBoxLayout();
	rangeRow->addWidget(new QLabel(tr("Global Range:"), wavefrontTab));
	this->globalMinSpin = new QDoubleSpinBox(wavefrontTab);
	this->globalMinSpin->setRange(-100.0, 0.0);
	this->globalMinSpin->setDecimals(3);
	this->globalMinSpin->setSingleStep(0.1);
	rangeRow->addWidget(this->globalMinSpin);
	rangeRow->addWidget(new QLabel(tr("to"), wavefrontTab));
	this->globalMaxSpin = new QDoubleSpinBox(wavefrontTab);
	this->globalMaxSpin->setRange(0.0, 100.0);
	this->globalMaxSpin->setDecimals(3);
	this->globalMaxSpin->setSingleStep(0.1);
	rangeRow->addWidget(this->globalMaxSpin);
	wavefrontLayout->addLayout(rangeRow);

	// Per-Zernike override table
	wavefrontLayout->addWidget(new QLabel(tr("Per-Zernike Range Overrides:"), wavefrontTab));
	this->overrideTable = new QTableWidget(wavefrontTab);
	this->overrideTable->setColumnCount(5);
	this->overrideTable->setHorizontalHeaderLabels({tr("Noll#"), tr("Name"), tr("Override"), tr("Min"), tr("Max")});
	this->overrideTable->horizontalHeader()->setStretchLastSection(true);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	this->overrideTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
	this->overrideTable->setSelectionMode(QAbstractItemView::NoSelection);
	wavefrontLayout->addWidget(this->overrideTable);

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
	// Block signals to prevent rebuildOverrideTable being triggered by textChanged
	this->nollIndicesEdit->blockSignals(true);
	this->nollIndicesEdit->setText(settings.nollIndexSpec);
	this->nollIndicesEdit->blockSignals(false);
	this->globalMinSpin->setValue(settings.globalMinCoefficient);
	this->globalMaxSpin->setValue(settings.globalMaxCoefficient);
	// Grid size combo
	int gridIdx = this->gridSizeCombo->findText(QString::number(settings.gridSize));
	if (gridIdx >= 0) {
		this->gridSizeCombo->setCurrentIndex(gridIdx);
	}

	this->apertureRadiusSpin->setValue(settings.apertureRadius);
	this->normalizationCombo->setCurrentIndex(settings.normalizationMode);

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

bool PSFSettingsDialog::validateNollSpec() const
{
	QVector<int> indices = parseNollIndexSpec(this->nollIndicesEdit->text());
	return !indices.isEmpty();
}

void PSFSettingsDialog::updateValidationState()
{
	bool valid = this->validateNollSpec();

	// Visual feedback
	if (valid) {
		this->nollIndicesEdit->setStyleSheet(QString());
	} else {
		this->nollIndicesEdit->setStyleSheet(QStringLiteral("border: 1px solid red;"));
	}

	this->okButton->setEnabled(valid);
	this->applyButton->setEnabled(valid);
}
