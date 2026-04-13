#include "ui_radar.h"

namespace ui {

void RadarPanel::begin(lv_obj_t* parent, ThemeManager& theme) {
	card_ = lv_obj_create(parent);
	lv_obj_set_size(card_, lv_pct(100), lv_pct(100));
	lv_obj_clear_flag(card_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(card_, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_set_style_border_width(card_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(card_, theme.spacing().cardPadding, LV_PART_MAIN);
	
	// Apply semi-transparent card overlay style
	ui_style_card(card_, theme);

	titleLabel_ = lv_label_create(card_);
	lv_obj_set_style_text_color(titleLabel_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(titleLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(titleLabel_, theme.typography().pageTitleZoom, LV_PART_MAIN);
	lv_label_set_text(titleLabel_, "Radar");
	lv_obj_align(titleLabel_, LV_ALIGN_TOP_LEFT, 0, 0);

	metaLabel_ = lv_label_create(card_);
	lv_obj_add_style(metaLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(metaLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_label_set_text(metaLabel_, "frames 0/0");
	lv_obj_align(metaLabel_, LV_ALIGN_TOP_RIGHT, 0, 10);

	stageLabel_ = lv_label_create(card_);
	lv_obj_add_style(stageLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(stageLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(stageLabel_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_label_set_text(stageLabel_, "idle");
	lv_obj_align(stageLabel_, LV_ALIGN_TOP_LEFT, 0, 56);

	image_ = lv_img_create(card_);
	lv_obj_set_style_bg_opa(image_, LV_OPA_20, LV_PART_MAIN);
	lv_obj_set_style_bg_color(image_, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_radius(image_, theme.spacing().imageRadius, LV_PART_MAIN);
	lv_obj_set_style_pad_all(image_, 6, LV_PART_MAIN);
	lv_obj_align(image_, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void RadarPanel::applyTheme(ThemeManager& theme) {
	lv_obj_set_style_border_width(card_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(card_, theme.spacing().cardPadding, LV_PART_MAIN);
	
	// Reapply semi-transparent card overlay for new theme
	ui_style_card(card_, theme);
	
	lv_obj_set_style_text_color(titleLabel_, theme.palette().textPrimary, LV_PART_MAIN);
	lv_obj_set_style_text_font(titleLabel_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(titleLabel_, theme.typography().pageTitleZoom, LV_PART_MAIN);
	lv_obj_add_style(metaLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_add_style(stageLabel_, theme.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(stageLabel_, theme.typography().captionZoom, LV_PART_MAIN);
	lv_obj_set_style_bg_color(image_, theme.palette().surfaceAlt, LV_PART_MAIN);
	lv_obj_set_style_radius(image_, theme.spacing().imageRadius, LV_PART_MAIN);
}

void RadarPanel::setDownloadProgress(size_t completed, size_t total, const char* stage) {
	if (metaLabel_ != nullptr) {
		String meta = "frames " + String(static_cast<unsigned>(completed)) + "/" +
									String(static_cast<unsigned>(total));
		lv_label_set_text(metaLabel_, meta.c_str());
	}
	if (stageLabel_ != nullptr) {
		lv_label_set_text(stageLabel_, stage != nullptr ? stage : "-");
	}
}

void RadarPanel::update(const WeatherData& data, weather::RadarEngine& engine) {
	if (metaLabel_ != nullptr) {
		String meta = "frames " + String(static_cast<unsigned>(engine.completedFrameCount())) + "/" +
									String(static_cast<unsigned>(engine.frameCount()));
		if (data.radarFrameCount > 0) {
			meta += "  src " + String(static_cast<unsigned>(data.radarFrameCount));
		}
		meta += "  idx " + String(static_cast<unsigned>(engine.currentAnimationIndex()));
		if (engine.hasDisplayEffects()) {
			meta += engine.isInterpolating() ? "  interp" : "  fx";
		}
		lv_label_set_text(metaLabel_, meta.c_str());
	}

	const lv_img_dsc_t* frame = engine.currentFrameAsLvglImage();
	if (frame != nullptr && image_ != nullptr) {
		lv_img_set_src(image_, frame);
		return;
	}

	if (stageLabel_ != nullptr) {
		if (engine.isDownloading()) {
			lv_label_set_text(stageLabel_, "downloading");
		} else if (engine.isAnimationReady()) {
			String stage = String("ready ") + String(engine.reflectivityModeLabel());
			if (engine.stormCellCount() > 0) {
				stage += " +cells";
			}
			lv_label_set_text(stageLabel_, stage.c_str());
		} else if (data.lastError == WeatherErrorCode::NotConfigured) {
			lv_label_set_text(stageLabel_, "configure weather to enable radar");
		} else if (data.lastError == WeatherErrorCode::WifiDisconnected) {
			lv_label_set_text(stageLabel_, "waiting for Wi-Fi");
		} else if (engine.lastErrorMessage().length() > 0) {
			lv_label_set_text(stageLabel_, engine.lastErrorMessage().c_str());
		} else {
			lv_label_set_text(stageLabel_, "waiting for radar frames");
		}
	}
}

}  // namespace ui
