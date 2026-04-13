#include "ui_root.h"

#include <Arduino.h>

#include "ui_backgrounds.h"

namespace {

void invalidateTree(lv_obj_t* object) {
	if (object == nullptr) {
		return;
	}
	lv_obj_invalidate(object);
	const uint32_t childCount = lv_obj_get_child_cnt(object);
	for (uint32_t index = 0; index < childCount; ++index) {
		invalidateTree(lv_obj_get_child(object, index));
	}
}

void applyTileSurface(lv_obj_t* object, const ui::ThemeManager& theme) {
	if (object == nullptr) {
		return;
	}
	lv_obj_set_style_bg_opa(object, LV_OPA_30, LV_PART_MAIN);
	lv_obj_set_style_bg_color(object, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_border_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_outline_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_shadow_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_radius(object, 0, LV_PART_MAIN);
}

}  // namespace

namespace ui {

void RootNavigator::begin(lv_obj_t* screen, app::SettingsStore& settingsStore) {
	screen_ = screen;
	settingsStore_ = &settingsStore;
	const ThemeId themeId = settingsStore.get_theme();
	theme_.begin(themeId);
	ui_theme_apply_to_root(screen_, themeId);
	ui_make_transparent(screen_);

	lv_obj_add_style(screen_, theme_.screenStyle(), LV_PART_MAIN);
	ui_backgrounds_init();
	backgroundLayer_ = ui_backgrounds_attach_to_root(screen_, themeId);
	if (backgroundLayer_ == nullptr) {
		Serial.println("[BG] ERROR: Failed to attach background layer to root.");
	}

	tileview_ = lv_tileview_create(screen_);
	if (tileview_ == nullptr) {
		Serial.println("[UI] ERROR: Failed to create tileview container.");
		return;
	}
	lv_obj_set_size(tileview_, lv_pct(100), lv_pct(100));
	lv_obj_add_style(tileview_, theme_.tabStyle(), LV_PART_MAIN);
	applyTileSurface(tileview_, theme_);
	lv_obj_set_style_border_width(tileview_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(tileview_, theme_.spacing().screenPadding, LV_PART_MAIN);
	lv_obj_set_scroll_dir(tileview_, LV_DIR_HOR);
	lv_obj_add_flag(tileview_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_scroll_snap_x(tileview_, LV_SCROLL_SNAP_CENTER);
	lv_obj_set_scroll_snap_y(tileview_, LV_SCROLL_SNAP_NONE);
	lv_obj_set_style_anim_time(tileview_, 140, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(tileview_, LV_SCROLLBAR_MODE_OFF);

	currentTile_ = lv_tileview_add_tile(tileview_, 0, 0, LV_DIR_HOR);
	weeklyTile_ = lv_tileview_add_tile(tileview_, 1, 0, LV_DIR_HOR);
	radarTile_ = lv_tileview_add_tile(tileview_, 2, 0, LV_DIR_HOR);
	alertsTile_ = lv_tileview_add_tile(tileview_, 3, 0, LV_DIR_HOR);
	systemTile_ = lv_tileview_add_tile(tileview_, 4, 0, LV_DIR_HOR);
	if (currentTile_ == nullptr || weeklyTile_ == nullptr || radarTile_ == nullptr || alertsTile_ == nullptr || systemTile_ == nullptr) {
		Serial.println("[UI] ERROR: Failed to create one or more tileview tiles.");
	}
	lv_obj_clear_flag(currentTile_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_clear_flag(weeklyTile_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_clear_flag(radarTile_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_clear_flag(alertsTile_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_clear_flag(systemTile_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(currentTile_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_add_flag(weeklyTile_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_add_flag(radarTile_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_add_flag(alertsTile_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_add_flag(systemTile_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_add_style(currentTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(weeklyTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(radarTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(alertsTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(systemTile_, theme_.tabStyle(), LV_PART_MAIN);
	applyTileSurface(currentTile_, theme_);
	applyTileSurface(weeklyTile_, theme_);
	applyTileSurface(radarTile_, theme_);
	applyTileSurface(alertsTile_, theme_);
	applyTileSurface(systemTile_, theme_);
	lv_obj_set_style_border_width(currentTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(weeklyTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(radarTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(alertsTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(systemTile_, 0, LV_PART_MAIN);

	currentPage_.begin(currentTile_, theme_);
	weeklyPage_.begin(weeklyTile_, theme_);
	radarPage_.begin(radarTile_, theme_);
	alertsPage_.begin(alertsTile_, theme_);
	systemPage_.begin(systemTile_, theme_, settingsStore, onThemeSelectedAdapter, this);

	debugOverlay_ = lv_label_create(screen_);
	lv_obj_set_width(debugOverlay_, lv_pct(98));
	lv_obj_set_style_text_font(debugOverlay_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(debugOverlay_, lv_color_black(), LV_PART_MAIN);
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
	Serial.println("[UI] Transparency applied");
}

void RootNavigator::setTheme(ThemeId themeId) {
	Serial.printf("[THEME] RootNavigator applying theme id=%d name=%s\n", static_cast<int>(themeId), theme_id_to_name(themeId));
	theme_.setTheme(themeId);
	ui_theme_apply_to_root(screen_, themeId);
	ui_make_transparent(screen_);
	lv_obj_add_style(screen_, theme_.screenStyle(), LV_PART_MAIN);
	ui_backgrounds_update_theme(screen_, themeId);
	lv_obj_add_style(tileview_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_set_style_pad_all(tileview_, theme_.spacing().screenPadding, LV_PART_MAIN);
	applyTileSurface(tileview_, theme_);
	lv_obj_set_style_border_width(tileview_, 0, LV_PART_MAIN);
	lv_obj_add_style(currentTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(weeklyTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(radarTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(alertsTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(systemTile_, theme_.tabStyle(), LV_PART_MAIN);
	applyTileSurface(currentTile_, theme_);
	applyTileSurface(weeklyTile_, theme_);
	applyTileSurface(radarTile_, theme_);
	applyTileSurface(alertsTile_, theme_);
	applyTileSurface(systemTile_, theme_);
	lv_obj_set_style_border_width(currentTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(weeklyTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(radarTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(alertsTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(systemTile_, 0, LV_PART_MAIN);
	currentPage_.applyTheme(theme_);
	weeklyPage_.applyTheme(theme_);
	radarPage_.applyTheme(theme_);
	alertsPage_.applyTheme(theme_);
	systemPage_.applyTheme(theme_);
	invalidateTree(screen_);
	Serial.println("[UI] Transparency applied");
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

bool RootNavigator::moveToAdjacentPage(int8_t delta, bool animated) {
	if (tileview_ == nullptr || delta == 0) {
		return false;
	}

	int16_t target = static_cast<int16_t>(activePageIndex()) + static_cast<int16_t>(delta);
	if (target < 0) {
		target = 0;
	}
	if (target > 4) {
		target = 4;
	}

	lv_obj_set_tile_id(tileview_, target, 0, animated ? LV_ANIM_ON : LV_ANIM_OFF);
	invalidateTree(screen_);
	return true;
}

void RootNavigator::onThemeSelectedAdapter(void* userContext, ThemeId themeId) {
	if (userContext == nullptr) {
		return;
	}
	static_cast<RootNavigator*>(userContext)->onThemeSelected(themeId);
}

void RootNavigator::onThemeSelected(ThemeId themeId) {
	setTheme(themeId);
}

}  // namespace ui
