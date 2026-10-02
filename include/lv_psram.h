#pragma once

#include <esp_heap_caps.h>

static inline void* lv_psram_alloc(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static inline void* lv_psram_realloc(void* pointer, size_t size) {
    return heap_caps_realloc(pointer, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
