"""Verify every rebuilt theme on the device, including painted transitions."""

from __future__ import annotations

import argparse
import io
import re
import statistics
import time
import urllib.request
from pathlib import Path

import serial
from PIL import Image

from verify_home_touch import command, tap, wait_page
from verify_navigation_performance import status
from verify_system_controls import layout, request, status as control_status, tap_label, wait_ready


THEMES = (
    "pixel_storm", "desert_calm", "future_pulse", "midnight_radar", "daybreak_clear",
    "stormglass", "aurora_line", "ocean_front", "mono_wireframe", "infrared_scan",
)
PAGES = {6: "WEATHER ATLAS", 0: "Current", 1: "Hourly", 2: "Daily",
         3: "Radar", 4: "Alerts", 5: "System Controls"}


def wait_theme(device: serial.Serial, expected: str) -> None:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if control_status(device)["theme"] == expected:
            time.sleep(0.3)
            return
        time.sleep(0.1)
    raise TimeoutError(f"Theme {expected} did not reach the UI")


def painted_page(device: serial.Serial, page: int, max_ms: float) -> float:
    previous = status(device)["paint"]
    start = time.monotonic()
    command(device, f"PAGE {page}")
    deadline = start + 8
    while time.monotonic() < deadline:
        sample = status(device)
        if sample["page"] == page and sample["painted_page"] == page and sample["paint"] != previous:
            elapsed = (time.monotonic() - start) * 1000
            assert elapsed < max_ms, (page, elapsed, sample)
            return elapsed
        time.sleep(0.005)
    raise TimeoutError(f"Page {page} did not finish painting")


def check_layout(device: serial.Serial, page: int) -> None:
    entries = layout(device)
    labels = []
    icons = []
    for line in entries:
        label = re.match(r"\[LAYOUT\] (-?\d+),(-?\d+) (\d+)x(\d+) zoom=\d+ text=(.*)", line)
        icon = re.match(r"\[IMAGE\] (-?\d+),(-?\d+) (\d+)x(\d+) holder_index=(\d+)", line)
        if label:
            x, y, width, height = map(int, label.groups()[:4])
            if 0 <= x < 320 and 0 <= y < 240:
                labels.append((x, y, width, height, label.group(5)))
        if icon:
            x, y, width, height, index = map(int, icon.groups())
            if 0 <= x < 320 and 0 <= y and y + height <= 240 and width in (34, 36):
                icons.append((x, y, width, height, index))
    assert any(entry[4] == PAGES[page] for entry in labels), (page, labels)
    if page == 6:
        for text in ("CURRENT", "HOURLY", "7 DAY", "RADAR", "ALERTS", "SYSTEM"):
            matches = [entry for entry in labels if entry[4] == text]
            assert len(matches) == 1, (text, matches)
            x, y, width, height, _ = matches[0]
            assert x + width <= 320 and y + height <= 240, matches[0]
    if page == 5:
        menu = [entry for entry in labels if entry[4] in
                ("Location", "Units", "Sound", "LEDs", "Display", "WiFi", "Quiet Hours", "Device")]
        assert len(menu) == 8 and all(entry[1] + entry[3] <= 240 for entry in menu), menu
        for entry in labels:
            if entry[4] == "Updated from shared settings":
                assert entry[1] + entry[3] <= min(button[1] for button in menu), (entry, menu)
    if page in (1, 2):
        assert icons, (page, entries)
        assert all(icon[4] == (1 if page == 1 else 0) for icon in icons), (page, icons)
        if page == 1:
            now = next(entry for entry in labels if entry[4] == "Now")
            first = min(icons, key=lambda entry: entry[1])
            assert now[0] + now[2] <= first[0], (now, first)
            temperatures = [entry for entry in labels if re.match(r"-?\d+\.\d [CF]$", entry[4])]
            assert any(first[0] + first[2] <= entry[0] for entry in temperatures), (first, temperatures)


def capture(base: str, output: Path | None, theme: str, name: str) -> None:
    with urllib.request.urlopen(base + "/api/device/screen", timeout=12) as response:
        data = response.read()
        assert response.status == 200 and data[:2] == b"BM"
    image = Image.open(io.BytesIO(data))
    assert image.size == (320, 240), image.size
    if theme != "pixel_storm" and name == "6":
        source = Path(__file__).resolve().parents[1] / "src" / "web" / "web_assets" / "themes" / f"{theme}.png"
        artwork = Image.open(source).convert("RGB")
        display = image.convert("RGB")
        for point in ((160, 230), (315, 140), (3, 210)):
            expected, actual = artwork.getpixel(point), display.getpixel(point)
            assert max(abs(a - b) for a, b in zip(actual, expected)) <= 8, (theme, point, actual, expected, "artwork not rendered")
    if output is not None:
        image.save(output / f"{theme}-{name}.png")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--url", required=True)
    parser.add_argument("--output", type=Path, help="Optional folder for screenshots")
    parser.add_argument("--max-ms", type=float, default=350)
    parser.add_argument("--restore-theme", choices=THEMES)
    parser.add_argument("--themes", nargs="+", choices=THEMES, default=list(THEMES),
                        help="Optional subset for focused reruns")
    args = parser.parse_args()
    if args.max_ms <= 0:
        parser.error("max-ms must be positive")
    if args.output is not None:
        args.output.mkdir(parents=True, exist_ok=True)
    base = args.url.rstrip("/")
    device = serial.Serial()
    device.port = args.port
    device.baudrate = 115200
    device.timeout = 0.25
    device.dtr = False
    device.rts = False
    times: list[float] = []
    with device:
        wait_ready(device)
        original = request(base, "/api/settings")
        restore = {"theme": args.restore_theme or original["theme"],
                   "displayDimSeconds": original["displayDimSeconds"]}
        try:
            request(base, "/api/settings", {"displayDimSeconds": 0})
            for theme in args.themes:
                saved = request(base, "/api/settings", {"theme": theme})
                assert saved["theme"] == theme
                wait_theme(device, theme)
                for page in PAGES:
                    times.append(painted_page(device, page, args.max_ms))
                    if page == 5:
                        entries = layout(device)
                        if not any("text=System Controls" in line for line in entries):
                            tap_label(device, "Back")
                    check_layout(device, page)
                    capture(base, args.output, theme, str(page))
                    if page != 6:
                        for held in (False, True):
                            tap(device, 160, 15, held)
                            wait_page(device, 6)
                            if not held:
                                times.append(painted_page(device, page, args.max_ms))
                tap_label(device, "SYSTEM")
                wait_page(device, 5)
                tap_label(device, "Display")
                capture(base, args.output, theme, "display")
                dropdowns = [line for line in layout(device) if line.startswith("[WIDGET] dropdown ")]
                assert dropdowns, (theme, "missing themed Display choices")
                tap_label(device, "Back")
                command(device, "PAGE 6")
                print(f"PASS {theme}: seven painted screens, stable forecast icon order, "
                      "Home tap/hold, System Display controls, rendered artwork and eight live captures")
            assert len(times) == 13 * len(args.themes), len(times)
            print(f"PASS {len(args.themes)} themes: {len(times)} painted transitions, "
                  f"median={statistics.median(times):.1f}ms max={max(times):.1f}ms")
        finally:
            request(base, "/api/settings", restore)
            wait_theme(device, restore["theme"])
            command(device, "PAGE 6")


if __name__ == "__main__":
    main()
