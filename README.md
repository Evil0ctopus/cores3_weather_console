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

## 🌤️ About the Project

**CoreS3 Weather Console** is a high-performance, embedded meteorological display built for the **M5Stack CoreS3** (powered by the ESP32-S3). It features custom UI rendering, smooth boot animations, radar behavior tracking, audio cues, and live atmospheric monitoring directly on the device's touch display.

---

## ✨ Core Features

* **Dynamic Radar & UI:** Polished weather console views with fluid radar behavior and custom graphics rendering.
* **Custom Boot Sequence:** Engaging boot animations and transition states stored directly in system assets.
* **Audio Integration:** Built-in audio cues and notification management for system events and alerts.
* **Optimized PlatformIO Build:** Clean architecture utilizing structured directories for custom scripts, source logic, and configuration components.

---

## 🗂️ Repository Structure

* `src/` & `include/` — Core firmware logic, UI controllers, and header files.
* `data/` — Boot animations, system assets, and graphical UI states.
* `scripts/` — Build helpers and automation tools.

---

## 🚀 Quick Start & Building

1. Ensure you have [PlatformIO](https://platformio.org/) installed in VS Code.
2. Clone or download this repository and open the folder in your workspace.
3. Connect your M5Stack CoreS3 via USB-C.
4. Run the build and upload task:
   ```bash
   pio run -t upload

   ## ☕ Support My Work

If you find this firmware or my open-source security tools helpful, consider supporting future development and late-night coding sessions!

[![PayPal](https://img.shields.io/badge/PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/Evil0ctopus)

👤 Author
Joshua Lorson (@Evil0ctopus)
