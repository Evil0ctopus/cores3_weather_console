#pragma once

#include <Arduino.h>

constexpr size_t kMaxForecastDays = 7;
constexpr size_t kMaxWeatherAlerts = 8;
constexpr size_t kMaxRadarFrames = 12;

enum class WeatherErrorCode : uint8_t {
	None = 0,
	NotConfigured,
	WifiDisconnected,
	HttpBeginFailed,
	HttpStatusError,
	JsonParseError,
	ResponseShapeError,
};

struct CurrentConditions {
	bool valid = false;
	uint32_t observedEpoch = 0;
	String summary;
	int icon = -1;
	bool isDaylight = true;
	float temperatureC = NAN;
	float feelsLikeC = NAN;
	float windKph = NAN;
	float pressureMb = NAN;
	int humidityPct = -1;
};

struct DailyForecast {
	bool valid = false;
	uint32_t epochDate = 0;
	String dayLabel;
	String summary;
	int dayIcon = -1;
	int nightIcon = -1;
	float minTempC = NAN;
	float maxTempC = NAN;
	int precipChancePct = -1;
};

struct WeatherAlert {
	bool valid = false;
	String id;
	String category;
	String severity;
	String title;
	String description;
	uint32_t onsetEpoch = 0;
	uint32_t expiresEpoch = 0;
};

struct RadarTileFrame {
	bool valid = false;
	uint32_t epochTime = 0;
	String url;
};

struct WeatherData {
	String provider = "AccuWeatherStyle";
	String locationKey;
	String locationName;
	String timezoneId;

	CurrentConditions current;

	DailyForecast forecast[kMaxForecastDays];
	size_t forecastCount = 0;

	WeatherAlert alerts[kMaxWeatherAlerts];
	size_t alertCount = 0;

	RadarTileFrame radarFrames[kMaxRadarFrames];
	size_t radarFrameCount = 0;

	uint32_t currentFetchedAtMs = 0;
	uint32_t forecastFetchedAtMs = 0;
	uint32_t alertsFetchedAtMs = 0;
	uint32_t radarFetchedAtMs = 0;
	uint32_t lastCycleAtMs = 0;

	WeatherErrorCode lastError = WeatherErrorCode::None;
	int lastHttpStatus = 0;
	String lastErrorMessage;
};
