#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../weather/radar_engine.h"
#include "../weather/weather_models.h"
#include "ui_theme.h"

namespace ui {

class RadarPanel {
 public:
	void begin(lv_obj_t* parent, ThemeManager& theme);
	void applyTheme(ThemeManager& theme);
	void setDownloadProgress(size_t completed, size_t total, const char* stage);
	void update(const WeatherData& data, weather::RadarEngine& engine);

 private:
	lv_obj_t* card_ = nullptr;
	lv_obj_t* titleLabel_ = nullptr;
	lv_obj_t* metaLabel_ = nullptr;
	lv_obj_t* stageLabel_ = nullptr;
	lv_obj_t* imageFrame_ = nullptr;
	lv_obj_t* image_ = nullptr;
	lv_obj_t* locationDot_ = nullptr;
	lv_obj_t* attributionLabel_ = nullptr;
	lv_obj_t* alertLabel_ = nullptr;
};

}  // namespace ui
