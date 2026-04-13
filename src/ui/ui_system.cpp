#include "ui_system.h"

namespace ui {
namespace {

void setRow(lv_obj_t* label, const char* heading, const String& value) {
	String text = String(heading) + "\n" + value;
	lv_label_set_text(label, text.c_str());
}

}  // namespace

void SystemPage::begin(lv_obj_t* parent, ThemeManager& theme) {
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
	lv_obj_add_style(root_, theme.cardStyle(), LV_PART_MAIN);
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

	list_ = lv_obj_create(root_);
	lv_obj_set_size(list_, lv_pct(100), lv_pct(100));
	lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 0, 82);
	lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(list_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(list_, theme.spacing().listRowGap, LV_PART_MAIN);
	lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
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
	lv_obj_set_style_pad_all(root_, theme.spacing().cardPadding, LV_PART_MAIN);
	lv_obj_set_style_text_color(title_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(title_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(title_, theme.typography().pageTitleZoom, LV_PART_MAIN);
	lv_obj_add_style(subtitle_, theme.captionStyle(), LV_PART_MAIN);
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

}  // namespace ui
