#include "rwsettingswidget.h"
#include "core/psf/richardswolfcalculator.h"
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSpinBox>


RWSettingsWidget::RWSettingsWidget(QWidget* parent)
	: QWidget(parent)
{
	QFormLayout* layout = new QFormLayout(this);
	layout->setContentsMargins(3, 3, 3, 3);
	layout->setVerticalSpacing(3);

	// Build spinboxes from RichardsWolfCalculator descriptors
	RichardsWolfCalculator rwCalc;
	QVector<NumericSettingDescriptor> descriptors = rwCalc.getSettingsDescriptors();

	for (const NumericSettingDescriptor& desc : descriptors) {
		// Skip settings configured in the settings dialog
		if (desc.key == QStringLiteral("phase_scale")) {
			continue;
		}
		if (desc.decimals == 0) {
			QSpinBox* spin = new QSpinBox(this);
			spin->setKeyboardTracking(false);
			spin->setRange(static_cast<int>(desc.minValue), static_cast<int>(desc.maxValue));
			spin->setSingleStep(static_cast<int>(desc.step));
			spin->setValue(static_cast<int>(desc.defaultValue));
			if (!desc.tooltip.isEmpty()) spin->setToolTip(desc.tooltip);
			layout->addRow(desc.name + QStringLiteral(":"), spin);
			connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
					this, &RWSettingsWidget::emitSettingChanged);
			this->spinBoxes[desc.key] = spin;
		} else {
			QDoubleSpinBox* spin = new QDoubleSpinBox(this);
			spin->setKeyboardTracking(false);
			spin->setRange(desc.minValue, desc.maxValue);
			spin->setDecimals(desc.decimals);
			spin->setSingleStep(desc.step);
			spin->setValue(desc.defaultValue);
			if (!desc.tooltip.isEmpty()) spin->setToolTip(desc.tooltip);
			layout->addRow(desc.name + QStringLiteral(":"), spin);
			connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
					this, &RWSettingsWidget::emitSettingChanged);
			this->spinBoxes[desc.key] = spin;
		}
	}

	// XY scaling mode (Fast = interpolation only, Exact = zero-padded FFT)
	this->scalingCombo = new QComboBox(this);
	this->scalingCombo->addItem(tr("Fast (Interpolation)"));
	this->scalingCombo->addItem(tr("Exact (Zero-Pad FFT)"));
	this->scalingCombo->setToolTip(tr("Fast: always native FFT size, resamples with interpolation.\n"
	                                  "Exact: zero-pads FFT for finer-than-native spacing."));
	layout->addRow(tr("XY Scaling:"), this->scalingCombo);

	connect(this->scalingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
	        this, &RWSettingsWidget::emitSettingChanged);

	// Polarization mode (custom UI, not auto-generated from descriptors)
	this->polarizationCombo = new QComboBox(this);
	this->polarizationCombo->addItem(tr("Unpolarized"));
	this->polarizationCombo->addItem(tr("Linear"));
	layout->addRow(tr("Polarization:"), this->polarizationCombo);

	this->polAngleLabel = new QLabel(tr("Pol. Angle (deg):"), this);
	this->polAngleSpinBox = new QDoubleSpinBox(this);
	this->polAngleSpinBox->setRange(0.0, 180.0);
	this->polAngleSpinBox->setDecimals(1);
	this->polAngleSpinBox->setSingleStep(1.0);
	this->polAngleSpinBox->setValue(0.0);
	this->polAngleSpinBox->setToolTip(tr("Polarization angle in degrees (0 = x-axis)"));
	layout->addRow(this->polAngleLabel, this->polAngleSpinBox);

	this->updatePolarizationVisibility(0);

	connect(this->polarizationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
	        this, &RWSettingsWidget::updatePolarizationVisibility);
	connect(this->polarizationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
	        this, &RWSettingsWidget::emitSettingChanged);
	connect(this->polAngleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
	        this, &RWSettingsWidget::emitSettingChanged);
}

QVariantMap RWSettingsWidget::getSettings() const
{
	QVariantMap map;
	for (auto it = this->spinBoxes.constBegin(); it != this->spinBoxes.constEnd(); ++it) {
		QDoubleSpinBox* dSpin = qobject_cast<QDoubleSpinBox*>(it.value());
		if (dSpin) { map[it.key()] = dSpin->value(); continue; }
		QSpinBox* iSpin = qobject_cast<QSpinBox*>(it.value());
		if (iSpin) { map[it.key()] = iSpin->value(); }
	}
	map[QStringLiteral("scaling_mode")] = this->scalingCombo->currentIndex();
	map[QStringLiteral("polarization_mode")] = this->polarizationCombo->currentIndex();
	map[QStringLiteral("polarization_angle")] = this->polAngleSpinBox->value();
	return map;
}

void RWSettingsWidget::setSettings(const QVariantMap& settings)
{
	for (auto it = this->spinBoxes.constBegin(); it != this->spinBoxes.constEnd(); ++it) {
		if (!settings.contains(it.key())) continue;
		it.value()->blockSignals(true);
		QDoubleSpinBox* dSpin = qobject_cast<QDoubleSpinBox*>(it.value());
		if (dSpin) { dSpin->setValue(settings[it.key()].toDouble()); }
		QSpinBox* iSpin = qobject_cast<QSpinBox*>(it.value());
		if (iSpin) { iSpin->setValue(settings[it.key()].toInt()); }
		it.value()->blockSignals(false);
	}
	if (settings.contains(QStringLiteral("scaling_mode"))) {
		this->scalingCombo->blockSignals(true);
		this->scalingCombo->setCurrentIndex(
			settings[QStringLiteral("scaling_mode")].toInt());
		this->scalingCombo->blockSignals(false);
	}
	if (settings.contains(QStringLiteral("polarization_mode"))) {
		this->polarizationCombo->blockSignals(true);
		this->polarizationCombo->setCurrentIndex(
			settings[QStringLiteral("polarization_mode")].toInt());
		this->polarizationCombo->blockSignals(false);
		this->updatePolarizationVisibility(this->polarizationCombo->currentIndex());
	}
	if (settings.contains(QStringLiteral("polarization_angle"))) {
		this->polAngleSpinBox->blockSignals(true);
		this->polAngleSpinBox->setValue(
			settings[QStringLiteral("polarization_angle")].toDouble());
		this->polAngleSpinBox->blockSignals(false);
	}
}

void RWSettingsWidget::updatePolarizationVisibility(int modeIndex)
{
	bool isLinear = (modeIndex == 1);
	this->polAngleLabel->setEnabled(isLinear);
	this->polAngleSpinBox->setEnabled(isLinear);
}

void RWSettingsWidget::emitSettingChanged()
{
	emit settingChanged(this->getSettings());
}
