#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "ui_theme.h"

namespace ui {

struct SystemInfo {
	String ipAddress;
	String webUiUrl;
	String wifiSsid;
	String wifiRssi;
	String wifiStatus;
	String bleStatus;
	String weatherStatus;
	String radarStatus;
	String lastUpdate;
	String spiffsUsage;
	String firmwareVersion;
	String ledMode;
};

class SystemPage {
 public:
	void begin(lv_obj_t* parent, ThemeManager& theme);
	void applyTheme(ThemeManager& theme);
	void update(const SystemInfo& info);

 private:
	lv_obj_t* root_ = nullptr;
	lv_obj_t* title_ = nullptr;
	lv_obj_t* subtitle_ = nullptr;
	lv_obj_t* list_ = nullptr;
	lv_obj_t* rowLabels_[11] = {};
};

}  // namespace ui
