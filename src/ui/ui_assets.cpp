#include "ui_assets.h"

#include <new>
#include <SPIFFS.h>

namespace ui {

namespace {

bool gStartupReportLogged = false;
lv_fs_drv_t gSpiffsDriver;

bool spiffsReady(lv_fs_drv_t* /*driver*/) {
	return SPIFFS.totalBytes() > 0;
}

void* spiffsOpen(lv_fs_drv_t* /*driver*/, const char* path, lv_fs_mode_t mode) {
	if (path == nullptr || (mode & LV_FS_MODE_RD) == 0) {
		return nullptr;
	}

	File* file = new (std::nothrow) File(SPIFFS.open(path, FILE_READ));
	if (file == nullptr || !*file) {
		delete file;
		return nullptr;
	}
	return file;
}

lv_fs_res_t spiffsClose(lv_fs_drv_t* /*driver*/, void* fileHandle) {
	if (fileHandle == nullptr) {
		return LV_FS_RES_INV_PARAM;
	}
	File* file = static_cast<File*>(fileHandle);
	file->close();
	delete file;
	return LV_FS_RES_OK;
}

lv_fs_res_t spiffsRead(lv_fs_drv_t* /*driver*/, void* fileHandle, void* buffer, uint32_t bytesToRead, uint32_t* bytesRead) {
	if (fileHandle == nullptr || buffer == nullptr || bytesRead == nullptr) {
		return LV_FS_RES_INV_PARAM;
	}
	File* file = static_cast<File*>(fileHandle);
	*bytesRead = static_cast<uint32_t>(file->read(static_cast<uint8_t*>(buffer), bytesToRead));
	return LV_FS_RES_OK;
}

lv_fs_res_t spiffsSeek(lv_fs_drv_t* /*driver*/, void* fileHandle, uint32_t position, lv_fs_whence_t whence) {
	if (fileHandle == nullptr) {
		return LV_FS_RES_INV_PARAM;
	}
	SeekMode mode = SeekSet;
	if (whence == LV_FS_SEEK_CUR) {
		mode = SeekCur;
	} else if (whence == LV_FS_SEEK_END) {
		mode = SeekEnd;
	}
	return static_cast<File*>(fileHandle)->seek(position, mode) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

lv_fs_res_t spiffsTell(lv_fs_drv_t* /*driver*/, void* fileHandle, uint32_t* position) {
	if (fileHandle == nullptr || position == nullptr) {
		return LV_FS_RES_INV_PARAM;
	}
	*position = static_cast<uint32_t>(static_cast<File*>(fileHandle)->position());
	return LV_FS_RES_OK;
}

const char* const kRequiredBackgrounds[] = {
	"/backgrounds/current.png",
	"/backgrounds/weekly.png",
	"/backgrounds/radar.png",
	"/backgrounds/alerts.png",
	"/backgrounds/system.png",
};

}  // namespace

void ui_asset_init() {
	static bool initialized = false;
	if (initialized) {
		return;
	}

	lv_fs_drv_init(&gSpiffsDriver);
	gSpiffsDriver.letter = 'S';
	gSpiffsDriver.ready_cb = spiffsReady;
	gSpiffsDriver.open_cb = spiffsOpen;
	gSpiffsDriver.close_cb = spiffsClose;
	gSpiffsDriver.read_cb = spiffsRead;
	gSpiffsDriver.seek_cb = spiffsSeek;
	gSpiffsDriver.tell_cb = spiffsTell;
	lv_fs_drv_register(&gSpiffsDriver);
	initialized = true;
}

bool ui_asset_exists(const char* path) {
	if (path == nullptr) {
		return false;
	}
	bool exists = SPIFFS.exists(path);
	Serial.printf("[ASSET] Exists: path=%s, result=%d\n", path, exists ? 1 : 0);
	return exists;
}

AssetLoadResult ui_asset_load_png(lv_obj_t* imgObj, const char* path) {
	AssetLoadResult result{false, false, "", ""};
	
	if (imgObj == nullptr) {
		Serial.println("[ASSET] ERROR: imgObj is null");
		result.error = "imgObj null";
		return result;
	}
	
	if (path == nullptr) {
		Serial.println("[ASSET] ERROR: path is null");
		result.error = "path null";
		return result;
	}
	
	result.path = path;
	Serial.printf("[ASSET] Loading PNG: path=%s\n", path);
	const String lvglPath = String("S:") + path;

	for (int attempt = 0; attempt < 2; ++attempt) {
		if (!SPIFFS.exists(path)) {
			if (attempt == 0) {
				Serial.printf("[ASSET] Missing file on first attempt: path=%s\n", path);
				continue;
			}
			Serial.printf("[ASSET] File not found: path=%s\n", path);
			result.error = "file_not_found";
			result.fallbackUsed = true;
			Serial.println("[ASSET] Auto-recovery engaged");
			return result;
		}

		lv_img_header_t header;
		memset(&header, 0, sizeof(header));
		const lv_res_t infoResult = lv_img_decoder_get_info(lvglPath.c_str(), &header);
		if (infoResult != LV_RES_OK) {
			Serial.printf("[LVGL] ERROR: decoder probe failed for path=%s\n", path);
			if (attempt == 0) {
				Serial.printf("[ASSET] Retry load once: path=%s\n", path);
				continue;
			}
			result.error = "decode_probe_failed";
			result.fallbackUsed = true;
			Serial.println("[ASSET] Auto-recovery engaged");
			return result;
		}

		lv_img_set_src(imgObj, lvglPath.c_str());
		const void* src = lv_img_get_src(imgObj);
		if (src != nullptr) {
			result.success = true;
			result.fallbackUsed = false;
			result.error = "";
			Serial.printf("[ASSET] Loaded OK: path=%s\n", path);
			return result;
		}

		Serial.printf("[LVGL] ERROR: lv_img_set_src failed for path=%s\n", path);
		if (attempt == 0) {
			Serial.printf("[ASSET] Retry load once: path=%s\n", path);
			continue;
		}
	}

	result.error = "src_set_failed";
	result.fallbackUsed = true;
	Serial.println("[ASSET] Auto-recovery engaged");
	return result;
}

void ui_asset_log_status(const AssetLoadResult& result) {
	Serial.printf("[ASSET] success=%d fallback=%d path=%s error=%s\n",
		result.success ? 1 : 0,
		result.fallbackUsed ? 1 : 0,
		result.path.c_str(),
		result.error.c_str());
}

void ui_asset_log_fs_health() {
	size_t total = SPIFFS.totalBytes();
	size_t used = SPIFFS.usedBytes();
	size_t available = (total > used) ? (total - used) : 0;
	Serial.printf("[FS] total=%u used=%u available=%u\n",
		static_cast<unsigned>(total),
		static_cast<unsigned>(used),
		static_cast<unsigned>(available));
}

void ui_asset_startup_report() {
	if (gStartupReportLogged) {
		return;
	}
	gStartupReportLogged = true;

	ui_asset_log_fs_health();

	size_t bgFound = 0;
	const size_t bgTotal = sizeof(kRequiredBackgrounds) / sizeof(kRequiredBackgrounds[0]);
	for (size_t i = 0; i < bgTotal; ++i) {
		if (SPIFFS.exists(kRequiredBackgrounds[i])) {
			bgFound++;
		} else {
			Serial.printf("[ASSET] MISSING background=%s\n", kRequiredBackgrounds[i]);
		}
	}

	uint8_t bootFrames = 0;
	for (uint8_t i = 0; i < 30; ++i) {
		char path[24] = {0};
		snprintf(path, sizeof(path), "/boot/frame_%03u.png", static_cast<unsigned>(i));
		if (!SPIFFS.exists(path)) {
			break;
		}
		bootFrames++;
	}

	if (bootFrames == 0) {
		Serial.println("[ASSET] MISSING boot frames under /boot/frame_000.png...");
	}

	Serial.printf("[ASSET] Startup report: backgrounds=%u/%u bootFrames=%u\n",
		static_cast<unsigned>(bgFound),
		static_cast<unsigned>(bgTotal),
		static_cast<unsigned>(bootFrames));
}

}  // namespace ui
