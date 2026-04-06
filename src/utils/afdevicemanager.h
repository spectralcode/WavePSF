#ifndef AFDEVICEMANAGER_H
#define AFDEVICEMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>

class SettingsFileManager;

struct AFDeviceInfo {
	int deviceId = 0;
	QString name;
	QString platform;
	QString toolkit;
	QString compute;
};

struct AFBackendInfo {
	int backendId = 0;
	QString name;
	QVector<AFDeviceInfo> devices;
};

class AFDeviceManager : public QObject
{
	Q_OBJECT

public:
	explicit AFDeviceManager(SettingsFileManager* guiSettings, QObject* parent = nullptr);

	// Query (cached data, no AF calls)
	QVector<AFBackendInfo> getAvailableBackends() const;
	int getActiveBackendId() const;
	int getActiveDeviceId() const;

	// Switch device (does AF calls + persists to INI).
	// Returns true if the device actually changed.
	// Emits aboutToChangeDevice before switching, deviceChanged after.
	bool setDevice(int backendId, int deviceId);

	// Set AF backend + device for the calling thread.
	// Worker threads must call this at the start of their work function
	// because AF backend and device state are per-thread.
	static void setDeviceForCurrentThread(int backendId, int deviceId);

signals:
	void aboutToChangeDevice(int backendId, int deviceId);
	void deviceChanged(int backendId, int deviceId);

private:
	void enumerateDevices();
	void loadSettings();
	void saveSettings();
	void restoreDevice();

	bool tryActivateBackend(int backendId, int deviceId);
	void syncActiveState();
	bool isBackendAvailable(int backendId) const;

	SettingsFileManager* guiSettings;
	QVector<AFBackendInfo> backends;
	int activeBackendId;
	int activeDeviceId;
};

#endif // AFDEVICEMANAGER_H
