#!/usr/bin/env python3
"""Draw the app icons for the standalones that ship in the installers.

    python3 scripts/make_app_icons.py

Writes resources/icons/<app>.png (1024x1024, RGBA). Subproject.cmake passes
an existing resources/icons/<target>.png to juce_add_plugin as ICON_BIG /
ICON_SMALL, and JUCE turns it into the .icns / .ico. Re-run after changing
the drawing; the PNGs are checked in so a build needs no Pillow.

The shapes follow the apps' own UI: the green level meters of mcfx_send /
mcfx_receive, and the nodes, orange/blue ports and white cables of
mcfx_graph, on a dark rounded square in the macOS icon grid (824 px body on
a 1024 px canvas, with a soft shadow).
"""

import math
import os

from PIL import Image, ImageDraw, ImageFilter

SIZE = 1024
SS = 4                      # supersampling factor for smooth edges
S = SIZE * SS

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "resources", "icons")

# macOS icon grid: 824 px body centred on the 1024 canvas.
BODY = (100, 100, 924, 924)
RADIUS = 185

BG_TOP = (38, 46, 62)
BG_BOTTOM = (13, 17, 28)
BORDER = (58, 70, 92)

GREEN = (123, 196, 127)
GREEN_DARK = (74, 140, 80)
CYAN = (79, 195, 247)
CYAN_DARK = (40, 120, 170)
ORANGE = (245, 165, 36)
PORT_IN = (143, 211, 244)
WHITE = (240, 244, 250)


def s(v):
    """Scale a design-space value (1024 grid) to the supersampled canvas."""
    return v * SS


def sbox(box):
    return tuple(s(v) for v in box)


def vertical_gradient(size, top, bottom):
    w, h = size
    grad = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / (h - 1)
        grad.putpixel((0, y), tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)))
    return grad.resize((w, h))


def base():
    """Transparent canvas with the shadowed, gradient-filled rounded square."""
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))

    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        sbox((BODY[0], BODY[1] + 12, BODY[2], BODY[3] + 12)), s(RADIUS), fill=(0, 0, 0, 140))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(s(18))))

    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle(sbox(BODY), s(RADIUS), fill=255)
    body = vertical_gradient((S, S), BG_TOP, BG_BOTTOM).convert("RGBA")
    img.paste(body, (0, 0), mask)

    ImageDraw.Draw(img).rounded_rectangle(sbox(BODY), s(RADIUS), outline=BORDER, width=s(4))
    return img


def meter(draw, x0, x1, floor, levels, colour, colour_dark):
    """A row of level-meter bars between x0..x1, bottoms on `floor`.
    levels: bar heights (design px); each gets a peak tick above it."""
    n = len(levels)
    gap = 18
    w = (x1 - x0 - gap * (n - 1)) / n
    for i, h in enumerate(levels):
        bx = x0 + i * (w + gap)
        draw.rounded_rectangle(sbox((bx, floor - h, bx + w, floor)), s(10), fill=colour)
        # darker lower half, like the meter's gradient
        draw.rounded_rectangle(sbox((bx, floor - h * 0.45, bx + w, floor)), s(10), fill=colour_dark)
        draw.rectangle(sbox((bx, floor - h * 0.45, bx + w, floor - h * 0.45 + 12)), fill=colour_dark)
        peak = floor - h - 34
        draw.rounded_rectangle(sbox((bx + 4, peak, bx + w - 4, peak + 12)), s(4), fill=WHITE)


def waves(img, cx, cy, radii, start, end, colour, width=40):
    """Concentric arcs around (cx, cy) with round ends, fading outwards.
    Each arc is drawn opaque on its own layer and faded as a whole, so the
    caps don't double up where they overlap the arc."""
    for i, r in enumerate(radii):
        layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
        d = ImageDraw.Draw(layer)
        # PIL draws the stroke inward from the bounding box; centre it on r.
        o = r + width / 2
        d.arc(sbox((cx - o, cy - o, cx + o, cy + o)), start, end, fill=colour, width=s(width))
        for a in (start, end):
            px = cx + r * math.cos(math.radians(a))
            py = cy + r * math.sin(math.radians(a))
            rr = width / 2
            d.ellipse(sbox((px - rr, py - rr, px + rr, py + rr)), fill=colour)
        alpha = 1.0 - 0.28 * i
        layer.putalpha(layer.getchannel("A").point(lambda v: round(v * alpha)))
        img.alpha_composite(layer)


def send_icon():
    img = base()
    d = ImageDraw.Draw(img)
    meter(d, 215, 545, 760, [330, 250, 400, 290, 360], GREEN, GREEN_DARK)
    radii = [95, 175, 255]
    cx, cy, span = 575, 525, 42
    waves(img, cx, cy, radii, -span, span, GREEN)
    return img


def receive_icon():
    img = base()
    d = ImageDraw.Draw(img)
    meter(d, 479, 809, 760, [300, 380, 260, 350, 310], CYAN, CYAN_DARK)
    radii = [95, 175, 255]
    cx, cy, span = 449, 525, 42
    waves(img, cx, cy, radii, 180 - span, 180 + span, CYAN)
    return img


def bezier(p0, p1, p2, p3, steps=200):
    pts = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        x = u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0]
        y = u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]
        pts.append((s(x), s(y)))
    return pts


def cable(draw, a, b, width=16):
    """A cable as a chain of discs along the curve: a smooth, round-capped
    stroke (PIL's wide polylines break up at the joints)."""
    dx = (b[0] - a[0]) * 0.55
    r = s(width / 2)
    for x, y in bezier(a, (a[0] + dx, a[1]), (b[0] - dx, b[1]), b, steps=800):
        draw.ellipse((x - r, y - r, x + r, y + r), fill=WHITE)


def node(draw, box, colour, title, ports_in, ports_out):
    x0, y0, x1, y1 = box
    draw.rounded_rectangle(sbox(box), s(26), fill=colour)
    draw.rounded_rectangle(sbox((x0, y0, x1, y0 + 56)), s(26), fill=title)
    draw.rectangle(sbox((x0, y0 + 30, x1, y0 + 56)), fill=title)
    r = 22
    for y in ports_in:
        draw.ellipse(sbox((x0 - r, y - r, x0 + r, y + r)), fill=PORT_IN)
    for y in ports_out:
        draw.ellipse(sbox((x1 - r, y - r, x1 + r, y + r)), fill=ORANGE)


def graph_icon():
    img = base()
    d = ImageDraw.Draw(img)

    a = (190, 300, 390, 720)      # input node, left
    b = (625, 215, 830, 470)      # upper right
    c = (625, 560, 830, 810)      # lower right
    a_out = [420, 510, 600]
    b_in, c_in = [330, 410], [680, 750]

    # cables under the nodes so the port dots sit on top
    cable(d, (a[2], a_out[0]), (b[0], b_in[0]))
    cable(d, (a[2], a_out[1]), (b[0], b_in[1]))
    cable(d, (a[2], a_out[1]), (c[0], c_in[0]))
    cable(d, (a[2], a_out[2]), (c[0], c_in[1]))

    node(d, a, (40, 62, 92), (52, 80, 118), [], a_out)
    node(d, b, (52, 80, 122), (70, 104, 156), b_in, [])
    node(d, c, (70, 56, 44), (104, 80, 60), c_in, [])
    return img


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, draw_fn in (("mcfx_send", send_icon),
                          ("mcfx_receive", receive_icon),
                          ("mcfx_graph", graph_icon)):
        path = os.path.join(OUT_DIR, name + ".png")
        draw_fn().resize((SIZE, SIZE), Image.LANCZOS).save(path, optimize=True)
        print("wrote", os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main()
