#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "ui_theme.h"

namespace ui {

/**
 * @brief Icon identifier enum for weather and system UI icons.
 */
enum class IconId : uint8_t {
	// Weather icons
	ICON_CLEAR_DAY = 0,
	ICON_CLEAR_NIGHT,
	ICON_PARTLY_CLOUDY,
	ICON_CLOUDY,
	ICON_RAIN,
	ICON_STORM,
	ICON_SNOW,
	ICON_FOG,
	ICON_WIND,

	// Navigation/System icons
	ICON_NAV_HOME,
	ICON_NAV_RADAR,
	ICON_NAV_ALERTS,
	ICON_NAV_SETTINGS,
	ICON_NAV_SYSTEM,
};

/**
 * @brief Get the SPIFFS file path for a given icon ID.
 * @param id The icon identifier.
 * @return Pointer to the file path string (e.g., "/icons/weather_clear_day.png").
 */
const char* ui_icon_get_path(IconId id);

/**
 * @brief Create an icon image object as a child of parent.
 * @param parent Parent LVGL object to attach the icon to.
 * @param id Icon identifier.
 * @param theme Current theme for styling (used for fallback primitive tint).
 * @return Pointer to the created lv_img object, or nullptr if creation failed.
 *
 * On success, returns an lv_img object displaying the PNG from SPIFFS.
 * On failure (file missing, decode error, etc.), creates a fallback primitive
 * (small colored circle or square) so the UI remains usable. Logs all errors
 * with IconId, file path, and reason.
 */
lv_obj_t* ui_icon_create(lv_obj_t* parent, IconId id, ThemeId theme);

/**
 * @brief Set the size of an icon object after creation.
 * @param icon The icon object (returned from ui_icon_create).
 * @param width Width in pixels.
 * @param height Height in pixels.
 */
void ui_icon_set_size(lv_obj_t* icon, uint16_t width, uint16_t height);

/**
 * @brief Delete an icon object and free associated resources.
 * @param icon The icon object to delete.
 */
void ui_icon_delete(lv_obj_t* icon);

/**
 * @brief Convert a weather token string to the best-matching IconId.
 * @param token The weather token (e.g., "sun", "rain", "cloud").
 * @return Best-matching IconId for the given token.
 */
IconId ui_icon_from_token(const char* token);

/**
 * @brief Convert an AccuWeather condition code to the best-matching IconId.
 * Maps condition codes 1-45 (and special codes) to icon types.
 * @param conditionCode AccuWeather condition code (-1 or 1-45).
 * @param isDaylight Whether it's daytime (affects day/night icon selection).
 * @return Best-matching IconId for the condition code.
 */
IconId ui_icon_from_condition_code(int conditionCode, bool isDaylight);

/**
 * @brief Emit a one-shot startup report for icon assets.
 *
 * Logs how many mapped icon files are present in SPIFFS and lists
 * missing primary files once at startup.
 */
void ui_icon_startup_report();

}  // namespace ui
