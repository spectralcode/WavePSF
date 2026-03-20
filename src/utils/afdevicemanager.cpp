#include "afdevicemanager.h"
#include "settingsfilemanager.h"
#include <arrayfire.h>

namespace {
	const QString SETTINGS_GROUP = QStringLiteral("af_device");
	const QString KEY_BACKEND    = QStringLiteral("backend");
	const QString KEY_DEVICE_ID  = QStringLiteral("device_id");

	// Migration: old keys stored in [misc] group
	const QString OLD_SETTINGS_GROUP  = QStringLiteral("misc");
	const QString OLD_KEY_BACKEND     = QStringLiteral("af_backend");
	const QString OLD_KEY_DEVICE_ID   = QStringLiteral("af_device_id");
}


AFDeviceManager::AFDeviceManager(SettingsFileManager* guiSettings, QObject* parent)
	: QObject(parent)
	, guiSettings(guiSettings)
	, activeBackendId(0)
	, activeDeviceId(0)
{
	this->enumerateDevices();
	this->loadSettings();
	this->restoreDevice();
}

QVector<AFBackendInfo> AFDeviceManager::getAvailableBackends() const
{
	return this->backends;
}

int AFDeviceManager::getActiveBackendId() const
{
	return this->activeBackendId;
}

int AFDeviceManager::getActiveDeviceId() const
{
	return this->activeDeviceId;
}

bool AFDeviceManager::setDevice(int backendId, int deviceId)
{
	if (backendId == this->activeBackendId && deviceId == this->activeDeviceId) {
		return false;
	}

	emit aboutToChangeDevice(backendId, deviceId);

	AFDeviceManager::setDeviceForCurrentThread(backendId, deviceId);

	this->activeBackendId = backendId;
	this->activeDeviceId = deviceId;
	this->saveSettings();
	emit deviceChanged(backendId, deviceId);
	return true;
}

void AFDeviceManager::setDeviceForCurrentThread(int backendId, int deviceId)
{
	af::setBackend(static_cast<af_backend>(backendId));
	if (deviceId >= 0 && deviceId < static_cast<int>(af::getDeviceCount())) {
		af::setDevice(deviceId);
	}
}

void AFDeviceManager::enumerateDevices()
{
	this->backends.clear();

	af_backend savedBackend = af::getActiveBackend();
	int savedDevice = static_cast<int>(af::getDevice());

	int available = af::getAvailableBackends();

	struct BackendEntry { af_backend id; const char* name; };
	BackendEntry entries[] = {
		{ AF_BACKEND_CUDA,   "CUDA" },
		{ AF_BACKEND_OPENCL, "OpenCL" },
		{ AF_BACKEND_CPU,    "CPU" },
	};

	for (const auto& entry : entries) {
		if (!(available & entry.id)) continue;

		af::setBackend(entry.id);

		AFBackendInfo backendInfo;
		backendInfo.backendId = static_cast<int>(entry.id);
		backendInfo.name = QString::fromLatin1(entry.name);

		int deviceCount = static_cast<int>(af::getDeviceCount());
		for (int i = 0; i < deviceCount; ++i) {
			af::setDevice(i);

			char name[256], platform[256], toolkit[256], compute[256];
			af::deviceInfo(name, platform, toolkit, compute);

			AFDeviceInfo devInfo;
			devInfo.deviceId = i;
			devInfo.name     = QString::fromUtf8(name).trimmed();
			devInfo.platform = QString::fromUtf8(platform).trimmed();
			devInfo.toolkit  = QString::fromUtf8(toolkit).trimmed();
			devInfo.compute  = QString::fromUtf8(compute).trimmed();
			backendInfo.devices.append(devInfo);
		}

		this->backends.append(backendInfo);
	}

	// Restore original AF state
	af::setBackend(savedBackend);
	af::setDevice(savedDevice);
}

void AFDeviceManager::loadSettings()
{
	// Try new settings group first
	QVariantMap settings = this->guiSettings->getStoredSettings(SETTINGS_GROUP);
	if (settings.contains(KEY_BACKEND)) {
		this->activeBackendId = settings.value(KEY_BACKEND, 0).toInt();
		this->activeDeviceId  = settings.value(KEY_DEVICE_ID, 0).toInt();
		return;
	}

	// Migration fallback: read from old [misc] group
	QVariantMap oldSettings = this->guiSettings->getStoredSettings(OLD_SETTINGS_GROUP);
	if (oldSettings.contains(OLD_KEY_BACKEND)) {
		this->activeBackendId = oldSettings.value(OLD_KEY_BACKEND, 0).toInt();
		this->activeDeviceId  = oldSettings.value(OLD_KEY_DEVICE_ID, 0).toInt();
		// Migrate: save to new group
		this->saveSettings();
		return;
	}

	// Default: use whatever AF initialized with
	this->activeBackendId = static_cast<int>(af::getActiveBackend());
	this->activeDeviceId  = static_cast<int>(af::getDevice());
}

void AFDeviceManager::saveSettings()
{
	QVariantMap settings;
	settings[KEY_BACKEND]   = this->activeBackendId;
	settings[KEY_DEVICE_ID] = this->activeDeviceId;
	this->guiSettings->storeSettings(SETTINGS_GROUP, settings);
}

void AFDeviceManager::restoreDevice()
{
	if (this->activeBackendId > 0 &&
		(af::getAvailableBackends() & this->activeBackendId)) {
		AFDeviceManager::setDeviceForCurrentThread(this->activeBackendId, this->activeDeviceId);
	}

	// Sync internal state with what AF actually set
	this->activeBackendId = static_cast<int>(af::getActiveBackend());
	this->activeDeviceId  = static_cast<int>(af::getDevice());
}
