#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace ui {

enum class ThemeId : uint8_t {
	PIXEL_STORM = 0,
	DESERT_CALM,
	FUTURE_PULSE,
	MIDNIGHT_RADAR,
	DAYBREAK_CLEAR,
	STORMGLASS,
	AURORA_LINE,
	OCEAN_FRONT,
	MONO_WIREFRAME,
	INFRARED_SCAN,
};

struct ThemeColors {
	uint32_t bg_main;
	uint32_t bg_card;
	uint32_t bg_tab;
	uint32_t accent_primary;
	uint32_t accent_secondary;
	uint32_t accent_warning;
	uint32_t text_primary;
	uint32_t text_secondary;
	uint32_t border_soft;
};

struct ThemePalette {
	lv_color_t bg;
	lv_color_t surface;
	lv_color_t surfaceAlt;
	lv_color_t textPrimary;
	lv_color_t textSecondary;
	lv_color_t accent;
	lv_color_t shadow;
	lv_color_t warning;
};

struct ThemeTypography {
	int16_t sectionLetterSpace = 1;
	int16_t titleLineSpace = 4;
	int16_t bodyLineSpace = 6;
	int16_t summaryLineSpace = 10;
	uint16_t pageTitleZoom = 190;
	uint16_t captionZoom = 128;
	uint16_t heroValueZoom = 300;
	uint16_t heroSummaryZoom = 170;
	uint16_t weeklyRowZoom = 125;
	uint16_t alertRowZoom = 122;
	uint16_t bootTitleZoom = 175;
};

struct ThemeSpacing {
	uint16_t screenPadding = 12;
	uint16_t cardPadding = 20;
	uint16_t currentCardPadding = 22;
	uint16_t cardAltPadding = 16;
	uint16_t cardRadius = 24;
	uint16_t cardAltRadius = 18;
	uint16_t listRowGap = 12;
	uint16_t weeklyListTop = 56;
	uint16_t alertsListTop = 82;
	uint16_t imageRadius = 18;
	uint16_t chipPadX = 12;
	uint16_t chipPadY = 6;
	uint16_t iconChipPadX = 14;
	uint16_t iconChipPadY = 8;
};

struct ThemeAccentRules {
	lv_color_t chipText;
	lv_color_t iconTint;
	lv_color_t iconMutedTint;
};

ThemeColors get_theme_colors(ThemeId id);
const char* theme_id_to_name(ThemeId id);
const char* theme_id_to_storage_key(ThemeId id);
bool parse_theme_id(const String& value, ThemeId& out);
uint8_t theme_count();
ThemeId theme_id_from_index(uint8_t index);
void ui_apply_theme_lvgl(ThemeId id);
void ui_theme_apply_to_root(lv_obj_t* root, ThemeId id);
void ui_make_container_transparent(lv_obj_t* obj);

/**
 * @brief Make an object completely transparent (all opacity layers).
 * 
 * @param obj The LVGL object to make transparent.
 * 
 * Sets all opacity-related styles to transparent:
 * - Background opacity
 * - Border opacity
 * - Outline opacity
 * - Shadow opacity
 * 
 * Allows background images and content behind the object to show through.
 */
void ui_make_transparent(lv_obj_t* obj);

class ThemeManager {
 public:
	ThemeManager();
	~ThemeManager();

	void begin(ThemeId id);
	void setTheme(ThemeId id);

	ThemeId themeId() const;
	const ThemePalette& palette() const;
	const ThemeTypography& typography() const;
	const ThemeSpacing& spacing() const;
	const ThemeAccentRules& accentRules() const;
	String weatherIconToken(int conditionCode, bool isDaylight) const;
	lv_color_t weatherIconColor(int conditionCode, bool isDaylight) const;

	lv_style_t* screenStyle();
	lv_style_t* cardStyle();
	lv_style_t* cardAltStyle();
	lv_style_t* titleStyle();
	lv_style_t* bodyStyle();
	lv_style_t* captionStyle();
	lv_style_t* chipStyle();
	lv_style_t* tabStyle();
	lv_style_t* defaultLabelStyle();

 private:
	void loadIconMap();
	bool ensureFilesystemReady();
	lv_color_t parseColor(const char* value, lv_color_t fallback) const;
	lv_color_t colorForRole(const String& role) const;

	struct WeatherIconEntry {
		String token;
		String role;
	};

	bool initialized_ = false;
	bool filesystemReady_ = false;
	bool iconMapLoaded_ = false;
	ThemeId themeId_ = ThemeId::PIXEL_STORM;
	WeatherIconEntry weatherIcons_[45]{};
	WeatherIconEntry defaultWeatherIcon_{};
};

/**
 * @brief Style an object as a semi-transparent card/overlay.
 * @param obj The LVGL object to style.
 * @param theme The theme providing color palette.
 * 
 * Applies semi-transparent (40% opacity) styling with subtle borders
 * so background content remains visible underneath.
 */
void ui_style_card(lv_obj_t* obj, const ThemeManager& theme);

}  // namespace ui
