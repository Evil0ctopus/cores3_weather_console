#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../system/settings.h"
#include "../system/location_lookup.h"
#include "ui_theme.h"

namespace ui {

struct SystemInfo {
	bool wifiConnected = false;
	uint8_t wifiSignalBars = 0;
	int batteryPct = 0;
	bool batteryCharging = false;
	String currentTime;
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
	String locationName;
	String actionStatus;
	String wifiNetworks;
	bool wifiScanning = false;
	bool quietActive = false;
};

class SystemPage {
 public:
	typedef void (*ThemeSelectedCallback)(void*, ThemeId);
	void begin(lv_obj_t* parent, ThemeManager& theme, app::SettingsStore& settingsStore,
		ThemeSelectedCallback callback, void* context);
	void applyTheme(ThemeManager& theme);
	void update(const SystemInfo& info);
	void closeOverlays();

 private:
	enum class BindingType : uint8_t { Action, Bool, Number, Choice, Units, Theme, Cadence, WifiAuto, WifiNetwork };
	struct Binding {
		SystemPage* page = nullptr;
		BindingType type = BindingType::Action;
		uint16_t index = 0;
		lv_obj_t* widget = nullptr;
		lv_obj_t* label = nullptr;
	};
	static void onControl(lv_event_t* event);
	static void onEditor(lv_event_t* event);
	static void onConfirm(lv_event_t* event);
	void showSection(uint8_t section);
	void addGroup(app::ControlGroup group);
	lv_obj_t* addLabel(const String& text);
	lv_obj_t* addButton(const char* text, uint16_t action);
	void addChoice(const char* label, const char* options, uint16_t selected, BindingType type, uint16_t index);
	void addToggle(const char* label, bool value, BindingType type, uint16_t index);
	void addNumber(const char* label, uint32_t value, uint32_t min, uint32_t max, BindingType type, uint16_t index);
	Binding* bind(lv_obj_t* widget, BindingType type, uint16_t index, lv_obj_t* label = nullptr);
	void handleControl(Binding& binding, lv_event_code_t event);
	bool save(const app::AppSettings& settings);
	void action(uint16_t action);
	void edit(const char* key, const String& value, bool numeric = false, bool password = false);
	void closeEditor();
	void confirm(uint16_t action, const String& text);
	void notify(const String& text, bool error = false);

	app::SettingsStore* settingsStore_ = nullptr;
	ThemeManager* theme_ = nullptr;
	lv_obj_t* root_ = nullptr;
	lv_obj_t* title_ = nullptr;
	lv_obj_t* message_ = nullptr;
	lv_obj_t* list_ = nullptr;
	lv_obj_t* back_ = nullptr;
	lv_obj_t* infoLabel_ = nullptr;
	lv_obj_t* wifiDropdown_ = nullptr;
	lv_obj_t* editor_ = nullptr;
	lv_obj_t* editorInput_ = nullptr;
	lv_obj_t* editorMessage_ = nullptr;
	lv_obj_t* editorSave_ = nullptr;
	lv_obj_t* confirmation_ = nullptr;
	const char* editorKey_ = nullptr;
	bool lookupPending_ = false;
	bool resolved_ = false;
	app::LocationResult location_;
	String draftSsid_;
	String draftPassword_;
	bool wifiDraftDirty_ = false;
	String lastNetworks_;
	uint16_t confirmAction_ = 0;
	uint8_t section_ = 0;
	uint32_t displayedRevision_ = 0;
	Binding bindings_[48]{};
	size_t bindingCount_ = 0;
};

}  // namespace ui
