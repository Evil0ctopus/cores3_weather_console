#include "ui_root.h"

namespace ui {

void RootNavigator::begin(lv_obj_t* screen, ThemeId themeId) {
	screen_ = screen;
	theme_.begin(themeId);

	lv_obj_add_style(screen_, theme_.screenStyle(), LV_PART_MAIN);

	tileview_ = lv_tileview_create(screen_);
	lv_obj_set_size(tileview_, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_opa(tileview_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(tileview_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(tileview_, theme_.spacing().screenPadding, LV_PART_MAIN);
	lv_obj_set_scroll_dir(tileview_, LV_DIR_HOR);
	lv_obj_set_scroll_snap_x(tileview_, LV_SCROLL_SNAP_CENTER);
	lv_obj_set_scroll_snap_y(tileview_, LV_SCROLL_SNAP_NONE);
	lv_obj_set_style_anim_time(tileview_, 260, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(tileview_, LV_SCROLLBAR_MODE_OFF);

	currentTile_ = lv_tileview_add_tile(tileview_, 0, 0, LV_DIR_HOR);
	weeklyTile_ = lv_tileview_add_tile(tileview_, 1, 0, LV_DIR_HOR);
	radarTile_ = lv_tileview_add_tile(tileview_, 2, 0, LV_DIR_HOR);
	alertsTile_ = lv_tileview_add_tile(tileview_, 3, 0, LV_DIR_HOR);
	systemTile_ = lv_tileview_add_tile(tileview_, 4, 0, LV_DIR_HOR);

	currentPage_.begin(currentTile_, theme_);
	weeklyPage_.begin(weeklyTile_, theme_);
	radarPage_.begin(radarTile_, theme_);
	alertsPage_.begin(alertsTile_, theme_);
	systemPage_.begin(systemTile_, theme_);

	debugOverlay_ = lv_label_create(screen_);
	lv_obj_set_width(debugOverlay_, lv_pct(98));
	lv_obj_set_style_text_font(debugOverlay_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(debugOverlay_, lv_color_hex(0xE7EEF9), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(debugOverlay_, LV_OPA_60, LV_PART_MAIN);
	lv_obj_set_style_bg_color(debugOverlay_, lv_color_hex(0x101A2A), LV_PART_MAIN);
	lv_obj_set_style_pad_hor(debugOverlay_, 8, LV_PART_MAIN);
	lv_obj_set_style_pad_ver(debugOverlay_, 6, LV_PART_MAIN);
	lv_obj_set_style_radius(debugOverlay_, 10, LV_PART_MAIN);
	lv_obj_set_style_border_width(debugOverlay_, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(debugOverlay_, lv_color_hex(0x5577A8), LV_PART_MAIN);
	lv_obj_align(debugOverlay_, LV_ALIGN_TOP_MID, 0, 2);
	lv_label_set_long_mode(debugOverlay_, LV_LABEL_LONG_WRAP);
	lv_label_set_text(debugOverlay_, "");
	lv_obj_add_flag(debugOverlay_, LV_OBJ_FLAG_HIDDEN);
}

void RootNavigator::setTheme(ThemeId themeId) {
	theme_.setTheme(themeId);
	lv_obj_add_style(screen_, theme_.screenStyle(), LV_PART_MAIN);
	lv_obj_set_style_pad_all(tileview_, theme_.spacing().screenPadding, LV_PART_MAIN);
	currentPage_.applyTheme(theme_);
	weeklyPage_.applyTheme(theme_);
	radarPage_.applyTheme(theme_);
	alertsPage_.applyTheme(theme_);
	systemPage_.applyTheme(theme_);
}

ThemeId RootNavigator::themeId() const {
	return theme_.themeId();
}

void RootNavigator::setRadarProgress(size_t completed, size_t total, const char* stage) {
	radarPage_.setDownloadProgress(completed, total, stage);
}

void RootNavigator::setDebugOverlayEnabled(bool enabled) {
	debugOverlayEnabled_ = enabled;
	if (debugOverlay_ == nullptr) {
		return;
	}
	if (enabled) {
		lv_obj_clear_flag(debugOverlay_, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(debugOverlay_, LV_OBJ_FLAG_HIDDEN);
	}
}

void RootNavigator::setDebugOverlayText(const String& text) {
	if (debugOverlay_ == nullptr || !debugOverlayEnabled_) {
		return;
	}
	lv_label_set_text(debugOverlay_, text.c_str());
}

void RootNavigator::update(const WeatherData& data, weather::RadarEngine& radar, const SystemInfo& systemInfo) {
	currentPage_.update(data, systemInfo);
	weeklyPage_.update(data);
	radarPage_.update(data, radar);
	alertsPage_.update(data);
	systemPage_.update(systemInfo);
}

uint8_t RootNavigator::activePageIndex() const {
	if (tileview_ == nullptr) {
		return 0;
	}

	lv_obj_t* activeTile = lv_tileview_get_tile_act(tileview_);
	if (activeTile == currentTile_) {
		return 0;
	}
	if (activeTile == weeklyTile_) {
		return 1;
	}
	if (activeTile == radarTile_) {
		return 2;
	}
	if (activeTile == alertsTile_) {
		return 3;
	}
	if (activeTile == systemTile_) {
		return 4;
	}
	return 0;
}

}  // namespace ui
