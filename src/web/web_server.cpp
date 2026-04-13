#include "web_server.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>

namespace web {
namespace {

const uint32_t kPreviewPublishIntervalMs = 1000;

String serializeJsonDocument(const JsonDocument& doc) {
	String payload;
	serializeJson(doc, payload);
	return payload;
}

}  // namespace

void WebServerHost::begin(app::SettingsStore& settingsStore,
							  app::WifiManager& wifiManager,
							  app::DebugLog* debugLog,
							  ThemeProvider themeProvider,
							  void* themeProviderContext,
							  PreviewProvider previewProvider,
							  void* previewProviderContext,
							  SettingsSavedCallback settingsSavedCallback,
							  void* settingsSavedContext) {
	if (started_) {
		return;
	}

	if (!SPIFFS.begin(true)) {
		return;
	}

	settingsStore_ = &settingsStore;
	debugLog_ = debugLog;
	themeProvider_ = themeProvider;
	themeProviderContext_ = themeProviderContext;
	previewProvider_ = previewProvider;
	previewProviderContext_ = previewProviderContext;
	settingsSavedCallback_ = settingsSavedCallback;
	settingsSavedContext_ = settingsSavedContext;
	lastPublishedRevision_ = settingsStore.revision();
	lastPublishedDebugRevision_ = debugLog_ != nullptr ? debugLog_->revision() : 0;

	events_.onConnect([this](AsyncEventSourceClient* client) {
		if (client == nullptr) {
			return;
		}
		client->send("connected", "status", millis());
		publishIfChanged(true);
	});
	server_.addHandler(&events_);

	registerRoutes(server_,
				   wifiManager,
				   settingsStore,
				   debugLog,
				   themeProvider,
				   themeProviderContext,
				   previewProvider,
				   previewProviderContext,
				   onSettingsSavedAdapter,
				   this);
	server_.begin();
	publishIfChanged(true);
	started_ = true;
}

void WebServerHost::tick() {
	if (!started_ || events_.count() == 0) {
		return;
	}

	const uint32_t now = millis();
	const bool settingsDirty = settingsStore_ != nullptr && settingsStore_->revision() != lastPublishedRevision_;
	const bool debugDirty = debugLog_ != nullptr && debugLog_->revision() != lastPublishedDebugRevision_;
	if ((now - lastPublishMs_) < kPreviewPublishIntervalMs && !settingsDirty && !debugDirty) {
		return;
	}

	publishIfChanged(false);
}

void WebServerHost::onSettingsSavedAdapter(void* userContext, const app::AppSettings& settings) {
	WebServerHost* host = static_cast<WebServerHost*>(userContext);
	if (host == nullptr) {
		return;
	}
	host->handleSettingsSaved(settings);
}

void WebServerHost::handleSettingsSaved(const app::AppSettings& settings) {
	if (settingsSavedCallback_ != nullptr) {
		settingsSavedCallback_(settingsSavedContext_, settings);
	}
	publishIfChanged(true);
}

void WebServerHost::publishIfChanged(bool force) {
	if (!started_ && !force) {
		return;
	}
	publishSettings(force);
	publishPreview(force);
	publishDebug(force);
	lastPublishMs_ = millis();
	if (settingsStore_ != nullptr) {
		lastPublishedRevision_ = settingsStore_->revision();
	}
	if (debugLog_ != nullptr) {
		lastPublishedDebugRevision_ = debugLog_->revision();
	}
}

void WebServerHost::publishSettings(bool force) {
	if (settingsStore_ == nullptr || events_.count() == 0) {
		return;
	}

	JsonDocument doc;
	const ui::ThemeId deviceTheme = themeProvider_ != nullptr ? themeProvider_(themeProviderContext_) : ui::ThemeId::Dark;
	writeWebSettingsJson(doc, settingsStore_->settings(), deviceTheme);
	doc["revision"] = settingsStore_->revision();
	const String payload = serializeJsonDocument(doc);
	if (!force && payload == lastSettingsPayload_) {
		return;
	}
	lastSettingsPayload_ = payload;
	events_.send(payload.c_str(), "settings", millis());
}

void WebServerHost::publishPreview(bool force) {
	if (previewProvider_ == nullptr || events_.count() == 0) {
		return;
	}

	JsonDocument doc;
	previewProvider_(previewProviderContext_, doc);
	const String payload = serializeJsonDocument(doc);
	if (!force && payload == lastPreviewPayload_) {
		return;
	}
	lastPreviewPayload_ = payload;
	events_.send(payload.c_str(), "preview", millis());
}

void WebServerHost::publishDebug(bool force) {
	if (debugLog_ == nullptr || events_.count() == 0) {
		return;
	}

	JsonDocument doc;
	debugLog_->writeRecentJson(doc, 60);
	const String payload = serializeJsonDocument(doc);
	if (!force && payload == lastDebugPayload_) {
		return;
	}
	lastDebugPayload_ = payload;
	events_.send(payload.c_str(), "debug", millis());
}

}  // namespace web
