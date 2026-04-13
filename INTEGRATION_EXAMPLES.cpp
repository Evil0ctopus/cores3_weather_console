// INTEGRATION EXAMPLES: Using Icon Manager in UI Pages

#pragma once

// This file shows before/after examples for integrating ui_icons into existing UI pages

// ============================================================================
// EXAMPLE 1: Weather Icon in CurrentPage (ui_current.cpp)
// ============================================================================

// BEFORE: Using WeatherIconView (canvas-based drawing)
/*
void CurrentPage::begin(lv_obj_t* parent, ThemeManager& theme) {
    root_ = lv_obj_create(parent);
    theme_ = &theme;
    
    // Icon drawn as canvas with primitives
    iconView_.begin(root_, 34);
    
    // Update with condition code
    WeatherData data{};
    data.conditionCode = 800;  // Clear sky
    update(data, {});
}

void CurrentPage::update(const WeatherData& data, const SystemInfo& info) {
    // Draw icon based on condition code
    iconView_.setIcon(theme_, getTokenForCode(data.conditionCode), ...);
}
*/

// AFTER: Using PNG icons via ui_icon_create
/*
#include "ui_icons.h"

void CurrentPage::begin(lv_obj_t* parent, ThemeManager& theme) {
    root_ = lv_obj_create(parent);
    theme_ = &theme;
    
    // Create container for icon
    lv_obj_t* iconContainer = lv_obj_create(root_);
    lv_obj_set_size(iconContainer, 80, 80);
    lv_obj_set_align(iconContainer, LV_ALIGN_TOP_MID);
    
    // Create PNG icon from ComfyUI
    currentIcon_ = ui::ui_icon_create(
        iconContainer,
        ui::IconId::ICON_CLEAR_DAY,  // Will update in update()
        theme.currentTheme()
    );
    ui::ui_icon_set_size(currentIcon_, 64, 64);
    lv_obj_set_align(currentIcon_, LV_ALIGN_CENTER);
}

void CurrentPage::update(const WeatherData& data, const SystemInfo& info) {
    // Map condition code to icon
    ui::IconId iconId = mapConditionToIcon(data.conditionCode, isDaylight);
    
    // Delete old icon and create new one
    if (currentIcon_ != nullptr) {
        ui::ui_icon_delete(currentIcon_);
    }
    
    lv_obj_t* iconContainer = root_;  // or store container reference
    currentIcon_ = ui::ui_icon_create(
        iconContainer,
        iconId,
        theme_->currentTheme()
    );
    ui::ui_icon_set_size(currentIcon_, 64, 64);
}

// Helper function to map OpenWeatherMap condition codes to icons
ui::IconId CurrentPage::mapConditionToIcon(int code, bool isDaylight) {
    // OpenWeatherMap condition codes
    if (code == 800) {  // Clear sky
        return isDaylight ? ui::IconId::ICON_CLEAR_DAY : ui::IconId::ICON_CLEAR_NIGHT;
    }
    if (code >= 801 && code <= 804) {  // Clouds (partly/mostly/overcast)
        return code == 801 ? ui::IconId::ICON_PARTLY_CLOUDY : ui::IconId::ICON_CLOUDY;
    }
    if (code >= 500 && code <= 531) {  // Rain
        if (code >= 200 && code <= 232) return ui::IconId::ICON_STORM;  // Thunderstorm
        return ui::IconId::ICON_RAIN;
    }
    if (code >= 600 && code <= 622) {  // Snow
        return ui::IconId::ICON_SNOW;
    }
    if (code >= 701 && code <= 781) {  // Fog, mist, dust
        return ui::IconId::ICON_FOG;
    }
    if (code == 905 || code == 906) {  // Windy
        return ui::IconId::ICON_WIND;
    }
    return isDaylight ? ui::IconId::ICON_CLEAR_DAY : ui::IconId::ICON_CLEAR_NIGHT;
}
*/

// Changes in class header (ui_current.h):
/*
class CurrentPage {
 private:
    lv_obj_t* currentIcon_ = nullptr;  // Changed from: WeatherIconView iconView_{}
    ui::IconId lastIconId_ = ui::IconId::ICON_CLEAR_DAY;
};
*/

// ============================================================================
// EXAMPLE 2: Navigation Icons in UI Root (ui_root.cpp)
// ============================================================================

// BEFORE: No icons on navigation tabs
/*
void createTabButtons(lv_obj_t* tabview) {
    lv_obj_t* homeBtn = lv_btn_create(tabview);
    lv_obj_t* homeLabel = lv_label_create(homeBtn);
    lv_label_set_text(homeLabel, "Home");
}
*/

// AFTER: Add PNG icons to nav buttons
/*
#include "ui_icons.h"

void UiRoot::createTabButtons(lv_obj_t* tabview) {
    // Home tab
    lv_obj_t* homeBtn = lv_btn_create(tabview);
    lv_obj_set_flex_flow(homeBtn, LV_FLEX_FLOW_COLUMN);
    
    lv_obj_t* homeIcon = ui::ui_icon_create(
        homeBtn,
        ui::IconId::ICON_NAV_HOME,
        currentTheme_
    );
    ui::ui_icon_set_size(homeIcon, 24, 24);
    
    lv_obj_t* homeLabel = lv_label_create(homeBtn);
    lv_label_set_text(homeLabel, "Home");
    
    // Radar tab
    lv_obj_t* radarBtn = lv_btn_create(tabview);
    lv_obj_set_flex_flow(radarBtn, LV_FLEX_FLOW_COLUMN);
    
    lv_obj_t* radarIcon = ui::ui_icon_create(
        radarBtn,
        ui::IconId::ICON_NAV_RADAR,
        currentTheme_
    );
    ui::ui_icon_set_size(radarIcon, 24, 24);
    
    lv_obj_t* radarLabel = lv_label_create(radarBtn);
    lv_label_set_text(radarLabel, "Radar");
    
    // Alerts tab
    lv_obj_t* alertsBtn = lv_btn_create(tabview);
    lv_obj_set_flex_flow(alertsBtn, LV_FLEX_FLOW_COLUMN);
    
    lv_obj_t* alertsIcon = ui::ui_icon_create(
        alertsBtn,
        ui::IconId::ICON_NAV_ALERTS,
        currentTheme_
    );
    ui::ui_icon_set_size(alertsIcon, 24, 24);
    
    lv_obj_t* alertsLabel = lv_label_create(alertsBtn);
    lv_label_set_text(alertsLabel, "Alerts");
    
    // Settings tab
    lv_obj_t* settingsBtn = lv_btn_create(tabview);
    lv_obj_set_flex_flow(settingsBtn, LV_FLEX_FLOW_COLUMN);
    
    lv_obj_t* settingsIcon = ui::ui_icon_create(
        settingsBtn,
        ui::IconId::ICON_NAV_SETTINGS,
        currentTheme_
    );
    ui::ui_icon_set_size(settingsIcon, 24, 24);
    
    lv_obj_t* settingsLabel = lv_label_create(settingsBtn);
    lv_label_set_text(settingsLabel, "Settings");
}
*/

// ============================================================================
// EXAMPLE 3: Alert Icons in AlertsPage (ui_alerts.cpp)
// ============================================================================

// BEFORE: Text-only alert list
/*
void AlertsPage::addAlertRow(const Alert& alert) {
    lv_obj_t* row = lv_obj_create(container_);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    
    lv_obj_t* titleLabel = lv_label_create(row);
    lv_label_set_text(titleLabel, alert.title.c_str());
    
    lv_obj_t* descLabel = lv_label_create(row);
    lv_label_set_text(descLabel, alert.description.c_str());
}
*/

// AFTER: Alert icons with severity indicators
/*
#include "ui_icons.h"

void AlertsPage::addAlertRow(const Alert& alert) {
    lv_obj_t* row = lv_obj_create(container_);
    lv_obj_set_size(row, LV_PCT(100), 48);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_flex_align_t pad = LV_FLEX_ALIGN_START;
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, pad);
    
    // Alert icon (based on severity/type)
    ui::IconId alertIcon = mapAlertToIcon(alert.severity);
    lv_obj_t* icon = ui::ui_icon_create(row, alertIcon, currentTheme_);
    ui::ui_icon_set_size(icon, 24, 24);
    lv_obj_set_style_margin_right(icon, 8, LV_PART_MAIN);
    
    // Text content
    lv_obj_t* content = lv_obj_create(row);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    
    lv_obj_t* titleLabel = lv_label_create(content);
    lv_label_set_text(titleLabel, alert.title.c_str());
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    
    lv_obj_t* descLabel = lv_label_create(content);
    lv_label_set_text(descLabel, alert.description.c_str());
    lv_obj_set_style_text_font(descLabel, &lv_font_montserrat_10, LV_PART_MAIN);
}

ui::IconId AlertsPage::mapAlertToIcon(AlertSeverity severity) {
    // Could use ICON_STORM for severe, ICON_WIND for moderate, etc.
    // For now, use a single warning icon or map to weather condition
    return ui::IconId::ICON_STORM;  // Or whatever represents alerts
}
*/

// ============================================================================
// EXAMPLE 4: System Icons in SystemPage (ui_system.cpp)
// ============================================================================

// BEFORE: No visual system status
/*
void SystemPage::update(const SystemInfo& info) {
    wifiLabel_.text = "WiFi: " + info.wifiConnected ? "Connected" : "Disconnected";
    batteryLabel_.text = "Battery: " + String(info.batteryPercent) + "%";
}
*/

// AFTER: System icons with status
/*
#include "ui_icons.h"

void SystemPage::begin(lv_obj_t* parent, ThemeManager& theme) {
    root_ = lv_obj_create(parent);
    theme_ = &theme;
    
    // WiFi status row
    lv_obj_t* wifiRow = lv_obj_create(root_);
    lv_obj_set_layout(wifiRow, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wifiRow, LV_FLEX_FLOW_ROW);
    
    wifiIcon_ = ui::ui_icon_create(wifiRow, ui::IconId::ICON_NAV_SETTINGS, theme_->currentTheme());
    ui::ui_icon_set_size(wifiIcon_, 20, 20);
    
    wifiLabel_ = lv_label_create(wifiRow);
    lv_label_set_text(wifiLabel_, "WiFi: Disconnected");
}

void SystemPage::update(const SystemInfo& info) {
    // Update WiFi status text
    lv_label_set_text(wifiLabel_, 
        info.wifiConnected ? "WiFi: Connected" : "WiFi: Disconnected");
    
    // Could also update icon color/opacity based on status
    if (!info.wifiConnected) {
        lv_obj_set_style_opa(wifiIcon_, LV_OPA_50, LV_PART_MAIN);
    } else {
        lv_obj_set_style_opa(wifiIcon_, LV_OPA_COVER, LV_PART_MAIN);
    }
}
*/

// ============================================================================
// COMMON PATTERN: Update Icon on Theme Change
// ============================================================================

/*
void SomePage::applyTheme(ThemeManager& theme) {
    // If you need to update icons when theme changes:
    
    if (currentIcon_ != nullptr) {
        ui::ui_icon_delete(currentIcon_);
        currentIcon_ = nullptr;
    }
    
    currentIcon_ = ui::ui_icon_create(
        iconContainer_,
        lastIconId_,
        theme.currentTheme()
    );
    ui::ui_icon_set_size(currentIcon_, 64, 64);
}
*/

// ============================================================================
// KEY PATTERNS TO REMEMBER
// ============================================================================

/*
1. Always check parent is not null before calling ui_icon_create()
2. If icon needs to change, delete old and create new
3. Use ui_icon_set_size() right after creation for consistent sizing
4. Use proper alignment (lv_obj_set_align) for positioning
5. Apply padding/margins using lv_obj_set_style functions
6. For frequently updated icons, consider caching the lv_obj_t* pointer
7. Watch Serial output for [ICON] messages during development
8. Test with SPIFFS full and empty to verify fallback behavior
9. Test all 10 themes to verify fallback colors look good
10. Monitor RAM usage if creating many icons
*/
