#pragma once

#include <lvgl.h>

#include "../weather/radar_engine.h"
#include "../weather/weather_models.h"
#include "ui_alerts.h"
#include "ui_current.h"
#include "ui_radar.h"
#include "ui_system.h"
#include "ui_theme.h"
#include "ui_weekly.h"

namespace ui {

class RootNavigator {
 public:
	void begin(lv_obj_t* screen, ThemeId themeId);
	void setTheme(ThemeId themeId);
	ThemeId themeId() const;

	void setRadarProgress(size_t completed, size_t total, const char* stage);
	void setDebugOverlayEnabled(bool enabled);
	void setDebugOverlayText(const String& text);
	void update(const WeatherData& data, weather::RadarEngine& radar, const SystemInfo& systemInfo);
	uint8_t activePageIndex() const;

 private:
	ThemeManager theme_;

	lv_obj_t* screen_ = nullptr;
	lv_obj_t* tileview_ = nullptr;
	lv_obj_t* currentTile_ = nullptr;
	lv_obj_t* weeklyTile_ = nullptr;
	lv_obj_t* radarTile_ = nullptr;
	lv_obj_t* alertsTile_ = nullptr;
	lv_obj_t* systemTile_ = nullptr;

	CurrentPage currentPage_;
	WeeklyPage weeklyPage_;
	RadarPanel radarPage_;
	AlertsPage alertsPage_;
	SystemPage systemPage_;
	lv_obj_t* debugOverlay_ = nullptr;
	bool debugOverlayEnabled_ = false;
};

}  // namespace ui
