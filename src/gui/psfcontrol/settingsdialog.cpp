#include "settingsdialog.h"
#include "core/psf/zernikegenerator.h"
#include "core/psf/ipsfgenerator.h"
#include "core/psf/psfgeneratorfactory.h"
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
#include <QSet>
#include <algorithm>

namespace {

const QString MODE_3D_MICROSCOPY = QStringLiteral("3D PSF Microscopy");

// Keys already shown inline by RWSettingsWidget — don't duplicate in settings dialog
const QSet<QString> RW_INLINE_KEYS = {
	QStringLiteral("wavelength_nm"), QStringLiteral("numerical_aperture"),
	QStringLiteral("immersion_index"), QStringLiteral("z_step_nm"),
	QStringLiteral("num_z_planes"), QStringLiteral("xy_step_nm")
};

// Flatten composed settings {generator_settings: {...}, propagator_settings: {...}} into a single map
QVariantMap flattenComposedSettings(const QVariantMap& composed)
{
	QVariantMap flat;
	QVariantMap gs = composed.value(QStringLiteral("generator_settings")).toMap();
	QVariantMap ps = composed.value(QStringLiteral("propagator_settings")).toMap();
	for (auto it = gs.constBegin(); it != gs.constEnd(); ++it) flat[it.key()] = it.value();
	for (auto it = ps.constBegin(); it != ps.constEnd(); ++it) flat[it.key()] = it.value();
	return flat;
}

} // namespace


SettingsDialog::SettingsDialog(const PSFSettings& settings,
							   bool autoRange, double displayMin, double displayMax,
							   const QVector<AFBackendInfo>& backends,
							   int activeBackend, int activeDevice,
							   QWidget* parent)
	: QDialog(parent)
	, initialSettings(settings)
	, currentGeneratorTypeName(settings.generatorTypeName)
	, initialActiveBackend(activeBackend)
	, initialActiveDevice(activeDevice)
	, cachedBackends(backends)
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

	this->updateValidationState();

	// Restore original settings on Cancel/close (but keep current generator type)
	connect(this, &QDialog::rejected, this, [this]() {
		PSFSettings revert = this->initialSettings;
		revert.generatorTypeName = this->currentGeneratorTypeName;
		emit settingsApplied(revert);
	});
}

PSFSettings SettingsDialog::getSettings() const
{
	PSFSettings s;
	s.generatorTypeName = this->currentGeneratorTypeName;
	s.gridSize = this->gridSizeCombo->currentText().toInt();

	// Start from initial settings to preserve anything not edited by this dialog
	s.allGeneratorSettings = this->initialSettings.allGeneratorSettings;

	// Read Zernike custom UI settings per mode
	for (auto it = this->zernikeWidgets.constBegin(); it != this->zernikeWidgets.constEnd(); ++it) {
		const QString& modeName = it.key();
		QVariantMap zernikeGs = this->readZernikeSettings(it.value());

		QVariantMap composed = s.allGeneratorSettings.value(modeName);
		composed[QStringLiteral("generator_settings")] = zernikeGs;
		s.allGeneratorSettings[modeName] = composed;
	}

	// Merge descriptor-based widget values for modes that have dialog widgets
	for (auto it = this->generatorSettingWidgets.constBegin();
	     it != this->generatorSettingWidgets.constEnd(); ++it) {
		QVariantMap descriptorComposed = this->readGeneratorSettingsWidgets(it.key());

		// If this mode also has custom Zernike UI, only merge propagator_settings
		// (generator_settings was already set by the Zernike UI loop above)
		if (this->zernikeWidgets.contains(it.key())) {
			QVariantMap existing = s.allGeneratorSettings.value(it.key());
			existing[QStringLiteral("propagator_settings")] =
				descriptorComposed.value(QStringLiteral("propagator_settings"));
			s.allGeneratorSettings[it.key()] = existing;
		} else {
			s.allGeneratorSettings[it.key()] = descriptorComposed;
		}
	}

	return s;
}

bool SettingsDialog::getAutoRange() const
{
	return this->displayAutoRangeCheck->isChecked();
}

double SettingsDialog::getDisplayMin() const
{
	return this->displayMinSpin->value();
}

double SettingsDialog::getDisplayMax() const
{
	return this->displayMaxSpin->value();
}

int SettingsDialog::getSelectedBackend() const
{
	return this->backendCombo->currentData().toInt();
}

int SettingsDialog::getSelectedDeviceId() const
{
	return this->deviceCombo->currentData().toInt();
}

void SettingsDialog::updateGeneratorType(const QString& typeName)
{
	this->currentGeneratorTypeName = typeName;
	this->updateValidationState();
}

void SettingsDialog::onApplyClicked()
{
	if (this->validateSettings()) {
		emit settingsApplied(this->getSettings());
		emit displaySettingsApplied(this->getAutoRange(), this->getDisplayMin(), this->getDisplayMax());
		emit deviceSettingsApplied(this->getSelectedBackend(), this->getSelectedDeviceId());
		this->initialSettings = this->getSettings();
	}
}

void SettingsDialog::setupUI()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setSpacing(3);

	QTabWidget* tabWidget = new QTabWidget(this);

	// --- PSF Generator tab ---
	QWidget* generatorTab = new QWidget(tabWidget);
	QVBoxLayout* generatorLayout = new QVBoxLayout(generatorTab);
	generatorLayout->setSpacing(3);
	generatorLayout->setContentsMargins(3, 3, 3, 3);

	// General PSF Settings groupbox
	QGroupBox* generalGroup = new QGroupBox(tr("General PSF Settings"), generatorTab);
	QHBoxLayout* generalLayout = new QHBoxLayout(generalGroup);
	generalLayout->setSpacing(3);
	QLabel* gridLabel = new QLabel(tr("Grid Size:"), generalGroup);
	gridLabel->setToolTip(tr("Resolution of the computation grid in pixels.\nLarger values produce more detailed PSFs but are slower."));
	generalLayout->addWidget(gridLabel);
	this->gridSizeCombo = new QComboBox(generalGroup);
	this->gridSizeCombo->setToolTip(gridLabel->toolTip());
	this->gridSizeCombo->addItems({QStringLiteral("64"), QStringLiteral("128"), QStringLiteral("256"), QStringLiteral("512"), QStringLiteral("768"), QStringLiteral("1024")});
	generalLayout->addWidget(this->gridSizeCombo);
	generalLayout->addStretch();
	generatorLayout->addWidget(generalGroup);

	// Sub-tabs per generator mode
	QTabWidget* generatorSubTabs = new QTabWidget(generatorTab);

	for (const QString& typeName : PSFGeneratorFactory::availableTypeNames()) {
		if (typeName == QStringLiteral("From File")) continue;
		QWidget* subTabPage = new QWidget(generatorSubTabs);
		QVBoxLayout* subTabLayout = new QVBoxLayout(subTabPage);
		subTabLayout->setSpacing(3);
		subTabLayout->setContentsMargins(3, 3, 3, 3);

		// Add Zernike custom UI to modes that use ZernikeGenerator
		if (typeName == QStringLiteral("Zernike") || typeName == MODE_3D_MICROSCOPY) {
			ZernikeWidgets zw = this->buildZernikeUI(typeName, subTabPage);
			this->zernikeWidgets[typeName] = zw;
			subTabLayout->addWidget(zw.groupBox);
		}

		// Add descriptor-based settings GroupBox
		IPSFGenerator* gen = PSFGeneratorFactory::create(typeName, nullptr);
		QVector<NumericSettingDescriptor> descriptors = gen->getSettingsDescriptors();
		delete gen;

		// For 3D PSF Microscopy, filter out settings already shown inline by RWSettingsWidget
		if (typeName == MODE_3D_MICROSCOPY) {
			descriptors.erase(std::remove_if(descriptors.begin(), descriptors.end(),
				[](const NumericSettingDescriptor& d) { return RW_INLINE_KEYS.contains(d.key); }),
				descriptors.end());
		}

		if (!descriptors.isEmpty()) {
			this->buildGeneratorSettingsGroup(typeName, descriptors, subTabPage);
			subTabLayout->addWidget(this->generatorGroupBoxes.value(typeName));
		}

		subTabLayout->addStretch();
		generatorSubTabs->addTab(subTabPage, typeName);
	}

	generatorLayout->addWidget(generatorSubTabs, 1);

	tabWidget->addTab(generatorTab, tr("PSF Generator"));

	// --- Display tab ---
	QWidget* displayTab = new QWidget(tabWidget);
	QFormLayout* displayLayout = new QFormLayout(displayTab);
	displayLayout->setSpacing(3);

	this->displayAutoRangeCheck = new QCheckBox(tr("Auto Range"), displayTab);
	this->displayAutoRangeCheck->setToolTip(tr("Automatically scale display range to fit the data."));
	displayLayout->addRow(this->displayAutoRangeCheck);

	this->displayMinSpin = new QDoubleSpinBox(displayTab);
	this->displayMinSpin->setRange(-99999.0, 99999.0);
	this->displayMinSpin->setDecimals(2);
	this->displayMinSpin->setToolTip(tr("Minimum display value for PSF visualization."));
	displayLayout->addRow(tr("Min:"), this->displayMinSpin);

	this->displayMaxSpin = new QDoubleSpinBox(displayTab);
	this->displayMaxSpin->setRange(-99999.0, 99999.0);
	this->displayMaxSpin->setDecimals(2);
	this->displayMaxSpin->setToolTip(tr("Maximum display value for PSF visualization."));
	displayLayout->addRow(tr("Max:"), this->displayMaxSpin);

	connect(this->displayAutoRangeCheck, &QCheckBox::toggled, this, [this](bool checked) {
		this->displayMinSpin->setEnabled(!checked);
		this->displayMaxSpin->setEnabled(!checked);
	});

	tabWidget->addTab(displayTab, tr("Display"));

	// --- Device tab ---
	QWidget* miscTab = new QWidget(tabWidget);
	QVBoxLayout* miscLayout = new QVBoxLayout(miscTab);
	miscLayout->setSpacing(3);

	QFormLayout* deviceForm = new QFormLayout();
	deviceForm->setSpacing(3);
	this->backendCombo = new QComboBox(miscTab);
	this->deviceCombo = new QComboBox(miscTab);

	// Populate backend combo from cached data (no AF calls)
	for (const AFBackendInfo& bi : this->cachedBackends) {
		this->backendCombo->addItem(bi.name, bi.backendId);
	}

	deviceForm->addRow(tr("Backend:"), this->backendCombo);
	deviceForm->addRow(tr("Compute Device:"), this->deviceCombo);
	miscLayout->addLayout(deviceForm);

	this->deviceInfoLabel = new QLabel(miscTab);
	this->deviceInfoLabel->setWordWrap(true);
	this->deviceInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	this->deviceInfoLabel->setFrameShape(QFrame::StyledPanel);
	this->deviceInfoLabel->setStyleSheet(QStringLiteral("QLabel { padding: 8px; }"));
	miscLayout->addWidget(this->deviceInfoLabel);
	miscLayout->addStretch();

	// Repopulate device combo from cached data when backend changes
	auto repopulateDevices = [this]() {
		this->deviceCombo->blockSignals(true);
		this->deviceCombo->clear();
		int backendId = this->backendCombo->currentData().toInt();
		for (const AFBackendInfo& bi : this->cachedBackends) {
			if (bi.backendId != backendId) continue;
			for (const AFDeviceInfo& di : bi.devices) {
				QString displayName = QStringLiteral("[%1] %2").arg(di.deviceId).arg(di.name);
				this->deviceCombo->addItem(displayName);
				int idx = this->deviceCombo->count() - 1;
				this->deviceCombo->setItemData(idx, di.deviceId, Qt::UserRole);
			QString toolkitLabel = (backendId == 1) ? tr("Compiler") : tr("Driver");
			QString info = QStringLiteral("Name: %1\nPlatform: %2\nCompute: %3\n%4: %5")
					.arg(di.name, di.platform, di.compute, toolkitLabel, di.toolkit);
				this->deviceCombo->setItemData(idx, info, Qt::UserRole + 1);
			}
			break;
		}
		this->deviceCombo->blockSignals(false);
		if (this->deviceCombo->count() > 0) {
			this->deviceCombo->setCurrentIndex(0);
			this->deviceInfoLabel->setText(
				this->deviceCombo->itemData(0, Qt::UserRole + 1).toString());
		} else {
			this->deviceInfoLabel->setText(tr("No devices available for this backend."));
		}
	};

	connect(this->backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, repopulateDevices);
	connect(this->deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, [this](int index) {
		if (index >= 0) {
			this->deviceInfoLabel->setText(
				this->deviceCombo->itemData(index, Qt::UserRole + 1).toString());
		}
	});

	// Set active backend
	for (int i = 0; i < this->backendCombo->count(); ++i) {
		if (this->backendCombo->itemData(i).toInt() == this->initialActiveBackend) {
			this->backendCombo->setCurrentIndex(i);
			break;
		}
	}
	// Explicitly populate devices (setCurrentIndex is a no-op if already at index 0)
	repopulateDevices();
	// Pre-select the currently active device
	for (int i = 0; i < this->deviceCombo->count(); ++i) {
		if (this->deviceCombo->itemData(i).toInt() == this->initialActiveDevice) {
			this->deviceCombo->setCurrentIndex(i);
			break;
		}
	}

	tabWidget->addTab(miscTab, tr("Device"));

	mainLayout->addWidget(tabWidget);

	// --- Button box ---
	QDialogButtonBox* buttonBox = new QDialogButtonBox(this);
	this->okButton = buttonBox->addButton(QDialogButtonBox::Ok);
	buttonBox->addButton(QDialogButtonBox::Cancel);
	this->applyButton = buttonBox->addButton(QDialogButtonBox::Apply);

	connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(this->applyButton, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);

	mainLayout->addWidget(buttonBox);

	this->resize(500, 600);
}

SettingsDialog::ZernikeWidgets SettingsDialog::buildZernikeUI(const QString& modeName, QWidget* parent)
{
	ZernikeWidgets zw;

	zw.groupBox = new QGroupBox(tr("Zernike Wavefront"), parent);
	QVBoxLayout* layout = new QVBoxLayout(zw.groupBox);
	layout->setSpacing(3);

	QHBoxLayout* nollRow = new QHBoxLayout();
	nollRow->setSpacing(3);
	QLabel* nollLabel = new QLabel(tr("Noll Indices:"), zw.groupBox);
	nollLabel->setToolTip(tr("Comma-separated Noll indices and ranges.\n"
	                         "Examples: \"2-21\", \"1-5, 7, 11\", \"4, 11, 15-21\""));
	nollRow->addWidget(nollLabel);
	zw.nollIndicesEdit = new QLineEdit(zw.groupBox);
	zw.nollIndicesEdit->setPlaceholderText(tr("e.g. 2-21 or 1-5, 7, 11"));
	zw.nollIndicesEdit->setToolTip(nollLabel->toolTip());
	connect(zw.nollIndicesEdit, &QLineEdit::textChanged, this, [this, modeName]() {
		this->updateValidationState();
		auto it = this->zernikeWidgets.find(modeName);
		if (it == this->zernikeWidgets.end()) return;
		QVector<int> indices = ZernikeGenerator::parseNollIndexSpec(it->nollIndicesEdit->text());
		if (!indices.isEmpty()) {
			this->rebuildOverrideTable(*it, indices);
		}
	});
	nollRow->addWidget(zw.nollIndicesEdit);
	layout->addLayout(nollRow);

	QHBoxLayout* rangeRow = new QHBoxLayout();
	rangeRow->setSpacing(3);
	QLabel* rangeLabel = new QLabel(tr("Global Range:"), zw.groupBox);
	rangeLabel->setToolTip(tr("Default coefficient range for all Zernike terms.\nSlider min/max in the coefficient editor."));
	rangeRow->addWidget(rangeLabel);
	zw.globalMinSpin = new QDoubleSpinBox(zw.groupBox);
	zw.globalMinSpin->setRange(-100.0, 0.0);
	zw.globalMinSpin->setDecimals(3);
	zw.globalMinSpin->setSingleStep(0.1);
	zw.globalMinSpin->setToolTip(tr("Minimum coefficient value for all Zernike terms."));
	rangeRow->addWidget(zw.globalMinSpin);
	rangeRow->addWidget(new QLabel(tr("to"), zw.groupBox));
	zw.globalMaxSpin = new QDoubleSpinBox(zw.groupBox);
	zw.globalMaxSpin->setRange(0.0, 100.0);
	zw.globalMaxSpin->setDecimals(3);
	zw.globalMaxSpin->setSingleStep(0.1);
	zw.globalMaxSpin->setToolTip(tr("Maximum coefficient value for all Zernike terms."));
	rangeRow->addWidget(zw.globalMaxSpin);
	layout->addLayout(rangeRow);

	QLabel* overrideLabel = new QLabel(tr("Per-Zernike Range Overrides:"), zw.groupBox);
	overrideLabel->setToolTip(tr("Override the global range for individual Zernike terms.\nUseful when certain aberrations need wider or narrower ranges."));
	layout->addWidget(overrideLabel);
	zw.overrideTable = new QTableWidget(zw.groupBox);
	zw.overrideTable->setColumnCount(5);
	zw.overrideTable->setHorizontalHeaderLabels({tr("Noll#"), tr("Name"), tr("Override"), tr("Min"), tr("Max")});
	zw.overrideTable->horizontalHeader()->setStretchLastSection(true);
	zw.overrideTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	zw.overrideTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	zw.overrideTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	zw.overrideTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	zw.overrideTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
	zw.overrideTable->setSelectionMode(QAbstractItemView::NoSelection);
	layout->addWidget(zw.overrideTable);

	return zw;
}

void SettingsDialog::rebuildOverrideTable(ZernikeWidgets& zw, const QVector<int>& indices)
{
	// Save existing overrides before rebuild
	QMap<int, QPair<double,double>> existingOverrides;
	QSet<int> checkedIndices;
	for (int row = 0; row < zw.overrideTable->rowCount(); ++row) {
		int nollIndex = zw.overrideTable->item(row, 0)->text().toInt();
		QCheckBox* checkBox = qobject_cast<QCheckBox*>(
			zw.overrideTable->cellWidget(row, 2));
		QDoubleSpinBox* minSpin = qobject_cast<QDoubleSpinBox*>(
			zw.overrideTable->cellWidget(row, 3));
		QDoubleSpinBox* maxSpin = qobject_cast<QDoubleSpinBox*>(
			zw.overrideTable->cellWidget(row, 4));
		if (checkBox && checkBox->isChecked() && minSpin && maxSpin) {
			checkedIndices.insert(nollIndex);
			existingOverrides[nollIndex] = qMakePair(minSpin->value(), maxSpin->value());
		}
	}

	// Clear old rows (removes cell widgets properly)
	zw.overrideTable->setRowCount(0);
	zw.overrideTable->setRowCount(indices.size());

	for (int i = 0; i < indices.size(); ++i) {
		int nollIndex = indices[i];

		// Noll# (read-only)
		QTableWidgetItem* nollItem = new QTableWidgetItem(QString::number(nollIndex));
		nollItem->setFlags(nollItem->flags() & ~Qt::ItemIsEditable);
		zw.overrideTable->setItem(i, 0, nollItem);

		// Name (read-only)
		QTableWidgetItem* nameItem = new QTableWidgetItem(ZernikeGenerator::getName(nollIndex));
		nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
		zw.overrideTable->setItem(i, 1, nameItem);

		// Override checkbox
		QCheckBox* checkBox = new QCheckBox(zw.overrideTable);
		zw.overrideTable->setCellWidget(i, 2, checkBox);

		// Min spinbox
		QDoubleSpinBox* minSpin = new QDoubleSpinBox(zw.overrideTable);
		minSpin->setRange(-100.0, 100.0);
		minSpin->setDecimals(3);
		minSpin->setSingleStep(0.1);
		minSpin->setValue(zw.globalMinSpin->value());
		minSpin->setEnabled(false);
		zw.overrideTable->setCellWidget(i, 3, minSpin);

		// Max spinbox
		QDoubleSpinBox* maxSpin = new QDoubleSpinBox(zw.overrideTable);
		maxSpin->setRange(-100.0, 100.0);
		maxSpin->setDecimals(3);
		maxSpin->setSingleStep(0.1);
		maxSpin->setValue(zw.globalMaxSpin->value());
		maxSpin->setEnabled(false);
		zw.overrideTable->setCellWidget(i, 4, maxSpin);

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

QVariantMap SettingsDialog::readZernikeSettings(const ZernikeWidgets& zw) const
{
	QVariantMap zernikeGs;
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_NOLL_INDEX_SPEC)] = zw.nollIndicesEdit->text().trimmed();
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_GLOBAL_MIN)] = zw.globalMinSpin->value();
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_GLOBAL_MAX)] = zw.globalMaxSpin->value();

	// Preserve step from initial settings (no UI widget for it)
	// Use Zernike mode's initial settings as default, fall back to 0.1
	QVariantMap initialZernikeGs = flattenComposedSettings(
		this->initialSettings.allGeneratorSettings.value(QStringLiteral("Zernike")));
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_STEP)] =
		initialZernikeGs.value(QLatin1String(ZernikeGenerator::KEY_STEP), 0.1);

	QVariantMap overrides;
	for (int row = 0; row < zw.overrideTable->rowCount(); ++row) {
		QCheckBox* checkBox = qobject_cast<QCheckBox*>(
			zw.overrideTable->cellWidget(row, 2));
		if (checkBox && checkBox->isChecked()) {
			int nollIndex = zw.overrideTable->item(row, 0)->text().toInt();
			QDoubleSpinBox* minSpin = qobject_cast<QDoubleSpinBox*>(
				zw.overrideTable->cellWidget(row, 3));
			QDoubleSpinBox* maxSpin = qobject_cast<QDoubleSpinBox*>(
				zw.overrideTable->cellWidget(row, 4));
			if (minSpin && maxSpin) {
				QVariantMap range;
				range[QLatin1String(ZernikeGenerator::KEY_RANGE_MIN)] = minSpin->value();
				range[QLatin1String(ZernikeGenerator::KEY_RANGE_MAX)] = maxSpin->value();
				overrides[QString::number(nollIndex)] = range;
			}
		}
	}
	zernikeGs[QLatin1String(ZernikeGenerator::KEY_RANGE_OVERRIDES)] = overrides;

	return zernikeGs;
}

void SettingsDialog::populateZernikeWidgets(ZernikeWidgets& zw, const QVariantMap& zernikeGs)
{
	QString nollSpec = zernikeGs.value(QLatin1String(ZernikeGenerator::KEY_NOLL_INDEX_SPEC)).toString();
	zw.nollIndicesEdit->blockSignals(true);
	zw.nollIndicesEdit->setText(nollSpec);
	zw.nollIndicesEdit->blockSignals(false);
	zw.globalMinSpin->setValue(zernikeGs.value(QLatin1String(ZernikeGenerator::KEY_GLOBAL_MIN)).toDouble());
	zw.globalMaxSpin->setValue(zernikeGs.value(QLatin1String(ZernikeGenerator::KEY_GLOBAL_MAX)).toDouble());

	QVector<int> indices = ZernikeGenerator::parseNollIndexSpec(nollSpec);
	this->rebuildOverrideTable(zw, indices);

	QVariantMap overrides = zernikeGs.value(QLatin1String(ZernikeGenerator::KEY_RANGE_OVERRIDES)).toMap();
	for (int row = 0; row < zw.overrideTable->rowCount(); ++row) {
		int nollIndex = zw.overrideTable->item(row, 0)->text().toInt();
		QString key = QString::number(nollIndex);
		if (overrides.contains(key)) {
			QVariantMap range = overrides[key].toMap();
			QCheckBox* checkBox = qobject_cast<QCheckBox*>(
				zw.overrideTable->cellWidget(row, 2));
			QDoubleSpinBox* minSpin = qobject_cast<QDoubleSpinBox*>(
				zw.overrideTable->cellWidget(row, 3));
			QDoubleSpinBox* maxSpin = qobject_cast<QDoubleSpinBox*>(
				zw.overrideTable->cellWidget(row, 4));
			if (checkBox && minSpin && maxSpin) {
				checkBox->setChecked(true);
				minSpin->setValue(range[QLatin1String(ZernikeGenerator::KEY_RANGE_MIN)].toDouble());
				maxSpin->setValue(range[QLatin1String(ZernikeGenerator::KEY_RANGE_MAX)].toDouble());
				minSpin->setEnabled(true);
				maxSpin->setEnabled(true);
			}
		}
	}
}

void SettingsDialog::populateFromSettings(const PSFSettings& settings)
{
	QMap<QString, QVariantMap> allGenSettings = settings.allGeneratorSettings;

	// Populate per-mode Zernike widgets
	for (auto it = this->zernikeWidgets.begin(); it != this->zernikeWidgets.end(); ++it) {
		QVariantMap composed = allGenSettings.value(it.key());
		QVariantMap zernikeGs = composed.value(QStringLiteral("generator_settings")).toMap();
		this->populateZernikeWidgets(it.value(), zernikeGs);
	}

	// Populate descriptor-based generator settings (flatten composed settings for widget population)
	for (auto it = this->generatorSettingWidgets.constBegin();
	     it != this->generatorSettingWidgets.constEnd(); ++it) {
		this->populateGeneratorSettingsWidgets(it.key(), allGenSettings.value(it.key()));
	}

	// Grid size
	int gridIdx = this->gridSizeCombo->findText(QString::number(settings.gridSize));
	if (gridIdx >= 0) {
		this->gridSizeCombo->setCurrentIndex(gridIdx);
	}
}

void SettingsDialog::buildGeneratorSettingsGroup(const QString& typeName,
                                                     const QVector<NumericSettingDescriptor>& descriptors,
                                                     QWidget* parent)
{
	QGroupBox* group = new QGroupBox(tr("Propagator Settings"), parent);
	QFormLayout* layout = new QFormLayout(group);
	layout->setSpacing(3);

	auto emitSettings = [this]() { emit settingsApplied(this->getSettings()); };
	QMap<QString, QWidget*> widgets;

	for (const NumericSettingDescriptor& desc : descriptors) {
		QLabel* label = new QLabel(desc.name + QStringLiteral(":"), group);
		if (!desc.tooltip.isEmpty()) label->setToolTip(desc.tooltip);

		if (!desc.options.isEmpty()) {
			// Render as QComboBox when options are provided
			QComboBox* combo = new QComboBox(group);
			combo->addItems(desc.options);
			combo->setCurrentIndex(static_cast<int>(desc.defaultValue));
			if (!desc.tooltip.isEmpty()) combo->setToolTip(desc.tooltip);
			layout->addRow(label, combo);
			connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, emitSettings);
			widgets[desc.key] = combo;
		} else if (desc.decimals == 0) {
			QSpinBox* spin = new QSpinBox(group);
			spin->setRange(static_cast<int>(desc.minValue), static_cast<int>(desc.maxValue));
			spin->setSingleStep(static_cast<int>(desc.step));
			spin->setValue(static_cast<int>(desc.defaultValue));
			if (!desc.tooltip.isEmpty()) spin->setToolTip(desc.tooltip);
			layout->addRow(label, spin);
			connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, emitSettings);
			widgets[desc.key] = spin;
		} else {
			QDoubleSpinBox* spin = new QDoubleSpinBox(group);
			spin->setRange(desc.minValue, desc.maxValue);
			spin->setDecimals(desc.decimals);
			spin->setSingleStep(desc.step);
			spin->setValue(desc.defaultValue);
			if (!desc.tooltip.isEmpty()) spin->setToolTip(desc.tooltip);
			layout->addRow(label, spin);
			connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, emitSettings);
			widgets[desc.key] = spin;
		}
	}

	this->generatorGroupBoxes[typeName] = group;
	this->generatorSettingWidgets[typeName] = widgets;
}

QVariantMap SettingsDialog::readGeneratorSettingsWidgets(const QString& typeName) const
{
	// Read flat values from widgets
	QVariantMap flat;
	const QMap<QString, QWidget*>& widgets = this->generatorSettingWidgets.value(typeName);
	for (auto it = widgets.constBegin(); it != widgets.constEnd(); ++it) {
		QComboBox* combo = qobject_cast<QComboBox*>(it.value());
		if (combo) { flat[it.key()] = combo->currentIndex(); continue; }
		QDoubleSpinBox* dSpin = qobject_cast<QDoubleSpinBox*>(it.value());
		if (dSpin) { flat[it.key()] = dSpin->value(); continue; }
		QSpinBox* iSpin = qobject_cast<QSpinBox*>(it.value());
		if (iSpin) { flat[it.key()] = iSpin->value(); }
	}

	// Round-trip through a temp generator to split flat map into composed format.
	// Each component only reads the keys it knows, so passing flat as both
	// generator_settings and propagator_settings is safe.
	IPSFGenerator* temp = PSFGeneratorFactory::create(typeName, nullptr);
	QVariantMap composed;
	composed[QStringLiteral("generator_settings")] = flat;
	composed[QStringLiteral("propagator_settings")] = flat;
	temp->deserializeSettings(composed);
	QVariantMap result = temp->serializeSettings();
	delete temp;
	return result;
}

void SettingsDialog::populateGeneratorSettingsWidgets(const QString& typeName, const QVariantMap& composedSettings)
{
	// Flatten composed settings for widget population
	QVariantMap flat = flattenComposedSettings(composedSettings);

	const QMap<QString, QWidget*>& widgets = this->generatorSettingWidgets.value(typeName);
	for (auto it = widgets.constBegin(); it != widgets.constEnd(); ++it) {
		if (!flat.contains(it.key())) continue;
		QComboBox* combo = qobject_cast<QComboBox*>(it.value());
		if (combo) { combo->setCurrentIndex(flat[it.key()].toInt()); continue; }
		QDoubleSpinBox* dSpin = qobject_cast<QDoubleSpinBox*>(it.value());
		if (dSpin) { dSpin->setValue(flat[it.key()].toDouble()); continue; }
		QSpinBox* iSpin = qobject_cast<QSpinBox*>(it.value());
		if (iSpin) { iSpin->setValue(flat[it.key()].toInt()); }
	}
}

bool SettingsDialog::validateSettings() const
{
	// Validate all Zernike-based modes
	for (auto it = this->zernikeWidgets.constBegin(); it != this->zernikeWidgets.constEnd(); ++it) {
		if (this->currentGeneratorTypeName == it.key()) {
			QVector<int> indices = ZernikeGenerator::parseNollIndexSpec(it->nollIndicesEdit->text());
			if (indices.isEmpty()) return false;
		}
	}
	return true;
}

void SettingsDialog::updateValidationState()
{
	bool valid = this->validateSettings();

	// Visual feedback for Zernike-based modes
	for (auto it = this->zernikeWidgets.constBegin(); it != this->zernikeWidgets.constEnd(); ++it) {
		if (this->currentGeneratorTypeName == it.key()) {
			QVector<int> indices = ZernikeGenerator::parseNollIndexSpec(it->nollIndicesEdit->text());
			if (!indices.isEmpty()) {
				it->nollIndicesEdit->setStyleSheet(QString());
			} else {
				it->nollIndicesEdit->setStyleSheet(QStringLiteral("border: 1px solid red;"));
			}
		}
	}

	this->okButton->setEnabled(valid);
	this->applyButton->setEnabled(valid);
}
