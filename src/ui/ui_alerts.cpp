#include "ui_alerts.h"

namespace ui {

void AlertsPage::begin(lv_obj_t* parent, ThemeManager& theme) {
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
	lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(root_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	
	// Apply semi-transparent card overlay style
	ui_style_card(root_, theme);

	title_ = lv_label_create(root_);
	lv_obj_set_style_text_color(title_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(title_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(title_, theme.typography().pageTitleZoom, LV_PART_MAIN);
	lv_label_set_text(title_, "Alerts");
	lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, 0);

	subtitle_ = lv_label_create(root_);
	lv_obj_add_style(subtitle_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(subtitle_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(subtitle_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_label_set_text(subtitle_, "No active warnings");
	lv_obj_align(subtitle_, LV_ALIGN_TOP_LEFT, 0, 52);

	list_ = lv_obj_create(root_);
	lv_obj_set_size(list_, lv_pct(100), lv_pct(100));
	lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 0, theme.spacing().alertsListTop);
	lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(list_, theme.spacing().listRowGap, LV_PART_MAIN);
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
		lv_obj_set_style_transform_zoom(rows_[i], theme.typography().alertRowZoom, LV_PART_MAIN);
		lv_obj_set_style_text_line_space(rows_[i], theme.typography().bodyLineSpace + 2, LV_PART_MAIN);
		lv_obj_set_style_pad_all(rows_[i], theme.spacing().cardAltPadding, LV_PART_MAIN);
		lv_label_set_long_mode(rows_[i], LV_LABEL_LONG_WRAP);
		lv_label_set_text(rows_[i], "No alert");
	}
}

void AlertsPage::applyTheme(ThemeManager& theme) {
	lv_obj_add_style(root_, theme.cardStyle(), LV_PART_MAIN);
	ui_make_container_transparent(root_);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	lv_obj_set_style_text_color(title_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(title_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(title_, theme.typography().pageTitleZoom, LV_PART_MAIN);
	lv_obj_add_style(subtitle_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(subtitle_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 0, theme.spacing().alertsListTop);
	lv_obj_set_style_pad_row(list_, theme.spacing().listRowGap, LV_PART_MAIN);
	for (size_t i = 0; i < kMaxWeatherAlerts; ++i) {
		lv_obj_add_style(rows_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_color(rows_[i], theme.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_text_font(rows_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_transform_zoom(rows_[i], theme.typography().alertRowZoom, LV_PART_MAIN);
		lv_obj_set_style_text_line_space(rows_[i], theme.typography().bodyLineSpace + 2, LV_PART_MAIN);
		lv_obj_set_style_pad_all(rows_[i], theme.spacing().cardAltPadding, LV_PART_MAIN);
	}
}

void AlertsPage::update(const WeatherData& data) {
	if (data.alertCount == 0) {
		if (data.lastError == WeatherErrorCode::NotConfigured) {
			lv_label_set_text(subtitle_, "Setup required");
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			lv_label_set_text(subtitle_, "Waiting for Wi-Fi");
		} else {
			lv_label_set_text(subtitle_, "No active warnings");
		}
	} else {
		lv_label_set_text(subtitle_, (String(data.alertCount) + " active").c_str());
	}

	for (size_t i = 0; i < kMaxWeatherAlerts; ++i) {
		if (i >= data.alertCount || !data.alerts[i].valid) {
			if (data.lastError == WeatherErrorCode::NotConfigured) {
				lv_label_set_text(rows_[i], i == 0 ? "Configure Wi-Fi and location in the Web UI to load alerts" : "Waiting for alert data");
			} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
				lv_label_set_text(rows_[i], i == 0 ? "Alerts will load after Wi-Fi connects" : "Waiting for alert data");
			} else {
				lv_label_set_text(rows_[i], "No alert");
			}
			continue;
		}

		const WeatherAlert& a = data.alerts[i];
		String line = a.severity.length() > 0 ? a.severity : String("Advisory");
		line += "  ";
		line += a.category.length() > 0 ? a.category : String("General");
		line += "\n";
		line += a.title.length() > 0 ? a.title : String("Weather alert");
		if (a.description.length() > 0) {
			line += "\n";
			line += a.description;
		}
		lv_label_set_text(rows_[i], line.c_str());
	}
}

}  // namespace ui
