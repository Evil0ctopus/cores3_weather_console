#pragma once

#include <lvgl.h>

#include "../weather/weather_models.h"
#include "ui_icons.h"
#include "ui_theme.h"

namespace ui {

class HourlyPage {
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
	lv_obj_t* cards_[8] = {};
	lv_obj_t* timeLabels_[8] = {};
	lv_obj_t* iconObjs_[8] = {};
	lv_obj_t* tempLabels_[8] = {};
	lv_obj_t* detailLabels_[8] = {};
	IconId iconIds_[8] = {};
	bool iconSet_[8] = {};
	int lastConditionCode_ = -1;
	bool lastIsDaylight_ = true;
};

}  // namespace ui
