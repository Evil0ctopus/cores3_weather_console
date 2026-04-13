#pragma once

#include <lvgl.h>

#include "../weather/weather_models.h"
#include "ui_system.h"
#include "ui_weather_icon.h"
#include "ui_theme.h"

namespace ui {

class CurrentPage {
 public:
	void begin(lv_obj_t* parent, ThemeManager& theme);
	void applyTheme(ThemeManager& theme);
	void update(const WeatherData& data, const SystemInfo& systemInfo);

 private:
	ThemeManager* theme_ = nullptr;
	lv_obj_t* root_ = nullptr;
	lv_obj_t* cityLabel_ = nullptr;
	lv_obj_t* tempLabel_ = nullptr;
	lv_obj_t* summaryLabel_ = nullptr;
	lv_obj_t* detailsLabel_ = nullptr;
	WeatherIconView iconView_{};
	int lastConditionCode_ = -1;
	bool lastIsDaylight_ = true;
};

}  // namespace ui
