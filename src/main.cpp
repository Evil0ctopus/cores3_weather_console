#include <M5Unified.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <lvgl.h>
#include <ArduinoJson.h>

#include "generated_git_version.h"
#include "audio/audio_engine.h"
#include "led/led_engine.h"
#include "system/debug_log.h"
#include "system/settings.h"
#include "system/time_manager.h"
#include "system/wifi_manager.h"
#include "ui/ui_boot.h"
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
  lv_init();

  static lv_disp_draw_buf_t drawBuf;
  lv_disp_draw_buf_init(&drawBuf, sLvglBuf, nullptr, 320 * 20);

  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = static_cast<lv_coord_t>(M5.Display.width());
  dispDrv.ver_res = static_cast<lv_coord_t>(M5.Display.height());
  dispDrv.flush_cb = LvglDisplayFlush;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);

  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = LvglTouchRead;
  lv_indev_drv_register(&indevDrv);
}

}  // namespace

namespace {

weather::WeatherApi gWeatherApi;
weather::RadarEngine gRadarEngine;
ui::RootNavigator gUi;
ui::BootAnimation gBootAnimation;
led::LedEngine gLedEngine;
audio::AudioEngine gAudioEngine;
app::SettingsStore gSettings;
app::WifiManager gWifi;
app::TimeManager gTime;
web::WebServerHost gWebServer;

bool gMainUiStarted = false;
bool gLastWifiConnected = false;
bool gLastErrorState = false;
uint8_t gLastPageIndex = 0xFF;
uint32_t gLastBootWhooshMs = 0;
size_t gLastRadarCompletedFrames = 0;
uint32_t gLastSettingsRevision = 0;

ui::ThemeId ThemeIdFromPreference(app::ThemePreference pref) {
  switch (pref) {
    case app::ThemePreference::Dark:         return ui::ThemeId::Dark;
    case app::ThemePreference::FutureCustom1: return ui::ThemeId::FutureCustom1;
    default:                                 return ui::ThemeId::Light;
  }
}

constexpr uint8_t kSystemInfoPageIndex = 4;
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
  gWeatherApi.begin(apiCfg);
  gAudioEngine.playSystemSound(audio::SystemSound::SettingsSavedTone);
}

void UpdateTouchFeedback() {
  if (M5.Touch.getCount() == 0) {
    return;
  }

  const m5::Touch_Class::touch_detail_t& detail = M5.Touch.getDetail(0);
  if (detail.wasClicked()) {
    gLedEngine.touchEvent(led::LedEngine::TouchKind::Tap, detail.distanceX(), detail.distanceY());
    gAudioEngine.playTouchSound(audio::TouchSound::TapClick);
    return;
  }

  if (detail.wasHold()) {
    gLedEngine.touchEvent(led::LedEngine::TouchKind::LongPress, detail.distanceX(), detail.distanceY());
    gAudioEngine.playTouchSound(audio::TouchSound::LongPressRise);
    return;
  }

  if (!detail.wasFlicked()) {
    return;
  }

  const int dx = detail.distanceX();
  const int dy = detail.distanceY();
  if (abs(dx) >= abs(dy)) {
    gLedEngine.touchEvent(dx >= 0 ? led::LedEngine::TouchKind::SwipeRight : led::LedEngine::TouchKind::SwipeLeft, dx, dy);
  } else {
    gLedEngine.touchEvent(dy >= 0 ? led::LedEngine::TouchKind::SwipeDown : led::LedEngine::TouchKind::SwipeUp, dx, dy);
  }
  gAudioEngine.playTouchSound(audio::TouchSound::SwipeWhoosh);
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
  SPIFFS.begin(true);

  gSettings.begin();
  gDebugLog.begin(gSettings.settings().debugMode);
  gLastSettingsRevision = gSettings.revision();

  gLedEngine.begin(96);
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
  InitializeLvgl();

  // Boot animation gets its own screen — shown immediately.
  lv_obj_t* bootScreen = gBootAnimation.begin(ui::ThemeId::Light);
  lv_disp_load_scr(bootScreen);
  gAudioEngine.playBootSound(audio::BootSound::Whoosh);
  gLastBootWhooshMs = millis();
  // Main UI is created after boot animation finishes (see loop()).

  weather::WeatherApiConfig apiCfg;
  apiCfg.apiKey = gSettings.settings().apiKey;
  apiCfg.locationQuery = gSettings.settings().locationQuery;
  apiCfg.locationKey = gSettings.settings().locationKey;
  apiCfg.useMetric = (gSettings.settings().units == app::UnitsSystem::Metric);
  gWeatherApi.begin(apiCfg);

  gRadarEngine.begin();
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
  cfg.led_brightness = 64;
  M5.begin(cfg);

  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  }
  M5.Display.setBrightness(128);

  InitializeSubsystems();
}

void loop() {
  M5.update();

  if (!gMainUiStarted) {
    if (gBootAnimation.update()) {
      gMainUiStarted = true;
      // Create the main UI on its own screen and switch to it.
      lv_obj_t* mainScreen = lv_obj_create(nullptr);
      const ui::ThemeId themeId = ThemeIdFromPreference(gSettings.settings().theme);
      gUi.begin(mainScreen, themeId);
      lv_disp_load_scr(mainScreen);
    } else {
      const uint32_t now = millis();
      if (!gAudioEngine.isPlaying() && (now - gLastBootWhooshMs) >= kBootWhooshIntervalMs) {
        gAudioEngine.playBootSound(audio::BootSound::Whoosh);
        gLastBootWhooshMs = now;
      }
      gLedEngine.bootAnimation(gBootAnimation.ledPhase(),
                               gBootAnimation.ledIntensity(),
                               true,
                               gBootAnimation.ledLockIn(),
                               gBootAnimation.ledFadingOut());
      gAudioEngine.update();
      lv_timer_handler();
      delay(16);
      return;
    }
  }

  gWifi.update();
  gTime.update(gWifi.connected());
  gWeatherApi.update();
  gRadarEngine.tick();
  gWebServer.tick();

  UpdateTouchFeedback();
  UpdateSystemEventAudio();

  const WeatherData& weatherData = gWeatherApi.data();

  ui::SystemInfo systemInfo;
  const app::WifiStatusInfo wifiInfo = gWifi.statusInfo();
  const String primaryIp = wifiInfo.ipAddress.length() > 0 ? wifiInfo.ipAddress : wifiInfo.accessPointIpAddress;
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
  gLedEngine.setSystemStatus(false, gWifi.connected(), warningState);
  gLedEngine.update();

  gAudioEngine.update();
  lv_timer_handler();

  delay(16);
}
