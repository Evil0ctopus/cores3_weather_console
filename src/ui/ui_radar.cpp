#include "ui_radar.h"

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

void style_meta_chip(lv_obj_t* obj, ThemeManager& theme) {
	lv_obj_add_style(obj, theme.chipStyle(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(obj, LV_OPA_70, LV_PART_MAIN);
	lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN);
}

constexpr uint16_t kRadarImageZoom = 320;
constexpr int kRadarImageOffsetX = 38;

}  // namespace

void RadarPanel::begin(lv_obj_t* parent, ThemeManager& theme) {
	card_ = lv_obj_create(parent);
	lv_obj_set_size(card_, lv_pct(100), lv_pct(100));
	lv_obj_clear_flag(card_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(card_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_style_border_width(card_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(card_, theme.spacing().cardPadding, LV_PART_MAIN);
	ui_style_card(card_, theme);

	titleLabel_ = lv_label_create(card_);
	style_title_chip(titleLabel_, theme, "Radar");
	lv_obj_align(titleLabel_, LV_ALIGN_TOP_LEFT, 0, 0);
	lv_obj_move_foreground(titleLabel_);

	metaLabel_ = lv_label_create(card_);
	style_meta_chip(metaLabel_, theme);
	lv_label_set_text(metaLabel_, "0 frames");
	lv_obj_align(metaLabel_, LV_ALIGN_TOP_RIGHT, 0, 0);

	stageLabel_ = lv_label_create(card_);
	lv_obj_add_style(stageLabel_, theme.bodyStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(stageLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(stageLabel_, 118, LV_PART_MAIN);
	lv_obj_set_width(stageLabel_, lv_pct(100));
	lv_label_set_long_mode(stageLabel_, LV_LABEL_LONG_WRAP);
	lv_label_set_text(stageLabel_, "Live radar imagery will appear here.");
	lv_obj_align(stageLabel_, LV_ALIGN_TOP_LEFT, 0, 34);

	alertLabel_ = nullptr;

	imageFrame_ = lv_obj_create(card_);
	lv_obj_set_size(imageFrame_, lv_pct(100), 136);
	lv_obj_align(imageFrame_, LV_ALIGN_BOTTOM_MID, -8, 0);
	lv_obj_clear_flag(imageFrame_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_border_width(imageFrame_, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(imageFrame_, LV_OPA_40, LV_PART_MAIN);
	lv_obj_set_style_bg_color(imageFrame_, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_radius(imageFrame_, theme.spacing().imageRadius, LV_PART_MAIN);
	lv_obj_set_style_pad_all(imageFrame_, 0, LV_PART_MAIN);
	lv_obj_set_style_clip_corner(imageFrame_, true, LV_PART_MAIN);

	image_ = lv_img_create(imageFrame_);
	lv_obj_set_size(image_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_img_set_zoom(image_, 256);
	lv_obj_center(image_);

	locationDot_ = lv_obj_create(imageFrame_);
	lv_obj_set_size(locationDot_, 12, 12);
	lv_obj_set_style_radius(locationDot_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	lv_obj_set_style_border_width(locationDot_, 2, LV_PART_MAIN);
	lv_obj_set_style_border_color(locationDot_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
	lv_obj_set_style_bg_color(locationDot_, lv_color_hex(0xFF4D4F), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(locationDot_, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_clear_flag(locationDot_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_align(locationDot_, LV_ALIGN_CENTER, 0, 0);
	lv_obj_move_foreground(locationDot_);

	attributionLabel_ = lv_label_create(imageFrame_);
	lv_label_set_text(attributionLabel_, "© OSM");
	lv_obj_set_style_text_font(attributionLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(attributionLabel_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
	lv_obj_set_style_bg_color(attributionLabel_, lv_color_hex(0x101820), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(attributionLabel_, LV_OPA_70, LV_PART_MAIN);
	lv_obj_set_style_radius(attributionLabel_, 6, LV_PART_MAIN);
	lv_obj_set_style_pad_left(attributionLabel_, 5, LV_PART_MAIN);
	lv_obj_set_style_pad_right(attributionLabel_, 5, LV_PART_MAIN);
	lv_obj_set_style_pad_top(attributionLabel_, 2, LV_PART_MAIN);
	lv_obj_set_style_pad_bottom(attributionLabel_, 2, LV_PART_MAIN);
	lv_obj_align(attributionLabel_, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
	lv_obj_move_foreground(attributionLabel_);
}

void RadarPanel::applyTheme(ThemeManager& theme) {
	ui_style_card(card_, theme);
	lv_obj_set_style_border_width(card_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(card_, theme.spacing().cardPadding, LV_PART_MAIN);
	style_title_chip(titleLabel_, theme, "Radar");
	style_meta_chip(metaLabel_, theme);
	lv_obj_add_style(stageLabel_, theme.bodyStyle(), LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(stageLabel_, 118, LV_PART_MAIN);
	lv_obj_set_style_bg_color(imageFrame_, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_radius(imageFrame_, theme.spacing().imageRadius, LV_PART_MAIN);
}

void RadarPanel::setDownloadProgress(size_t completed, size_t total, const char* stage) {
	if (metaLabel_ != nullptr) {
		String meta = String(static_cast<unsigned>(completed)) + "/" + String(static_cast<unsigned>(total)) + " frames";
		lv_label_set_text(metaLabel_, meta.c_str());
	}
	if (stageLabel_ != nullptr && stage != nullptr) {
		lv_label_set_text(stageLabel_, stage);
	}
}

void RadarPanel::update(const WeatherData& data, weather::RadarEngine& engine) {
	if (metaLabel_ != nullptr) {
		String meta;
		if (engine.isAnimationReady()) {
			meta = "RainViewer • local view";
		} else if (engine.lastErrorMessage().length() > 0) {
			meta = "Radar error";
		} else if (engine.isDownloading()) {
			meta = "Refreshing radar";
		} else {
			meta = "Waiting for radar";
		}
		lv_label_set_text(metaLabel_, meta.c_str());
	}

	const lv_img_dsc_t* frame = engine.currentFrameAsLvglImage();
	if (frame != nullptr && image_ != nullptr) {
		const int32_t sourceWidth = frame->header.w > 0 ? static_cast<int32_t>(frame->header.w) : 256;
		const int32_t sourceHeight = frame->header.h > 0 ? static_cast<int32_t>(frame->header.h) : 256;
		const int32_t mapOffsetX = (((sourceWidth / 2) - static_cast<int32_t>(data.radarMarkerX)) * static_cast<int32_t>(kRadarImageZoom) / 256) + kRadarImageOffsetX;
		const int32_t mapOffsetY = ((sourceHeight / 2) - static_cast<int32_t>(data.radarMarkerY)) * static_cast<int32_t>(kRadarImageZoom) / 256;
		lv_img_set_src(image_, frame);
		lv_img_set_zoom(image_, kRadarImageZoom);
		lv_obj_align(image_, LV_ALIGN_CENTER, static_cast<lv_coord_t>(mapOffsetX), static_cast<lv_coord_t>(mapOffsetY));
	}
	if (locationDot_ != nullptr) {
		lv_obj_align(locationDot_, LV_ALIGN_CENTER, 0, 0);
		lv_obj_move_foreground(locationDot_);
	}

	if (stageLabel_ != nullptr) {
		if (engine.isDownloading()) {
			lv_label_set_text(stageLabel_, "Refreshing radar image for this location.");
		} else if (engine.isAnimationReady()) {
			String stage = String("Center dot marks ") + (data.locationName.length() > 0 ? data.locationName : String("your saved location"));
			if (data.alertCount > 0) {
				stage += " • warned area";
			}
			lv_label_set_text(stageLabel_, stage.c_str());
		} else if (data.lastError == WeatherErrorCode::NotConfigured) {
			lv_label_set_text(stageLabel_, "Configure weather to unlock live radar.");
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			lv_label_set_text(stageLabel_, "Radar is waiting for Wi-Fi to reconnect.");
		} else if (engine.lastErrorMessage().length() > 0) {
			lv_label_set_text(stageLabel_, engine.lastErrorMessage().c_str());
		} else if (data.lastErrorMessage.length() > 0) {
			lv_label_set_text(stageLabel_, data.lastErrorMessage.c_str());
		} else {
			lv_label_set_text(stageLabel_, "Waiting for the next radar update.");
		}
	}
}

}  // namespace ui
