"""Build final Casso icon assets (PNG + ICO) for all four variants.

Each variant produces:
  Out/<name>.png  - 1024x1024 master PNG
  Out/<name>.ico  - Multi-resolution ICO (16, 24, 32, 48, 64, 128, 256)
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageOps

HERE = Path(__file__).parent
SRC_PHOTO = HERE / "Source" / "cassowary-photo-square.png"
SRC_SIL = HERE / "Source" / "cassowary-silhouette.png"
OUT_DIR = HERE.parent.parent / "Resources" / "Icons"
OUT_DIR.mkdir(parents=True, exist_ok=True)

ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
MASTER = 1024
RADIUS_FRAC = 0.18

APPLE_BANDS = [
    (97,  189, 79),
    (253, 184, 39),
    (245, 130, 32),
    (224, 58,  62),
    (150, 61,  151),
    (0,   159, 223),
]


# ---------- helpers ----------------------------------------------------------

def rounded_mask(size: int, radius: int) -> Image.Image:
    m = Image.new("L", (size, size), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, size - 1, size - 1],
                                         radius=radius, fill=255)
    return m


def silhouette_alpha() -> Image.Image:
    img = Image.open(SRC_SIL).convert("RGBA")
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, _ = px[x, y]
            lum = (r + g + b) / 3
            if lum > 200:
                px[x, y] = (0, 0, 0, 0)
            else:
                px[x, y] = (0, 0, 0, int(255 * (1 - lum / 200)))
    return img.crop(img.getbbox())


def fit_into(size: int, src: Image.Image, height_frac: float,
             y_bias_frac: float = 0.0) -> tuple[Image.Image, int, int]:
    target_h = int(size * height_frac)
    scale = target_h / src.height
    target_w = int(src.width * scale)
    scaled = src.resize((target_w, target_h), Image.LANCZOS)
    x = (size - target_w) // 2
    y = (size - target_h) // 2 + int(size * y_bias_frac)
    return scaled, x, y


def fit_with_padding(size: int, src: Image.Image,
                     content_frac: float = 0.94) -> tuple[Image.Image, int, int]:
    target_long = int(size * content_frac)
    scale = target_long / max(src.width, src.height)
    w = int(src.width * scale)
    h = int(src.height * scale)
    scaled = src.resize((w, h), Image.LANCZOS)
    x = (size - w) // 2
    y = (size - h) // 2
    return scaled, x, y


def chromakey_green(img: Image.Image) -> Image.Image:
    img = img.convert("RGBA")
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if g > r + 12 and g > b + 12 and g > 60:
                px[x, y] = (r, g, b, 0)
    return img


# ---------- variants ---------------------------------------------------------

def variant_silhouette_rainbow(size: int) -> Image.Image:
    radius = int(size * RADIUS_FRAC)
    bg = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(bg)
    band_h = size / len(APPLE_BANDS)
    for i, c in enumerate(APPLE_BANDS):
        y0 = int(i * band_h)
        y1 = int((i + 1) * band_h) if i < len(APPLE_BANDS) - 1 else size
        draw.rectangle([0, y0, size, y1], fill=c + (255,))
    bg.putalpha(rounded_mask(size, radius))

    sil = silhouette_alpha()
    sil_s, x, y = fit_into(size, sil, 0.86, 0.03)

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shadow.paste(sil_s, (x + int(size * 0.006), y + int(size * 0.008)), sil_s)
    shadow = shadow.filter(ImageFilter.GaussianBlur(size * 0.008))
    shadow.putalpha(shadow.split()[3].point(lambda a: int(a * 0.35)))

    out = Image.alpha_composite(bg, shadow)
    out.paste(sil_s, (x, y), sil_s)
    out.putalpha(rounded_mask(size, radius))
    return out


def variant_flat_color_head(size: int) -> Image.Image:
    radius = int(size * RADIUS_FRAC)
    photo = Image.open(SRC_PHOTO).convert("RGBA")

    rgb = photo.convert("RGB")
    flat = ImageOps.posterize(rgb, 3)
    flat = flat.quantize(colors=8, method=Image.Quantize.MEDIANCUT).convert("RGBA")
    flat = chromakey_green(flat)

    flat_s, x, y = fit_with_padding(size, flat, content_frac=1.0)

    tile = Image.new("RGBA", (size, size), (248, 244, 232, 255))
    tile.putalpha(rounded_mask(size, radius))
    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    layer.paste(flat_s, (x, y), flat_s)
    out = Image.alpha_composite(tile, layer)
    out.putalpha(rounded_mask(size, radius))
    return out


def variant_photoreal(size: int) -> Image.Image:
    radius = int(size * RADIUS_FRAC)
    photo = Image.open(SRC_PHOTO).convert("RGBA")
    pw, ph = photo.size
    scale = size / min(pw, ph)
    sw, sh = int(round(pw * scale)), int(round(ph * scale))
    scaled = photo.resize((sw, sh), Image.LANCZOS)
    left = (sw - size) // 2
    top = (sh - size) // 2
    tile = scaled.crop((left, top, left + size, top + size)).convert("RGBA")
    tile.putalpha(rounded_mask(size, radius))
    return tile


def variant_silhouette_accent(size: int) -> Image.Image:
    radius = int(size * RADIUS_FRAC)
    bg = Image.new("RGBA", (size, size), (18, 16, 22, 255))

    overlay = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    step = max(2, int(size / 170))
    for y in range(0, size, step):
        od.line([(0, y), (size, y)], fill=(0, 0, 0, 55))
    bg = Image.alpha_composite(bg, overlay)

    stripe_w = int(size * 0.06)
    stripe_x = int(size * 0.08)
    band_h = size / len(APPLE_BANDS)
    stripe = ImageDraw.Draw(bg)
    for i, c in enumerate(APPLE_BANDS):
        y0 = int(i * band_h)
        y1 = int((i + 1) * band_h) if i < len(APPLE_BANDS) - 1 else size
        stripe.rectangle([stripe_x, y0, stripe_x + stripe_w, y1], fill=c + (255,))

    sil = silhouette_alpha()
    sil_recolored = Image.new("RGBA", sil.size, (245, 235, 210, 0))
    sil_recolored.putalpha(sil.split()[3])
    sil_s, x, y = fit_into(size, sil_recolored, 0.84, 0.04)
    x = max(x, stripe_x + stripe_w + int(size * 0.04))
    bg.paste(sil_s, (x, y), sil_s)

    bg.putalpha(rounded_mask(size, radius))
    return bg


# ---------- driver -----------------------------------------------------------

VARIANTS = {
    "Casso-1-silhouette-rainbow": variant_silhouette_rainbow,
    "Casso-2-flat-color-head":    variant_flat_color_head,
    "Casso-3-photoreal":          variant_photoreal,
    "Casso-4-silhouette-accent":  variant_silhouette_accent,
}


def build(name: str, fn) -> None:
    master = fn(MASTER)
    png_path = OUT_DIR / f"{name}.png"
    master.save(png_path, "PNG", optimize=True)

    icons = [fn(s) for s in ICO_SIZES]
    # Pillow's ICO writer requires the base image to be at least as large
    # as the largest requested size, and uses append_images for the
    # additional sizes. Pass the largest first.
    icons.reverse()
    ico_path = OUT_DIR / f"{name}.ico"
    icons[0].save(ico_path, format="ICO",
                  sizes=[(s, s) for s in ICO_SIZES],
                  append_images=icons[1:])

    print(f"  {png_path.name:38} {png_path.stat().st_size:>8} B")
    print(f"  {ico_path.name:38} {ico_path.stat().st_size:>8} B")


def main() -> None:
    for name, fn in VARIANTS.items():
        print(f"Building {name} ...")
        build(name, fn)


if __name__ == "__main__":
    main()
