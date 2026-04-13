#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include "../system/debug_log.h"
#include "../system/settings.h"
#include "../system/wifi_manager.h"
#include "../ui/ui_theme.h"

namespace web {

typedef ui::ThemeId (*ThemeProvider)(void* userContext);
typedef void (*SettingsSavedCallback)(void* userContext, const app::AppSettings& settings);
typedef void (*PreviewProvider)(void* userContext, JsonDocument& doc);

void writeWebSettingsJson(JsonDocument& doc,
							  const app::AppSettings& settings,
							  ui::ThemeId deviceTheme);

void registerRoutes(AsyncWebServer& server,
							 app::WifiManager& wifiManager,
							 app::SettingsStore& settingsStore,
							 app::DebugLog* debugLog,
							 ThemeProvider themeProvider,
							 void* themeProviderContext,
							 PreviewProvider previewProvider,
							 void* previewProviderContext,
							 SettingsSavedCallback settingsSavedCallback,
							 void* settingsSavedContext);

}  // namespace web
