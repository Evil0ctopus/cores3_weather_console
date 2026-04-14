#pragma once

#include <lvgl.h>

#include "../system/settings.h"
#include "../weather/radar_engine.h"
#include "../weather/weather_models.h"
#include "ui_alerts.h"
#include "ui_current.h"
#include "ui_daily.h"
#include "ui_hourly.h"
#include "ui_radar.h"
#include "ui_system.h"
#include "ui_theme.h"

namespace ui {

class RootNavigator {
 public:
	void begin(lv_obj_t* screen, app::SettingsStore& settingsStore);
	void setTheme(ThemeId themeId);
	ThemeId themeId() const;

	void setRadarProgress(size_t completed, size_t total, const char* stage);
	void setDebugOverlayEnabled(bool enabled);
	void setDebugOverlayText(const String& text);
	void update(const WeatherData& data, weather::RadarEngine& radar, const SystemInfo& systemInfo);
	uint8_t activePageIndex() const;
	void recenterActivePage(bool animated = false);
	bool moveToAdjacentPage(int8_t delta, bool animated = true);

 private:
	lv_obj_t* tileForPage(uint8_t page) const;
	static void onThemeSelectedAdapter(void* userContext, ThemeId themeId);
	void onThemeSelected(ThemeId themeId);

	ThemeManager theme_;
	app::SettingsStore* settingsStore_ = nullptr;

	lv_obj_t* screen_ = nullptr;
	lv_obj_t* backgroundLayer_ = nullptr;
	lv_obj_t* tileview_ = nullptr;
	lv_obj_t* currentTile_ = nullptr;
	lv_obj_t* hourlyTile_ = nullptr;
	lv_obj_t* dailyTile_ = nullptr;
	lv_obj_t* radarTile_ = nullptr;
	lv_obj_t* alertsTile_ = nullptr;
	lv_obj_t* systemTile_ = nullptr;

	CurrentPage currentPage_;
	HourlyPage hourlyPage_;
	DailyPage dailyPage_;
	RadarPanel radarPage_;
	AlertsPage alertsPage_;
	SystemPage systemPage_;
	lv_obj_t* debugOverlay_ = nullptr;
	lv_obj_t* timeHud_ = nullptr;
	lv_obj_t* timeLabel_ = nullptr;
	lv_obj_t* statusHud_ = nullptr;
	lv_obj_t* wifiIcon_ = nullptr;
	lv_obj_t* wifiBars_[4] = {};
	lv_obj_t* batteryShell_ = nullptr;
	lv_obj_t* batteryFill_ = nullptr;
	lv_obj_t* batteryHead_ = nullptr;
	lv_obj_t* batteryText_ = nullptr;
	lv_obj_t* batteryCharge_ = nullptr;
	bool debugOverlayEnabled_ = false;
	uint8_t currentPageIndex_ = 0;
};

}  // namespace ui
