#include "led_patterns.h"

#include <math.h>

namespace led {
namespace patterns {

namespace {

uint8_t ClampU8(int value) {
	if (value < 0) {
		return 0;
	}
	if (value > 255) {
		return 255;
	}
	return static_cast<uint8_t>(value);
}

}  // namespace

float clamp01(float value) {
	if (value < 0.0f) {
		return 0.0f;
	}
	if (value > 1.0f) {
		return 1.0f;
	}
	return value;
}

float easeInOut(float t) {
	t = clamp01(t);
	return t * t * (3.0f - (2.0f * t));
}

float triangleWave(float phase) {
	phase -= floorf(phase);
	const float shifted = (phase < 0.5f) ? phase : (1.0f - phase);
	return shifted * 2.0f;
}

float pulseWave(uint32_t nowMs, float speedHz, float minValue, float maxValue) {
	const float phase = (static_cast<float>(nowMs) * speedHz) / 1000.0f;
	const float wave = 0.5f + (0.5f * sinf(phase * 6.2831853f));
	return minValue + ((maxValue - minValue) * wave);
}

RGBColor color(uint8_t red, uint8_t green, uint8_t blue) {
	return RGBColor(red, green, blue);
}

RGBColor scale(const RGBColor& value, float amount) {
	amount = clamp01(amount);
	return RGBColor(
			ClampU8(static_cast<int>(value.r * amount)),
			ClampU8(static_cast<int>(value.g * amount)),
			ClampU8(static_cast<int>(value.b * amount)));
}

RGBColor blend(const RGBColor& from, const RGBColor& to, float amount) {
	amount = clamp01(amount);
	const float inv = 1.0f - amount;
	return RGBColor(
			ClampU8(static_cast<int>((from.r * inv) + (to.r * amount))),
			ClampU8(static_cast<int>((from.g * inv) + (to.g * amount))),
			ClampU8(static_cast<int>((from.b * inv) + (to.b * amount))));
}

RGBColor add(const RGBColor& base, const RGBColor& overlay) {
	return RGBColor(
			ClampU8(static_cast<int>(base.r) + static_cast<int>(overlay.r)),
			ClampU8(static_cast<int>(base.g) + static_cast<int>(overlay.g)),
			ClampU8(static_cast<int>(base.b) + static_cast<int>(overlay.b)));
}

RGBColor withBrightness(const RGBColor& value, uint8_t brightness) {
	return scale(value, static_cast<float>(brightness) / 255.0f);
}

void fill(RGBColor* leds, size_t count, const RGBColor& value) {
	if (leds == nullptr) {
		return;
	}

	for (size_t i = 0; i < count; ++i) {
		leds[i] = value;
	}
}

void fillGradient(RGBColor* leds,
									size_t count,
					const RGBColor& start,
					const RGBColor& end,
									float offset) {
	if (leds == nullptr || count == 0) {
		return;
	}

	for (size_t i = 0; i < count; ++i) {
		float t = (count == 1) ? 0.0f : static_cast<float>(i) / static_cast<float>(count - 1);
		t += offset;
		t -= floorf(t);
		leds[i] = blend(start, end, t);
	}
}

void addSweep(RGBColor* leds,
							size_t count,
							float center,
							float width,
							const RGBColor& value,
							float intensity) {
	if (leds == nullptr || count == 0 || width <= 0.0f || intensity <= 0.0f) {
		return;
	}

	for (size_t i = 0; i < count; ++i) {
		const float distance = fabsf(static_cast<float>(i) - center);
		if (distance > width) {
			continue;
		}

		const float amount = intensity * (1.0f - (distance / width));
		leds[i] = add(leds[i], scale(value, amount));
	}
}

}  // namespace patterns
}  // namespace led
