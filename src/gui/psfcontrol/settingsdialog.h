#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QMap>
#include <QVector>
#include "core/psf/psfsettings.h"
#include "core/psf/iwavefrontgenerator.h"
#include "utils/afdevicemanager.h"

class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QTableWidget;
class QCheckBox;
class QPushButton;
class QGroupBox;
class QWidget;
class QLabel;

class SettingsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit SettingsDialog(const PSFSettings& settings,
							   bool autoRange, double displayMin, double displayMax,
							   const QVector<AFBackendInfo>& backends,
							   int activeBackend, int activeDevice,
							   QWidget* parent = nullptr);

	PSFSettings getSettings() const;
	bool getAutoRange() const;
	double getDisplayMin() const;
	double getDisplayMax() const;
	int getSelectedBackend() const;
	int getSelectedDeviceId() const;

signals:
	void settingsApplied(PSFSettings settings);
	void displaySettingsApplied(bool autoRange, double min, double max);
	void deviceSettingsApplied(int backendId, int deviceId);

public slots:
	void updateGeneratorType(const QString& typeName);

private slots:
	void onApplyClicked();

private:
	void setupUI();
	void populateFromSettings(const PSFSettings& settings);
	bool validateSettings() const;
	void updateValidationState();

	// Builds a QGroupBox with auto-generated spinboxes from generator settings descriptors.
	// Stores created widgets in generatorSettingWidgets[typeName].
	void buildGeneratorSettingsGroup(const QString& typeName,
	                                 const QVector<NumericSettingDescriptor>& descriptors,
	                                 QWidget* parent);
	QVariantMap readGeneratorSettingsWidgets(const QString& typeName) const;
	void populateGeneratorSettingsWidgets(const QString& typeName, const QVariantMap& gs);

	// Per-mode Zernike wavefront UI (modes that use ZernikeGenerator)
	struct ZernikeWidgets {
		QGroupBox* groupBox = nullptr;
		QLineEdit* nollIndicesEdit = nullptr;
		QDoubleSpinBox* globalMinSpin = nullptr;
		QDoubleSpinBox* globalMaxSpin = nullptr;
		QTableWidget* overrideTable = nullptr;
	};
	ZernikeWidgets buildZernikeUI(const QString& modeName, QWidget* parent);
	void rebuildOverrideTable(ZernikeWidgets& zw, const QVector<int>& indices);
	QVariantMap readZernikeSettings(const ZernikeWidgets& zw) const;
	void populateZernikeWidgets(ZernikeWidgets& zw, const QVariantMap& zernikeGs);

	QMap<QString, ZernikeWidgets> zernikeWidgets;

	// Auto-generated GroupBoxes for descriptor-based generators (typeName → GroupBox)
	QMap<QString, QGroupBox*> generatorGroupBoxes;

	// Auto-generated setting widgets (typeName → key → QSpinBox or QDoubleSpinBox)
	QMap<QString, QMap<QString, QWidget*>> generatorSettingWidgets;

	// PSF calculation controls
	QComboBox* gridSizeCombo;

	// Display controls
	QCheckBox* displayAutoRangeCheck;
	QDoubleSpinBox* displayMinSpin;
	QDoubleSpinBox* displayMaxSpin;

	// Misc controls
	QComboBox* backendCombo;
	QComboBox* deviceCombo;
	QLabel* deviceInfoLabel;
	QVector<AFBackendInfo> cachedBackends;

	// Buttons
	QPushButton* okButton;
	QPushButton* applyButton;

	// State
	PSFSettings initialSettings;
	QString currentGeneratorTypeName;
	int initialActiveBackend;
	int initialActiveDevice;
};

#endif // SETTINGSDIALOG_H
