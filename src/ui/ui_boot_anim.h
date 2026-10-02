#pragma once

#include <lvgl.h>

#include "ui_theme.h"

namespace ui {

typedef void (*BootAnimCompleteCallback)(void* userContext);

/**
 * @brief Start the timer-driven Weather Atlas boot animation.
 * 
 * @param parent Parent screen to attach animation to.
 * @param theme Theme providing styling and fallback colors.
 * @return The root object for the animation, or nullptr if init failed.
 * 
 * Loads /boot/neon_intro.png once and animates a reveal and overlays for 3.6 seconds.
 * If the backdrop cannot load, uses a solid color fallback.
 */
lv_obj_t* ui_boot_anim_begin(lv_obj_t* parent, const ThemeManager& theme);

/**
 * @brief Start boot animation playback using an internal LVGL timer.
 *
 * Uses a single PNG backdrop, avoiding repeated full-screen PNG decoding.
 * Missing artwork uses the same timed animation on a solid background.
 */
lv_obj_t* ui_boot_anim_play(lv_obj_t* parent,
							const ThemeManager& theme,
							BootAnimCompleteCallback onComplete,
							void* userContext);

/**
 * @brief Update the boot animation (call from timer or main loop).
 * 
 * @param bootObj The object returned from ui_boot_anim_begin.
 * @return True if animation is still playing, false if finished.
 * 
 * Returns the playback state; the internal timer advances the animation.
 */
bool ui_boot_anim_update(lv_obj_t* bootObj);

/**
 * @brief Check if boot animation has finished.
 * 
 * @param bootObj The object returned from ui_boot_anim_begin.
 * @return True if animation is complete.
 */
bool ui_boot_anim_finished(lv_obj_t* bootObj);

}  // namespace ui
