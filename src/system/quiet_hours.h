#pragma once

#include <stdint.h>

namespace app {

inline bool quietHoursInRange(bool enabled, uint32_t start, uint32_t end, int hour) {
	if (!enabled || start > 23 || end > 23 || hour < 0 || hour > 23) return false;
	if (start == end) return true;
	const uint32_t localHour = static_cast<uint32_t>(hour);
	return start < end ? localHour >= start && localHour < end :
		localHour >= start || localHour < end;
}

}  // namespace app
