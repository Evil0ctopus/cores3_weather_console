from __future__ import annotations

import argparse
import time
from pathlib import Path

import serial


def main() -> None:
    parser = argparse.ArgumentParser(description="Capture the CoreS3 display over USB serial.")
    parser.add_argument("port", help="Serial port, for example COM3")
    parser.add_argument("output", nargs="?", default="device-screen.bmp")
    args = parser.parse_args()

    output = Path(args.output)
    device = serial.Serial()
    device.port = args.port
    device.baudrate = 115200
    device.timeout = 0.25
    device.dtr = False
    device.rts = False
    with device:
        device.reset_input_buffer()
        device.write(b"SCREEN_BMP\n")
        deadline = time.monotonic() + 45
        image_length = None
        frame_count = 0
        while time.monotonic() < deadline:
            line = device.readline().strip()
            if line.startswith(b"SCREEN_BMP "):
                fields = line.split()
                image_length = int(fields[1])
                if len(fields) > 2:
                    frame_count = int(fields[2])
                break
            if line == b"SCREEN_ERROR":
                raise RuntimeError("Firmware could not allocate a screenshot buffer.")

        if image_length is None:
            raise TimeoutError("No SCREEN_BMP response. Check COM port and use the flashed firmware.")

        image_data = bytearray()
        while len(image_data) < image_length and time.monotonic() < deadline:
            image_data.extend(device.read(image_length - len(image_data)))
        if len(image_data) != image_length:
            raise TimeoutError(f"Incomplete screenshot: received {len(image_data)} of {image_length} bytes.")

    output.write_bytes(image_data)
    print(f"Saved {output.resolve()} ({image_length} bytes, {frame_count} display flushes)")


if __name__ == "__main__":
    main()