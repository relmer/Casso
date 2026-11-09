"""Build Casso Explorer's icon: File Explorer's folder, with Casso's cassowary.

Produces:
  Resources/Icons/CassoExplorer.png       - 1024x1024 master PNG
  Resources/Icons/CassoExplorer.ico       - Multi-resolution ICO (16 .. 256)
  CassoExplorer/Assets/CassoExplorer.png  - 256x256, the picture in the About box
  CassoExplorer/Assets/Cassowary.png      - 256x256, the photograph, heading the About box

The folder follows Windows' own File Explorer icon in shape and color, so the
two read as a pair; the Apple bands down its left edge and the cassowary are
Casso's, from the same silhouette its icons use. The bands and the folder are
close in value, so a tight shadow to the right of the bands separates them;
below about 32 pixels that shadow falls under one pixel and drops out, which
is what Windows' own art does at those sizes.
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

import build_icons as bi

HERE = Path(__file__).parent
ABOUT_PNG = HERE.parent.parent / "CassoExplorer" / "Assets" / "CassoExplorer.png"
ABOUT_PHOTO_PNG = ABOUT_PNG.with_name("Cassowary.png")

MASTER = 1024
SS = 4                                  # supersample, for clean curves

AMBER_TOP = (255, 216, 102)
AMBER_BOTTOM = (245, 185, 33)
TAB = (224, 159, 0)
BLACK = (22, 20, 26)

#  Where each piece sits, as a fraction of the icon.
FRONT = (0.012, 0.175, 0.988, 0.949)
FRONT_RADIUS = 0.075
STRIPE = (0.012, 0.175, 0.097, 0.949)
SHADOW_REACH = 0.012                    # how far the shadow falls past the bands
SHADOW_BLUR = 0.006
SHADOW_ALPHA = 210
BIRD_HEIGHT = 0.50
BIRD_CENTER = 0.58
BIRD_BOTTOM = 0.85


# ---------- pieces -----------------------------------------------------------

def front_mask(s):
    """The folder's front, which everything painted on it is clipped to."""
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle((FRONT[0] * s, FRONT[1] * s, FRONT[2] * s, FRONT[3] * s),
                                           radius=s * FRONT_RADIUS, fill=255)
    return mask


def clip_to_front(s, layer):
    layer.putalpha(Image.composite(layer.split()[3], Image.new("L", (s, s), 0), front_mask(s)))
    return layer


def vertical_gradient(s, box, top, bottom, radius):
    w = int(box[2] - box[0])
    h = int(box[3] - box[1])
    strip = Image.new("RGBA", (1, h))

    for y in range(h):
        t = y / max(1, h - 1)
        strip.putpixel((0, y), (int(top[0] + (bottom[0] - top[0]) * t),
                                int(top[1] + (bottom[1] - top[1]) * t),
                                int(top[2] + (bottom[2] - top[2]) * t), 255))

    fill = strip.resize((w, h), Image.BILINEAR)
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w - 1, h - 1), radius=radius, fill=255)

    out = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    out.paste(fill, (int(box[0]), int(box[1])), mask)
    return out


def folder(s):
    """The tab behind, the front over it."""
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    tab = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(tab)

    draw.rounded_rectangle((s * 0.012, s * 0.078, s * 0.40, s * 0.36), radius=s * 0.045, fill=TAB + (255,))
    draw.polygon([(s * 0.30, s * 0.078), (s * 0.45, s * 0.30), (s * 0.30, s * 0.30)], fill=TAB + (255,))

    img.alpha_composite(tab)
    img.alpha_composite(vertical_gradient(s, (FRONT[0] * s, FRONT[1] * s, FRONT[2] * s, FRONT[3] * s),
                                          AMBER_TOP, AMBER_BOTTOM, s * FRONT_RADIUS))
    return img


def stripe_shadow(s):
    layer = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(layer).rectangle((STRIPE[0] * s, STRIPE[1] * s, (STRIPE[2] + SHADOW_REACH) * s, STRIPE[3] * s),
                                    fill=(0, 0, 0, SHADOW_ALPHA))
    return clip_to_front(s, layer.filter(ImageFilter.GaussianBlur(SHADOW_BLUR * s)))


def apple_bands(s):
    x0, y0, x1, y1 = [int(v * s) for v in STRIPE]
    layer = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    band_h = (y1 - y0) / len(bi.APPLE_BANDS)

    for i, color in enumerate(bi.APPLE_BANDS):
        a = int(y0 + i * band_h)
        b = int(y0 + (i + 1) * band_h) if i < len(bi.APPLE_BANDS) - 1 else y1
        draw.rectangle([x0, a, x1, b], fill=color + (255,))

    return clip_to_front(s, layer)


def bird(s):
    sil = bi.silhouette_alpha()
    h = int(s * BIRD_HEIGHT)
    w = max(1, int(sil.width * h / sil.height))
    sil = sil.resize((w, h), Image.LANCZOS)

    layer = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    layer.paste(Image.new("RGBA", (w, h), BLACK + (255,)),
                (int(BIRD_CENTER * s - w / 2), int(BIRD_BOTTOM * s - h)), sil)
    return layer


# ---------- driver -----------------------------------------------------------

def compose(size):
    s = size * SS
    img = folder(s)
    img.alpha_composite(stripe_shadow(s))
    img.alpha_composite(apple_bands(s))
    img.alpha_composite(bird(s))
    return img.resize((size, size), Image.LANCZOS)


def main() -> None:
    master = compose(MASTER)

    png_path = bi.OUT_DIR / "CassoExplorer.png"
    master.save(png_path, "PNG", optimize=True)

    #  Each size is drawn at that size rather than shrunk from the master, so
    #  the shadow and the bands fall where the pixels are.
    icons = [compose(n) for n in reversed(bi.ICO_SIZES)]
    ico_path = bi.OUT_DIR / "CassoExplorer.ico"
    icons[0].save(ico_path, format="ICO", sizes=[(n, n) for n in bi.ICO_SIZES], append_images=icons[1:])

    compose(256).save(ABOUT_PNG, "PNG", optimize=True)
    bi.variant_photoreal(512).resize((256, 256), Image.LANCZOS).save(ABOUT_PHOTO_PNG, "PNG", optimize=True)

    for path in (png_path, ico_path, ABOUT_PNG, ABOUT_PHOTO_PNG):
        print(f"  {path.name:24} {path.stat().st_size:>8} B")


if __name__ == "__main__":
    main()
