from __future__ import annotations

import argparse
import re
import time

import serial


def response(device: serial.Serial, prefix: str) -> str:
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        line = device.readline().decode(errors="replace").strip()
        if line.startswith("CONTROL_ERROR"):
            raise RuntimeError(line)
        if line.startswith(prefix):
            return line
    raise TimeoutError(f"No {prefix} response")


def command(device: serial.Serial, text: str, prefix: str = "CONTROL_OK") -> str:
    device.reset_input_buffer()
    device.write(text.encode("ascii") + b"\n")
    return response(device, prefix)


def wait_page(device: serial.Serial, expected: int) -> None:
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        line = command(device, "STATUS", "DEVICE_STATUS")
        if re.search(rf"ready=1 page={expected}\b", line):
            return
        time.sleep(0.1)
    raise TimeoutError(f"Expected page {expected}: {line}")


def tap(device: serial.Serial, x: int, y: int, held: bool) -> None:
    if held:
        command(device, f"TOUCH {x} {y} 1")
        time.sleep(0.15)
        command(device, f"TOUCH {x} {y} 0")
    else:
        device.reset_input_buffer()
        device.write(f"TOUCH {x} {y} 1\nTOUCH {x} {y} 0\n".encode("ascii"))
        response(device, "CONTROL_OK")
        response(device, "CONTROL_OK")


def main() -> None:
    parser = argparse.ArgumentParser(description="Verify quick and held Home taps on every CoreS3 page.")
    parser.add_argument("port", help="Serial port, for example COM3")
    args = parser.parse_args()
    device = serial.Serial()
    device.port = args.port
    device.baudrate = 115200
    device.timeout = 0.25
    device.dtr = False
    device.rts = False
    with device:
        deadline = time.monotonic() + 45
        while "ready=1" not in command(device, "STATUS", "DEVICE_STATUS"):
            if time.monotonic() >= deadline:
                raise TimeoutError("UI did not finish boot")
            time.sleep(0.5)
        for page in range(6):
            for held in (False, True):
                for x, y in ((160, 15), (150, 10), (170, 20),
                             (178, 29), (155, 33), (168, 34), (161, 41),
                             (163, 39), (170, 36), (166, 31), (176, 31)):
                    command(device, f"PAGE {page}")
                    wait_page(device, page)
                    time.sleep(0.3)
                    tap(device, x, y, held)
                    wait_page(device, 6)
            tap(device, 80 if page % 2 == 0 else 236, 108 + page // 2 * 48, False)
            wait_page(device, page)
            tap(device, 160, 15, False)
            wait_page(device, 6)
            print(f"PASS page {page}: quick/held Home center, edges and recorded finger positions; quick hub taps")
        print("PASS: all six pages return Home; device left on Home")


if __name__ == "__main__":
    main()
