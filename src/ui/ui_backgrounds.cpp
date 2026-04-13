#include "ui_backgrounds.h"

#include <Arduino.h>
#include <SPIFFS.h>

#include "ui_assets.h"

namespace ui {
namespace {

lv_obj_t* gBackgroundImage = nullptr;

const char* fallback_path_for_theme(ThemeId theme) {
	switch (theme) {
		case ThemeId::PIXEL_STORM:
			return "/backgrounds/bgps.png";
		case ThemeId::DESERT_CALM:
			return "/backgrounds/bgdc.png";
		case ThemeId::FUTURE_PULSE:
			return "/backgrounds/bgfp.png";
		default:
			return "/backgrounds/current.png";
	}
}

const char* path_for_theme(ThemeId theme) {
	switch (theme) {
		case ThemeId::PIXEL_STORM:
			return "/backgrounds/pixel_storm.png";
		case ThemeId::DESERT_CALM:
			return "/backgrounds/desert_calm.png";
		case ThemeId::FUTURE_PULSE:
			return "/backgrounds/future_pulse.png";
		case ThemeId::MIDNIGHT_RADAR:
			return "/backgrounds/bgmr.png";
		case ThemeId::DAYBREAK_CLEAR:
			return "/backgrounds/bgdb.png";
		case ThemeId::STORMGLASS:
			return "/backgrounds/bgsg.png";
		case ThemeId::AURORA_LINE:
			return "/backgrounds/bgal.png";
		case ThemeId::OCEAN_FRONT:
			return "/backgrounds/bgof.png";
		case ThemeId::MONO_WIREFRAME:
			return "/backgrounds/bgmw.png";
		case ThemeId::INFRARED_SCAN:
			return "/backgrounds/bgis.png";
	}
	return "/backgrounds/pixel_storm.png";
}

void apply_background_fallback(lv_obj_t* root, ThemeId theme, const char* reason) {
	const ThemeColors colors = get_theme_colors(theme);
	lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(root, lv_color_hex(colors.bg_main), LV_PART_MAIN);
	if (gBackgroundImage != nullptr) {
		lv_obj_add_flag(gBackgroundImage, LV_OBJ_FLAG_HIDDEN);
	}
	Serial.printf("[BG] Fallback color applied: theme=%d reason=%s\n", static_cast<int>(theme), reason);
}

}  // namespace

void ui_backgrounds_init() {
	ui_asset_log_fs_health();
}

const char* ui_backgrounds_get_path_for_theme(ThemeId theme) {
	return path_for_theme(theme);
}

lv_obj_t* ui_background_create(lv_obj_t* root, ThemeId theme) {
	if (root == nullptr) {
		return nullptr;
	}
	if (gBackgroundImage != nullptr) {
		lv_obj_del(gBackgroundImage);
		gBackgroundImage = nullptr;
	}

	gBackgroundImage = lv_img_create(root);
	if (gBackgroundImage == nullptr) {
		Serial.println("[BG] ERROR: failed to create background image object");
		return nullptr;
	}
	lv_obj_set_size(gBackgroundImage, lv_pct(100), lv_pct(100));
	lv_obj_set_style_border_width(gBackgroundImage, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(gBackgroundImage, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_clear_flag(gBackgroundImage, LV_OBJ_FLAG_CLICKABLE);

	ui_background_update(root, theme);
	lv_obj_move_background(gBackgroundImage);
	Serial.println("[BG] Background attached");
	return gBackgroundImage;
}

void ui_background_update(lv_obj_t* root, ThemeId theme) {
	if (root == nullptr || gBackgroundImage == nullptr) {
		return;
	}

	const char* preferred = path_for_theme(theme);
	Serial.printf("[BG] Loading: %s\n", preferred);
	Serial.printf("[BG] Exists: %d\n", SPIFFS.exists(preferred) ? 1 : 0);
	AssetLoadResult result = ui_asset_load_png(gBackgroundImage, preferred);
	if (!result.success) {
		Serial.println("[ASSET] Auto-recovery engaged");
		const char* fallback = fallback_path_for_theme(theme);
		Serial.printf("[BG] Loading: %s\n", fallback);
		Serial.printf("[BG] Exists: %d\n", SPIFFS.exists(fallback) ? 1 : 0);
		AssetLoadResult fallbackResult = ui_asset_load_png(gBackgroundImage, fallback);
		if (!fallbackResult.success) {
			Serial.println("[BG] Decode failed, using fallback color");
			apply_background_fallback(root, theme, fallbackResult.error.c_str());
			ui_asset_log_status(fallbackResult);
			return;
		}
		ui_asset_log_status(fallbackResult);
		Serial.printf("[BG] Load success: %s\n", fallback);
		lv_obj_clear_flag(gBackgroundImage, LV_OBJ_FLAG_HIDDEN);
		return;
	}

	Serial.printf("[BG] Load success: %s\n", preferred);
	lv_obj_clear_flag(gBackgroundImage, LV_OBJ_FLAG_HIDDEN);
	ui_asset_log_status(result);
}

lv_obj_t* ui_backgrounds_attach_to_root(lv_obj_t* root, ThemeId theme) {
	return ui_background_create(root, theme);
}

void ui_backgrounds_update_theme(lv_obj_t* root, ThemeId theme) {
	ui_background_update(root, theme);
}

}  // namespace ui
