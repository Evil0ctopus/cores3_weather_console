#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "lv_psram.h"
#define LV_MEM_CUSTOM_ALLOC lv_psram_alloc
#define LV_MEM_CUSTOM_FREE heap_caps_free
#define LV_MEM_CUSTOM_REALLOC lv_psram_realloc
#define LV_USE_LOG 0
#define LV_USE_PNG 1
#define LV_IMG_CACHE_DEF_SIZE 24

// Use Arduino millis() as the tick source so lv_tick_inc() doesn't need
// to be called manually in the loop.
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
#define LV_TICK_CUSTOM_EXPRESSION (millis())

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1

#endif  // LV_CONF_H
