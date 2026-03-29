#ifndef RWSETTINGSWIDGET_H
#define RWSETTINGSWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVariantMap>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

class RWSettingsWidget : public QWidget
{
	Q_OBJECT
public:
	explicit RWSettingsWidget(QWidget* parent = nullptr);

	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

signals:
	void settingChanged(QVariantMap rwSettings);

private:
	void emitSettingChanged();
	void updatePolarizationVisibility(int modeIndex);

	QMap<QString, QWidget*> spinBoxes;
	QComboBox* scalingCombo;
	QComboBox* polarizationCombo;
	QLabel* polAngleLabel;
	QDoubleSpinBox* polAngleSpinBox;
	QCheckBox* confocalCheckBox;
};

#endif // RWSETTINGSWIDGET_H
