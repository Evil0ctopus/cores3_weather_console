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

void snapTileviewToTile(lv_obj_t* tileview, lv_obj_t* tile, bool animated) {
	if (tileview == nullptr || tile == nullptr) {
		return;
	}
	const lv_coord_t targetX = lv_obj_get_x(tile) - lv_obj_get_style_pad_left(tileview, LV_PART_MAIN);
	const lv_coord_t targetY = lv_obj_get_y(tile) - lv_obj_get_style_pad_top(tileview, LV_PART_MAIN);
	lv_obj_scroll_to_x(tileview, targetX, animated ? LV_ANIM_ON : LV_ANIM_OFF);
	lv_obj_scroll_to_y(tileview, targetY, animated ? LV_ANIM_ON : LV_ANIM_OFF);
}

lv_color_t batteryLevelColor(int percent) {
	if (percent <= 20) {
		return lv_color_hex(0xFF5C5C);
	}
	if (percent <= 50) {
		return lv_color_hex(0xF5C542);
	}
	return lv_color_hex(0x2ED573);
}

void styleStatusHud(lv_obj_t* object, const ui::ThemeManager& theme) {
	if (object == nullptr) {
		return;
	}
	lv_obj_set_style_bg_opa(object, LV_OPA_40, LV_PART_MAIN);
	lv_obj_set_style_bg_color(object, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_border_width(object, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(object, theme.palette().shadow, LV_PART_MAIN);
	lv_obj_set_style_radius(object, 14, LV_PART_MAIN);
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
	lv_obj_set_scroll_dir(tileview_, LV_DIR_NONE);
	lv_obj_clear_flag(tileview_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
	lv_obj_add_flag(tileview_, LV_OBJ_FLAG_SCROLL_ONE);
	lv_obj_set_scroll_snap_x(tileview_, LV_SCROLL_SNAP_NONE);
	lv_obj_set_scroll_snap_y(tileview_, LV_SCROLL_SNAP_NONE);
	lv_obj_set_style_anim_time(tileview_, 140, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(tileview_, LV_SCROLLBAR_MODE_OFF);

	currentTile_ = lv_tileview_add_tile(tileview_, 0, 0, LV_DIR_HOR);
	hourlyTile_ = lv_tileview_add_tile(tileview_, 1, 0, LV_DIR_HOR);
	dailyTile_ = lv_tileview_add_tile(tileview_, 2, 0, LV_DIR_HOR);
	radarTile_ = lv_tileview_add_tile(tileview_, 3, 0, LV_DIR_HOR);
	alertsTile_ = lv_tileview_add_tile(tileview_, 4, 0, LV_DIR_HOR);
	systemTile_ = lv_tileview_add_tile(tileview_, 5, 0, LV_DIR_HOR);
	if (currentTile_ == nullptr || hourlyTile_ == nullptr || dailyTile_ == nullptr || radarTile_ == nullptr || alertsTile_ == nullptr || systemTile_ == nullptr) {
		Serial.println("[UI] ERROR: Failed to create one or more tileview tiles.");
	}
	lv_obj_clear_flag(currentTile_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
	lv_obj_clear_flag(hourlyTile_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
	lv_obj_clear_flag(dailyTile_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
	lv_obj_clear_flag(radarTile_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
	lv_obj_clear_flag(alertsTile_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
	lv_obj_clear_flag(systemTile_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
	lv_obj_add_style(currentTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(hourlyTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(dailyTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(radarTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(alertsTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(systemTile_, theme_.tabStyle(), LV_PART_MAIN);
	applyTileSurface(currentTile_, theme_);
	applyTileSurface(hourlyTile_, theme_);
	applyTileSurface(dailyTile_, theme_);
	applyTileSurface(radarTile_, theme_);
	applyTileSurface(alertsTile_, theme_);
	applyTileSurface(systemTile_, theme_);
	lv_obj_set_style_border_width(currentTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(hourlyTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(dailyTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(radarTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(alertsTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(systemTile_, 0, LV_PART_MAIN);

	currentPage_.begin(currentTile_, theme_);
	hourlyPage_.begin(hourlyTile_, theme_);
	dailyPage_.begin(dailyTile_, theme_);
	radarPage_.begin(radarTile_, theme_);
	alertsPage_.begin(alertsTile_, theme_);
	systemPage_.begin(systemTile_, theme_, settingsStore, onThemeSelectedAdapter, this);

	timeHud_ = lv_obj_create(screen_);
	lv_obj_set_size(timeHud_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_set_style_pad_left(timeHud_, 10, LV_PART_MAIN);
	lv_obj_set_style_pad_right(timeHud_, 10, LV_PART_MAIN);
	lv_obj_set_style_pad_top(timeHud_, 4, LV_PART_MAIN);
	lv_obj_set_style_pad_bottom(timeHud_, 4, LV_PART_MAIN);
	lv_obj_clear_flag(timeHud_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
	styleStatusHud(timeHud_, theme_);
	lv_obj_align(timeHud_, LV_ALIGN_TOP_LEFT, 4, 2);

	timeLabel_ = lv_label_create(timeHud_);
	lv_obj_set_style_text_font(timeLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(timeLabel_, theme_.palette().textPrimary, LV_PART_MAIN);
	lv_label_set_text(timeLabel_, "--:--");

	statusHud_ = lv_obj_create(screen_);
	lv_obj_set_size(statusHud_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_set_style_pad_left(statusHud_, 8, LV_PART_MAIN);
	lv_obj_set_style_pad_right(statusHud_, 8, LV_PART_MAIN);
	lv_obj_set_style_pad_top(statusHud_, 4, LV_PART_MAIN);
	lv_obj_set_style_pad_bottom(statusHud_, 4, LV_PART_MAIN);
	lv_obj_set_style_pad_column(statusHud_, 8, LV_PART_MAIN);
	lv_obj_set_style_pad_row(statusHud_, 0, LV_PART_MAIN);
	lv_obj_set_flex_flow(statusHud_, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(statusHud_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_clear_flag(statusHud_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
	styleStatusHud(statusHud_, theme_);
	lv_obj_align(statusHud_, LV_ALIGN_TOP_RIGHT, -4, 2);

	lv_obj_t* wifiGroup = lv_obj_create(statusHud_);
	lv_obj_set_size(wifiGroup, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(wifiGroup, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(wifiGroup, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(wifiGroup, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_column(wifiGroup, 4, LV_PART_MAIN);
	lv_obj_set_flex_flow(wifiGroup, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(wifiGroup, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_clear_flag(wifiGroup, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

	wifiIcon_ = lv_label_create(wifiGroup);
	lv_obj_set_style_text_font(wifiIcon_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_label_set_text(wifiIcon_, LV_SYMBOL_WIFI);

	lv_obj_t* wifiBarsWrap = lv_obj_create(wifiGroup);
	lv_obj_set_size(wifiBarsWrap, 18, 12);
	lv_obj_set_style_bg_opa(wifiBarsWrap, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(wifiBarsWrap, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(wifiBarsWrap, 0, LV_PART_MAIN);
	lv_obj_clear_flag(wifiBarsWrap, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
	for (uint8_t i = 0; i < 4; ++i) {
		static const lv_coord_t kBarHeights[4] = {3, 5, 8, 10};
		wifiBars_[i] = lv_obj_create(wifiBarsWrap);
		lv_obj_set_size(wifiBars_[i], 3, kBarHeights[i]);
		lv_obj_set_style_radius(wifiBars_[i], 1, LV_PART_MAIN);
		lv_obj_set_style_border_width(wifiBars_[i], 0, LV_PART_MAIN);
		lv_obj_align(wifiBars_[i], LV_ALIGN_BOTTOM_LEFT, i * 4, 0);
	}

	batteryShell_ = lv_obj_create(statusHud_);
	lv_obj_set_size(batteryShell_, 46, 18);
	lv_obj_set_style_radius(batteryShell_, 4, LV_PART_MAIN);
	lv_obj_set_style_border_width(batteryShell_, 1, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(batteryShell_, LV_OPA_20, LV_PART_MAIN);
	lv_obj_set_style_pad_all(batteryShell_, 0, LV_PART_MAIN);
	lv_obj_clear_flag(batteryShell_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

	batteryFill_ = lv_obj_create(batteryShell_);
	lv_obj_set_pos(batteryFill_, 2, 2);
	lv_obj_set_size(batteryFill_, 20, 14);
	lv_obj_set_style_radius(batteryFill_, 3, LV_PART_MAIN);
	lv_obj_set_style_border_width(batteryFill_, 0, LV_PART_MAIN);

	batteryText_ = lv_label_create(batteryShell_);
	lv_obj_set_style_text_font(batteryText_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_align(batteryText_, LV_ALIGN_CENTER, 3, 0);
	lv_label_set_text(batteryText_, "--%");

	batteryCharge_ = lv_label_create(batteryShell_);
	lv_obj_set_style_text_font(batteryCharge_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_label_set_text(batteryCharge_, LV_SYMBOL_CHARGE);
	lv_obj_align(batteryCharge_, LV_ALIGN_LEFT_MID, 2, 0);
	lv_obj_add_flag(batteryCharge_, LV_OBJ_FLAG_HIDDEN);

	batteryHead_ = lv_obj_create(statusHud_);
	lv_obj_set_size(batteryHead_, 3, 8);
	lv_obj_set_style_radius(batteryHead_, 2, LV_PART_MAIN);
	lv_obj_set_style_border_width(batteryHead_, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(batteryHead_, LV_OPA_70, LV_PART_MAIN);

	lv_obj_move_foreground(statusHud_);
	lv_obj_align_to(batteryHead_, batteryShell_, LV_ALIGN_OUT_RIGHT_MID, 2, 0);

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
	lv_obj_add_style(hourlyTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(dailyTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(radarTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(alertsTile_, theme_.tabStyle(), LV_PART_MAIN);
	lv_obj_add_style(systemTile_, theme_.tabStyle(), LV_PART_MAIN);
	applyTileSurface(currentTile_, theme_);
	applyTileSurface(hourlyTile_, theme_);
	applyTileSurface(dailyTile_, theme_);
	applyTileSurface(radarTile_, theme_);
	applyTileSurface(alertsTile_, theme_);
	applyTileSurface(systemTile_, theme_);
	lv_obj_set_style_border_width(currentTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(hourlyTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(dailyTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(radarTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(alertsTile_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(systemTile_, 0, LV_PART_MAIN);
	currentPage_.applyTheme(theme_);
	hourlyPage_.applyTheme(theme_);
	dailyPage_.applyTheme(theme_);
	radarPage_.applyTheme(theme_);
	alertsPage_.applyTheme(theme_);
	systemPage_.applyTheme(theme_);
	if (timeHud_ != nullptr) {
		styleStatusHud(timeHud_, theme_);
	}
	if (timeLabel_ != nullptr) {
		lv_obj_set_style_text_color(timeLabel_, theme_.palette().textPrimary, LV_PART_MAIN);
	}
	if (statusHud_ != nullptr) {
		styleStatusHud(statusHud_, theme_);
	}
	if (wifiIcon_ != nullptr) {
		lv_obj_set_style_text_color(wifiIcon_, theme_.palette().accent, LV_PART_MAIN);
	}
	if (batteryShell_ != nullptr) {
		lv_obj_set_style_border_color(batteryShell_, theme_.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_bg_color(batteryShell_, theme_.palette().surface, LV_PART_MAIN);
	}
	if (batteryHead_ != nullptr) {
		lv_obj_set_style_bg_color(batteryHead_, theme_.palette().textPrimary, LV_PART_MAIN);
	}
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
	if (timeLabel_ != nullptr) {
		lv_label_set_text(timeLabel_, systemInfo.currentTime.c_str());
	}
	if (statusHud_ != nullptr) {
		const uint8_t wifiBars = systemInfo.wifiSignalBars > 4 ? 4 : systemInfo.wifiSignalBars;
		const lv_color_t wifiColor = systemInfo.wifiConnected ? theme_.palette().accent : theme_.palette().warning;
		const lv_color_t idleColor = theme_.palette().textSecondary;
		if (wifiIcon_ != nullptr) {
			lv_obj_set_style_text_color(wifiIcon_, wifiColor, LV_PART_MAIN);
			lv_label_set_text(wifiIcon_, LV_SYMBOL_WIFI);
		}
		for (uint8_t i = 0; i < 4; ++i) {
			if (wifiBars_[i] == nullptr) {
				continue;
			}
			const bool active = systemInfo.wifiConnected && i < wifiBars;
			lv_obj_set_style_bg_opa(wifiBars_[i], active ? LV_OPA_COVER : LV_OPA_20, LV_PART_MAIN);
			lv_obj_set_style_bg_color(wifiBars_[i], active ? wifiColor : idleColor, LV_PART_MAIN);
		}

		const int batteryPct = systemInfo.batteryPct < 0 ? 0 : (systemInfo.batteryPct > 100 ? 100 : systemInfo.batteryPct);
		const lv_color_t fillColor = batteryLevelColor(batteryPct);
		if (batteryFill_ != nullptr) {
			lv_coord_t fillWidth = static_cast<lv_coord_t>((40 * batteryPct) / 100);
			if (batteryPct > 0 && fillWidth < 5) {
				fillWidth = 5;
			}
			lv_obj_set_width(batteryFill_, fillWidth);
			lv_obj_set_style_bg_opa(batteryFill_, batteryPct > 0 ? LV_OPA_COVER : LV_OPA_10, LV_PART_MAIN);
			lv_obj_set_style_bg_color(batteryFill_, fillColor, LV_PART_MAIN);
		}
		if (batteryText_ != nullptr) {
			String batteryText = String(batteryPct) + "%";
			lv_label_set_text(batteryText_, batteryText.c_str());
			lv_obj_set_style_text_color(batteryText_, batteryPct > 25 ? lv_color_black() : lv_color_white(), LV_PART_MAIN);
		}
		if (batteryCharge_ != nullptr) {
			if (systemInfo.batteryCharging) {
				lv_obj_clear_flag(batteryCharge_, LV_OBJ_FLAG_HIDDEN);
				lv_obj_set_style_text_color(batteryCharge_, batteryPct > 25 ? lv_color_black() : lv_color_white(), LV_PART_MAIN);
			} else {
				lv_obj_add_flag(batteryCharge_, LV_OBJ_FLAG_HIDDEN);
			}
		}
		lv_obj_align_to(batteryHead_, batteryShell_, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
	}

	currentPage_.update(data, systemInfo);
	hourlyPage_.update(data);
	dailyPage_.update(data);
	radarPage_.update(data, radar);
	alertsPage_.update(data);
	systemPage_.update(systemInfo);
}

lv_obj_t* RootNavigator::tileForPage(uint8_t page) const {
	switch (page) {
		case 0: return currentTile_;
		case 1: return hourlyTile_;
		case 2: return dailyTile_;
		case 3: return radarTile_;
		case 4: return alertsTile_;
		case 5: return systemTile_;
		default: return currentTile_;
	}
}

uint8_t RootNavigator::activePageIndex() const {
	return currentPageIndex_;
}

void RootNavigator::recenterActivePage(bool animated) {
	if (tileview_ == nullptr) {
		return;
	}
	const uint8_t page = currentPageIndex_;
	lv_obj_set_tile_id(tileview_, page, 0, animated ? LV_ANIM_ON : LV_ANIM_OFF);
	lv_obj_update_layout(tileview_);
	lv_obj_t* activeTile = tileForPage(page);
	snapTileviewToTile(tileview_, activeTile, animated);
	lv_obj_update_layout(tileview_);
	invalidateTree(screen_);
}

bool RootNavigator::moveToAdjacentPage(int8_t delta, bool animated) {
	if (tileview_ == nullptr || delta == 0) {
		return false;
	}

	constexpr int16_t kPageCount = 6;
	const int16_t current = static_cast<int16_t>(currentPageIndex_);
	int16_t target = current + static_cast<int16_t>(delta);

	if (target < 0) {
		target = kPageCount - 1;
	} else if (target >= kPageCount) {
		target = 0;
	}

	currentPageIndex_ = static_cast<uint8_t>(target);
	lv_obj_set_tile_id(tileview_, currentPageIndex_, 0, animated ? LV_ANIM_ON : LV_ANIM_OFF);
	lv_obj_update_layout(tileview_);
	lv_obj_t* activeTile = tileForPage(currentPageIndex_);
	snapTileviewToTile(tileview_, activeTile, animated);
	lv_obj_update_layout(tileview_);
	lv_obj_update_layout(screen_);
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
