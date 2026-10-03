#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace app {

enum class ControlGroup : uint8_t { Sound, Leds, Display, Quiet };

struct ControlSettings {
	uint32_t soundVolume = 78;
	bool soundMuted = false;
	String soundPack = "default";
	bool soundTouch = true;
	bool soundNavigation = true;
	bool soundStartup = true;
	bool soundAlerts = true;
	bool soundSystem = true;
	String ledMode = "weather";
	String ledColor = "cyan";
	uint32_t ledBrightness = 63;
	uint32_t ledSpeed = 35;
	bool ledTouch = true;
	bool ledNavigation = true;
	bool ledAlerts = true;
	bool ledLightning = true;
	uint32_t displayBrightness = 50;
	uint32_t displayDimSeconds = 0;
	uint32_t displayNightBrightness = 15;
	bool displayAnimations = true;
	bool quietEnabled = false;
	uint32_t quietStartHour = 22;
	uint32_t quietEndHour = 7;
	bool quietAlertOverride = true;
};

struct BoolControl {
	const char* key;
	const char* label;
	ControlGroup group;
	bool ControlSettings::*member;
};
struct NumberControl {
	const char* key;
	const char* label;
	ControlGroup group;
	uint32_t ControlSettings::*member;
	uint32_t min;
	uint32_t max;
};
struct ChoiceControl {
	const char* key;
	const char* label;
	ControlGroup group;
	String ControlSettings::*member;
	const char* values;
	const char* labels;
};

extern const BoolControl kBoolControls[];
extern const size_t kBoolControlCount;
extern const NumberControl kNumberControls[];
extern const size_t kNumberControlCount;
extern const ChoiceControl kChoiceControls[];
extern const size_t kChoiceControlCount;

String choiceAt(const char* options, uint16_t index);
int choiceIndex(const char* options, const String& value);
void writeControlSettings(JsonDocument& doc, const ControlSettings& settings);
bool readControlSettings(JsonVariantConst source, ControlSettings& settings, String& error);
bool validateControlSettings(const ControlSettings& settings, String& error);
bool quietHoursActive(const ControlSettings& settings, int localHour);

}  // namespace app
