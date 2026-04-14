#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../system/settings.h"
#include "ui_theme.h"

namespace ui {

struct SystemInfo {
	bool wifiConnected = false;
	uint8_t wifiSignalBars = 0;
	int batteryPct = 0;
	bool batteryCharging = false;
	String currentTime;
	String ipAddress;
	String webUiUrl;
	String wifiSsid;
	String wifiRssi;
	String wifiStatus;
	String bleStatus;
	String weatherStatus;
	String radarStatus;
	String lastUpdate;
	String spiffsUsage;
	String firmwareVersion;
	String ledMode;
};

class SystemPage {
 public:
	typedef void (*ThemeSelectedCallback)(void* userContext, ThemeId themeId);

	void begin(lv_obj_t* parent,
			   ThemeManager& theme,
			   app::SettingsStore& settingsStore,
			   ThemeSelectedCallback themeSelectedCallback,
			   void* themeSelectedContext);
	void applyTheme(ThemeManager& theme);
	void update(const SystemInfo& info);

 private:
	static void onThemeDropdownChanged(lv_event_t* event);
	void handleThemeDropdownChanged();
	void syncThemeSelector();

	app::SettingsStore* settingsStore_ = nullptr;
	ThemeSelectedCallback themeSelectedCallback_ = nullptr;
	void* themeSelectedContext_ = nullptr;
	lv_obj_t* root_ = nullptr;
	lv_obj_t* title_ = nullptr;
	lv_obj_t* subtitle_ = nullptr;
	lv_obj_t* themeLabel_ = nullptr;
	lv_obj_t* themeDropdown_ = nullptr;
	lv_obj_t* list_ = nullptr;
	lv_obj_t* rowLabels_[11] = {};
};

}  // namespace ui
