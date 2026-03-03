#ifndef PSFSETTINGSDIALOG_H
#define PSFSETTINGSDIALOG_H

#include <QDialog>
#include "core/psf/psfsettings.h"

class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QTableWidget;
class QCheckBox;
class QPushButton;
class QGroupBox;

class PSFSettingsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit PSFSettingsDialog(const PSFSettings& settings,
							   bool autoRange, double displayMin, double displayMax,
							   QWidget* parent = nullptr);

	PSFSettings getSettings() const;
	bool getAutoRange() const;
	double getDisplayMin() const;
	double getDisplayMax() const;

signals:
	void settingsApplied(PSFSettings settings);
	void displaySettingsApplied(bool autoRange, double min, double max);

private slots:
	void onNollIndicesChanged();
	void onApplyClicked();

private:
	void setupUI();
	void populateFromSettings(const PSFSettings& settings);
	void rebuildOverrideTable(const QVector<int>& indices);
	bool validateSettings() const;
	void updateValidationState();

	// Generator GroupBoxes
	QGroupBox* zernikeGroupBox;
	QGroupBox* dmGroupBox;

	// Zernike generator controls
	QLineEdit* nollIndicesEdit;
	QDoubleSpinBox* globalMinSpin;
	QDoubleSpinBox* globalMaxSpin;
	QTableWidget* overrideTable;

	// DM generator controls
	QSpinBox* dmRowsSpin;
	QSpinBox* dmColsSpin;
	QDoubleSpinBox* dmCouplingSpin;
	QDoubleSpinBox* dmGaussianIndexSpin;
	QDoubleSpinBox* dmCommandMinSpin;
	QDoubleSpinBox* dmCommandMaxSpin;
	QDoubleSpinBox* dmCommandStepSpin;

	// PSF calculation controls
	QComboBox* gridSizeCombo;
	QDoubleSpinBox* apertureRadiusSpin;
	QComboBox* apertureGeometryCombo;
	QComboBox* normalizationCombo;
	QComboBox* paddingFactorCombo;

	// Display controls
	QCheckBox* displayAutoRangeCheck;
	QDoubleSpinBox* displayMinSpin;
	QDoubleSpinBox* displayMaxSpin;

	// Buttons
	QPushButton* okButton;
	QPushButton* applyButton;

	// State
	PSFSettings initialSettings;
};

#endif // PSFSETTINGSDIALOG_H
