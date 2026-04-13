#pragma once

#include <lvgl.h>

#include "ui_theme.h"

namespace ui {

typedef void (*BootAnimCompleteCallback)(void* userContext);

/**
 * @brief Start playing the PNG-based boot animation.
 * 
 * @param parent Parent screen to attach animation to.
 * @param theme Theme providing styling and fallback colors.
 * @return The root object for the animation, or nullptr if init failed.
 * 
 * Loads PNG frames from /boot/frame_000.png, frame_001.png, etc.
 * Plays them sequentially with 20ms intervals.
 * If any frame fails to load, uses a solid color fallback.
 * Logs all frame loads and playback progress with [BOOT] prefix.
 */
lv_obj_t* ui_boot_anim_begin(lv_obj_t* parent, const ThemeManager& theme);

/**
 * @brief Start boot animation playback using an internal LVGL timer.
 *
 * Loads PNG frames from /boot/frame_000.png ... /boot/frame_XXX.png.
 * If a frame is missing or decode fails, switches to solid bg_main fallback
 * for one second and then completes.
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
 * Non-blocking update that advances to the next frame when appropriate.
 * Logs when animation completes.
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
