#!/usr/bin/env python3
"""Turns the artwork in assets/source (made with Meta AI) into the UI images in assets/ui.

    python3 tools/prepare_assets.py

- panel.jpg     the main panel, lightly cleaned of compression blocks
- knob.png      the knob cut out round (pointer straight up; the plug-in rotates it)
- key_off.png / key_on.png   keycaps cut out (drawn 9-slice into the key slots)
- rocker_off.png / rocker_on.png   the rocker switches without their labels, turned to fit the slots
- monitor.png   the About window monitor, background removed
- plastic.jpg   tileable case plastic
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageChops

ROOT = Path(__file__).resolve().parent.parent
SRC, OUT = ROOT / "assets" / "source", ROOT / "assets" / "ui"
OUT.mkdir(parents=True, exist_ok=True)


def load(name):
    return Image.open(SRC / name).convert("RGB")


def clean(im, blur=0.7):
    """Soften JPEG/WebP blocking, then bring the edges back."""
    return im.filter(ImageFilter.GaussianBlur(blur)).filter(ImageFilter.UnsharpMask(radius=2, percent=60, threshold=2))


def rounded_mask(size, radius, scale=4):
    w, h = size
    m = Image.new("L", (w * scale, h * scale), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, w * scale - 1, h * scale - 1), radius * scale, fill=255)
    return m.resize((w, h), Image.LANCZOS)


def remove_white(im, tol=20):
    """Flood-fills the near-white background from the corners to transparent."""
    rgba = im.convert("RGBA")
    w, h = rgba.size
    key = (255, 0, 255, 255)
    for c in [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)]:
        if min(rgba.getpixel(c)[:3]) > 255 - tol * 2:
            ImageDraw.floodfill(rgba, c, key, thresh=tol * 3)
    px = rgba.load()
    for y in range(h):
        for x in range(w):
            if px[x, y] == key:
                px[x, y] = (0, 0, 0, 0)
    # soften the cut edge a little
    a = rgba.getchannel("A").filter(ImageFilter.GaussianBlur(1.0))
    rgba.putalpha(a)
    return rgba


# ---------------------------------------------------------------- panel
panel = clean(load("panel.webp"))
panel.save(OUT / "panel.jpg", quality=92, optimize=True)

# ---------------------------------------------------------------- knob (centre 800,800, radius 566)
k = load("knob.webp")
cx, cy, r = 800, 800, 566
k = clean(k.crop((cx - r, cy - r, cx + r, cy + r)), 0.6).convert("RGBA")
mask = Image.new("L", (r * 8, r * 8), 0)
ImageDraw.Draw(mask).ellipse((0, 0, r * 8 - 1, r * 8 - 1), fill=255)
k.putalpha(mask.resize(k.size, Image.LANCZOS))
k.resize((320, 320), Image.LANCZOS).save(OUT / "knob.png", optimize=True)

# ---------------------------------------------------------------- keycaps
for name, box in [("key_off", (244, 290, 1334, 1340)), ("key_on", (256, 284, 1332, 1338))]:
    key = clean(load(name + ".webp").crop(box), 0.6).convert("RGBA")
    key.putalpha(rounded_mask(key.size, int(key.size[0] * 0.11)))
    key.resize((420, int(420 * key.size[1] / key.size[0])), Image.LANCZOS).save(OUT / (name + ".png"), optimize=True)

# ---------------------------------------------------------------- rocker switches (labels cropped off, turned 90 degrees)
rk = load("rocker.webp")
for name, box in [("rocker_off", (200, 144, 786, 1006)), ("rocker_on", (1134, 144, 1722, 1006))]:
    sw = remove_white(clean(rk.crop(box), 0.6))
    sw = sw.rotate(90, expand=True, resample=Image.BICUBIC)
    inset = Image.new("L", sw.size, 0)
    inset.paste(rounded_mask((sw.size[0] - 24, sw.size[1] - 24), 40), (12, 12))
    sw.putalpha(ImageChops.multiply(sw.getchannel("A"), inset))
    sw.resize((int(320 * sw.size[0] / sw.size[1]), 320), Image.LANCZOS).save(OUT / (name + ".png"), optimize=True)

# ---------------------------------------------------------------- About monitor
mon = remove_white(clean(load("monitor.webp").crop((300, 60, 1640, 1088)), 0.8))
mon.resize((1072, 822), Image.LANCZOS).save(OUT / "monitor.png", optimize=True)

# ---------------------------------------------------------------- plastic
load("plastic.webp").resize((512, 512), Image.LANCZOS).save(OUT / "plastic.jpg", quality=88)

print("assets written to", OUT)
