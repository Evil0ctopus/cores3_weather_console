#pragma once

#include <stddef.h>
#include <stdint.h>

namespace app {

enum class TouchFeedback : uint8_t { None, Tap, Hold, SwipeUp, SwipeDown };

struct TouchSample {
	int16_t x = 0;
	int16_t y = 0;
	int16_t dx = 0;
	int16_t dy = 0;
	bool pressed = false;
	TouchFeedback feedback = TouchFeedback::None;
};

class TouchBuffer {
 public:
	static constexpr size_t kCapacity = 32;

	bool push(const TouchSample& sample) {
		if (count_ > 0) {
			TouchSample& last = samples_[(head_ + count_ - 1) % kCapacity];
			if (last.pressed == sample.pressed &&
					last.feedback == TouchFeedback::None && sample.feedback == TouchFeedback::None) {
				last = sample;
				return true;
			}
		}
		if (count_ == kCapacity) {
			return false;
		}
		samples_[(head_ + count_) % kCapacity] = sample;
		++count_;
		return true;
	}

	bool pop(TouchSample& sample) {
		if (count_ == 0) {
			return false;
		}
		sample = samples_[head_];
		head_ = (head_ + 1) % kCapacity;
		--count_;
		return true;
	}

	bool empty() const { return count_ == 0; }
	void clear() { head_ = 0; count_ = 0; }

 private:
	TouchSample samples_[kCapacity]{};
	size_t head_ = 0;
	size_t count_ = 0;
};

}  // namespace app
