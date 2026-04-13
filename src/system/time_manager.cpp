#include "time_manager.h"

#include <time.h>

namespace app {

void TimeManager::begin(const TimeConfig& config) {
	config_ = config;
	initialized_ = true;
	configured_ = false;
	synchronized_ = false;
	lastSyncAttemptMs_ = 0;
	lastSuccessfulSyncMs_ = 0;
}

void TimeManager::update(bool wifiConnected) {
	if (!initialized_ || !wifiConnected) {
		return;
	}

	const uint32_t nowMs = millis();
	const bool needsConfig = !configured_;
	const bool needsRetry = !synchronized_ && ((nowMs - lastSyncAttemptMs_) >= config_.retryIntervalMs);
	const bool needsResync = synchronized_ && ((nowMs - lastSuccessfulSyncMs_) >= config_.resyncIntervalMs);
	if (needsConfig || needsRetry || needsResync) {
		requestSync(nowMs);
	}

	time_t now = time(nullptr);
	if (now > 1700000000) {
		synchronized_ = true;
		lastSuccessfulSyncMs_ = nowMs;
	}
}

bool TimeManager::configured() const {
	return configured_;
}

bool TimeManager::synchronized() const {
	return synchronized_;
}

void TimeManager::requestSync(uint32_t nowMs) {
	configTime(config_.gmtOffsetSeconds,
			   config_.daylightOffsetSeconds,
			   config_.primaryServer,
			   config_.secondaryServer);
	configured_ = true;
	lastSyncAttemptMs_ = nowMs;
}

}  // namespace app
