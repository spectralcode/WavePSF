#ifndef SETTINGSFILEMANAGER_H
#define SETTINGSFILEMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariantMap>
#include <QDateTime>

class SettingsFileManager : public QObject {
	Q_OBJECT

public:
	explicit SettingsFileManager(const QString& filePath, QObject *parent = nullptr);
	~SettingsFileManager();

	/**
	* Stores settings from arbitrary QVariantMap into the settings group defined by "settingsGroupName". 
	* This method is typically used to store settings from systems. Systems are shared libraries, 
	* so the main application can not know in advance (during compile time) which settings every 
	* system has. This method could be used to mess up previously stored settings if settingsGroupName 
	* is an already used group name.
	*
	* @param settingsGroupName is the group name that will be used in the settings file. To load the saved settings, the identical group name needs to be used.
	* @param settingsMap is a arbitrary QVariantMap that contains the settings to be saved. 
	**/
	void storeSettings(QString settingsGroupName, QVariantMap settingsMap);

	/**
	* Loads previously stored settings from the specified group
	*
	* @see storeSettings(QString settingsGroupName, QVariantMap settingsMap)
	* @param settingsGroupName is the group name that will be used in the settings file. To load the saved settings, the identical group name needs to be used.
	* @return QVariantMap that contains previously saved settings.
	**/
	QVariantMap getStoredSettings(QString settingsGroupName);

	void setTimestamp(QString timestamp);
	void setCurrentTimeStamp();
	QString getTimestamp() const;

	QString getSettingsFilePath() const;

private:
	void storeValues(QSettings* settings, QString groupName, QVariantMap settingsMap);
	void loadValues(QSettings* settings, QString groupName, QVariantMap* settingsMap);
	bool createSettingsDirAndEmptyFile(QString settingsFilePath);

	QString settingsFilePath;
	QString timestamp;
	
	static const QString TIMESTAMP;
};

#endif // SETTINGSFILEMANAGER_H