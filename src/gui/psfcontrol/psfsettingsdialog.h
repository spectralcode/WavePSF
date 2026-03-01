#ifndef PSFSETTINGSDIALOG_H
#define PSFSETTINGSDIALOG_H

#include <QDialog>
#include "core/psf/psfsettings.h"

class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QTableWidget;
class QPushButton;
class QStackedWidget;

class PSFSettingsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit PSFSettingsDialog(const PSFSettings& settings, QWidget* parent = nullptr);

	PSFSettings getSettings() const;

signals:
	void settingsApplied(PSFSettings settings);

private slots:
	void onNollIndicesChanged();
	void onApplyClicked();

private:
	void setupUI();
	void populateFromSettings(const PSFSettings& settings);
	void rebuildOverrideTable(const QVector<int>& indices);
	bool validateSettings() const;
	void updateValidationState();

	// Generator-specific stacked widget
	QStackedWidget* generatorStack;

	// Zernike generator controls (page 0)
	QLineEdit* nollIndicesEdit;
	QDoubleSpinBox* globalMinSpin;
	QDoubleSpinBox* globalMaxSpin;
	QTableWidget* overrideTable;

	// DM generator controls (page 1)
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
	QComboBox* normalizationCombo;

	// Buttons
	QPushButton* okButton;
	QPushButton* applyButton;

	// State
	PSFSettings initialSettings;
};

#endif // PSFSETTINGSDIALOG_H
