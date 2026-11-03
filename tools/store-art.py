#!/usr/bin/env python3
"""Builds store artwork from 2x editor snapshots.

    tools/store-art.py <snapshots-dir> <out-dir>

Expects bus.png, drive.png, shelf.png, hfcomp.png, width.png and iron.png rendered with
`<Plugin>_snapshot <file> <preset> --scale 2`. Writes 1280x720 covers and a 600x600 thumbnail.
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1280, 720
BG_TOP, BG_BOTTOM = (30, 33, 39), (17, 19, 23)
TEXT, DIM = (226, 229, 234), (134, 140, 151)
FONT_DIR = Path("/usr/share/fonts/noto")

PLUGINS = [
    ("bus", "Fenix Bus", "Five-stage colour chain", (255, 106, 61)),
    ("drive", "Fenix Drive", "Harmonic saturator", (232, 116, 59)),
    ("shelf", "Fenix Shelf EQ", "Broad shelving equaliser", (155, 124, 240)),
    ("hfcomp", "Fenix HF Comp", "High-frequency compressor", (79, 195, 217)),
    ("width", "Fenix Width", "Stereo image processor", (108, 207, 138)),
    ("iron", "Fenix Iron", "Output transformer", (217, 180, 74)),
]


def font(weight, size):
    return ImageFont.truetype(str(FONT_DIR / f"NotoSans-{weight}.ttf"), size)


def background(width, height, accent):
    """Vertical gradient with a soft accent glow from the top edge."""
    img = Image.new("RGB", (width, height))
    px = img.load()
    for y in range(height):
        t = y / (height - 1)
        glow = max(0.0, 1.0 - y / (height * 0.45)) * 0.18
        row = tuple(int(BG_TOP[c] + (BG_BOTTOM[c] - BG_TOP[c]) * t + accent[c] * glow) for c in range(3))
        for x in range(width):
            px[x, y] = row
    ImageDraw.Draw(img).rectangle([0, 0, width, 3], fill=accent)
    return img


def place_screenshot(canvas, shot, box_height, top):
    """Pastes `shot` scaled to `box_height`, centred, with rounded corners and a drop shadow."""
    scale = box_height / shot.height
    w, h = int(shot.width * scale), box_height
    w = min(w, canvas.width - 48)
    h = int(shot.height * w / shot.width)
    shot = shot.convert("RGB").resize((w, h), Image.LANCZOS)
    x = (canvas.width - w) // 2

    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius=18, fill=255)

    shadow = Image.new("L", (w + 80, h + 80), 0)
    ImageDraw.Draw(shadow).rounded_rectangle([40, 52, w + 40, h + 52], radius=24, fill=150)
    shadow = shadow.filter(ImageFilter.GaussianBlur(22))
    canvas.paste((0, 0, 0), (x - 40, top - 40), shadow)
    canvas.paste(shot, (x, top), mask)


def flame(draw, x, y, w, h, colour):
    """The Fenix mark as a polygon, sampled from the same cubic curves the plugins draw."""
    segments = [
        ((0.50, 0.00), (0.62, 0.28), (1.02, 0.42), (0.92, 0.74)),
        ((0.92, 0.74), (0.86, 0.92), (0.70, 1.00), (0.50, 1.00)),
        ((0.50, 1.00), (0.30, 1.00), (0.14, 0.92), (0.08, 0.74)),
        ((0.08, 0.74), (0.00, 0.48), (0.22, 0.40), (0.30, 0.18)),
        ((0.30, 0.18), (0.36, 0.38), (0.42, 0.46), (0.50, 0.50)),
        ((0.50, 0.50), (0.52, 0.30), (0.46, 0.14), (0.50, 0.00)),
    ]
    points = []
    for p0, p1, p2, p3 in segments:
        for i in range(24):
            t = i / 24
            a, b, c, d = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t ** 2, t ** 3
            points.append((x + w * (a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0]),
                           y + h * (a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1])))
    draw.polygon(points, fill=colour)


def centred_text(draw, y, text, fnt, fill, width=W, spacing=0):
    if spacing:
        total = sum(draw.textlength(ch, font=fnt) + spacing for ch in text) - spacing
        x = (width - total) / 2
        for ch in text:
            draw.text((x, y), ch, font=fnt, fill=fill)
            x += draw.textlength(ch, font=fnt) + spacing
    else:
        draw.text(((width - draw.textlength(text, font=fnt)) / 2, y), text, font=fnt, fill=fill)


def hero(shots, out):
    accent = PLUGINS[0][3]
    img = background(W, H, accent)
    d = ImageDraw.Draw(img)
    flame(d, W / 2 - 236, 34, 34, 50, (255, 196, 90))
    d.text((W / 2 - 190, 28), "FENIX", font=font("Black", 50), fill=TEXT)
    d.text((W / 2 + 6, 44), "COLOUR SUITE", font=font("Bold", 30), fill=accent)
    centred_text(d, 100, "Six mix-bus colour plugins  ·  VST3 + LV2  ·  Linux", font("Medium", 22), DIM)
    place_screenshot(img, shots["bus"], 540, 158)
    img.save(out / "cover-1-suite.png", optimize=True)


def plugin_cover(index, key, name, tagline, accent, shot, out):
    img = background(W, H, accent)
    d = ImageDraw.Draw(img)
    centred_text(d, 26, name.upper(), font("Black", 44), TEXT, spacing=4)
    centred_text(d, 86, tagline, font("Medium", 22), accent)
    place_screenshot(img, shot, 570, 136)
    img.save(out / f"cover-{index}-{key}.png", optimize=True)


def thumbnail(out):
    size = 600
    accent = PLUGINS[0][3]
    img = background(size, size, accent)
    d = ImageDraw.Draw(img)
    flame(d, size / 2 - 70, 120, 140, 205, (255, 196, 90))
    centred_text(d, 352, "FENIX", font("Black", 92), TEXT, width=size, spacing=6)
    centred_text(d, 470, "COLOUR SUITE", font("Bold", 34), accent, width=size, spacing=3)
    img.save(out / "thumbnail.png", optimize=True)


def main():
    shots_dir, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    shots = {key: Image.open(shots_dir / f"{key}.png") for key, *_ in PLUGINS}

    hero(shots, out)
    for i, (key, name, tagline, accent) in enumerate(PLUGINS, start=2):
        plugin_cover(i, key, name, tagline, accent, shots[key], out)
    thumbnail(out)


if __name__ == "__main__":
    main()
