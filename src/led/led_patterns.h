#pragma once

#include <Arduino.h>
#include <M5Unified.h>

namespace led {
namespace patterns {

float clamp01(float value);
float easeInOut(float t);
float triangleWave(float phase);
float pulseWave(uint32_t nowMs, float speedHz, float minValue, float maxValue);

RGBColor color(uint8_t red, uint8_t green, uint8_t blue);
RGBColor scale(const RGBColor& value, float amount);
RGBColor blend(const RGBColor& from, const RGBColor& to, float amount);
RGBColor add(const RGBColor& base, const RGBColor& overlay);
RGBColor withBrightness(const RGBColor& value, uint8_t brightness);

void fill(RGBColor* leds, size_t count, const RGBColor& value);
void fillGradient(RGBColor* leds,
				  size_t count,
				  const RGBColor& start,
				  const RGBColor& end,
				  float offset = 0.0f);
void addSweep(RGBColor* leds,
			  size_t count,
			  float center,
			  float width,
			  const RGBColor& value,
			  float intensity = 1.0f);

}  // namespace patterns
}  // namespace led
