#include "control_settings.h"
#include "quiet_hours.h"

namespace app {

const BoolControl kBoolControls[] = {
	{"soundMuted", "Mute all sound", ControlGroup::Sound, &ControlSettings::soundMuted},
	{"soundTouch", "Touch sounds", ControlGroup::Sound, &ControlSettings::soundTouch},
	{"soundNavigation", "Page sounds", ControlGroup::Sound, &ControlSettings::soundNavigation},
	{"soundStartup", "Startup sounds", ControlGroup::Sound, &ControlSettings::soundStartup},
	{"soundAlerts", "Weather alert sounds", ControlGroup::Sound, &ControlSettings::soundAlerts},
	{"soundSystem", "System sounds", ControlGroup::Sound, &ControlSettings::soundSystem},
	{"ledTouch", "Touch LED feedback", ControlGroup::Leds, &ControlSettings::ledTouch},
	{"ledNavigation", "Page LED feedback", ControlGroup::Leds, &ControlSettings::ledNavigation},
	{"ledAlerts", "Allow alert LEDs even if Off", ControlGroup::Leds, &ControlSettings::ledAlerts},
	{"ledLightning", "Soft storm lightning", ControlGroup::Leds, &ControlSettings::ledLightning},
	{"displayAnimations", "Background animations", ControlGroup::Display, &ControlSettings::displayAnimations},
	{"quietEnabled", "Enable quiet hours", ControlGroup::Quiet, &ControlSettings::quietEnabled},
	{"quietAlertOverride", "Alerts override quiet hours", ControlGroup::Quiet, &ControlSettings::quietAlertOverride},
};
const size_t kBoolControlCount = sizeof(kBoolControls) / sizeof(kBoolControls[0]);
const NumberControl kNumberControls[] = {
	{"soundVolume", "Volume (%)", ControlGroup::Sound, &ControlSettings::soundVolume, 0, 100},
	{"ledBrightness", "LED brightness (%)", ControlGroup::Leds, &ControlSettings::ledBrightness, 0, 100},
	{"ledSpeed", "Effect speed (%)", ControlGroup::Leds, &ControlSettings::ledSpeed, 1, 100},
	{"displayBrightness", "Screen brightness (%)", ControlGroup::Display, &ControlSettings::displayBrightness, 5, 100},
	{"displayDimSeconds", "Dim after seconds (0=never)", ControlGroup::Display, &ControlSettings::displayDimSeconds, 0, 3600},
	{"displayNightBrightness", "Dim/night brightness (%)", ControlGroup::Display, &ControlSettings::displayNightBrightness, 5, 100},
	{"quietStartHour", "Quiet start (local hour 0-23)", ControlGroup::Quiet, &ControlSettings::quietStartHour, 0, 23},
	{"quietEndHour", "Quiet end (local hour 0-23)", ControlGroup::Quiet, &ControlSettings::quietEndHour, 0, 23},
};
const size_t kNumberControlCount = sizeof(kNumberControls) / sizeof(kNumberControls[0]);
const ChoiceControl kChoiceControls[] = {
	{"soundPack", "Sound pack", ControlGroup::Sound, &ControlSettings::soundPack,
	 "default\nmodern\nretro\nminimal", "Default\nModern\nRetro\nMinimal"},
	{"ledMode", "LED mode", ControlGroup::Leds, &ControlSettings::ledMode,
	 "weather\nsolid\nrainbow\nbreathing\noff", "Active Weather\nSolid Color\nFlowing Rainbow\nBreathing\nOff"},
	{"ledColor", "Solid / breathing color", ControlGroup::Leds, &ControlSettings::ledColor,
	 "red\norange\nyellow\ngreen\ncyan\nblue\npurple\nwhite",
	 "Red\nOrange\nYellow\nGreen\nCyan\nBlue\nPurple\nWhite"},
};
const size_t kChoiceControlCount = sizeof(kChoiceControls) / sizeof(kChoiceControls[0]);

String choiceAt(const char* options, uint16_t index) {
	String list(options);
	int start = 0;
	while (index-- > 0) {
		start = list.indexOf('\n', start);
		if (start < 0) return String();
		++start;
	}
	const int end = list.indexOf('\n', start);
	return end < 0 ? list.substring(start) : list.substring(start, end);
}

int choiceIndex(const char* options, const String& value) {
	for (uint16_t i = 0; ; ++i) {
		const String item = choiceAt(options, i);
		if (item.length() == 0) return -1;
		if (item == value) return i;
	}
}

void writeControlSettings(JsonDocument& doc, const ControlSettings& settings) {
	for (size_t i = 0; i < kBoolControlCount; ++i) doc[kBoolControls[i].key] = settings.*kBoolControls[i].member;
	for (size_t i = 0; i < kNumberControlCount; ++i) doc[kNumberControls[i].key] = settings.*kNumberControls[i].member;
	for (size_t i = 0; i < kChoiceControlCount; ++i) doc[kChoiceControls[i].key] = settings.*kChoiceControls[i].member;
}

bool validateControlSettings(const ControlSettings& settings, String& error) {
	for (size_t i = 0; i < kNumberControlCount; ++i) {
		const auto& field = kNumberControls[i];
		const uint32_t value = settings.*field.member;
		if (value < field.min || value > field.max) {
			error = String(field.key) + " is out of range.";
			return false;
		}
	}
	for (size_t i = 0; i < kChoiceControlCount; ++i) {
		const auto& field = kChoiceControls[i];
		if (choiceIndex(field.values, settings.*field.member) < 0) {
			error = String(field.key) + " is invalid.";
			return false;
		}
	}
	error = "";
	return true;
}

bool readControlSettings(JsonVariantConst source, ControlSettings& settings, String& error) {
	ControlSettings next = settings;
	for (size_t i = 0; i < kBoolControlCount; ++i) {
		const auto& field = kBoolControls[i];
		JsonVariantConst value = source[field.key];
		if (value.isNull()) continue;
		if (!value.is<bool>()) {
			error = String(field.key) + " must be a boolean.";
			return false;
		}
		next.*field.member = value.as<bool>();
	}
	for (size_t i = 0; i < kNumberControlCount; ++i) {
		const auto& field = kNumberControls[i];
		JsonVariantConst value = source[field.key];
		if (value.isNull()) continue;
		if (!value.is<uint32_t>()) {
			error = String(field.key) + " must be a non-negative integer.";
			return false;
		}
		next.*field.member = value.as<uint32_t>();
	}
	for (size_t i = 0; i < kChoiceControlCount; ++i) {
		const auto& field = kChoiceControls[i];
		JsonVariantConst value = source[field.key];
		if (value.isNull()) continue;
		if (!value.is<const char*>()) {
			error = String(field.key) + " must be a string.";
			return false;
		}
		next.*field.member = value.as<String>();
	}
	if (!validateControlSettings(next, error)) return false;
	settings = next;
	return true;
}

bool quietHoursActive(const ControlSettings& settings, int localHour) {
	return quietHoursInRange(settings.quietEnabled, settings.quietStartHour, settings.quietEndHour, localHour);
}

}  // namespace app
