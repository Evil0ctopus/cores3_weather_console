#include "weather_api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <stdlib.h>

namespace weather {
namespace {

String lower(const String& in) {
	String out = in;
	out.toLowerCase();
	return out;
}

String urlEncode(const String& value) {
	String out;
	out.reserve(value.length() * 3);
	for (size_t i = 0; i < value.length(); ++i) {
		const char c = value[i];
		const bool unreserved =
			(c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~';
		if (unreserved) {
			out += c;
		} else if (c == ' ') {
			out += "%20";
		} else {
			char encoded[4];
			snprintf(encoded, sizeof(encoded), "%%%02X", static_cast<unsigned char>(c));
			out += encoded;
		}
	}
	return out;
}

float asMetricTemperature(float value, const String& unit) {
	const String u = lower(unit);
	if (u == "f") {
		return (value - 32.0f) * (5.0f / 9.0f);
	}
	return value;
}

int parseLocalMinutes(const String& isoText) {
	const int timeStart = isoText.indexOf('T') >= 0 ? isoText.indexOf('T') + 1 : 0;
	if (isoText.length() < (timeStart + 5)) {
		return -1;
	}
	const int colonIndex = isoText.indexOf(':', timeStart);
	if (colonIndex < 0) {
		return -1;
	}
	const int hour = isoText.substring(timeStart, colonIndex).toInt();
	const int minute = isoText.substring(colonIndex + 1, colonIndex + 3).toInt();
	if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
		return -1;
	}
	return (hour * 60) + minute;
}

int parseTimezoneOffsetMinutes(const String& isoText) {
	if (isoText.length() >= 6 && isoText[isoText.length() - 3] == ':') {
		const char signChar = isoText[isoText.length() - 6];
		if (signChar == '+' || signChar == '-') {
			const int hours = isoText.substring(isoText.length() - 5, isoText.length() - 3).toInt();
			const int minutes = isoText.substring(isoText.length() - 2).toInt();
			const int total = hours * 60 + minutes;
			return signChar == '-' ? -total : total;
		}
	}
	return 0;
}

}  // namespace

WeatherApi::WeatherApi() = default;

bool WeatherApi::begin(const WeatherApiConfig& config) {
	config_ = config;
	configured_ = isConfigured();
	forceRefresh_ = true;
	radarChanged_ = false;
	lastRequestAtMs_ = 0;
	nextAllowedAttemptMs_ = 0;
	resolvedLatitude_ = NAN;
	resolvedLongitude_ = NAN;
	data_ = WeatherData();
	data_.locationKey = config_.locationKey;
	data_.locationName = config_.locationQuery;
	data_.provider = usingOpenMeteo() ? "OpenMeteo" : "AccuWeatherStyle";

	if (!configured_) {
		setError(WeatherErrorCode::NotConfigured, "weather location not configured");
		return false;
	}

	clearError();
	return true;
}

void WeatherApi::requestRefresh() {
	forceRefresh_ = true;
}

void WeatherApi::update() {
	if (!configured_) {
		setError(WeatherErrorCode::NotConfigured, "weather module not configured");
		return;
	}

	if (usingOpenMeteo() && !ensureOpenMeteoLocationResolved()) {
		return;
	}
	if (!usingOpenMeteo() && !isnan(resolvedLatitude_) && !isnan(resolvedLongitude_)) {
		data_.locationKey = config_.locationKey;
	}

	if (WiFi.status() != WL_CONNECTED) {
		setError(WeatherErrorCode::WifiDisconnected, "wifi disconnected");
		return;
	}

	const uint32_t now = millis();
	if (nextAllowedAttemptMs_ != 0 && now < nextAllowedAttemptMs_) {
		return;
	}
	if ((now - lastRequestAtMs_) < config_.minRequestGapMs) {
		return;
	}

	const UpdateTask task = chooseTask(now);
	if (task == UpdateTask::Idle) {
		return;
	}

	String endpoint;
	if (task == UpdateTask::Radar) {
		if (!ensureLocationCoordinatesResolved()) {
			return;
		}
		endpoint = "http://api.rainviewer.com/public/weather-maps.json";
	} else if (usingOpenMeteo()) {
		if (task == UpdateTask::Alerts) {
			data_.alertCount = 0;
			data_.alertsFetchedAtMs = now;
			return;
		}
		endpoint = buildOpenMeteoForecastUrl();
	} else if (task == UpdateTask::Current) {
		endpoint = config_.baseUrl + "/currentconditions/v1/" + config_.locationKey +
							 "?details=true&language=" + config_.language + buildAuthQuery();
	} else if (task == UpdateTask::Forecast) {
		endpoint = config_.baseUrl + "/forecasts/v1/daily/5day/" + config_.locationKey +
							 "?details=true&metric=" + String(config_.useMetric ? "true" : "false") +
							 "&language=" + config_.language + buildAuthQuery();
	} else if (task == UpdateTask::Alerts) {
		endpoint = config_.baseUrl + "/alerts/v1/" + config_.locationKey +
							 "?language=" + config_.language + buildAuthQuery();
	} else {
		if (!ensureLocationCoordinatesResolved()) {
			return;
		}
		endpoint = "http://api.rainviewer.com/public/weather-maps.json";
	}

	String payload;
	int httpStatus = 0;
	lastRequestAtMs_ = now;
	lastTask_ = task;

	if (!fetchJson(endpoint, payload, httpStatus)) {
		return;
	}

	bool parsed = false;
	if (task == UpdateTask::Current) {
		if (usingOpenMeteo()) {
			parsed = parseCurrent(payload) && parseForecast(payload);
			if (parsed) {
				data_.currentFetchedAtMs = now;
				data_.forecastFetchedAtMs = now;
			}
		} else {
			parsed = parseCurrent(payload);
			if (parsed) {
				data_.currentFetchedAtMs = now;
			}
		}
	} else if (task == UpdateTask::Forecast) {
		parsed = parseForecast(payload);
		if (parsed) {
			data_.forecastFetchedAtMs = now;
		}
	} else if (task == UpdateTask::Alerts) {
		parsed = parseAlerts(payload);
		if (parsed) {
			data_.alertsFetchedAtMs = now;
		}
	} else if (task == UpdateTask::Radar) {
		parsed = parseRadar(payload);
		if (parsed) {
			data_.radarFetchedAtMs = now;
			radarChanged_ = true;
		}
	}

	if (parsed) {
		forceRefresh_ = false;
		data_.lastCycleAtMs = now;
		clearError();
	}
}

const WeatherData& WeatherApi::data() const {
	return data_;
}

bool WeatherApi::hasValidCurrent() const {
	return data_.current.valid;
}

bool WeatherApi::hasAnyError() const {
	return data_.lastError != WeatherErrorCode::None;
}

size_t WeatherApi::copyRadarFrames(String* outUrls, uint32_t* outEpochs, size_t maxFrames) const {
	if (outUrls == nullptr || maxFrames == 0) {
		return 0;
	}
	const size_t count = data_.radarFrameCount < maxFrames ? data_.radarFrameCount : maxFrames;
	for (size_t i = 0; i < count; ++i) {
		outUrls[i] = data_.radarFrames[i].url;
		if (outEpochs != nullptr) {
			outEpochs[i] = data_.radarFrames[i].epochTime;
		}
	}
	return count;
}

bool WeatherApi::consumeRadarChanged() {
	const bool changed = radarChanged_;
	radarChanged_ = false;
	return changed;
}

bool WeatherApi::isConfigured() const {
	if (usingOpenMeteo()) {
		return config_.locationQuery.length() > 0;
	}
	return config_.apiKey.length() > 0 && config_.locationKey.length() > 0;
}

bool WeatherApi::usingOpenMeteo() const {
	return config_.apiKey.length() == 0;
}

bool WeatherApi::ensureLocationCoordinatesResolved() {
	if (!isnan(resolvedLatitude_) && !isnan(resolvedLongitude_)) {
		return updateRadarTileProjection();
	}

	float lat = 0.0f;
	float lon = 0.0f;
	if (parseLocationCoordinates(lat, lon)) {
		resolvedLatitude_ = lat;
		resolvedLongitude_ = lon;
		return updateRadarTileProjection();
	}

	if (config_.locationQuery.length() == 0) {
		setError(WeatherErrorCode::NotConfigured, "weather location not configured");
		return false;
	}

	String payload;
	int httpStatus = 0;
	const String endpoint = "http://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(config_.locationQuery) + "&count=1&language=en&format=json";
	if (!fetchJson(endpoint, payload, httpStatus)) {
		return false;
	}

	JsonDocument doc;
	if (deserializeJson(doc, payload) != DeserializationError::Ok) {
		setError(WeatherErrorCode::JsonParseError, "geocode parse failed");
		return false;
	}

	JsonArray results = doc["results"].as<JsonArray>();
	if (results.isNull() || results.size() == 0) {
		setError(WeatherErrorCode::ResponseShapeError, "geocode results missing");
		return false;
	}

	JsonObject first = results[0].as<JsonObject>();
	resolvedLatitude_ = first["latitude"] | NAN;
	resolvedLongitude_ = first["longitude"] | NAN;
	if (isnan(resolvedLatitude_) || isnan(resolvedLongitude_)) {
		setError(WeatherErrorCode::ResponseShapeError, "geocode coordinates missing");
		return false;
	}

	if (usingOpenMeteo()) {
		config_.locationKey = String(resolvedLatitude_, 4) + "," + String(resolvedLongitude_, 4);
		data_.locationKey = config_.locationKey;
	}
	if (data_.locationName.length() == 0) {
		data_.locationName = String(static_cast<const char*>(first["name"] | ""));
		const String admin1 = String(static_cast<const char*>(first["admin1"] | ""));
		if (admin1.length() > 0) {
			data_.locationName += ", " + admin1;
		}
		if (data_.locationName.length() == 0) {
			data_.locationName = config_.locationQuery;
		}
	}
	return updateRadarTileProjection();
}

bool WeatherApi::updateRadarTileProjection(uint8_t radarZoom, int* outTileX, int* outTileY) {
	if (isnan(resolvedLatitude_) || isnan(resolvedLongitude_)) {
		return false;
	}

	double latitude = static_cast<double>(resolvedLatitude_);
	if (latitude > 85.05112878) {
		latitude = 85.05112878;
	} else if (latitude < -85.05112878) {
		latitude = -85.05112878;
	}

	double longitude = static_cast<double>(resolvedLongitude_);
	const double tileCount = static_cast<double>(1UL << radarZoom);
	double xTileFloat = ((longitude + 180.0) / 360.0) * tileCount;
	const double latRadians = latitude * PI / 180.0;
	double yTileFloat = (1.0 - log(tan(latRadians) + (1.0 / cos(latRadians))) / PI) * 0.5 * tileCount;
	const double maxTileValue = tileCount - 0.000001;
	if (xTileFloat < 0.0) {
		xTileFloat = 0.0;
	} else if (xTileFloat > maxTileValue) {
		xTileFloat = maxTileValue;
	}
	if (yTileFloat < 0.0) {
		yTileFloat = 0.0;
	} else if (yTileFloat > maxTileValue) {
		yTileFloat = maxTileValue;
	}

	const int xTile = static_cast<int>(floor(xTileFloat));
	const int yTile = static_cast<int>(floor(yTileFloat));
	int markerX = static_cast<int>(floor((xTileFloat - static_cast<double>(xTile)) * 256.0));
	int markerY = static_cast<int>(floor((yTileFloat - static_cast<double>(yTile)) * 256.0));
	if (markerX < 0) markerX = 0;
	if (markerX > 255) markerX = 255;
	if (markerY < 0) markerY = 0;
	if (markerY > 255) markerY = 255;

	data_.radarMarkerX = markerX;
	data_.radarMarkerY = markerY;
	data_.radarMapUrl = String("https://tile.openstreetmap.org/") + String(radarZoom) + "/" + String(xTile) + "/" + String(yTile) + ".png";
	if (outTileX != nullptr) {
		*outTileX = xTile;
	}
	if (outTileY != nullptr) {
		*outTileY = yTile;
	}
	return true;
}

bool WeatherApi::ensureOpenMeteoLocationResolved() {
	return ensureLocationCoordinatesResolved();
}

bool WeatherApi::parseLocationCoordinates(float& outLat, float& outLon) const {
	const int comma = config_.locationKey.indexOf(',');
	if (comma <= 0 || comma >= (config_.locationKey.length() - 1)) {
		return false;
	}
	const String latStr = config_.locationKey.substring(0, comma);
	const String lonStr = config_.locationKey.substring(comma + 1);
	char* latEnd = nullptr;
	char* lonEnd = nullptr;
	outLat = strtof(latStr.c_str(), &latEnd);
	outLon = strtof(lonStr.c_str(), &lonEnd);
	if (latEnd == latStr.c_str() || lonEnd == lonStr.c_str()) {
		return false;
	}
	if (*latEnd != '\0' || *lonEnd != '\0') {
		return false;
	}
	return fabsf(outLat) <= 90.0f && fabsf(outLon) <= 180.0f;
}

String WeatherApi::buildOpenMeteoForecastUrl() const {
	float lat = 0.0f;
	float lon = 0.0f;
	if (!parseLocationCoordinates(lat, lon)) {
		return String();
	}

	String endpoint = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
							 "&longitude=" + String(lon, 4) +
							 "&current=temperature_2m,relative_humidity_2m,apparent_temperature,pressure_msl,wind_speed_10m,weather_code,is_day" +
							 "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max" +
							 "&forecast_days=7&timezone=auto";
	if (!config_.useMetric) {
		endpoint += "&temperature_unit=fahrenheit&wind_speed_unit=mph";
	}
	return endpoint;
}

String WeatherApi::buildAuthQuery() const {
	return "&apikey=" + config_.apiKey;
}

bool WeatherApi::fetchJson(const String& url, String& payload, int& httpStatus) {
	HTTPClient http;
	const bool secure = url.startsWith("https://");
	WiFiClient client;
	WiFiClientSecure secureClient;
	if (secure) {
		secureClient.setInsecure();
		if (!http.begin(secureClient, url)) {
			setError(WeatherErrorCode::HttpBeginFailed, "http.begin failed");
			return false;
		}
	} else {
		if (!http.begin(client, url)) {
			setError(WeatherErrorCode::HttpBeginFailed, "http.begin failed");
			return false;
		}
	}

	http.setConnectTimeout(config_.connectTimeoutMs);
	http.setTimeout(config_.requestTimeoutMs);
	http.addHeader("Accept", "application/json");
	httpStatus = http.GET();

	if (httpStatus < 200 || httpStatus >= 300) {
		String body = http.getString();
		http.end();
		setError(WeatherErrorCode::HttpStatusError, "http status " + String(httpStatus) + " body=" + body, httpStatus);
		return false;
	}

	payload = http.getString();
	http.end();
	return true;
}

bool WeatherApi::parseCurrent(const String& payload) {
	JsonDocument doc;
	DeserializationError err = deserializeJson(doc, payload);
	if (err) {
		setError(WeatherErrorCode::JsonParseError, "current parse failed");
		return false;
	}

	JsonObject item;
	if (usingOpenMeteo()) {
		item = doc["current"].as<JsonObject>();
		if (item.isNull()) {
			setError(WeatherErrorCode::ResponseShapeError, "current object missing");
			return false;
		}
	} else if (doc.is<JsonArray>()) {
		JsonArray a = doc.as<JsonArray>();
		if (a.isNull() || a.size() == 0) {
			setError(WeatherErrorCode::ResponseShapeError, "current array empty");
			return false;
		}
		item = a[0].as<JsonObject>();
	} else {
		item = doc.as<JsonObject>();
	}

	if (item.isNull()) {
		setError(WeatherErrorCode::ResponseShapeError, "current item missing");
		return false;
	}

	data_.current.valid = true;
	if (usingOpenMeteo()) {
		const int weatherCode = item["weather_code"] | -1;
		const bool isDaylight = (item["is_day"] | 1) != 0;
		data_.timezoneId = String(doc["timezone"] | "");
		data_.timezoneOffsetMinutes = static_cast<int>((doc["utc_offset_seconds"] | 0) / 60);
		data_.currentLocalMinutes = parseLocalMinutes(String(item["time"] | ""));
		data_.current.observedEpoch = 0;
		data_.current.summary = openMeteoSummaryFromCode(weatherCode);
		data_.current.icon = openMeteoIconFromCode(weatherCode, isDaylight);
		data_.current.isDaylight = isDaylight;
		const float temp = item["temperature_2m"] | NAN;
		const float feelsLike = item["apparent_temperature"] | NAN;
		const float wind = item["wind_speed_10m"] | NAN;
		data_.current.temperatureC = config_.useMetric ? temp : asMetricTemperature(temp, "F");
		data_.current.feelsLikeC = config_.useMetric ? feelsLike : asMetricTemperature(feelsLike, "F");
		data_.current.windKph = config_.useMetric ? wind : (wind * 1.60934f);
		data_.current.pressureMb = item["pressure_msl"] | NAN;
		data_.current.humidityPct = item["relative_humidity_2m"] | -1;
		return true;
	}

	data_.current.observedEpoch = item["EpochTime"] | 0;
	const String observationTime = String(item["LocalObservationDateTime"] | "");
	data_.currentLocalMinutes = parseLocalMinutes(observationTime);
	data_.timezoneOffsetMinutes = parseTimezoneOffsetMinutes(observationTime);
	data_.current.summary = String(item["WeatherText"] | "");
	data_.current.icon = item["WeatherIcon"] | -1;
	data_.current.isDaylight = item["IsDayTime"] | true;

	JsonObject tempObj = item["Temperature"]["Metric"].as<JsonObject>();
	if (!tempObj.isNull()) {
		data_.current.temperatureC = tempObj["Value"] | NAN;
	}

	JsonObject realFeel = item["RealFeelTemperature"]["Metric"].as<JsonObject>();
	if (!realFeel.isNull()) {
		data_.current.feelsLikeC = realFeel["Value"] | NAN;
	}

	JsonObject wind = item["Wind"]["Speed"]["Metric"].as<JsonObject>();
	if (!wind.isNull()) {
		data_.current.windKph = wind["Value"] | NAN;
	}

	JsonObject pressure = item["Pressure"]["Metric"].as<JsonObject>();
	if (!pressure.isNull()) {
		data_.current.pressureMb = pressure["Value"] | NAN;
	}
	data_.current.humidityPct = item["RelativeHumidity"] | -1;
	return true;
}

bool WeatherApi::parseForecast(const String& payload) {
	JsonDocument doc;
	DeserializationError err = deserializeJson(doc, payload);
	if (err) {
		setError(WeatherErrorCode::JsonParseError, "forecast parse failed");
		return false;
	}

	JsonArray days = doc["DailyForecasts"].as<JsonArray>();
	if (usingOpenMeteo()) {
		JsonArray weatherCodes = doc["daily"]["weather_code"].as<JsonArray>();
		JsonArray maxTemps = doc["daily"]["temperature_2m_max"].as<JsonArray>();
		JsonArray minTemps = doc["daily"]["temperature_2m_min"].as<JsonArray>();
		JsonArray rainProb = doc["daily"]["precipitation_probability_max"].as<JsonArray>();
		JsonArray dates = doc["daily"]["time"].as<JsonArray>();
		if (weatherCodes.isNull() || maxTemps.isNull() || minTemps.isNull()) {
			setError(WeatherErrorCode::ResponseShapeError, "forecast daily data missing");
			return false;
		}

		data_.forecastCount = 0;
		const size_t count = weatherCodes.size();
		for (size_t i = 0; i < count && data_.forecastCount < kMaxForecastDays; ++i) {
			DailyForecast& out = data_.forecast[data_.forecastCount++];
			out = DailyForecast();
			out.valid = true;
			const int weatherCode = weatherCodes[i] | -1;
			out.dayLabel = String(dates[i] | "");
			out.summary = openMeteoSummaryFromCode(weatherCode);
			out.dayIcon = openMeteoIconFromCode(weatherCode, true);
			out.nightIcon = openMeteoIconFromCode(weatherCode, false);
			const float minTemp = minTemps[i] | NAN;
			const float maxTemp = maxTemps[i] | NAN;
			out.minTempC = config_.useMetric ? minTemp : asMetricTemperature(minTemp, "F");
			out.maxTempC = config_.useMetric ? maxTemp : asMetricTemperature(maxTemp, "F");
			out.precipChancePct = rainProb.isNull() ? -1 : static_cast<int>(rainProb[i] | -1);
		}
		return data_.forecastCount > 0;
	}
	if (days.isNull()) {
		setError(WeatherErrorCode::ResponseShapeError, "forecast DailyForecasts missing");
		return false;
	}

	data_.forecastCount = 0;
	for (JsonVariant v : days) {
		if (data_.forecastCount >= kMaxForecastDays) {
			break;
		}
		JsonObject d = v.as<JsonObject>();
		DailyForecast& out = data_.forecast[data_.forecastCount++];
		out = DailyForecast();
		out.valid = true;
		out.epochDate = d["EpochDate"] | 0;
		out.dayLabel = String(d["Date"] | "");
		out.dayIcon = d["Day"]["Icon"] | -1;
		out.nightIcon = d["Night"]["Icon"] | -1;
		out.summary = String(d["Day"]["IconPhrase"] | "");

		JsonObject minObj = d["Temperature"]["Minimum"].as<JsonObject>();
		JsonObject maxObj = d["Temperature"]["Maximum"].as<JsonObject>();
		if (!minObj.isNull()) {
			out.minTempC = asMetricTemperature(minObj["Value"] | NAN, String(minObj["Unit"] | "C"));
		}
		if (!maxObj.isNull()) {
			out.maxTempC = asMetricTemperature(maxObj["Value"] | NAN, String(maxObj["Unit"] | "C"));
		}

		int dayRain = d["Day"]["RainProbability"] | -1;
		int nightRain = d["Night"]["RainProbability"] | -1;
		out.precipChancePct = dayRain > nightRain ? dayRain : nightRain;
	}
	return data_.forecastCount > 0;
}

String WeatherApi::openMeteoSummaryFromCode(int weatherCode) const {
	switch (weatherCode) {
		case 0:
			return "Clear";
		case 1:
			return "Mostly clear";
		case 2:
			return "Partly cloudy";
		case 3:
			return "Overcast";
		case 45:
		case 48:
			return "Fog";
		case 51:
		case 53:
		case 55:
			return "Drizzle";
		case 61:
		case 63:
		case 65:
		case 80:
		case 81:
		case 82:
			return "Rain";
		case 71:
		case 73:
		case 75:
		case 77:
		case 85:
		case 86:
			return "Snow";
		case 95:
		case 96:
		case 99:
			return "Thunderstorm";
		default:
			return "Weather";
	}
}

int WeatherApi::openMeteoIconFromCode(int weatherCode, bool isDaylight) const {
	switch (weatherCode) {
		case 0:
			return isDaylight ? 1 : 33;
		case 1:
			return isDaylight ? 2 : 34;
		case 2:
			return isDaylight ? 3 : 35;
		case 3:
			return 7;
		case 45:
		case 48:
			return 11;
		case 51:
		case 53:
		case 55:
			return 9;
		case 61:
		case 63:
		case 65:
		case 80:
		case 81:
		case 82:
			return 12;
		case 71:
		case 73:
		case 75:
		case 77:
		case 85:
		case 86:
			return 19;
		case 95:
		case 96:
		case 99:
			return 15;
		default:
			return -1;
	}
}

bool WeatherApi::parseAlerts(const String& payload) {
	JsonDocument doc;
	DeserializationError err = deserializeJson(doc, payload);
	if (err) {
		setError(WeatherErrorCode::JsonParseError, "alerts parse failed");
		return false;
	}

	JsonArray alerts;
	if (doc.is<JsonArray>()) {
		alerts = doc.as<JsonArray>();
	} else {
		alerts = doc["Alerts"].as<JsonArray>();
	}

	data_.alertCount = 0;
	if (alerts.isNull()) {
		return true;
	}

	for (JsonVariant v : alerts) {
		if (data_.alertCount >= kMaxWeatherAlerts) {
			break;
		}
		JsonObject a = v.as<JsonObject>();
		WeatherAlert& out = data_.alerts[data_.alertCount++];
		out = WeatherAlert();
		out.valid = true;
		out.id = String(a["AlertID"] | a["Id"] | "");
		out.category = String(a["Category"] | "");
		out.severity = String(a["Severity"] | "");
		out.title = String(a["Area"] | a["Headline"] | "");
		out.description = String(a["Description"] | a["Text"] | "");
		out.onsetEpoch = a["OnsetEpoch"] | 0;
		out.expiresEpoch = a["ExpireEpoch"] | 0;
	}
	return true;
}

bool WeatherApi::parseRadar(const String& payload) {
	JsonDocument doc;
	DeserializationError err = deserializeJson(doc, payload);
	if (err) {
		setError(WeatherErrorCode::JsonParseError, "radar parse failed");
		return false;
	}

	if (!ensureLocationCoordinatesResolved()) {
		return false;
	}

	const uint8_t radarZoom = 7;
	int xTile = 0;
	int yTile = 0;
	if (!updateRadarTileProjection(radarZoom, &xTile, &yTile)) {
		setError(WeatherErrorCode::ResponseShapeError, "radar projection failed");
		return false;
	}

	String host = String(doc["host"] | "");
	host.replace("https://", "http://");
	JsonArray pastFrames = doc["radar"]["past"].as<JsonArray>();
	JsonArray nowcastFrames = doc["radar"]["nowcast"].as<JsonArray>();
	if (host.length() == 0 || (pastFrames.isNull() && nowcastFrames.isNull())) {
		setError(WeatherErrorCode::ResponseShapeError, "radar frames missing");
		return false;
	}

	data_.radarFrameCount = 0;
	auto appendFrames = [&](JsonArray frames) {
		for (JsonVariant v : frames) {
			if (data_.radarFrameCount >= kMaxRadarFrames) {
				break;
			}
			JsonObject f = v.as<JsonObject>();
			const String path = String(f["path"] | "");
			if (path.length() == 0) {
				continue;
			}
			RadarTileFrame& out = data_.radarFrames[data_.radarFrameCount++];
			out = RadarTileFrame();
			out.valid = true;
			out.epochTime = f["time"] | 0;
			out.url = host + path + "/256/" + String(radarZoom) + "/" + String(xTile) + "/" + String(yTile) + "/6/1_1.png";
		}
	};

	if (!pastFrames.isNull()) {
		const size_t totalPast = pastFrames.size();
		const size_t startIndex = totalPast > kMaxRadarFrames ? (totalPast - kMaxRadarFrames) : 0;
		for (size_t i = startIndex; i < totalPast; ++i) {
			if (data_.radarFrameCount >= kMaxRadarFrames) {
				break;
			}
			JsonObject f = pastFrames[i].as<JsonObject>();
			const String path = String(f["path"] | "");
			if (path.length() == 0) {
				continue;
			}
			RadarTileFrame& out = data_.radarFrames[data_.radarFrameCount++];
			out = RadarTileFrame();
			out.valid = true;
			out.epochTime = f["time"] | 0;
			out.url = host + path + "/256/" + String(radarZoom) + "/" + String(xTile) + "/" + String(yTile) + "/6/1_1.png";
		}
	}
	if (data_.radarFrameCount < kMaxRadarFrames && !nowcastFrames.isNull()) {
		appendFrames(nowcastFrames);
	}

	if (data_.radarFrameCount < 1) {
		setError(WeatherErrorCode::ResponseShapeError, "no radar frames available");
		return false;
	}

	return true;
}

void WeatherApi::setError(WeatherErrorCode code, const String& message, int httpStatus) {
	data_.lastError = code;
	data_.lastErrorMessage = message;
	data_.lastHttpStatus = httpStatus;
	if (config_.failureRetryMs > 0) {
		nextAllowedAttemptMs_ = millis() + config_.failureRetryMs;
	}
}

void WeatherApi::clearError() {
	data_.lastError = WeatherErrorCode::None;
	data_.lastErrorMessage = "";
	data_.lastHttpStatus = 0;
	nextAllowedAttemptMs_ = 0;
}

WeatherApi::UpdateTask WeatherApi::chooseTask(uint32_t nowMs) const {
	auto stale = [&](uint32_t lastAt, uint32_t ttl) {
		if (forceRefresh_) {
			return true;
		}
		if (lastAt == 0) {
			return true;
		}
		return (nowMs - lastAt) >= ttl;
	};

	if (stale(data_.currentFetchedAtMs, config_.currentCacheMs)) {
		return UpdateTask::Current;
	}
	if (stale(data_.forecastFetchedAtMs, config_.forecastCacheMs)) {
		return UpdateTask::Forecast;
	}
	if (stale(data_.alertsFetchedAtMs, config_.alertsCacheMs)) {
		return UpdateTask::Alerts;
	}
	if (stale(data_.radarFetchedAtMs, config_.radarCacheMs)) {
		return UpdateTask::Radar;
	}
	return UpdateTask::Idle;
}

}  // namespace weather
