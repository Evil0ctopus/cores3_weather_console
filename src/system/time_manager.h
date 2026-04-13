#pragma once

#include <Arduino.h>

namespace app {

struct TimeConfig {
	long gmtOffsetSeconds = 0;
	int daylightOffsetSeconds = 0;
	const char* primaryServer = "pool.ntp.org";
	const char* secondaryServer = "time.nist.gov";
	uint32_t resyncIntervalMs = 6UL * 60UL * 60UL * 1000UL;
	uint32_t retryIntervalMs = 30000UL;
};

class TimeManager {
 public:
	void begin(const TimeConfig& config = TimeConfig());
	void update(bool wifiConnected);

	bool configured() const;
	bool synchronized() const;

 private:
	void requestSync(uint32_t nowMs);

	TimeConfig config_{};
	bool initialized_ = false;
	bool configured_ = false;
	bool synchronized_ = false;
	uint32_t lastSyncAttemptMs_ = 0;
	uint32_t lastSuccessfulSyncMs_ = 0;
};

}  // namespace app
