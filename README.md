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

### First boot

On first boot, use the on-device Wi-Fi provisioning AP / settings UI to join a network and enter your weather API key and location. No personal machine paths or secrets are required in the clone for a successful build.

---

## Support

If you find this firmware helpful, consider supporting future development:

[![PayPal](https://img.shields.io/badge/PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/Evil0ctopus)

**Author:** Evil0ctopus

