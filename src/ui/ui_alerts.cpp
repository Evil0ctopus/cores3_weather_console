#include "ui_alerts.h"

namespace ui {
namespace {

void style_title_chip(lv_obj_t* obj, ThemeManager& theme, const char* text) {
	lv_obj_add_style(obj, theme.titleStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_color(obj, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(obj, 115, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(obj, LV_OPA_70, LV_PART_MAIN);
	lv_obj_set_style_bg_color(obj, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_radius(obj, 8, LV_PART_MAIN);
	lv_obj_set_style_pad_left(obj, 8, LV_PART_MAIN);
	lv_obj_set_style_pad_right(obj, 8, LV_PART_MAIN);
	lv_obj_set_style_pad_top(obj, 3, LV_PART_MAIN);
	lv_obj_set_style_pad_bottom(obj, 3, LV_PART_MAIN);
	lv_label_set_text(obj, text);
}

lv_color_t alert_color_for(const String& severity, ThemeManager* theme) {
	String level = severity;
	level.toLowerCase();
	if (level.indexOf("extreme") >= 0 || level.indexOf("warning") >= 0) {
		return theme != nullptr ? theme->palette().warning : lv_color_hex(0xFF6B6B);
	}
	if (level.indexOf("watch") >= 0) {
		return theme != nullptr ? theme->palette().accent : lv_color_hex(0x4FC3F7);
	}
	return theme != nullptr ? theme->palette().surfaceAlt : lv_color_hex(0x3A4455);
}

}  // namespace

void AlertsPage::begin(lv_obj_t* parent, ThemeManager& theme) {
	theme_ = &theme;
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
	lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(root_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	ui_style_card(root_, theme);

	title_ = lv_label_create(root_);
	style_title_chip(title_, theme, "Alerts");
	lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, 0);
	lv_obj_move_foreground(title_);

	subtitle_ = lv_label_create(root_);
	lv_obj_add_style(subtitle_, theme.chipStyle(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(subtitle_, LV_OPA_70, LV_PART_MAIN);
	lv_label_set_text(subtitle_, "All clear");
	lv_obj_align(subtitle_, LV_ALIGN_TOP_RIGHT, 0, 0);

	list_ = lv_obj_create(root_);
	lv_obj_set_size(list_, lv_pct(100), lv_pct(100));
	lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 0, 42);
	lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(list_, 8, LV_PART_MAIN);
	lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(list_, LV_DIR_VER);
	lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);

	for (size_t i = 0; i < kMaxWeatherAlerts; ++i) {
		rows_[i] = lv_label_create(list_);
		lv_obj_set_width(rows_[i], lv_pct(100));
		lv_obj_add_style(rows_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_color(rows_[i], theme.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_text_font(rows_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_transform_zoom(rows_[i], 110, LV_PART_MAIN);
		lv_obj_set_style_text_line_space(rows_[i], theme.typography().bodyLineSpace, LV_PART_MAIN);
		lv_obj_set_style_pad_all(rows_[i], 12, LV_PART_MAIN);
		lv_obj_set_style_radius(rows_[i], 16, LV_PART_MAIN);
		lv_label_set_long_mode(rows_[i], LV_LABEL_LONG_WRAP);
		lv_label_set_text(rows_[i], "No alert");
	}
}

void AlertsPage::applyTheme(ThemeManager& theme) {
	theme_ = &theme;
	ui_style_card(root_, theme);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	style_title_chip(title_, theme, "Alerts");
	lv_obj_add_style(subtitle_, theme.chipStyle(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(subtitle_, LV_OPA_70, LV_PART_MAIN);
	for (size_t i = 0; i < kMaxWeatherAlerts; ++i) {
		lv_obj_add_style(rows_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_color(rows_[i], theme.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_text_font(rows_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_pad_all(rows_[i], 12, LV_PART_MAIN);
	}
}

void AlertsPage::update(const WeatherData& data) {
	if (data.alertCount == 0) {
		if (data.lastError == WeatherErrorCode::NotConfigured) {
			lv_label_set_text(subtitle_, "Setup");
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			lv_label_set_text(subtitle_, "Offline");
		} else {
			lv_label_set_text(subtitle_, "All clear");
		}
	} else {
		lv_label_set_text(subtitle_, (String(data.alertCount) + " active").c_str());
	}

	for (size_t i = 0; i < kMaxWeatherAlerts; ++i) {
		if (i >= data.alertCount || !data.alerts[i].valid) {
			if (i == 0) {
				lv_obj_clear_flag(rows_[i], LV_OBJ_FLAG_HIDDEN);
				lv_obj_set_style_bg_opa(rows_[i], LV_OPA_40, LV_PART_MAIN);
				lv_obj_set_style_bg_color(rows_[i], theme_ != nullptr ? theme_->palette().surfaceAlt : lv_color_hex(0x2A3242), LV_PART_MAIN);
				if (data.lastError == WeatherErrorCode::NotConfigured) {
					lv_label_set_text(rows_[i], "Weather alerts need setup\nConfigure Wi-Fi and location in the Web UI.");
				} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
					lv_label_set_text(rows_[i], "Alerts paused\nWaiting for Wi-Fi before checking warnings.");
				} else {
					lv_label_set_text(rows_[i], "No active weather alerts\nConditions are currently calm.");
				}
			} else {
				lv_obj_add_flag(rows_[i], LV_OBJ_FLAG_HIDDEN);
			}
			continue;
		}

		lv_obj_clear_flag(rows_[i], LV_OBJ_FLAG_HIDDEN);
		const WeatherAlert& alert = data.alerts[i];
		const String severity = alert.severity.length() > 0 ? alert.severity : String("Advisory");
		lv_obj_set_style_bg_color(rows_[i], alert_color_for(severity, theme_), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(rows_[i], LV_OPA_40, LV_PART_MAIN);

		String line = severity;
		line += " • ";
		line += alert.category.length() > 0 ? alert.category : String("General");
		line += "\n";
		line += alert.title.length() > 0 ? alert.title : String("Weather alert");
		if (alert.description.length() > 0) {
			line += "\n";
			line += alert.description;
		}
		lv_label_set_text(rows_[i], line.c_str());
	}
}

}  // namespace ui
