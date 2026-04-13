#pragma once

#include <Arduino.h>

#include "weather_models.h"

namespace weather {

struct WeatherApiConfig {
	String apiKey;
	String locationQuery;
	String locationKey;
	String baseUrl = "http://dataservice.accuweather.com";
	String language = "en-us";
	bool useMetric = true;

	uint32_t currentCacheMs = 5UL * 60UL * 1000UL;
	uint32_t forecastCacheMs = 30UL * 60UL * 1000UL;
	uint32_t alertsCacheMs = 5UL * 60UL * 1000UL;
	uint32_t radarCacheMs = 10UL * 60UL * 1000UL;
	uint32_t minRequestGapMs = 250UL;
};

class WeatherApi {
 public:
	WeatherApi();

	bool begin(const WeatherApiConfig& config);
	void requestRefresh();
	void update();

	const WeatherData& data() const;
	bool hasValidCurrent() const;
	bool hasAnyError() const;

	size_t copyRadarFrames(String* outUrls, uint32_t* outEpochs, size_t maxFrames) const;
	bool consumeRadarChanged();

 private:
	enum class UpdateTask : uint8_t {
		Idle = 0,
		Current,
		Forecast,
		Alerts,
		Radar,
	};

	bool isConfigured() const;
	bool usingOpenMeteo() const;
	bool ensureOpenMeteoLocationResolved();
	bool parseLocationCoordinates(float& outLat, float& outLon) const;
	String buildOpenMeteoForecastUrl() const;
	String openMeteoSummaryFromCode(int weatherCode) const;
	int openMeteoIconFromCode(int weatherCode, bool isDaylight) const;
	String buildAuthQuery() const;

	bool fetchJson(const String& url, String& payload, int& httpStatus);
	bool parseCurrent(const String& payload);
	bool parseForecast(const String& payload);
	bool parseAlerts(const String& payload);
	bool parseRadar(const String& payload);

	void setError(WeatherErrorCode code, const String& message, int httpStatus = 0);
	void clearError();
	UpdateTask chooseTask(uint32_t nowMs) const;

	WeatherApiConfig config_;
	WeatherData data_;

	bool forceRefresh_ = true;
	bool radarChanged_ = false;
	bool configured_ = false;

	uint32_t lastRequestAtMs_ = 0;
	UpdateTask lastTask_ = UpdateTask::Idle;
};

}  // namespace weather
