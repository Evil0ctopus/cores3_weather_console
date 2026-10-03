#pragma once

#include <Arduino.h>

namespace app {

struct LocationResult {
	String query;
	String apiKey;
	String key;
	String name;
	String error;
};

bool resolveWeatherLocation(const String& apiKey, const String& query,
	String& key, String& name, String& error);
bool locationLookupStart(const String& apiKey, const String& query, String& error);
bool locationLookupTake(LocationResult& result);

}  // namespace app
