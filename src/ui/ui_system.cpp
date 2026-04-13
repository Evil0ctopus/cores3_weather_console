#include "ui_system.h"

namespace ui {
namespace {

void setRow(lv_obj_t* label, const char* heading, const String& value) {
	String text = String(heading) + "\n" + value;
	lv_label_set_text(label, text.c_str());
}

String themeOptions() {
	String options;
	for (uint8_t index = 0; index < theme_count(); ++index) {
		if (index > 0) {
			options += "\n";
		}
		options += theme_id_to_name(theme_id_from_index(index));
	}
	return options;
}

}  // namespace

void SystemPage::begin(lv_obj_t* parent,
				   ThemeManager& theme,
				   app::SettingsStore& settingsStore,
				   ThemeSelectedCallback themeSelectedCallback,
				   void* themeSelectedContext) {
	settingsStore_ = &settingsStore;
	themeSelectedCallback_ = themeSelectedCallback;
	themeSelectedContext_ = themeSelectedContext;
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
	lv_label_set_text(title_, "System");
	lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, 0);

	subtitle_ = lv_label_create(root_);
	lv_obj_add_style(subtitle_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(subtitle_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(subtitle_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_label_set_text(subtitle_, "Device diagnostics and runtime status");
	lv_obj_align(subtitle_, LV_ALIGN_TOP_LEFT, 0, 52);

	themeLabel_ = lv_label_create(root_);
	lv_obj_add_style(themeLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_label_set_text(themeLabel_, "Theme");
	lv_obj_align(themeLabel_, LV_ALIGN_TOP_LEFT, 0, 82);

	themeDropdown_ = lv_dropdown_create(root_);
	lv_obj_set_width(themeDropdown_, lv_pct(100));
	lv_obj_align(themeDropdown_, LV_ALIGN_TOP_LEFT, 0, 108);
	lv_dropdown_set_options(themeDropdown_, themeOptions().c_str());
	lv_obj_add_style(themeDropdown_, theme.cardAltStyle(), LV_PART_MAIN);
	lv_obj_add_style(themeDropdown_, theme.defaultLabelStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(themeDropdown_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_add_flag(themeDropdown_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_add_event_cb(themeDropdown_, onThemeDropdownChanged, LV_EVENT_VALUE_CHANGED, this);
	syncThemeSelector();

	list_ = lv_obj_create(root_);
	lv_obj_set_size(list_, lv_pct(100), lv_pct(100));
	lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 0, 156);
	lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(list_, theme.spacing().listRowGap, LV_PART_MAIN);
	lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(list_, LV_DIR_VER);
	lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);

	for (size_t i = 0; i < 11; ++i) {
		rowLabels_[i] = lv_label_create(list_);
		lv_obj_set_width(rowLabels_[i], lv_pct(100));
		lv_obj_add_style(rowLabels_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_color(rowLabels_[i], theme.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_text_font(rowLabels_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_text_line_space(rowLabels_[i], theme.typography().bodyLineSpace + 2, LV_PART_MAIN);
		lv_obj_set_style_pad_all(rowLabels_[i], theme.spacing().cardAltPadding, LV_PART_MAIN);
		lv_label_set_long_mode(rowLabels_[i], LV_LABEL_LONG_WRAP);
	}

	update(SystemInfo());
}

void SystemPage::applyTheme(ThemeManager& theme) {
	lv_obj_add_style(root_, theme.cardStyle(), LV_PART_MAIN);
	ui_make_container_transparent(root_);
	lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	lv_obj_set_style_text_color(title_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(title_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(title_, theme.typography().pageTitleZoom, LV_PART_MAIN);
	lv_obj_add_style(subtitle_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_add_style(themeLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_add_style(themeDropdown_, theme.cardAltStyle(), LV_PART_MAIN);
	lv_obj_add_style(themeDropdown_, theme.defaultLabelStyle(), LV_PART_MAIN);
	lv_obj_set_style_border_color(themeDropdown_, theme.palette().shadow, LV_PART_MAIN);
	lv_obj_set_style_bg_color(themeDropdown_, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(subtitle_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_obj_set_style_pad_row(list_, theme.spacing().listRowGap, LV_PART_MAIN);
	for (size_t i = 0; i < 11; ++i) {
		lv_obj_add_style(rowLabels_[i], theme.cardAltStyle(), LV_PART_MAIN);
		lv_obj_set_style_text_color(rowLabels_[i], theme.palette().textPrimary, LV_PART_MAIN);
		lv_obj_set_style_text_font(rowLabels_[i], &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_set_style_text_line_space(rowLabels_[i], theme.typography().bodyLineSpace + 2, LV_PART_MAIN);
		lv_obj_set_style_pad_all(rowLabels_[i], theme.spacing().cardAltPadding, LV_PART_MAIN);
	}
}

void SystemPage::update(const SystemInfo& info) {
	setRow(rowLabels_[0], "IP", info.ipAddress.length() > 0 ? info.ipAddress : String("Unavailable"));
	setRow(rowLabels_[1], "Web UI", info.webUiUrl.length() > 0 ? info.webUiUrl : String("Unavailable"));
	String wifiText = info.wifiSsid.length() > 0 ? info.wifiSsid : String("Unknown SSID");
	if (info.wifiStatus.length() > 0) {
		wifiText += "\nStatus: ";
		wifiText += info.wifiStatus;
	}
	if (info.wifiRssi.length() > 0) {
		wifiText += "\nRSSI: ";
		wifiText += info.wifiRssi;
	}
	setRow(rowLabels_[2], "WiFi", wifiText);
	setRow(rowLabels_[3], "BLE", info.bleStatus.length() > 0 ? info.bleStatus : String("Idle"));
	setRow(rowLabels_[4], "Weather", info.weatherStatus.length() > 0 ? info.weatherStatus : String("Waiting"));
	setRow(rowLabels_[5], "Radar", info.radarStatus.length() > 0 ? info.radarStatus : String("Waiting"));
	setRow(rowLabels_[6], "Updated", info.lastUpdate.length() > 0 ? info.lastUpdate : String("Never"));
	setRow(rowLabels_[7], "Storage", info.spiffsUsage.length() > 0 ? info.spiffsUsage : String("Unknown"));
	setRow(rowLabels_[8], "Build", info.firmwareVersion.length() > 0 ? info.firmwareVersion : String("dev"));
	setRow(rowLabels_[9], "LED", info.ledMode.length() > 0 ? info.ledMode : String("Unknown"));
	setRow(rowLabels_[10], "Page", String("System active"));
}

void SystemPage::onThemeDropdownChanged(lv_event_t* event) {
	if (event == nullptr) {
		return;
	}
	SystemPage* page = static_cast<SystemPage*>(lv_event_get_user_data(event));
	if (page == nullptr) {
		return;
	}
	page->handleThemeDropdownChanged();
}

void SystemPage::handleThemeDropdownChanged() {
	if (themeDropdown_ == nullptr || settingsStore_ == nullptr) {
		return;
	}
	const ThemeId selectedTheme = theme_id_from_index(static_cast<uint8_t>(lv_dropdown_get_selected(themeDropdown_)));
	if (settingsStore_->get_theme() == selectedTheme) {
		return;
	}
	if (!settingsStore_->set_theme(selectedTheme)) {
		syncThemeSelector();
		return;
	}
	Serial.printf("[THEME] User selected theme id=%d name=%s\n", static_cast<int>(selectedTheme), theme_id_to_name(selectedTheme));
	ui_apply_theme_lvgl(selectedTheme);
	if (themeSelectedCallback_ != nullptr) {
		themeSelectedCallback_(themeSelectedContext_, selectedTheme);
	}
	lv_obj_invalidate(root_);
}

void SystemPage::syncThemeSelector() {
	if (themeDropdown_ == nullptr || settingsStore_ == nullptr) {
		return;
	}
	lv_dropdown_set_selected(themeDropdown_, static_cast<uint16_t>(settingsStore_->get_theme()));
}

}  // namespace ui
