#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace app {

struct WifiConfig {
	String ssid;
	String password;
	bool autoConnect = true;
	uint32_t reconnectIntervalMs = 10000;
	uint32_t connectTimeoutMs = 12000;
};

struct WifiNetworkInfo {
	String ssid;
	int32_t rssi = -127;
	bool secure = false;
};

struct WifiStatusInfo {
	String ssid;
	String ipAddress;
	String statusText;
	String accessPointSsid;
	String accessPointIpAddress;
	int32_t rssi = -127;
	bool connected = false;
	bool connecting = false;
	bool autoConnect = false;
	bool accessPointActive = false;
	wl_status_t statusCode = WL_DISCONNECTED;
	uint32_t lastAttemptAtMs = 0;
};

class WifiManager {
 public:
	static const size_t kMaxScanResults = 16;

	bool begin(const WifiConfig& config);
	void update();

	bool connected() const;
	bool connecting() const;
	wl_status_t status() const;
	uint32_t lastAttemptAtMs() const;
	const WifiConfig& config() const;
	bool applyConfig(const WifiConfig& config, bool connectImmediately, bool persistToNvs);
	size_t scanNetworks(WifiNetworkInfo* outResults, size_t maxResults);
	bool startScan();
	bool scanInProgress() const;
	WifiStatusInfo statusInfo() const;
	String statusLabel() const;

 private:
	bool hasUsableConfig(const WifiConfig& config) const;
	bool loadStoredConfig(WifiConfig& out);
	bool saveStoredConfig(const WifiConfig& config);
	String buildProvisioningSsid() const;
	void updateProvisioningAccessPoint();
	void stopProvisioningAccessPoint();
	void connectNow(uint32_t nowMs);
	void finalizeScanResults(int found);

	WifiConfig config_{};
	bool initialized_ = false;
	uint32_t lastAttemptAtMs_ = 0;
	String provisioningSsid_;
	bool provisioningActive_ = false;
	WifiNetworkInfo scanResults_[kMaxScanResults]{};
	size_t scanResultCount_ = 0;
	bool scanInProgress_ = false;
 	uint32_t scanStartedAtMs_ = 0;
};

}  // namespace app
