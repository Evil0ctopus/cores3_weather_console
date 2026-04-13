# Audio Engine Integration Guide

The M5Stack CoreS3 audio engine provides non-blocking WAV/PCM playback with theme-based sound pack support. This document outlines integration points and setup instructions.

## Directory Structure for Audio Files

Create the following SPIFFS directory structure:

```
/audio/
├── default/
│   ├── boot.wav
│   ├── touch_tap.wav
│   ├── swipe_left.wav
│   ├── swipe_right.wav
│   ├── swipe_up.wav
│   ├── swipe_down.wav
│   ├── long_press.wav
│   ├── weather_alert.wav
│   ├── weather_warning.wav
│   ├── weather_critical.wav
│   ├── wifi_connected.wav
│   ├── wifi_disconnected.wav
│   ├── settings_confirm.wav
│   ├── radar_update.wav
│   ├── page_transition.wav
│   └── led_alert.wav
├── modern/
│   ├── boot.wav
│   ├── touch_tap.wav
│   ... (all sound files)
├── retro/
│   ... (all sound files)
└── minimal/
    ... (all sound files)
```

## Sound Types

The audio engine supports 16 sound types via the `SoundType` enum:

| Sound Type | Trigger | Default Volume | Duration |
|---|---|---|---|
| `BootAnimation` | Device startup animation | 220 | ~2000ms |
| `TouchTap` | UI button tap | 180 | ~80ms |
| `SwipeLeft` | Swipe left gesture | 190 | ~120ms |
| `SwipeRight` | Swipe right gesture | 190 | ~120ms |
| `SwipeUp` | Swipe up gesture | 190 | ~100ms |
| `SwipeDown` | Swipe down gesture | 190 | ~100ms |
| `LongPress` | Long press gesture | 200 | ~200ms |
| `WeatherAlert` | Info-level weather alert | 210 | ~400ms |
| `WeatherWarning` | Warning-level weather alert | 220 | ~600ms |
| `WeatherCritical` | Critical-level weather alert | 230 | ~800ms |
| `WiFiConnected` | Network connected | 200 | ~400ms |
| `WiFiDisconnected` | Network disconnected | 190 | ~300ms |
| `SettingsConfirm` | Settings saved | 210 | ~250ms |
| `RadarUpdate` | Radar animation updated | 180 | ~150ms |
| `PageTransition` | UI page changed | 190 | ~180ms |
| `LedAlert` | LED alert triggered | 210 | ~350ms |

## Integration Points

### 1. Touch/Gesture Feedback

**Automatically integrated** in `main.cpp`:

```cpp
void UpdateTouchLedAndAudioFeedback() {
  // ... touch detection code ...
  gAudioEngine.onTouchSound((uint8_t)touchKind, deltaX, deltaY);
}
```

Maps touch gestures to sounds:
- Tap → `TouchTap`
- Hold → `LongPress`
- Swipe Left → `SwipeLeft`
- Swipe Right → `SwipeRight`
- Swipe Up → `SwipeUp`
- Swipe Down → `SwipeDown`

### 2. WiFi Events

To integrate WiFi connection sounds, update `wifi_manager.cpp` or `web_routes.cpp`:

```cpp
// In wifi_manager.cpp, after successful connection:
extern audio::AudioEngine gAudioEngine;

void WiFiManager::onConnected() {
  // ... existing connection logic ...
  gAudioEngine.onWiFiEvent("connected");
}

void WiFiManager::onDisconnected() {
  // ... existing disconnection logic ...
  gAudioEngine.onWiFiEvent("disconnected");
}
```

Or in Web UI route callback:
```cpp
// In web_routes.cpp, POST /wifi/connect handler
auto callback = [](void* ctx) {
  extern audio::AudioEngine gAudioEngine;
  gAudioEngine.onWiFiEvent("connected");
};
```

### 3. Weather Alerts

Integrate with the weather alert system to trigger alert sounds:

```cpp
// In main.cpp or weather update callback:
led::LedEngine::AlertLevel alertLevel = AlertLevelFromWeather(weatherData);
if (alertLevel != led::LedEngine::AlertLevel::None) {
  gAudioEngine.onWeatherAlert((uint8_t)alertLevel);
}
```

Alert levels map to sounds:
- `Info` (1) → `WeatherAlert`
- `Warning` (2) → `WeatherWarning`
- `Critical` (3) → `WeatherCritical`

### 4. Settings Changes

Play confirmation sound when settings are saved:

```cpp
// In web_routes.cpp or settings handler:
void OnSettingsSaved(void* userContext, const app::AppSettings& settings) {
  extern audio::AudioEngine gAudioEngine;
  // ... existing settings update logic ...
  gAudioEngine.onSettingsChanged();
}
```

### 5. Radar Updates

Play update sound during radar animation:

```cpp
// In OnRadarProgress callback:
void OnRadarProgress(void* userContext, const weather::RadarProgress& progress) {
  extern audio::AudioEngine gAudioEngine;
  
  if (progress.completedFrames > 0 && progress.completedFrames % 5 == 0) {
    gAudioEngine.onRadarUpdate();
  }
  // ... existing radar logic ...
}
```

### 6. Page Transitions

Play sound when UI page changes:

```cpp
// In main.cpp loop, after gUi.update():
uint8_t currentPage = gUi.activePageIndex();
if (currentPage != gLastPageIndex) {
  gAudioEngine.onPageTransition(gLastPageIndex, currentPage);
  gLastPageIndex = currentPage;
}
```

### 7. LED Alerts

Sync audio with LED alert patterns:

```cpp
// In OnLedEvent callback or LED update:
void OnLedEvent(void* userContext, const char* eventName, const String& details) {
  extern audio::AudioEngine gAudioEngine;
  
  if (String(eventName) == "alert") {
    gAudioEngine.onLedAlert(alertLevel);
  }
}
```

### 8. Boot Animation

Automatically integrated in `main.cpp`:

```cpp
// In BeginSubsystems():
lv_scr_load(gBootAnimation.begin(ResolveTheme(CurrentSettings().theme)));
gAudioEngine.onBootAnimationStart();

// In loop(), when boot animation completes:
if (gBootAnimation.update()) {
  BeginMainUi();
  gAudioEngine.onBootAnimationEnd();
}
```

## Theme Support

### Setting a Theme

```cpp
// Use built-in theme ID
gAudioEngine.setTheme(audio::ThemeId::Modern);

// Or with custom name
gAudioEngine.setTheme(audio::ThemeId::Custom1, "my_custom_theme");
```

### Registering Custom Sounds

```cpp
// Override default sound for a type
gAudioEngine.registerSound(
  audio::SoundType::TouchTap,
  "touch_tap.wav",        // filename
  85,                      // duration in ms
  190,                     // default volume (0-255)
  false                    // loop
);
```

### Creating New Sound Packs

1. Create a new directory in SPIFFS: `/audio/my_theme/`
2. Add all required WAV files (see directory structure above)
3. Switch theme: `gAudioEngine.setTheme(audio::ThemeId::Custom1, "my_theme");`

## Configuration

### Master Volume Control

```cpp
// Set master volume (0-255)
gAudioEngine.setMasterVolume(200);

// Get current volume
uint8_t vol = gAudioEngine.getMasterVolume();

// Mute all audio
gAudioEngine.setMuted(true);
gAudioEngine.setMuted(false);  // Unmute
```

### Per-Sound Volume Override

```cpp
// Play sound with custom volume (overrides default)
gAudioEngine.play(audio::SoundType::TouchTap, 150);
```

## Event Callbacks

The audio engine emits events for system integration:

```cpp
void OnAudioEvent(void* userContext, const char* eventName, const String& details) {
  if (String(eventName) == "initialized") {
    // Engine ready
  } else if (String(eventName) == "sound_started") {
    // Sound playback started: details = "file=...,volume=..."
  } else if (String(eventName) == "sound_finished") {
    // Sound finished: details = "type=..."
  } else if (String(eventName) == "theme_changed") {
    // Theme switched: details = "theme_name"
  } else if (String(eventName) == "theme_pack_loaded") {
    // Sound pack loaded: details = "theme_name"
  } else if (String(eventName) == "volume_changed") {
    // Volume adjusted: details = "volume"
  } else if (String(eventName) == "muted") {
    // Audio muted
  } else if (String(eventName) == "unmuted") {
    // Audio unmuted
  }
}

gAudioEngine.setEventCallback(OnAudioEvent, nullptr);
```

## Playback Queue and Non-Blocking Operation

The audio engine maintains a playback queue (max 10 items) and processes updates in the main loop:

```cpp
// In loop(), update is called every 50ms:
gAudioEngine.update();
```

- Sounds are queued immediately when `gAudioEngine.play()` is called
- Queue prevents missed sounds and ensures proper sequencing
- Current playback has priority; new sounds wait in queue
- Queue fills automatically; oldest sounds drop if overflow

## WAV File Requirements

Supported audio formats:
- **Format**: PCM WAV (uncompressed)
- **Bit Depth**: 16-bit
- **Sample Rate**: 44.1 kHz (recommended) or 22.05 kHz
- **Channels**: Mono (1 channel)
- **Max File Size**: 32 KB (buffer size)
- **Typical Duration**: 50ms - 2000ms

### Creating WAV Files

Using FFmpeg:
```bash
# Convert MP3 to WAV
ffmpeg -i input.mp3 -acodec pcm_s16le -ar 44100 -ac 1 output.wav

# Trim/edit audio
ffmpeg -i input.wav -ss 0.5 -t 1.0 -acodec pcm_s16le -ar 44100 -ac 1 output.wav

# Check file info
ffprobe output.wav
```

## SPIFFS Upload

Add audio files to `src/web/web_assets/audio/` directory structure:

```
src/web/web_assets/
├── audio/
│   ├── default/
│   │   └── *.wav
│   ├── modern/
│   │   └── *.wav
│   └── ...
├── index.html
└── app.js
```

Build SPIFFS image:
```bash
pio run -t buildfs
```

Upload to device:
```bash
pio run -t uploadfs
```

## Integration Checklist

- [ ] Create audio directory structure in `src/web/web_assets/audio/`
- [ ] Add WAV files for all themes
- [ ] Initialize `gAudioEngine.begin()` in `BeginSubsystems()`
- [ ] Add audio event callback: `gAudioEngine.setEventCallback(OnAudioEvent, nullptr)`
- [ ] Integrate WiFi event sounds
- [ ] Integrate weather alert sounds
- [ ] Integrate settings confirmation sound
- [ ] Integrate radar update sounds
- [ ] Integrate page transition sounds (optional)
- [ ] Build and upload SPIFFS: `pio run -t buildfs && pio run -t uploadfs`
- [ ] Test audio playback on device

## Troubleshooting

### No Sound
1. Check SPIFFS files exist: `/audio/theme_name/sound.wav`
2. Verify M5 speaker is enabled and powered
3. Check master volume: `gAudioEngine.getMasterVolume() > 0`
4. Confirm not muted: `gAudioEngine.isMuted() == false`
5. Check debug log: `gAudioEngine.dumpAudioStatus()`

### Crackling/Distorted Audio
- Reduce volume (0-255)
- Ensure WAV files are 16-bit PCM at 44.1 kHz
- Reduce file size (trim silent portions)

### Queue Overflows
- Reduce frequency of sounds triggering simultaneously
- Increase theme complexity (longer sound durations)
- Disable sounds in low-memory scenarios: `gAudioEngine.setMuted(true)`

### File Not Found Errors
- Check SPIFFS path: `/audio/theme_name/filename.wav`
- Rebuild SPIFFS image
- Re-upload filesystem

## Future Enhancements

- [ ] Dynamic sound pack loading from web UI
- [ ] Per-theme volume presets
- [ ] Sound effect intensity mapping to LED patterns
- [ ] Audio visualization on UI
- [ ] User recorded custom sounds
- [ ] Spatial audio (if supported by M5)
- [ ] Sound scheduling and timing precision
