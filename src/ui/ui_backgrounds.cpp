#include "ui_backgrounds.h"

#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_heap_caps.h>
#include <PNGdec.h>
#include <new>

#include "ui_assets.h"

namespace ui {
namespace {

lv_obj_t* gBackgroundImage = nullptr;
lv_obj_t* gBackgroundGlowTop = nullptr;
lv_obj_t* gBackgroundGlowBottom = nullptr;
lv_img_dsc_t gAuroraDescriptor = {};
uint8_t* gAuroraPixels = nullptr;
lv_img_dsc_t gThemeDescriptor = {};
lv_color_t* gThemePixels = nullptr;

struct BackdropDecode {
	PNG* decoder;
	lv_color_t* pixels;
};

int decode_backdrop_line(PNGDRAW* draw) {
	auto* context = static_cast<BackdropDecode*>(draw->pUser);
	context->decoder->getLineAsRGB565(draw,
		reinterpret_cast<uint16_t*>(context->pixels + draw->y * 320),
		PNG_RGB565_LITTLE_ENDIAN, 0);
	return 1;
}

bool load_native_theme(lv_obj_t* image, const char* path) {
	File file = SPIFFS.open(path, FILE_READ);
	const size_t size = file ? file.size() : 0;
	if (size == 0 || size > 256U * 1024U) {
		Serial.printf("[BG] ERROR: missing or oversized artwork: %s\n", path);
		return false;
	}
	constexpr size_t kPixels = 320U * 240U * sizeof(lv_color_t);
	if (gThemePixels == nullptr) {
		gThemePixels = static_cast<lv_color_t*>(heap_caps_malloc(kPixels, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	}
	auto* source = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	PNG* decoder = new (std::nothrow) PNG();
	if (gThemePixels == nullptr || source == nullptr || decoder == nullptr) {
		heap_caps_free(source);
		delete decoder;
		Serial.println("[BG] ERROR: artwork decode allocation failed");
		return false;
	}
	bool success = file.read(source, size) == size;
	bool opened = false;
	if (success) {
		opened = decoder->openRAM(source, static_cast<int>(size), decode_backdrop_line) == PNG_SUCCESS;
		success = opened && decoder->getWidth() == 320 && decoder->getHeight() == 240;
	}
	if (success) {
		lv_img_cache_invalidate_src(&gThemeDescriptor);
		BackdropDecode context{decoder, gThemePixels};
		success = decoder->decode(&context, PNG_FAST_PALETTE) == PNG_SUCCESS;
	}
	if (opened) decoder->close();
	delete decoder;
	heap_caps_free(source);
	if (!success) {
		Serial.printf("[BG] ERROR: artwork read/decode failed: %s\n", path);
		return false;
	}
	gThemeDescriptor.header.cf = LV_IMG_CF_TRUE_COLOR;
	gThemeDescriptor.header.w = 320;
	gThemeDescriptor.header.h = 240;
	gThemeDescriptor.data_size = kPixels;
	gThemeDescriptor.data = reinterpret_cast<const uint8_t*>(gThemePixels);
	lv_img_set_src(image, &gThemeDescriptor);
	lv_obj_invalidate(image);
	Serial.printf("[BG] Original artwork resident in PSRAM: %s\n", path);
	return true;
}

bool load_native_aurora(lv_obj_t* image) {
	if (gAuroraPixels == nullptr) {
		constexpr size_t kSize = 320U * 240U * sizeof(lv_color_t);
		File file = SPIFFS.open("/backgrounds/aurora.rgb", FILE_READ);
		if (!file || file.size() != kSize) {
			Serial.println("[BG] Native aurora missing or invalid; using PNG fallback");
			return false;
		}
		uint8_t* pixels = static_cast<uint8_t*>(heap_caps_malloc(kSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
		if (pixels == nullptr) {
			Serial.println("[BG] Native aurora allocation failed; using PNG fallback");
			return false;
		}
		if (file.read(pixels, kSize) != kSize) {
			heap_caps_free(pixels);
			Serial.println("[BG] Native aurora read failed; using PNG fallback");
			return false;
		}
		gAuroraPixels = pixels;
		gAuroraDescriptor.header.cf = LV_IMG_CF_TRUE_COLOR;
		gAuroraDescriptor.header.w = 320;
		gAuroraDescriptor.header.h = 240;
		gAuroraDescriptor.data_size = kSize;
		gAuroraDescriptor.data = gAuroraPixels;
		Serial.println("[BG] Native RGB565 aurora resident in PSRAM");
	}
	lv_img_set_src(image, &gAuroraDescriptor);
	return true;
}

uint32_t mix_hex(uint32_t left, uint32_t right, uint8_t mix) {
	const uint32_t invMix = 255U - mix;
	const uint32_t lr = (left >> 16) & 0xFFU;
	const uint32_t lg = (left >> 8) & 0xFFU;
	const uint32_t lb = left & 0xFFU;
	const uint32_t rr = (right >> 16) & 0xFFU;
	const uint32_t rg = (right >> 8) & 0xFFU;
	const uint32_t rb = right & 0xFFU;
	const uint32_t nr = (lr * invMix + rr * mix) / 255U;
	const uint32_t ng = (lg * invMix + rg * mix) / 255U;
	const uint32_t nb = (lb * invMix + rb * mix) / 255U;
	return (nr << 16) | (ng << 8) | nb;
}

uint32_t fallback_base_for_theme(ThemeId theme, const ThemeColors& colors) {
	switch (theme) {
		case ThemeId::DAYBREAK_CLEAR:
			return 0xBFDFFF;
		case ThemeId::DESERT_CALM:
			return mix_hex(colors.bg_main, colors.accent_primary, 28);
		case ThemeId::MONO_WIREFRAME:
			return 0x11161C;
		default:
			return mix_hex(colors.bg_main, colors.accent_secondary, 18);
	}
}

void update_background_decor(ThemeId theme) {
	const ThemeColors colors = get_theme_colors(theme);
	if (gBackgroundGlowTop != nullptr) {
		lv_obj_set_style_bg_color(gBackgroundGlowTop, lv_color_hex(colors.accent_secondary), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(gBackgroundGlowTop, LV_OPA_30, LV_PART_MAIN);
	}
	if (gBackgroundGlowBottom != nullptr) {
		lv_obj_set_style_bg_color(gBackgroundGlowBottom, lv_color_hex(colors.accent_primary), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(gBackgroundGlowBottom, LV_OPA_20, LV_PART_MAIN);
	}
}

void ensure_background_decor(lv_obj_t* root, ThemeId theme) {
	if (root == nullptr) {
		return;
	}
	if (gBackgroundGlowTop == nullptr) {
		gBackgroundGlowTop = lv_obj_create(root);
		lv_obj_remove_style_all(gBackgroundGlowTop);
		lv_obj_set_size(gBackgroundGlowTop, 180, 180);
		lv_obj_set_style_radius(gBackgroundGlowTop, LV_RADIUS_CIRCLE, LV_PART_MAIN);
		lv_obj_set_style_border_width(gBackgroundGlowTop, 0, LV_PART_MAIN);
		lv_obj_clear_flag(gBackgroundGlowTop, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_align(gBackgroundGlowTop, LV_ALIGN_TOP_LEFT, -34, -28);
	}
	if (gBackgroundGlowBottom == nullptr) {
		gBackgroundGlowBottom = lv_obj_create(root);
		lv_obj_remove_style_all(gBackgroundGlowBottom);
		lv_obj_set_size(gBackgroundGlowBottom, 220, 220);
		lv_obj_set_style_radius(gBackgroundGlowBottom, LV_RADIUS_CIRCLE, LV_PART_MAIN);
		lv_obj_set_style_border_width(gBackgroundGlowBottom, 0, LV_PART_MAIN);
		lv_obj_clear_flag(gBackgroundGlowBottom, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_align(gBackgroundGlowBottom, LV_ALIGN_BOTTOM_RIGHT, 52, 70);
	}
	update_background_decor(theme);
	lv_obj_move_background(gBackgroundGlowBottom);
	lv_obj_move_background(gBackgroundGlowTop);
}

const char* path_for_theme(ThemeId theme) {
	switch (theme) {
		case ThemeId::PIXEL_STORM:
			return "/backgrounds/neon_aurora.png";
		case ThemeId::DESERT_CALM:
			return "/themes/desert_calm.png";
		case ThemeId::FUTURE_PULSE:
			return "/themes/future_pulse.png";
		case ThemeId::MIDNIGHT_RADAR:
			return "/themes/midnight_radar.png";
		case ThemeId::DAYBREAK_CLEAR:
			return "/themes/daybreak_clear.png";
		case ThemeId::STORMGLASS:
			return "/themes/stormglass.png";
		case ThemeId::AURORA_LINE:
			return "/themes/aurora_line.png";
		case ThemeId::OCEAN_FRONT:
			return "/themes/ocean_front.png";
		case ThemeId::MONO_WIREFRAME:
			return "/themes/mono_wireframe.png";
		case ThemeId::INFRARED_SCAN:
			return "/themes/infrared_scan.png";
	}
	return "/backgrounds/neon_aurora.png";
}

void apply_background_fallback(lv_obj_t* root, ThemeId theme, const char* reason) {
	const ThemeColors colors = get_theme_colors(theme);
	lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(root, lv_color_hex(fallback_base_for_theme(theme, colors)), LV_PART_MAIN);
	ensure_background_decor(root, theme);
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

	ensure_background_decor(root, theme);
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
	lv_obj_add_flag(gBackgroundGlowTop, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(gBackgroundGlowBottom, LV_OBJ_FLAG_HIDDEN);
	if (theme == ThemeId::PIXEL_STORM) {
		lv_obj_add_flag(gBackgroundGlowTop, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(gBackgroundGlowBottom, LV_OBJ_FLAG_HIDDEN);
		if (load_native_aurora(gBackgroundImage)) {
			lv_obj_clear_flag(gBackgroundImage, LV_OBJ_FLAG_HIDDEN);
			return;
		}
	} else {
		if (load_native_theme(gBackgroundImage, path_for_theme(theme))) {
			lv_obj_clear_flag(gBackgroundImage, LV_OBJ_FLAG_HIDDEN);
		} else {
			apply_background_fallback(root, theme, "original artwork unavailable");
		}
		return;
	}

	const char* preferred = path_for_theme(theme);
	Serial.printf("[BG] Loading: %s\n", preferred);
	Serial.printf("[BG] Exists: %d\n", SPIFFS.exists(preferred) ? 1 : 0);
	AssetLoadResult result = ui_asset_load_png(gBackgroundImage, preferred);
	if (!result.success) {
		Serial.println("[ASSET] Auto-recovery engaged");
		apply_background_fallback(root, theme, result.error.c_str());
		ui_asset_log_status(result);
		return;
	}

	update_background_decor(theme);
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
