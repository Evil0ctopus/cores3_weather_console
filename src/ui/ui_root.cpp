#include "ui_root.h"

#include <Arduino.h>
#include <string.h>

#include "ui_backgrounds.h"
#include "ui_assets.h"
#include "ui_icons.h"
#include "ui_units.h"

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
	lv_obj_set_style_bg_opa(object, theme.themeId() == ui::ThemeId::PIXEL_STORM ? LV_OPA_TRANSP : LV_OPA_30, LV_PART_MAIN);
	lv_obj_set_style_bg_color(object, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_border_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_outline_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_shadow_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_radius(object, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

void snapTileviewToTile(lv_obj_t* tileview, lv_obj_t* tile, bool animated) {
	if (tileview == nullptr || tile == nullptr) {
		return;
	}
	const lv_coord_t targetX = lv_obj_get_x(tile);
	const lv_coord_t targetY = lv_obj_get_y(tile);
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
	lv_obj_set_style_bg_opa(object, LV_OPA_70, LV_PART_MAIN);
	lv_obj_set_style_bg_color(object, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_border_width(object, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(object, theme.palette().shadow, LV_PART_MAIN);
	lv_obj_set_style_border_opa(object, LV_OPA_40, LV_PART_MAIN);
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
	lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
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
	lv_obj_set_width(tileview_, lv_pct(100));
	lv_obj_set_height(tileview_, lv_obj_get_height(screen_) - 32);
	lv_obj_set_pos(tileview_, 0, 32);
	lv_obj_add_style(tileview_, theme_.tabStyle(), LV_PART_MAIN);
	applyTileSurface(tileview_, theme_);
	lv_obj_set_style_border_width(tileview_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(tileview_, 0, LV_PART_MAIN);
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

	homePanel_ = lv_obj_create(screen_);
	lv_obj_remove_style_all(homePanel_);
	lv_obj_set_size(homePanel_, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_opa(homePanel_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_bg_color(homePanel_, theme_.palette().bg, LV_PART_MAIN);
	lv_obj_set_style_pad_all(homePanel_, 8, LV_PART_MAIN);
	lv_obj_clear_flag(homePanel_, LV_OBJ_FLAG_SCROLLABLE);
	homeBackdrop_ = lv_obj_create(screen_);
	lv_obj_remove_style_all(homeBackdrop_);
	lv_obj_set_size(homeBackdrop_, 320, 240);
	lv_obj_set_pos(homeBackdrop_, 0, 0);
	lv_obj_set_style_bg_opa(homeBackdrop_, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(homeBackdrop_, lv_color_hex(0x050919), LV_PART_MAIN);
	lv_obj_set_style_bg_grad_color(homeBackdrop_, lv_color_hex(0x191039), LV_PART_MAIN);
	lv_obj_set_style_bg_grad_dir(homeBackdrop_, LV_GRAD_DIR_VER, LV_PART_MAIN);
	lv_obj_clear_flag(homeBackdrop_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	for (uint8_t spark = 0; spark < 6; ++spark) {
		stormSparks_[spark] = lv_obj_create(homeBackdrop_);
		lv_obj_remove_style_all(stormSparks_[spark]);
		lv_obj_set_size(stormSparks_[spark], 2, 2);
		lv_obj_set_style_radius(stormSparks_[spark], LV_RADIUS_CIRCLE, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(stormSparks_[spark], LV_OPA_30, LV_PART_MAIN);
		lv_obj_set_style_bg_color(stormSparks_[spark], lv_color_hex(0xB9DEFF), LV_PART_MAIN);
		lv_obj_set_pos(stormSparks_[spark], 22 + spark * 49, 40 + (spark * 37) % 180);
		lv_obj_clear_flag(stormSparks_[spark], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	}
	lv_obj_move_to_index(homeBackdrop_, 1);

	lv_obj_t* hubTitle = lv_label_create(homePanel_);
	homeTitle_ = hubTitle;
	lv_obj_set_style_text_font(hubTitle, &lv_font_montserrat_16, LV_PART_MAIN);
	lv_obj_set_style_text_color(hubTitle, theme_.palette().accent, LV_PART_MAIN);
	lv_label_set_text(hubTitle, "WEATHER ATLAS");
	lv_obj_align(hubTitle, LV_ALIGN_TOP_LEFT, 2, 28);

	homeReadout_ = lv_label_create(homePanel_);
	lv_obj_set_width(homeReadout_, 208);
	lv_obj_set_style_text_font(homeReadout_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(homeReadout_, theme_.palette().textSecondary, LV_PART_MAIN);
	lv_label_set_long_mode(homeReadout_, LV_LABEL_LONG_DOT);
	lv_label_set_text(homeReadout_, "Waiting for weather data");
	lv_obj_align(homeReadout_, LV_ALIGN_TOP_LEFT, 2, 48);

	static const char* kHubPageLabels[6] = {"CURRENT", "HOURLY", "7 DAY", "RADAR", "ALERTS", "SYSTEM"};
	static const IconId kHubIcons[6] = {IconId::ICON_CLEAR_DAY, IconId::ICON_CLEAR_NIGHT,
		IconId::ICON_PARTLY_CLOUDY, IconId::ICON_NAV_RADAR, IconId::ICON_NAV_ALERTS, IconId::ICON_NAV_SYSTEM};
	for (uint8_t page = 0; page < 6; ++page) {
		const lv_coord_t column = page % 2;
		const lv_coord_t row = page / 2;
		lv_obj_t* button = lv_btn_create(homePanel_);
		hubButtons_[page] = button;
		lv_obj_set_size(button, 148, 44);
		lv_obj_set_pos(button, column == 0 ? 0 : 156, 78 + row * 48);
		lv_obj_set_style_radius(button, 4, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
		lv_obj_set_style_bg_color(button, theme_.palette().surfaceAlt, LV_PART_MAIN);
		lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
		lv_obj_set_style_border_color(button, theme_.palette().shadow, LV_PART_MAIN);
		lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
		lv_obj_set_style_pad_all(button, 4, LV_PART_MAIN);
		lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);

		lv_obj_t* label = lv_label_create(button);
		lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_text_color(label, theme_.palette().textPrimary, LV_PART_MAIN);
		lv_label_set_text(label, kHubPageLabels[page]);
		lv_obj_align(label, LV_ALIGN_CENTER, 12, 0);
		lv_obj_t* icon = ui_icon_create(button, kHubIcons[page], themeId);
		if (icon != nullptr) {
			ui_icon_set_size(icon, 26, 26);
			lv_obj_align(icon, LV_ALIGN_LEFT_MID, 1, 0);
		}

		hubButtonContexts_[page].navigator = this;
		hubButtonContexts_[page].page = page;
		lv_obj_add_event_cb(button, onHubPageSelected, LV_EVENT_CLICKED, &hubButtonContexts_[page]);
	}
	styleHome();
	homeAnimation_ = lv_timer_create(animateStorm, 100, this);
	if (homeAnimation_ == nullptr) {
		Serial.println("[UI] ERROR: could not create storm animation timer.");
	}

	homeButton_ = lv_btn_create(screen_);
	lv_obj_set_size(homeButton_, 38, 26);
	lv_obj_align(homeButton_, LV_ALIGN_TOP_MID, 0, 2);
	lv_obj_set_ext_click_area(homeButton_, 18);
	lv_obj_set_style_radius(homeButton_, 9, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(homeButton_, LV_OPA_80, LV_PART_MAIN);
	lv_obj_set_style_bg_color(homeButton_, theme_.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_border_width(homeButton_, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(homeButton_, theme_.palette().accent, LV_PART_MAIN);
	lv_obj_set_style_border_opa(homeButton_, LV_OPA_40, LV_PART_MAIN);
	lv_obj_clear_flag(homeButton_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_t* homeButtonIcon = ui_icon_create(homeButton_, IconId::ICON_NAV_HOME, themeId);
	if (homeButtonIcon != nullptr) {
		ui_icon_set_size(homeButtonIcon, 22, 22);
		lv_obj_center(homeButtonIcon);
	}
	lv_obj_add_event_cb(homeButton_, onHomeSelected, LV_EVENT_CLICKED, this);
	lv_obj_add_flag(homeButton_, LV_OBJ_FLAG_HIDDEN);
	homeVisible_ = true;
	lv_obj_add_flag(tileview_, LV_OBJ_FLAG_HIDDEN);
	lv_obj_move_foreground(homePanel_);
	lv_obj_move_foreground(timeHud_);
	lv_obj_move_foreground(statusHud_);
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
	lv_obj_set_style_pad_all(tileview_, 0, LV_PART_MAIN);
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
	if (homePanel_ != nullptr) {
		lv_obj_set_style_bg_color(homePanel_, theme_.palette().bg, LV_PART_MAIN);
	}
	for (lv_obj_t* button : hubButtons_) {
		if (button != nullptr) {
			lv_obj_set_style_bg_color(button, theme_.palette().surfaceAlt, LV_PART_MAIN);
			lv_obj_set_style_border_color(button, theme_.palette().shadow, LV_PART_MAIN);
		}
	}
	if (homeReadout_ != nullptr) {
		lv_obj_set_style_text_color(homeReadout_, theme_.palette().textSecondary, LV_PART_MAIN);
	}
	if (homeButton_ != nullptr) {
		lv_obj_set_style_bg_color(homeButton_, theme_.palette().surfaceAlt, LV_PART_MAIN);
		lv_obj_set_style_border_color(homeButton_, theme_.palette().accent, LV_PART_MAIN);
	}
	styleHome();
	if (!homeVisible_ && themeId != ThemeId::PIXEL_STORM) {
		lv_obj_add_flag(homeBackdrop_, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_clear_flag(homeBackdrop_, LV_OBJ_FLAG_HIDDEN);
	}
	invalidateTree(screen_);
	Serial.println("[UI] Transparency applied");
}

void RootNavigator::styleHome() {
	if (homePanel_ == nullptr) {
		return;
	}
	lv_obj_set_style_bg_color(homePanel_, lv_color_hex(0x050919), LV_PART_MAIN);
	const bool cinematic = theme_.themeId() == ThemeId::PIXEL_STORM;
	lv_obj_set_style_bg_opa(homeBackdrop_, cinematic ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_text_color(homeTitle_, lv_color_hex(0xF0F7FF), LV_PART_MAIN);
	lv_obj_set_style_text_letter_space(homeTitle_, 1, LV_PART_MAIN);
	lv_obj_set_style_text_color(homeReadout_, lv_color_hex(0xBCCAE8), LV_PART_MAIN);
	for (uint8_t page = 0; page < 6; ++page) {
		lv_obj_t* button = hubButtons_[page];
		if (button == nullptr) {
			continue;
		}
		const lv_color_t accent = lv_color_hex(page % 2 == 0 ? 0x73DFFF : 0xAA91ED);
		lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(button, LV_OPA_80, LV_PART_MAIN);
		lv_obj_set_style_bg_color(button, lv_color_hex(0x0B1730), LV_PART_MAIN);
		lv_obj_set_style_bg_grad_color(button, lv_color_hex(page % 2 == 0 ? 0x152B43 : 0x26233F), LV_PART_MAIN);
		lv_obj_set_style_bg_grad_dir(button, LV_GRAD_DIR_VER, LV_PART_MAIN);
		lv_obj_set_style_border_color(button, accent, LV_PART_MAIN);
		lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
		lv_obj_set_style_border_opa(button, LV_OPA_40, LV_PART_MAIN);
		lv_obj_set_style_bg_color(button, lv_color_hex(0x30516B), LV_PART_MAIN | LV_STATE_PRESSED);
		lv_obj_t* label = lv_obj_get_child(button, 0);
		lv_obj_set_style_text_color(label, lv_color_hex(0xF0F7FF), LV_PART_MAIN);
	}
}

void RootNavigator::animateStorm(lv_timer_t* timer) {
	RootNavigator* navigator = static_cast<RootNavigator*>(timer->user_data);
	if (lv_obj_has_flag(navigator->homeBackdrop_, LV_OBJ_FLAG_HIDDEN)) {
		return;
	}
	const uint16_t phase = (lv_tick_get() % 8000U) * 360U / 8000U;
	const uint8_t spark = (lv_tick_get() / 100U) % 6;
	const int32_t wave = lv_trigo_sin((phase + spark * 57) % 360);
	lv_obj_set_style_bg_opa(navigator->stormSparks_[spark], 75 + wave * 50 / 32767, LV_PART_MAIN);
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
	const bool imperial = settingsStore_ != nullptr &&
		settingsStore_->settings().units == app::UnitsSystem::Imperial;
	if (homeReadout_ != nullptr) {
		String readout = data.locationName.length() > 0 ? data.locationName : String("LOCATION PENDING");
		readout += "  |  ";
		if (data.current.valid) {
			readout += format_temperature(data.current.temperatureC, imperial) + "  " + data.current.summary;
		} else {
			readout += "WEATHER DATA PENDING";
		}
		ui_label_set_text_if_changed(homeReadout_, readout.c_str());
	}
	if (timeLabel_ != nullptr) {
		ui_label_set_text_if_changed(timeLabel_, systemInfo.currentTime.c_str());
	}
	if (statusHud_ != nullptr) {
		const uint8_t wifiBars = systemInfo.wifiSignalBars > 4 ? 4 : systemInfo.wifiSignalBars;
		const lv_color_t wifiColor = systemInfo.wifiConnected ? theme_.palette().accent : theme_.palette().warning;
		const lv_color_t idleColor = theme_.palette().textSecondary;
		if (wifiIcon_ != nullptr) {
			if (lv_obj_get_style_text_color(wifiIcon_, LV_PART_MAIN).full != wifiColor.full) {
				lv_obj_set_style_text_color(wifiIcon_, wifiColor, LV_PART_MAIN);
			}
			ui_label_set_text_if_changed(wifiIcon_, LV_SYMBOL_WIFI);
		}
		for (uint8_t i = 0; i < 4; ++i) {
			if (wifiBars_[i] == nullptr) {
				continue;
			}
			const bool active = systemInfo.wifiConnected && i < wifiBars;
			const lv_opa_t opacity = active ? LV_OPA_COVER : LV_OPA_20;
			const lv_color_t color = active ? wifiColor : idleColor;
			if (lv_obj_get_style_bg_opa(wifiBars_[i], LV_PART_MAIN) != opacity) {
				lv_obj_set_style_bg_opa(wifiBars_[i], opacity, LV_PART_MAIN);
			}
			if (lv_obj_get_style_bg_color(wifiBars_[i], LV_PART_MAIN).full != color.full) {
				lv_obj_set_style_bg_color(wifiBars_[i], color, LV_PART_MAIN);
			}
		}

		const int batteryPct = systemInfo.batteryPct < 0 ? 0 : (systemInfo.batteryPct > 100 ? 100 : systemInfo.batteryPct);
		const lv_color_t fillColor = batteryLevelColor(batteryPct);
		if (batteryFill_ != nullptr) {
			lv_coord_t fillWidth = static_cast<lv_coord_t>((40 * batteryPct) / 100);
			if (batteryPct > 0 && fillWidth < 5) {
				fillWidth = 5;
			}
			lv_obj_set_width(batteryFill_, fillWidth);
			const lv_opa_t opacity = batteryPct > 0 ? LV_OPA_COVER : LV_OPA_10;
			if (lv_obj_get_style_bg_opa(batteryFill_, LV_PART_MAIN) != opacity) {
				lv_obj_set_style_bg_opa(batteryFill_, opacity, LV_PART_MAIN);
			}
			if (lv_obj_get_style_bg_color(batteryFill_, LV_PART_MAIN).full != fillColor.full) {
				lv_obj_set_style_bg_color(batteryFill_, fillColor, LV_PART_MAIN);
			}
		}
		if (batteryText_ != nullptr) {
			String batteryText = String(batteryPct) + "%";
			ui_label_set_text_if_changed(batteryText_, batteryText.c_str());
			const lv_color_t color = batteryPct > 25 ? lv_color_black() : lv_color_white();
			if (lv_obj_get_style_text_color(batteryText_, LV_PART_MAIN).full != color.full) {
				lv_obj_set_style_text_color(batteryText_, color, LV_PART_MAIN);
			}
		}
		if (batteryCharge_ != nullptr) {
			if (systemInfo.batteryCharging) {
				lv_obj_clear_flag(batteryCharge_, LV_OBJ_FLAG_HIDDEN);
				const lv_color_t color = batteryPct > 25 ? lv_color_black() : lv_color_white();
				if (lv_obj_get_style_text_color(batteryCharge_, LV_PART_MAIN).full != color.full) {
					lv_obj_set_style_text_color(batteryCharge_, color, LV_PART_MAIN);
				}
			} else {
				lv_obj_add_flag(batteryCharge_, LV_OBJ_FLAG_HIDDEN);
			}
		}
		lv_obj_align_to(batteryHead_, batteryShell_, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
	}

	if (!weatherPagesPrimed_ && data.current.valid) {
		currentPage_.update(data, systemInfo, imperial);
		hourlyPage_.update(data, imperial);
		dailyPage_.update(data, imperial);
		radarPage_.update(data, radar);
		alertsPage_.update(data);
		systemPage_.update(systemInfo);
		weatherPagesPrimed_ = true;
	} else if (!homeVisible_) {
		switch (currentPageIndex_) {
			case 0: currentPage_.update(data, systemInfo, imperial); break;
			case 1: hourlyPage_.update(data, imperial); break;
			case 2: dailyPage_.update(data, imperial); break;
			case 3: radarPage_.update(data, radar); break;
			case 4: alertsPage_.update(data); break;
			case 5: systemPage_.update(systemInfo); break;
		}
	}
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
	return homeVisible_ ? 6 : currentPageIndex_;
}

void RootNavigator::showHome() {
	if (homePanel_ == nullptr) {
		return;
	}
	homeVisible_ = true;
	lv_obj_clear_flag(homeBackdrop_, LV_OBJ_FLAG_HIDDEN);
	if (homeAnimation_ != nullptr) {
		lv_timer_resume(homeAnimation_);
	}
	lv_obj_add_flag(tileview_, LV_OBJ_FLAG_HIDDEN);
	lv_obj_clear_flag(homePanel_, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(homeButton_, LV_OBJ_FLAG_HIDDEN);
	lv_obj_move_foreground(homePanel_);
	lv_obj_move_foreground(timeHud_);
	lv_obj_move_foreground(statusHud_);
}

void RootNavigator::openPage(uint8_t page, bool animated) {
	if (tileview_ == nullptr || page >= 6) {
		return;
	}
	currentPageIndex_ = page;
	const bool fromHome = homeVisible_;
	homeVisible_ = false;
	if (theme_.themeId() != ThemeId::PIXEL_STORM) {
		lv_obj_add_flag(homeBackdrop_, LV_OBJ_FLAG_HIDDEN);
	}
	lv_obj_add_flag(homePanel_, LV_OBJ_FLAG_HIDDEN);
	lv_obj_clear_flag(tileview_, LV_OBJ_FLAG_HIDDEN);
	lv_obj_clear_flag(homeButton_, LV_OBJ_FLAG_HIDDEN);
	const bool slide = animated && !fromHome && theme_.themeId() != ThemeId::PIXEL_STORM;
	lv_obj_set_tile_id(tileview_, currentPageIndex_, 0, slide ? LV_ANIM_ON : LV_ANIM_OFF);
	lv_obj_update_layout(tileview_);
	snapTileviewToTile(tileview_, tileForPage(currentPageIndex_), slide);
	lv_obj_update_layout(tileview_);
	lv_obj_move_foreground(homeButton_);
}

void RootNavigator::recenterActivePage(bool animated) {
	if (tileview_ == nullptr) {
		return;
	}
	const uint8_t page = currentPageIndex_;
	const bool slide = animated && theme_.themeId() != ThemeId::PIXEL_STORM;
	lv_obj_set_tile_id(tileview_, page, 0, slide ? LV_ANIM_ON : LV_ANIM_OFF);
	lv_obj_update_layout(tileview_);
	lv_obj_t* activeTile = tileForPage(page);
	snapTileviewToTile(tileview_, activeTile, slide);
	lv_obj_update_layout(tileview_);
	invalidateTree(screen_);
}

bool RootNavigator::isPageFullyVisible() const {
	if (homeVisible_) {
		return true;
	}
	if (tileview_ == nullptr) {
		return false;
	}
	lv_obj_t* tile = tileForPage(currentPageIndex_);
	return lv_obj_get_scroll_x(tileview_) == lv_obj_get_x(tile) &&
		lv_obj_get_scroll_y(tileview_) == lv_obj_get_y(tile);
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

	openPage(static_cast<uint8_t>(target), animated);
	return true;
}

void RootNavigator::onHubPageSelected(lv_event_t* event) {
	if (event == nullptr) {
		return;
	}
	HubPageButtonContext* context = static_cast<HubPageButtonContext*>(lv_event_get_user_data(event));
	if (context != nullptr && context->navigator != nullptr) {
		context->navigator->openPage(context->page);
	}
}

void RootNavigator::onHomeSelected(lv_event_t* event) {
	if (event == nullptr) {
		return;
	}
	RootNavigator* navigator = static_cast<RootNavigator*>(lv_event_get_user_data(event));
	if (navigator != nullptr) {
		navigator->showHome();
	}
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
