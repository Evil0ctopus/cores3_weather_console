#pragma once

#include <lvgl.h>

#include "../weather/weather_models.h"
#include "ui_icons.h"
#include "ui_theme.h"

namespace ui {

class DailyPage {
 public:
	void begin(lv_obj_t* parent, ThemeManager& theme);
	void applyTheme(ThemeManager& theme);
	void update(const WeatherData& data);

 private:
	ThemeManager* theme_ = nullptr;
	lv_obj_t* root_ = nullptr;
	lv_obj_t* title_ = nullptr;
	lv_obj_t* subtitle_ = nullptr;
	lv_obj_t* list_ = nullptr;
	lv_obj_t* rowCards_[kMaxForecastDays] = {};
	lv_obj_t* rowIcons_[kMaxForecastDays] = {};
	lv_obj_t* rowLabels_[kMaxForecastDays] = {};
	lv_obj_t* rowTempLabels_[kMaxForecastDays] = {};
	IconId rowIconIds_[kMaxForecastDays] = {};
	bool rowIconSet_[kMaxForecastDays] = {};
};

}  // namespace ui
