# ComfyUI Icon Integration Guide

## Overview
The `ui_icons.*` module provides centralized PNG icon management for CORE_WEATHER, replacing LVGL-drawn primitives with ComfyUI-generated PNG icons stored in SPIFFS.

## Features
- **IconId enum**: 14 weather and system icon types
- **Path mapping**: IconId → SPIFFS file path
- **PNG loading**: Direct SPIFFS → LVGL image rendering
- **Error handling**: Comprehensive logging and safe fallbacks
- **Fallback primitive**: Colored circles if PNG fails
- **Theme integration**: Uses theme colors for fallback styling

## Architecture

### Header: `src/ui/ui_icons.h`
```cpp
namespace ui {
  enum class IconId : uint8_t { /* 14 icon types */ };
  const char* ui_icon_get_path(IconId id);
  lv_obj_t* ui_icon_create(lv_obj_t* parent, IconId id, ThemeId theme);
  void ui_icon_set_size(lv_obj_t* icon, uint16_t width, uint16_t height);
  void ui_icon_delete(lv_obj_t* icon);
}
```

### Implementation: `src/ui/ui_icons.cpp`
- Icon path mapping (weather_*.png, nav_*.png)
- SPIFFS availability checking
- Fallback circle creation with theme colors
- PNG loading with error logging
- Styling (padding, margins)

## Icon Categories

### Weather Icons (9 types)
- `ICON_CLEAR_DAY` → `/icons/weather_clear_day.png`
- `ICON_CLEAR_NIGHT` → `/icons/weather_clear_night.png`
- `ICON_PARTLY_CLOUDY` → `/icons/weather_partly_cloudy.png`
- `ICON_CLOUDY` → `/icons/weather_cloudy.png`
- `ICON_RAIN` → `/icons/weather_rain.png`
- `ICON_STORM` → `/icons/weather_storm.png`
- `ICON_SNOW` → `/icons/weather_snow.png`
- `ICON_FOG` → `/icons/weather_fog.png`
- `ICON_WIND` → `/icons/weather_wind.png`

### Navigation/System Icons (5 types)
- `ICON_NAV_HOME` → `/icons/nav_home.png`
- `ICON_NAV_RADAR` → `/icons/nav_radar.png`
- `ICON_NAV_ALERTS` → `/icons/nav_alerts.png`
- `ICON_NAV_SETTINGS` → `/icons/nav_settings.png`
- `ICON_NAV_SYSTEM` → `/icons/nav_system.png`

## Usage Pattern

### Basic Icon Creation
```cpp
#include "ui_icons.h"

// Create icon as child of parent
lv_obj_t* icon = ui_icon_create(parent_obj, IconId::ICON_RAIN, theme_id);

// Optionally set size
ui_icon_set_size(icon, 32, 32);

// Optional: position or apply additional styling
lv_obj_set_align(icon, LV_ALIGN_CENTER);
```

### In a Class (Example from ui_current.cpp)
```cpp
void CurrentPage::begin(lv_obj_t* parent, ThemeManager& theme) {
  // Instead of:
  // iconView_.begin(parent, 34);
  
  // Use:
  lv_obj_t* iconContainer = lv_obj_create(parent);
  lv_obj_set_size(iconContainer, 80, 80);
  
  // Create weather icon
  lv_obj_t* weatherIcon = ui_icon_create(iconContainer, 
    IconId::ICON_PARTLY_CLOUDY, theme.currentTheme());
  ui_icon_set_size(weatherIcon, 64, 64);
  lv_obj_set_align(weatherIcon, LV_ALIGN_CENTER);
}
```

## Error Handling & Logging

### Success Log
```
[ICON] Loaded icon id 0 from /icons/weather_clear_day.png
```

### Error Scenarios
1. **Parent is null**: Returns nullptr, logs error
2. **Unknown IconId**: Creates fallback circle (red #FF6B6B)
3. **SPIFFS unavailable**: Creates fallback with accent_primary color
4. **File not found**: Creates fallback with accent_secondary color
5. **PNG decode fails**: Creates fallback with text_secondary color

### Fallback Behavior
All errors result in a small colored circle (24x24 default) so the UI remains usable. Color is style-aware based on the provided theme.

## Integration Checklist

### Phase 1: Weather Icons (ui_current.cpp)
Replace `WeatherIconView` with `ui_icon_create`:
- [ ] Read current WeatherIconView implementation
- [ ] Map condition codes to IconId values
- [ ] Replace canvas rendering with PNG loading
- [ ] Test with various themes
- [ ] Verify sizing and alignment

### Phase 2: Navigation Icons (ui_root.cpp / ui_system.cpp)
Add nav icons to tab/menu items:
- [ ] Create nav icon objects
- [ ] Size appropriately (24x24 or 32x32)
- [ ] Apply theme styling
- [ ] Test fallback if PNGs missing

### Phase 3: Alert Icons (ui_alerts.cpp)
Add status/severity icons:
- [ ] Consider mapping alert types to icons
- [ ] Position in list rows
- [ ] Theme-aware sizing

### Phase 4: System Icons (ui_system.cpp)
WiFi, battery, settings indicators:
- [ ] Map system states to icon representations
- [ ] Scale for compact display
- [ ] Ensure consistent alignment

## Performance Notes
- PNG loading happens at icon creation time
- SPIFFS check is cached per load attempt
- Fallback circle is instant, no overhead
- No memory overhead beyond the PNG buffer
- Icon objects follow standard LVGL lifecycle

## Compatibility
- LVGL 8.4.0 (M5Stack cores3)
- Arduino ESP32 framework
- SPIFFS filesystem
- Works with all 10 themes (uses theme colors for fallback)

## Future Enhancements
- [ ] Icon atlas/sprite sheet for batch loading
- [ ] Animation support (multi-frame icons)
- [ ] Icon caching in RAM for frequently used icons
- [ ] Dynamic tinting based on theme
- [ ] Icon state overlays (e.g., warning badge)

## Testing Checklist
- [ ] SPIFFS mount verification
- [ ] File existence checks
- [ ] PNG decode error handling
- [ ] Fallback circle rendering
- [ ] Theme color application
- [ ] Layout alignment with various sizes
- [ ] Memory usage with multiple icons
- [ ] Boot time impact from icon preloading
