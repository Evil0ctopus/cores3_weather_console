#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace ui {

enum AssetType {
	ASSET_BACKGROUND,
	ASSET_ICON,
	ASSET_BOOT_FRAME
};

/**
 * @brief Asset loading result with status and diagnostics.
 */
struct AssetLoadResult {
	bool success;          /// True if asset loaded without fallback
	bool fallbackUsed;     /// True if fallback was engaged
	String path;           /// Path to the asset
	String error;          /// Error message if fallback was used
};

/**
 * @brief Load and display a PNG image from SPIFFS into an lv_img object.
 * 
 * @param imgObj The lv_img object to populate.
 * @param path The SPIFFS path to the PNG file.
 * @return Result indicating success/failure and fallback status.
 * 
 * On failure, applies a fallback (solid color) to the parent.
 * Always logs via Serial with [ASSET] prefix.
 */
AssetLoadResult ui_asset_load_png(lv_obj_t* imgObj, const char* path);

/**
 * @brief Check if an asset file exists on SPIFFS.
 * 
 * @param path The SPIFFS path to check.
 * @return True if file exists and is readable.
 */
bool ui_asset_exists(const char* path);

/**
 * @brief Log the full status of an asset load result.
 * 
 * @param result The AssetLoadResult to log.
 * 
 * Prints: "[ASSET] success=X fallback=Y path=Z error=E"
 */
void ui_asset_log_status(const AssetLoadResult& result);

/**
 * @brief Log SPIFFS filesystem health.
 * 
 * Prints: "[FS] total=X used=Y available=Z"
 */
void ui_asset_log_fs_health();

void ui_asset_startup_report();

}  // namespace ui
