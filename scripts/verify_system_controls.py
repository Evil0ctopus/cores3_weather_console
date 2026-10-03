"""Exercise shared System preferences and runtime effects on a connected CoreS3."""

from __future__ import annotations

import argparse
import json
import re
import time
import urllib.error
import urllib.parse
import urllib.request

import serial

from verify_home_touch import command, tap, wait_page


def request(base: str, path: str, fields: dict | None = None) -> dict:
    body = None
    if fields is not None:
        body = urllib.parse.urlencode({
            key: str(value).lower() if isinstance(value, bool) else str(value)
            for key, value in fields.items()
        }).encode()
    with urllib.request.urlopen(base + path, body, timeout=12) as response:
        return json.load(response)


def status(device: serial.Serial) -> dict:
    return json.loads(command(device, "CONTROL_STATUS", "CONTROL_STATUS").split(" ", 1)[1])


def layout(device: serial.Serial) -> list[str]:
    device.reset_input_buffer()
    device.write(b"UI_LABELS\n")
    found = []
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        line = device.readline().decode(errors="replace").strip()
        if line == "LAYOUT_END":
            return found
        found.append(line)
    raise TimeoutError("No label layout response")


def labels(device: serial.Serial) -> list[tuple[int, int, int, int, str]]:
    found = []
    for line in layout(device):
        match = re.match(r"\[LAYOUT\] (-?\d+),(-?\d+) (\d+)x(\d+) zoom=\d+ text=(.*)", line)
        if match:
            x, y, width, height = map(int, match.groups()[:4])
            if 0 <= x < 320 and 0 <= y < 240:
                found.append((x, y, width, height, match.group(5)))
    return found


def widgets(device: serial.Serial) -> list[tuple[str, int, int, int, int, int, str]]:
    found = []
    for line in layout(device):
        match = re.match(r"\[WIDGET\] (\w+) (-?\d+),(-?\d+) (\d+)x(\d+) value=(-?\d+) text=(.*)", line)
        if match:
            x, y, width, height, value = map(int, match.groups()[1:6])
            if 0 <= x < 320 and 0 <= y < 240:
                found.append((match.group(1), x, y, width, height, value, match.group(7)))
    return found


def tap_label(device: serial.Serial, text: str) -> None:
    matches = [entry for entry in labels(device) if entry[4] == text]
    assert len(matches) == 1, (text, matches)
    x, y, width, height, _ = matches[0]
    tap(device, x + width // 2, y + height // 2, False)
    time.sleep(0.4)


def wait_ready(device: serial.Serial) -> None:
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        if "ready=1" in command(device, "STATUS", "DEVICE_STATUS"):
            return
        time.sleep(0.5)
    raise TimeoutError("Device did not become ready")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--url", required=True, help="Device URL, e.g. http://192.168.1.2")
    parser.add_argument("--reboot", action="store_true", help="Also verify persistence across a restart")
    args = parser.parse_args()
    base = args.url.rstrip("/")
    device = serial.Serial()
    device.port = args.port
    device.baudrate = 115200
    device.timeout = 0.25
    device.dtr = False
    device.rts = False
    with device:
        wait_ready(device)
        original = request(base, "/api/settings")
        keys = [field["key"] for field in original["controlSchema"]] + ["units", "updateIntervalMinutes"]
        restore = {key: original[key] for key in keys}

        def save(fields: dict) -> dict:
            result = request(base, "/api/settings", fields)
            for key, value in fields.items():
                assert result[key] == value, (key, result[key], value)
            time.sleep(0.3)
            return result

        try:
            assert len(original["controlSchema"]) == 24
            modes = next(field["options"] for field in original["controlSchema"] if field["key"] == "ledMode")
            assert {option["value"] for option in modes} == {"weather", "solid", "rainbow", "breathing", "off"}
            save({
                "ledMode": "solid", "ledColor": "green", "ledBrightness": 72,
                "ledTouch": False, "ledNavigation": False, "ledAlerts": False,
                "quietEnabled": False, "soundVolume": 32, "soundMuted": True,
                "displayBrightness": 60, "displayDimSeconds": 0,
            })
            time.sleep(1.6)
            live = status(device)
            assert live["ledReady"] and live["ledPixels"], live
            assert live["ledBrightness"] == 183 and live["volume"] == 81 and live["muted"], live
            assert live["displayBrightness"] == 153, live
            assert all(pixel == [0, 255, 50] for pixel in live["ledPixels"]), live
            print("PASS: shared schema, partial saves, solid color, LED/screen brightness and volume/mute")

            save({"ledMode": "off"})
            assert all(pixel == [0, 0, 0] for pixel in status(device)["ledPixels"])
            save({"ledMode": "rainbow", "ledSpeed": 80})
            first = status(device)["ledPixels"]
            time.sleep(0.6)
            second = status(device)["ledPixels"]
            assert first != second and len({tuple(pixel) for pixel in second}) > 1, (first, second)
            save({"ledMode": "breathing", "ledColor": "red"})
            first = status(device)["ledPixels"]
            time.sleep(0.6)
            second = status(device)["ledPixels"]
            assert first != second and all(pixel[1:] == [0, 0] for pixel in second), (first, second)
            print("PASS: Off blanks existing pixels; flowing rainbow moves across pixels; selected-color breathing")

            before = request(base, "/api/settings")
            for invalid in (
                {"ledMode": "invalid"}, {"ledBrightness": 101}, {"ledBrightness": -1},
                {"ledSpeed": "1.5"}, {"soundMuted": "yes"}, {"soundPack": "missing"},
                {"ledColor": "blue", "displayBrightness": 0},
            ):
                try:
                    request(base, "/api/settings", invalid)
                except urllib.error.HTTPError as error:
                    assert error.code == 400, error.code
                    assert json.load(error).get("error")
                else:
                    raise AssertionError(f"Invalid settings accepted: {invalid}")
                after = request(base, "/api/settings")
                assert all(before[key] == after[key] for key in keys)
            print("PASS: invalid choices, booleans, fractions and ranges rejected transactionally")

            save({"displayDimSeconds": 1, "displayNightBrightness": 12})
            time.sleep(1.3)
            assert status(device)["displayBrightness"] == 30
            tap(device, 10, 10, False)
            time.sleep(0.2)
            assert status(device)["displayBrightness"] == 153
            print("PASS: inactivity dimming wakes on touch")
            save({"displayDimSeconds": 0, "quietEnabled": True, "quietStartHour": 0, "quietEndHour": 0})
            live = status(device)
            if live["localHour"] >= 0:
                assert live["quiet"] and live["ledQuiet"] and live["ledBrightness"] == 45, live
                assert live["displayBrightness"] == 30, live
                print("PASS: weather-local all-day quiet hours dim screen and LEDs")
            else:
                assert not live["quiet"], live
                print("PASS: unknown weather-local time does not activate quiet hours")
            save({"quietEnabled": False, "displayDimSeconds": 0})

            save({"units": "imperial"})
            command(device, "PAGE 5")
            wait_page(device, 5)
            time.sleep(0.5)
            tap_label(device, "Units")
            tap(device, 130, 158, False)
            time.sleep(0.3)
            items = labels(device)
            option = next(entry for entry in items if entry[4].startswith("Metric:") and entry[3] > 20)
            tap(device, option[0] + 30, option[1] + 8, False)
            time.sleep(0.5)
            saved = request(base, "/api/settings")
            assert saved["units"] == "metric", saved["units"]
            command(device, "PAGE 0")
            wait_page(device, 0)
            time.sleep(0.3)
            assert any(entry[4].endswith(" C") for entry in labels(device))
            command(device, "PAGE 5")
            wait_page(device, 5)
            time.sleep(0.3)
            print("PASS: on-device Imperial-to-Metric change is saved and visible through the web API")
            tap_label(device, "Back")
            save({"ledMode": "weather"})
            tap_label(device, "LEDs")
            dropdown = next(entry for entry in widgets(device) if entry[0] == "dropdown")
            tap(device, dropdown[1] + 40, dropdown[2] + dropdown[4] // 2, False)
            time.sleep(0.3)
            options = next(entry for entry in labels(device) if entry[4] == "Active Weather" and entry[3] > 40)
            tap(device, options[0] + 40, options[1] + (options[3] // 5) * 2 + 8, False)
            time.sleep(0.5)
            assert request(base, "/api/settings")["ledMode"] == "rainbow"
            save({"ledMode": "off"})
            dropdown = next(entry for entry in widgets(device) if entry[0] == "dropdown")
            assert dropdown[5] == 4 and dropdown[6] == "Off", dropdown
            print("PASS: touch LED mode selection saves to web; web LED change updates the open device editor")
            tap_label(device, "Back")
            if original["locationQuery"].isdigit():
                tap_label(device, "Location")
                tap_label(device, "Enter ZIP (US postal code)")
                tap_label(device, "Find location")
                deadline = time.monotonic() + 15
                while not any(entry[4] == "Confirm place" for entry in labels(device)):
                    assert time.monotonic() < deadline, "ZIP lookup did not offer confirmation"
                    time.sleep(0.3)
                tap_label(device, "Cancel")
                assert request(base, "/api/settings")["locationQuery"] == original["locationQuery"]
                print("PASS: async on-device ZIP lookup offers place confirmation; Cancel preserves saved location")
                tap_label(device, "Back")
            tap_label(device, "WiFi")
            tap_label(device, "Scan nearby networks")
            deadline = time.monotonic() + 15
            while True:
                scan = json.loads(command(device, "WIFI_NETWORKS", "WIFI_NETWORKS").split(" ", 1)[1])
                if not scan["inProgress"] and scan["networks"]:
                    break
                assert time.monotonic() < deadline, "On-device WiFi scan did not complete"
                time.sleep(0.3)
            assert request(base, "/api/settings")["wifiSsid"] == original["wifiSsid"]
            print("PASS: on-device async WiFi scan completes without changing saved credentials")
            tap_label(device, "Back")

            save({"ledMode": "rainbow", "ledSpeed": 55, "soundVolume": 32})
            if args.reboot:
                request(base, "/api/device/control", {"action": "restart", "confirm": True})
                time.sleep(3)
                wait_ready(device)
                persisted = request(base, "/api/settings")
                assert persisted["ledMode"] == "rainbow" and persisted["ledSpeed"] == 55
                assert persisted["soundVolume"] == 32
                assert status(device)["ledMode"] == "rainbow"
                print("PASS: NVS persistence and runtime reapplication after restart")
        finally:
            request(base, "/api/settings", restore)
            command(device, "PAGE 6")
            print("Original preferences restored; device left on Home")


if __name__ == "__main__":
    main()
