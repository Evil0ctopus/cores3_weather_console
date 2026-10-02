from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SIZE = 384
OUTPUT_SIZE = 96
INK = (35, 58, 98, 255)
GREEN = (170, 101, 255, 255)
MINT = (35, 223, 255, 255)
PALE = (240, 247, 255, 255)
AMBER = (255, 186, 62, 255)
CORAL = (255, 104, 143, 255)


def gradient_fill(mask: Image.Image, top: tuple[int, int, int], bottom: tuple[int, int, int]) -> Image.Image:
    image = Image.new("RGBA", mask.size)
    draw = ImageDraw.Draw(image)
    for y in range(mask.height):
        fraction = y / (mask.height - 1)
        color = tuple(round(a + (b - a) * fraction) for a, b in zip(top, bottom))
        draw.line((0, y, mask.width, y), fill=(*color, 255))
    image.putalpha(mask)
    return image


def canvas() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def stroke(draw: ImageDraw.ImageDraw, points: list[tuple[int, int]], fill=INK, width=18) -> None:
    draw.line(points, fill=fill, width=width, joint="curve")


def draw_sun(draw: ImageDraw.ImageDraw, center=(192, 174), radius=68) -> None:
    x, y = center
    for index in range(8):
        angle = index * 45
        import math
        a = math.radians(angle)
        inner = (int(x + math.cos(a) * (radius + 24)), int(y + math.sin(a) * (radius + 24)))
        outer = (int(x + math.cos(a) * (radius + 49)), int(y + math.sin(a) * (radius + 49)))
        stroke(draw, [inner, outer], INK, 20)
        stroke(draw, [inner, outer], AMBER, 11)
    draw.ellipse((x-radius-8, y-radius-2, x+radius+8, y+radius+16), fill=(0,0,0,42))
    draw.ellipse((x-radius, y-radius, x+radius, y+radius), fill=INK)
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).ellipse((x-radius+9, y-radius+9, x+radius-9, y+radius-9), fill=255)
    draw._image.alpha_composite(gradient_fill(mask, (255, 242, 155), (255, 128, 38)))
    draw.arc((x-radius+18, y-radius+18, x+radius-18, y+radius-18), 205, 320, fill=(255,226,145,255), width=10)


def draw_moon(draw: ImageDraw.ImageDraw) -> None:
    mask = Image.new("L", (SIZE, SIZE), 0)
    m = ImageDraw.Draw(mask)
    m.ellipse((163, 35, 323, 195), fill=255)
    moon = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    moon_draw = ImageDraw.Draw(moon)
    moon_draw.ellipse((109, 67, 269, 227), fill=GREEN)
    image_mask = ImageChops.subtract(moon.getchannel("A"), mask)
    draw._image.alpha_composite(gradient_fill(image_mask, (231, 226, 255), (120, 118, 244)))
    for x, y, r in ((286, 92, 12), (302, 231, 8), (74, 273, 9)):
        draw.polygon(((x, y-r), (x+4, y-4), (x+r, y), (x+4, y+4), (x, y+r), (x-4, y+4), (x-r, y), (x-4, y-4)), fill=AMBER)


def cloud_layer() -> Image.Image:
    mask = Image.new("L", (SIZE, SIZE), 0)
    d = ImageDraw.Draw(mask)
    d.ellipse((48, 133, 204, 294), fill=255)
    d.ellipse((121, 75, 280, 270), fill=255)
    d.ellipse((210, 129, 340, 276), fill=255)
    d.rounded_rectangle((66, 188, 319, 290), radius=48, fill=255)

    outline_mask = mask.filter(ImageFilter.MaxFilter(19))
    outline = Image.new("RGBA", (SIZE, SIZE), INK)
    outline.putalpha(outline_mask)
    result = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    result.alpha_composite(outline)

    fill = gradient_fill(mask, (250, 253, 255), (105, 155, 222))
    result.alpha_composite(fill)
    hi = ImageDraw.Draw(result)
    hi.arc((75, 147, 193, 273), 192, 296, fill=PALE, width=10)
    hi.arc((143, 91, 260, 247), 205, 306, fill=PALE, width=10)
    return result


def draw_cloud(draw: ImageDraw.ImageDraw) -> None:
    draw._image.alpha_composite(cloud_layer())


def draw_partly(draw: ImageDraw.ImageDraw) -> None:
    draw_sun(draw, (253, 105), 40)
    draw._image.alpha_composite(cloud_layer())


def draw_rain(draw: ImageDraw.ImageDraw) -> None:
    draw_cloud(draw)
    for x in (118, 192, 266):
        stroke(draw, [(x+8, 302), (x-8, 337)], INK, 20)
        stroke(draw, [(x+8, 302), (x-8, 337)], MINT, 11)


def draw_storm(draw: ImageDraw.ImageDraw) -> None:
    draw_cloud(draw)
    bolt = [(208, 273), (165, 326), (199, 324), (177, 367), (247, 298), (211, 299)]
    draw.polygon([(x+7,y+8) for x,y in bolt], fill=INK)
    draw.polygon(bolt, fill=AMBER)
    stroke(draw, [(201, 330), (218, 323)], (255, 229, 151, 255), 7)


def draw_snow(draw: ImageDraw.ImageDraw) -> None:
    draw_cloud(draw)
    for x, y in ((120, 329), (192, 350), (265, 329)):
        for angle in (0, 60, 120):
            import math
            a = math.radians(angle)
            dx, dy = int(math.cos(a)*21), int(math.sin(a)*21)
            stroke(draw, [(x-dx,y-dy),(x+dx,y+dy)], INK, 13)
            stroke(draw, [(x-dx,y-dy),(x+dx,y+dy)], PALE, 7)


def draw_fog(draw: ImageDraw.ImageDraw) -> None:
    draw_cloud(draw)
    d = ImageDraw.Draw(draw._image)
    for y, offset, length in ((306, 0, 236), (333, 22, 202), (360, 2, 225)):
        d.rounded_rectangle((74+offset, y, 74+offset+length, y+14), radius=7, fill=INK)
        d.rounded_rectangle((80+offset, y-5, 80+offset+length-12, y+4), radius=5, fill=MINT)


def draw_wind(draw: ImageDraw.ImageDraw) -> None:
    d = ImageDraw.Draw(draw._image)
    for y, start, end in ((134, 74, 296), (208, 40, 270), (282, 83, 306)):
        pts=[]
        for i in range(65):
            t=i/64
            x=start+(end-start)*t
            yy=y+18*math.sin(t*math.pi*2)
            pts.append((int(x),int(yy)))
        stroke(d, pts, INK, 23)
        stroke(d, pts, MINT, 12)
        x=end-8
        d.arc((x-17,y-29,x+30,y+18), 268, 102, fill=INK, width=19)
        d.arc((x-14,y-26,x+27,y+14), 268, 102, fill=GREEN, width=9)


def draw_home(draw: ImageDraw.ImageDraw) -> None:
    d = ImageDraw.Draw(draw._image)
    roof=[(75,170),(192,70),(309,170)]
    stroke(d, roof, INK, 30); stroke(d, roof, GREEN, 15)
    d.rounded_rectangle((104,153,280,313),radius=10,fill=INK)
    d.rounded_rectangle((116,165,268,303),radius=5,fill=MINT)
    d.rounded_rectangle((170,222,214,303),radius=3,fill=INK)
    d.rounded_rectangle((128,190,157,220),radius=3,fill=(11,34,29,255))
    d.rounded_rectangle((225,190,254,220),radius=3,fill=(11,34,29,255))


def draw_radar(draw: ImageDraw.ImageDraw) -> None:
    d=ImageDraw.Draw(draw._image)
    for r in (49,93,138):
        d.ellipse((192-r,192-r,192+r,192+r),outline=INK,width=16)
        d.ellipse((192-r+7,192-r+7,192+r-7,192+r-7),outline=MINT,width=5)
    stroke(d,[(192,192),(293,84)],INK,20)
    stroke(d,[(192,192),(293,84)],GREEN,10)
    d.ellipse((176,176,208,208),fill=INK)
    d.ellipse((183,183,201,201),fill=AMBER)


def draw_alert(draw: ImageDraw.ImageDraw) -> None:
    d=ImageDraw.Draw(draw._image)
    triangle=[(192,51),(336,311),(48,311)]
    d.polygon([(x+7,y+9) for x,y in triangle],fill=INK)
    d.polygon(triangle,fill=CORAL)
    d.line((192,119,192,218),fill=INK,width=28)
    d.ellipse((178,243,206,271),fill=INK)
    d.line((192,119,192,209),fill=PALE,width=12)
    d.ellipse((183,238,201,256),fill=PALE)


def draw_settings(draw: ImageDraw.ImageDraw) -> None:
    d=ImageDraw.Draw(draw._image)
    d.ellipse((65,65,319,319),fill=INK)
    d.ellipse((82,82,302,302),fill=MINT)
    d.ellipse((143,143,241,241),fill=INK)
    d.ellipse((160,160,224,224),fill=(11,31,27,255))
    for angle in range(0,360,45):
        import math
        a=math.radians(angle)
        x1,y1=192+math.cos(a)*105,192+math.sin(a)*105
        x2,y2=192+math.cos(a)*148,192+math.sin(a)*148
        stroke(d,[(int(x1),int(y1)),(int(x2),int(y2))],INK,27)
        stroke(d,[(int(x1),int(y1)),(int(x2),int(y2))],MINT,16)


def draw_system(draw: ImageDraw.ImageDraw) -> None:
    d=ImageDraw.Draw(draw._image)
    d.rounded_rectangle((55,69,329,315),radius=22,fill=INK)
    d.rounded_rectangle((69,83,315,301),radius=14,fill=(15,43,36,255),outline=MINT,width=7)
    d.line((96,133,288,133),fill=MINT,width=8)
    d.line((96,171,248,171),fill=GREEN,width=8)
    d.line((96,209,276,209),fill=MINT,width=8)
    d.ellipse((95,249,116,270),fill=GREEN)
    d.ellipse((133,249,154,270),fill=AMBER)
    d.ellipse((171,249,192,270),fill=CORAL)


DRAWERS = {
    "sun": draw_sun,
    "moon": draw_moon,
    "partly_cloudy": draw_partly,
    "cloud": draw_cloud,
    "rain": draw_rain,
    "storm": draw_storm,
    "snow": draw_snow,
    "fog": draw_fog,
    "wind": draw_wind,
    "nav_home": draw_home,
    "nav_radar": draw_radar,
    "nav_alerts": draw_alert,
    "nav_settings": draw_settings,
    "nav_system": draw_system,
}


ALIASES = {
    "weather_clear_day": "sun",
    "weather_clear_night": "moon",
    "weather_partly_cloudy": "partly_cloudy",
    "weather_cloudy": "cloud",
    "weather_rain": "rain",
    "weather_storm": "storm",
    "weather_snow": "snow",
    "weather_fog": "fog",
    "weather_wind": "wind",
    "clear_day": "sun",
    "clear_night": "moon",
    "cloudy": "cloud",
    "thunderstorm": "storm",
    "brightness": "sun",
    "drought": "sun",
    "dust_storm": "wind",
    "flood": "rain",
    "hail": "storm",
    "heavy_rain": "rain",
    "home": "nav_home",
    "hurricane": "storm",
    "ice": "snow",
    "radar": "nav_radar",
    "sleet": "snow",
    "alerts": "nav_alerts",
    "settings": "nav_settings",
    "system": "nav_system",
    "theme": "nav_settings",
    "tornado": "storm",
    "volume": "nav_system",
    "wifi": "nav_system",
}


def render(name: str, output_size: int = OUTPUT_SIZE) -> Image.Image:
    image = Image.new("RGBA", (SIZE, SIZE), (0,0,0,0))
    drawer = ImageDraw.Draw(image)
    DRAWERS[name](drawer)
    bounds = image.getbbox()
    if bounds is None:
        raise ValueError(f"Icon {name} has no visible artwork")
    artwork = image.crop(bounds)
    edge = max(artwork.size)
    padding = round(edge * 0.08)
    framed = Image.new("RGBA", (edge + padding * 2, edge + padding * 2))
    framed.alpha_composite(artwork, (
        padding + (edge - artwork.width) // 2,
        padding + (edge - artwork.height) // 2,
    ))
    return framed.resize((output_size, output_size), Image.Resampling.LANCZOS)


def main() -> None:
    device_dir = ROOT / "src" / "web" / "web_assets" / "icons"
    source_dir = ROOT / "data" / "icons"
    contact = Image.new("RGBA", (5*OUTPUT_SIZE, 3*OUTPUT_SIZE), (5,9,25,255))
    native = bytearray(b"ATI1")
    for index, (name, drawer) in enumerate(DRAWERS.items()):
        image = render(name)
        image.save(device_dir / f"{name}.png", optimize=True)
        image.save(source_dir / f"{name}.png", optimize=True)
        for size in (22, 26, 34, 36, 48, 56):
            sized = render(name, size)
            rgba = sized.tobytes()
            for offset in range(0, len(rgba), 4):
                red, green, blue, alpha = rgba[offset:offset + 4]
                color = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
                native.extend((color & 255, color >> 8, alpha))
        contact.alpha_composite(image, ((index % 5)*OUTPUT_SIZE, (index // 5)*OUTPUT_SIZE))
    for alias, target in ALIASES.items():
        image = render(target)
        image.save(device_dir / f"{alias}.png", optimize=True)
        image.save(source_dir / f"{alias}.png", optimize=True)
    contact.convert("RGB").save(ROOT / "data" / "icons" / "weather_atlas_preview.png", optimize=True)
    (device_dir / "atlas.bin").write_bytes(native)
    print(f"Generated {len(DRAWERS)} master icons and {len(ALIASES)} compatible aliases at {OUTPUT_SIZE}x{OUTPUT_SIZE}.")
    print("Preview: data/icons/weather_atlas_preview.png")


if __name__ == "__main__":
    main()
