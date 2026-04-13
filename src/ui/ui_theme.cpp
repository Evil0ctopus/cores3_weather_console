#include "ui_theme.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

namespace ui {
namespace {

ThemeColors gThemeColors = {};
ThemePalette gPalette = {};
ThemeTypography gTypography = {};
ThemeSpacing gSpacing = {};
ThemeAccentRules gAccentRules = {};

bool gStylesInitialized = false;
lv_style_t gScreenStyle;
lv_style_t gTabStyle;
lv_style_t gCardStyle;
lv_style_t gCardAltStyle;
lv_style_t gLabelStyle;
lv_style_t gTitleStyle;
lv_style_t gBodyStyle;
lv_style_t gCaptionStyle;
lv_style_t gChipStyle;

void ensureStylesInitialized() {
	if (gStylesInitialized) {
		return;
	}
	lv_style_init(&gScreenStyle);
	lv_style_init(&gTabStyle);
	lv_style_init(&gCardStyle);
	lv_style_init(&gCardAltStyle);
	lv_style_init(&gLabelStyle);
	lv_style_init(&gTitleStyle);
	lv_style_init(&gBodyStyle);
	lv_style_init(&gCaptionStyle);
	lv_style_init(&gChipStyle);
	gStylesInitialized = true;
}

lv_color_t hexColor(uint32_t value) {
	return lv_color_hex(value);
}

lv_color_t mixColor(uint32_t left, uint32_t right, uint8_t mix) {
	return lv_color_mix(hexColor(left), hexColor(right), mix);
}

bool useLightText(uint32_t color) {
	const uint32_t red = (color >> 16) & 0xFFU;
	const uint32_t green = (color >> 8) & 0xFFU;
	const uint32_t blue = color & 0xFFU;
	const uint32_t luma = red * 299U + green * 587U + blue * 114U;
	return luma < 140000U;
}

void rebuildThemeState(ThemeId id) {
	gThemeColors = get_theme_colors(id);
	gTypography = ThemeTypography();
	gSpacing = ThemeSpacing();

	gPalette.bg = hexColor(gThemeColors.bg_main);
	gPalette.surface = hexColor(gThemeColors.bg_card);
	gPalette.surfaceAlt = mixColor(gThemeColors.bg_card, gThemeColors.bg_tab, LV_OPA_50);
	gPalette.textPrimary = hexColor(gThemeColors.text_primary);
	gPalette.textSecondary = hexColor(gThemeColors.text_secondary);
	gPalette.accent = hexColor(gThemeColors.accent_primary);
	gPalette.shadow = hexColor(gThemeColors.border_soft);
	gPalette.warning = hexColor(gThemeColors.accent_warning);

	gAccentRules.chipText = useLightText(gThemeColors.accent_primary) ? lv_color_white() : lv_color_black();
	gAccentRules.iconTint = hexColor(gThemeColors.accent_secondary);
	gAccentRules.iconMutedTint = hexColor(gThemeColors.text_secondary);
}

void rebuildStyles() {
	ensureStylesInitialized();

	lv_style_reset(&gScreenStyle);
	lv_style_set_bg_opa(&gScreenStyle, LV_OPA_COVER);
	lv_style_set_bg_color(&gScreenStyle, hexColor(gThemeColors.bg_main));
	lv_style_set_text_color(&gScreenStyle, hexColor(gThemeColors.text_primary));

	lv_style_reset(&gTabStyle);
	lv_style_set_bg_opa(&gTabStyle, LV_OPA_TRANSP);
	lv_style_set_bg_color(&gTabStyle, hexColor(gThemeColors.bg_tab));
	lv_style_set_border_width(&gTabStyle, 0);
	lv_style_set_border_color(&gTabStyle, hexColor(gThemeColors.border_soft));
	lv_style_set_radius(&gTabStyle, 0);

	lv_style_reset(&gCardStyle);
	lv_style_set_radius(&gCardStyle, gSpacing.cardRadius);
	lv_style_set_bg_opa(&gCardStyle, LV_OPA_COVER);
	lv_style_set_bg_color(&gCardStyle, hexColor(gThemeColors.bg_card));
	lv_style_set_border_width(&gCardStyle, 1);
	lv_style_set_border_color(&gCardStyle, hexColor(gThemeColors.border_soft));
	lv_style_set_pad_all(&gCardStyle, gSpacing.cardPadding);
	lv_style_set_shadow_opa(&gCardStyle, LV_OPA_TRANSP);
	lv_style_set_shadow_width(&gCardStyle, 0);
	lv_style_set_shadow_ofs_y(&gCardStyle, 0);

	lv_style_reset(&gCardAltStyle);
	lv_style_set_radius(&gCardAltStyle, gSpacing.cardAltRadius);
	lv_style_set_bg_opa(&gCardAltStyle, LV_OPA_COVER);
	lv_style_set_bg_color(&gCardAltStyle, mixColor(gThemeColors.bg_card, gThemeColors.bg_tab, LV_OPA_60));
	lv_style_set_border_width(&gCardAltStyle, 1);
	lv_style_set_border_color(&gCardAltStyle, hexColor(gThemeColors.border_soft));
	lv_style_set_pad_all(&gCardAltStyle, gSpacing.cardAltPadding);
	lv_style_set_shadow_opa(&gCardAltStyle, LV_OPA_TRANSP);
	lv_style_set_shadow_width(&gCardAltStyle, 0);
	lv_style_set_shadow_ofs_y(&gCardAltStyle, 0);

	lv_style_reset(&gLabelStyle);
	lv_style_set_text_color(&gLabelStyle, hexColor(gThemeColors.text_primary));
	lv_style_set_text_font(&gLabelStyle, &lv_font_montserrat_14);

	lv_style_reset(&gTitleStyle);
	lv_style_set_text_color(&gTitleStyle, hexColor(gThemeColors.text_primary));
	lv_style_set_text_font(&gTitleStyle, &lv_font_montserrat_14);
	lv_style_set_text_letter_space(&gTitleStyle, gTypography.sectionLetterSpace);
	lv_style_set_text_line_space(&gTitleStyle, gTypography.titleLineSpace);

	lv_style_reset(&gBodyStyle);
	lv_style_set_text_color(&gBodyStyle, hexColor(gThemeColors.text_primary));
	lv_style_set_text_font(&gBodyStyle, &lv_font_montserrat_14);
	lv_style_set_text_line_space(&gBodyStyle, gTypography.bodyLineSpace);

	lv_style_reset(&gCaptionStyle);
	lv_style_set_text_color(&gCaptionStyle, hexColor(gThemeColors.text_secondary));
	lv_style_set_text_font(&gCaptionStyle, &lv_font_montserrat_14);
	lv_style_set_text_line_space(&gCaptionStyle, gTypography.titleLineSpace);

	lv_style_reset(&gChipStyle);
	lv_style_set_radius(&gChipStyle, 999);
	lv_style_set_bg_opa(&gChipStyle, LV_OPA_COVER);
	lv_style_set_bg_color(&gChipStyle, hexColor(gThemeColors.accent_primary));
	lv_style_set_text_color(&gChipStyle, gAccentRules.chipText);
	lv_style_set_text_font(&gChipStyle, &lv_font_montserrat_14);
	lv_style_set_pad_left(&gChipStyle, gSpacing.chipPadX);
	lv_style_set_pad_right(&gChipStyle, gSpacing.chipPadX);
	lv_style_set_pad_top(&gChipStyle, gSpacing.chipPadY);
	lv_style_set_pad_bottom(&gChipStyle, gSpacing.chipPadY);
}

}  // namespace

ThemeColors get_theme_colors(ThemeId id) {
	switch (id) {
		case ThemeId::PIXEL_STORM:
			return ThemeColors{0x050816, 0x0b1020, 0x050816, 0xffb347, 0x4fd1ff, 0xff5c5c, 0xf5f7ff, 0xa3b0d0, 0x1a2238};
		case ThemeId::DESERT_CALM:
			return ThemeColors{0xf7f1e8, 0xf0e3d2, 0xf7f1e8, 0xd47b4a, 0xb89b6d, 0xc0392b, 0x2f2418, 0x7a6a55, 0xe0d2c0};
		case ThemeId::FUTURE_PULSE:
			return ThemeColors{0x02010a, 0x07071a, 0x02010a, 0xff2fbf, 0x00f0ff, 0xffb347, 0xf8f9ff, 0x9a9ccf, 0x191933};
		case ThemeId::MIDNIGHT_RADAR:
			return ThemeColors{0x06111b, 0x0d1c28, 0x08131e, 0x53e6ff, 0x7dff9b, 0xff7b54, 0xeef8ff, 0x93acbf, 0x1a3445};
		case ThemeId::DAYBREAK_CLEAR:
			return ThemeColors{0xeef6ff, 0xffffff, 0xe8f1fb, 0xff9f45, 0x4aa8ff, 0xd94c4c, 0x1d2a36, 0x60758a, 0xcbd9e7};
		case ThemeId::STORMGLASS:
			return ThemeColors{0x0a1720, 0x122532, 0x0d1d27, 0x79c7ff, 0xb2f7ef, 0xff8a5b, 0xf2fbff, 0x92aebc, 0x24404d};
		case ThemeId::AURORA_LINE:
			return ThemeColors{0x04110f, 0x0a1b19, 0x061412, 0x6df7c1, 0x7caeff, 0xffad5a, 0xf3fff9, 0x92b6ae, 0x18342f};
		case ThemeId::OCEAN_FRONT:
			return ThemeColors{0x061623, 0x0d2435, 0x081b2b, 0x4fc3f7, 0x7ef0d1, 0xff9860, 0xf1fbff, 0x8fb2c5, 0x1d3d53};
		case ThemeId::MONO_WIREFRAME:
			return ThemeColors{0x0f1012, 0x16181b, 0x111315, 0xd7d9de, 0x8f98a3, 0xff6b6b, 0xf2f4f8, 0xa0a6af, 0x2d333b};
		case ThemeId::INFRARED_SCAN:
			return ThemeColors{0x120505, 0x1d0909, 0x150606, 0xff6b35, 0xffc857, 0xff4040, 0xfff4ed, 0xd8a892, 0x3a1616};
	}
	return get_theme_colors(ThemeId::PIXEL_STORM);
}

const char* theme_id_to_name(ThemeId id) {
	switch (id) {
		case ThemeId::PIXEL_STORM:
			return "Pixel Storm";
		case ThemeId::DESERT_CALM:
			return "Desert Calm";
		case ThemeId::FUTURE_PULSE:
			return "Future Pulse";
		case ThemeId::MIDNIGHT_RADAR:
			return "Midnight Radar";
		case ThemeId::DAYBREAK_CLEAR:
			return "Daybreak Clear";
		case ThemeId::STORMGLASS:
			return "Stormglass";
		case ThemeId::AURORA_LINE:
			return "Aurora Line";
		case ThemeId::OCEAN_FRONT:
			return "Ocean Front";
		case ThemeId::MONO_WIREFRAME:
			return "Mono Wireframe";
		case ThemeId::INFRARED_SCAN:
			return "Infrared Scan";
	}
	return "Pixel Storm";
}

const char* theme_id_to_storage_key(ThemeId id) {
	switch (id) {
		case ThemeId::PIXEL_STORM:
			return "pixel_storm";
		case ThemeId::DESERT_CALM:
			return "desert_calm";
		case ThemeId::FUTURE_PULSE:
			return "future_pulse";
		case ThemeId::MIDNIGHT_RADAR:
			return "midnight_radar";
		case ThemeId::DAYBREAK_CLEAR:
			return "daybreak_clear";
		case ThemeId::STORMGLASS:
			return "stormglass";
		case ThemeId::AURORA_LINE:
			return "aurora_line";
		case ThemeId::OCEAN_FRONT:
			return "ocean_front";
		case ThemeId::MONO_WIREFRAME:
			return "mono_wireframe";
		case ThemeId::INFRARED_SCAN:
			return "infrared_scan";
	}
	return "pixel_storm";
}

bool parse_theme_id(const String& value, ThemeId& out) {
	String normalizedValue = value;
	normalizedValue.trim();
	normalizedValue.toLowerCase();
	for (uint8_t index = 0; index < theme_count(); ++index) {
		const ThemeId candidate = theme_id_from_index(index);
		String storageKey = String(theme_id_to_storage_key(candidate));
		storageKey.toLowerCase();
		if (normalizedValue == storageKey) {
			out = candidate;
			return true;
		}

		String displayName = String(theme_id_to_name(candidate));
		displayName.toLowerCase();
		displayName.replace(" ", "_");
		if (normalizedValue == displayName) {
			out = candidate;
			return true;
		}
	}
	return false;
}

uint8_t theme_count() {
	return 10;
}

ThemeId theme_id_from_index(uint8_t index) {
	if (index >= theme_count()) {
		return ThemeId::PIXEL_STORM;
	}
	return static_cast<ThemeId>(index);
}

void ui_apply_theme_lvgl(ThemeId id) {
	rebuildThemeState(id);
	rebuildStyles();
	lv_obj_report_style_change(nullptr);
}

void ui_theme_apply_to_root(lv_obj_t* root, ThemeId id) {
	ui_apply_theme_lvgl(id);
	if (root == nullptr) {
		return;
	}
	lv_obj_add_style(root, &gScreenStyle, LV_PART_MAIN);
	lv_obj_set_style_bg_color(root, hexColor(gThemeColors.bg_main), LV_PART_MAIN);
	lv_obj_set_style_text_color(root, hexColor(gThemeColors.text_primary), LV_PART_MAIN);
	lv_obj_invalidate(root);
}

void ui_make_container_transparent(lv_obj_t* obj) {
	if (obj == nullptr) {
		Serial.println("[UI] ERROR: ui_make_container_transparent received null object.");
		return;
	}
	lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_outline_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
	Serial.println("[UI] Transparency applied");
}

void ui_make_transparent(lv_obj_t* obj) {
	if (obj == nullptr) {
		Serial.println("[UI] ERROR: ui_make_transparent received null object.");
		return;
	}
	lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_outline_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
	Serial.println("[UI] Transparency applied");
}

void ui_style_card(lv_obj_t* obj, const ThemeManager& theme) {
	if (obj == nullptr) {
		Serial.println("[UI] ERROR: ui_style_card received null object.");
		return;
	}
	
	// Get theme colors
	const ThemeColors colors = get_theme_colors(theme.themeId());
	
	// Semi-transparent background overlay (40% opacity)
	lv_obj_set_style_bg_opa(obj, LV_OPA_40, LV_PART_MAIN);
	lv_obj_set_style_bg_color(obj, lv_color_hex(colors.bg_card), LV_PART_MAIN);
	
	// Rounded corners
	lv_obj_set_style_radius(obj, 12, LV_PART_MAIN);
	
	// Subtle border overlay (20% opacity)
	lv_obj_set_style_border_opa(obj, LV_OPA_20, LV_PART_MAIN);
	lv_obj_set_style_border_color(obj, lv_color_hex(colors.border_soft), LV_PART_MAIN);
	lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
	
	Serial.println("[UI] Styled card overlay");
}

ThemeManager::ThemeManager() = default;

ThemeManager::~ThemeManager() = default;

void ThemeManager::begin(ThemeId id) {
	initialized_ = true;
	loadIconMap();
	setTheme(id);
}

void ThemeManager::setTheme(ThemeId id) {
	themeId_ = id;
	ui_apply_theme_lvgl(id);
	Serial.printf("[THEME] Theme set id=%d name=%s\n", static_cast<int>(id), theme_id_to_name(id));
}

ThemeId ThemeManager::themeId() const {
	return themeId_;
}

const ThemePalette& ThemeManager::palette() const {
	return gPalette;
}

const ThemeTypography& ThemeManager::typography() const {
	return gTypography;
}

const ThemeSpacing& ThemeManager::spacing() const {
	return gSpacing;
}

const ThemeAccentRules& ThemeManager::accentRules() const {
	return gAccentRules;
}

String ThemeManager::weatherIconToken(int conditionCode, bool isDaylight) const {
	(void)isDaylight;
	if (conditionCode >= 0 && conditionCode < static_cast<int>(sizeof(weatherIcons_) / sizeof(weatherIcons_[0])) &&
		weatherIcons_[conditionCode].token.length() > 0) {
		return weatherIcons_[conditionCode].token;
	}
	return defaultWeatherIcon_.token.length() > 0 ? defaultWeatherIcon_.token : String("cloud");
}

lv_color_t ThemeManager::weatherIconColor(int conditionCode, bool isDaylight) const {
	(void)isDaylight;
	if (conditionCode >= 0 && conditionCode < static_cast<int>(sizeof(weatherIcons_) / sizeof(weatherIcons_[0])) &&
		weatherIcons_[conditionCode].role.length() > 0) {
		return colorForRole(weatherIcons_[conditionCode].role);
	}
	return colorForRole(defaultWeatherIcon_.role.length() > 0 ? defaultWeatherIcon_.role : String("iconMutedTint"));
}

lv_style_t* ThemeManager::screenStyle() {
	ensureStylesInitialized();
	return &gScreenStyle;
}

lv_style_t* ThemeManager::cardStyle() {
	ensureStylesInitialized();
	return &gCardStyle;
}

lv_style_t* ThemeManager::cardAltStyle() {
	ensureStylesInitialized();
	return &gCardAltStyle;
}

lv_style_t* ThemeManager::titleStyle() {
	ensureStylesInitialized();
	return &gTitleStyle;
}

lv_style_t* ThemeManager::bodyStyle() {
	ensureStylesInitialized();
	return &gBodyStyle;
}

lv_style_t* ThemeManager::captionStyle() {
	ensureStylesInitialized();
	return &gCaptionStyle;
}

lv_style_t* ThemeManager::chipStyle() {
	ensureStylesInitialized();
	return &gChipStyle;
}

lv_style_t* ThemeManager::tabStyle() {
	ensureStylesInitialized();
	return &gTabStyle;
}

lv_style_t* ThemeManager::defaultLabelStyle() {
	ensureStylesInitialized();
	return &gLabelStyle;
}

void ThemeManager::loadIconMap() {
	if (iconMapLoaded_) {
		return;
	}
	defaultWeatherIcon_.token = "cloud";
	defaultWeatherIcon_.role = "iconMutedTint";
	if (!ensureFilesystemReady()) {
		iconMapLoaded_ = true;
		return;
	}

	File file = SPIFFS.open("/themes/weather_icons.json", FILE_READ);
	if (!file) {
		iconMapLoaded_ = true;
		return;
	}

	JsonDocument doc;
	if (deserializeJson(doc, file) != DeserializationError::Ok) {
		file.close();
		iconMapLoaded_ = true;
		return;
	}
	file.close();

	JsonObject defaultIcon = doc["default"].as<JsonObject>();
	if (!defaultIcon.isNull()) {
		defaultWeatherIcon_.token = String(static_cast<const char*>(defaultIcon["token"] | "cloud"));
		defaultWeatherIcon_.role = String(static_cast<const char*>(defaultIcon["role"] | "iconMutedTint"));
	}

	JsonObject codes = doc["codes"].as<JsonObject>();
	for (JsonPair kv : codes) {
		const int index = atoi(kv.key().c_str());
		if (index < 0 || index >= static_cast<int>(sizeof(weatherIcons_) / sizeof(weatherIcons_[0]))) {
			continue;
		}
		JsonObject icon = kv.value().as<JsonObject>();
		weatherIcons_[index].token = String(static_cast<const char*>(icon["token"] | defaultWeatherIcon_.token.c_str()));
		weatherIcons_[index].role = String(static_cast<const char*>(icon["role"] | defaultWeatherIcon_.role.c_str()));
	}
	iconMapLoaded_ = true;
}

bool ThemeManager::ensureFilesystemReady() {
	if (filesystemReady_) {
		return true;
	}
	filesystemReady_ = SPIFFS.begin(false);
	return filesystemReady_;
}

lv_color_t ThemeManager::parseColor(const char* value, lv_color_t fallback) const {
	(void)value;
	return fallback;
}

lv_color_t ThemeManager::colorForRole(const String& role) const {
	if (role == "accent" || role == "iconTint") {
		return gAccentRules.iconTint;
	}
	if (role == "warning") {
		return gPalette.warning;
	}
	if (role == "textSecondary") {
		return gPalette.textSecondary;
	}
	if (role == "textPrimary") {
		return gPalette.textPrimary;
	}
	return gAccentRules.iconMutedTint;
}

}  // namespace ui
