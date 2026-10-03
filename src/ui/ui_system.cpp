#include "ui_system.h"

#include "../system/device_remote.h"
#include <string.h>

namespace ui {
namespace {
const char* kSections[] = {"System Controls", "Location & Weather", "Units", "Sound",
	"LEDs", "Display", "WiFi", "Quiet Hours", "Device & Diagnostics"};

void styleButton(lv_obj_t* button, const ThemeManager& theme) {
	lv_obj_set_style_bg_color(button, theme.palette().surfaceAlt, 0);
	lv_obj_set_style_bg_color(button, lv_color_mix(theme.palette().accent, theme.palette().surfaceAlt, LV_OPA_30), LV_STATE_PRESSED);
	lv_obj_set_style_border_width(button, 1, 0);
	lv_obj_set_style_border_color(button, theme.palette().accent, 0);
	lv_obj_set_style_border_opa(button, LV_OPA_40, 0);
	lv_obj_set_style_radius(button, 12, 0);
	lv_obj_set_style_text_color(lv_obj_get_child(button, 0), theme.palette().textPrimary, 0);
}
}

void SystemPage::begin(lv_obj_t* parent, ThemeManager& theme, app::SettingsStore& store,
		ThemeSelectedCallback callback, void* context) {
	(void)callback;
	(void)context;
	settingsStore_ = &store;
	theme_ = &theme;
	root_ = lv_obj_create(parent);
	lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
	ui_make_container_transparent(root_);
	lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(root_, 6, 0);
	lv_obj_set_style_border_width(root_, 0, 0);
	title_ = lv_label_create(root_);
	lv_obj_set_style_text_font(title_, &lv_font_montserrat_14, 0);
	lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, 4);
	back_ = lv_btn_create(root_);
	lv_obj_set_size(back_, 62, 30);
	lv_obj_align(back_, LV_ALIGN_TOP_RIGHT, 0, 0);
	lv_obj_t* label = lv_label_create(back_);
	lv_label_set_text(label, "Back");
	lv_obj_center(label);
	lv_obj_add_event_cb(back_, [](lv_event_t* event) {
		static_cast<SystemPage*>(lv_event_get_user_data(event))->showSection(0);
	}, LV_EVENT_CLICKED, this);
	message_ = lv_label_create(root_);
	lv_obj_set_width(message_, lv_pct(100));
	lv_obj_set_style_text_font(message_, &lv_font_montserrat_14, 0);
	lv_label_set_long_mode(message_, LV_LABEL_LONG_DOT);
	lv_obj_set_height(message_, 32);
	lv_obj_set_pos(message_, 0, 32);
	list_ = lv_obj_create(root_);
	lv_obj_set_size(list_, lv_pct(100), 130);
	lv_obj_set_pos(list_, 0, 64);
	lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(list_, 0, 0);
	lv_obj_set_style_pad_all(list_, 2, 0);
	lv_obj_set_style_pad_row(list_, 10, 0);
	lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(list_, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_AUTO);
	applyTheme(theme);
	showSection(0);
}

void SystemPage::applyTheme(ThemeManager& theme) {
	theme_ = &theme;
	lv_obj_set_style_text_color(root_, theme.palette().textPrimary, 0);
	lv_obj_set_style_text_color(title_, theme.palette().accent, 0);
	lv_obj_set_style_text_color(message_, theme.palette().textSecondary, 0);
	styleButton(back_, theme);
	for (size_t i = 0; i < bindingCount_; ++i) {
		if (bindings_[i].type != BindingType::Action) continue;
		styleButton(bindings_[i].widget, theme);
	}
}

SystemPage::Binding* SystemPage::bind(lv_obj_t* widget, BindingType type, uint16_t index, lv_obj_t* label) {
	if (bindingCount_ >= sizeof(bindings_) / sizeof(bindings_[0])) {
		notify("Too many controls", true);
		return nullptr;
	}
	Binding& binding = bindings_[bindingCount_++];
	binding.page = this;
	binding.type = type;
	binding.index = index;
	binding.widget = widget;
	binding.label = label;
	lv_obj_add_event_cb(widget, onControl, LV_EVENT_ALL, &binding);
	return &binding;
}

lv_obj_t* SystemPage::addLabel(const String& text) {
	lv_obj_t* label = lv_label_create(list_);
	lv_obj_set_width(label, lv_pct(100));
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_label_set_text(label, text.c_str());
	return label;
}

lv_obj_t* SystemPage::addButton(const char* text, uint16_t actionId) {
	lv_obj_t* button = lv_btn_create(list_);
	lv_obj_set_size(button, lv_pct(100), 42);
	lv_obj_set_style_bg_color(button, theme_->palette().surfaceAlt, 0);
	lv_obj_t* label = lv_label_create(button);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, theme_->palette().textPrimary, 0);
	lv_label_set_text(label, text);
	lv_obj_center(label);
	styleButton(button, *theme_);
	bind(button, BindingType::Action, actionId);
	return button;
}

void SystemPage::addChoice(const char* label, const char* options, uint16_t selected, BindingType type, uint16_t index) {
	addLabel(label);
	lv_obj_t* dropdown = lv_dropdown_create(list_);
	lv_obj_set_size(dropdown, lv_pct(100), 42);
	lv_obj_set_style_text_font(dropdown, &lv_font_montserrat_14, 0);
	lv_dropdown_set_options(dropdown, options);
	lv_dropdown_set_selected(dropdown, selected);
	bind(dropdown, type, index);
}

void SystemPage::addToggle(const char* label, bool value, BindingType type, uint16_t index) {
	lv_obj_t* checkbox = lv_checkbox_create(list_);
	lv_checkbox_set_text(checkbox, label);
	lv_obj_set_width(checkbox, lv_pct(100));
	lv_obj_set_style_text_font(checkbox, &lv_font_montserrat_14, 0);
	lv_obj_set_style_pad_ver(checkbox, 8, 0);
	if (value) lv_obj_add_state(checkbox, LV_STATE_CHECKED);
	bind(checkbox, type, index);
}

void SystemPage::addNumber(const char* label, uint32_t value, uint32_t min, uint32_t max, BindingType type, uint16_t index) {
	lv_obj_t* caption = addLabel(String(label) + ": " + value);
	lv_obj_t* slider = lv_slider_create(list_);
	lv_obj_set_size(slider, lv_pct(90), 18);
	lv_slider_set_range(slider, min, max);
	lv_slider_set_value(slider, value, LV_ANIM_OFF);
	bind(slider, type, index, caption);
}

void SystemPage::addGroup(app::ControlGroup group) {
	const auto& controls = settingsStore_->settings().controls;
	for (size_t i = 0; i < app::kChoiceControlCount; ++i) {
		const auto& field = app::kChoiceControls[i];
		if (field.group == group) addChoice(field.label, field.labels,
			app::choiceIndex(field.values, controls.*field.member), BindingType::Choice, i);
	}
	for (size_t i = 0; i < app::kNumberControlCount; ++i) {
		const auto& field = app::kNumberControls[i];
		if (field.group == group) addNumber(field.label, controls.*field.member, field.min, field.max, BindingType::Number, i);
	}
	for (size_t i = 0; i < app::kBoolControlCount; ++i) {
		const auto& field = app::kBoolControls[i];
		if (field.group == group) addToggle(field.label, controls.*field.member, BindingType::Bool, i);
	}
}

void SystemPage::showSection(uint8_t section) {
	section_ = section;
	lv_obj_clean(list_);
	bindingCount_ = 0;
	infoLabel_ = nullptr;
	wifiDropdown_ = nullptr;
	lv_obj_scroll_to_y(list_, 0, LV_ANIM_OFF);
	lv_obj_set_flex_flow(list_, section == 0 ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(list_, section == 0 ? 4 : 10, 0);
	lv_obj_set_style_pad_column(list_, 4, 0);
	lv_obj_set_pos(list_, 0, section == 0 ? 38 : 64);
	lv_obj_set_height(list_, section == 0 ? 156 : 130);
	lv_obj_set_pos(message_, 0, section == 0 ? 22 : 32);
	lv_obj_set_height(message_, section == 0 ? 16 : 32);
	lv_label_set_text(title_, kSections[section]);
	lv_label_set_text(message_, "");
	if (section == 0) lv_obj_add_flag(back_, LV_OBJ_FLAG_HIDDEN);
	else lv_obj_clear_flag(back_, LV_OBJ_FLAG_HIDDEN);
	const auto& settings = settingsStore_->settings();
	displayedRevision_ = settingsStore_->revision();
	switch (section) {
		case 0:
			for (uint8_t i = 1; i < 9; ++i) {
				const char* labels[] = {"Location", "Units", "Sound", "LEDs", "Display", "WiFi", "Quiet Hours", "Device"};
				lv_obj_t* button = addButton(labels[i - 1], 100 + i);
				lv_obj_set_size(button, lv_pct(49), 36);
			}
			break;
		case 1:
			infoLabel_ = addLabel("Saved: " + settings.locationQuery);
			addButton("Enter ZIP (US postal code)", 1);
			addButton("Enter city / state", 2);
			addButton("Weather API key (optional)", 3);
			addNumber("Update interval (minutes)", settings.updateIntervalMinutes, 1, 60, BindingType::Cadence, 0);
			addButton("Refresh weather now", 4);
			break;
		case 2:
			addChoice("Temperature, wind and pressure", "Metric: C / kph / mbar\nImperial: F / mph / inHg",
				static_cast<uint16_t>(settings.units), BindingType::Units, 0);
			break;
		case 3:
			addGroup(app::ControlGroup::Sound);
			addButton("Test speaker", 5);
			break;
		case 4:
			addGroup(app::ControlGroup::Leds);
			addLabel("Active Weather follows current conditions. Storm pulses are soft and occasional. Mode changes apply immediately.");
			break;
		case 5: {
			String options;
			for (uint8_t i = 0; i < theme_count(); ++i) {
				if (i) options += "\n";
				options += theme_id_to_name(theme_id_from_index(i));
			}
			addChoice("Theme", options.c_str(), static_cast<uint16_t>(settings.theme), BindingType::Theme, 0);
			addGroup(app::ControlGroup::Display);
			break;
		}
		case 6:
			draftSsid_ = settings.wifiSsid;
			draftPassword_ = settings.wifiPassword;
			wifiDraftDirty_ = false;
			addButton("Scan nearby networks", 6);
			addChoice("Choose network", "Scan to find networks", 0, BindingType::WifiNetwork, 0);
			wifiDropdown_ = bindings_[bindingCount_ - 1].widget;
			lastNetworks_ = "";
			addButton("Enter network name", 7);
			addButton("Enter WiFi password", 8);
			addToggle("Auto reconnect", settings.wifiAutoConnect, BindingType::WifiAuto, 0);
			addButton("Save and connect", 9);
			infoLabel_ = addLabel("Network: " + draftSsid_);
			break;
		case 7:
			addGroup(app::ControlGroup::Quiet);
			addLabel("Uses weather-location local time. Equal start/end means all day. Quiet hours dim the screen/LEDs and silence optional sounds; mute always wins.");
			break;
		case 8:
			addButton("Restart device", 10);
			addButton("Restore control preferences", 11);
			infoLabel_ = addLabel("Loading diagnostics...");
			break;
	}
}

void SystemPage::onControl(lv_event_t* event) {
	Binding* binding = static_cast<Binding*>(lv_event_get_user_data(event));
	binding->page->handleControl(*binding, lv_event_get_code(event));
}

void SystemPage::handleControl(Binding& binding, lv_event_code_t event) {
	if (binding.type == BindingType::Action) {
		if (event == LV_EVENT_CLICKED) action(binding.index);
		return;
	}
	const bool slider = binding.type == BindingType::Number || binding.type == BindingType::Cadence;
	if (slider && event == LV_EVENT_VALUE_CHANGED) {
		const char* label = binding.type == BindingType::Number ? app::kNumberControls[binding.index].label : "Update interval (minutes)";
		lv_label_set_text(binding.label, (String(label) + ": " + lv_slider_get_value(binding.widget)).c_str());
		return;
	}
	if ((slider && event != LV_EVENT_RELEASED) || (!slider && event != LV_EVENT_VALUE_CHANGED)) return;
	app::AppSettings next = settingsStore_->settings();
	switch (binding.type) {
		case BindingType::Bool:
			next.controls.*app::kBoolControls[binding.index].member = lv_obj_has_state(binding.widget, LV_STATE_CHECKED); break;
		case BindingType::Number:
			next.controls.*app::kNumberControls[binding.index].member = lv_slider_get_value(binding.widget); break;
		case BindingType::Choice:
			next.controls.*app::kChoiceControls[binding.index].member =
				app::choiceAt(app::kChoiceControls[binding.index].values, lv_dropdown_get_selected(binding.widget)); break;
		case BindingType::Units:
			next.units = static_cast<app::UnitsSystem>(lv_dropdown_get_selected(binding.widget)); break;
		case BindingType::Theme:
			next.theme = theme_id_from_index(lv_dropdown_get_selected(binding.widget)); break;
		case BindingType::Cadence:
			next.updateIntervalMinutes = lv_slider_get_value(binding.widget); break;
		case BindingType::WifiAuto:
			next.wifiAutoConnect = lv_obj_has_state(binding.widget, LV_STATE_CHECKED); break;
		case BindingType::WifiNetwork: {
			char ssid[33];
			lv_dropdown_get_selected_str(binding.widget, ssid, sizeof(ssid));
			if (lastNetworks_.length() == 0) return;
			if (draftSsid_ != ssid) draftPassword_ = "";
			draftSsid_ = ssid;
			wifiDraftDirty_ = true;
			notify("Selected " + draftSsid_ + "; enter password then Connect");
			return;
		}
		case BindingType::Action: return;
	}
	save(next);
}

bool SystemPage::save(const app::AppSettings& settings) {
	String error;
	app::AppSettings next = settings;
	if (!app::validateSettings(next, error, false) || !settingsStore_->save(next)) {
		notify(error.length() ? error : String("Could not save settings"), true);
		return false;
	}
	displayedRevision_ = settingsStore_->revision();
	notify("Saved");
	return true;
}

void SystemPage::notify(const String& text, bool error) {
	lv_obj_set_style_text_color(message_, error ? theme_->palette().warning : theme_->palette().textSecondary, 0);
	lv_label_set_text(message_, text.c_str());
	if (error && editorMessage_ != nullptr) lv_label_set_text(editorMessage_, text.c_str());
	if (error) Serial.printf("[SYSTEM] ERROR: %s\n", text.c_str());
}

void SystemPage::action(uint16_t id) {
	if (id > 100 && id < 109) {
		showSection(id - 100);
		return;
	}
	const auto& settings = settingsStore_->settings();
	if (id == 1 || id == 2) edit("locationQuery", settings.locationQuery, id == 1);
	else if (id == 3) edit("apiKey", settings.apiKey, false, true);
	else if (id == 7) edit("wifiSsid", draftSsid_);
	else if (id == 8) edit("wifiPassword", draftPassword_, false, true);
	else if (id == 10) confirm(id, "Restart the device?");
	else if (id == 11) confirm(id, "Restore sound, LED, display,\nquiet-hour, unit and theme defaults?\nLocation and WiFi are kept.");
	else {
		app::DeviceRemoteCommand command;
		if (id == 4) command.type = app::DeviceRemoteCommandType::Refresh;
		else if (id == 5) command.type = app::DeviceRemoteCommandType::Sound;
		else if (id == 6) command.type = app::DeviceRemoteCommandType::WifiScan;
		else if (id == 9) {
			app::AppSettings next = settings;
			next.wifiSsid = draftSsid_;
			next.wifiPassword = draftPassword_;
			if (draftSsid_.length() == 0) { notify("Select or enter a network", true); return; }
			if (!save(next)) return;
			wifiDraftDirty_ = false;
			command.type = app::DeviceRemoteCommandType::WifiConnect;
		} else return;
		if (!app::device_remote_enqueue(command)) notify("Device command queue full", true);
		else notify("Command requested");
	}
}

void SystemPage::edit(const char* key, const String& value, bool numeric, bool password) {
	closeEditor();
	editorKey_ = key;
	resolved_ = false;
	editor_ = lv_obj_create(lv_layer_top());
	lv_obj_set_size(editor_, 320, 240);
	lv_obj_set_pos(editor_, 0, 0);
	lv_obj_clear_flag(editor_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(editor_, 4, 0);
	lv_obj_set_style_border_width(editor_, 0, 0);
	lv_obj_set_style_pad_row(editor_, 4, 0);
	lv_obj_set_flex_flow(editor_, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_bg_color(editor_, theme_->palette().surface, 0);
	lv_obj_set_style_text_color(editor_, theme_->palette().textPrimary, 0);
	editorMessage_ = lv_label_create(editor_);
	lv_obj_set_size(editorMessage_, lv_pct(100), 32);
	lv_obj_set_style_text_font(editorMessage_, &lv_font_montserrat_14, 0);
	lv_label_set_long_mode(editorMessage_, LV_LABEL_LONG_WRAP);
	lv_label_set_text(editorMessage_, key);
	editorInput_ = lv_textarea_create(editor_);
	lv_obj_set_size(editorInput_, lv_pct(100), 38);
	lv_textarea_set_one_line(editorInput_, true);
	lv_textarea_set_password_mode(editorInput_, password);
	lv_textarea_set_max_length(editorInput_, strcmp(key, "wifiSsid") == 0 ? 32 : strcmp(key, "wifiPassword") == 0 ? 63 : 100);
	if (numeric) lv_textarea_set_accepted_chars(editorInput_, "0123456789");
	lv_textarea_set_text(editorInput_, value.c_str());
	lv_obj_t* keyboard = lv_keyboard_create(editor_);
	lv_obj_set_width(keyboard, lv_pct(100));
	lv_obj_set_flex_grow(keyboard, 1);
	lv_keyboard_set_mode(keyboard, numeric ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
	lv_keyboard_set_textarea(keyboard, editorInput_);
	lv_obj_t* actions = lv_obj_create(editor_);
	ui_make_container_transparent(actions);
	lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_size(actions, lv_pct(100), 38);
	lv_obj_set_style_border_width(actions, 0, 0);
	lv_obj_set_style_pad_all(actions, 0, 0);
	lv_obj_set_style_pad_column(actions, 8, 0);
	lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
	lv_obj_t* cancel = lv_btn_create(actions);
	lv_obj_set_height(cancel, lv_pct(100));
	lv_obj_set_flex_grow(cancel, 1);
	lv_obj_t* label = lv_label_create(cancel);
	lv_label_set_text(label, "Cancel");
	lv_obj_center(label);
	styleButton(cancel, *theme_);
	lv_obj_add_event_cb(cancel, onEditor, LV_EVENT_CLICKED, this);
	editorSave_ = lv_btn_create(actions);
	lv_obj_set_height(editorSave_, lv_pct(100));
	lv_obj_set_flex_grow(editorSave_, 1);
	label = lv_label_create(editorSave_);
	lv_label_set_text(label, strcmp(key, "locationQuery") == 0 ? "Find location" : "Save");
	lv_obj_center(label);
	styleButton(editorSave_, *theme_);
	lv_obj_add_event_cb(editorSave_, onEditor, LV_EVENT_CLICKED, this);
}

void SystemPage::closeEditor() {
	if (editor_ != nullptr) lv_obj_del(editor_);
	editor_ = nullptr;
	editorInput_ = nullptr;
	editorSave_ = nullptr;
	editorMessage_ = nullptr;
	editorKey_ = nullptr;
}

void SystemPage::closeOverlays() {
	closeEditor();
	if (confirmation_ != nullptr) lv_obj_del(confirmation_);
	confirmation_ = nullptr;
	for (size_t i = 0; i < bindingCount_; ++i) {
		if (lv_obj_check_type(bindings_[i].widget, &lv_dropdown_class)) lv_dropdown_close(bindings_[i].widget);
	}
}

void SystemPage::onEditor(lv_event_t* event) {
	auto* page = static_cast<SystemPage*>(lv_event_get_user_data(event));
	if (lv_event_get_target(event) != page->editorSave_) {
		page->closeEditor();
		return;
	}
	const String value(lv_textarea_get_text(page->editorInput_));
	const String key(page->editorKey_);
	if (key == "locationQuery") {
		if (!page->resolved_ || page->location_.query != value ||
				page->location_.apiKey != page->settingsStore_->settings().apiKey) {
			String error;
			if (!app::locationLookupStart(page->settingsStore_->settings().apiKey, value, error)) {
				lv_label_set_text(page->editorMessage_, error.c_str());
				return;
			}
			page->lookupPending_ = true;
			lv_label_set_text(page->editorMessage_, "Resolving location...");
			return;
		}
		app::AppSettings next = page->settingsStore_->settings();
		next.locationQuery = value;
		next.locationKey = page->location_.key;
		next.locationName = page->location_.name;
		if (!page->save(next)) return;
	} else if (key == "wifiSsid" || key == "wifiPassword") {
		if (key == "wifiSsid") {
			if (value != page->draftSsid_) page->draftPassword_ = "";
			page->draftSsid_ = value;
		} else page->draftPassword_ = value;
		page->wifiDraftDirty_ = true;
		page->notify("WiFi draft updated; tap Save and connect");
	} else {
		app::AppSettings next = page->settingsStore_->settings();
		next.apiKey = value;
		next.locationKey = "";
		next.locationName = "";
		if (!page->save(next)) return;
	}
	page->closeEditor();
}

void SystemPage::confirm(uint16_t actionId, const String& text) {
	confirmAction_ = actionId;
	confirmation_ = lv_obj_create(lv_layer_top());
	lv_obj_set_size(confirmation_, 320, 240);
	lv_obj_center(confirmation_);
	lv_obj_set_style_bg_color(confirmation_, theme_->palette().surface, 0);
	lv_obj_set_style_text_color(confirmation_, theme_->palette().textPrimary, 0);
	lv_obj_t* label = lv_label_create(confirmation_);
	lv_obj_set_width(label, 280);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_label_set_text(label, text.c_str());
	lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 30);
	for (uint8_t i = 0; i < 2; ++i) {
		lv_obj_t* button = lv_btn_create(confirmation_);
		lv_obj_set_size(button, 120, 44);
		lv_obj_align(button, i ? LV_ALIGN_BOTTOM_RIGHT : LV_ALIGN_BOTTOM_LEFT, 0, -10);
		lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
		label = lv_label_create(button);
		lv_label_set_text(label, i ? "Confirm" : "Cancel");
		lv_obj_center(label);
		styleButton(button, *theme_);
		lv_obj_add_event_cb(button, onConfirm, LV_EVENT_CLICKED, this);
	}
}

void SystemPage::onConfirm(lv_event_t* event) {
	auto* page = static_cast<SystemPage*>(lv_event_get_user_data(event));
	const bool confirmed = lv_obj_get_user_data(lv_event_get_target(event)) != nullptr;
	const uint16_t action = page->confirmAction_;
	lv_obj_del(page->confirmation_);
	page->confirmation_ = nullptr;
	if (!confirmed) return;
	if (action == 10) {
		app::DeviceRemoteCommand command;
		command.type = app::DeviceRemoteCommandType::Restart;
		if (!app::device_remote_enqueue(command)) page->notify("Restart queue full", true);
	} else {
		app::AppSettings next = page->settingsStore_->settings();
		next.controls = app::ControlSettings();
		next.units = app::UnitsSystem::Metric;
		next.theme = ThemeId::PIXEL_STORM;
		page->save(next);
	}
}

void SystemPage::update(const SystemInfo& info) {
	app::LocationResult result;
	if (lookupPending_ && app::locationLookupTake(result)) {
		lookupPending_ = false;
		if (editor_ != nullptr && editorKey_ != nullptr && strcmp(editorKey_, "locationQuery") == 0) {
			location_ = result;
			if (result.error.length() == 0 && (result.query != lv_textarea_get_text(editorInput_) ||
					result.apiKey != settingsStore_->settings().apiKey)) {
				result.error = "Input changed during lookup. Tap Try again.";
			}
			resolved_ = result.error.length() == 0 &&
				result.query == lv_textarea_get_text(editorInput_) &&
				result.apiKey == settingsStore_->settings().apiKey;
			lv_label_set_text(editorMessage_, resolved_ ? result.name.c_str() : result.error.c_str());
			lv_label_set_text(lv_obj_get_child(editorSave_, 0), resolved_ ? "Confirm place" : "Try again");
		}
	}
	if (editor_ == nullptr && confirmation_ == nullptr && displayedRevision_ != settingsStore_->revision()) {
		const bool preserveWifi = section_ == 6 && wifiDraftDirty_;
		const String ssid = preserveWifi ? draftSsid_ : String();
		const String password = preserveWifi ? draftPassword_ : String();
		showSection(section_);
		if (preserveWifi) {
			draftSsid_ = ssid;
			draftPassword_ = password;
			wifiDraftDirty_ = true;
		}
		notify("Updated from shared settings");
	}
	if (section_ == 6 && wifiDropdown_ != nullptr && info.wifiNetworks.length() > 0 && info.wifiNetworks != lastNetworks_) {
		lastNetworks_ = info.wifiNetworks;
		lv_dropdown_set_options(wifiDropdown_, lastNetworks_.c_str());
		const int index = app::choiceIndex(lastNetworks_.c_str(), draftSsid_);
		if (index >= 0) lv_dropdown_set_selected(wifiDropdown_, index);
	}
	if (infoLabel_ != nullptr) {
		String text;
		if (section_ == 1) text = "Saved: " + settingsStore_->settings().locationQuery + "\nResolved: " +
			settingsStore_->settings().locationName + "\n" + info.weatherStatus;
		else if (section_ == 6) text = "Draft: " + draftSsid_ + "\n" + info.wifiStatus +
			(info.wifiScanning ? "\nScanning..." : "") + "\n" + info.actionStatus;
		else if (section_ == 8) text = "Firmware: " + info.firmwareVersion + "\nIP: " + info.ipAddress +
			"\nWiFi: " + info.wifiSsid + " " + info.wifiRssi + "\nWeather: " + info.weatherStatus +
			"\nRadar: " + info.radarStatus + "\nUpdated: " + info.lastUpdate + "\nStorage: " +
			info.spiffsUsage + "\nLEDs: " + info.ledMode + "\nQuiet hours: " + (info.quietActive ? "Active" : "Inactive");
		ui_label_set_text_if_changed(infoLabel_, text.c_str());
	}
}

}  // namespace ui
