#include <M5Unified.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <atomic>

#include "generated_git_version.h"
#include "audio/audio_engine.h"
#include "led/led_engine.h"
#include "system/debug_log.h"
#include "system/device_remote.h"
#include "system/settings.h"
#include "system/time_manager.h"
#include "system/touch_buffer.h"
#include "system/wifi_manager.h"
#include "ui/ui_assets.h"
#include "ui/ui_boot_anim.h"
#include "ui/ui_icons.h"
#include "ui/ui_root.h"
#include "ui/ui_system.h"
#include "weather/radar_engine.h"
#include "weather/weather_api.h"
#include "web/web_server.h"

app::DebugLog gDebugLog;

// ---------------------------------------------------------------------------
// LVGL display + touch driver
// ---------------------------------------------------------------------------
namespace {

bool gRemoteTouchPressed = false;
int16_t gRemoteTouchX = 0;
int16_t gRemoteTouchY = 0;
lv_timer_t* gTouchReadTimer = nullptr;
uint32_t gFlushPixels = 0;
uint32_t gFlushCalls = 0;
uint32_t gLastFlushPixels = 0;
uint32_t gLastFlushCalls = 0;
uint32_t gLastAudioMs = 0;
uint32_t gPaintGeneration = 0;
uint8_t gPaintedPage = 0xFF;
app::TouchBuffer gPhysicalTouchBuffer;
app::TouchBuffer gTouchFeedbackBuffer;
app::TouchSample gPhysicalTouch;
uint32_t gLastTouchSampleMs = 0;
uint32_t gMaxTouchGapMs = 0;
uint32_t gMaxTouchReadMs = 0;
uint32_t gPhysicalPressCount = 0;
uint32_t gPhysicalReleaseCount = 0;
uint32_t gTouchOverflowCount = 0;
uint32_t gMaxInputDispatchMs = 0;

void SamplePhysicalTouch(bool updateController) {
  const uint32_t now = millis();
  if (updateController && now - gLastTouchSampleMs <= m5::Touch_Class::TOUCH_MIN_UPDATE_MSEC) {
    return;
  }
  if (gLastTouchSampleMs != 0 && now - gLastTouchSampleMs > gMaxTouchGapMs) {
    gMaxTouchGapMs = now - gLastTouchSampleMs;
  }
  gLastTouchSampleMs = now;
  const uint32_t readStarted = millis();
  if (updateController && M5.Touch.isEnabled()) {
    M5.Touch.update(now);
  }
  const uint32_t readMs = millis() - readStarted;
  if (readMs > gMaxTouchReadMs) {
    gMaxTouchReadMs = readMs;
  }
  app::TouchSample sample = gPhysicalTouch;
  sample.pressed = false;
  sample.feedback = app::TouchFeedback::None;
  if (M5.Touch.getCount() > 0) {
    const auto& detail = M5.Touch.getDetail(0);
    sample.pressed = detail.isPressed();
    sample.x = detail.x;
    sample.y = detail.y;
    sample.dx = detail.distanceX();
    sample.dy = detail.distanceY();
    if (detail.wasClicked()) sample.feedback = app::TouchFeedback::Tap;
    else if (detail.wasHold()) sample.feedback = app::TouchFeedback::Hold;
    else if (detail.wasFlicked() && abs(sample.dx) < abs(sample.dy)) {
      sample.feedback = sample.dy >= 0 ? app::TouchFeedback::SwipeDown : app::TouchFeedback::SwipeUp;
    }
  }
  if (sample.pressed != gPhysicalTouch.pressed) {
    if (sample.pressed) ++gPhysicalPressCount;
    else ++gPhysicalReleaseCount;
  }
  if (!gPhysicalTouchBuffer.push(sample)) {
    ++gTouchOverflowCount;
    gPhysicalTouchBuffer.clear();
    sample.pressed = false;
    sample.feedback = app::TouchFeedback::None;
    gPhysicalTouchBuffer.push(sample);
    Serial.println("[TOUCH] ERROR: sample buffer overflow; cancelling press");
  }
  gPhysicalTouch = sample;
}

// Short flush slices also provide physical touch polling opportunities during redraws.
constexpr uint32_t kLvglBufferRows = 20;
static lv_color_t sLvglBuf[320 * kLvglBufferRows];

static void LvglDisplayFlush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  static bool firstFlushLogged = false;
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  gFlushPixels += static_cast<uint32_t>(w * h);
  ++gFlushCalls;
  if (!firstFlushLogged) {
    firstFlushLogged = true;
    Serial.printf("[UI] First display flush: %ld,%ld %ldx%ld color=%04X\n",
                  static_cast<long>(area->x1), static_cast<long>(area->y1),
                  static_cast<long>(w), static_cast<long>(h),
                  static_cast<unsigned>(reinterpret_cast<uint16_t*>(color_p)[0]));
  }
  app::device_remote_capture(area, color_p);
  M5.Display.startWrite();
  M5.Display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<uint16_t*>(color_p));
  M5.Display.endWrite();
  // Acquire only: never call LVGL recursively from its display flush.
  SamplePhysicalTouch(true);
  lv_disp_flush_ready(drv);
}

static void LvglTouchRead(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
  app::DeviceRemoteCommand command;
  if (app::device_remote_peek_command(command) &&
      command.type == app::DeviceRemoteCommandType::Touch &&
      app::device_remote_take_command(command)) {
    gRemoteTouchX = command.x;
    gRemoteTouchY = command.y;
    gRemoteTouchPressed = command.pressed;
    data->state = command.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = command.x;
    data->point.y = command.y;
    // Deliver both edges of a quick tap even when they arrive in one loop.
    data->continue_reading = app::device_remote_peek_command(command) &&
        command.type == app::DeviceRemoteCommandType::Touch;
    return;
  }
  if (gRemoteTouchPressed) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = gRemoteTouchX;
    data->point.y = gRemoteTouchY;
    return;
  }
  app::TouchSample sample = gPhysicalTouch;
  if (gPhysicalTouchBuffer.pop(sample)) {
    data->continue_reading = !gPhysicalTouchBuffer.empty();
    if (sample.feedback != app::TouchFeedback::None && !gTouchFeedbackBuffer.push(sample)) {
      ++gTouchOverflowCount;
      Serial.println("[TOUCH] ERROR: feedback buffer overflow");
    }
  }
  data->state = sample.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point.x = sample.x;
  data->point.y = sample.y;
}

void InitializeLvgl() {
  Serial.println("[UI] InitializeLvgl() start");
  lv_init();
  Serial.println("[UI] lv_init() complete");
  ui::ui_asset_init();

  static lv_disp_draw_buf_t drawBuf;
  lv_disp_draw_buf_init(&drawBuf, sLvglBuf, nullptr, 320 * kLvglBufferRows);
  Serial.println("[UI] LVGL draw buffer initialized");

  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = static_cast<lv_coord_t>(M5.Display.width());
  dispDrv.ver_res = static_cast<lv_coord_t>(M5.Display.height());
  dispDrv.flush_cb = LvglDisplayFlush;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);
  Serial.println("[UI] LVGL display driver registered");

  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = LvglTouchRead;
  lv_indev_t* touchInput = lv_indev_drv_register(&indevDrv);
  if (touchInput == nullptr) {
    Serial.println("[UI] ERROR: LVGL input driver registration failed");
    return;
  }
  // Read immediately after M5.update(), not on a separate polling schedule.
  gTouchReadTimer = touchInput->driver->read_timer;
  lv_timer_pause(gTouchReadTimer);
  Serial.println("[UI] LVGL input driver registered");
}

}  // namespace

namespace {

weather::WeatherApi gWeatherApi;
weather::RadarEngine gRadarEngine;
ui::RootNavigator gUi;
ui::ThemeManager gBootTheme;
lv_obj_t* gBootScreen = nullptr;
lv_obj_t* gBootAnimObj = nullptr;
led::LedEngine gLedEngine;
audio::AudioEngine gAudioEngine;
app::SettingsStore gSettings;
app::WifiManager gWifi;
app::TimeManager gTime;
web::WebServerHost gWebServer;

bool gMainUiStarted = false;
bool gRemoteFramebufferReady = false;
bool gLedEngineInitialized = false;
bool gLastWifiConnected = false;
bool gLastErrorState = false;
String gLastAlertSignature;
uint8_t gLastPageIndex = 0xFF;
uint32_t gLastBootWhooshMs = 0;
uint32_t gLastUiContentUpdateMs = 0;
size_t gLastRadarCompletedFrames = 0;
uint32_t gLastSettingsRevision = 0;
std::atomic<bool> gSettingsApplyPending{false};
weather::WeatherApiConfig gAppliedWeatherConfig;
uint32_t gLastLoopMs = 0;
uint32_t gLastInputMs = 0;
uint32_t gLastNetworkMs = 0;
uint32_t gMaxNetworkMs = 0;
uint32_t gLastUiRadarRevision = 0;
uint32_t gLastContentMs = 0;
uint32_t gLastRenderMs = 0;

constexpr uint8_t kSystemInfoPageIndex = 5;
constexpr uint32_t kBootWhooshIntervalMs = 2200;
constexpr uint32_t kUiContentUpdateIntervalMs = 100;

ui::ThemeId CurrentThemeProvider(void* userContext) {
  (void)userContext;
  return gUi.themeId();
}

String FormatRelativeAge(uint32_t timestampMs) {
  if (timestampMs == 0) {
    return String();
  }

  const uint32_t ageSeconds = (millis() - timestampMs) / 1000UL;
  if (ageSeconds < 60UL) {
    return String(ageSeconds) + "s ago";
  }

  const uint32_t ageMinutes = ageSeconds / 60UL;
  if (ageMinutes < 60UL) {
    return String(ageMinutes) + "m ago";
  }

  const uint32_t ageHours = ageMinutes / 60UL;
  return String(ageHours) + "h ago";
}

String FormatSpiffsUsage() {
  const size_t totalBytes = SPIFFS.totalBytes();
  if (totalBytes == 0U) {
    return String();
  }

  const size_t usedBytes = SPIFFS.usedBytes();
  const uint32_t usedPercent = static_cast<uint32_t>((usedBytes * 100U + (totalBytes / 2U)) / totalBytes);
  return String(static_cast<unsigned>(usedBytes / 1024U)) + " / " +
         String(static_cast<unsigned>(totalBytes / 1024U)) + " KB (" +
         String(usedPercent) + "%)";
}

String FormatCurrentClock(const WeatherData& data) {
  time_t now = time(nullptr);
  if (now > 1700000000) {
    now += static_cast<time_t>(data.timezoneOffsetMinutes * 60);
    struct tm timeInfo;
    gmtime_r(&now, &timeInfo);
    char buffer[16];
    strftime(buffer, sizeof(buffer), "%I:%M %p", &timeInfo);
    String value(buffer);
    if (value.length() > 0 && value[0] == '0') {
      value.remove(0, 1);
    }
    return value;
  }

  if (data.currentLocalMinutes < 0) {
    return "--:--";
  }

  const uint32_t baseMs = data.currentFetchedAtMs != 0 ? data.currentFetchedAtMs : data.lastCycleAtMs;
  const uint32_t elapsedSeconds = baseMs == 0 ? 0 : ((millis() - baseMs) / 1000UL);
  int totalMinutes = data.currentLocalMinutes + static_cast<int>(elapsedSeconds / 60UL);
  totalMinutes %= (24 * 60);
  if (totalMinutes < 0) {
    totalMinutes += (24 * 60);
  }

  const int hour24 = totalMinutes / 60;
  const int minute = totalMinutes % 60;
  const bool isPm = hour24 >= 12;
  int hour12 = hour24 % 12;
  if (hour12 == 0) {
    hour12 = 12;
  }

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d:%02d %s", hour12, minute, isPm ? "PM" : "AM");
  return String(buffer);
}

uint8_t WifiSignalBars(bool connected, int32_t rssi) {
  if (!connected) {
    return 0;
  }
  if (rssi >= -55) {
    return 4;
  }
  if (rssi >= -67) {
    return 3;
  }
  if (rssi >= -75) {
    return 2;
  }
  return 1;
}

int ClampBatteryPercent(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return value;
}

void BuildPreviewPayload(void* userContext, JsonDocument& doc) {
  (void)userContext;
  const WeatherData& data = gWeatherApi.data();
  doc["wifiConnected"] = gWifi.connected();
  doc["weatherError"] = static_cast<int>(data.lastError);
  doc["radarError"] = static_cast<int>(gRadarEngine.lastError());
  doc["radarFrames"] = gRadarEngine.frameCount();
  doc["audioPlaying"] = gAudioEngine.isPlaying();
}

void OnAudioEvent(void* userContext, const char* eventName, const String& details) {
  (void)userContext;
  if (eventName == nullptr || strlen(eventName) == 0) {
    return;
  }
  gDebugLog.log("audio", String(eventName) + (details.length() ? String(" ") + details : String()));
}

void OnLedEvent(void* userContext, const char* eventName, const String& details) {
  (void)userContext;
  if (eventName == nullptr) {
    return;
  }
  gDebugLog.log("led", String(eventName) + (details.length() ? String(" ") + details : String()));
}

void OnSettingsSaved(void* userContext, const app::AppSettings& settings) {
  (void)userContext;
  (void)settings;
  gSettingsApplyPending.store(true);
}

void ApplySavedSettings(const app::AppSettings& settings) {
  weather::WeatherApiConfig apiCfg;
  apiCfg.apiKey = settings.apiKey;
  apiCfg.locationQuery = settings.locationQuery;
  apiCfg.locationKey = settings.locationKey;
  apiCfg.useMetric = true;
  apiCfg.radarCacheMs = 15UL * 60UL * 1000UL;
  if (apiCfg.apiKey != gAppliedWeatherConfig.apiKey ||
      apiCfg.locationQuery != gAppliedWeatherConfig.locationQuery ||
      apiCfg.locationKey != gAppliedWeatherConfig.locationKey) {
    gAppliedWeatherConfig = apiCfg;
    gWeatherApi.begin(apiCfg);
    gWeatherApi.requestRefresh();
    gRadarEngine.reset();
  }
  if (gMainUiStarted && gUi.themeId() != settings.theme) {
    gUi.setTheme(settings.theme);
  } else if (!gMainUiStarted) {
    gBootTheme.setTheme(settings.theme);
  }
  gAudioEngine.playSystemSound(audio::SystemSound::SettingsSavedTone);
}

void InitializeLedEngineAfterBoot() {
  if (gLedEngineInitialized) {
    return;
  }
  Serial.println("[LED] InitializeLedEngineAfterBoot: begin");
  gLedEngine.begin(160);
  Serial.println("[LED] InitializeLedEngineAfterBoot: calling selfTestBottom3()");
  gLedEngine.selfTestBottom3();
  Serial.println("[LED] InitializeLedEngineAfterBoot: selfTestBottom3() returned");
  gLedEngineInitialized = true;
  Serial.println("[LED] LED engine initialized");
}

void UpdateTouchFeedback() {
  app::TouchSample sample;
  while (gTouchFeedbackBuffer.pop(sample)) {
    switch (sample.feedback) {
      case app::TouchFeedback::Tap:
        gLedEngine.touchEvent(led::LedEngine::TouchKind::Tap, sample.dx, sample.dy);
        gAudioEngine.playTouchSound(audio::TouchSound::TapClick);
        break;
      case app::TouchFeedback::Hold:
        gLedEngine.touchEvent(led::LedEngine::TouchKind::LongPress, sample.dx, sample.dy);
        gAudioEngine.playTouchSound(audio::TouchSound::LongPressRise);
        break;
      case app::TouchFeedback::SwipeUp:
      case app::TouchFeedback::SwipeDown:
        gLedEngine.touchEvent(sample.feedback == app::TouchFeedback::SwipeDown ?
            led::LedEngine::TouchKind::SwipeDown : led::LedEngine::TouchKind::SwipeUp, sample.dx, sample.dy);
        gAudioEngine.playTouchSound(audio::TouchSound::SwipeWhoosh);
        break;
      case app::TouchFeedback::None: break;
    }
  }
}

void UpdateAlertFeedback(const WeatherData& data) {
  if (data.alertCount == 0) {
    gLedEngine.clearAlert();
    gLastAlertSignature = "";
    return;
  }

  const WeatherAlert& alert = data.alerts[0];
  const String severity = alert.severity;
  String signature = alert.id + "|" + severity + "|" + alert.title + "|" + String(alert.onsetEpoch) + "|" + alert.description;
  if (signature == gLastAlertSignature) {
    return;
  }
  gLastAlertSignature = signature;

  String lower = severity;
  lower.toLowerCase();

  if (lower.indexOf("tornado") >= 0 || lower.indexOf("extreme") >= 0) {
    gLedEngine.alert(led::LedEngine::AlertLevel::Critical, severity);
    gAudioEngine.playAlertSound(audio::AlertSound::TornadoWarning);
    return;
  }

  if (lower.indexOf("flood") >= 0) {
    gLedEngine.alert(led::LedEngine::AlertLevel::Warning, severity, 0xFFFFFFFFUL);
    gAudioEngine.playAlertSound(audio::AlertSound::FloodWarning);
    return;
  }

  gLedEngine.alert(led::LedEngine::AlertLevel::Warning, severity, 0xFFFFFFFFUL);
  gAudioEngine.playAlertSound(audio::AlertSound::SevereWeather);
}

void ProcessRemoteCommands() {
  app::DeviceRemoteCommand command;
  while (app::device_remote_peek_command(command)) {
    if (command.type == app::DeviceRemoteCommandType::Touch) {
      break;
    }
    if (!app::device_remote_take_command(command)) {
      break;
    }
    switch (command.type) {
      case app::DeviceRemoteCommandType::Page:
        if (!gMainUiStarted || command.value > 6) {
          break;
        }
        if (command.value == 6) {
          gUi.showHome();
        } else {
          gUi.openPage(command.value, true);
        }
        break;
      case app::DeviceRemoteCommandType::Touch:
        break;
      case app::DeviceRemoteCommandType::Sound:
        gAudioEngine.playBootSound(audio::BootSound::Ready);
        break;
    }
  }
}

void PrintLabelLayout(lv_obj_t* object) {
  if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }
  if (lv_obj_check_type(object, &lv_label_class)) {
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    Serial.printf("[LAYOUT] %d,%d %dx%d zoom=%d text=%s\n",
                  area.x1, area.y1, lv_area_get_width(&area), lv_area_get_height(&area),
                  lv_obj_get_style_transform_zoom(object, LV_PART_MAIN), lv_label_get_text(object));
  }
  for (uint32_t index = 0; index < lv_obj_get_child_cnt(object); ++index) {
    PrintLabelLayout(lv_obj_get_child(object, index));
  }
}

void ProcessSerialCaptureCommand() {
  static char commandBuffer[512] = {};
  static size_t commandLength = 0;
  static bool commandOverflow = false;

  while (Serial.available() > 0) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\r') {
      continue;
    }
    if (character != '\n') {
      if (commandLength < sizeof(commandBuffer) - 1) {
        commandBuffer[commandLength++] = character;
      } else {
        commandOverflow = true;
      }
      continue;
    }

    commandBuffer[commandLength] = '\0';
    if (commandOverflow) {
      Serial.println("CONTROL_ERROR command too long");
    } else if (strcmp(commandBuffer, "RESTART") == 0) {
      Serial.println("CONTROL_OK restarting");
      Serial.flush();
      ESP.restart();
    } else if (strcmp(commandBuffer, "WIFI_SCAN") == 0) {
      Serial.println(gWifi.startScan() ? "CONTROL_OK scan started" : "CONTROL_ERROR scan could not start");
    } else if (strcmp(commandBuffer, "WIFI_NETWORKS") == 0) {
      app::WifiNetworkInfo networks[app::WifiManager::kMaxScanResults];
      const size_t count = gWifi.scanNetworks(networks, app::WifiManager::kMaxScanResults);
      JsonDocument results;
      results["inProgress"] = gWifi.scanInProgress();
      JsonArray items = results["networks"].to<JsonArray>();
      for (size_t index = 0; index < count; ++index) {
        JsonObject item = items.add<JsonObject>();
        item["ssid"] = networks[index].ssid;
        item["rssi"] = networks[index].rssi;
      }
      Serial.print("WIFI_NETWORKS ");
      serializeJson(results, Serial);
      Serial.println();
    } else if (strncmp(commandBuffer, "WIFI ", 5) == 0) {
      JsonDocument credentials;
      const DeserializationError error = deserializeJson(credentials, commandBuffer + 5);
      if (error || !credentials["ssid"].is<const char*>() ||
          !credentials["password"].is<const char*>()) {
        Serial.println("CONTROL_ERROR WIFI requires JSON string fields ssid and password");
      } else {
        app::WifiConfig config = gWifi.config();
        config.ssid = credentials["ssid"].as<String>();
        config.ssid.trim();
        config.password = credentials["password"].as<String>();
        config.autoConnect = true;
        if (config.ssid.length() == 0 || config.ssid.length() > 32 ||
            (config.password.length() != 0 &&
             (config.password.length() < 8 || config.password.length() > 63))) {
          Serial.println("CONTROL_ERROR invalid WiFi credential lengths");
        } else if (!gSettings.saveWifiSettings(config.ssid, config.password, true)) {
          Serial.println("CONTROL_ERROR could not persist WiFi settings");
        } else if (!gWifi.applyConfig(config, true, true)) {
          Serial.println("CONTROL_ERROR could not apply WiFi configuration");
        } else {
          Serial.println("CONTROL_OK WiFi connection started");
        }
      }
    } else if (strcmp(commandBuffer, "SCREEN_BMP") == 0) {
      size_t imageLength = 0;
      uint8_t* image = app::device_remote_create_bmp(imageLength);
      if (image == nullptr || imageLength == 0) {
        Serial.println("SCREEN_ERROR");
        if (image != nullptr) {
          heap_caps_free(image);
        }
      } else {
        Serial.printf("SCREEN_BMP %u %lu\n", static_cast<unsigned>(imageLength),
                static_cast<unsigned long>(app::device_remote_frame_count()));
        Serial.flush();
        size_t sent = 0;
        while (sent < imageLength) {
          sent += Serial.write(image + sent, imageLength - sent);
        }
        Serial.flush();
        heap_caps_free(image);
      }
    } else if (strcmp(commandBuffer, "UI_LABELS") == 0) {
      PrintLabelLayout(lv_scr_act());
      Serial.println("LAYOUT_END");
    } else if (strcmp(commandBuffer, "STATUS") == 0) {
      const app::WifiStatusInfo wifi = gWifi.statusInfo();
      Serial.printf("DEVICE_STATUS ready=%u page=%u ip=%s ap=%s heap=%u psram=%u loop=%u input=%u network=%u content=%u render=%u theme=%u audio=%u speaker=%u pixels=%u flushes=%u audio_ms=%u painted_page=%u paint=%u network_max=%u radar_busy=%u radar_frames=%u radar_error=%u\n",
                    gMainUiStarted ? 1U : 0U, static_cast<unsigned>(gUi.activePageIndex()),
                    wifi.ipAddress.c_str(), wifi.accessPointIpAddress.c_str(),
                    static_cast<unsigned>(ESP.getFreeHeap()),
                    static_cast<unsigned>(ESP.getFreePsram()),
                    static_cast<unsigned>(gLastLoopMs), static_cast<unsigned>(gLastInputMs),
                    static_cast<unsigned>(gLastNetworkMs), static_cast<unsigned>(gLastContentMs),
                    static_cast<unsigned>(gLastRenderMs),
                    static_cast<unsigned>(gSettings.get_theme()), gAudioEngine.isPlaying() ? 1U : 0U,
                    M5.Speaker.isPlaying() ? 1U : 0U,
                    static_cast<unsigned>(gLastFlushPixels), static_cast<unsigned>(gLastFlushCalls),
                    static_cast<unsigned>(gLastAudioMs), static_cast<unsigned>(gPaintedPage),
                    static_cast<unsigned>(gPaintGeneration),
                    static_cast<unsigned>(gMaxNetworkMs), gRadarEngine.isDownloading() ? 1U : 0U,
                    static_cast<unsigned>(gRadarEngine.completedFrameCount()),
                    static_cast<unsigned>(gRadarEngine.lastError()));
    } else if (strcmp(commandBuffer, "TOUCH_STATUS") == 0) {
      Serial.printf("TOUCH_STATUS pressed=%u x=%d y=%d presses=%u releases=%u gap_max=%u read_max=%u dispatch_max=%u overflows=%u\n",
                    gPhysicalTouch.pressed ? 1U : 0U, gPhysicalTouch.x, gPhysicalTouch.y,
                    static_cast<unsigned>(gPhysicalPressCount), static_cast<unsigned>(gPhysicalReleaseCount),
                    static_cast<unsigned>(gMaxTouchGapMs), static_cast<unsigned>(gMaxTouchReadMs),
                    static_cast<unsigned>(gMaxInputDispatchMs), static_cast<unsigned>(gTouchOverflowCount));
    } else if (strcmp(commandBuffer, "TOUCH_RESET") == 0) {
      gPhysicalPressCount = 0;
      gPhysicalReleaseCount = 0;
      gMaxTouchGapMs = 0;
      gMaxTouchReadMs = 0;
      gMaxInputDispatchMs = 0;
      gTouchOverflowCount = 0;
      Serial.println("CONTROL_OK touch diagnostics reset");
    } else if (strcmp(commandBuffer, "RADAR_REFRESH") == 0) {
      if (!gMainUiStarted || !gWifi.connected() || gWeatherApi.data().radarFrameCount == 0) {
        Serial.println("CONTROL_ERROR radar refresh requires WiFi and radar metadata");
      } else {
        gRadarEngine.reset();
        gMaxNetworkMs = 0;
        Serial.println("CONTROL_OK radar refresh started");
      }
    } else if (strcmp(commandBuffer, "SOUND") == 0 || strncmp(commandBuffer, "PAGE ", 5) == 0 ||
               strncmp(commandBuffer, "TOUCH ", 6) == 0) {
      app::DeviceRemoteCommand command;
      int page = -1;
      int x = -1;
      int y = -1;
      int pressed = -1;
      int consumed = 0;
      bool valid = false;
      if (strcmp(commandBuffer, "SOUND") == 0) {
        command.type = app::DeviceRemoteCommandType::Sound;
        valid = true;
      } else if (sscanf(commandBuffer, "PAGE %d%n", &page, &consumed) == 1 &&
                 commandBuffer[consumed] == '\0' && page >= 0 && page <= 6) {
        command.type = app::DeviceRemoteCommandType::Page;
        command.value = static_cast<uint8_t>(page);
        valid = gMainUiStarted;
      } else if (sscanf(commandBuffer, "TOUCH %d %d %d%n", &x, &y, &pressed, &consumed) == 3 &&
                 commandBuffer[consumed] == '\0' && x >= 0 && x < 320 && y >= 0 && y < 240 &&
                 (pressed == 0 || pressed == 1)) {
        command.type = app::DeviceRemoteCommandType::Touch;
        command.x = static_cast<int16_t>(x);
        command.y = static_cast<int16_t>(y);
        command.pressed = pressed == 1;
        valid = gMainUiStarted;
      }
      if (!valid) {
        Serial.println("CONTROL_ERROR invalid command or UI not ready");
      } else if (!app::device_remote_enqueue(command)) {
        Serial.println("CONTROL_ERROR queue unavailable or full");
      } else {
        Serial.println("CONTROL_OK");
      }
    } else if (commandLength > 0) {
      Serial.println("CONTROL_ERROR unknown command");
    }
    commandLength = 0;
    commandOverflow = false;
    memset(commandBuffer, 0, sizeof(commandBuffer));
  }
}

void OnRadarProgress(void* /*userContext*/, const weather::RadarProgress& progress) {
  if (progress.totalFrames > 0) {
    const uint32_t rawPercent = (progress.completedFrames * 100U) / progress.totalFrames;
    const uint8_t percent = rawPercent > 100U ? 100U : static_cast<uint8_t>(rawPercent);
    gLedEngine.progress(percent, progress.completedFrames < progress.totalFrames);
  } else {
    gLedEngine.progress(0, false);
  }

  if (gMainUiStarted) {
    gUi.setRadarProgress(progress.completedFrames, progress.totalFrames, progress.stage);
  }
}

void MaybeStartRadarDownload() {
  const bool radarChanged = gWeatherApi.consumeRadarChanged();
  const WeatherData& data = gWeatherApi.data();
  if ((!radarChanged && gRadarEngine.frameCount() > 0) || gRadarEngine.isDownloading()) {
    return;
  }
  if (data.radarFrameCount < weather::RadarEngine::kMinFrameCount) {
    return;
  }

  String urls[weather::RadarEngine::kMaxFrameCount];
  uint32_t epochs[weather::RadarEngine::kMaxFrameCount] = {};
  const size_t availableCount = gWeatherApi.copyRadarFrames(urls, epochs, weather::RadarEngine::kMaxFrameCount);
  const size_t count = availableCount > 0 ? 1 : 0;
  if (count < 1) {
    return;
  }

  urls[0] = urls[availableCount - 1];
  epochs[0] = epochs[availableCount - 1];

  weather::RadarDownloadConfig config;
  config.storageMode = weather::RadarStorageMode::RamOnly;
  config.expectedFormat = weather::RadarFrameFormat::EncodedPng;
  config.expectedWidth = 256;
  config.expectedHeight = 256;
  config.ramBudgetBytes = 192 * 1024;
  config.baseMapUrl = data.radarMapUrl;
  config.connectTimeoutMs = 1500;
  config.readTimeoutMs = 1500;

  weather::RadarVisualConfig visuals;
  visuals.enableFrameInterpolation = false;
  visuals.enableAutoContrast = false;
  visuals.enableStormCellOverlays = false;
  visuals.interpolationSteps = 0;
  visuals.smoothingPasses = 0;

  gRadarEngine.setVisualConfig(visuals);
  gRadarEngine.setAnimationFps(3.0f);
  gRadarEngine.startDownload(urls, epochs, count, config);
}

void UpdateSystemEventAudio() {
  const bool wifiConnected = gWifi.connected();
  if (wifiConnected != gLastWifiConnected) {
    gAudioEngine.onWiFiEvent(wifiConnected ? "connected" : "disconnected");
    gLastWifiConnected = wifiConnected;
  }

  if (gSettings.revision() != gLastSettingsRevision) {
    gLastSettingsRevision = gSettings.revision();
    gAudioEngine.onSettingsChanged();
  }

  const bool hasError = gWeatherApi.hasAnyError() || (gRadarEngine.lastError() != weather::RadarEngineError::None);
  if (hasError && !gLastErrorState) {
    gAudioEngine.playSystemSound(audio::SystemSound::ErrorBuzz);
  }
  gLastErrorState = hasError;
}

void InitializeSubsystems() {
  Serial.println("[UI] InitializeSubsystems() start");
  SPIFFS.begin(true);
  ui::ui_asset_startup_report();
  ui::ui_icon_startup_report();

  gSettings.begin();
  gDebugLog.begin(gSettings.settings().debugMode);
  gLastSettingsRevision = gSettings.revision();

  gLedEngine.setEventCallback(OnLedEvent, nullptr);

  gAudioEngine.begin();
  gAudioEngine.setEventCallback(OnAudioEvent, nullptr);
  gAudioEngine.setMasterVolume(200);

  app::WifiConfig wifiCfg;
  wifiCfg.ssid = gSettings.settings().wifiSsid;
  wifiCfg.password = gSettings.settings().wifiPassword;
  wifiCfg.autoConnect = gSettings.settings().wifiAutoConnect;
  gWifi.begin(wifiCfg);
  gLastWifiConnected = gWifi.connected();
  const app::WifiStatusInfo startupWifiInfo = gWifi.statusInfo();
  Serial.printf("[REMOTE] WiFi=%s IP=%s setupSSID=%s setupIP=%s\n",
                startupWifiInfo.statusText.c_str(),
                startupWifiInfo.ipAddress.c_str(),
                startupWifiInfo.accessPointSsid.c_str(),
                startupWifiInfo.accessPointIpAddress.c_str());

  gTime.begin();

  // LVGL must be initialised before any UI widget is created.
  Serial.println("[UI] Calling InitializeLvgl()");
  InitializeLvgl();
  Serial.println("[UI] InitializeLvgl() finished");

  // Boot animation gets its own screen — shown immediately.
  gBootTheme.begin(gSettings.get_theme());
  gBootScreen = lv_obj_create(nullptr);
  lv_obj_remove_style_all(gBootScreen);
  lv_obj_set_style_bg_opa(gBootScreen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(gBootScreen, lv_color_hex(0x030913), LV_PART_MAIN);
  lv_obj_set_style_border_width(gBootScreen, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(gBootScreen, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(gBootScreen, 0, LV_PART_MAIN);
  gBootAnimObj = ui::ui_boot_anim_play(gBootScreen, gBootTheme, nullptr, nullptr);
  lv_disp_load_scr(gBootScreen);
  gAudioEngine.playBootSound(audio::BootSound::Whoosh);
  gLastBootWhooshMs = millis();
  // Main UI is created after boot animation finishes (see loop()).

  weather::WeatherApiConfig apiCfg;
  apiCfg.apiKey = gSettings.settings().apiKey;
  apiCfg.locationQuery = gSettings.settings().locationQuery;
  apiCfg.locationKey = gSettings.settings().locationKey;
  apiCfg.useMetric = true;
  apiCfg.radarCacheMs = 15UL * 60UL * 1000UL;
  gWeatherApi.begin(apiCfg);
  gAppliedWeatherConfig = apiCfg;

  gRadarEngine.begin();
  gRadarEngine.setProgressCallback(OnRadarProgress, nullptr);
  gLastRadarCompletedFrames = gRadarEngine.completedFrameCount();

  gWebServer.begin(gSettings,
                   gWifi,
                   &gDebugLog,
                   CurrentThemeProvider,
                   nullptr,
                   BuildPreviewPayload,
                   nullptr,
                   OnSettingsSaved,
                   nullptr);
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  cfg.output_power = true;
  cfg.internal_spk = true;
  cfg.internal_mic = true;
  cfg.led_brightness = 0;
  M5.begin(cfg);
  gRemoteFramebufferReady = app::device_remote_begin();
  Serial.printf("[REMOTE] framebuffer=%s\n", gRemoteFramebufferReady ? "ready" : "unavailable");
  Serial.println("[UI] M5.begin() complete");

  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  }
  M5.Display.setBrightness(128);
  M5.Display.setSwapBytes(true);
  Serial.println("[UI] Display initialized, brightness set, RGB565 swap enabled");

  InitializeSubsystems();
}

void loop() {
  const uint32_t loopStarted = millis();
  M5.update();
  gLastInputMs = millis() - loopStarted;
  if (gLastInputMs > gMaxTouchReadMs) {
    gMaxTouchReadMs = gLastInputMs;
  }
  SamplePhysicalTouch(false);
  ProcessRemoteCommands();
  ProcessSerialCaptureCommand();
  if (gTouchReadTimer != nullptr) {
    const uint32_t dispatchStarted = millis();
    lv_indev_read_timer_cb(gTouchReadTimer);
    const uint32_t dispatchMs = millis() - dispatchStarted;
    if (dispatchMs > gMaxInputDispatchMs) {
      gMaxInputDispatchMs = dispatchMs;
    }
  }

  if (!gMainUiStarted) {
    if (ui::ui_boot_anim_finished(gBootAnimObj)) {
      InitializeLedEngineAfterBoot();
      gMainUiStarted = true;
      // Create the main UI on its own screen and switch to it.
      lv_obj_t* mainScreen = lv_obj_create(nullptr);
      ui::ui_make_transparent(mainScreen);
      gUi.begin(mainScreen, gSettings);
      lv_disp_load_scr(mainScreen);
    } else {
      ui::ui_boot_anim_update(gBootAnimObj);
      const uint32_t now = millis();
      if (!gAudioEngine.isPlaying() && (now - gLastBootWhooshMs) >= kBootWhooshIntervalMs) {
        gAudioEngine.playBootSound(audio::BootSound::Whoosh);
        gLastBootWhooshMs = now;
      }
      gAudioEngine.update();
      lv_timer_handler();
      delay(16);
      return;
    }
  }

  const uint32_t networkStarted = millis();
  if (gSettingsApplyPending.exchange(false)) {
    ApplySavedSettings(gSettings.settings());
  }
  gWifi.update();
  gTime.update(gWifi.connected());
  gWeatherApi.update();
  MaybeStartRadarDownload();
  gRadarEngine.tick();
  if (!gRadarEngine.isDownloading()) {
    gLedEngine.progress(0, false);
  }
  gWebServer.tick();
  gLastNetworkMs = millis() - networkStarted;
  if (gLastNetworkMs > gMaxNetworkMs) {
    gMaxNetworkMs = gLastNetworkMs;
  }

  const uint32_t contentStarted = millis();
  UpdateTouchFeedback();
  UpdateSystemEventAudio();

  const WeatherData& weatherData = gWeatherApi.data();
  const uint32_t uiNowMs = millis();
  if ((uiNowMs - gLastUiContentUpdateMs) >= kUiContentUpdateIntervalMs ||
      gUi.activePageIndex() != gLastPageIndex ||
      gRadarEngine.displayRevision() != gLastUiRadarRevision) {
    gLastUiContentUpdateMs = uiNowMs;
    gLastUiRadarRevision = gRadarEngine.displayRevision();
    ui::SystemInfo systemInfo;
    const app::WifiStatusInfo wifiInfo = gWifi.statusInfo();
    const String primaryIp = wifiInfo.ipAddress.length() > 0 ? wifiInfo.ipAddress : wifiInfo.accessPointIpAddress;
    systemInfo.wifiConnected = wifiInfo.connected;
    systemInfo.wifiSignalBars = WifiSignalBars(wifiInfo.connected, wifiInfo.rssi);
    systemInfo.batteryPct = ClampBatteryPercent(M5.Power.getBatteryLevel());
    systemInfo.batteryCharging = static_cast<bool>(M5.Power.isCharging());
    systemInfo.currentTime = FormatCurrentClock(weatherData);
    systemInfo.ipAddress = primaryIp;
    systemInfo.webUiUrl = primaryIp.length() > 0 ? String("http://") + primaryIp + ":80" : String();
    systemInfo.wifiSsid = wifiInfo.connected ? wifiInfo.ssid : (wifiInfo.accessPointSsid.length() > 0 ? wifiInfo.accessPointSsid : wifiInfo.ssid);
    systemInfo.wifiRssi = wifiInfo.connected ? String(wifiInfo.rssi) + " dBm" : String();
    systemInfo.wifiStatus = wifiInfo.statusText;
    systemInfo.weatherStatus = weatherData.lastErrorMessage.length() > 0 ? weatherData.lastErrorMessage : String(weatherData.current.valid ? "Ready" : "Waiting for first update");
    systemInfo.radarStatus = gRadarEngine.lastErrorMessage().length() > 0 ? gRadarEngine.lastErrorMessage() : String(gRadarEngine.frameCount() > 0 ? "Ready" : "Waiting for radar frames");
    systemInfo.lastUpdate = FormatRelativeAge(weatherData.lastCycleAtMs);
    systemInfo.spiffsUsage = FormatSpiffsUsage();
    systemInfo.firmwareVersion = APP_GIT_VERSION;
    systemInfo.ledMode = gLedEngine.statusLabel();

    gUi.update(weatherData, gRadarEngine, systemInfo);
    gLedEngine.updateWeatherMood(weatherData);
  }

  const uint8_t activePage = gUi.activePageIndex();
  if (activePage != gLastPageIndex) {
    gLedEngine.pageTransition(gLastPageIndex == 0xFF ? activePage : gLastPageIndex, activePage);
    gAudioEngine.onPageTransition(gLastPageIndex == 0xFF ? activePage : gLastPageIndex, activePage);
    if (activePage == kSystemInfoPageIndex) {
      gAudioEngine.playSwipeSound(audio::SwipeSound::SystemInfoEnterTone);
    }
    gLastPageIndex = activePage;
  }

  UpdateAlertFeedback(weatherData);

  const size_t completedFrames = gRadarEngine.completedFrameCount();
  if (completedFrames != gLastRadarCompletedFrames) {
    gLastRadarCompletedFrames = completedFrames;
    gAudioEngine.playAlertSound(audio::AlertSound::RadarUpdate);
  }

  const bool warningState = gWeatherApi.hasAnyError() || (gRadarEngine.lastError() != weather::RadarEngineError::None);
  gLedEngine.setSystemStatus(!gMainUiStarted, gWifi.connected(), warningState);
  gLedEngine.update();

  gLastContentMs = millis() - contentStarted;
  const uint32_t renderStarted = millis();
  gAudioEngine.update();
  gLastAudioMs = millis() - renderStarted;
  gFlushPixels = 0;
  gFlushCalls = 0;
  lv_timer_handler();
  gLastRenderMs = millis() - renderStarted - gLastAudioMs;
  gLastFlushPixels = gFlushPixels;
  gLastFlushCalls = gFlushCalls;
  if (gFlushCalls != 0) {
    gPaintedPage = gUi.isPageFullyVisible() ? gUi.activePageIndex() : 0xFF;
    ++gPaintGeneration;
  }
  gLastLoopMs = millis() - loopStarted;

  delay(16);
}
