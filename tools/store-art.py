#!/usr/bin/env python3
"""fenrikos store art: flat rune badges and product shots of the real panels, for Gumroad.

    tools/store-art.py <renders-dir> <fonts-dir> <out-root>

<fonts-dir> is common/fonts (Barlow, Barlow Condensed). Covers Fenix, AxisEQ and MOSAIC.

<renders-dir> holds 2x editor renders: hfcomp.png drive.png shelf.png width.png iron.png bus.png
axiseq.png mosaic.png oneknob.png. Writes <out-root>/<product>/{logo,logo-wordmark,thumbnail,cover-*}.png.
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

RENDERS, FONTS, OUT = (Path(a) for a in sys.argv[1:4])

PAINT = (29, 30, 27)          # graphite, as the faceplates
PAINT_HI = (37, 38, 35)
INK = (236, 229, 211)          # silkscreen cream
INK_DIM = (154, 147, 131)
INK_FAINT = (111, 106, 95)
SS = 4                         # supersampling for clean edges

RUNES = {
    "fehu": [(7, 2, 7, 18), (7, 8, 15, 2.5), (7, 12.5, 15, 7)],
    "ansuz": [(7, 2, 7, 18), (7, 2.4, 15, 8), (7, 7.6, 15, 13.2)],
    "mannaz": [(4.5, 2, 4.5, 18), (15.5, 2, 15.5, 18), (4.5, 2, 15.5, 10), (15.5, 2, 4.5, 10)],
}


def font(name, size):
    return ImageFont.truetype(str(FONTS / name), size)


def legend(size):
    return font("BarlowCondensed-Bold.ttf", size)


def legend_semi(size):
    return font("BarlowCondensed-SemiBold.ttf", size)


def text_font(size):
    return font("Barlow-Medium.ttf", size)


def tracked(draw, xy, text, fnt, fill, tracking=0.0, anchor_right=False):
    """Draw text with letter spacing (tracking as a fraction of the font size)."""
    x, y = xy
    widths = [draw.textlength(c, font=fnt) for c in text]
    total = sum(widths) + tracking * fnt.size * (len(text) - 1)
    if anchor_right:
        x -= total
    for c, w in zip(text, widths):
        draw.text((x, y), c, font=fnt, fill=fill)
        x += w + tracking * fnt.size
    return total


def tracked_width(draw, text, fnt, tracking):
    return sum(draw.textlength(c, font=fnt) for c in text) + tracking * fnt.size * (len(text) - 1)


def grain(img, amount=6, seed=3):
    """Fine matte grain so flat paint does not look like a digital fill."""
    import random
    rnd = random.Random(seed)
    px = img.load()
    w, h = img.size
    for _ in range(w * h // 7):
        x, y = rnd.randrange(w), rnd.randrange(h)
        d = rnd.randint(-amount, amount)
        r, g, b = px[x, y][:3]
        px[x, y] = (max(0, min(255, r + d)), max(0, min(255, g + d)), max(0, min(255, b + d))) + px[x, y][3:]
    return img


def plate_background(w, h):
    img = Image.new("RGB", (w, h), PAINT)
    d = ImageDraw.Draw(img)
    for y in range(h):  # very gentle fall-off from the top light, no colour
        t = y / max(1, h - 1)
        c = tuple(int(PAINT_HI[i] * (1 - t) + PAINT[i] * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=c)
    return grain(img)


def rune_layer(strokes, box, colour, thickness):
    """A rune as round-capped strokes, drawn into an RGBA layer the size of `box` (x, y, size)."""
    x0, y0, size = box
    layer = Image.new("RGBA", (size * SS, size * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    k = size * SS / 20.0
    w = thickness * k
    for x1, y1, x2, y2 in strokes:
        a, b = (x1 * k, y1 * k), (x2 * k, y2 * k)
        d.line([a, b], fill=colour, width=int(w))
        for p in (a, b):
            d.ellipse([p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2], fill=colour)
    return layer.resize((size, size), Image.LANCZOS), (x0, y0)


def badge(rune, size, ring=True):
    """Maker's badge: graphite disc, a printed cream ring, the rune printed flat in cream."""
    S = size * SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pad = int(S * 0.03)
    d.ellipse([pad, pad, S - pad, S - pad], fill=PAINT + (255,))
    # a hairline lighter edge on top, like a pressed metal badge catching light
    d.arc([pad, pad, S - pad, S - pad], 200, 340, fill=(70, 70, 64, 255), width=max(2, S // 160))
    if ring:
        inset = int(S * 0.095)
        d.ellipse([inset, inset, S - inset, S - inset], outline=INK + (255,), width=max(2, S // 70))
    img = img.resize((size, size), Image.LANCZOS)
    glyph, _ = rune_layer(RUNES[rune], (0, 0, int(size * 0.5)), INK + (255,), 2.6)
    img.alpha_composite(glyph, (int(size * 0.25), int(size * 0.25)))
    return img


def drop_shadow(img, radius=18, offset=(0, 14), opacity=170):
    alpha = img.split()[-1] if img.mode == "RGBA" else Image.new("L", img.size, 255)
    pad = radius * 3
    sh = Image.new("RGBA", (img.width + 2 * pad, img.height + 2 * pad), (0, 0, 0, 0))
    mask = Image.new("L", sh.size, 0)
    mask.paste(alpha.point(lambda v: v * opacity // 255), (pad, pad))
    sh.putalpha(mask.filter(ImageFilter.GaussianBlur(radius)))
    return sh, (offset[0] - pad, offset[1] - pad)


def place(canvas, unit, xy, shadow=True):
    unit = unit.convert("RGBA")
    if shadow:
        sh, (dx, dy) = drop_shadow(unit)
        canvas.alpha_composite(sh, (xy[0] + dx, xy[1] + dy))
    canvas.alpha_composite(unit, xy)


def fit(img, w=None, h=None):
    if w is not None:
        return img.resize((w, round(img.height * w / img.width)), Image.LANCZOS)
    return img.resize((round(img.width * h / img.height), h), Image.LANCZOS)


#==============================================================================
def logo_files(out, rune, name, sub):
    out.mkdir(parents=True, exist_ok=True)
    badge(rune, 1024).save(out / "logo.png")
    # Horizontal lockup: badge, then the name and a sub-legend, as printed on the faceplates.
    W, H = 1800, 520
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.alpha_composite(badge(rune, 400), (40, 60))
    d = ImageDraw.Draw(img)
    tracked(d, (500, 92), name, legend(210), INK, 0.06)
    tracked(d, (508, 340), sub, text_font(52), INK_DIM, 0.22)
    img.save(out / "logo-wordmark.png")


def header(canvas, rune, name, sub, right="fenrikos"):
    d = ImageDraw.Draw(canvas)
    canvas.alpha_composite(badge(rune, 64, ring=False), (52, 40))
    tracked(d, (132, 38), name, legend(44), INK, 0.08)
    tracked(d, (134, 88), sub, text_font(16), INK_DIM, 0.22)
    tracked(d, (canvas.width - 52, 56), right, text_font(16), INK_FAINT, 0.32, anchor_right=True)


def footer(canvas, line):
    d = ImageDraw.Draw(canvas)
    tracked(d, (52, canvas.height - 50), line, text_font(16), INK_DIM, 0.2)


def cover(out_file, render, rune, name, sub, line, max_w=1176, max_h=480):
    canvas = plate_background(1280, 720).convert("RGBA")
    unit = Image.open(render)
    unit = fit(unit, w=max_w) if unit.width / unit.height > max_w / max_h else fit(unit, h=max_h)
    place(canvas, unit, ((1280 - unit.width) // 2, 140 + (max_h - unit.height) // 2))
    header(canvas, rune, name, sub)
    footer(canvas, line)
    canvas.convert("RGB").save(out_file)


def thumbnail(out_file, render, crop, rune, name, sub):
    """Close product shot: a crop of the real faceplate filling the top, the name printed below."""
    canvas = plate_background(600, 600).convert("RGBA")
    unit = Image.open(render).convert("RGBA")
    x0, y0, x1, y1 = [int(v * unit.width) if i % 2 == 0 else int(v * unit.height) for i, v in enumerate(crop)]
    shot = unit.crop((x0, y0, x1, y1))
    shot = fit(shot, w=600)
    if shot.height > 440:
        shot = shot.crop((0, 0, 600, 440))
    canvas.alpha_composite(shot, (0, 0))
    d = ImageDraw.Draw(canvas)
    d.rectangle([0, shot.height, 600, shot.height + 1], fill=(0, 0, 0, 255))
    d.rectangle([0, shot.height + 2, 600, shot.height + 2], fill=(255, 255, 255, 18))
    y = shot.height + (600 - shot.height) // 2 - 40
    canvas.alpha_composite(badge(rune, 72), (36, y + 4))
    tracked(d, (126, y - 4), name, legend(58), INK, 0.06)
    tracked(d, (128, y + 58), sub, text_font(17), INK_DIM, 0.2)
    canvas.convert("RGB").save(out_file)


def suite_cover(out_file, renders, line):
    """The Fenix units stacked as they would sit in a rack, running off the bottom edge."""
    canvas = plate_background(1280, 720).convert("RGBA")
    y = 140
    for r in renders:
        unit = fit(Image.open(r), w=1176)
        place(canvas, unit, (52, y))
        y += unit.height + 4
        if y > 720:
            break
    header(canvas, "fehu", "FENIX", "COLOUR SERIES  ·  SIX MIX-BUS PROCESSORS")
    # Footer over a dark band so it stays legible over the units.
    band = Image.new("RGBA", (1280, 80), (20, 21, 19, 235))
    canvas.alpha_composite(band, (0, 640))
    footer(canvas, line)
    canvas.convert("RGB").save(out_file)


#==============================================================================
FORMATS = "VST3  ·  AU  ·  LV2      WINDOWS  ·  MACOS  ·  LINUX"
R = lambda n: RENDERS / f"{n}.png"

fenix = OUT / "fenix"
logo_files(fenix, "fehu", "FENIX", "COLOUR SERIES")
suite_cover(fenix / "cover-1-suite.png", [R("hfcomp"), R("drive"), R("iron"), R("shelf")], FORMATS)
cover(fenix / "cover-2-bus.png", R("bus"), "fehu", "FENIX BUS", "FIVE-STAGE COLOUR CHAIN", FORMATS)
for i, (key, name, sub) in enumerate([("drive", "FENIX DRIVE", "HARMONIC SATURATOR"),
                                      ("shelf", "FENIX SHELF EQ", "BROAD SHELVING EQUALISER"),
                                      ("hfcomp", "FENIX HF COMP", "HIGH-FREQUENCY COMPRESSOR"),
                                      ("width", "FENIX WIDTH", "STEREO IMAGE PROCESSOR"),
                                      ("iron", "FENIX IRON", "OUTPUT TRANSFORMER")], start=3):
    cover(fenix / f"cover-{i}-{key}.png", R(key), "fehu", name, sub, FORMATS, max_h=300)
thumbnail(fenix / "thumbnail.png", R("drive"), (0.0, 0.0, 0.62, 1.0), "fehu", "FENIX", "COLOUR SERIES  ·  6 PLUGINS")

axis = OUT / "axiseq"
logo_files(axis, "ansuz", "AXIS EQ", "24-BAND DYNAMIC EQUALISER")
cover(axis / "cover-1-axiseq.png", R("axiseq"), "ansuz", "AXIS EQ", "24-BAND DYNAMIC EQUALISER", FORMATS, max_h=500)
thumbnail(axis / "thumbnail.png", R("axiseq"), (0.0, 0.0, 0.62, 0.6), "ansuz", "AXIS EQ", "24-BAND DYNAMIC EQ")

mosaic = OUT / "mosaic"
logo_files(mosaic, "mannaz", "MOSAIC", "MODULAR PAD SAMPLER")
cover(mosaic / "cover-1-mosaic.png", R("mosaic"), "mannaz", "MOSAIC", "MODULAR PAD SAMPLER", "VST3  ·  AU  ·  STANDALONE      WINDOWS  ·  MACOS  ·  LINUX", max_h=500)
thumbnail(mosaic / "thumbnail.png", R("mosaic"), (0.0, 0.47, 0.66, 1.0), "mannaz", "MOSAIC", "MODULAR PAD SAMPLER")
print("written to", OUT)
