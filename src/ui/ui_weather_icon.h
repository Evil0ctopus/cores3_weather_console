#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "ui_theme.h"

namespace ui {

class WeatherIconView {
 public:
	void begin(lv_obj_t* parent, uint16_t size = 34);
	void applyTheme(ThemeManager& theme);
	void setIcon(ThemeManager& theme, const String& token, lv_color_t color);
	lv_obj_t* root() const;

 private:
	static constexpr uint16_t kCanvasSize = 36;

	void render();
	void drawToken();
	void drawSun();
	void drawMoon();
	void drawCloud();
	void drawRain();
	void drawStorm();
	void drawSnow();
	void drawFog();
	void drawWind();
	void drawHeat();
	void drawCold();
	void drawMix();
	void drawHail();
	void drawPartlyCloudy();
	void drawLine(int x1, int y1, int x2, int y2, uint8_t width = 2);
	void drawArc(int x, int y, int radius, int startAngle, int endAngle, uint8_t width = 2);
	void drawDot(int x, int y, uint8_t radius = 2);
	void drawSnowflake(int x, int y, uint8_t arm = 4);

	lv_obj_t* root_ = nullptr;
	lv_obj_t* canvas_ = nullptr;
	lv_color_t buffer_[kCanvasSize * kCanvasSize] = {};
	ThemeManager* theme_ = nullptr;
	String token_ = "sky";
	lv_color_t strokeColor_ = lv_color_hex(0x5AB1FF);
	uint16_t size_ = 34;
};

}  // namespace ui