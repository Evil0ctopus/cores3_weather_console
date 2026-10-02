<div align="center">

  <h1>CORE S3: WEATHER CONSOLE</h1>
  <p><b>Advanced Meteorological Display & Radar Station</b></p>
  <p><i>Real-time weather tracking, radar visuals, and atmospheric data for the M5Stack CoreS3</i></p>

  [![target](https://img.shields.io/badge/target-M5Stack%20CoreS3-blue)](https://m5stack.com)
  [![framework](https://img.shields.io/badge/framework-Arduino%2FPlatformIO-green)](https://platformio.org/)
  [![language](https://img.shields.io/badge/language-C%++%2087.7%25-orange)](https://github.com/Evil0ctopus/cores3_weather_console/search?l=c%2B%2B)
  [![license](https://img.shields.io/badge/license-MIT-yellow.svg)](LICENSE)
  [![status](https://img.shields.io/badge/status-Active%20Development-success)]()

</div>

---

## About the Project

**CoreS3 Weather Console** is an embedded meteorological display for the **M5Stack CoreS3** (ESP32-S3). It features custom UI rendering, boot animations, radar-style visuals, audio cues, and live atmospheric monitoring on the device touch display.

Wi-Fi credentials and weather API settings are configured on-device (captive provisioning / settings UI) and stored in NVS — nothing sensitive is hardcoded in the repo.

---

## Core Features

* **Dynamic Radar & UI:** Weather console views with radar-style graphics and custom rendering.
* **Custom Boot Sequence:** Boot animations and transition states stored in SPIFFS assets.
* **Audio Integration:** On-device audio cues for system events and alerts.
* **PlatformIO Build:** Structured firmware layout with scripts, source, and web/SPIFFS assets.

---

## Repository Structure

* `src/` & `include/` — Firmware logic, UI controllers, and headers (`include/lv_conf.h` for LVGL).
* `src/web/web_assets/` — SPIFFS payload (boot frames, backgrounds, audio, web UI). Mapped via `platformio.ini` `data_dir`.
* `data/` — Working/source asset copies (not the flash `data_dir`).
* `scripts/` — Build helpers (e.g. git version stamp).

---

## Quick Start & Building

### Prerequisites

1. [Visual Studio Code](https://code.visualstudio.com/) (or another editor) with the [PlatformIO](https://platformio.org/) extension / CLI.
2. An M5Stack CoreS3 and a USB-C cable.

### Clone and open

```bash
git clone https://github.com/Evil0ctopus/cores3_weather_console.git
cd cores3_weather_console
```

Open the folder in VS Code (or your editor). PlatformIO will pick up `platformio.ini`.

### Build, flash firmware, and upload SPIFFS

Connect the CoreS3 over USB-C. Let PlatformIO auto-detect the serial port (do not hardcode `COMx` / `/dev/ttyUSB*` in project files).

```bash
# Firmware
pio run -t upload

# SPIFFS assets (boot frames, backgrounds, audio, web UI)
pio run -t uploadfs
```

Or use the PlatformIO Upload / Upload Filesystem actions in the IDE status bar.

### Serial monitor (optional)

```bash
pio device monitor
```

Default baud is `115200` (`monitor_speed` in `platformio.ini`).

### Local web preview

Run the **Weather Atlas web preview** VS Code task, or `node scripts/web_preview.cjs`,
then open `http://127.0.0.1:4173`. This serves static assets only; settings,
live weather, and remote controls require the CoreS3's own web server.

The firmware uses CoreS3 PSRAM for LVGL allocations, including full-screen PNG
decoding. Upload both firmware and SPIFFS assets after changing the artwork;
uploading firmware alone leaves the previous boot frames on the device.
Run firmware upload and filesystem upload as separate PlatformIO commands.
The boot screen loads one backdrop and animates LVGL overlays rather than
decoding a full-screen PNG on every frame.
LVGL's image cache is enabled at compile time so redraws reuse decoded PNGs.
Page content stays below the status bar and uses native font sizes rather
than sub-256 transform zoom values.
On-device theme changes are persisted independently and work before weather
location setup is complete.
The home hub uses a cinematic Neon Storm treatment: supersampled aurora and
starfield artwork matching the boot intro, translucent gradient panels,
fine cyan/violet edges, and gently twinkling stars. One small star updates
every 100 ms, avoiding widely scattered invalidations in a single frame.
Its layout and touch targets are unchanged. Neon Storm (the existing
`pixel_storm` saved theme) extends the palette and shared backdrop to every
device page and the web control panel. Only small star overlays animate;
the full-screen aurora uses a resident 150 KiB RGB565 image in PSRAM, avoiding
PNG decoding and alpha blending on navigation (PNG remains a logged fallback).
Home opens pages directly instead of sliding through hidden pages; Neon Storm
also uses direct adjacent-page changes to avoid multi-frame scroll stalls.
Other themes retain adjacent-page slides. Background animation and vertical
content scrolling remain enabled.
Unchanged HUD text is not redrawn.
The icon cache holds 24 entries to avoid churn between pages. Other selectable themes
remain available; their device pages keep their original backgrounds.
The cinematic boot intro uses original supersampled aurora artwork, a glass
weather emblem, fine cyan/violet orbital highlights, a smooth reveal, and
a staged title sequence over 3.6 seconds. Generate its native 320x240 PNG
with `scripts/generate_boot_intro.py`; it is decoded once, not per frame.
The same generator produces `backgrounds/neon_aurora.png` (320x240 device)
and `backgrounds/aurora_web.png` (1280x960 browser) without the boot emblem.
It also generates `backgrounds/aurora.rgb`, native little-endian RGB565 pixels.
Web animation respects reduced-motion preferences. The live screen mirror shows these
same effects (its refresh rate may sample fewer animation frames).
Weather and navigation icons use original 96x96 transparent artwork rendered
from 384x384 masters by `scripts/generate_atlas_icons.py`. The Neon Storm set
uses shaded sun/cloud/moon forms, cyan precipitation, and cyan/violet system
symbols. Device icons are scaled with antialiasing to their actual layout
sizes rather than cropped; drawn primitives are reserved for asset failures.
The icon generator also emits an RGB565+alpha atlas with exact-size
22/26/34/36/48/56-pixel variants from the supersampled masters. The device
loads its 380 KB atlas into PSRAM once at startup, avoiding PNG decoding,
filesystem probes and software rescaling during navigation. The original
96-pixel PNGs remain available for the web UI and device fallback.
Units are applied at display time to the metric weather model: Imperial uses
Fahrenheit, mph, and inHg; Metric uses Celsius, kph, and millibars. Changing
units updates device labels (including no-data placeholders) without waiting
for another weather download.

USB diagnostics at 115200 baud support `STATUS`, `PAGE 0` through `PAGE 6`
(6 is the home hub), `TOUCH x y 1` / `TOUCH x y 0`, and `SOUND`.
Touch presses and releases are delivered to LVGL in order, including quick
taps received within one loop. Physical input uses the touch detail's pressed
state, not its presence (release details remain available for click feedback).
The LVGL input reader runs immediately after each M5 touch update, before
network work and rendering, so its polling timer cannot skip a sampled press.
`RESTART` acknowledges and reboots the device to replay the boot intro.
USB provisioning accepts `WIFI {"ssid":"your-network","password":"your-password"}`.
Credentials are saved on the device, never echoed in the command response.
An empty password selects an open network; secured-network passwords must be
8 to 63 bytes. `CONTROL_OK` starts connection; check `STATUS` for a station IP.
`WIFI_SCAN` starts a scan; poll `WIFI_NETWORKS` for its JSON results.
Network names are case-sensitive, and CoreS3 supports only 2.4 GHz Wi-Fi.
`STATUS` includes recent loop-phase timings in milliseconds, the theme ID,
the audio-engine playback flag, and the speaker-driver playback flag.
Rendering timing excludes audio loading; `audio_ms` reports it separately.
`pixels` and `flushes` describe the last render pass. `painted_page` and `paint`
identify the page and generation most recently flushed to the physical display.
`painted_page=255` means a page slide has not reached its final position yet.
The playback flags confirm software activity, not physical audibility. `UI_LABELS`
prints label geometry and scale for diagnosing layout issues.
`python scripts/capture_device_screen.py COM3 output.bmp` captures the screen;
substitute the detected port. Capture opens the port without asserting reset
lines, so it does not intentionally restart the device.
`python scripts/verify_home_touch.py COM3` checks quick and held Home taps
on all six pages, including icon-center and edge taps, and quick hub navigation.
The top-center Home control has an 18-pixel extended touch target around its
38x26 artwork, accommodating finger taps below the icon without changing layout.
The regression also replays recorded physical tap positions through y=41.
`python scripts/verify_navigation_performance.py COM3 --cycles 10 --max-ms 350 --mirror-url http://DEVICE_IP/api/device/screen` times completed physical
display flushes, including settled adjacent-page slides, while exercising the
live mirror. Run after boot and initial weather synchronization. It fails
explicitly for missing paints, slow transitions or failed mirror requests.
The web mirror uses native top-down RGB565 BMPs (153,666 bytes rather than
230,454), preserving screen color precision without expensive RGB conversion.
USB `SCREEN_BMP` retains its original 24-bit BMP format. Snapshot conversion
runs outside the display lock; the lock only protects a framebuffer copy.
Navigation also uses a 60-row internal-RAM draw buffer and avoids updating
unchanged labels/HUD properties. Radar repainting follows a display revision:
new downloads, frames, interpolation steps, modes and overlays still redraw,
but an unchanged radar map no longer invalidates the page every content tick.

### First boot

On first boot, use the on-device Wi-Fi provisioning AP / settings UI to join a network and enter your weather API key and location. No personal machine paths or secrets are required in the clone for a successful build.

---

## Support

If you find this firmware helpful, consider supporting future development:

[![PayPal](https://img.shields.io/badge/PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/Evil0ctopus)

**Author:** Evil0ctopus
