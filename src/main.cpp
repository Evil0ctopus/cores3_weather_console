#include <M5Unified.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include <time.h>

#include "generated_git_version.h"
#include "audio/audio_engine.h"
#include "led/led_engine.h"
#include "system/debug_log.h"
#include "system/settings.h"
#include "system/time_manager.h"
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

// Draw buffer: 20 rows of 320 pixels at 16-bit colour = 12.5 KB
static lv_color_t sLvglBuf[320 * 20];

static void LvglDisplayFlush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  M5.Display.startWrite();
  M5.Display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<uint16_t*>(color_p));
  M5.Display.endWrite();
  lv_disp_flush_ready(drv);
}

static void LvglTouchRead(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
  if (M5.Touch.getCount() > 0) {
    const auto& t = M5.Touch.getDetail(0);
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = t.x;
    data->point.y = t.y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void InitializeLvgl() {
  Serial.println("[UI] InitializeLvgl() start");
  lv_init();
  Serial.println("[UI] lv_init() complete");

  static lv_disp_draw_buf_t drawBuf;
  lv_disp_draw_buf_init(&drawBuf, sLvglBuf, nullptr, 320 * 20);
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
  lv_indev_drv_register(&indevDrv);
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
bool gLedEngineInitialized = false;
bool gLastWifiConnected = false;
bool gLastErrorState = false;
uint8_t gLastPageIndex = 0xFF;
uint32_t gLastBootWhooshMs = 0;
size_t gLastRadarCompletedFrames = 0;
uint32_t gLastSettingsRevision = 0;

constexpr uint8_t kSystemInfoPageIndex = 5;
constexpr uint32_t kBootWhooshIntervalMs = 2200;

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
  if (strcmp(eventName, "alert") == 0) {
    gAudioEngine.onLedAlert(1);
  }
  gDebugLog.log("led", String(eventName) + (details.length() ? String(" ") + details : String()));
}

void OnSettingsSaved(void* userContext, const app::AppSettings& settings) {
  (void)userContext;
  weather::WeatherApiConfig apiCfg;
  apiCfg.apiKey = settings.apiKey;
  apiCfg.locationQuery = settings.locationQuery;
  apiCfg.locationKey = settings.locationKey;
  apiCfg.useMetric = (settings.units == app::UnitsSystem::Metric);
  apiCfg.radarCacheMs = 15UL * 60UL * 1000UL;
  gWeatherApi.begin(apiCfg);
  gWeatherApi.requestRefresh();
  gRadarEngine.reset();
  if (gMainUiStarted) {
    gUi.setTheme(settings.theme);
  } else {
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
  static bool wasTouchActive = false;
  static bool swipeHandled = false;
  static int lastDx = 0;
  static int lastDy = 0;

  if (M5.Touch.getCount() == 0) {
    if (wasTouchActive) {
      if (!swipeHandled && abs(lastDx) >= 24 && abs(lastDx) > abs(lastDy) + 6) {
        gUi.moveToAdjacentPage(lastDx < 0 ? 1 : -1, true);
        gLedEngine.touchEvent(lastDx >= 0 ? led::LedEngine::TouchKind::SwipeRight : led::LedEngine::TouchKind::SwipeLeft, lastDx, lastDy);
        gAudioEngine.playTouchSound(audio::TouchSound::SwipeWhoosh);
      }
      gUi.recenterActivePage(true);
    }
    wasTouchActive = false;
    swipeHandled = false;
    lastDx = 0;
    lastDy = 0;
    return;
  }

  wasTouchActive = true;
  const m5::Touch_Class::touch_detail_t& detail = M5.Touch.getDetail(0);
  lastDx = detail.distanceX();
  lastDy = detail.distanceY();

  if (detail.wasClicked()) {
    gLedEngine.touchEvent(led::LedEngine::TouchKind::Tap, lastDx, lastDy);
    gAudioEngine.playTouchSound(audio::TouchSound::TapClick);
    return;
  }

  if (detail.wasHold()) {
    gLedEngine.touchEvent(led::LedEngine::TouchKind::LongPress, lastDx, lastDy);
    gAudioEngine.playTouchSound(audio::TouchSound::LongPressRise);
    return;
  }

  if (!swipeHandled && abs(lastDx) >= 32 && abs(lastDx) > abs(lastDy) + 6) {
    gUi.moveToAdjacentPage(lastDx < 0 ? 1 : -1, true);
    gLedEngine.touchEvent(lastDx >= 0 ? led::LedEngine::TouchKind::SwipeRight : led::LedEngine::TouchKind::SwipeLeft, lastDx, lastDy);
    gAudioEngine.playTouchSound(audio::TouchSound::SwipeWhoosh);
    swipeHandled = true;
    return;
  }

  if (detail.wasFlicked()) {
    if (abs(lastDx) < abs(lastDy)) {
      gLedEngine.touchEvent(lastDy >= 0 ? led::LedEngine::TouchKind::SwipeDown : led::LedEngine::TouchKind::SwipeUp, lastDx, lastDy);
      gAudioEngine.playTouchSound(audio::TouchSound::SwipeWhoosh);
    }
  }
}

void UpdateAlertFeedback(const WeatherData& data) {
  if (data.alertCount == 0) {
    gLedEngine.clearAlert();
    return;
  }

  const String severity = data.alerts[0].severity;
  String lower = severity;
  lower.toLowerCase();

  if (lower.indexOf("tornado") >= 0 || lower.indexOf("extreme") >= 0) {
    gLedEngine.alert(led::LedEngine::AlertLevel::Critical, severity);
    gAudioEngine.playAlertSound(audio::AlertSound::TornadoWarning);
    return;
  }

  if (lower.indexOf("flood") >= 0) {
    gLedEngine.alert(led::LedEngine::AlertLevel::Warning, severity);
    gAudioEngine.playAlertSound(audio::AlertSound::FloodWarning);
    return;
  }

  gLedEngine.alert(led::LedEngine::AlertLevel::Warning, severity);
  gAudioEngine.playAlertSound(audio::AlertSound::SevereWeather);
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
  apiCfg.useMetric = (gSettings.settings().units == app::UnitsSystem::Metric);
  apiCfg.radarCacheMs = 15UL * 60UL * 1000UL;
  gWeatherApi.begin(apiCfg);

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
  M5.update();

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

  gWifi.update();
  gTime.update(gWifi.connected());
  gWeatherApi.update();
  MaybeStartRadarDownload();
  gRadarEngine.tick();
  if (!gRadarEngine.isDownloading()) {
    gLedEngine.progress(0, false);
  }
  gWebServer.tick();

  UpdateTouchFeedback();
  UpdateSystemEventAudio();

  const WeatherData& weatherData = gWeatherApi.data();

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

  gAudioEngine.update();
  lv_timer_handler();

  delay(16);
}
