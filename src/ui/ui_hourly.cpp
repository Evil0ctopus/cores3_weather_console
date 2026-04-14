#include "ui_hourly.h"

#include <math.h>

namespace ui {
namespace {

static const char* kHourLabels[8] = {"Now", "+2h", "+4h", "+6h", "+8h", "+10h", "+12h", "+14h"};

float safe_temp(float value, float fallback) {
	return isnan(value) ? fallback : value;
}

float estimate_hourly_temp(const WeatherData& data, size_t slot) {
	float current = data.current.valid ? data.current.temperatureC : NAN;
	float low = data.forecastCount > 0 ? data.forecast[0].minTempC : NAN;
	float high = data.forecastCount > 0 ? data.forecast[0].maxTempC : NAN;

	if (isnan(current)) {
		if (!isnan(low) && !isnan(high)) {
			current = (low + high) * 0.5f;
		} else {
			current = 20.0f;
		}
	}

	low = safe_temp(low, current - 2.0f);
	high = safe_temp(high, current + 3.0f);

	const float phase = static_cast<float>(slot) / 7.0f;
	if (phase < 0.45f) {
		return current + (high - current) * (phase / 0.45f);
	}
	return high + (low - high) * ((phase - 0.45f) / 0.55f);
}

int estimate_precip(const WeatherData& data, size_t slot) {
	int base = data.forecastCount > 0 ? data.forecast[0].precipChancePct : -1;
	if (base < 0) {
		base = 10;
	}
	int value = base + static_cast<int>(slot) * 3;
	if (value > 95) {
		value = 95;
	}
	return value;
}

IconId estimate_icon(const WeatherData& data, size_t slot, bool& daylight) {
	daylight = slot < 5 ? true : false;
	int iconCode = data.current.valid ? data.current.icon : -1;
	if (slot >= 5 && data.forecastCount > 0) {
		iconCode = data.forecast[0].nightIcon >= 0 ? data.forecast[0].nightIcon : data.forecast[0].dayIcon;
	}
	if (slot >= 3 && data.forecastCount > 1 && data.forecast[1].dayIcon >= 0) {
		iconCode = data.forecast[1].dayIcon;
	}
	return ui_icon_from_condition_code(iconCode, daylight);
}

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

}  // namespace

void HourlyPage::begin(lv_obj_t* parent, ThemeManager& theme) {
	theme_ = &theme;
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
	lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(root_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	lv_obj_set_style_pad_row(root_, 6, LV_PART_MAIN);
	lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
	ui_style_card(root_, theme);

	title_ = lv_label_create(root_);
	style_title_chip(title_, theme, "Hourly");
	lv_obj_move_foreground(title_);

	subtitle_ = lv_label_create(root_);
	lv_obj_add_style(subtitle_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(subtitle_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(subtitle_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_label_set_text(subtitle_, "Next trend");

	list_ = lv_obj_create(root_);
	lv_obj_set_width(list_, lv_pct(100));
	lv_obj_set_flex_grow(list_, 1);
	lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_column(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(list_, 8, LV_PART_MAIN);
	lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(list_, LV_DIR_VER);
	lv_obj_add_flag(list_, LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_ONE);
	lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);

	for (size_t i = 0; i < 8; ++i) {
		cards_[i] = lv_obj_create(list_);
		lv_obj_set_width(cards_[i], lv_pct(100));
		lv_obj_set_height(cards_[i], 68);
		lv_obj_add_style(cards_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_border_width(cards_[i], 0, LV_PART_MAIN);
		lv_obj_set_style_pad_left(cards_[i], 10, LV_PART_MAIN);
		lv_obj_set_style_pad_right(cards_[i], 10, LV_PART_MAIN);
		lv_obj_set_style_pad_top(cards_[i], 8, LV_PART_MAIN);
		lv_obj_set_style_pad_bottom(cards_[i], 8, LV_PART_MAIN);
		lv_obj_set_style_pad_column(cards_[i], 10, LV_PART_MAIN);
		lv_obj_set_flex_flow(cards_[i], LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(cards_[i], LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_clear_flag(cards_[i], LV_OBJ_FLAG_SCROLLABLE);

		timeLabels_[i] = lv_label_create(cards_[i]);
		lv_obj_add_style(timeLabels_[i], theme.captionStyle(), LV_PART_MAIN);
		lv_obj_set_width(timeLabels_[i], 48);
		lv_label_set_text(timeLabels_[i], kHourLabels[i]);

		iconObjs_[i] = ui_icon_create(cards_[i], IconId::ICON_CLEAR_DAY, theme.themeId());
		if (iconObjs_[i] != nullptr) {
			ui_icon_set_size(iconObjs_[i], 34, 34);
			iconIds_[i] = IconId::ICON_CLEAR_DAY;
			iconSet_[i] = true;
		}

		tempLabels_[i] = lv_label_create(cards_[i]);
		lv_obj_add_style(tempLabels_[i], theme.titleStyle(), LV_PART_MAIN);
		lv_obj_set_width(tempLabels_[i], 58);
		lv_obj_set_style_text_font(tempLabels_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_transform_zoom(tempLabels_[i], 145, LV_PART_MAIN);
		lv_label_set_text(tempLabels_[i], "--.- C");

		detailLabels_[i] = lv_label_create(cards_[i]);
		lv_obj_add_style(detailLabels_[i], theme.captionStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_align(detailLabels_[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
		lv_label_set_long_mode(detailLabels_[i], LV_LABEL_LONG_WRAP);
		lv_obj_set_width(detailLabels_[i], 88);
		lv_label_set_text(detailLabels_[i], "Rain --%\nWind --");
	}
}

void HourlyPage::applyTheme(ThemeManager& theme) {
	theme_ = &theme;
	ui_style_card(root_, theme);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	style_title_chip(title_, theme, "Hourly");
	lv_obj_add_style(subtitle_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(subtitle_, theme.typography().captionZoom, LV_PART_MAIN);

	for (size_t i = 0; i < 8; ++i) {
		lv_obj_add_style(cards_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_border_width(cards_[i], 0, LV_PART_MAIN);
		lv_obj_add_style(timeLabels_[i], theme.captionStyle(), LV_PART_MAIN);
		lv_obj_add_style(tempLabels_[i], theme.titleStyle(), LV_PART_MAIN);
		lv_obj_set_style_transform_zoom(tempLabels_[i], 150, LV_PART_MAIN);
		lv_obj_add_style(detailLabels_[i], theme.captionStyle(), LV_PART_MAIN);

		if (iconObjs_[i] != nullptr) {
			ui_icon_delete(iconObjs_[i]);
			iconObjs_[i] = nullptr;
		}
		iconObjs_[i] = ui_icon_create(cards_[i], IconId::ICON_CLEAR_DAY, theme.themeId());
		if (iconObjs_[i] != nullptr) {
			ui_icon_set_size(iconObjs_[i], 34, 34);
			iconIds_[i] = IconId::ICON_CLEAR_DAY;
			iconSet_[i] = true;
		}
	}
}

void HourlyPage::update(const WeatherData& data) {
	if (subtitle_ != nullptr) {
		if (data.lastError == WeatherErrorCode::NotConfigured) {
			lv_label_set_text(subtitle_, "Setup required");
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			lv_label_set_text(subtitle_, "Waiting for Wi-Fi");
		} else {
			lv_label_set_text(subtitle_, "Next 14 hour trend");
		}
	}

	lastConditionCode_ = data.current.icon;
	lastIsDaylight_ = data.current.isDaylight;

	for (size_t i = 0; i < 8; ++i) {
		lv_label_set_text(timeLabels_[i], kHourLabels[i]);

		bool daylight = true;
		IconId iconId = estimate_icon(data, i, daylight);
		if (theme_ != nullptr && (!iconSet_[i] || iconIds_[i] != iconId)) {
			if (iconObjs_[i] != nullptr) {
				ui_icon_delete(iconObjs_[i]);
				iconObjs_[i] = nullptr;
			}
			iconObjs_[i] = ui_icon_create(cards_[i], iconId, theme_->themeId());
			if (iconObjs_[i] != nullptr) {
				ui_icon_set_size(iconObjs_[i], 34, 34);
				iconIds_[i] = iconId;
				iconSet_[i] = true;
			}
		}

		const float temp = estimate_hourly_temp(data, i);
		String tempText = String(temp, 1) + " C";
		if (!data.current.valid && data.forecastCount == 0) {
			tempText = "--.- C";
		}
		lv_label_set_text(tempLabels_[i], tempText.c_str());

		String detail;
		if (!data.current.valid && data.forecastCount == 0) {
			detail = i == 0 ? "Waiting for\nforecast" : "--";
		} else {
			detail = "Rain ";
			detail += String(estimate_precip(data, i));
			detail += "%\nWind ";
			float wind = data.current.valid ? data.current.windKph : NAN;
			if (isnan(wind)) {
				wind = 6.0f + static_cast<float>(i);
			}
			detail += String(wind + static_cast<float>(i) * 0.6f, 0);
			detail += "k";
		}
		lv_label_set_text(detailLabels_[i], detail.c_str());
	}
}

}  // namespace ui
