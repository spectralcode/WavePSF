#include "afdevicemanager.h"
#include "settingsfilemanager.h"
#include "logging.h"
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

	if (!this->tryActivateBackend(backendId, deviceId)) {
		LOG_WARNING() << "Failed to switch to backend" << backendId
					  << "device" << deviceId;
		// Restore previous backend (best-effort)
		this->tryActivateBackend(this->activeBackendId, this->activeDeviceId);
		this->syncActiveState();
		return false;
	}

	this->activeBackendId = backendId;
	this->activeDeviceId = deviceId;
	this->saveSettings();
	emit deviceChanged(backendId, deviceId);
	return true;
}

void AFDeviceManager::setDeviceForCurrentThread(int backendId, int deviceId)
{
	try {
		af::setBackend(static_cast<af_backend>(backendId));
		int deviceCount = static_cast<int>(af::getDeviceCount());
		if (deviceId < 0 || deviceId >= deviceCount) {
			LOG_WARNING() << "setDeviceForCurrentThread: invalid device" << deviceId
						  << "for backend" << backendId
						  << "(device count:" << deviceCount << ")";
			return;
		}
		af::setDevice(deviceId);
	} catch (af::exception& e) {
		LOG_WARNING() << "setDeviceForCurrentThread failed for backend"
					  << backendId << "device" << deviceId << ":" << e.what();
	}
}

void AFDeviceManager::enumerateDevices()
{
	this->backends.clear();

	// Save current AF state (best-effort — may fail if startup state is broken)
	af_backend savedBackend = AF_BACKEND_CPU;
	int savedDevice = 0;
	try {
		savedBackend = af::getActiveBackend();
		savedDevice = static_cast<int>(af::getDevice());
	} catch (af::exception&) {}

	int available = 0;
	try {
		available = af::getAvailableBackends();
	} catch (af::exception& e) {
		LOG_WARNING() << "af::getAvailableBackends() failed:" << e.what();
		return;
	}

	struct BackendEntry { af_backend id; const char* name; };
	BackendEntry entries[] = {
		{ AF_BACKEND_CUDA,   "CUDA" },
		{ AF_BACKEND_OPENCL, "OpenCL" },
		{ AF_BACKEND_CPU,    "CPU" },
	};

	for (const auto& entry : entries) {
		if (!(available & entry.id)) continue;

		try {
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
		} catch (af::exception& e) {
			LOG_WARNING() << "Skipping" << entry.name << "backend:" << e.what();
		}
	}

	// Restore original AF state (best-effort)
	try {
		af::setBackend(savedBackend);
		af::setDevice(savedDevice);
	} catch (af::exception&) {}
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

	// Default: use whatever AF initialized with (best-effort)
	try {
		this->activeBackendId = static_cast<int>(af::getActiveBackend());
		this->activeDeviceId  = static_cast<int>(af::getDevice());
	} catch (af::exception& e) {
		LOG_WARNING() << "loadSettings default AF query failed:" << e.what();
		this->activeBackendId = AF_BACKEND_CPU;
		this->activeDeviceId  = 0;
	}
}

void AFDeviceManager::saveSettings()
{
	QVariantMap settings;
	settings[KEY_BACKEND]   = this->activeBackendId;
	settings[KEY_DEVICE_ID] = this->activeDeviceId;
	this->guiSettings->storeSettings(SETTINGS_GROUP, settings);
}

bool AFDeviceManager::tryActivateBackend(int backendId, int deviceId)
{
	try {
		af::setBackend(static_cast<af_backend>(backendId));
		int deviceCount = static_cast<int>(af::getDeviceCount());
		if (deviceId < 0 || deviceId >= deviceCount) {
			LOG_WARNING() << "Invalid device" << deviceId
						  << "for backend" << backendId
						  << "(device count:" << deviceCount << ")";
			return false;
		}
		af::setDevice(deviceId);
		// Compute probe: force real execution to verify backend works.
		// eval() alone may defer via JIT; sync() forces completion.
		af::array probe = af::constant(1.0f, 2);
		probe.eval();
		af::sync();
		return true;
	} catch (af::exception& e) {
		LOG_WARNING() << "Backend" << backendId << "device" << deviceId
					  << "failed probe:" << e.what();
		return false;
	}
}

void AFDeviceManager::syncActiveState()
{
	try {
		this->activeBackendId = static_cast<int>(af::getActiveBackend());
		this->activeDeviceId  = static_cast<int>(af::getDevice());
	} catch (af::exception& e) {
		LOG_WARNING() << "syncActiveState failed:" << e.what();
	}
}

bool AFDeviceManager::isBackendAvailable(int backendId) const
{
	for (const auto& backend : this->backends) {
		if (backend.backendId == backendId) {
			return !backend.devices.isEmpty();
		}
	}
	return false;
}

void AFDeviceManager::restoreDevice()
{
	// Try saved backend+device first
	if (this->isBackendAvailable(this->activeBackendId)) {
		if (this->tryActivateBackend(this->activeBackendId, this->activeDeviceId)) {
			this->syncActiveState();
			return;
		}
		LOG_WARNING() << "Saved backend" << this->activeBackendId
					  << "failed, trying fallback chain";
	}

	// Fallback: try each enumerated backend in priority order (CUDA > OpenCL > CPU)
	for (const auto& backend : this->backends) {
		if (!backend.devices.isEmpty()) {
			if (this->tryActivateBackend(backend.backendId, 0)) {
				LOG_INFO() << "Fallback activated:" << backend.name;
				this->syncActiveState();
				this->saveSettings();
				return;
			}
		}
	}

	// Last resort: whatever AF defaults to.
	// Do NOT save settings here — avoid overwriting a previously good config
	// with a potentially broken default state.
	LOG_WARNING() << "All backends failed probe, using AF default";
	this->syncActiveState();
}
