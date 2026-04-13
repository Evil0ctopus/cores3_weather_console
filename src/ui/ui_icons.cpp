#include "ui_icons.h"

#include <Arduino.h>
#include <SPIFFS.h>

#include "ui_assets.h"
#include "ui_theme.h"

namespace ui {

namespace {

constexpr size_t kIconCount = 14;
bool gMissingLogged[kIconCount] = {false};
bool gLoadedLogged[kIconCount] = {false};
bool gSpiffsUnavailableLogged = false;
bool gStartupReportLogged = false;

struct IconPathSpec {
	const char* primary;
	const char* secondary;
};

size_t icon_index(IconId id) {
	switch (id) {
		case IconId::ICON_CLEAR_DAY: return 0;
		case IconId::ICON_CLEAR_NIGHT: return 1;
		case IconId::ICON_PARTLY_CLOUDY: return 2;
		case IconId::ICON_CLOUDY: return 3;
		case IconId::ICON_RAIN: return 4;
		case IconId::ICON_STORM: return 5;
		case IconId::ICON_SNOW: return 6;
		case IconId::ICON_FOG: return 7;
		case IconId::ICON_WIND: return 8;
		case IconId::ICON_NAV_HOME: return 9;
		case IconId::ICON_NAV_RADAR: return 10;
		case IconId::ICON_NAV_ALERTS: return 11;
		case IconId::ICON_NAV_SETTINGS: return 12;
		case IconId::ICON_NAV_SYSTEM: return 13;
	}
	return 0;
}

IconPathSpec icon_path_spec(IconId id) {
	switch (id) {
		case IconId::ICON_CLEAR_DAY:
			return {"/icons/sun.png", "/icons/weather_clear_day.png"};
		case IconId::ICON_CLEAR_NIGHT:
			return {"/icons/moon.png", "/icons/weather_clear_night.png"};
		case IconId::ICON_PARTLY_CLOUDY:
			return {"/icons/partly_cloudy.png", "/icons/weather_partly_cloudy.png"};
		case IconId::ICON_CLOUDY:
			return {"/icons/cloud.png", "/icons/weather_cloudy.png"};
		case IconId::ICON_RAIN:
			return {"/icons/rain.png", "/icons/weather_rain.png"};
		case IconId::ICON_STORM:
			return {"/icons/storm.png", "/icons/weather_storm.png"};
		case IconId::ICON_SNOW:
			return {"/icons/snow.png", "/icons/weather_snow.png"};
		case IconId::ICON_FOG:
			return {"/icons/fog.png", "/icons/weather_fog.png"};
		case IconId::ICON_WIND:
			return {"/icons/wind.png", "/icons/weather_wind.png"};
		case IconId::ICON_NAV_HOME:
			return {"/icons/nav_home.png", nullptr};
		case IconId::ICON_NAV_RADAR:
			return {"/icons/nav_radar.png", nullptr};
		case IconId::ICON_NAV_ALERTS:
			return {"/icons/nav_alerts.png", nullptr};
		case IconId::ICON_NAV_SETTINGS:
			return {"/icons/nav_settings.png", nullptr};
		case IconId::ICON_NAV_SYSTEM:
			return {"/icons/nav_system.png", nullptr};
	}
	return {nullptr, nullptr};
}

const char* resolve_icon_path(IconId id) {
	const IconPathSpec spec = icon_path_spec(id);
	if (spec.primary != nullptr && SPIFFS.exists(spec.primary)) {
		return spec.primary;
	}
	if (spec.secondary != nullptr && SPIFFS.exists(spec.secondary)) {
		return spec.secondary;
	}
	return spec.primary;
}

}  // namespace

/**
 * @brief Map IconId to preferred SPIFFS file path.
 */
const char* ui_icon_get_path(IconId id) {
	return icon_path_spec(id).primary;
}

/**
 * @brief Create a simple LVGL shape part for fallback icons.
 */
static lv_obj_t* make_icon_part(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, lv_color_t color, lv_coord_t radius = LV_RADIUS_CIRCLE) {
	lv_obj_t* part = lv_obj_create(parent);
	if (part == nullptr) {
		return nullptr;
	}
	lv_obj_remove_style_all(part);
	lv_obj_set_size(part, w, h);
	lv_obj_set_style_bg_opa(part, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(part, color, LV_PART_MAIN);
	lv_obj_set_style_radius(part, radius, LV_PART_MAIN);
	lv_obj_set_style_border_width(part, 0, LV_PART_MAIN);
	lv_obj_clear_flag(part, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	return part;
}

/**
 * @brief Create a drawn vector-style fallback icon when PNG loading fails.
 */
static lv_obj_t* create_fallback_circle(lv_obj_t* parent, lv_color_t color, IconId id) {
	if (parent == nullptr) {
		return nullptr;
	}

	lv_obj_t* fallback = lv_obj_create(parent);
	if (fallback == nullptr) {
		return nullptr;
	}

	lv_obj_remove_style_all(fallback);
	lv_obj_set_size(fallback, 48, 48);
	lv_obj_set_style_bg_opa(fallback, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(fallback, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(fallback, 0, LV_PART_MAIN);
	lv_obj_clear_flag(fallback, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

	const lv_color_t light = lv_color_mix(lv_color_white(), color, LV_OPA_40);
	const lv_color_t dim = lv_color_mix(color, lv_color_black(), LV_OPA_20);
	const lv_color_t warn = lv_color_hex(0xFFD166);

	switch (id) {
		case IconId::ICON_CLEAR_DAY: {
			const lv_color_t sunCore = lv_color_hex(0xFFD54A);
			lv_obj_t* core = make_icon_part(fallback, 24, 24, sunCore);
			if (core != nullptr) lv_obj_align(core, LV_ALIGN_CENTER, 0, 0);

			const lv_coord_t rayOffsets[8][2] = {
				{0, -16}, {0, 16}, {-16, 0}, {16, 0},
				{-11, -11}, {11, -11}, {-11, 11}, {11, 11}
			};
			for (int i = 0; i < 8; ++i) {
				const bool diagonal = i >= 4;
				lv_obj_t* ray = make_icon_part(
					fallback,
					diagonal ? 6 : ((i < 2) ? 4 : 14),
					diagonal ? 6 : ((i < 2) ? 14 : 4),
					color,
					3);
				if (ray != nullptr) {
					lv_obj_align(ray, LV_ALIGN_CENTER, rayOffsets[i][0], rayOffsets[i][1]);
				}
			}
			break;
		}
		case IconId::ICON_CLEAR_NIGHT: {
			lv_obj_t* moon = make_icon_part(fallback, 22, 22, light);
			if (moon != nullptr) lv_obj_align(moon, LV_ALIGN_CENTER, -2, 0);
			lv_obj_t* cutout = make_icon_part(fallback, 20, 20, lv_color_hex(0x07111A));
			if (cutout != nullptr) lv_obj_align(cutout, LV_ALIGN_CENTER, 6, -2);
			break;
		}
		case IconId::ICON_PARTLY_CLOUDY: {
			lv_obj_t* sun = make_icon_part(fallback, 14, 14, warn);
			if (sun != nullptr) lv_obj_align(sun, LV_ALIGN_CENTER, -10, -10);
			/* fall through */
		}
		case IconId::ICON_CLOUDY: {
			lv_obj_t* p1 = make_icon_part(fallback, 14, 14, light);
			lv_obj_t* p2 = make_icon_part(fallback, 16, 16, light);
			lv_obj_t* p3 = make_icon_part(fallback, 13, 13, light);
			lv_obj_t* base = make_icon_part(fallback, 28, 12, light, 6);
			if (p1 != nullptr) lv_obj_align(p1, LV_ALIGN_CENTER, -10, 2);
			if (p2 != nullptr) lv_obj_align(p2, LV_ALIGN_CENTER, 0, -2);
			if (p3 != nullptr) lv_obj_align(p3, LV_ALIGN_CENTER, 10, 2);
			if (base != nullptr) lv_obj_align(base, LV_ALIGN_CENTER, 0, 8);
			break;
		}
		case IconId::ICON_RAIN: {
			lv_obj_t* p1 = make_icon_part(fallback, 14, 14, light);
			lv_obj_t* p2 = make_icon_part(fallback, 16, 16, light);
			lv_obj_t* p3 = make_icon_part(fallback, 13, 13, light);
			lv_obj_t* base = make_icon_part(fallback, 28, 12, light, 6);
			if (p1 != nullptr) lv_obj_align(p1, LV_ALIGN_CENTER, -10, -4);
			if (p2 != nullptr) lv_obj_align(p2, LV_ALIGN_CENTER, 0, -8);
			if (p3 != nullptr) lv_obj_align(p3, LV_ALIGN_CENTER, 10, -4);
			if (base != nullptr) lv_obj_align(base, LV_ALIGN_CENTER, 0, 2);
			for (int i = 0; i < 3; ++i) {
				lv_obj_t* drop = make_icon_part(fallback, 4, 10, color, 3);
				if (drop != nullptr) lv_obj_align(drop, LV_ALIGN_CENTER, -8 + i * 8, 15);
			}
			break;
		}
		case IconId::ICON_STORM: {
			lv_obj_t* cloud = make_icon_part(fallback, 28, 14, light, 7);
			if (cloud != nullptr) lv_obj_align(cloud, LV_ALIGN_CENTER, 0, -4);
			lv_obj_t* bolt1 = make_icon_part(fallback, 8, 4, warn, 1);
			lv_obj_t* bolt2 = make_icon_part(fallback, 4, 10, warn, 1);
			lv_obj_t* bolt3 = make_icon_part(fallback, 8, 4, warn, 1);
			if (bolt1 != nullptr) lv_obj_align(bolt1, LV_ALIGN_CENTER, 2, 8);
			if (bolt2 != nullptr) lv_obj_align(bolt2, LV_ALIGN_CENTER, -2, 14);
			if (bolt3 != nullptr) lv_obj_align(bolt3, LV_ALIGN_CENTER, 4, 18);
			break;
		}
		case IconId::ICON_SNOW: {
			lv_obj_t* cloud = make_icon_part(fallback, 28, 14, light, 7);
			if (cloud != nullptr) lv_obj_align(cloud, LV_ALIGN_CENTER, 0, -4);
			for (int i = 0; i < 3; ++i) {
				lv_obj_t* flake = make_icon_part(fallback, 6, 6, lv_color_white());
				if (flake != nullptr) lv_obj_align(flake, LV_ALIGN_CENTER, -8 + i * 8, 14);
			}
			break;
		}
		case IconId::ICON_FOG: {
			for (int i = 0; i < 3; ++i) {
				lv_obj_t* band = make_icon_part(fallback, 30 - i * 4, 4, light, 2);
				if (band != nullptr) lv_obj_align(band, LV_ALIGN_CENTER, 0, -8 + i * 8);
			}
			break;
		}
		case IconId::ICON_WIND: {
			for (int i = 0; i < 3; ++i) {
				lv_obj_t* streak = make_icon_part(fallback, 28 - i * 4, 4, dim, 2);
				if (streak != nullptr) lv_obj_align(streak, LV_ALIGN_CENTER, 0, -8 + i * 8);
			}
			break;
		}
		default: {
			lv_obj_t* dot = make_icon_part(fallback, 20, 20, color);
			if (dot != nullptr) lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);
			break;
		}
	}

	return fallback;
}

/**
 * @brief Check if SPIFFS is ready.
 */
static bool ensure_spiffs_ready() {
	if (SPIFFS.totalBytes() > 0) {
		return true;
	}
	if (SPIFFS.begin(false)) {
		return true;
	}
	if (!gSpiffsUnavailableLogged) {
		Serial.println("[ICON] ERROR: SPIFFS is not mounted.");
		gSpiffsUnavailableLogged = true;
	}
	return false;
}

lv_obj_t* ui_icon_create(lv_obj_t* parent, IconId id, ThemeId theme) {
	if (parent == nullptr) {
		Serial.println("[ICON] ERROR: ui_icon_create parent is null.");
		return nullptr;
	}

	const IconPathSpec spec = icon_path_spec(id);
	const char* path = resolve_icon_path(id);
	if (path == nullptr) {
		Serial.printf("[ICON] ERROR: Unknown IconId %d\n", static_cast<int>(id));
		Serial.println("[ASSET] Auto-recovery engaged");
		return create_fallback_circle(parent, lv_color_hex(0xFF6B6B), id);
	}
	Serial.printf("[ICON] Loading id=%d path=%s\n", static_cast<int>(id), path);

	// Check if SPIFFS is available
	if (!ensure_spiffs_ready()) {
		Serial.printf("[ICON] Failed to load icon id %d: SPIFFS unavailable\n", static_cast<int>(id));
		Serial.println("[ASSET] Auto-recovery engaged");
		const ThemeColors colors = get_theme_colors(theme);
		return create_fallback_circle(parent, lv_color_hex(colors.accent_primary), id);
	}

	// Check if file exists
	if (!SPIFFS.exists(path)) {
		const size_t idx = icon_index(id);
		if (idx < kIconCount && !gMissingLogged[idx]) {
			Serial.printf("[ICON] Missing icon id %d primary=%s secondary=%s\n",
				static_cast<int>(id),
				spec.primary != nullptr ? spec.primary : "(null)",
				spec.secondary != nullptr ? spec.secondary : "(none)");
			gMissingLogged[idx] = true;
		}
		Serial.println("[ASSET] Auto-recovery engaged");
		const ThemeColors colors = get_theme_colors(theme);
		return create_fallback_circle(parent, lv_color_hex(colors.accent_secondary), id);
	}

	// Create the image object
	lv_obj_t* icon = lv_img_create(parent);
	if (icon == nullptr) {
		Serial.printf("[ICON] Failed to create lv_img object for icon id %d\n", static_cast<int>(id));
		const ThemeColors colors = get_theme_colors(theme);
		return create_fallback_circle(parent, lv_color_hex(colors.text_secondary), id);
	}

	AssetLoadResult loadResult = ui_asset_load_png(icon, path);
	if (!loadResult.success) {
		Serial.printf("[ICON] Failed to load PNG image for icon id %d from path %s\n",
			static_cast<int>(id), path);
		Serial.println("[ASSET] Auto-recovery engaged");
		ui_asset_log_status(loadResult);
		lv_obj_del(icon);
		const ThemeColors colors = get_theme_colors(theme);
		return create_fallback_circle(parent, lv_color_hex(colors.text_secondary), id);
	}

	const size_t idx = icon_index(id);
	if (idx < kIconCount && !gLoadedLogged[idx]) {
		Serial.printf("[ICON] Loaded icon id %d from %s\n",
			static_cast<int>(id), path);
		gLoadedLogged[idx] = true;
	}
	Serial.printf("[ICON] Load success id=%d path=%s\n", static_cast<int>(id), path);

	// Set some default styling
	lv_obj_set_style_pad_all(icon, 0, LV_PART_MAIN);

	return icon;
}

void ui_icon_startup_report() {
	if (gStartupReportLogged) {
		return;
	}
	gStartupReportLogged = true;

	if (!ensure_spiffs_ready()) {
		Serial.println("[ICON] Startup report: SPIFFS unavailable");
		return;
	}

	unsigned found = 0;
	for (int i = 0; i < static_cast<int>(kIconCount); ++i) {
		IconId id = static_cast<IconId>(i);
		const IconPathSpec spec = icon_path_spec(id);
		const bool hasPrimary = (spec.primary != nullptr) && SPIFFS.exists(spec.primary);
		const bool hasSecondary = (spec.secondary != nullptr) && SPIFFS.exists(spec.secondary);
		if (hasPrimary || hasSecondary) {
			found++;
		} else {
			Serial.printf("[ICON] Startup missing id=%d primary=%s secondary=%s\n",
				i,
				spec.primary != nullptr ? spec.primary : "(null)",
				spec.secondary != nullptr ? spec.secondary : "(none)");
		}
	}

	Serial.printf("[ICON] Startup report: available=%u/%u\n",
		found,
		static_cast<unsigned>(kIconCount));
}

void ui_icon_set_size(lv_obj_t* icon, uint16_t width, uint16_t height) {
	if (icon == nullptr) {
		return;
	}
	lv_obj_set_size(icon, width, height);
}

void ui_icon_delete(lv_obj_t* icon) {
	if (icon != nullptr) {
		lv_obj_del(icon);
	}
}

/**
 * @brief Convert a weather token to the best-matching IconId.
 * @param token The weather token (e.g., "sun", "rain", "cloud").
 * @return Best-matching IconId for the given token.
 */
IconId ui_icon_from_token(const char* token) {
	if (token == nullptr) {
		return IconId::ICON_CLOUDY;
	}

	// Day/Night cases
	if (strcmp(token, "sun") == 0) {
		return IconId::ICON_CLEAR_DAY;
	}
	if (strcmp(token, "moon") == 0) {
		return IconId::ICON_CLEAR_NIGHT;
	}

	// Cloud/Overcast
	if (strcmp(token, "cloud") == 0 || strcmp(token, "mostly_cloudy") == 0 || 
		strcmp(token, "overcast") == 0) {
		return IconId::ICON_CLOUDY;
	}
	if (strcmp(token, "partly_cloudy") == 0) {
		return IconId::ICON_PARTLY_CLOUDY;
	}

	// Precipitation
	if (strcmp(token, "rain") == 0 || strcmp(token, "showers") == 0) {
		return IconId::ICON_RAIN;
	}
	if (strcmp(token, "snow") == 0 || strcmp(token, "flurries") == 0) {
		return IconId::ICON_SNOW;
	}
	if (strcmp(token, "mix") == 0) {
		return IconId::ICON_RAIN;  // Mixed precip mapped to rain
	}
	if (strcmp(token, "hail") == 0) {
		return IconId::ICON_STORM;  // Hail is serious, map to storm
	}

	// Severe Weather
	if (strcmp(token, "storm") == 0) {
		return IconId::ICON_STORM;
	}

	// Atmospheric
	if (strcmp(token, "fog") == 0) {
		return IconId::ICON_FOG;
	}
	if (strcmp(token, "wind") == 0) {
		return IconId::ICON_WIND;
	}

	// Temperature extremes mapped to related weather
	if (strcmp(token, "heat") == 0) {
		return IconId::ICON_CLEAR_DAY;
	}
	if (strcmp(token, "cold") == 0) {
		return IconId::ICON_SNOW;
	}

	// Default fallback
	return IconId::ICON_CLOUDY;
}

/**
 * @brief Convert an AccuWeather condition code to the best-matching IconId.
 * Maps condition codes 1-45 (and special codes) to icon types.
 * @param conditionCode AccuWeather condition code (-1 or 1-45).
 * @param isDaylight Whether it's daytime (affects day/night icon selection).
 * @return Best-matching IconId for the condition code.
 */
IconId ui_icon_from_condition_code(int conditionCode, bool isDaylight) {
	// Handle unknown/missing codes
	if (conditionCode < 0) {
		return isDaylight ? IconId::ICON_CLEAR_DAY : IconId::ICON_CLEAR_NIGHT;
	}

	// Map AccuWeather codes to IconId
	switch (conditionCode) {
		case 1:  // Sunny
		case 2:  // Mostly sunny
			return IconId::ICON_CLEAR_DAY;
		case 33: // Clear (night)
		case 34: // Mostly clear (night)
			return IconId::ICON_CLEAR_NIGHT;

		case 3:  // Partly sunny
		case 35: // Partly cloudy (night)
			return IconId::ICON_PARTLY_CLOUDY;

		case 4:  // Intermittent clouds
		case 6:  // Mostly cloudy
		case 7:  // Overcast
		case 8:  // Drizzle
		case 11: // Light rain
			return IconId::ICON_CLOUDY;

		case 5:  // Hazy
		case 9:  // Rain
		case 10: // Snow/Rain mix
		case 12: // Light rain
		case 13: // Moderate rain
		case 14: // Heavy rain
		case 18: // Rain
			return IconId::ICON_RAIN;

		case 15: // Thunderstorm
		case 16: // Rainy thunderstorm
		case 17: // Thunderstorm
		case 39: // Isolated thunderstorms
		case 40: // Scattered thunderstorms
			return IconId::ICON_STORM;

		case 19: // Snow
		case 20: // Snow/Rain mix
		case 21: // Snow/Sleet mix
		case 22: // Sleet
		case 23: // Snow showers
		case 24: // Blowing snow
		case 25: // Sleet
		case 29: // Heavy snow
			return IconId::ICON_SNOW;

		case 30: // Hot
			return IconId::ICON_CLEAR_DAY;  // Often sunny and hot

		case 31: // Cold
			return IconId::ICON_SNOW;  // Cold often correlates with snow

		case 32: // Windy
		case 36: // Windy
			return IconId::ICON_WIND;

		case 37: // Tornado
		case 38: // Tropical storm
			return IconId::ICON_STORM;

		case 41: // Fog
		case 42: // Freezing fog
		case 43: // Sleet
		case 44: // Blowing snow
		case 45: // Blowing sleet
			return IconId::ICON_FOG;

		default:
			return isDaylight ? IconId::ICON_CLEAR_DAY : IconId::ICON_CLEAR_NIGHT;
	}
}

}  // namespace ui
