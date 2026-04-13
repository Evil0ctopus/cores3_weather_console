#pragma once

#include <lvgl.h>

#include "ui_theme.h"

namespace ui {

lv_obj_t* ui_background_create(lv_obj_t* root, ThemeId theme);
void ui_background_update(lv_obj_t* root, ThemeId theme);

void ui_backgrounds_init();
lv_obj_t* ui_backgrounds_attach_to_root(lv_obj_t* root, ThemeId theme);
void ui_backgrounds_update_theme(lv_obj_t* root, ThemeId theme);
const char* ui_backgrounds_get_path_for_theme(ThemeId theme);

}  // namespace ui
