"""Build Cassque's icon and About picture: a cask with a cassowary's casque on it.

Produces:
  Resources/Icons/Cassque.png   - 1024x1024 master PNG
  Resources/Icons/Cassque.ico   - Multi-resolution ICO (16, 24, 32, 48, 64, 128, 256)
  Cassque/Assets/Cassque.png    - 256x256, the picture in Cassque's About box

The casque is the top of the same cassowary silhouette Casso's icons use.
"""
import math
from pathlib import Path

from PIL import Image, ImageDraw

import build_icons as bi

HERE = Path(__file__).parent
ABOUT_PNG = HERE.parent.parent / "Cassque" / "Assets" / "Cassque.png"

MASTER = 1024
DARK = (18, 16, 22)
CREAM = (245, 235, 210)
WOOD = (150, 94, 52)
WOOD_DARK = (104, 62, 32)
WOOD_LIGHT = (184, 124, 74)
HOOP = (70, 70, 78)
HOOP_LIGHT = (120, 120, 130)

# An ellipse's height over its width: how far above the barrel the eye is.
ASPECT = 0.22


# ---------- the barrel -------------------------------------------------------
#
# The barrel is drawn upright, seen from a little above: the top is an ellipse,
# so every horizontal section is an ellipse of the same aspect, which makes the
# bottom edge and each hoop the lower half of one. The icon turns that drawing
# a quarter turn, so the perspective on its side is consistent by construction.

def half_width(t, w, bulge):
    """Half the barrel's width at fraction t from top (0) to bottom (1)."""
    k = 1.0 - (2.0 * t - 1.0) ** 2
    return w / 2.0 - bulge * (1.0 - k)


def lower_arc(cx, cy, rx, steps=48, reverse=False):
    """The lower half of an ellipse, right to left unless reversed."""
    ry = rx * ASPECT
    pts = [(cx + rx * math.cos(math.pi * i / steps), cy + ry * math.sin(math.pi * i / steps))
           for i in range(steps + 1)]
    return pts[::-1] if reverse else pts


def upper_arc(cx, cy, rx, steps=48):
    """The upper half of an ellipse, left to right."""
    ry = rx * ASPECT
    return [(cx - rx * math.cos(math.pi * i / steps), cy - ry * math.sin(math.pi * i / steps))
            for i in range(steps + 1)]


def upright_barrel(size, box):
    x0, y0, x1, y1 = box
    w = x1 - x0
    h = y1 - y0
    cx = (x0 + x1) / 2.0
    bulge = w * 0.10
    rx_end = half_width(0.0, w, bulge)
    steps = 60

    left = [(cx - half_width(i / steps, w, bulge), y0 + h * i / steps) for i in range(steps + 1)]
    right = [(cx + half_width(i / steps, w, bulge), y0 + h * i / steps) for i in range(steps + 1)]
    bottom = lower_arc(cx, y1, rx_end, reverse=True)
    top = upper_arc(cx, y0, rx_end)
    outline = left + bottom + right[::-1] + top[::-1]

    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    ImageDraw.Draw(layer).polygon(outline, fill=WOOD + (255,))

    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).polygon(outline, fill=255)

    detail = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    dd = ImageDraw.Draw(detail)

    # Staves: vertical seams, which the mask trims to the curved ends.
    count = 7
    for i in range(1, count):
        fx = x0 + w * i / count
        dd.line([(fx, y0 - h), (fx, y1 + h)], fill=WOOD_DARK + (255,), width=max(2, size // 180))
    dd.rectangle([x0, y0 - h, x0 + w * 0.22, y1 + h], fill=WOOD_LIGHT + (70,))

    # Hoops: bands between two lower arcs at the width the barrel has there.
    band = h * 0.075
    for center_t in (0.20, 0.80):
        rx = half_width(center_t, w, bulge) + 6
        yc = y0 + h * center_t
        upper = lower_arc(cx, yc - band / 2, rx)
        lower = lower_arc(cx, yc + band / 2, rx, reverse=True)
        dd.polygon(upper + lower, fill=HOOP + (255,))
        shine = lower_arc(cx, yc - band / 2 + band * 0.25, rx, reverse=True)
        dd.polygon(upper + shine, fill=HOOP_LIGHT + (255,))

    detail.putalpha(Image.composite(detail.split()[3], Image.new("L", (size, size), 0), mask))
    layer = Image.alpha_composite(layer, detail)

    # The head: the top ellipse, with an inset for the rim.
    ry = rx_end * ASPECT
    hd = ImageDraw.Draw(layer)
    hd.ellipse([cx - rx_end, y0 - ry, cx + rx_end, y0 + ry], fill=WOOD_DARK + (255,))
    inset = w * 0.045
    hd.ellipse([cx - rx_end + inset, y0 - ry + inset * ASPECT, cx + rx_end - inset, y0 + ry - inset * ASPECT],
               fill=WOOD_LIGHT + (255,))
    return layer


def side_barrel(size, box):
    """The upright barrel drawn whole on a padded canvas of its own, turned a
    quarter turn clockwise so its head faces right, and centered in box.
    Returns the layer, the middle of its top edge, and the widest base a
    casque may have there."""
    bw = box[3] - box[1]
    bh = box[2] - box[0]
    side = int(max(bw, bh) * 1.6)
    ux0 = (side - bw) / 2.0
    uy0 = (side - bh) / 2.0
    upright = upright_barrel(side, (ux0, uy0, ux0 + bw, uy0 + bh))

    turned = upright.rotate(-90, resample=Image.BICUBIC, expand=True)
    turned = turned.crop(turned.getbbox())

    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    left = int(round((box[0] + box[2]) / 2.0 - turned.width / 2.0))
    top = int(round((box[1] + box[3]) / 2.0 - turned.height / 2.0))
    layer.alpha_composite(turned, (left, top))

    column = left + turned.width // 2
    alpha = layer.split()[3].load()
    contact_y = next((y for y in range(size) if alpha[column, y] > 128), top)
    return layer, (column, contact_y), bh * 0.40


# ---------- the casque -------------------------------------------------------

def casque():
    """The top half of the silhouette, in cream: the casque, with no feathers."""
    sil = bi.silhouette_alpha()
    crop = sil.crop((0, 0, sil.width, int(sil.height * 0.50)))
    crop = crop.crop(crop.getbbox())
    horn = Image.new("RGBA", crop.size, CREAM + (0,))
    horn.putalpha(crop.split()[3])
    return horn


def bottom_edge_span(piece):
    """The left, right and row of the piece's lowest solid row: where it touches."""
    alpha = piece.split()[3].load()
    w, h = piece.size
    for y in range(h - 1, -1, -1):
        xs = [x for x in range(w) if alpha[x, y] > 160]
        if len(xs) > w * 0.05:
            return xs[0], xs[-1], y
    return 0, w - 1, h - 1


def set_down(canvas, piece, contact, height, max_base):
    """Scales the piece to height, narrower if its base would overhang
    max_base, and sets the middle of its bottom edge on contact."""
    left, right, _ = bottom_edge_span(piece)
    scale = height / piece.height
    base = (right - left + 1) * scale
    if base > max_base:
        scale *= max_base / base
    scaled = piece.resize((max(1, int(piece.width * scale)), max(1, int(piece.height * scale))), Image.LANCZOS)
    left, right, bottom = bottom_edge_span(scaled)
    mid = (left + right) / 2.0
    canvas.alpha_composite(scaled, (int(round(contact[0] - mid)), int(round(contact[1] - bottom))))


# ---------- driver -----------------------------------------------------------

def compose():
    s = MASTER
    img = Image.new("RGBA", (s, s), DARK + (255,))
    layer, contact, max_base = side_barrel(s, (int(s * 0.16), int(s * 0.50), int(s * 0.80), int(s * 0.88)))
    img.alpha_composite(layer)
    set_down(img, casque(), contact, s * 0.40, max_base * 0.95)
    img.putalpha(bi.rounded_mask(s, int(s * bi.RADIUS_FRAC)))
    return img


def main() -> None:
    master = compose()

    png_path = bi.OUT_DIR / "Cassque.png"
    master.save(png_path, "PNG", optimize=True)

    icons = [master.resize((n, n), Image.LANCZOS) for n in reversed(bi.ICO_SIZES)]
    ico_path = bi.OUT_DIR / "Cassque.ico"
    icons[0].save(ico_path, format="ICO", sizes=[(n, n) for n in bi.ICO_SIZES], append_images=icons[1:])

    master.resize((256, 256), Image.LANCZOS).save(ABOUT_PNG, "PNG", optimize=True)

    for path in (png_path, ico_path, ABOUT_PNG):
        print(f"  {path.name:14} {path.stat().st_size:>8} B")


if __name__ == "__main__":
    main()
