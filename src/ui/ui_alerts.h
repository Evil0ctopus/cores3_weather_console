#pragma once

#include <lvgl.h>

#include "../weather/weather_models.h"
#include "ui_theme.h"

namespace ui {

class AlertsPage {
 public:
	void begin(lv_obj_t* parent, ThemeManager& theme);
	void applyTheme(ThemeManager& theme);
	void update(const WeatherData& data);

 private:
	lv_obj_t* root_ = nullptr;
	lv_obj_t* title_ = nullptr;
	lv_obj_t* subtitle_ = nullptr;
	lv_obj_t* list_ = nullptr;
	lv_obj_t* rows_[kMaxWeatherAlerts] = {};
};

}  // namespace ui
