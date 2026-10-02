#include "device_remote.h"

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <string.h>

namespace app {
namespace {

constexpr uint16_t kScreenWidth = 320;
constexpr uint16_t kScreenHeight = 240;
constexpr size_t kPixelCount = static_cast<size_t>(kScreenWidth) * kScreenHeight;
constexpr size_t kBmpHeaderSize = 54;
constexpr size_t kBmpRowSize = static_cast<size_t>(kScreenWidth) * 3;
constexpr size_t kBmpSize = kBmpHeaderSize + (kBmpRowSize * kScreenHeight);

uint16_t* gScreenMirror = nullptr;
SemaphoreHandle_t gScreenMutex = nullptr;
QueueHandle_t gCommandQueue = nullptr;
uint32_t gFrameCaptureCount = 0;

void writeU16(uint8_t* output, size_t offset, uint16_t value) {
	output[offset] = static_cast<uint8_t>(value & 0xFFU);
	output[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
}

void writeU32(uint8_t* output, size_t offset, uint32_t value) {
	output[offset] = static_cast<uint8_t>(value & 0xFFU);
	output[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
	output[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
	output[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
}

}  // namespace

bool device_remote_begin() {
	if (gScreenMirror == nullptr) {
		gScreenMirror = static_cast<uint16_t*>(heap_caps_calloc(
			kPixelCount, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	}
	if (gScreenMutex == nullptr) {
		gScreenMutex = xSemaphoreCreateMutex();
	}
	if (gCommandQueue == nullptr) {
		gCommandQueue = xQueueCreate(8, sizeof(DeviceRemoteCommand));
	}
	return gScreenMirror != nullptr && gScreenMutex != nullptr && gCommandQueue != nullptr;
}

void device_remote_capture(const lv_area_t* area, const lv_color_t* pixels) {
	if (gScreenMirror == nullptr || gScreenMutex == nullptr || area == nullptr || pixels == nullptr) {
		return;
	}
	if (xSemaphoreTake(gScreenMutex, portMAX_DELAY) != pdTRUE) {
		return;
	}

	const int32_t x1 = area->x1 < 0 ? 0 : area->x1;
	const int32_t y1 = area->y1 < 0 ? 0 : area->y1;
	const int32_t x2 = area->x2 >= kScreenWidth ? kScreenWidth - 1 : area->x2;
	const int32_t y2 = area->y2 >= kScreenHeight ? kScreenHeight - 1 : area->y2;
	const int32_t sourceWidth = area->x2 - area->x1 + 1;
	const uint16_t* source = reinterpret_cast<const uint16_t*>(pixels);
	for (int32_t y = y1; y <= y2; ++y) {
		const size_t sourceOffset = static_cast<size_t>(y - area->y1) * sourceWidth + static_cast<size_t>(x1 - area->x1);
		const size_t destinationOffset = static_cast<size_t>(y) * kScreenWidth + static_cast<size_t>(x1);
		memcpy(gScreenMirror + destinationOffset, source + sourceOffset,
			static_cast<size_t>(x2 - x1 + 1) * sizeof(uint16_t));
	}
	++gFrameCaptureCount;
	xSemaphoreGive(gScreenMutex);
}

uint8_t* device_remote_create_bmp(size_t& outLength, bool nativeColor) {
	outLength = 0;
	if (gScreenMirror == nullptr || gScreenMutex == nullptr) {
		return nullptr;
	}

	const size_t headerSize = nativeColor ? 66 : kBmpHeaderSize;
	const size_t bitmapSize = nativeColor ? headerSize + kPixelCount * sizeof(uint16_t) : kBmpSize;
	uint8_t* bitmap = static_cast<uint8_t*>(heap_caps_malloc(
		bitmapSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	if (bitmap == nullptr) {
		return nullptr;
	}
	memset(bitmap, 0, headerSize);

	bitmap[0] = 'B';
	bitmap[1] = 'M';
	writeU32(bitmap, 2, static_cast<uint32_t>(bitmapSize));
	writeU32(bitmap, 10, static_cast<uint32_t>(headerSize));
	writeU32(bitmap, 14, 40);
	writeU32(bitmap, 18, kScreenWidth);
	writeU32(bitmap, 22, nativeColor ? static_cast<uint32_t>(-static_cast<int32_t>(kScreenHeight)) : kScreenHeight);
	writeU16(bitmap, 26, 1);
	writeU16(bitmap, 28, nativeColor ? 16 : 24);
	writeU32(bitmap, 34, static_cast<uint32_t>(bitmapSize - headerSize));
	if (nativeColor) {
		// Top-down RGB565 bitfields preserve display pixels without per-pixel conversion.
		writeU32(bitmap, 30, 3);
		writeU32(bitmap, 54, 0xF800);
		writeU32(bitmap, 58, 0x07E0);
		writeU32(bitmap, 62, 0x001F);
		if (xSemaphoreTake(gScreenMutex, portMAX_DELAY) != pdTRUE) {
			heap_caps_free(bitmap);
			return nullptr;
		}
		memcpy(bitmap + headerSize, gScreenMirror, kPixelCount * sizeof(uint16_t));
		xSemaphoreGive(gScreenMutex);
		outLength = bitmapSize;
		return bitmap;
	}

	uint16_t* snapshot = static_cast<uint16_t*>(heap_caps_malloc(
		kPixelCount * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	if (snapshot == nullptr) {
		heap_caps_free(bitmap);
		return nullptr;
	}
	if (xSemaphoreTake(gScreenMutex, portMAX_DELAY) != pdTRUE) {
		heap_caps_free(snapshot);
		heap_caps_free(bitmap);
		return nullptr;
	}
	memcpy(snapshot, gScreenMirror, kPixelCount * sizeof(uint16_t));
	xSemaphoreGive(gScreenMutex);
	// Convert the private snapshot without blocking physical display flushes.
	for (uint16_t outputY = 0; outputY < kScreenHeight; ++outputY) {
		const uint16_t sourceY = kScreenHeight - 1U - outputY;
		uint8_t* row = bitmap + kBmpHeaderSize + static_cast<size_t>(outputY) * kBmpRowSize;
		for (uint16_t x = 0; x < kScreenWidth; ++x) {
			const uint16_t color = snapshot[static_cast<size_t>(sourceY) * kScreenWidth + x];
			const uint8_t red5 = static_cast<uint8_t>((color >> 11) & 0x1FU);
			const uint8_t green6 = static_cast<uint8_t>((color >> 5) & 0x3FU);
			const uint8_t blue5 = static_cast<uint8_t>(color & 0x1FU);
			const size_t pixelOffset = static_cast<size_t>(x) * 3;
			row[pixelOffset] = static_cast<uint8_t>((blue5 << 3) | (blue5 >> 2));
			row[pixelOffset + 1] = static_cast<uint8_t>((green6 << 2) | (green6 >> 4));
			row[pixelOffset + 2] = static_cast<uint8_t>((red5 << 3) | (red5 >> 2));
		}
	}
	heap_caps_free(snapshot);
	outLength = kBmpSize;
	return bitmap;
}

uint32_t device_remote_frame_count() {
	return gFrameCaptureCount;
}

bool device_remote_enqueue(const DeviceRemoteCommand& command) {
	return gCommandQueue != nullptr && xQueueSend(gCommandQueue, &command, 0) == pdTRUE;
}

bool device_remote_take_command(DeviceRemoteCommand& command) {
	return gCommandQueue != nullptr && xQueueReceive(gCommandQueue, &command, 0) == pdTRUE;
}

bool device_remote_peek_command(DeviceRemoteCommand& command) {
	return gCommandQueue != nullptr && xQueuePeek(gCommandQueue, &command, 0) == pdTRUE;
}

}  // namespace app