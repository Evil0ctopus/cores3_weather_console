from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE_SIZE = (640, 480)
DEVICE_SIZE = (320, 240)
RNG = random.Random(70831)


def make_atlas() -> Image.Image:
    width, height = SOURCE_SIZE
    image = Image.new("RGB", SOURCE_SIZE)
    pixels = image.load()

    for y in range(height):
        depth = y / (height - 1)
        for x in range(width):
            pixels[x, y] = (
                max(0, 7 + int(depth * 4)),
                max(0, 17 + int(depth * 9)),
                max(0, 16 + int(depth * 7)),
            )

    grid = ImageDraw.Draw(image)
    for x in range(0, width, 48):
        major = x % 144 == 0
        color = (28, 59, 49) if major else (18, 39, 34)
        grid.line((x, 0, x, height), fill=color, width=1)
    for y in range(0, height, 48):
        major = y % 144 == 0
        color = (28, 59, 49) if major else (18, 39, 34)
        grid.line((0, y, width, y), fill=color, width=1)

    terrain = Image.new("RGBA", SOURCE_SIZE, (0, 0, 0, 0))
    terrain_draw = ImageDraw.Draw(terrain)
    land = [
        (0, 106), (81, 91), (139, 120), (161, 168), (128, 207),
        (148, 273), (104, 324), (78, 407), (0, 432),
    ]
    terrain_draw.polygon(land, fill=(34, 62, 48, 96))
    terrain_draw.line(land + [land[0]], fill=(91, 145, 93, 150), width=2)
    image = Image.alpha_composite(image.convert("RGBA"), terrain)
    draw = ImageDraw.Draw(image)

    for ring in range(8):
        base_x = 45 + ring * 11
        base_y = 229 + ring * 7
        points = []
        for step in range(97):
            angle = (step / 96) * math.tau
            wobble = 1 + 0.055 * math.sin(angle * 3 + ring) + 0.028 * math.cos(angle * 5 - ring)
            x = 103 + math.cos(angle) * base_x * wobble
            y = 226 + math.sin(angle) * base_y * wobble
            points.append((int(x), int(y)))
        shade = 44 + ring * 3
        draw.line(points, fill=(shade, shade + 33, shade + 22, 160), width=1)

    radar_center = (465, 185)
    for radius in (55, 111, 168, 225):
        draw.ellipse(
            (radar_center[0] - radius, radar_center[1] - radius,
             radar_center[0] + radius, radar_center[1] + radius),
            outline=(48, 111, 94, 125),
            width=1,
        )

    sweep = Image.new("RGBA", SOURCE_SIZE, (0, 0, 0, 0))
    sweep_draw = ImageDraw.Draw(sweep)
    sweep_draw.pieslice((241, -39, 689, 409), start=319, end=333, fill=(97, 221, 171, 36))
    sweep_draw.line((radar_center, (618, 29)), fill=(134, 238, 184, 170), width=2)
    image = Image.alpha_composite(image, sweep)
    draw = ImageDraw.Draw(image)

    storm = Image.new("RGBA", SOURCE_SIZE, (0, 0, 0, 0))
    storm_draw = ImageDraw.Draw(storm)
    storm_draw.ellipse((326, 76, 455, 184), fill=(35, 125, 107, 115))
    storm_draw.ellipse((376, 103, 515, 218), fill=(52, 178, 125, 135))
    storm_draw.ellipse((437, 61, 554, 151), fill=(180, 188, 84, 140))
    storm_draw.ellipse((465, 80, 527, 129), fill=(239, 174, 67, 185))
    image = Image.alpha_composite(image, storm)
    draw = ImageDraw.Draw(image)

    for _ in range(460):
        x = RNG.randrange(300, 555)
        y = RNG.randrange(54, 232)
        if not (x - 430) ** 2 / 140**2 + (y - 135) ** 2 / 100**2 < 1:
            continue
        color = RNG.choice(((72, 221, 166, 190), (166, 227, 96, 210), (245, 184, 74, 225)))
        draw.rectangle((x, y, x + 2, y + 2), fill=color)

    for _ in range(85):
        x = RNG.randrange(width)
        y = RNG.randrange(height)
        if RNG.random() < 0.55:
            draw.point((x, y), fill=(37, 78, 65, 180))

    draw.ellipse((radar_center[0] - 6, radar_center[1] - 6,
                  radar_center[0] + 6, radar_center[1] + 6),
                 fill=(242, 190, 89, 255), outline=(247, 250, 228, 255), width=2)
    draw.ellipse((radar_center[0] - 15, radar_center[1] - 15,
                  radar_center[0] + 15, radar_center[1] + 15),
                 outline=(242, 190, 89, 190), width=1)

    return image.convert("RGB")


def make_boot_frame(source: Image.Image, frame: int) -> Image.Image:
    from PIL import ImageDraw

    image = source.copy().convert("RGBA")
    draw = ImageDraw.Draw(image)
    center = (465, 185)
    angle = (frame / 60.0) * math.tau
    length = 222
    end = (int(center[0] + math.cos(angle) * length), int(center[1] + math.sin(angle) * length))
    draw.line((center, end), fill=(157, 232, 178, 230), width=4)
    draw.ellipse((center[0] - 9, center[1] - 9, center[0] + 9, center[1] + 9),
        fill=(244, 192, 91, 255), outline=(246, 251, 230, 255), width=3)
    for point in ((386, 117), (472, 83), (518, 145), (452, 227)):
        distance = math.hypot(point[0] - center[0], point[1] - center[1])
        bearing = math.atan2(point[1] - center[1], point[0] - center[0])
        if abs(math.atan2(math.sin(bearing - angle), math.cos(bearing - angle))) < 0.12:
            radius = max(5, int(12 - distance / 35))
            draw.ellipse((point[0]-radius, point[1]-radius, point[0]+radius, point[1]+radius),
                fill=(188, 234, 105, 255), outline=(239, 247, 210, 255), width=2)
    return image.convert("RGB").resize(DEVICE_SIZE, Image.Resampling.LANCZOS)


def recolor(source: Image.Image, tint: tuple[int, int, int], amount: float) -> Image.Image:
    overlay = Image.new("RGB", source.size, tint)
    return Image.blend(source.convert("RGB"), overlay, amount)


def save_device_png(image: Image.Image, path: Path) -> None:
    image.convert("RGB").quantize(colors=96, method=Image.Quantize.MEDIANCUT,
                                  dither=Image.Dither.NONE).save(path, optimize=True)


def main() -> None:
    source = make_atlas()
    source_path = ROOT / "data" / "backgrounds" / "weather_atlas_hub_source.png"
    source_background_dir = ROOT / "data" / "backgrounds"
    device_background_dir = ROOT / "src" / "web" / "web_assets" / "backgrounds"
    source_boot_dir = ROOT / "data" / "boot"
    device_boot_dir = ROOT / "src" / "web" / "web_assets" / "boot"
    source.save(source_path, optimize=True)
    atlas = source.resize(DEVICE_SIZE, Image.Resampling.LANCZOS)

    theme_tints = {
        "pixel_storm.png": (59, 100, 87), "bgps.png": (59, 100, 87),
        "desert_calm.png": (145, 126, 79), "bgdc.png": (145, 126, 79),
        "future_pulse.png": (61, 93, 121), "bgfp.png": (61, 93, 121),
        "bgmr.png": (53, 112, 111), "bgdb.png": (102, 135, 122),
        "bgsg.png": (64, 111, 94), "bgal.png": (60, 118, 95),
        "bgof.png": (51, 105, 124), "bgmw.png": (105, 112, 105),
        "bgis.png": (142, 89, 58),
    }
    page_art = {
        "current.png": (69, 116, 91), "weekly.png": (76, 106, 111),
        "radar.png": (62, 133, 104), "alerts.png": (144, 81, 67),
        "system.png": (70, 98, 86),
    }
    for name, tint in {**theme_tints, **page_art}.items():
        amount = 0.08 if name not in page_art else 0.12
        background = recolor(atlas, tint, amount)
        save_device_png(background, device_background_dir / name)
        background.save(source_background_dir / name, optimize=True)

    for frame in range(60):
        image = make_boot_frame(source, frame)
        filename = f"frame_{frame:03d}.png"
        save_device_png(image, device_boot_dir / filename)
        image.save(source_boot_dir / filename, optimize=True)

    print(f"Generated {source_path} ({SOURCE_SIZE[0]}x{SOURCE_SIZE[1]})")
    print(f"Replaced {len(theme_tints) + len(page_art)} themed/page backgrounds and 60 boot frames")


if __name__ == "__main__":
    main()
