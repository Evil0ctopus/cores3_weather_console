#include "ui_current.h"

#include <math.h>

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

void style_chip(lv_obj_t* obj, ThemeManager& theme) {
	lv_obj_add_style(obj, theme.chipStyle(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(obj, LV_OPA_80, LV_PART_MAIN);
	lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN);
}

void set_metric(lv_obj_t* valueLabel, const String& value) {
	if (valueLabel != nullptr) {
		lv_label_set_text(valueLabel, value.c_str());
	}
}

}  // namespace

void CurrentPage::begin(lv_obj_t* parent, ThemeManager& theme) {
	theme_ = &theme;
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
	lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(root_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().currentCardPadding, LV_PART_MAIN);
	ui_style_card(root_, theme);

	titleLabel_ = lv_label_create(root_);
	style_title_chip(titleLabel_, theme, "Current");
	lv_obj_align(titleLabel_, LV_ALIGN_TOP_LEFT, 0, 0);
	lv_obj_move_foreground(titleLabel_);

	scroll_ = lv_obj_create(root_);
	lv_obj_set_size(scroll_, lv_pct(100), lv_pct(100));
	lv_obj_align(scroll_, LV_ALIGN_TOP_LEFT, 0, 30);
	lv_obj_set_style_bg_opa(scroll_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(scroll_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(scroll_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(scroll_, 10, LV_PART_MAIN);
	lv_obj_set_flex_flow(scroll_, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(scroll_, LV_DIR_VER);
	lv_obj_add_flag(scroll_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_scrollbar_mode(scroll_, LV_SCROLLBAR_MODE_OFF);

	heroCard_ = lv_obj_create(scroll_);
	lv_obj_set_width(heroCard_, lv_pct(100));
	lv_obj_set_height(heroCard_, 126);
	lv_obj_add_style(heroCard_, theme.cardAltStyle(), LV_PART_MAIN);
	lv_obj_set_style_border_width(heroCard_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(heroCard_, 12, LV_PART_MAIN);

	cityLabel_ = lv_label_create(heroCard_);
	lv_obj_add_style(cityLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(cityLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(cityLabel_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_label_set_text(cityLabel_, "Location");
	lv_obj_align(cityLabel_, LV_ALIGN_TOP_LEFT, 0, 0);

	statusChip_ = lv_label_create(heroCard_);
	style_chip(statusChip_, theme);
	lv_label_set_text(statusChip_, "Live");
	lv_obj_align(statusChip_, LV_ALIGN_TOP_RIGHT, 0, 0);

	iconObj_ = ui_icon_create(heroCard_, IconId::ICON_CLEAR_DAY, theme.themeId());
	if (iconObj_ != nullptr) {
		ui_icon_set_size(iconObj_, 56, 56);
		lv_obj_align(iconObj_, LV_ALIGN_RIGHT_MID, -2, 12);
		displayedIconId_ = IconId::ICON_CLEAR_DAY;
		hasDisplayedIcon_ = true;
	}

	tempLabel_ = lv_label_create(heroCard_);
	lv_obj_add_style(tempLabel_, theme.titleStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_color(tempLabel_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(tempLabel_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_text_font(tempLabel_, &lv_font_montserrat_48, LV_PART_MAIN);
	lv_obj_set_style_text_letter_space(tempLabel_, -2, LV_PART_MAIN);
	lv_obj_set_width(tempLabel_, 150);
	lv_label_set_long_mode(tempLabel_, LV_LABEL_LONG_CLIP);
	lv_label_set_text(tempLabel_, "-- C");
	lv_obj_align(tempLabel_, LV_ALIGN_LEFT_MID, 0, 4);
	lv_obj_move_foreground(tempLabel_);

	summaryLabel_ = lv_label_create(heroCard_);
	lv_obj_add_style(summaryLabel_, theme.bodyStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(summaryLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(summaryLabel_, 145, LV_PART_MAIN);
	lv_obj_set_style_text_line_space(summaryLabel_, theme.typography().bodyLineSpace, LV_PART_MAIN);
	lv_obj_set_width(summaryLabel_, lv_pct(56));
	lv_label_set_long_mode(summaryLabel_, LV_LABEL_LONG_WRAP);
	lv_label_set_text(summaryLabel_, "Waiting for current conditions");
	lv_obj_align(summaryLabel_, LV_ALIGN_BOTTOM_LEFT, 0, 4);

	metricsWrap_ = lv_obj_create(scroll_);
	lv_obj_set_width(metricsWrap_, lv_pct(100));
	lv_obj_set_height(metricsWrap_, LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(metricsWrap_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(metricsWrap_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(metricsWrap_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(metricsWrap_, 8, LV_PART_MAIN);
	lv_obj_set_style_pad_column(metricsWrap_, 8, LV_PART_MAIN);
	lv_obj_set_flex_flow(metricsWrap_, LV_FLEX_FLOW_ROW_WRAP);
	lv_obj_clear_flag(metricsWrap_, LV_OBJ_FLAG_SCROLLABLE);

	static const char* metricNames[6] = {"Feels like", "Humidity", "Wind", "Pressure", "Updated", "Alerts"};
	for (size_t i = 0; i < 6; ++i) {
		metricCards_[i] = lv_obj_create(metricsWrap_);
		lv_obj_set_size(metricCards_[i], lv_pct(48), 82);
		lv_obj_add_style(metricCards_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_border_width(metricCards_[i], 0, LV_PART_MAIN);
		lv_obj_set_style_pad_all(metricCards_[i], 10, LV_PART_MAIN);
		lv_obj_set_style_pad_row(metricCards_[i], 4, LV_PART_MAIN);
		lv_obj_set_flex_flow(metricCards_[i], LV_FLEX_FLOW_COLUMN);
		lv_obj_set_flex_align(metricCards_[i], LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
		lv_obj_clear_flag(metricCards_[i], LV_OBJ_FLAG_SCROLLABLE);

		metricNameLabels_[i] = lv_label_create(metricCards_[i]);
		lv_obj_add_style(metricNameLabels_[i], theme.captionStyle(), LV_PART_MAIN);
		lv_obj_set_width(metricNameLabels_[i], lv_pct(100));
		lv_label_set_long_mode(metricNameLabels_[i], LV_LABEL_LONG_WRAP);
		lv_label_set_text(metricNameLabels_[i], metricNames[i]);

		metricValueLabels_[i] = lv_label_create(metricCards_[i]);
		lv_obj_add_style(metricValueLabels_[i], theme.titleStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_color(metricValueLabels_[i], theme.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_text_font(metricValueLabels_[i], &lv_font_montserrat_22, LV_PART_MAIN);
		lv_obj_set_width(metricValueLabels_[i], lv_pct(100));
		lv_obj_set_style_text_align(metricValueLabels_[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
		lv_label_set_long_mode(metricValueLabels_[i], LV_LABEL_LONG_WRAP);
		lv_label_set_text(metricValueLabels_[i], "Waiting");
	}

	footerCard_ = lv_obj_create(scroll_);
	lv_obj_set_width(footerCard_, lv_pct(100));
	lv_obj_set_height(footerCard_, LV_SIZE_CONTENT);
	lv_obj_add_style(footerCard_, theme.cardAltStyle(), LV_PART_MAIN);
	lv_obj_set_style_border_width(footerCard_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(footerCard_, 12, LV_PART_MAIN);
	lv_obj_clear_flag(footerCard_, LV_OBJ_FLAG_SCROLLABLE);

	detailsLabel_ = lv_label_create(footerCard_);
	lv_obj_set_width(detailsLabel_, lv_pct(100));
	lv_obj_add_style(detailsLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_label_set_long_mode(detailsLabel_, LV_LABEL_LONG_WRAP);
	lv_label_set_text(detailsLabel_, "Sync status will appear here");
}

void CurrentPage::applyTheme(ThemeManager& theme) {
	theme_ = &theme;
	ui_style_card(root_, theme);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().currentCardPadding, LV_PART_MAIN);
	style_title_chip(titleLabel_, theme, "Current");
	style_chip(statusChip_, theme);
	lv_obj_move_foreground(titleLabel_);

	lv_obj_add_style(heroCard_, theme.cardAltStyle(), LV_PART_MAIN);
	lv_obj_add_style(cityLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_add_style(tempLabel_, theme.titleStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_color(tempLabel_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(tempLabel_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_text_font(tempLabel_, &lv_font_montserrat_48, LV_PART_MAIN);
	lv_obj_set_style_text_letter_space(tempLabel_, -2, LV_PART_MAIN);
	lv_obj_set_width(tempLabel_, 150);
	lv_obj_move_foreground(tempLabel_);
	lv_obj_add_style(summaryLabel_, theme.bodyStyle(), LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(summaryLabel_, 145, LV_PART_MAIN);

	for (size_t i = 0; i < 6; ++i) {
		lv_obj_add_style(metricCards_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_pad_row(metricCards_[i], 4, LV_PART_MAIN);
		lv_obj_add_style(metricNameLabels_[i], theme.captionStyle(), LV_PART_MAIN);
		lv_obj_add_style(metricValueLabels_[i], theme.titleStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_color(metricValueLabels_[i], theme.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_text_font(metricValueLabels_[i], &lv_font_montserrat_22, LV_PART_MAIN);
	}

	lv_obj_add_style(footerCard_, theme.cardAltStyle(), LV_PART_MAIN);
	lv_obj_add_style(detailsLabel_, theme.captionStyle(), LV_PART_MAIN);

	if (iconObj_ != nullptr) {
		ui_icon_delete(iconObj_);
		iconObj_ = nullptr;
	}
	const IconId id = ui_icon_from_condition_code(lastConditionCode_, lastIsDaylight_);
	iconObj_ = ui_icon_create(heroCard_, id, theme.themeId());
	if (iconObj_ != nullptr) {
		ui_icon_set_size(iconObj_, 52, 52);
		lv_obj_align(iconObj_, LV_ALIGN_RIGHT_MID, 0, 12);
		displayedIconId_ = id;
		hasDisplayedIcon_ = true;
	}
}

void CurrentPage::update(const WeatherData& data, const SystemInfo& systemInfo) {
	String city = data.locationName.length() > 0 ? data.locationName : data.locationKey;
	if (city.length() == 0) {
		if (data.lastError == WeatherErrorCode::NotConfigured) {
			city = "Setup required";
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			city = "Waiting for Wi-Fi";
		} else {
			city = "Unknown location";
		}
	}
	lv_label_set_text(cityLabel_, city.c_str());

	if (!data.current.valid) {
		lastConditionCode_ = -1;
		lastIsDaylight_ = true;
		lv_label_set_text(tempLabel_, "-- C");
		if (data.lastError == WeatherErrorCode::NotConfigured) {
			lv_label_set_text(summaryLabel_, "Weather setup required");
			lv_label_set_text(statusChip_, "Setup");
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			lv_label_set_text(summaryLabel_, "Waiting for Wi-Fi");
			lv_label_set_text(statusChip_, "Offline");
		} else {
			lv_label_set_text(summaryLabel_, "Current conditions unavailable");
			lv_label_set_text(statusChip_, "Idle");
		}

		set_metric(metricValueLabels_[0], !isnan(data.current.temperatureC) ? String(data.current.temperatureC, 0) + " C" : String("Pending"));
		set_metric(metricValueLabels_[1], "-- %");
		set_metric(metricValueLabels_[2], "-- kph");
		set_metric(metricValueLabels_[3], "---- mb");
		set_metric(metricValueLabels_[4], systemInfo.lastUpdate.length() > 0 ? systemInfo.lastUpdate : String("Never"));
		set_metric(metricValueLabels_[5], String(data.alertCount) + " active");

		String details = "Open ";
		details += systemInfo.webUiUrl.length() > 0 ? systemInfo.webUiUrl : String("http://192.168.4.1:80");
		details += " to finish setup or wait for the next sync.";
		if (data.lastErrorMessage.length() > 0) {
			details += "  ";
			details += data.lastErrorMessage;
		}
		lv_label_set_text(detailsLabel_, details.c_str());

		if (theme_ != nullptr && (!hasDisplayedIcon_ || displayedIconId_ != IconId::ICON_CLEAR_DAY)) {
			if (iconObj_ != nullptr) {
				ui_icon_delete(iconObj_);
				iconObj_ = nullptr;
			}
			iconObj_ = ui_icon_create(heroCard_, IconId::ICON_CLEAR_DAY, theme_->themeId());
			if (iconObj_ != nullptr) {
				ui_icon_set_size(iconObj_, 56, 56);
				lv_obj_align(iconObj_, LV_ALIGN_RIGHT_MID, -2, 12);
				displayedIconId_ = IconId::ICON_CLEAR_DAY;
				hasDisplayedIcon_ = true;
			}
		}
		return;
	}

	lastConditionCode_ = data.current.icon;
	lastIsDaylight_ = data.current.isDaylight;
	if (theme_ != nullptr) {
		const IconId id = ui_icon_from_condition_code(lastConditionCode_, lastIsDaylight_);
		if (!hasDisplayedIcon_ || displayedIconId_ != id) {
			if (iconObj_ != nullptr) {
				ui_icon_delete(iconObj_);
				iconObj_ = nullptr;
			}
			iconObj_ = ui_icon_create(heroCard_, id, theme_->themeId());
			if (iconObj_ != nullptr) {
				ui_icon_set_size(iconObj_, 56, 56);
				lv_obj_align(iconObj_, LV_ALIGN_RIGHT_MID, -2, 12);
				displayedIconId_ = id;
				hasDisplayedIcon_ = true;
			}
		}
	}

	String temp = isnan(data.current.temperatureC) ? String("-- C") : String(data.current.temperatureC, 0) + " C";
	lv_label_set_text(tempLabel_, temp.c_str());

	String summary = data.current.summary.length() > 0 ? data.current.summary : String("Conditions ready");
	summary += data.current.isDaylight ? " • Day" : " • Night";
	lv_label_set_text(summaryLabel_, summary.c_str());
	lv_label_set_text(statusChip_, data.current.isDaylight ? "Day" : "Night");

	set_metric(metricValueLabels_[0], !isnan(data.current.feelsLikeC)
		? String(data.current.feelsLikeC, 0) + " C"
		: (!isnan(data.current.temperatureC) ? String(data.current.temperatureC, 0) + " C" : String("Pending")));
	set_metric(metricValueLabels_[1], data.current.humidityPct >= 0 ? String(data.current.humidityPct) + " %" : String("-- %"));
	set_metric(metricValueLabels_[2], isnan(data.current.windKph) ? String("-- kph") : String(data.current.windKph, 0) + " kph");
	set_metric(metricValueLabels_[3], isnan(data.current.pressureMb) ? String("---- mb") : String(data.current.pressureMb, 0) + " mb");
	set_metric(metricValueLabels_[4], systemInfo.lastUpdate.length() > 0 ? systemInfo.lastUpdate : String("Just now"));
	set_metric(metricValueLabels_[5], String(data.alertCount) + (data.alertCount == 1 ? " alert" : " alerts"));

	String details = "Provider: ";
	details += data.provider.length() > 0 ? data.provider : String("Weather");
	if (systemInfo.lastUpdate.length() > 0) {
		details += "  •  Updated ";
		details += systemInfo.lastUpdate;
	}
	if (systemInfo.wifiStatus.length() > 0) {
		details += "\nWi-Fi: ";
		details += systemInfo.wifiStatus;
	}
	if (systemInfo.webUiUrl.length() > 0) {
		details += "  •  ";
		details += systemInfo.webUiUrl;
	}
	lv_label_set_text(detailsLabel_, details.c_str());
}

}  // namespace ui
