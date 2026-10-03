from __future__ import annotations

import argparse
import re
import statistics
import time
import threading
import urllib.request
from collections.abc import Generator
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager

import serial

from verify_home_touch import command


@contextmanager
def mirror_load(url: str | None) -> Generator[None]:
    if url is None:
        yield
        return
    stop = threading.Event()

    def poll() -> int:
        count = 0
        while not stop.is_set():
            with urllib.request.urlopen(url, timeout=5) as response:
                image = response.read()
                if response.status != 200 or image[:2] != b"BM" or len(image) < 54:
                    raise RuntimeError("Live mirror did not return a BMP")
                count += 1
            stop.wait(0.7)
        return count

    with ThreadPoolExecutor(max_workers=1) as executor:
        future = executor.submit(poll)
        try:
            yield
        finally:
            stop.set()
            count = future.result()
            if count == 0:
                raise RuntimeError("Live mirror was not exercised")
            print(f"PASS: {count} live mirror requests completed during navigation")


def status(device: serial.Serial) -> dict[str, int]:
    line = command(device, "STATUS", "DEVICE_STATUS")
    return {key: int(value) for key, value in re.findall(r"\b(\w+)=(\d+)\b", line)}


def main() -> None:
    parser = argparse.ArgumentParser(description="Measure completed CoreS3 screen transitions, not just command acceptance.")
    parser.add_argument("port")
    parser.add_argument("--cycles", type=int, default=5)
    parser.add_argument("--max-ms", type=float, default=350)
    parser.add_argument("--mirror-url", help="Exercise live screen capture concurrently, e.g. http://192.168.1.2/api/device/screen")
    parser.add_argument("--radar-refresh", action="store_true",
                        help="Refresh radar during every cycle, including an in-flight reset.")
    parser.add_argument("--max-network-ms", type=int, default=50,
                        help="Maximum UI-loop network phase during a radar refresh (default: 50).")
    args = parser.parse_args()
    if args.cycles < 1 or args.max_ms <= 0 or args.max_network_ms <= 0:
        parser.error("cycles, max-ms and max-network-ms must be positive")
    device = serial.Serial()
    device.port = args.port
    device.baudrate = 115200
    device.timeout = 0.25
    device.dtr = False
    device.rts = False
    times: list[float] = []
    with device, mirror_load(args.mirror_url):
        deadline = time.monotonic() + 45
        while True:
            sample = status(device)
            if sample["ready"] and (not args.radar_refresh or sample.get("radar_frames", 0) > 0):
                break
            if time.monotonic() >= deadline:
                raise TimeoutError("UI boot or initial radar download did not finish")
            time.sleep(0.5)
        for cycle in range(args.cycles):
            saw_radar_busy = False
            if args.radar_refresh:
                command(device, "RADAR_REFRESH")
                sample = status(device)
                saw_radar_busy = bool(sample["radar_busy"])
                if not saw_radar_busy:
                    raise RuntimeError("Radar download was not active after refresh")
                # Reset with a worker job in flight to catch stale-response publication.
                command(device, "RADAR_REFRESH")
            for page in (0, 6, 1, 6, 2, 6, 3, 6, 4, 6, 5, 6, 0, 1, 2, 3, 4, 5, 6):
                previous = status(device)["paint"]
                start = time.monotonic()
                command(device, f"PAGE {page}")
                deadline = start + 8
                while True:
                    sample = status(device)
                    if args.radar_refresh:
                        saw_radar_busy |= bool(sample["radar_busy"])
                        if sample["network_max"] >= args.max_network_ms:
                            raise RuntimeError(f"UI-loop network phase reached {sample['network_max']}ms")
                    if sample["page"] == page and sample["painted_page"] == page and sample["paint"] != previous:
                        break
                    if time.monotonic() >= deadline:
                        raise TimeoutError(f"Page {page} never finished painting")
                    time.sleep(0.005)
                elapsed = (time.monotonic() - start) * 1000
                times.append(elapsed)
                print(f"cycle={cycle + 1} page={page} painted={elapsed:.1f}ms "
                      f"loop={sample['loop']}ms input={sample['input']}ms "
                      f"network={sample['network']}ms content={sample['content']}ms "
                      f"audio={sample['audio_ms']}ms render={sample['render']}ms")
                time.sleep(0.15)
            if args.radar_refresh:
                deadline = time.monotonic() + 30
                while True:
                    sample = status(device)
                    if not sample["radar_busy"]:
                        break
                    if time.monotonic() >= deadline:
                        raise TimeoutError("Radar refresh never completed")
                    time.sleep(0.05)
                if sample["radar_error"] or sample["radar_frames"] < 1:
                    raise RuntimeError(f"Radar refresh failed: {sample}")
                if not saw_radar_busy or sample["network_max"] >= args.max_network_ms:
                    raise RuntimeError(f"Radar responsiveness check failed: {sample}")
                print(f"PASS: cycle {cycle + 1} radar refresh completed after in-flight reset; "
                      f"network peak={sample['network_max']}ms")
        print(f"Completed {len(times)} transitions: median={statistics.median(times):.1f}ms max={max(times):.1f}ms")
        if max(times) >= args.max_ms:
            raise RuntimeError(f"Completed transition exceeded {args.max_ms:.0f}ms")
        print("PASS: every requested page completed a display flush; device left on Home")


if __name__ == "__main__":
    main()
