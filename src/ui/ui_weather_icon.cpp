#include "ui_weather_icon.h"

namespace ui {

void WeatherIconView::begin(lv_obj_t* parent, uint16_t size) {
	size_ = size;
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, size_, size_);
	lv_obj_set_style_radius(root_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

	canvas_ = lv_canvas_create(root_);
	lv_canvas_set_buffer(canvas_, buffer_, kCanvasSize, kCanvasSize, LV_IMG_CF_TRUE_COLOR);
	lv_obj_set_size(canvas_, kCanvasSize, kCanvasSize);
	lv_obj_center(canvas_);
	lv_obj_set_style_bg_opa(canvas_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(canvas_, 0, LV_PART_MAIN);
}

void WeatherIconView::applyTheme(ThemeManager& theme) {
	theme_ = &theme;
	lv_obj_set_style_bg_color(root_, theme.palette().surfaceAlt, LV_PART_MAIN);
	render();
}

void WeatherIconView::setIcon(ThemeManager& theme, const String& token, lv_color_t color) {
	theme_ = &theme;
	token_ = token;
	strokeColor_ = color;
	lv_obj_set_style_bg_color(root_, theme.palette().surfaceAlt, LV_PART_MAIN);
	render();
}

lv_obj_t* WeatherIconView::root() const {
	return root_;
}

void WeatherIconView::render() {
	if (canvas_ == nullptr || theme_ == nullptr) {
		return;
	}
	lv_canvas_fill_bg(canvas_, theme_->palette().surfaceAlt, LV_OPA_COVER);
	drawToken();
}

void WeatherIconView::drawToken() {
	if (token_ == "sun") {
		drawSun();
		return;
	}
	if (token_ == "moon") {
		drawMoon();
		return;
	}
	if (token_ == "cloud" || token_ == "mostly_cloudy" || token_ == "overcast") {
		drawCloud();
		return;
	}
	if (token_ == "partly_cloudy") {
		drawPartlyCloudy();
		return;
	}
	if (token_ == "rain" || token_ == "showers") {
		drawRain();
		return;
	}
	if (token_ == "storm") {
		drawStorm();
		return;
	}
	if (token_ == "snow" || token_ == "flurries") {
		drawSnow();
		return;
	}
	if (token_ == "fog") {
		drawFog();
		return;
	}
	if (token_ == "wind") {
		drawWind();
		return;
	}
	if (token_ == "heat") {
		drawHeat();
		return;
	}
	if (token_ == "cold") {
		drawCold();
		return;
	}
	if (token_ == "mix") {
		drawMix();
		return;
	}
	if (token_ == "hail") {
		drawHail();
		return;
	}
	drawCloud();
}

void WeatherIconView::drawSun() {
	drawArc(18, 18, 6, 0, 360, 2);
	for (int index = 0; index < 8; ++index) {
		const int sx = (index == 0 || index == 1 || index == 7) ? 18 : (index < 4 ? 25 : 11);
		const int sy = (index == 1 || index == 2 || index == 3) ? 11 : (index > 4 ? 25 : 18);
		const int ex = (index == 0 || index == 1 || index == 7) ? 18 : (index < 4 ? 29 : 7);
		const int ey = (index == 1 || index == 2 || index == 3) ? 7 : (index > 4 ? 29 : 18);
		drawLine(sx, sy, ex, ey, 2);
	}
}

void WeatherIconView::drawMoon() {
	drawArc(18, 18, 8, 50, 310, 2);
	lv_draw_arc_dsc_t dsc;
	lv_draw_arc_dsc_init(&dsc);
	dsc.color = theme_->palette().surfaceAlt;
	dsc.width = 4;
	dsc.opa = LV_OPA_COVER;
	lv_canvas_draw_arc(canvas_, 21, 17, 8, 60, 300, &dsc);
}

void WeatherIconView::drawCloud() {
	drawArc(14, 18, 6, 180, 360, 2);
	drawArc(20, 15, 7, 180, 360, 2);
	drawArc(25, 18, 5, 180, 360, 2);
	drawLine(9, 18, 28, 18, 2);
	drawLine(10, 24, 27, 24, 2);
	drawLine(9, 18, 9, 24, 2);
	drawLine(28, 18, 28, 24, 2);
}

void WeatherIconView::drawRain() {
	drawCloud();
	drawLine(13, 27, 11, 31, 2);
	drawLine(20, 27, 18, 31, 2);
	drawLine(27, 27, 25, 31, 2);
}

void WeatherIconView::drawStorm() {
	drawCloud();
	drawLine(20, 25, 16, 31, 2);
	drawLine(16, 31, 20, 31, 2);
	drawLine(20, 31, 17, 35, 2);
}

void WeatherIconView::drawSnow() {
	drawCloud();
	drawSnowflake(14, 29, 3);
	drawSnowflake(22, 31, 3);
}

void WeatherIconView::drawFog() {
	drawLine(8, 14, 28, 14, 2);
	drawLine(10, 19, 30, 19, 2);
	drawLine(7, 24, 25, 24, 2);
	drawLine(12, 29, 28, 29, 2);
}

void WeatherIconView::drawWind() {
	drawArc(13, 15, 7, 200, 360, 2);
	drawLine(13, 15, 28, 15, 2);
	drawArc(18, 23, 8, 200, 350, 2);
	drawLine(11, 23, 28, 23, 2);
	drawArc(14, 30, 5, 200, 345, 2);
	drawLine(10, 30, 23, 30, 2);
}

void WeatherIconView::drawHeat() {
	drawSun();
	drawArc(12, 29, 3, 200, 340, 2);
	drawArc(20, 31, 3, 200, 340, 2);
}

void WeatherIconView::drawCold() {
	drawSnowflake(18, 18, 7);
}

void WeatherIconView::drawMix() {
	drawCloud();
	drawLine(13, 27, 11, 31, 2);
	drawSnowflake(24, 30, 3);
}

void WeatherIconView::drawHail() {
	drawCloud();
	drawDot(13, 30, 2);
	drawDot(20, 32, 2);
	drawDot(27, 30, 2);
}

void WeatherIconView::drawPartlyCloudy() {
	drawArc(12, 14, 5, 0, 360, 2);
	drawLine(12, 5, 12, 2, 2);
	drawLine(6, 9, 4, 7, 2);
	drawLine(18, 9, 20, 7, 2);
	drawCloud();
}

void WeatherIconView::drawLine(int x1, int y1, int x2, int y2, uint8_t width) {
	lv_draw_line_dsc_t dsc;
	lv_draw_line_dsc_init(&dsc);
	dsc.color = strokeColor_;
	dsc.width = width;
	dsc.round_end = 1;
	dsc.round_start = 1;
	lv_point_t points[2];
	points[0].x = x1;
	points[0].y = y1;
	points[1].x = x2;
	points[1].y = y2;
	lv_canvas_draw_line(canvas_, points, 2, &dsc);
}

void WeatherIconView::drawArc(int x, int y, int radius, int startAngle, int endAngle, uint8_t width) {
	lv_draw_arc_dsc_t dsc;
	lv_draw_arc_dsc_init(&dsc);
	dsc.color = strokeColor_;
	dsc.width = width;
	dsc.opa = LV_OPA_COVER;
	lv_canvas_draw_arc(canvas_, x, y, radius, startAngle, endAngle, &dsc);
}

void WeatherIconView::drawDot(int x, int y, uint8_t radius) {
	lv_draw_rect_dsc_t dsc;
	lv_draw_rect_dsc_init(&dsc);
	dsc.bg_opa = LV_OPA_COVER;
	dsc.bg_color = strokeColor_;
	dsc.radius = LV_RADIUS_CIRCLE;
	dsc.border_width = 0;
	lv_area_t area;
	area.x1 = x - radius;
	area.y1 = y - radius;
	area.x2 = x + radius;
	area.y2 = y + radius;
	lv_canvas_draw_rect(canvas_, area.x1, area.y1, area.x2 - area.x1 + 1, area.y2 - area.y1 + 1, &dsc);
}

void WeatherIconView::drawSnowflake(int x, int y, uint8_t arm) {
	drawLine(x - arm, y, x + arm, y, 1);
	drawLine(x, y - arm, x, y + arm, 1);
	drawLine(x - arm + 1, y - arm + 1, x + arm - 1, y + arm - 1, 1);
	drawLine(x - arm + 1, y + arm - 1, x + arm - 1, y - arm + 1, 1);
}

}  // namespace ui