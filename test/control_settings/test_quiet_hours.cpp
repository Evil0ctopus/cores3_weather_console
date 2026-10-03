#include "../../src/system/quiet_hours.h"

#include <assert.h>
#include <stdio.h>

int main() {
	for (int hour = 0; hour < 24; ++hour) {
		assert(app::quietHoursInRange(true, 22, 7, hour) == (hour >= 22 || hour < 7));
		assert(app::quietHoursInRange(true, 9, 17, hour) == (hour >= 9 && hour < 17));
		assert(app::quietHoursInRange(true, 0, 0, hour));
		assert(!app::quietHoursInRange(false, 22, 7, hour));
	}
	assert(!app::quietHoursInRange(true, 22, 7, -1));
	assert(!app::quietHoursInRange(true, 22, 7, 24));
	assert(!app::quietHoursInRange(true, 24, 7, 12));
	assert(!app::quietHoursInRange(true, 7, 24, 12));
	puts("PASS: quiet-hours boundaries, overnight, same-day, all-day and unknown time");
}
