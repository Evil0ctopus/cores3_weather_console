#pragma once

#include <Arduino.h>
#include <math.h>

namespace ui {

constexpr float display_temperature(float celsius, bool imperial) {
	return imperial ? celsius * 1.8f + 32.0f : celsius;
}

constexpr float display_wind(float kph, bool imperial) {
	return imperial ? kph / 1.609344f : kph;
}

constexpr float display_pressure(float millibars, bool imperial) {
	return imperial ? millibars / 33.86389f : millibars;
}

inline String format_temperature(float celsius, bool imperial, unsigned decimals = 0) {
	return (isnan(celsius) ? String("--") : String(display_temperature(celsius, imperial), decimals)) +
		(imperial ? " F" : " C");
}

inline String format_wind(float kph, bool imperial) {
	return (isnan(kph) ? String("--") : String(display_wind(kph, imperial), 0)) +
		(imperial ? " mph" : " kph");
}

inline String format_pressure(float millibars, bool imperial) {
	return (isnan(millibars) ? String("----") :
		String(display_pressure(millibars, imperial), imperial ? 2 : 0)) +
		(imperial ? " inHg" : " mb");
}

}  // namespace ui
