// Example integration of audio callbacks into various systems
// This file shows best practices for wiring audio events into different modules

#pragma once

#include <Arduino.h>
#include "audio_engine.h"

namespace audio {

// =============================================================================
// WiFi Manager Integration Helper
// =============================================================================
// Usage: Call this from WiFiManager::onConnected() and ::onDisconnected()

inline void TriggerWiFiAudio(bool connected, audio::AudioEngine* engine) {
	if (engine == nullptr) return;
	engine->onWiFiEvent(connected ? "connected" : "disconnected");
}

// =============================================================================
// LED Engine Integration Helper
// =============================================================================
// Usage: Call this from LEDEngine callbacks when alerts are triggered

inline void TriggerAlertAudio(uint8_t alertLevel, audio::AudioEngine* engine) {
	if (engine == nullptr) return;
	engine->onLedAlert(alertLevel);
}

// =============================================================================
// Weather Alert Integration Helper
// =============================================================================
// Usage: Call this from weather update handler when alerts change

inline void TriggerWeatherAudio(const WeatherData& data, audio::AudioEngine* engine) {
	if (engine == nullptr) return;
	
	uint8_t strongestSeverity = 0;
	for (size_t i = 0; i < data.alertCount; ++i) {
		if (!data.alerts[i].valid) continue;
		
		String severity = data.alerts[i].severity;
		severity.toLowerCase();
		if (severity.indexOf("extreme") >= 0 || severity.indexOf("severe") >= 0) {
			strongestSeverity = 3;
			break;
		}
		if (severity.indexOf("moderate") >= 0 || severity.indexOf("warning") >= 0) {
			if (strongestSeverity < 2) strongestSeverity = 2;
		} else if (severity.indexOf("minor") >= 0 || severity.indexOf("watch") >= 0) {
			if (strongestSeverity < 1) strongestSeverity = 1;
		}
	}
	
	if (strongestSeverity > 0) {
		engine->onWeatherAlert(strongestSeverity);
	}
}

// =============================================================================
// UI Page Transition Integration Helper
// =============================================================================
// Usage: Call this from main loop when page changes

inline void TriggerPageTransitionAudio(uint8_t fromPage, uint8_t toPage, 
                                       audio::AudioEngine* engine) {
	if (engine == nullptr || fromPage == toPage) return;
	engine->onPageTransition(fromPage, toPage);
}

// =============================================================================
// Settings Update Integration Helper
// =============================================================================
// Usage: Call this when settings are saved via web UI or local storage

inline void TriggerSettingsConfirmAudio(const app::AppSettings& oldSettings,
                                         const app::AppSettings& newSettings,
                                         audio::AudioEngine* engine) {
	if (engine == nullptr) return;
	
	// Only play if meaningful settings changed (not just debug mode toggle)
	bool themeChanged = oldSettings.theme != newSettings.theme;
	bool unitsChanged = oldSettings.units != newSettings.units;
	bool locationChanged = oldSettings.locationKey != newSettings.locationKey;
	bool updateIntervalChanged = oldSettings.updateIntervalMinutes != newSettings.updateIntervalMinutes;
	
	if (themeChanged || unitsChanged || locationChanged || updateIntervalChanged) {
		engine->onSettingsChanged();
	}
}

// =============================================================================
// Radar Update Integration Helper
// =============================================================================
// Usage: Call periodically during radar frame downloads

inline void TriggerRadarAudio(const weather::RadarProgress& progress,
                              audio::AudioEngine* engine) {
	if (engine == nullptr) return;
	if (progress.completedFrames > 0 && progress.completedFrames % progress.totalFrames == 0) {
		engine->onRadarUpdate();
	}
}

// =============================================================================
// Theme Synchronization Helper
// =============================================================================
// Usage: Call when theme changes to sync audio engine theme with UI theme

inline void SyncAudioTheme(ui::ThemeId uiTheme, audio::AudioEngine* engine) {
	if (engine == nullptr) return;
	
	audio::ThemeId audioTheme = audio::ThemeId::Default;
	switch (uiTheme) {
		case ui::ThemeId::Light:
			audioTheme = audio::ThemeId::Default;
			break;
		case ui::ThemeId::Dark:
			audioTheme = audio::ThemeId::Modern;
			break;
		case ui::ThemeId::FutureCustom1:
			audioTheme = audio::ThemeId::Minimal;
			break;
		default:
			audioTheme = audio::ThemeId::Default;
			break;
	}
	
	engine->setTheme(audioTheme);
}

// =============================================================================
// Master Volume Adapter
// =============================================================================
// Usage: Sync system volume with audio engine when settings change

inline void ApplyAudioVolumeSettings(const app::AppSettings& settings,
                                     audio::AudioEngine* engine) {
	if (engine == nullptr) return;
	
	// Map settings volume preference to 0-255 range
	// You could add a "audioVolume" field to AppSettings (0-100)
	// For now, use a moderate fixed volume
	uint8_t volume = 200;  // Default: 78% of max
	
	// Example: if you add audioVolume to settings
	// uint8_t volume = (settings.audioVolume * 255) / 100;
	
	engine->setMasterVolume(volume);
}

// =============================================================================
// Audio Mute Helper
// =============================================================================
// Usage: Mute/unmute based on system state (e.g., during calls, meetings)

inline void SetAudioMuteState(bool shouldMute, audio::AudioEngine* engine) {
	if (engine == nullptr) return;
	engine->setMuted(shouldMute);
}

// =============================================================================
// Complete Integration Example
// =============================================================================
// This shows how to wire everything together in main.cpp or a separate
// integration module

/*
// In BeginSubsystems():
gAudioEngine.begin();
gAudioEngine.setEventCallback(OnAudioEvent, nullptr);
SyncAudioTheme(gUi.themeId(), &gAudioEngine);
ApplyAudioVolumeSettings(CurrentSettings(), &gAudioEngine);

// In settings callback:
void OnSettingsSaved(void* userContext, const app::AppSettings& settings) {
  static app::AppSettings lastSettings = settings;
  TriggerSettingsConfirmAudio(lastSettings, settings, &gAudioEngine);
  SyncAudioTheme(ResolveTheme(settings.theme), &gAudioEngine);
  ApplyAudioVolumeSettings(settings, &gAudioEngine);
  lastSettings = settings;
}

// In main loop:
// Page transitions
static uint8_t lastPageIndex = 0;
uint8_t currentPage = gUi.activePageIndex();
if (currentPage != lastPageIndex) {
  TriggerPageTransitionAudio(lastPageIndex, currentPage, &gAudioEngine);
  lastPageIndex = currentPage;
}

// Weather alerts
static WeatherErrorCode lastWeatherError = WeatherErrorCode::None;
const WeatherData& weatherData = gWeatherApi.data();
static uint8_t lastAlertCount = 0;
if (weatherData.alertCount != lastAlertCount || 
    (weatherData.alertCount > 0 && weatherData.lastCycleAtMs != gLastWeatherCycleMs)) {
  TriggerWeatherAudio(weatherData, &gAudioEngine);
  lastAlertCount = weatherData.alertCount;
}

// Radar updates
static size_t lastRadarFrames = 0;
if (gRadarEngine.completedFrameCount() > lastRadarFrames) {
  weather::RadarProgress prog;
  prog.completedFrames = gRadarEngine.completedFrameCount();
  prog.totalFrames = gRadarEngine.frameCount();
  TriggerRadarAudio(prog, &gAudioEngine);
  lastRadarFrames = prog.completedFrames;
}
*/

}  // namespace audio
