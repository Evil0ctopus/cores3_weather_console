#include "settings.h"

#include <Preferences.h>

namespace app {
namespace {

const char* kNamespace = "fliccfg";
const char* kKeyLocationQuery = "loc_query";
const char* kKeyLocationKey = "loc_key";
const char* kKeyLocationName = "loc_name";
const char* kKeyApiKey = "api_key";
const char* kKeyWifiSsid = "wifi_ssid";
const char* kKeyWifiPassword = "wifi_pass";
const char* kKeyWifiAutoConnect = "wifi_auto";
const char* kKeyUnits = "units";
const char* kKeyTheme = "theme";
const char* kKeyRadarMode = "radar_mode";
const char* kKeyRadarAutoContrast = "radar_auto";
const char* kKeyRadarInterpolation = "radar_interp";
const char* kKeyRadarOverlays = "radar_cell";
const char* kKeyRadarInterpSteps = "radar_istep";
const char* kKeyRadarSmoothPasses = "radar_smooth";
const char* kKeyDebugMode = "debug_mode";
const char* kKeyUpdateMinutes = "upd_min";

String normalized(const String& value) {
	String out = value;
	out.trim();
	return out;
}

uint8_t clampRadarInterpolationSteps(uint8_t steps) {
	if (steps > 3U) {
		return 3U;
	}
	return steps;
}

uint8_t clampRadarSmoothingPasses(uint8_t passes) {
	if (passes > 3U) {
		return 3U;
	}
	return passes;
}

bool isValidUnitsRaw(uint8_t raw) {
	return raw <= static_cast<uint8_t>(UnitsSystem::Imperial);
}

bool isValidThemeRaw(uint8_t raw) {
	return raw < ui::theme_count();
}

bool isValidRadarModeRaw(uint8_t raw) {
	return raw <= static_cast<uint8_t>(RadarMode::EchoTop);
}

}  // namespace

bool SettingsStore::begin() {
	initialized_ = true;
	settings_ = defaults();
	return loadFromNvs();
}

bool SettingsStore::reload() {
	if (!initialized_) {
		return begin();
	}
	return loadFromNvs();
}

bool SettingsStore::save(const AppSettings& settings) {
	Preferences prefs;
	if (!prefs.begin(kNamespace, false)) {
		return false;
	}

	AppSettings clean = settings;
	String error;
	if (!validateSettings(clean, error, false)) {
		prefs.end();
		return false;
	}

	prefs.putString(kKeyLocationQuery, clean.locationQuery);
	prefs.putString(kKeyLocationKey, clean.locationKey);
	prefs.putString(kKeyLocationName, clean.locationName);
	prefs.putString(kKeyApiKey, clean.apiKey);
	prefs.putString(kKeyWifiSsid, clean.wifiSsid);
	prefs.putString(kKeyWifiPassword, clean.wifiPassword);
	prefs.putBool(kKeyWifiAutoConnect, clean.wifiAutoConnect);
	prefs.putUChar(kKeyUnits, static_cast<uint8_t>(clean.units));
	prefs.putUChar(kKeyTheme, static_cast<uint8_t>(clean.theme));
	prefs.putUChar(kKeyRadarMode, static_cast<uint8_t>(clean.radarMode));
	prefs.putBool(kKeyRadarAutoContrast, clean.radarAutoContrast);
	prefs.putBool(kKeyRadarInterpolation, clean.radarInterpolation);
	prefs.putBool(kKeyRadarOverlays, clean.radarStormOverlays);
	prefs.putUChar(kKeyRadarInterpSteps, clampRadarInterpolationSteps(clean.radarInterpolationSteps));
	prefs.putUChar(kKeyRadarSmoothPasses, clampRadarSmoothingPasses(clean.radarSmoothingPasses));
	prefs.putBool(kKeyDebugMode, clean.debugMode);
	prefs.putUInt(kKeyUpdateMinutes, clean.updateIntervalMinutes);
	prefs.end();

	settings_ = clean;
	++revision_;
	return true;
}

bool SettingsStore::saveWifiSettings(const String& ssid, const String& password, bool autoConnect) {
	Preferences prefs;
	if (!prefs.begin(kNamespace, false)) {
		return false;
	}

	settings_.wifiSsid = normalized(ssid);
	settings_.wifiPassword = password;
	settings_.wifiAutoConnect = autoConnect;
	prefs.putString(kKeyWifiSsid, settings_.wifiSsid);
	prefs.putString(kKeyWifiPassword, settings_.wifiPassword);
	prefs.putBool(kKeyWifiAutoConnect, settings_.wifiAutoConnect);
	prefs.end();
	++revision_;
	return true;
}

const AppSettings& SettingsStore::settings() const {
	return settings_;
}

ui::ThemeId SettingsStore::get_theme() const {
	return settings_.theme;
}

bool SettingsStore::set_theme(ui::ThemeId theme) {
	if (!initialized_) {
		begin();
	}
	AppSettings next = settings_;
	next.theme = theme;
	return save(next);
}

uint32_t SettingsStore::revision() const {
	return revision_;
}

AppSettings SettingsStore::defaults() {
	AppSettings settings;
	settings.locationQuery = "";
	settings.locationKey = "";
	settings.locationName = "";
	settings.apiKey = "";
	settings.wifiSsid = "";
	settings.wifiPassword = "";
	settings.wifiAutoConnect = true;
	settings.units = UnitsSystem::Metric;
	settings.theme = ui::ThemeId::PIXEL_STORM;
	settings.radarMode = RadarMode::BaseReflectivity;
	settings.radarAutoContrast = true;
	settings.radarInterpolation = true;
	settings.radarStormOverlays = true;
	settings.radarInterpolationSteps = 1;
	settings.radarSmoothingPasses = 1;
	settings.debugMode = false;
	settings.updateIntervalMinutes = 5;
	settings.valid = false;
	return settings;
}

uint32_t SettingsStore::clampUpdateIntervalMinutes(uint32_t minutes) {
	if (minutes < 1U) {
		return 1U;
	}
	if (minutes > 60U) {
		return 60U;
	}
	return minutes;
}

bool SettingsStore::loadFromNvs() {
	Preferences prefs;
	if (!prefs.begin(kNamespace, true)) {
		settings_ = defaults();
		return false;
	}

	AppSettings loaded = defaults();
	loaded.locationQuery = normalized(prefs.getString(kKeyLocationQuery, loaded.locationQuery));
	loaded.locationKey = normalized(prefs.getString(kKeyLocationKey, loaded.locationKey));
	loaded.locationName = normalized(prefs.getString(kKeyLocationName, loaded.locationName));
	loaded.apiKey = normalized(prefs.getString(kKeyApiKey, loaded.apiKey));
	loaded.wifiSsid = normalized(prefs.getString(kKeyWifiSsid, loaded.wifiSsid));
	loaded.wifiPassword = prefs.getString(kKeyWifiPassword, loaded.wifiPassword);
	loaded.wifiAutoConnect = prefs.getBool(kKeyWifiAutoConnect, loaded.wifiAutoConnect);
	const uint8_t unitsRaw = prefs.getUChar(kKeyUnits, static_cast<uint8_t>(loaded.units));
	const uint8_t themeRaw = prefs.getUChar(kKeyTheme, static_cast<uint8_t>(loaded.theme));
	const uint8_t radarRaw = prefs.getUChar(kKeyRadarMode, static_cast<uint8_t>(loaded.radarMode));
	loaded.units = isValidUnitsRaw(unitsRaw) ? static_cast<UnitsSystem>(unitsRaw) : UnitsSystem::Metric;
	loaded.theme = isValidThemeRaw(themeRaw) ? static_cast<ui::ThemeId>(themeRaw) : ui::ThemeId::PIXEL_STORM;
	loaded.radarMode = isValidRadarModeRaw(radarRaw) ? static_cast<RadarMode>(radarRaw) : RadarMode::BaseReflectivity;
	loaded.radarAutoContrast = prefs.getBool(kKeyRadarAutoContrast, loaded.radarAutoContrast);
	loaded.radarInterpolation = prefs.getBool(kKeyRadarInterpolation, loaded.radarInterpolation);
	loaded.radarStormOverlays = prefs.getBool(kKeyRadarOverlays, loaded.radarStormOverlays);
	loaded.radarInterpolationSteps = clampRadarInterpolationSteps(prefs.getUChar(kKeyRadarInterpSteps, loaded.radarInterpolationSteps));
	loaded.radarSmoothingPasses = clampRadarSmoothingPasses(prefs.getUChar(kKeyRadarSmoothPasses, loaded.radarSmoothingPasses));
	loaded.debugMode = prefs.getBool(kKeyDebugMode, loaded.debugMode);
	loaded.updateIntervalMinutes = clampUpdateIntervalMinutes(prefs.getUInt(kKeyUpdateMinutes, loaded.updateIntervalMinutes));
	String validationError;
	validateSettings(loaded, validationError, false);
	prefs.end();

	settings_ = loaded;
	return true;
}

String toString(UnitsSystem value) {
	switch (value) {
		case UnitsSystem::Metric:
			return "metric";
		case UnitsSystem::Imperial:
			return "imperial";
	}
	return "metric";
}

String toString(ui::ThemeId value) {
	return String(ui::theme_id_to_storage_key(value));
}

String toString(RadarMode value) {
	switch (value) {
		case RadarMode::BaseReflectivity:
			return "base";
		case RadarMode::CompositeReflectivity:
			return "composite";
		case RadarMode::EchoTop:
			return "echotop";
	}
	return "base";
}

bool parseUnitsSystem(const String& value, UnitsSystem& out) {
	String normalizedValue = value;
	normalizedValue.toLowerCase();
	if (normalizedValue == "metric") {
		out = UnitsSystem::Metric;
		return true;
	}
	if (normalizedValue == "imperial") {
		out = UnitsSystem::Imperial;
		return true;
	}
	return false;
}

bool parseThemeId(const String& value, ui::ThemeId& out) {
	return ui::parse_theme_id(value, out);
}

bool parseRadarMode(const String& value, RadarMode& out) {
	String normalizedValue = value;
	normalizedValue.toLowerCase();
	if (normalizedValue == "base") {
		out = RadarMode::BaseReflectivity;
		return true;
	}
	if (normalizedValue == "composite") {
		out = RadarMode::CompositeReflectivity;
		return true;
	}
	if (normalizedValue == "echotop") {
		out = RadarMode::EchoTop;
		return true;
	}
	return false;
}

void writeSettingsJson(JsonDocument& doc, const AppSettings& settings) {
	doc["locationQuery"] = settings.locationQuery;
	doc["locationKey"] = settings.locationKey;
	doc["locationName"] = settings.locationName;
	doc["apiKey"] = settings.apiKey;
	doc["wifiSsid"] = settings.wifiSsid;
	doc["wifiAutoConnect"] = settings.wifiAutoConnect;
	doc["units"] = toString(settings.units);
	doc["theme"] = toString(settings.theme);
	doc["radarMode"] = toString(settings.radarMode);
	doc["radarAutoContrast"] = settings.radarAutoContrast;
	doc["radarInterpolation"] = settings.radarInterpolation;
	doc["radarStormOverlays"] = settings.radarStormOverlays;
	doc["radarInterpolationSteps"] = settings.radarInterpolationSteps;
	doc["radarSmoothingPasses"] = settings.radarSmoothingPasses;
	doc["debugMode"] = settings.debugMode;
	doc["updateIntervalMinutes"] = settings.updateIntervalMinutes;
	doc["valid"] = settings.valid;
}

bool readSettingsJson(JsonVariantConst source,
					  AppSettings& out,
					  String& outError,
					  const AppSettings* fallback) {
	AppSettings next = fallback != nullptr ? *fallback : SettingsStore::defaults();

	const JsonVariantConst locationQueryVar = source["locationQuery"];
	if (!locationQueryVar.isNull()) {
		if (!locationQueryVar.is<const char*>()) {
			outError = "locationQuery must be a string.";
			return false;
		}
		next.locationQuery = String(locationQueryVar.as<const char*>());
	}

	const JsonVariantConst locationKeyVar = source["locationKey"];
	if (!locationKeyVar.isNull()) {
		if (!locationKeyVar.is<const char*>()) {
			outError = "locationKey must be a string.";
			return false;
		}
		next.locationKey = String(locationKeyVar.as<const char*>());
	}

	const JsonVariantConst locationNameVar = source["locationName"];
	if (!locationNameVar.isNull()) {
		if (!locationNameVar.is<const char*>()) {
			outError = "locationName must be a string.";
			return false;
		}
		next.locationName = String(locationNameVar.as<const char*>());
	}

	const JsonVariantConst apiKeyVar = source["apiKey"];
	if (!apiKeyVar.isNull()) {
		if (!apiKeyVar.is<const char*>()) {
			outError = "apiKey must be a string.";
			return false;
		}
		next.apiKey = String(apiKeyVar.as<const char*>());
	}

	const JsonVariantConst wifiSsidVar = source["wifiSsid"];
	if (!wifiSsidVar.isNull()) {
		if (!wifiSsidVar.is<const char*>()) {
			outError = "wifiSsid must be a string.";
			return false;
		}
		next.wifiSsid = String(wifiSsidVar.as<const char*>());
	}

	const JsonVariantConst wifiPasswordVar = source["wifiPassword"];
	if (!wifiPasswordVar.isNull()) {
		if (!wifiPasswordVar.is<const char*>()) {
			outError = "wifiPassword must be a string.";
			return false;
		}
		next.wifiPassword = String(wifiPasswordVar.as<const char*>());
	}

	const JsonVariantConst wifiAutoConnectVar = source["wifiAutoConnect"];
	if (!wifiAutoConnectVar.isNull()) {
		if (!wifiAutoConnectVar.is<bool>()) {
			outError = "wifiAutoConnect must be a boolean.";
			return false;
		}
		next.wifiAutoConnect = wifiAutoConnectVar.as<bool>();
	}

	const JsonVariantConst unitsVar = source["units"];
	if (!unitsVar.isNull()) {
		if (!unitsVar.is<const char*>()) {
			outError = "units must be a string.";
			return false;
		}
		UnitsSystem parsedUnits = next.units;
		if (!parseUnitsSystem(String(unitsVar.as<const char*>()), parsedUnits)) {
			outError = "Units selection is invalid.";
			return false;
		}
		next.units = parsedUnits;
	}

	const JsonVariantConst themeVar = source["theme"];
	if (!themeVar.isNull()) {
		if (!themeVar.is<const char*>()) {
			outError = "theme must be a string.";
			return false;
		}
		ui::ThemeId parsedTheme = next.theme;
		if (!parseThemeId(String(themeVar.as<const char*>()), parsedTheme)) {
			outError = "Theme selection is invalid.";
			return false;
		}
		next.theme = parsedTheme;
	}

	const JsonVariantConst radarModeVar = source["radarMode"];
	if (!radarModeVar.isNull()) {
		if (!radarModeVar.is<const char*>()) {
			outError = "radarMode must be a string.";
			return false;
		}
		RadarMode parsedRadarMode = next.radarMode;
		if (!parseRadarMode(String(radarModeVar.as<const char*>()), parsedRadarMode)) {
			outError = "Radar mode selection is invalid.";
			return false;
		}
		next.radarMode = parsedRadarMode;
	}

	const JsonVariantConst radarAutoContrastVar = source["radarAutoContrast"];
	if (!radarAutoContrastVar.isNull()) {
		if (!radarAutoContrastVar.is<bool>()) {
			outError = "radarAutoContrast must be a boolean.";
			return false;
		}
		next.radarAutoContrast = radarAutoContrastVar.as<bool>();
	}

	const JsonVariantConst radarInterpolationVar = source["radarInterpolation"];
	if (!radarInterpolationVar.isNull()) {
		if (!radarInterpolationVar.is<bool>()) {
			outError = "radarInterpolation must be a boolean.";
			return false;
		}
		next.radarInterpolation = radarInterpolationVar.as<bool>();
	}

	const JsonVariantConst radarStormOverlaysVar = source["radarStormOverlays"];
	if (!radarStormOverlaysVar.isNull()) {
		if (!radarStormOverlaysVar.is<bool>()) {
			outError = "radarStormOverlays must be a boolean.";
			return false;
		}
		next.radarStormOverlays = radarStormOverlaysVar.as<bool>();
	}

	const JsonVariantConst radarInterpolationStepsVar = source["radarInterpolationSteps"];
	if (!radarInterpolationStepsVar.isNull()) {
		if (!radarInterpolationStepsVar.is<int>() && !radarInterpolationStepsVar.is<uint32_t>() && !radarInterpolationStepsVar.is<const char*>()) {
			outError = "radarInterpolationSteps must be numeric.";
			return false;
		}
		uint8_t value = 0;
		if (radarInterpolationStepsVar.is<const char*>()) {
			value = static_cast<uint8_t>(String(radarInterpolationStepsVar.as<const char*>()).toInt());
		} else {
			value = static_cast<uint8_t>(radarInterpolationStepsVar.as<int>());
		}
		next.radarInterpolationSteps = clampRadarInterpolationSteps(value);
	}

	const JsonVariantConst radarSmoothingPassesVar = source["radarSmoothingPasses"];
	if (!radarSmoothingPassesVar.isNull()) {
		if (!radarSmoothingPassesVar.is<int>() && !radarSmoothingPassesVar.is<uint32_t>() && !radarSmoothingPassesVar.is<const char*>()) {
			outError = "radarSmoothingPasses must be numeric.";
			return false;
		}
		uint8_t value = 0;
		if (radarSmoothingPassesVar.is<const char*>()) {
			value = static_cast<uint8_t>(String(radarSmoothingPassesVar.as<const char*>()).toInt());
		} else {
			value = static_cast<uint8_t>(radarSmoothingPassesVar.as<int>());
		}
		next.radarSmoothingPasses = clampRadarSmoothingPasses(value);
	}

	const JsonVariantConst debugModeVar = source["debugMode"];
	if (!debugModeVar.isNull()) {
		if (!debugModeVar.is<bool>()) {
			outError = "debugMode must be a boolean.";
			return false;
		}
		next.debugMode = debugModeVar.as<bool>();
	}

	const JsonVariantConst updateIntervalVar = source["updateIntervalMinutes"];
	if (!updateIntervalVar.isNull()) {
		if (!updateIntervalVar.is<uint32_t>() && !updateIntervalVar.is<int>() && !updateIntervalVar.is<const char*>()) {
			outError = "updateIntervalMinutes must be numeric.";
			return false;
		}
		uint32_t value = 0;
		if (updateIntervalVar.is<const char*>()) {
			value = static_cast<uint32_t>(String(updateIntervalVar.as<const char*>()).toInt());
		} else {
			value = static_cast<uint32_t>(updateIntervalVar.as<int>());
		}
		next.updateIntervalMinutes = SettingsStore::clampUpdateIntervalMinutes(value);
	}

	out = next;
	outError = "";
	return true;
}

bool validateSettings(AppSettings& settings, String& outError, bool requireResolvedLocationKey) {
	settings.locationQuery = normalized(settings.locationQuery);
	settings.locationKey = normalized(settings.locationKey);
	settings.locationName = normalized(settings.locationName);
	settings.apiKey = normalized(settings.apiKey);
	settings.wifiSsid = normalized(settings.wifiSsid);
	settings.updateIntervalMinutes = SettingsStore::clampUpdateIntervalMinutes(settings.updateIntervalMinutes);
	settings.radarInterpolationSteps = clampRadarInterpolationSteps(settings.radarInterpolationSteps);
	settings.radarSmoothingPasses = clampRadarSmoothingPasses(settings.radarSmoothingPasses);

	if (settings.locationQuery.length() == 0) {
		outError = "Location is required.";
		settings.valid = false;
		return false;
	}
	const bool requiresResolvedLocationKey = requireResolvedLocationKey && settings.apiKey.length() > 0;
	if (requiresResolvedLocationKey && settings.locationKey.length() == 0) {
		outError = "Resolved location key is required.";
		settings.valid = false;
		return false;
	}

	settings.valid = settings.locationQuery.length() > 0 && (settings.apiKey.length() == 0 || settings.locationKey.length() > 0);
	outError = "";
	return true;
}

}  // namespace app
