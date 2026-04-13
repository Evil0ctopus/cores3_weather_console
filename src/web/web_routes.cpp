#include "web_routes.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace web {
namespace {

const char* kAccuWeatherBaseUrl = "http://dataservice.accuweather.com";
const char* kAccuWeatherCitySearchPath = "/locations/v1/cities/search";
const char* kAccuWeatherPostalSearchPath = "/locations/v1/postalcodes/search";
const char* kOpenMeteoGeocodeUrl = "https://geocoding-api.open-meteo.com/v1/search";

String themeIdToString(ui::ThemeId themeId);

void writeWifiStatusJson(JsonDocument& doc,
							 const app::WifiStatusInfo& status,
							 const app::AppSettings& settings) {
	doc["connected"] = status.connected;
	doc["connecting"] = status.connecting;
	doc["ssid"] = status.ssid.length() > 0 ? status.ssid : settings.wifiSsid;
	doc["savedSsid"] = settings.wifiSsid;
	doc["ipAddress"] = status.ipAddress;
	doc["rssi"] = status.rssi;
	doc["status"] = status.statusText;
	doc["statusCode"] = static_cast<int>(status.statusCode);
	doc["autoConnect"] = status.autoConnect;
	doc["lastAttemptAtMs"] = status.lastAttemptAtMs;
}

String effectiveThemeString(const app::AppSettings& settings, ui::ThemeId deviceTheme) {
	(void)deviceTheme;
	return app::toString(settings.theme);
}

void sendJson(AsyncWebServerRequest* request, const JsonDocument& doc, int statusCode = 200) {
	String payload;
	serializeJson(doc, payload);
	AsyncWebServerResponse* response = request->beginResponse(statusCode, "application/json", payload);
	response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
	response->addHeader("Pragma", "no-cache");
	response->addHeader("Expires", "0");
	request->send(response);
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

String themeIdToString(ui::ThemeId themeId) {
	return String(ui::theme_id_to_storage_key(themeId));
}

void sendError(AsyncWebServerRequest* request, const String& message, int statusCode) {
	JsonDocument doc;
	doc["error"] = message;
	sendJson(request, doc, statusCode);
}

bool resolveLocation(const String& apiKey,
					   const String& locationQuery,
					   String& outLocationKey,
					   String& outLocationName,
					   String& outError) {
	auto queryLooksPostal = [](const String& query) {
		if (query.length() < 3 || query.length() > 12) {
			return false;
		}
		bool hasDigit = false;
		for (size_t i = 0; i < query.length(); ++i) {
			const char c = query[i];
			if (c >= '0' && c <= '9') {
				hasDigit = true;
				continue;
			}
			if (c == '-' || c == ' ') {
				continue;
			}
			return false;
		}
		return hasDigit;
	};

	auto resolveWithPath = [&](const char* path, bool& noMatch) {
		noMatch = false;
		HTTPClient http;
		const String endpoint = String(kAccuWeatherBaseUrl) + path + "?apikey=" +
									apiKey + "&language=en-us&details=false&q=" + urlEncode(locationQuery);
		if (!http.begin(endpoint)) {
			outError = "Failed to connect to the location service.";
			return false;
		}

		const int status = http.GET();
		if (status != HTTP_CODE_OK) {
			outError = "Location lookup failed with HTTP " + String(status) + ".";
			http.end();
			return false;
		}

		const String payload = http.getString();
		http.end();

		JsonDocument doc;
		if (deserializeJson(doc, payload) != DeserializationError::Ok || !doc.is<JsonArray>()) {
			outError = "Location lookup returned unreadable data.";
			return false;
		}

		JsonArray results = doc.as<JsonArray>();
		if (results.isNull() || results.size() == 0) {
			noMatch = true;
			return false;
		}

		JsonObject first = results[0].as<JsonObject>();
		outLocationKey = String(static_cast<const char*>(first["Key"] | ""));
		outLocationName = String(static_cast<const char*>(first["LocalizedName"] | ""));
		if (outLocationKey.length() == 0) {
			outError = "Location lookup did not return a usable key.";
			return false;
		}
		return true;
	};

	auto resolveWithOpenMeteo = [&]() {
		HTTPClient http;
		WiFiClientSecure client;
		client.setInsecure();
		const String endpoint = String(kOpenMeteoGeocodeUrl) +
			"?name=" + urlEncode(locationQuery) + "&count=1&language=en&format=json";
		if (!http.begin(client, endpoint)) {
			outError = "Failed to connect to the location service.";
			return false;
		}

		const int status = http.GET();
		if (status != HTTP_CODE_OK) {
			outError = "Location lookup failed with HTTP " + String(status) + ".";
			http.end();
			return false;
		}

		const String payload = http.getString();
		http.end();

		JsonDocument doc;
		if (deserializeJson(doc, payload) != DeserializationError::Ok) {
			outError = "Location lookup returned unreadable data.";
			return false;
		}

		JsonArray results = doc["results"].as<JsonArray>();
		if (results.isNull() || results.size() == 0) {
			outError = "No matching location was found. Try city/state or ZIP code.";
			return false;
		}

		JsonObject first = results[0].as<JsonObject>();
		const float latitude = first["latitude"] | NAN;
		const float longitude = first["longitude"] | NAN;
		if (isnan(latitude) || isnan(longitude)) {
			outError = "Location lookup did not return usable coordinates.";
			return false;
		}

		outLocationKey = String(latitude, 4) + "," + String(longitude, 4);
		outLocationName = String(static_cast<const char*>(first["name"] | ""));
		const String admin1 = String(static_cast<const char*>(first["admin1"] | ""));
		if (admin1.length() > 0) {
			outLocationName += ", " + admin1;
		}
		return outLocationName.length() > 0;
	};

	if (locationQuery.length() == 0) {
		outError = "Location cannot be empty. Enter a city/state or ZIP code.";
		return false;
	}
	if (WiFi.status() != WL_CONNECTED) {
		outError = "WiFi must be connected to resolve a location.";
		return false;
	}

	if (apiKey.length() == 0) {
		if (resolveWithOpenMeteo()) {
			return true;
		}
		if (outError.length() == 0) {
			outError = "No matching location was found. Try city/state or ZIP code.";
		}
		return false;
	}

	const bool looksPostal = queryLooksPostal(locationQuery);
	const char* primaryPath = looksPostal ? kAccuWeatherPostalSearchPath : kAccuWeatherCitySearchPath;
	const char* fallbackPath = looksPostal ? kAccuWeatherCitySearchPath : kAccuWeatherPostalSearchPath;

	bool noMatch = false;
	if (resolveWithPath(primaryPath, noMatch)) {
		return true;
	}
	if (noMatch && resolveWithPath(fallbackPath, noMatch)) {
		return true;
	}
	if (noMatch) {
		outError = "No matching location was found. Try city/state or ZIP code.";
	}
	return false;
}

app::AppSettings buildSettingsFromRequest(AsyncWebServerRequest* request,
											 const app::AppSettings& current,
											 String& outError) {
	JsonDocument source;
	source["locationQuery"] = request->hasParam("locationQuery", true) ? request->getParam("locationQuery", true)->value() : current.locationQuery;
	source["apiKey"] = request->hasParam("apiKey", true) ? request->getParam("apiKey", true)->value() : current.apiKey;
	source["wifiSsid"] = request->hasParam("wifiSsid", true) ? request->getParam("wifiSsid", true)->value() : current.wifiSsid;
	source["wifiPassword"] = request->hasParam("wifiPassword", true) ? request->getParam("wifiPassword", true)->value() : current.wifiPassword;
	source["wifiAutoConnect"] = request->hasParam("wifiAutoConnect", true)
		? request->getParam("wifiAutoConnect", true)->value() == "true"
		: current.wifiAutoConnect;
	source["units"] = request->hasParam("units", true) ? request->getParam("units", true)->value() : app::toString(current.units);
	source["theme"] = request->hasParam("theme", true) ? request->getParam("theme", true)->value() : app::toString(current.theme);
	source["radarMode"] = request->hasParam("radarMode", true) ? request->getParam("radarMode", true)->value() : app::toString(current.radarMode);
	source["radarAutoContrast"] = request->hasParam("radarAutoContrast", true)
		? request->getParam("radarAutoContrast", true)->value() == "true"
		: current.radarAutoContrast;
	source["radarInterpolation"] = request->hasParam("radarInterpolation", true)
		? request->getParam("radarInterpolation", true)->value() == "true"
		: current.radarInterpolation;
	source["radarStormOverlays"] = request->hasParam("radarStormOverlays", true)
		? request->getParam("radarStormOverlays", true)->value() == "true"
		: current.radarStormOverlays;
	source["radarInterpolationSteps"] = request->hasParam("radarInterpolationSteps", true)
		? request->getParam("radarInterpolationSteps", true)->value()
		: String(current.radarInterpolationSteps);
	source["radarSmoothingPasses"] = request->hasParam("radarSmoothingPasses", true)
		? request->getParam("radarSmoothingPasses", true)->value()
		: String(current.radarSmoothingPasses);
	source["debugMode"] = request->hasParam("debugMode", true)
		? request->getParam("debugMode", true)->value() == "true"
		: current.debugMode;
	source["updateIntervalMinutes"] = request->hasParam("updateIntervalMinutes", true) ? request->getParam("updateIntervalMinutes", true)->value() : String(current.updateIntervalMinutes);

	app::AppSettings next = current;
	if (!app::readSettingsJson(source.as<JsonVariantConst>(), next, outError, &current)) {
		return current;
	}

	if (!app::validateSettings(next, outError, false)) {
		return current;
	}

	const bool locationChanged = next.locationQuery != current.locationQuery || next.apiKey != current.apiKey || current.locationKey.length() == 0;
	if (locationChanged) {
		if (next.apiKey.length() == 0) {
			next.locationKey = "";
			next.locationName = next.locationQuery;
		} else {
			String resolvedKey;
			String resolvedName;
			if (!resolveLocation(next.apiKey, next.locationQuery, resolvedKey, resolvedName, outError)) {
				return current;
			}
			next.locationKey = resolvedKey;
			next.locationName = resolvedName.length() > 0 ? resolvedName : next.locationQuery;
		}
	}
	if (!app::validateSettings(next, outError, true)) {
		return current;
	}
	return next;
}

}  // namespace

void writeWebSettingsJson(JsonDocument& doc, const app::AppSettings& settings, ui::ThemeId deviceTheme) {
	app::writeSettingsJson(doc, settings);
	doc["deviceTheme"] = themeIdToString(deviceTheme);
	doc["effectiveTheme"] = effectiveThemeString(settings, deviceTheme);
}

void registerRoutes(AsyncWebServer& server,
							 app::WifiManager& wifiManager,
							 app::SettingsStore& settingsStore,
							 app::DebugLog* debugLog,
							 ThemeProvider themeProvider,
							 void* themeProviderContext,
							 PreviewProvider previewProvider,
							 void* previewProviderContext,
							 SettingsSavedCallback settingsSavedCallback,
							 void* settingsSavedContext) {
	server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
		AsyncWebServerResponse* response = request->beginResponse(SPIFFS, "/index.html", "text/html");
		response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
		request->send(response);
	});

	server.serveStatic("/web_assets/", SPIFFS, "/")
		.setDefaultFile("index.html")
		.setCacheControl("no-store, no-cache, must-revalidate, max-age=0");

	server.on("/api/settings", HTTP_GET, [&settingsStore, themeProvider, themeProviderContext](AsyncWebServerRequest* request) {
		JsonDocument doc;
		const ui::ThemeId deviceTheme = themeProvider != nullptr ? themeProvider(themeProviderContext) : ui::ThemeId::PIXEL_STORM;
		writeWebSettingsJson(doc, settingsStore.settings(), deviceTheme);
		doc["revision"] = settingsStore.revision();
		sendJson(request, doc);
	});

	server.on("/api/preview", HTTP_GET, [previewProvider, previewProviderContext](AsyncWebServerRequest* request) {
		JsonDocument doc;
		if (previewProvider != nullptr) {
			previewProvider(previewProviderContext, doc);
		}
		sendJson(request, doc);
	});

	server.on("/api/debug/logs", HTTP_GET, [debugLog](AsyncWebServerRequest* request) {
		JsonDocument doc;
		if (debugLog != nullptr) {
			debugLog->writeRecentJson(doc, 60);
		} else {
			doc["enabled"] = false;
			doc["revision"] = 0;
			doc["count"] = 0;
			doc["entries"].to<JsonArray>();
		}
		sendJson(request, doc);
	});

	server.on("/api/debug/clear", HTTP_POST, [debugLog](AsyncWebServerRequest* request) {
		if (debugLog == nullptr) {
			sendError(request, "Debug logger unavailable.", 500);
			return;
		}
		debugLog->clear();
		JsonDocument doc;
		doc["message"] = "Debug logs cleared.";
		debugLog->writeRecentJson(doc, 60);
		sendJson(request, doc);
	});

	server.on("/wifi/status", HTTP_GET, [&wifiManager, &settingsStore](AsyncWebServerRequest* request) {
		JsonDocument doc;
		writeWifiStatusJson(doc, wifiManager.statusInfo(), settingsStore.settings());
		sendJson(request, doc);
	});

	server.on("/wifi/scan", HTTP_GET, [&wifiManager](AsyncWebServerRequest* request) {
		const bool startRequested = request->hasParam("start") && request->getParam("start")->value() == "1";
		if (startRequested && !wifiManager.scanInProgress()) {
			wifiManager.startScan();
		}

		app::WifiNetworkInfo networks[app::WifiManager::kMaxScanResults];
		const size_t count = wifiManager.scanNetworks(networks, app::WifiManager::kMaxScanResults);
		JsonDocument doc;
		doc["inProgress"] = wifiManager.scanInProgress();
		doc["count"] = count;
		JsonArray items = doc["networks"].to<JsonArray>();
		for (size_t i = 0; i < count; ++i) {
			JsonObject item = items.add<JsonObject>();
			item["ssid"] = networks[i].ssid;
			item["rssi"] = networks[i].rssi;
			item["secure"] = networks[i].secure;
		}
		sendJson(request, doc);
	});

	server.on("/wifi/connect", HTTP_POST, [&wifiManager, &settingsStore](AsyncWebServerRequest* request) {
		const String requestedSsid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : String();
		const String requestedPassword = request->hasParam("password", true) ? request->getParam("password", true)->value() : String();
		const bool autoConnect = request->hasParam("autoConnect", true)
			? request->getParam("autoConnect", true)->value() == "true"
			: settingsStore.settings().wifiAutoConnect;

		String ssid = requestedSsid;
		ssid.trim();
		if (ssid.length() == 0) {
			sendError(request, "SSID is required.", 400);
			return;
		}

		app::WifiConfig config = wifiManager.config();
		config.ssid = ssid;
		config.autoConnect = autoConnect;
		if (requestedPassword.length() > 0 || settingsStore.settings().wifiSsid != ssid) {
			config.password = requestedPassword;
		} else if (config.password.length() == 0) {
			config.password = settingsStore.settings().wifiPassword;
		}

		wifiManager.applyConfig(config, true, true);
		const bool persisted = settingsStore.saveWifiSettings(config.ssid, config.password, config.autoConnect);

		JsonDocument doc;
		writeWifiStatusJson(doc, wifiManager.statusInfo(), settingsStore.settings());
		doc["message"] = "WiFi connection started.";
		doc["persisted"] = persisted;
		if (!persisted) {
			doc["warning"] = "WiFi connected in memory, but credentials were not persisted to settings NVS.";
		}
		sendJson(request, doc, persisted ? 200 : 202);
	});

	auto saveSettingsHandler = [&settingsStore, themeProvider, themeProviderContext, settingsSavedCallback, settingsSavedContext](AsyncWebServerRequest* request) {
		String error;
		const app::AppSettings current = settingsStore.settings();
		const app::AppSettings next = buildSettingsFromRequest(request, current, error);
		if (error.length() > 0) {
			sendError(request, error, 400);
			return;
		}

		if (!settingsStore.save(next)) {
			sendError(request, "Failed to save settings to NVS.", 500);
			return;
		}

		if (settingsSavedCallback != nullptr) {
			settingsSavedCallback(settingsSavedContext, next);
		}

		JsonDocument doc;
		const ui::ThemeId deviceTheme = themeProvider != nullptr ? themeProvider(themeProviderContext) : ui::ThemeId::PIXEL_STORM;
		writeWebSettingsJson(doc, settingsStore.settings(), deviceTheme);
		doc["revision"] = settingsStore.revision();
		doc["message"] = "Settings saved and applied.";
		sendJson(request, doc);
	};

	server.on("/api/settings", HTTP_POST, saveSettingsHandler);
	server.on("/api/settings/live", HTTP_POST, saveSettingsHandler);
}

}  // namespace web
