#include "ui_weekly.h"

#include <math.h>

namespace ui {

void WeeklyPage::begin(lv_obj_t* parent, ThemeManager& theme) {
	theme_ = &theme;
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
	lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(root_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_add_style(root_, theme.cardStyle(), LV_PART_MAIN);
	ui_make_container_transparent(root_);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);

	title_ = lv_label_create(root_);
	lv_obj_set_style_text_color(title_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(title_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(title_, theme.typography().pageTitleZoom, LV_PART_MAIN);
	lv_label_set_text(title_, "Weekly");
	lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, 0);

	list_ = lv_obj_create(root_);
	lv_obj_set_size(list_, lv_pct(100), lv_pct(100));
	lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 0, theme.spacing().weeklyListTop);
	lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(list_, theme.spacing().listRowGap, LV_PART_MAIN);
	lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(list_, LV_DIR_VER);
	lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);

	for (size_t i = 0; i < kMaxForecastDays; ++i) {
		rowCards_[i] = lv_obj_create(list_);
		lv_obj_set_width(rowCards_[i], lv_pct(100));
		lv_obj_add_style(rowCards_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_pad_all(rowCards_[i], theme.spacing().cardAltPadding, LV_PART_MAIN);
		lv_obj_set_style_border_width(rowCards_[i], 0, LV_PART_MAIN);
		lv_obj_set_flex_flow(rowCards_[i], LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(rowCards_[i], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_column(rowCards_[i], 12, LV_PART_MAIN);
		lv_obj_clear_flag(rowCards_[i], LV_OBJ_FLAG_SCROLLABLE);

		rowIcons_[i] = ui_icon_create(rowCards_[i], IconId::ICON_CLOUDY, theme.themeId());
		if (rowIcons_[i] != nullptr) {
			ui_icon_set_size(rowIcons_[i], 34, 34);
			rowIconIds_[i] = IconId::ICON_CLOUDY;
			rowIconSet_[i] = true;
		}

		rowLabels_[i] = lv_label_create(rowCards_[i]);
		lv_obj_set_flex_grow(rowLabels_[i], 1);
		lv_obj_set_style_text_color(rowLabels_[i], theme.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_text_font(rowLabels_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_transform_zoom(rowLabels_[i], theme.typography().weeklyRowZoom, LV_PART_MAIN);
		lv_obj_set_style_text_line_space(rowLabels_[i], theme.typography().bodyLineSpace + 2, LV_PART_MAIN);
		lv_label_set_long_mode(rowLabels_[i], LV_LABEL_LONG_WRAP);
		lv_label_set_text(rowLabels_[i], "--");
	}
}

void WeeklyPage::applyTheme(ThemeManager& theme) {
	theme_ = &theme;
	lv_obj_add_style(root_, theme.cardStyle(), LV_PART_MAIN);
	ui_make_container_transparent(root_);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	lv_obj_set_style_text_color(title_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(title_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(title_, theme.typography().pageTitleZoom, LV_PART_MAIN);
	lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 0, theme.spacing().weeklyListTop);
	lv_obj_set_style_pad_row(list_, theme.spacing().listRowGap, LV_PART_MAIN);
	for (size_t i = 0; i < kMaxForecastDays; ++i) {
		lv_obj_add_style(rowCards_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_pad_all(rowCards_[i], theme.spacing().cardAltPadding, LV_PART_MAIN);
		lv_obj_set_style_pad_column(rowCards_[i], 12, LV_PART_MAIN);
		
		// Recreate icon for new theme
		if (rowIcons_[i] != nullptr) {
			ui_icon_delete(rowIcons_[i]);
			rowIcons_[i] = nullptr;
		}
		rowIcons_[i] = ui_icon_create(rowCards_[i], IconId::ICON_CLOUDY, theme.themeId());
		if (rowIcons_[i] != nullptr) {
			ui_icon_set_size(rowIcons_[i], 34, 34);
			rowIconIds_[i] = IconId::ICON_CLOUDY;
			rowIconSet_[i] = true;
		}
		
		lv_obj_set_style_text_color(rowLabels_[i], theme.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_text_font(rowLabels_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_transform_zoom(rowLabels_[i], theme.typography().weeklyRowZoom, LV_PART_MAIN);
		lv_obj_set_style_text_line_space(rowLabels_[i], theme.typography().bodyLineSpace + 2, LV_PART_MAIN);
	}
}

void WeeklyPage::update(const WeatherData& data) {
	if (data.forecastCount == 0) {
		String message = "Waiting for forecast";
		if (data.lastError == WeatherErrorCode::NotConfigured) {
			message = "Setup required\nConfigure Wi-Fi and location in the Web UI";
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			message = "Waiting for Wi-Fi\nForecast will load after network connection";
		} else if (data.lastErrorMessage.length() > 0) {
			message = data.lastErrorMessage;
		}

		for (size_t i = 0; i < kMaxForecastDays; ++i) {
			if (!rowIconSet_[i] || rowIconIds_[i] != IconId::ICON_CLEAR_DAY) {
				if (rowIcons_[i] != nullptr) {
					ui_icon_delete(rowIcons_[i]);
					rowIcons_[i] = nullptr;
				}
				rowIcons_[i] = ui_icon_create(rowCards_[i], IconId::ICON_CLEAR_DAY, theme_->themeId());
				if (rowIcons_[i] != nullptr) {
					ui_icon_set_size(rowIcons_[i], 34, 34);
					rowIconIds_[i] = IconId::ICON_CLEAR_DAY;
					rowIconSet_[i] = true;
				}
			}
			lv_label_set_text(rowLabels_[i], i == 0 ? message.c_str() : "Waiting for forecast");
		}
		return;
	}

	for (size_t i = 0; i < kMaxForecastDays; ++i) {
		if (i >= data.forecastCount || !data.forecast[i].valid) {
			if (!rowIconSet_[i] || rowIconIds_[i] != IconId::ICON_CLEAR_DAY) {
				if (rowIcons_[i] != nullptr) {
					ui_icon_delete(rowIcons_[i]);
					rowIcons_[i] = nullptr;
				}
				rowIcons_[i] = ui_icon_create(rowCards_[i], IconId::ICON_CLEAR_DAY, theme_->themeId());
				if (rowIcons_[i] != nullptr) {
					ui_icon_set_size(rowIcons_[i], 34, 34);
					rowIconIds_[i] = IconId::ICON_CLEAR_DAY;
					rowIconSet_[i] = true;
				}
			}
			lv_label_set_text(rowLabels_[i], "No data");
			continue;
		}

		const DailyForecast& f = data.forecast[i];
		if (theme_ != nullptr) {
			IconId id = ui_icon_from_condition_code(f.dayIcon, true);
			if (!rowIconSet_[i] || rowIconIds_[i] != id) {
				if (rowIcons_[i] != nullptr) {
					ui_icon_delete(rowIcons_[i]);
					rowIcons_[i] = nullptr;
				}
				rowIcons_[i] = ui_icon_create(rowCards_[i], id, theme_->themeId());
				if (rowIcons_[i] != nullptr) {
					ui_icon_set_size(rowIcons_[i], 40, 40);
					rowIconIds_[i] = id;
					rowIconSet_[i] = true;
				}
			}
		}
		String line = f.dayLabel.length() > 0 ? f.dayLabel : String("Day ") + String(static_cast<unsigned>(i + 1));
		line += "\n";
		line += f.summary.length() > 0 ? f.summary : String("--");
		line += "\n";
		line += "Low ";
		line += isnan(f.minTempC) ? "--.-" : String(f.minTempC, 1);
		line += " C  High ";
		line += isnan(f.maxTempC) ? "--.-" : String(f.maxTempC, 1);
		line += " C  Rain ";
		line += f.precipChancePct >= 0 ? String(f.precipChancePct) : String("--");
		line += "%";
		lv_label_set_text(rowLabels_[i], line.c_str());
	}
}

}  // namespace ui
