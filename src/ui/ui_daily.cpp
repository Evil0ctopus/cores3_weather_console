#include "ui_daily.h"

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

String build_temp_range(const DailyForecast& forecast) {
	String text = "L ";
	text += isnan(forecast.minTempC) ? "--" : String(forecast.minTempC, 0);
	text += "  H ";
	text += isnan(forecast.maxTempC) ? "--" : String(forecast.maxTempC, 0);
	text += " C";
	return text;
}

}  // namespace

void DailyPage::begin(lv_obj_t* parent, ThemeManager& theme) {
	theme_ = &theme;
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
	lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(root_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	ui_style_card(root_, theme);

	title_ = lv_label_create(root_);
	style_title_chip(title_, theme, "Daily");
	lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, 0);
	lv_obj_move_foreground(title_);

	subtitle_ = lv_label_create(root_);
	lv_obj_add_style(subtitle_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(subtitle_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(subtitle_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_label_set_text(subtitle_, "7-day outlook");
	lv_obj_align(subtitle_, LV_ALIGN_TOP_LEFT, 0, 24);

	list_ = lv_obj_create(root_);
	lv_obj_set_size(list_, lv_pct(100), lv_pct(100));
	lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 0, 52);
	lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(list_, 8, LV_PART_MAIN);
	lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(list_, LV_DIR_VER);
	lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);

	for (size_t i = 0; i < kMaxForecastDays; ++i) {
		rowCards_[i] = lv_obj_create(list_);
		lv_obj_set_width(rowCards_[i], lv_pct(100));
		lv_obj_add_style(rowCards_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_border_width(rowCards_[i], 0, LV_PART_MAIN);
		lv_obj_set_style_pad_all(rowCards_[i], 10, LV_PART_MAIN);
		lv_obj_set_style_pad_column(rowCards_[i], 10, LV_PART_MAIN);
		lv_obj_set_flex_flow(rowCards_[i], LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(rowCards_[i], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_clear_flag(rowCards_[i], LV_OBJ_FLAG_SCROLLABLE);

		rowIcons_[i] = ui_icon_create(rowCards_[i], IconId::ICON_CLOUDY, theme.themeId());
		if (rowIcons_[i] != nullptr) {
			ui_icon_set_size(rowIcons_[i], 36, 36);
			rowIconIds_[i] = IconId::ICON_CLOUDY;
			rowIconSet_[i] = true;
		}

		rowLabels_[i] = lv_label_create(rowCards_[i]);
		lv_obj_set_flex_grow(rowLabels_[i], 1);
		lv_obj_add_style(rowLabels_[i], theme.bodyStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_font(rowLabels_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_text_line_space(rowLabels_[i], theme.typography().bodyLineSpace, LV_PART_MAIN);
		lv_label_set_long_mode(rowLabels_[i], LV_LABEL_LONG_WRAP);
		lv_label_set_text(rowLabels_[i], "--");

		rowTempLabels_[i] = lv_label_create(rowCards_[i]);
		lv_obj_add_style(rowTempLabels_[i], theme.titleStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_font(rowTempLabels_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_transform_zoom(rowTempLabels_[i], 118, LV_PART_MAIN);
		lv_label_set_text(rowTempLabels_[i], "--");
	}
}

void DailyPage::applyTheme(ThemeManager& theme) {
	theme_ = &theme;
	ui_style_card(root_, theme);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	style_title_chip(title_, theme, "Daily");
	lv_obj_add_style(subtitle_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(subtitle_, theme.typography().captionZoom, LV_PART_MAIN);

	for (size_t i = 0; i < kMaxForecastDays; ++i) {
		lv_obj_add_style(rowCards_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_border_width(rowCards_[i], 0, LV_PART_MAIN);
		lv_obj_add_style(rowLabels_[i], theme.bodyStyle(), LV_PART_MAIN);
		lv_obj_add_style(rowTempLabels_[i], theme.titleStyle(), LV_PART_MAIN);
		lv_obj_set_style_transform_zoom(rowTempLabels_[i], 118, LV_PART_MAIN);

		if (rowIcons_[i] != nullptr) {
			ui_icon_delete(rowIcons_[i]);
			rowIcons_[i] = nullptr;
		}
		rowIcons_[i] = ui_icon_create(rowCards_[i], IconId::ICON_CLOUDY, theme.themeId());
		if (rowIcons_[i] != nullptr) {
			ui_icon_set_size(rowIcons_[i], 36, 36);
			rowIconIds_[i] = IconId::ICON_CLOUDY;
			rowIconSet_[i] = true;
		}
	}
}

void DailyPage::update(const WeatherData& data) {
	if (subtitle_ != nullptr) {
		if (data.lastError == WeatherErrorCode::NotConfigured) {
			lv_label_set_text(subtitle_, "Setup required");
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			lv_label_set_text(subtitle_, "Waiting for Wi-Fi");
		} else {
			lv_label_set_text(subtitle_, "7-day outlook");
		}
	}

	for (size_t i = 0; i < kMaxForecastDays; ++i) {
		if (i >= data.forecastCount || !data.forecast[i].valid) {
			if (theme_ != nullptr && (!rowIconSet_[i] || rowIconIds_[i] != IconId::ICON_CLEAR_DAY)) {
				if (rowIcons_[i] != nullptr) {
					ui_icon_delete(rowIcons_[i]);
					rowIcons_[i] = nullptr;
				}
				rowIcons_[i] = ui_icon_create(rowCards_[i], IconId::ICON_CLEAR_DAY, theme_->themeId());
				if (rowIcons_[i] != nullptr) {
					ui_icon_set_size(rowIcons_[i], 36, 36);
					rowIconIds_[i] = IconId::ICON_CLEAR_DAY;
					rowIconSet_[i] = true;
				}
			}
			lv_label_set_text(rowLabels_[i], i == 0 ? "Waiting for forecast\nDaily cards will fill after sync" : "No data");
			lv_label_set_text(rowTempLabels_[i], "--");
			continue;
		}

		const DailyForecast& forecast = data.forecast[i];
		IconId iconId = ui_icon_from_condition_code(forecast.dayIcon, true);
		if (theme_ != nullptr && (!rowIconSet_[i] || rowIconIds_[i] != iconId)) {
			if (rowIcons_[i] != nullptr) {
				ui_icon_delete(rowIcons_[i]);
				rowIcons_[i] = nullptr;
			}
			rowIcons_[i] = ui_icon_create(rowCards_[i], iconId, theme_->themeId());
			if (rowIcons_[i] != nullptr) {
				ui_icon_set_size(rowIcons_[i], 36, 36);
				rowIconIds_[i] = iconId;
				rowIconSet_[i] = true;
			}
		}

		String label = forecast.dayLabel.length() > 0 ? forecast.dayLabel : String("Day ") + String(static_cast<unsigned>(i + 1));
		label += "\n";
		label += forecast.summary.length() > 0 ? forecast.summary : String("Conditions pending");
		label += "  •  Rain ";
		label += forecast.precipChancePct >= 0 ? String(forecast.precipChancePct) : String("--");
		label += "%";
		lv_label_set_text(rowLabels_[i], label.c_str());

		String tempText = build_temp_range(forecast);
		lv_label_set_text(rowTempLabels_[i], tempText.c_str());
	}
}

}  // namespace ui
