// QUICK REFERENCE: Icon Manager API

#include "ui/ui_icons.h"

using namespace ui;

// ============================================================================
// ICON IDs
// ============================================================================

IconId::ICON_CLEAR_DAY          // Weather: clear daytime
IconId::ICON_CLEAR_NIGHT        // Weather: clear nighttime
IconId::ICON_PARTLY_CLOUDY      // Weather: partly cloudy
IconId::ICON_CLOUDY             // Weather: overcast
IconId::ICON_RAIN               // Weather: rain
IconId::ICON_STORM              // Weather: thunderstorm
IconId::ICON_SNOW               // Weather: snow
IconId::ICON_FOG                // Weather: fog
IconId::ICON_WIND               // Weather: high wind

IconId::ICON_NAV_HOME           // Nav: home/dashboard
IconId::ICON_NAV_RADAR          // Nav: radar/weather map
IconId::ICON_NAV_ALERTS         // Nav: alerts/warnings
IconId::ICON_NAV_SETTINGS       // Nav: settings/configuration
IconId::ICON_NAV_SYSTEM         // Nav: system info

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

// Get SPIFFS path for an icon
const char* path = ui_icon_get_path(IconId::ICON_RAIN);
// Returns: "/icons/weather_rain.png"

// Create icon (returns lv_img object or fallback circle)
lv_obj_t* icon = ui_icon_create(
    parent_object,           // Parent LVGL object
    IconId::ICON_RAIN,       // Which icon
    ThemeId::PIXEL_STORM     // Current theme
);

// Resize icon after creation
ui_icon_set_size(icon, 32, 32);  // 32x32 pixels

// Delete icon
ui_icon_delete(icon);

// ============================================================================
// COMMON PATTERNS
// ============================================================================

// Pattern 1: Simple Weather Icon
{
    lv_obj_t* icon = ui_icon_create(container, IconId::ICON_RAIN, theme);
    ui_icon_set_size(icon, 64, 64);
    lv_obj_set_align(icon, LV_ALIGN_CENTER);
}

// Pattern 2: Navigation Tab Icon
{
    lv_obj_t* navIcon = ui_icon_create(tab_button, IconId::ICON_NAV_RADAR, theme);
    ui_icon_set_size(navIcon, 24, 24);
    lv_obj_set_style_margin_top(navIcon, 4, LV_PART_MAIN);
}

// Pattern 3: Icon with Label
{
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    
    lv_obj_t* icon = ui_icon_create(row, IconId::ICON_ALERTS, theme);
    ui_icon_set_size(icon, 24, 24);
    
    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, "Alerts");
}

// Pattern 4: Mapping Condition Code to Icon
{
    // In weather.h or weather_models.h
    enum class WeatherCondition { CLEAR, CLOUDY, RAINY, STORMY };
    
    IconId getIconForCondition(WeatherCondition cond, bool isDaylight) {
        switch (cond) {
            case WeatherCondition::CLEAR:
                return isDaylight ? IconId::ICON_CLEAR_DAY : IconId::ICON_CLEAR_NIGHT;
            case WeatherCondition::CLOUDY:
                return IconId::ICON_CLOUDY;
            case WeatherCondition::RAINY:
                return IconId::ICON_RAIN;
            case WeatherCondition::STORMY:
                return IconId::ICON_STORM;
            default:
                return IconId::ICON_PARTLY_CLOUDY;
        }
    }
    
    // Usage:
    IconId iconId = getIconForCondition(condition, isDaylight);
    lv_obj_t* icon = ui_icon_create(parent, iconId, theme);
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

// All errors are logged to Serial with "[ICON]" prefix
// Examples:
// [ICON] Loaded icon id 0 from /icons/weather_clear_day.png
// [ICON] ERROR: file not found
// [ICON] ERROR: SPIFFS is not mounted.

// If icon cannot load, a fallback colored circle is returned instead
// The UI will show a small circle instead of the PNG
// Check Serial monitor to diagnose issues

// ============================================================================
// STYLING
// ============================================================================

// After creation, you can apply LVGL styling:
lv_obj_t* icon = ui_icon_create(parent, IconId::ICON_RAIN, theme);

// Alignment
lv_obj_set_align(icon, LV_ALIGN_CENTER);
lv_obj_set_x(icon, 10);
lv_obj_set_y(icon, 20);

// Visibility
lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);      // Hide
lv_obj_clear_flag(icon, LV_OBJ_FLAG_HIDDEN);   // Show

// Opacity
lv_obj_set_style_opa(icon, LV_OPA_50, LV_PART_MAIN);  // 50% transparency

// ============================================================================
// SPIFFS PATHS (for reference)
// ============================================================================

// Weather icons:
/icons/weather_clear_day.png
/icons/weather_clear_night.png
/icons/weather_partly_cloudy.png
/icons/weather_cloudy.png
/icons/weather_rain.png
/icons/weather_storm.png
/icons/weather_snow.png
/icons/weather_fog.png
/icons/weather_wind.png

// Navigation icons:
/icons/nav_home.png
/icons/nav_radar.png
/icons/nav_alerts.png
/icons/nav_settings.png
/icons/nav_system.png

// ============================================================================
// DEBUGGING TIPS
// ============================================================================

// 1. Check if SPIFFS is mounted
if (!SPIFFS.begin(false)) { Serial.println("SPIFFS failed!"); }

// 2. List files in /icons/
File root = SPIFFS.open("/icons/");
File file = root.openNextFile();
while (file) {
    Serial.println(file.name());
    file = root.openNextFile();
}

// 3. Verify icon creation
lv_obj_t* icon = ui_icon_create(parent, IconId::ICON_RAIN, theme);
if (icon == nullptr) { Serial.println("Icon creation failed"); }

// 4. Monitor Serial output for [ICON] messages
// Watch Serial monitor while loading a page to see icon load status
