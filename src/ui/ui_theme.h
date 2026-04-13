#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace ui {

enum class ThemeId : uint8_t {
	Light = 0,
	Dark,
	FutureCustom1,
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

 private:
	void initStyles();
	void applyFallbackTheme(ThemeId id);
	void loadThemeFromJson(ThemeId id);
	void loadIconMap();
	String themePath(ThemeId id) const;
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
	ThemeId themeId_ = ThemeId::Dark;
	ThemePalette palette_{};
	ThemeTypography typography_{};
	ThemeSpacing spacing_{};
	ThemeAccentRules accentRules_{};
	WeatherIconEntry weatherIcons_[45]{};
	WeatherIconEntry defaultWeatherIcon_{};

	lv_style_t screenStyle_{};
	lv_style_t cardStyle_{};
	lv_style_t cardAltStyle_{};
	lv_style_t titleStyle_{};
	lv_style_t bodyStyle_{};
	lv_style_t captionStyle_{};
	lv_style_t chipStyle_{};
};

}  // namespace ui
