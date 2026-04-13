#include "ui_boot_anim.h"

#include <SPIFFS.h>

#include "ui_assets.h"

namespace ui {

namespace {

constexpr uint8_t kMaxFrames = 60;
constexpr uint32_t kFrameIntervalMs = 33;
constexpr uint32_t kFallbackDisplayMs = 2200;
constexpr uint32_t kBootBackdropColor = 0x030913;
constexpr uint32_t kBootAccentA = 0x3FE0FF;
constexpr uint32_t kBootAccentB = 0xFF4FD8;

struct BootAnimContext {
	lv_obj_t* container = nullptr;
	lv_obj_t* backdropObj = nullptr;
	lv_obj_t* imageObj = nullptr;
	lv_obj_t* fallbackObj = nullptr;
	lv_obj_t* sweepArc = nullptr;
	lv_obj_t* pulseArc = nullptr;
	lv_obj_t* centerDot = nullptr;
	lv_obj_t* titleLabel = nullptr;
	lv_obj_t* statusLabel = nullptr;
	lv_obj_t* progressBar = nullptr;
	lv_timer_t* timer = nullptr;
	ThemeManager theme;
	BootAnimCompleteCallback onComplete = nullptr;
	void* userContext = nullptr;
	uint8_t currentFrame = 0;
	uint8_t totalFrames = 0;
	uint32_t fallbackStartMs = 0;
	bool finished = false;
	bool usedFallback = false;
	bool callbackFired = false;
};

String frame_path(uint8_t frameIdx) {
	char path[24] = {0};
	snprintf(path, sizeof(path), "/boot/frame_%03u.png", static_cast<unsigned>(frameIdx));
	return String(path);
}

uint8_t discover_frame_count() {
	uint8_t count = 0;
	for (uint8_t i = 0; i < kMaxFrames; ++i) {
		String path = frame_path(i);
		if (!SPIFFS.exists(path.c_str())) {
			break;
		}
		count++;
	}
	Serial.printf("[BOOT] Discovered %u frame(s)\n", static_cast<unsigned>(count));
	return count;
}

lv_obj_t* create_fallback(lv_obj_t* parent, const ThemeManager& theme) {
	(void)theme;
	lv_obj_t* fallback = lv_obj_create(parent);
	lv_obj_remove_style_all(fallback);
	lv_obj_set_size(fallback, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_opa(fallback, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(fallback, lv_color_hex(kBootBackdropColor), LV_PART_MAIN);
	lv_obj_set_style_border_width(fallback, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(fallback, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(fallback, 0, LV_PART_MAIN);
	lv_obj_clear_flag(fallback, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_move_background(fallback);
	Serial.println("[BOOT] Fallback solid background active");
	return fallback;
}

void bring_overlay_to_front(BootAnimContext* ctx) {
	if (ctx == nullptr) {
		return;
	}
	if (ctx->backdropObj != nullptr) {
		lv_obj_move_background(ctx->backdropObj);
	}
	if (ctx->fallbackObj != nullptr) {
		lv_obj_move_background(ctx->fallbackObj);
	}
	if (ctx->imageObj != nullptr) {
		lv_obj_move_foreground(ctx->imageObj);
	}
	if (ctx->sweepArc != nullptr) {
		lv_obj_move_foreground(ctx->sweepArc);
	}
	if (ctx->pulseArc != nullptr) {
		lv_obj_move_foreground(ctx->pulseArc);
	}
	if (ctx->centerDot != nullptr) {
		lv_obj_move_foreground(ctx->centerDot);
	}
	if (ctx->titleLabel != nullptr) {
		lv_obj_move_foreground(ctx->titleLabel);
	}
	if (ctx->statusLabel != nullptr) {
		lv_obj_move_foreground(ctx->statusLabel);
	}
	if (ctx->progressBar != nullptr) {
		lv_obj_move_foreground(ctx->progressBar);
	}
}

void create_overlay(BootAnimContext* ctx) {
	if (ctx == nullptr || ctx->container == nullptr) {
		return;
	}

	ctx->sweepArc = lv_arc_create(ctx->container);
	lv_obj_remove_style_all(ctx->sweepArc);
	lv_obj_set_size(ctx->sweepArc, 136, 136);
	lv_arc_set_range(ctx->sweepArc, 0, 100);
	lv_arc_set_bg_angles(ctx->sweepArc, 0, 360);
	lv_arc_set_rotation(ctx->sweepArc, 270);
	lv_arc_set_value(ctx->sweepArc, 22);
	lv_obj_set_style_arc_width(ctx->sweepArc, 6, LV_PART_MAIN);
	lv_obj_set_style_arc_opa(ctx->sweepArc, LV_OPA_30, LV_PART_MAIN);
	lv_obj_set_style_arc_color(ctx->sweepArc, lv_color_hex(0x173247), LV_PART_MAIN);
	lv_obj_set_style_arc_width(ctx->sweepArc, 7, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(ctx->sweepArc, lv_color_hex(kBootAccentA), LV_PART_INDICATOR);
	lv_obj_align(ctx->sweepArc, LV_ALIGN_CENTER, 0, -18);
	lv_obj_move_foreground(ctx->sweepArc);

	ctx->pulseArc = lv_arc_create(ctx->container);
	lv_obj_remove_style_all(ctx->pulseArc);
	lv_obj_set_size(ctx->pulseArc, 88, 88);
	lv_arc_set_range(ctx->pulseArc, 0, 100);
	lv_arc_set_bg_angles(ctx->pulseArc, 0, 360);
	lv_arc_set_rotation(ctx->pulseArc, 90);
	lv_arc_set_value(ctx->pulseArc, 16);
	lv_obj_set_style_arc_width(ctx->pulseArc, 4, LV_PART_MAIN);
	lv_obj_set_style_arc_opa(ctx->pulseArc, LV_OPA_20, LV_PART_MAIN);
	lv_obj_set_style_arc_color(ctx->pulseArc, lv_color_hex(0x102638), LV_PART_MAIN);
	lv_obj_set_style_arc_width(ctx->pulseArc, 4, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(ctx->pulseArc, lv_color_hex(kBootAccentB), LV_PART_INDICATOR);
	lv_obj_align(ctx->pulseArc, LV_ALIGN_CENTER, 0, -18);
	lv_obj_move_foreground(ctx->pulseArc);

	ctx->centerDot = lv_obj_create(ctx->container);
	lv_obj_remove_style_all(ctx->centerDot);
	lv_obj_set_size(ctx->centerDot, 16, 16);
	lv_obj_set_style_radius(ctx->centerDot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(ctx->centerDot, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ctx->centerDot, lv_color_hex(kBootAccentA), LV_PART_MAIN);
	lv_obj_align(ctx->centerDot, LV_ALIGN_CENTER, 0, -18);
	lv_obj_move_foreground(ctx->centerDot);

	ctx->titleLabel = lv_label_create(ctx->container);
	lv_obj_set_style_text_color(ctx->titleLabel, lv_color_hex(kBootAccentA), LV_PART_MAIN);
	lv_obj_set_style_text_font(ctx->titleLabel, &lv_font_montserrat_28, LV_PART_MAIN);
	lv_label_set_text(ctx->titleLabel, "CORE WEATHER");
	lv_obj_align(ctx->titleLabel, LV_ALIGN_CENTER, 0, 62);
	lv_obj_move_foreground(ctx->titleLabel);

	ctx->statusLabel = lv_label_create(ctx->container);
	lv_obj_set_style_text_color(ctx->statusLabel, lv_color_hex(0xD9F7FF), LV_PART_MAIN);
	lv_obj_set_style_text_font(ctx->statusLabel, &lv_font_montserrat_16, LV_PART_MAIN);
	lv_label_set_text(ctx->statusLabel, "Loading");
	lv_obj_align(ctx->statusLabel, LV_ALIGN_CENTER, 0, 92);
	lv_obj_move_foreground(ctx->statusLabel);

	ctx->progressBar = lv_bar_create(ctx->container);
	lv_obj_set_size(ctx->progressBar, 220, 10);
	lv_bar_set_range(ctx->progressBar, 0, 100);
	lv_bar_set_value(ctx->progressBar, 3, LV_ANIM_OFF);
	lv_obj_set_style_bg_opa(ctx->progressBar, LV_OPA_40, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ctx->progressBar, lv_color_hex(0x123042), LV_PART_MAIN);
	lv_obj_set_style_radius(ctx->progressBar, 5, LV_PART_MAIN);
	lv_obj_set_style_border_width(ctx->progressBar, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ctx->progressBar, lv_color_hex(kBootAccentB), LV_PART_INDICATOR);
	lv_obj_set_style_bg_opa(ctx->progressBar, LV_OPA_COVER, LV_PART_INDICATOR);
	lv_obj_set_style_radius(ctx->progressBar, 5, LV_PART_INDICATOR);
	lv_obj_align(ctx->progressBar, LV_ALIGN_CENTER, 0, 118);
	lv_obj_move_foreground(ctx->progressBar);
	bring_overlay_to_front(ctx);
}

void update_overlay(BootAnimContext* ctx) {
	if (ctx == nullptr) {
		return;
	}

	const uint32_t tick = lv_tick_get();
	const bool pulse = ((tick / 220U) % 2U) == 0U;
	if (ctx->titleLabel != nullptr) {
		lv_obj_set_style_text_color(ctx->titleLabel,
			pulse ? lv_color_hex(kBootAccentA) : lv_color_hex(kBootAccentB),
			LV_PART_MAIN);
	}
	if (ctx->sweepArc != nullptr) {
		lv_arc_set_rotation(ctx->sweepArc, static_cast<uint16_t>((tick / 6U) % 360U));
		lv_arc_set_value(ctx->sweepArc, static_cast<int16_t>(18 + ((tick / 24U) % 45U)));
	}
	if (ctx->pulseArc != nullptr) {
		lv_arc_set_rotation(ctx->pulseArc, static_cast<uint16_t>(360U - ((tick / 10U) % 360U)));
		lv_arc_set_value(ctx->pulseArc, static_cast<int16_t>(12 + ((tick / 30U) % 25U)));
	}
	if (ctx->centerDot != nullptr) {
		const lv_coord_t dotSize = pulse ? 18 : 12;
		lv_obj_set_size(ctx->centerDot, dotSize, dotSize);
		lv_obj_set_style_bg_color(ctx->centerDot,
			pulse ? lv_color_hex(kBootAccentA) : lv_color_hex(kBootAccentB),
			LV_PART_MAIN);
		lv_obj_align(ctx->centerDot, LV_ALIGN_CENTER, 0, -18);
	}

	uint8_t percent = 5;
	if (!ctx->usedFallback && ctx->totalFrames > 0) {
		percent = static_cast<uint8_t>(((static_cast<uint16_t>(ctx->currentFrame) + 1U) * 100U) / ctx->totalFrames);
	} else if (ctx->usedFallback) {
		const uint32_t elapsed = tick - ctx->fallbackStartMs;
		percent = static_cast<uint8_t>((elapsed >= kFallbackDisplayMs)
			? 100U
			: (elapsed * 100U) / kFallbackDisplayMs);
	}
	if (percent < 5) {
		percent = 5;
	}

	if (ctx->progressBar != nullptr) {
		lv_bar_set_value(ctx->progressBar, percent, LV_ANIM_OFF);
	}
	if (ctx->statusLabel != nullptr) {
		const uint8_t dots = static_cast<uint8_t>((tick / 260U) % 4U);
		String text = ctx->usedFallback ? String("Recovering") : String("Loading");
		for (uint8_t i = 0; i < dots; ++i) {
			text += ".";
		}
		lv_label_set_text(ctx->statusLabel, text.c_str());
	}

	bring_overlay_to_front(ctx);
}

void fire_complete(BootAnimContext* ctx) {
	if (ctx == nullptr || ctx->callbackFired) {
		return;
	}
	ctx->callbackFired = true;
	if (ctx->onComplete != nullptr) {
		ctx->onComplete(ctx->userContext);
	}
}

void finish_animation(BootAnimContext* ctx) {
	if (ctx == nullptr || ctx->finished) {
		return;
	}
	ctx->finished = true;
	if (ctx->timer != nullptr) {
		lv_timer_del(ctx->timer);
		ctx->timer = nullptr;
	}
	Serial.println("[BOOT] Animation complete");
	fire_complete(ctx);
}

bool load_frame(BootAnimContext* ctx, uint8_t frameIdx) {
	if (ctx == nullptr || ctx->imageObj == nullptr) {
		return false;
	}
	String path = frame_path(frameIdx);
	Serial.printf("[BOOT] Load frame %u: %s\n", static_cast<unsigned>(frameIdx), path.c_str());
	AssetLoadResult result = ui_asset_load_png(ctx->imageObj, path.c_str());
	if (!result.success) {
		Serial.printf("[BOOT] Frame %u failed: %s\n", static_cast<unsigned>(frameIdx), result.error.c_str());
		ui_asset_log_status(result);
		return false;
	}
	lv_obj_center(ctx->imageObj);
	lv_obj_move_foreground(ctx->imageObj);
	Serial.printf("[BOOT] Frame %u loaded\n", static_cast<unsigned>(frameIdx));
	return true;
}

void enter_fallback(BootAnimContext* ctx, const char* reason) {
	if (ctx == nullptr || ctx->usedFallback) {
		return;
	}
	ctx->usedFallback = true;
	if (ctx->imageObj != nullptr) {
		lv_obj_del(ctx->imageObj);
		ctx->imageObj = nullptr;
	}
	ctx->fallbackObj = create_fallback(ctx->container, ctx->theme);
	ctx->fallbackStartMs = lv_tick_get();
	bring_overlay_to_front(ctx);
	Serial.printf("[BOOT] Fallback reason: %s\n", reason != nullptr ? reason : "unknown");
	Serial.println("[ASSET] Auto-recovery engaged");
}

void boot_timer_cb(lv_timer_t* timer) {
	if (timer == nullptr) {
		return;
	}
	BootAnimContext* ctx = static_cast<BootAnimContext*>(timer->user_data);
	if (ctx == nullptr || ctx->finished) {
		return;
	}

	update_overlay(ctx);

	if (ctx->usedFallback) {
		if ((lv_tick_get() - ctx->fallbackStartMs) >= kFallbackDisplayMs) {
			finish_animation(ctx);
		}
		return;
	}

	const uint8_t nextFrame = static_cast<uint8_t>(ctx->currentFrame + 1);
	if (nextFrame >= ctx->totalFrames) {
		if (ctx->progressBar != nullptr) {
			lv_bar_set_value(ctx->progressBar, 100, LV_ANIM_OFF);
		}
		finish_animation(ctx);
		return;
	}

	ctx->currentFrame = nextFrame;
	if (!load_frame(ctx, ctx->currentFrame)) {
		enter_fallback(ctx, "decode or file load failure");
	}
}

}  // namespace

lv_obj_t* ui_boot_anim_begin(lv_obj_t* parent, const ThemeManager& theme) {
	return ui_boot_anim_play(parent, theme, nullptr, nullptr);
}

lv_obj_t* ui_boot_anim_play(lv_obj_t* parent,
							const ThemeManager& theme,
							BootAnimCompleteCallback onComplete,
							void* userContext) {
	if (parent == nullptr) {
		Serial.println("[BOOT] ERROR: parent is null");
		return nullptr;
	}

	BootAnimContext* ctx = new BootAnimContext();
	if (ctx == nullptr) {
		Serial.println("[BOOT] ERROR: Failed to allocate context");
		return nullptr;
	}

	ctx->theme = theme;
	ctx->onComplete = onComplete;
	ctx->userContext = userContext;

	ctx->container = lv_obj_create(parent);
	lv_obj_remove_style_all(ctx->container);
	lv_obj_set_size(ctx->container, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_opa(ctx->container, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(ctx->container, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(ctx->container, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(ctx->container, 0, LV_PART_MAIN);
	lv_obj_clear_flag(ctx->container, LV_OBJ_FLAG_CLICKABLE);

	// Always paint a dark base first so boot never appears as a blank white screen.
	ctx->backdropObj = lv_obj_create(ctx->container);
	lv_obj_remove_style_all(ctx->backdropObj);
	lv_obj_set_size(ctx->backdropObj, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_opa(ctx->backdropObj, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ctx->backdropObj, lv_color_hex(kBootBackdropColor), LV_PART_MAIN);
	lv_obj_set_style_border_width(ctx->backdropObj, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(ctx->backdropObj, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(ctx->backdropObj, 0, LV_PART_MAIN);
	lv_obj_clear_flag(ctx->backdropObj, LV_OBJ_FLAG_CLICKABLE);

	ctx->container->user_data = ctx;
	create_overlay(ctx);
	update_overlay(ctx);

	ctx->totalFrames = discover_frame_count();
	if (ctx->totalFrames == 0) {
		enter_fallback(ctx, "no boot frames found");
	} else {
		ctx->imageObj = lv_img_create(ctx->container);
		lv_obj_remove_style_all(ctx->imageObj);
		lv_obj_set_style_bg_opa(ctx->imageObj, LV_OPA_TRANSP, LV_PART_MAIN);
		lv_obj_set_style_border_width(ctx->imageObj, 0, LV_PART_MAIN);
		ctx->currentFrame = 0;
		if (!load_frame(ctx, 0)) {
			enter_fallback(ctx, "first frame load failure");
		} else {
			lv_obj_center(ctx->imageObj);
		}
	}

	ctx->timer = lv_timer_create(boot_timer_cb, kFrameIntervalMs, ctx);
	if (ctx->timer == nullptr) {
		Serial.println("[BOOT] ERROR: Failed to create animation timer");
		enter_fallback(ctx, "timer create failed");
	}

	Serial.println("[BOOT] Animation started (timer-driven)");
	return ctx->container;
}

bool ui_boot_anim_update(lv_obj_t* bootObj) {
	if (bootObj == nullptr) {
		return false;
	}
	
	BootAnimContext* ctx = static_cast<BootAnimContext*>(bootObj->user_data);
	if (ctx == nullptr) {
		return false;
	}

	return !ctx->finished;
}

bool ui_boot_anim_finished(lv_obj_t* bootObj) {
	if (bootObj == nullptr) {
		return true;
	}
	
	BootAnimContext* ctx = static_cast<BootAnimContext*>(bootObj->user_data);
	if (ctx == nullptr) {
		return true;
	}
	
	return ctx->finished;
}

}  // namespace ui
