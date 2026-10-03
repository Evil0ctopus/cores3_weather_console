#include "../../src/system/touch_buffer.h"

#include <assert.h>
#include <stdio.h>

int main() {
	app::TouchBuffer buffer;
	app::TouchSample press;
	press.pressed = true;
	press.x = 160;
	press.y = 15;
	app::TouchSample release = press;
	release.pressed = false;
	release.feedback = app::TouchFeedback::Tap;

	assert(buffer.push(press));
	for (int i = 0; i < 200; ++i) {
		app::TouchSample move = press;
		move.x = static_cast<int16_t>(160 + i % 4);
		assert(buffer.push(move));
	}
	assert(buffer.push(release));
	app::TouchSample sample;
	assert(buffer.pop(sample) && sample.pressed);
	assert(buffer.pop(sample) && !sample.pressed && sample.feedback == app::TouchFeedback::Tap);
	assert(buffer.empty());

	app::TouchSample hold = press;
	hold.feedback = app::TouchFeedback::Hold;
	assert(buffer.push(press));
	assert(buffer.push(hold));
	assert(buffer.push(press));
	assert(buffer.push(release));
	assert(buffer.pop(sample) && sample.feedback == app::TouchFeedback::None);
	assert(buffer.pop(sample) && sample.feedback == app::TouchFeedback::Hold);
	assert(buffer.pop(sample) && sample.pressed);
	assert(buffer.pop(sample) && sample.feedback == app::TouchFeedback::Tap);
	assert(!buffer.pop(sample));

	for (size_t i = 0; i < app::TouchBuffer::kCapacity; ++i) {
		assert(buffer.push(i % 2 == 0 ? press : release));
	}
	assert(!buffer.push(press));
	for (size_t i = 0; i < app::TouchBuffer::kCapacity; ++i) {
		assert(buffer.pop(sample));
		assert(sample.pressed == (i % 2 == 0));
	}
	assert(buffer.empty());
	for (int cycle = 0; cycle < 100; ++cycle) {
		assert(buffer.push(press));
		assert(buffer.push(release));
		assert(buffer.pop(sample) && sample.pressed);
		assert(buffer.pop(sample) && !sample.pressed);
	}
	assert(buffer.push(press));
	buffer.clear();
	assert(buffer.empty());
	puts("PASS: quick edges, movement coalescing, hold feedback, overflow, wraparound and reset");
}
