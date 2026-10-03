#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace app {

enum class DeviceRemoteCommandType : uint8_t {
	Page,
	Touch,
	Sound,
	Refresh,
	WifiScan,
	WifiConnect,
	Restart,
};

struct DeviceRemoteCommand {
	DeviceRemoteCommandType type = DeviceRemoteCommandType::Page;
	int16_t x = 0;
	int16_t y = 0;
	uint8_t value = 0;
	bool pressed = false;
};

bool device_remote_begin();
void device_remote_capture(const lv_area_t* area, const lv_color_t* pixels);
uint8_t* device_remote_create_bmp(size_t& outLength, bool nativeColor = false);
uint32_t device_remote_frame_count();
bool device_remote_enqueue(const DeviceRemoteCommand& command);
bool device_remote_peek_command(DeviceRemoteCommand& command);
bool device_remote_take_command(DeviceRemoteCommand& command);

}  // namespace app