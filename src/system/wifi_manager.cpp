#include "wifi_manager.h"

#include <Preferences.h>

namespace app {
namespace {

const char* kProvisioningSsidPrefix = "CoreWeather-Setup-";
const uint32_t kScanTimeoutMs = 12000;
const uint32_t kScanMaxMsPerChannel = 300;

const char* kWifiNamespace = "wifi";
const char* kKeySsid = "ssid";
const char* kKeyPassword = "password";
const char* kKeyAutoConnect = "autoconn";

String normalizedValue(const String& value) {
	String out = value;
	out.trim();
	return out;
}

String statusToString(wl_status_t status) {
	switch (status) {
		case WL_CONNECTED:
			return "Connected";
		case WL_IDLE_STATUS:
			return "Connecting";
		case WL_NO_SSID_AVAIL:
			return "SSID not found";
		case WL_CONNECT_FAILED:
			return "Connect failed";
		case WL_CONNECTION_LOST:
			return "Connection lost";
		case WL_DISCONNECTED:
			return "Disconnected";
		case WL_SCAN_COMPLETED:
			return "Scan complete";
		default:
			return "Offline";
	}
}

}  // namespace

bool WifiManager::begin(const WifiConfig& config) {
	config_ = config;
	if (!hasUsableConfig(config_)) {
		WifiConfig stored;
		if (loadStoredConfig(stored)) {
			config_ = stored;
		}
	}

	initialized_ = true;
	WiFi.mode(WIFI_STA);
	if (config_.autoConnect && hasUsableConfig(config_)) {
		connectNow(millis());
	}
	updateProvisioningAccessPoint();
	return true;
}

void WifiManager::update() {
	if (!initialized_) {
		return;
	}
	if (WiFi.status() != WL_CONNECTED && config_.autoConnect && hasUsableConfig(config_)) {
		const uint32_t nowMs = millis();
		if ((nowMs - lastAttemptAtMs_) >= config_.reconnectIntervalMs) {
			connectNow(nowMs);
		}
	}
	updateProvisioningAccessPoint();
}

bool WifiManager::connected() const {
	return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::connecting() const {
	if (!initialized_ || !hasUsableConfig(config_) || connected()) {
		return false;
	}
	if (WiFi.status() == WL_IDLE_STATUS) {
		return true;
	}
	return (millis() - lastAttemptAtMs_) < config_.connectTimeoutMs;
}

wl_status_t WifiManager::status() const {
	return WiFi.status();
}

uint32_t WifiManager::lastAttemptAtMs() const {
	return lastAttemptAtMs_;
}

const WifiConfig& WifiManager::config() const {
	return config_;
}

bool WifiManager::applyConfig(const WifiConfig& config, bool connectImmediately, bool persistToNvs) {
	WifiConfig next = config;
	next.ssid = normalizedValue(next.ssid);
	if (next.reconnectIntervalMs == 0) {
		next.reconnectIntervalMs = config_.reconnectIntervalMs > 0 ? config_.reconnectIntervalMs : 10000;
	}
	if (next.connectTimeoutMs == 0) {
		next.connectTimeoutMs = config_.connectTimeoutMs > 0 ? config_.connectTimeoutMs : 12000;
	}
	config_ = next;

	if (persistToNvs) {
		saveStoredConfig(config_);
	}

	if (connectImmediately && hasUsableConfig(config_)) {
		connectNow(millis());
	} else if (!config_.autoConnect || !hasUsableConfig(config_)) {
		WiFi.disconnect(false, false);
	}
	updateProvisioningAccessPoint();
	return true;
}

size_t WifiManager::scanNetworks(WifiNetworkInfo* outResults, size_t maxResults) {
	if (outResults == nullptr || maxResults == 0) {
		return 0;
	}

	if (scanInProgress_) {
		const int result = WiFi.scanComplete();
		if (result >= 0) {
			finalizeScanResults(result);
		} else if (result == WIFI_SCAN_FAILED || (millis() - scanStartedAtMs_) > kScanTimeoutMs) {
			scanResultCount_ = 0;
			scanInProgress_ = false;
			WiFi.scanDelete();
		}
	}

	if (scanInProgress_) {
		return 0;
	}

	const size_t count = scanResultCount_ < maxResults ? scanResultCount_ : maxResults;
	for (size_t i = 0; i < count; ++i) {
		outResults[i] = scanResults_[i];
	}
	return count;
}

bool WifiManager::startScan() {
	if (!initialized_ || scanInProgress_) {
		return false;
	}

	// Keep AP+STA active while provisioning so the web UI remains reachable.
	const wifi_mode_t targetMode = provisioningActive_ ? WIFI_AP_STA : WIFI_STA;
	if (WiFi.getMode() != targetMode) {
		WiFi.mode(targetMode);
	}

	WiFi.scanDelete();
	scanResultCount_ = 0;
	scanStartedAtMs_ = millis();
	const int startResult = WiFi.scanNetworks(true, true, false, kScanMaxMsPerChannel);
	if (startResult == WIFI_SCAN_RUNNING) {
		scanInProgress_ = true;
		return true;
	}

	if (startResult >= 0) {
		finalizeScanResults(startResult);
		return true;
	}

	scanInProgress_ = false;
	return false;
}

bool WifiManager::scanInProgress() const {
	return scanInProgress_;
}

void WifiManager::finalizeScanResults(int found) {
	scanResultCount_ = 0;
	for (int i = 0; i < found && scanResultCount_ < kMaxScanResults; ++i) {
		const String ssid = WiFi.SSID(i);
		if (ssid.length() == 0) {
			continue;
		}

		bool duplicate = false;
		for (size_t existing = 0; existing < scanResultCount_; ++existing) {
			if (scanResults_[existing].ssid == ssid) {
				if (WiFi.RSSI(i) > scanResults_[existing].rssi) {
					scanResults_[existing].rssi = WiFi.RSSI(i);
					scanResults_[existing].secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
				}
				duplicate = true;
				break;
			}
		}
		if (duplicate) {
			continue;
		}

		scanResults_[scanResultCount_].ssid = ssid;
		scanResults_[scanResultCount_].rssi = WiFi.RSSI(i);
		scanResults_[scanResultCount_].secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
		++scanResultCount_;
	}

	scanInProgress_ = false;
	WiFi.scanDelete();
}

WifiStatusInfo WifiManager::statusInfo() const {
	WifiStatusInfo info;
	info.connected = connected();
	info.connecting = connecting();
	info.autoConnect = config_.autoConnect;
	info.statusCode = status();
	info.statusText = statusLabel();
	info.lastAttemptAtMs = lastAttemptAtMs_;
	info.ssid = info.connected ? WiFi.SSID() : config_.ssid;
	if (info.connected) {
		info.ipAddress = WiFi.localIP().toString();
		info.rssi = WiFi.RSSI();
	}
	info.accessPointActive = provisioningActive_;
	info.accessPointSsid = provisioningSsid_;
	if (provisioningActive_) {
		info.accessPointIpAddress = WiFi.softAPIP().toString();
	}
	return info;
}

String WifiManager::statusLabel() const {
	if (!initialized_) {
		return "Not initialized";
	}
	if (connected()) {
		return statusToString(WiFi.status());
	}
	if (!hasUsableConfig(config_)) {
		return provisioningActive_ ? "Setup AP active" : "Setup required";
	}
	if (provisioningActive_) {
		return "Connecting (setup AP active)";
	}
	return statusToString(WiFi.status());
}

bool WifiManager::hasUsableConfig(const WifiConfig& config) const {
	return config.ssid.length() > 0;
}

bool WifiManager::loadStoredConfig(WifiConfig& out) {
	Preferences prefs;
	if (!prefs.begin(kWifiNamespace, true)) {
		return false;
	}

	out = WifiConfig{};
	out.ssid = normalizedValue(prefs.getString(kKeySsid, ""));
	out.password = prefs.getString(kKeyPassword, "");
	out.autoConnect = prefs.getBool(kKeyAutoConnect, true);
	prefs.end();
	return out.ssid.length() > 0;
}

bool WifiManager::saveStoredConfig(const WifiConfig& config) {
	Preferences prefs;
	if (!prefs.begin(kWifiNamespace, false)) {
		return false;
	}

	prefs.putString(kKeySsid, config.ssid);
	prefs.putString(kKeyPassword, config.password);
	prefs.putBool(kKeyAutoConnect, config.autoConnect);
	prefs.end();
	return true;
}

String WifiManager::buildProvisioningSsid() const {
	uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFULL);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%s%06lX", kProvisioningSsidPrefix, static_cast<unsigned long>(suffix));
	return String(buffer);
}

void WifiManager::updateProvisioningAccessPoint() {
	if (!initialized_) {
		return;
	}

	const wifi_mode_t targetMode = WIFI_AP_STA;
	if (WiFi.getMode() != targetMode) {
		WiFi.mode(targetMode);
	}

	if (provisioningSsid_.length() == 0) {
		provisioningSsid_ = buildProvisioningSsid();
	}
	if (!provisioningActive_) {
		const IPAddress apIp(192, 168, 4, 1);
		const IPAddress apGateway(192, 168, 4, 1);
		const IPAddress apSubnet(255, 255, 255, 0);
		WiFi.softAPConfig(apIp, apGateway, apSubnet);
		provisioningActive_ = WiFi.softAP(provisioningSsid_.c_str());
	}
}

void WifiManager::stopProvisioningAccessPoint() {
	if (!provisioningActive_) {
		return;
	}
	WiFi.softAPdisconnect(true);
	provisioningActive_ = false;
}

void WifiManager::connectNow(uint32_t nowMs) {
	if (!hasUsableConfig(config_)) {
		return;
	}
	lastAttemptAtMs_ = nowMs;
	WiFi.disconnect(false, false);
	WiFi.begin(config_.ssid.c_str(), config_.password.c_str());
}

}  // namespace app
