#include "settingsfilemanager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

const QString SettingsFileManager::TIMESTAMP = "timestamp";

SettingsFileManager::SettingsFileManager(const QString& filePath, QObject *parent)
	: QObject(parent), settingsFilePath(filePath) {
	
	// Create settings directory and file if they don't exist
	if (!this->createSettingsDirAndEmptyFile(this->settingsFilePath)) {
		qWarning() << "Failed to create settings file:" << this->settingsFilePath;
	}
	
	// Load timestamp if it exists
	if (QFile::exists(this->settingsFilePath)) {
		QSettings settings(this->settingsFilePath, QSettings::IniFormat);
		this->timestamp = settings.value(TIMESTAMP, "").toString();
	}
}

SettingsFileManager::~SettingsFileManager() {
	// Update timestamp on destruction
	this->setCurrentTimeStamp();
}

void SettingsFileManager::storeSettings(QString settingsGroupName, QVariantMap settingsMap) {
	QSettings settings(this->settingsFilePath, QSettings::IniFormat);
	this->storeValues(&settings, settingsGroupName, settingsMap);
}

QVariantMap SettingsFileManager::getStoredSettings(QString settingsGroupName) {
	QVariantMap settingsMap;

	// Only try to load if the file exists
	if (QFile::exists(this->settingsFilePath)) {
		QSettings settings(this->settingsFilePath, QSettings::IniFormat);
		this->loadValues(&settings, settingsGroupName, &settingsMap);
	}

	return settingsMap;
}

void SettingsFileManager::setTimestamp(QString timestamp) {
	this->timestamp = timestamp;
	QSettings settings(this->settingsFilePath, QSettings::IniFormat);
	settings.setValue(TIMESTAMP, this->timestamp);
}

void SettingsFileManager::setCurrentTimeStamp() {
	QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmsszzz");
	this->setTimestamp(timestamp);
}

QString SettingsFileManager::getTimestamp() const {
	return this->timestamp;
}

QString SettingsFileManager::getSettingsFilePath() const {
	return this->settingsFilePath;
}

void SettingsFileManager::storeValues(QSettings* settings, QString groupName, QVariantMap settingsMap) {
	settings->beginGroup(groupName);
	QMapIterator<QString, QVariant> i(settingsMap);
	while (i.hasNext()) {
		i.next();
		if (i.value().type() == QVariant::Map) {
			storeValues(settings, i.key(), i.value().toMap()); // recurse → [groupName/key] section
		} else {
			settings->setValue(i.key(), i.value()); // leaf value: plain text in INI
		}
	}
	settings->endGroup();
}

void SettingsFileManager::loadValues(QSettings* settings, QString groupName, QVariantMap* settingsMap) {
	settings->beginGroup(groupName);
	for (const QString& key : settings->childKeys()) {
		settingsMap->insert(key, settings->value(key));
	}
	for (const QString& group : settings->childGroups()) {
		QVariantMap subMap;
		loadValues(settings, group, &subMap);
		settingsMap->insert(group, subMap);
	}
	settings->endGroup();
}

bool SettingsFileManager::createSettingsDirAndEmptyFile(QString settingsFilePath) {
	QFile file(settingsFilePath);
	if(!file.exists()){
		QFileInfo fileInfo(file);
		QString dirPath = fileInfo.absolutePath(); //get file path without file name
		QDir settingsDir;
		if(!settingsDir.exists(dirPath)){
			if(!settingsDir.mkpath(dirPath)){
				return false; //failed to create the directory
			}
		}
		//ceate the file as well
		if (!file.open(QIODevice::WriteOnly)) {
			return false; //failed to create the file
		}
		file.close();
		QFile::setPermissions(settingsFilePath, QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ReadOther | QFileDevice::WriteOther);
	}
	return true;
}