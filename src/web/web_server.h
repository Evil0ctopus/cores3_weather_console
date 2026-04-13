#pragma once

#include <ESPAsyncWebServer.h>

#include "../system/debug_log.h"
#include "../system/settings.h"
#include "../system/wifi_manager.h"
#include "web_routes.h"

namespace web {

class WebServerHost {
 public:
	void begin(app::SettingsStore& settingsStore,
			   app::WifiManager& wifiManager,
			   app::DebugLog* debugLog,
			   ThemeProvider themeProvider,
			   void* themeProviderContext,
			   PreviewProvider previewProvider,
			   void* previewProviderContext,
			   SettingsSavedCallback settingsSavedCallback,
			   void* settingsSavedContext);
	void tick();

 private:
	static void onSettingsSavedAdapter(void* userContext, const app::AppSettings& settings);
	void handleSettingsSaved(const app::AppSettings& settings);
	void publishIfChanged(bool force);
	void publishSettings(bool force);
	void publishPreview(bool force);
	void publishDebug(bool force);

	AsyncWebServer server_{80};
	AsyncEventSource events_{"/api/events"};
	app::SettingsStore* settingsStore_ = nullptr;
	app::DebugLog* debugLog_ = nullptr;
	ThemeProvider themeProvider_ = nullptr;
	void* themeProviderContext_ = nullptr;
	PreviewProvider previewProvider_ = nullptr;
	void* previewProviderContext_ = nullptr;
	SettingsSavedCallback settingsSavedCallback_ = nullptr;
	void* settingsSavedContext_ = nullptr;
	uint32_t lastPublishMs_ = 0;
	uint32_t lastPublishedRevision_ = 0;
	String lastSettingsPayload_;
	String lastPreviewPayload_;
	String lastDebugPayload_;
	uint32_t lastPublishedDebugRevision_ = 0;
	bool started_ = false;
};

}  // namespace web
