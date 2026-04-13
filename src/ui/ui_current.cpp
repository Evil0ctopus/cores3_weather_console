#include "ui_current.h"

#include <math.h>
#include "ui_icons.h"

namespace ui {

void CurrentPage::begin(lv_obj_t* parent, ThemeManager& theme) {
	theme_ = &theme;
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
	lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(root_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().currentCardPadding, LV_PART_MAIN);
	
	// Apply semi-transparent card overlay style
	ui_style_card(root_, theme);

	cityLabel_ = lv_label_create(root_);
	lv_obj_add_style(cityLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(cityLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(cityLabel_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_label_set_text(cityLabel_, "Location");
	lv_obj_align(cityLabel_, LV_ALIGN_TOP_LEFT, 0, 0);

	// Create PNG weather icon
	iconObj_ = ui_icon_create(root_, IconId::ICON_CLEAR_DAY, theme.themeId());
	if (iconObj_ != nullptr) {
		ui_icon_set_size(iconObj_, 40, 40);
		lv_obj_align(iconObj_, LV_ALIGN_TOP_RIGHT, 0, 0);
		displayedIconId_ = IconId::ICON_CLEAR_DAY;
		hasDisplayedIcon_ = true;
	}

	tempLabel_ = lv_label_create(root_);
	lv_obj_set_style_text_color(tempLabel_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(tempLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(tempLabel_, theme.typography().heroValueZoom, LV_PART_MAIN);
	lv_obj_set_style_text_letter_space(tempLabel_, -1, LV_PART_MAIN);
	lv_label_set_text(tempLabel_, "--.- C");
	lv_obj_align(tempLabel_, LV_ALIGN_TOP_LEFT, 0, 24);

	summaryLabel_ = lv_label_create(root_);
	lv_obj_add_style(summaryLabel_, theme.bodyStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(summaryLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(summaryLabel_, theme.typography().heroSummaryZoom, LV_PART_MAIN);
	lv_obj_set_style_text_line_space(summaryLabel_, theme.typography().summaryLineSpace, LV_PART_MAIN);
	lv_label_set_text(summaryLabel_, "Waiting for current conditions");
	lv_obj_set_width(summaryLabel_, lv_pct(100));
	lv_obj_align(summaryLabel_, LV_ALIGN_TOP_LEFT, 0, 94);

	detailsLabel_ = lv_label_create(root_);
	lv_obj_add_style(detailsLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(detailsLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_label_set_text(detailsLabel_, "Feels --.- C  Humidity --%  Wind --.- kph");
	lv_obj_set_width(detailsLabel_, lv_pct(100));
	lv_obj_align(detailsLabel_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void CurrentPage::applyTheme(ThemeManager& theme) {
	theme_ = &theme;
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().currentCardPadding, LV_PART_MAIN);
	
	// Reapply semi-transparent card overlay for new theme
	ui_style_card(root_, theme);
	
	lv_obj_add_style(cityLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(cityLabel_, theme.typography().captionZoom, LV_PART_MAIN);
	
	// Recreate icon for new theme
	if (iconObj_ != nullptr) {
		ui_icon_delete(iconObj_);
		iconObj_ = nullptr;
	}
	IconId id = ui_icon_from_condition_code(lastConditionCode_, lastIsDaylight_);
	iconObj_ = ui_icon_create(root_, id, theme.themeId());
	if (iconObj_ != nullptr) {
		ui_icon_set_size(iconObj_, 40, 40);
		lv_obj_align(iconObj_, LV_ALIGN_TOP_RIGHT, 0, 0);
		displayedIconId_ = id;
		hasDisplayedIcon_ = true;
	}
	
	lv_obj_add_style(tempLabel_, theme.titleStyle(), LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(tempLabel_, theme.typography().heroValueZoom, LV_PART_MAIN);
	lv_obj_add_style(summaryLabel_, theme.bodyStyle(), LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(summaryLabel_, theme.typography().heroSummaryZoom, LV_PART_MAIN);
	lv_obj_set_style_text_line_space(summaryLabel_, theme.typography().summaryLineSpace, LV_PART_MAIN);
	lv_obj_add_style(detailsLabel_, theme.captionStyle(), LV_PART_MAIN);
}

void CurrentPage::update(const WeatherData& data, const SystemInfo& systemInfo) {
	if (cityLabel_ != nullptr) {
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
	}

	if (!data.current.valid) {
		lastConditionCode_ = -1;
		lastIsDaylight_ = true;
		lv_label_set_text(tempLabel_, "--.- C");
		String summary = "Current conditions unavailable";
		String details = "Weather data will appear after the first successful sync";
		const bool hasSetupUrl = systemInfo.webUiUrl.length() > 0;
		const bool hasSetupSsid = systemInfo.wifiSsid.length() > 0;
		if (data.lastError == WeatherErrorCode::NotConfigured) {
			summary = "Weather setup required";
			details = "Join ";
			details += hasSetupSsid ? systemInfo.wifiSsid : String("the setup network");
			details += " and open ";
			details += hasSetupUrl ? systemInfo.webUiUrl : String("http://192.168.4.1:80");
			details += " to add Wi-Fi and location";
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			summary = "Waiting for Wi-Fi";
			details = "Open ";
			details += hasSetupUrl ? systemInfo.webUiUrl : String("http://192.168.4.1:80");
			details += " to connect Wi-Fi or wait for the saved network to return";
		} else if (data.lastErrorMessage.length() > 0) {
			summary = data.lastErrorMessage;
			if (hasSetupUrl) {
				details = String("Web UI: ") + systemInfo.webUiUrl;
			}
		}
		lv_label_set_text(summaryLabel_, summary.c_str());
		lv_label_set_text(detailsLabel_, details.c_str());
		if (theme_ != nullptr && (!hasDisplayedIcon_ || displayedIconId_ != IconId::ICON_CLEAR_DAY)) {
			if (iconObj_ != nullptr) {
				ui_icon_delete(iconObj_);
				iconObj_ = nullptr;
			}
			iconObj_ = ui_icon_create(root_, IconId::ICON_CLEAR_DAY, theme_->themeId());
			if (iconObj_ != nullptr) {
				ui_icon_set_size(iconObj_, 40, 40);
				lv_obj_align(iconObj_, LV_ALIGN_TOP_RIGHT, 0, 0);
				displayedIconId_ = IconId::ICON_CLEAR_DAY;
				hasDisplayedIcon_ = true;
			}
		}
		return;
	}

	lastConditionCode_ = data.current.icon;
	lastIsDaylight_ = data.current.isDaylight;
	if (theme_ != nullptr) {
		IconId id = ui_icon_from_condition_code(lastConditionCode_, lastIsDaylight_);
		if (!hasDisplayedIcon_ || displayedIconId_ != id) {
			if (iconObj_ != nullptr) {
				ui_icon_delete(iconObj_);
				iconObj_ = nullptr;
			}
			iconObj_ = ui_icon_create(root_, id, theme_->themeId());
			if (iconObj_ != nullptr) {
				ui_icon_set_size(iconObj_, 40, 40);
				lv_obj_align(iconObj_, LV_ALIGN_TOP_RIGHT, 0, 0);
				displayedIconId_ = id;
				hasDisplayedIcon_ = true;
			}
		}
	}

	String temp = isnan(data.current.temperatureC)
										? String("--.- C")
										: String(data.current.temperatureC, 1) + " C";
	lv_label_set_text(tempLabel_, temp.c_str());

	String summary = data.current.summary;
	if (summary.length() == 0) {
		summary = "No summary";
	}
	summary += data.current.isDaylight ? "  Day" : "  Night";
	lv_label_set_text(summaryLabel_, summary.c_str());

	String details = "Feels ";
	details += isnan(data.current.feelsLikeC) ? "--.-" : String(data.current.feelsLikeC, 1);
	details += " C  Humidity ";
	details += data.current.humidityPct >= 0 ? String(data.current.humidityPct) : String("--");
	details += "%  Wind ";
	details += isnan(data.current.windKph) ? "--.-" : String(data.current.windKph, 1);
	details += " kph";
	lv_label_set_text(detailsLabel_, details.c_str());
}

}  // namespace ui
