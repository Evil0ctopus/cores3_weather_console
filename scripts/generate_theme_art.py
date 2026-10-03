"""Generate original matching web/device artwork for the nine rebuilt themes."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SIZE = (640, 480)
THEMES = {
    "desert_calm": ("#f7eedf", "#ecd2ae", "#b67752", "#f4c579"),
    "future_pulse": ("#050617", "#191231", "#ff2fbf", "#00d8eb"),
    "midnight_radar": ("#03101c", "#112b38", "#50d6ef", "#7dff9b"),
    "daybreak_clear": ("#b9dafa", "#f8ead8", "#ffa752", "#ffffff"),
    "stormglass": ("#091720", "#294656", "#79c7ff", "#b2f7ef"),
    "aurora_line": ("#03110e", "#102c2b", "#6df7c1", "#7caeff"),
    "ocean_front": ("#041626", "#154461", "#4fc3f7", "#7ef0d1"),
    "mono_wireframe": ("#0c0e12", "#272d37", "#aab6c5", "#e4eaf2"),
    "infrared_scan": ("#16070a", "#3a151b", "#ff6b35", "#ffc857"),
}


def rgba(color: str, alpha: int = 255) -> tuple[int, int, int, int]:
    return (*tuple(int(color[i:i + 2], 16) for i in (1, 3, 5)), alpha)


def artwork(key: str, colors: tuple[str, str, str, str]) -> Image.Image:
    top, bottom, accent, secondary = colors
    gradient = Image.linear_gradient("L").resize(SIZE)
    image = Image.composite(Image.new("RGB", SIZE, bottom), Image.new("RGB", SIZE, top), gradient).convert("RGBA")
    glow = Image.new("RGBA", SIZE)
    gd = ImageDraw.Draw(glow)
    gd.ellipse((290, 50, 680, 390), fill=rgba(secondary, 42))
    image = Image.alpha_composite(image, glow.filter(ImageFilter.GaussianBlur(65)))
    layer = Image.new("RGBA", SIZE)
    draw = ImageDraw.Draw(layer)

    if key == "desert_calm":
        draw.ellipse((458, 54, 558, 154), fill=rgba(secondary, 210))
        for band, color in enumerate(("#e4bf90", "#d5a77a", "#b9805a", "#9d664d")):
            points = [(x, 290 + band * 45 + 29 * math.sin(x / 160 + band * 1.1)) for x in range(-2, 643, 2)]
            draw.polygon([(-2, 480), *points, (642, 480)], fill=rgba(color))
            draw.line(points, fill=rgba("#ffedd0", 110), width=2)
    elif key == "daybreak_clear":
        draw.ellipse((465, 62, 565, 162), fill=rgba(accent, 235))
        for x, y, w in ((-40, 225, 230), (355, 295, 300), (70, 395, 390)):
            draw.rounded_rectangle((x, y, x + w, y + 30), radius=15, fill=rgba("#ffffff", 130))
            draw.ellipse((x + w * .25, y - 35, x + w * .65, y + 25), fill=rgba("#ffffff", 130))
        draw.polygon([(0, 440), (210, 409), (375, 445), (640, 398), (640, 480), (0, 480)], fill=rgba("#90b9c9", 110))
    elif key == "future_pulse":
        for i in range(7):
            y = 72 + i * 56
            x = 360 + (i % 3) * 45
            draw.line([(0, y + 90), (x - 50, y + 90), (x, y + 40), (640, y + 40)], fill=rgba(accent if i % 2 else secondary, 100), width=2)
            draw.ellipse((x - 4, y + 36, x + 4, y + 44), fill=rgba(secondary, 220))
        draw.polygon([(445, 140), (525, 95), (605, 140), (605, 232), (525, 278), (445, 232)], outline=rgba(accent, 170), width=3)
        draw.polygon([(470, 155), (525, 123), (580, 155), (580, 217), (525, 248), (470, 217)], outline=rgba(secondary, 140), width=2)
    elif key == "midnight_radar":
        center = (500, 230)
        for r in (45, 85, 130, 180, 235, 295):
            draw.ellipse((center[0] - r, center[1] - r, center[0] + r, center[1] + r), outline=rgba(accent, 100), width=2)
        draw.pieslice((265, -5, 735, 465), 245, 280, fill=rgba(secondary, 24))
        draw.line((500, 230, 560, 2), fill=rgba(secondary, 190), width=2)
        draw.line((500, 0, 500, 480), fill=rgba(accent, 70), width=1)
        draw.line((0, 230, 640, 230), fill=rgba(accent, 70), width=1)
        for x, y in ((530, 120), (400, 280), (570, 335)):
            draw.ellipse((x - 4, y - 4, x + 4, y + 4), fill=rgba(secondary, 220))
    elif key == "stormglass":
        for x, y in ((70, 20), (355, 100), (570, 235)):
            draw.rounded_rectangle((x, y, x + 180, y + 260), radius=28, fill=rgba(secondary, 12), outline=rgba(secondary, 42), width=2)
        for i in range(44):
            x = (i * 83) % 730 - 45
            y = (i * 137) % 480
            draw.line((x, y, x - 15, y + 39), fill=rgba(accent, 55 + (i % 4) * 12), width=2)
    elif key == "aurora_line":
        ribbons = Image.new("RGBA", SIZE)
        rd = ImageDraw.Draw(ribbons)
        for band in range(6):
            points = [(x, 155 + band * 23 + 65 * math.sin(x / 140 + band * .22)) for x in range(-10, 651, 2)]
            rd.line(points, fill=rgba(accent if band % 2 else secondary, 95), width=12)
        layer = Image.alpha_composite(layer, ribbons.filter(ImageFilter.GaussianBlur(9)))
        draw = ImageDraw.Draw(layer)
        draw.polygon([(0, 450), (100, 370), (170, 418), (300, 335), (410, 415), (520, 365), (640, 435), (640, 480), (0, 480)], fill=rgba("#041411", 230))
    elif key == "ocean_front":
        draw.ellipse((485, 70, 535, 120), fill=rgba("#bde7f5", 185))
        for band in range(8):
            points = [(x, 220 + band * 33 + (5 + band * 2) * math.sin(x / 100 + band)) for x in range(-2, 643, 2)]
            draw.line(points, fill=rgba(accent if band % 2 else secondary, 70 + band * 9), width=2 + band // 3)
        for i in range(6):
            draw.line((495 - i * 10, 255 + i * 24, 520 + i * 9, 255 + i * 24), fill=rgba(secondary, 50), width=2)
    elif key == "mono_wireframe":
        for x in range(-400, 1100, 85):
            draw.line((320, 170, x, 480), fill=rgba(accent, 90), width=1)
        for i in range(1, 11):
            y = 170 + (i / 10) ** 2 * 310
            draw.line((0, y, 640, y), fill=rgba(accent, 90), width=1)
        draw.polygon([(375, 135), (475, 75), (565, 130), (565, 245), (465, 305), (375, 250)], outline=rgba(secondary, 130), width=2)
        draw.line([(375, 135), (465, 190), (565, 130), (465, 190), (465, 305)], fill=rgba(secondary, 130), width=2)
    else:
        heat = Image.new("RGBA", SIZE)
        hd = ImageDraw.Draw(heat)
        hd.ellipse((350, 30, 610, 370), fill=rgba(accent, 55))
        layer = Image.alpha_composite(layer, heat.filter(ImageFilter.GaussianBlur(35)))
        draw = ImageDraw.Draw(layer)
        for band in range(9):
            points = []
            for step in range(181):
                angle = step * math.tau / 180
                radius = 25 + band * 20 + 9 * math.sin(angle * 3 + band * .3)
                points.append((490 + math.cos(angle) * radius, 215 + math.sin(angle) * radius * .8))
            draw.line(points, fill=rgba(secondary if band < 3 else accent, 165 - band * 12), width=2)
        draw.line((0, 328, 640, 328), fill=rgba(secondary, 90), width=2)

    return Image.alpha_composite(image, layer).convert("RGB")


def main() -> None:
    directory = ROOT / "src" / "web" / "web_assets" / "backgrounds"
    device_directory = directory.parent / "themes"
    device_directory.mkdir(exist_ok=True)
    for key, colors in THEMES.items():
        target = directory / f"{key}.png"
        image = artwork(key, colors)
        image.save(target, optimize=True)
        image.resize((320, 240), Image.Resampling.LANCZOS).save(device_directory / f"{key}.png", optimize=True)
        print(f"{key}: original 640x480 artwork, {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
