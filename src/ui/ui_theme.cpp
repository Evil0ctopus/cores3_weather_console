#include "ui_theme.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

namespace ui {

ThemeManager::ThemeManager() = default;

ThemeManager::~ThemeManager() {
	if (!initialized_) {
		return;
	}
	lv_style_reset(&screenStyle_);
	lv_style_reset(&cardStyle_);
	lv_style_reset(&cardAltStyle_);
	lv_style_reset(&titleStyle_);
	lv_style_reset(&bodyStyle_);
	lv_style_reset(&captionStyle_);
	lv_style_reset(&chipStyle_);
}

void ThemeManager::begin(ThemeId id) {
	if (!initialized_) {
		lv_style_init(&screenStyle_);
		lv_style_init(&cardStyle_);
		lv_style_init(&cardAltStyle_);
		lv_style_init(&titleStyle_);
		lv_style_init(&bodyStyle_);
		lv_style_init(&captionStyle_);
		lv_style_init(&chipStyle_);
		initialized_ = true;
	}
	loadIconMap();
	setTheme(id);
}

void ThemeManager::setTheme(ThemeId id) {
	themeId_ = id;
	applyFallbackTheme(id);
	loadThemeFromJson(id);
	initStyles();
}

ThemeId ThemeManager::themeId() const {
	return themeId_;
}

const ThemePalette& ThemeManager::palette() const {
	return palette_;
}

const ThemeTypography& ThemeManager::typography() const {
	return typography_;
}

const ThemeSpacing& ThemeManager::spacing() const {
	return spacing_;
}

const ThemeAccentRules& ThemeManager::accentRules() const {
	return accentRules_;
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
	return &screenStyle_;
}

lv_style_t* ThemeManager::cardStyle() {
	return &cardStyle_;
}

lv_style_t* ThemeManager::cardAltStyle() {
	return &cardAltStyle_;
}

lv_style_t* ThemeManager::titleStyle() {
	return &titleStyle_;
}

lv_style_t* ThemeManager::bodyStyle() {
	return &bodyStyle_;
}

lv_style_t* ThemeManager::captionStyle() {
	return &captionStyle_;
}

lv_style_t* ThemeManager::chipStyle() {
	return &chipStyle_;
}

void ThemeManager::initStyles() {
	lv_style_reset(&screenStyle_);
	lv_style_set_bg_opa(&screenStyle_, LV_OPA_COVER);
	lv_style_set_bg_color(&screenStyle_, palette_.bg);
	lv_style_set_bg_grad_color(&screenStyle_, palette_.surfaceAlt);
	lv_style_set_bg_grad_dir(&screenStyle_, LV_GRAD_DIR_VER);

	lv_style_reset(&cardStyle_);
	lv_style_set_radius(&cardStyle_, spacing_.cardRadius);
	lv_style_set_bg_opa(&cardStyle_, LV_OPA_COVER);
	lv_style_set_bg_color(&cardStyle_, palette_.surface);
	lv_style_set_border_width(&cardStyle_, 0);
	lv_style_set_pad_all(&cardStyle_, spacing_.cardPadding);
	lv_style_set_shadow_color(&cardStyle_, palette_.shadow);
	lv_style_set_shadow_opa(&cardStyle_, LV_OPA_20);
	lv_style_set_shadow_width(&cardStyle_, 20);
	lv_style_set_shadow_ofs_y(&cardStyle_, 6);

	lv_style_reset(&cardAltStyle_);
	lv_style_set_radius(&cardAltStyle_, spacing_.cardAltRadius);
	lv_style_set_bg_opa(&cardAltStyle_, LV_OPA_COVER);
	lv_style_set_bg_color(&cardAltStyle_, palette_.surfaceAlt);
	lv_style_set_pad_all(&cardAltStyle_, spacing_.cardAltPadding);
	lv_style_set_shadow_color(&cardAltStyle_, palette_.shadow);
	lv_style_set_shadow_opa(&cardAltStyle_, LV_OPA_10);
	lv_style_set_shadow_width(&cardAltStyle_, 12);
	lv_style_set_shadow_ofs_y(&cardAltStyle_, 3);

	lv_style_reset(&titleStyle_);
	lv_style_set_text_color(&titleStyle_, palette_.textPrimary);
	lv_style_set_text_font(&titleStyle_, &lv_font_montserrat_14);
	lv_style_set_text_letter_space(&titleStyle_, typography_.sectionLetterSpace);
	lv_style_set_text_line_space(&titleStyle_, typography_.titleLineSpace);

	lv_style_reset(&bodyStyle_);
	lv_style_set_text_color(&bodyStyle_, palette_.textPrimary);
	lv_style_set_text_font(&bodyStyle_, &lv_font_montserrat_14);
	lv_style_set_text_line_space(&bodyStyle_, typography_.bodyLineSpace);

	lv_style_reset(&captionStyle_);
	lv_style_set_text_color(&captionStyle_, palette_.textSecondary);
	lv_style_set_text_font(&captionStyle_, &lv_font_montserrat_14);
	lv_style_set_text_line_space(&captionStyle_, typography_.titleLineSpace);

	lv_style_reset(&chipStyle_);
	lv_style_set_radius(&chipStyle_, 999);
	lv_style_set_bg_opa(&chipStyle_, LV_OPA_COVER);
	lv_style_set_bg_color(&chipStyle_, palette_.accent);
	lv_style_set_text_color(&chipStyle_, accentRules_.chipText);
	lv_style_set_text_font(&chipStyle_, &lv_font_montserrat_14);
	lv_style_set_pad_left(&chipStyle_, spacing_.chipPadX);
	lv_style_set_pad_right(&chipStyle_, spacing_.chipPadX);
	lv_style_set_pad_top(&chipStyle_, spacing_.chipPadY);
	lv_style_set_pad_bottom(&chipStyle_, spacing_.chipPadY);
}

void ThemeManager::applyFallbackTheme(ThemeId id) {
	if (id == ThemeId::Light) {
		palette_.bg = lv_color_hex(0xEEF2F6);
		palette_.surface = lv_color_hex(0xFFFFFF);
		palette_.surfaceAlt = lv_color_hex(0xF6F8FB);
		palette_.textPrimary = lv_color_hex(0x111827);
		palette_.textSecondary = lv_color_hex(0x64748B);
		palette_.accent = lv_color_hex(0x1E88FF);
		palette_.shadow = lv_color_hex(0xA5B4C7);
		palette_.warning = lv_color_hex(0xE45B58);
		accentRules_.chipText = lv_color_hex(0xFFFFFF);
		accentRules_.iconTint = lv_color_hex(0x1E88FF);
		accentRules_.iconMutedTint = lv_color_hex(0x64748B);
	} else if (id == ThemeId::Dark) {
		palette_.bg = lv_color_hex(0x0D1118);
		palette_.surface = lv_color_hex(0x171E2A);
		palette_.surfaceAlt = lv_color_hex(0x1E2734);
		palette_.textPrimary = lv_color_hex(0xEEF2F8);
		palette_.textSecondary = lv_color_hex(0x9CA8BA);
		palette_.accent = lv_color_hex(0x5AB1FF);
		palette_.shadow = lv_color_hex(0x02050B);
		palette_.warning = lv_color_hex(0xFF7A7A);
		accentRules_.chipText = lv_color_hex(0x0D1118);
		accentRules_.iconTint = lv_color_hex(0x5AB1FF);
		accentRules_.iconMutedTint = lv_color_hex(0x9CA8BA);
	} else {
		palette_.bg = lv_color_hex(0x151922);
		palette_.surface = lv_color_hex(0x1D2430);
		palette_.surfaceAlt = lv_color_hex(0x273142);
		palette_.textPrimary = lv_color_hex(0xF3F5FA);
		palette_.textSecondary = lv_color_hex(0xA0ADC4);
		palette_.accent = lv_color_hex(0x5AB1FF);
		palette_.shadow = lv_color_hex(0x030509);
		palette_.warning = lv_color_hex(0xFF7A7A);
		accentRules_.chipText = lv_color_hex(0x151922);
		accentRules_.iconTint = lv_color_hex(0x5AB1FF);
		accentRules_.iconMutedTint = lv_color_hex(0xA0ADC4);
	}

	typography_ = ThemeTypography();
	spacing_ = ThemeSpacing();
}

void ThemeManager::loadThemeFromJson(ThemeId id) {
	if (!ensureFilesystemReady()) {
		return;
	}

	File file = SPIFFS.open(themePath(id).c_str(), FILE_READ);
	if (!file) {
		return;
	}

	JsonDocument doc;
	if (deserializeJson(doc, file) != DeserializationError::Ok) {
		file.close();
		return;
	}
	file.close();

	JsonObject colors = doc["colors"].as<JsonObject>();
	palette_.bg = parseColor(colors["bg"] | nullptr, palette_.bg);
	palette_.surface = parseColor(colors["surface"] | nullptr, palette_.surface);
	palette_.surfaceAlt = parseColor(colors["surfaceAlt"] | nullptr, palette_.surfaceAlt);
	palette_.textPrimary = parseColor(colors["textPrimary"] | nullptr, palette_.textPrimary);
	palette_.textSecondary = parseColor(colors["textSecondary"] | nullptr, palette_.textSecondary);
	palette_.accent = parseColor(colors["accent"] | nullptr, palette_.accent);
	palette_.shadow = parseColor(colors["shadow"] | nullptr, palette_.shadow);
	palette_.warning = parseColor(colors["warning"] | nullptr, palette_.warning);

	JsonObject typography = doc["typography"].as<JsonObject>();
	typography_.sectionLetterSpace = typography["sectionLetterSpace"] | typography_.sectionLetterSpace;
	typography_.titleLineSpace = typography["titleLineSpace"] | typography_.titleLineSpace;
	typography_.bodyLineSpace = typography["bodyLineSpace"] | typography_.bodyLineSpace;
	typography_.summaryLineSpace = typography["summaryLineSpace"] | typography_.summaryLineSpace;
	typography_.pageTitleZoom = typography["pageTitleZoom"] | typography_.pageTitleZoom;
	typography_.captionZoom = typography["captionZoom"] | typography_.captionZoom;
	typography_.heroValueZoom = typography["heroValueZoom"] | typography_.heroValueZoom;
	typography_.heroSummaryZoom = typography["heroSummaryZoom"] | typography_.heroSummaryZoom;
	typography_.weeklyRowZoom = typography["weeklyRowZoom"] | typography_.weeklyRowZoom;
	typography_.alertRowZoom = typography["alertRowZoom"] | typography_.alertRowZoom;
	typography_.bootTitleZoom = typography["bootTitleZoom"] | typography_.bootTitleZoom;

	JsonObject spacing = doc["spacing"].as<JsonObject>();
	spacing_.screenPadding = spacing["screenPadding"] | spacing_.screenPadding;
	spacing_.cardPadding = spacing["cardPadding"] | spacing_.cardPadding;
	spacing_.currentCardPadding = spacing["currentCardPadding"] | spacing_.currentCardPadding;
	spacing_.cardAltPadding = spacing["cardAltPadding"] | spacing_.cardAltPadding;
	spacing_.cardRadius = spacing["cardRadius"] | spacing_.cardRadius;
	spacing_.cardAltRadius = spacing["cardAltRadius"] | spacing_.cardAltRadius;
	spacing_.listRowGap = spacing["listRowGap"] | spacing_.listRowGap;
	spacing_.weeklyListTop = spacing["weeklyListTop"] | spacing_.weeklyListTop;
	spacing_.alertsListTop = spacing["alertsListTop"] | spacing_.alertsListTop;
	spacing_.imageRadius = spacing["imageRadius"] | spacing_.imageRadius;
	spacing_.chipPadX = spacing["chipPadX"] | spacing_.chipPadX;
	spacing_.chipPadY = spacing["chipPadY"] | spacing_.chipPadY;
	spacing_.iconChipPadX = spacing["iconChipPadX"] | spacing_.iconChipPadX;
	spacing_.iconChipPadY = spacing["iconChipPadY"] | spacing_.iconChipPadY;

	JsonObject accent = doc["accent"].as<JsonObject>();
	accentRules_.chipText = parseColor(accent["chipText"] | nullptr, accentRules_.chipText);
	accentRules_.iconTint = parseColor(accent["iconTint"] | nullptr, accentRules_.iconTint);
	accentRules_.iconMutedTint = parseColor(accent["iconMutedTint"] | nullptr, accentRules_.iconMutedTint);
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

String ThemeManager::themePath(ThemeId id) const {
	if (id == ThemeId::Light) {
		return String("/themes/light.json");
	}
	if (id == ThemeId::Dark) {
		return String("/themes/dark.json");
	}
	return String("/themes/future1.json");
}

bool ThemeManager::ensureFilesystemReady() {
	if (filesystemReady_) {
		return true;
	}
	filesystemReady_ = SPIFFS.begin(false);
	return filesystemReady_;
}

lv_color_t ThemeManager::parseColor(const char* value, lv_color_t fallback) const {
	if (value == nullptr || value[0] == '\0') {
		return fallback;
	}
	const char* start = value[0] == '#' ? value + 1 : value;
	char* endPtr = nullptr;
	const unsigned long parsed = strtoul(start, &endPtr, 16);
	if (endPtr == start) {
		return fallback;
	}
	return lv_color_hex(static_cast<uint32_t>(parsed));
}

lv_color_t ThemeManager::colorForRole(const String& role) const {
	if (role == "accent" || role == "iconTint") {
		return accentRules_.iconTint;
	}
	if (role == "warning") {
		return palette_.warning;
	}
	if (role == "textSecondary") {
		return palette_.textSecondary;
	}
	if (role == "textPrimary") {
		return palette_.textPrimary;
	}
	return accentRules_.iconMutedTint;
}

}  // namespace ui
