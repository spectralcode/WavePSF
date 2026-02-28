#ifndef PSFSETTINGSDIALOG_H
#define PSFSETTINGSDIALOG_H

#include <QDialog>
#include "core/psf/psfsettings.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QTableWidget;
class QPushButton;

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
	bool validateNollSpec() const;
	void updateValidationState();

	// Wavefront generator controls
	QLineEdit* nollIndicesEdit;
	QDoubleSpinBox* globalMinSpin;
	QDoubleSpinBox* globalMaxSpin;
	QTableWidget* overrideTable;

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
