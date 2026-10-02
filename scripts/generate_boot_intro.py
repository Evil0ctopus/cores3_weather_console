from __future__ import annotations

import math
import random
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SCALE = 4
WIDTH, HEIGHT = 320 * SCALE, 240 * SCALE


def generate_backdrop() -> Image.Image:
    image = Image.new("RGB", (WIDTH, HEIGHT))
    pixels = image.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            cyan = math.exp(-(((x - 330) / 420) ** 2 + ((y - 330) / 240) ** 2))
            violet = math.exp(-(((x - 940) / 360) ** 2 + ((y - 440) / 270) ** 2))
            pixels[x, y] = (
                round(4 + violet * 26 + cyan * 3),
                round(8 + cyan * 30 + violet * 3),
                round(22 + cyan * 35 + violet * 45),
            )
    aurora = Image.new("RGBA", image.size)
    draw = ImageDraw.Draw(aurora)
    for band in range(4):
        points = []
        for x in range(-40, WIDTH + 40, 4):
            y = 360 + band * 35 + 95 * math.sin(x / 260 + band * 0.4)
            points.append((x, round(y)))
        draw.line(points, fill=(35, 223, 255, 90) if band % 2 == 0 else (170, 101, 255, 100), width=18)
    image = Image.alpha_composite(image.convert("RGBA"), aurora.filter(ImageFilter.GaussianBlur(14)))
    draw = ImageDraw.Draw(image)
    rng = random.Random(2026)
    for _ in range(52):
        x, y = rng.randrange(WIDTH), rng.randrange(610)
        radius = rng.choice((1, 2, 3))
        draw.ellipse((x-radius, y-radius, x+radius, y+radius), fill=(153, 197, 245, rng.randrange(90, 210)))
    return image


def generate() -> Image.Image:
    image = generate_backdrop()
    glow = Image.new("RGBA", image.size)
    gd = ImageDraw.Draw(glow)
    gd.ellipse((410, 116, 870, 576), fill=(35, 223, 255, 45))
    image = Image.alpha_composite(image, glow.filter(ImageFilter.GaussianBlur(48)))
    draw = ImageDraw.Draw(image)
    draw.ellipse((430, 136, 850, 556), fill=(11, 23, 48), outline=(68, 106, 161), width=4)
    draw.arc((441, 147, 839, 545), 205, 310, fill=(150, 230, 255), width=5)
    draw.ellipse((588, 205, 750, 367), fill=(255, 186, 62), outline=(255, 239, 180), width=5)
    for angle in range(0, 360, 45):
        a = math.radians(angle)
        points = [(round(669 + math.cos(a) * r), round(286 + math.sin(a) * r)) for r in (96, 112)]
        draw.line(points, fill=(255, 206, 108), width=6)

    mask = Image.new("L", image.size)
    md = ImageDraw.Draw(mask)
    md.ellipse((480, 324, 628, 472), fill=255)
    md.ellipse((561, 264, 721, 452), fill=255)
    md.ellipse((659, 319, 794, 465), fill=255)
    md.rounded_rectangle((517, 384, 761, 470), radius=40, fill=255)
    cloud = Image.new("RGBA", image.size)
    cd = ImageDraw.Draw(cloud)
    for y in range(HEIGHT):
        t = min(1, max(0, (y - 264) / 206))
        cd.line((0, y, WIDTH, y), fill=(round(246-114*t), round(253-79*t), round(255-27*t), 255))
    cloud.putalpha(mask)
    image = Image.alpha_composite(image, cloud)
    draw = ImageDraw.Draw(image)
    draw.arc((576, 279, 707, 431), 206, 294, fill=(255, 255, 255), width=5)
    return image.resize((320, 240), Image.Resampling.LANCZOS).convert("RGB")


def main() -> None:
    assets = ROOT / "src" / "web" / "web_assets"
    target = assets / "boot" / "neon_intro.png"
    generate().save(target, optimize=True)
    print(f"Generated supersampled 320x240 boot artwork: {target.name}")
    backdrop = generate_backdrop().convert("RGB")
    native = backdrop.resize((320, 240), Image.Resampling.LANCZOS)
    native.save(assets / "backgrounds" / "neon_aurora.png", optimize=True)
    pixels = bytearray()
    source = native.tobytes()
    for index in range(0, len(source), 3):
        red, green, blue = source[index:index + 3]
        pixels.extend(struct.pack("<H", ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)))
    (assets / "backgrounds" / "aurora.rgb").write_bytes(pixels)
    backdrop.save(assets / "backgrounds" / "aurora_web.png", optimize=True)
    print("Generated matching device and high-resolution web aurora backdrops")


if __name__ == "__main__":
    main()
