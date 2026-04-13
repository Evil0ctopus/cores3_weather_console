#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace app {

enum class UnitsSystem : uint8_t {
	Metric = 0,
	Imperial,
};

enum class ThemePreference : uint8_t {
	MatchDevice = 0,
	Light,
	Dark,
	FutureCustom1,
};

enum class RadarMode : uint8_t {
	BaseReflectivity = 0,
	CompositeReflectivity,
	EchoTop,
};

struct AppSettings {
	String locationQuery;
	String locationKey;
	String locationName;
	String apiKey;
	String wifiSsid;
	String wifiPassword;
	bool wifiAutoConnect = true;
	UnitsSystem units = UnitsSystem::Metric;
	ThemePreference theme = ThemePreference::MatchDevice;
	RadarMode radarMode = RadarMode::BaseReflectivity;
	bool radarAutoContrast = true;
	bool radarInterpolation = true;
	bool radarStormOverlays = true;
	uint8_t radarInterpolationSteps = 1;
	uint8_t radarSmoothingPasses = 1;
	bool debugMode = false;
	uint32_t updateIntervalMinutes = 5;
	bool valid = false;
};

class SettingsStore {
 public:
	bool begin();
	bool reload();
	bool save(const AppSettings& settings);
	bool saveWifiSettings(const String& ssid, const String& password, bool autoConnect);

	const AppSettings& settings() const;
	uint32_t revision() const;

	static AppSettings defaults();
	static uint32_t clampUpdateIntervalMinutes(uint32_t minutes);

 private:
	bool loadFromNvs();

	AppSettings settings_{};
	uint32_t revision_ = 0;
	bool initialized_ = false;
};

String toString(UnitsSystem value);
String toString(ThemePreference value);
String toString(RadarMode value);

bool parseUnitsSystem(const String& value, UnitsSystem& out);
bool parseThemePreference(const String& value, ThemePreference& out);
bool parseRadarMode(const String& value, RadarMode& out);

void writeSettingsJson(JsonDocument& doc, const AppSettings& settings);
bool readSettingsJson(JsonVariantConst source,
					  AppSettings& out,
					  String& outError,
					  const AppSettings* fallback = nullptr);
bool validateSettings(AppSettings& settings, String& outError, bool requireResolvedLocationKey);

}  // namespace app
