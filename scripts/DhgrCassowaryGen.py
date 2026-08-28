#!/usr/bin/env python3
"""
Generate the casso-rocks demo's two DHGR cassowary images.

Reads:  Assets/3a Mrs Cassowary closeup 8167.jpg
Writes: Apple2/Demos/dhgr-cassowary-aux.bin        (8 KB, 16-color)
        Apple2/Demos/dhgr-cassowary-main.bin       (8 KB, 16-color)
        Apple2/Demos/dhgr-cassowary-mono-aux.bin   (8 KB, 560x192 1-bit)
        Apple2/Demos/dhgr-cassowary-mono-main.bin  (8 KB, 560x192 1-bit)
        plus a preview PNG of each.

WHY TWO IMAGES. A DHGR framebuffer is decoded two different ways
depending on the monitor. A color monitor groups every 4 dots into one
16-color cell (140 cells across); a monochrome monitor shows all 560
dots. Those decodes want opposite things from the encoder:

  - The color image dithers to the 16-color palette, so each cell's
    nibble carries HUE. A monochrome monitor sees the raw nibbles, and
    since a mid-gray and a saturated green both land near 50% dot duty,
    the picture reads as noise. No amount of filtering downstream gets
    it back -- the luminance is gone at encode time.

  - The mono image dithers the photo's LUMINANCE to 1 bit across all 560
    dots. That is the highest-resolution mode the machine has, and it
    reads as a photograph on a green, amber, or white monitor. On a
    color monitor the same dots group into arbitrary nibbles and come
    out fringed.

They are exactly complementary, and nothing on the disk can tell which
monitor is attached, so the demo ships both and the user picks. Each is
captioned with the monitor it was authored for, in text drawn on the
cell grid (see CellCaption) so the caption stays legible through EITHER
decode -- the whole point being that the caption is readable precisely
when the image around it is not.

Layout: a title band across the top and a caption band across the
bottom, both cleared to black, with the photo letterboxed into the rows
between. Both images use the same bands and the same photo box, so
switching between them moves nothing but the picture.
"""

import sys
from pathlib import Path

try:
    from PIL import Image, ImageEnhance, ImageFilter
except ImportError:
    sys.stderr.write("PIL/Pillow required: pip install Pillow\n")
    sys.exit(1)

# Reach the sibling pipeline modules.
sys.path.insert(0, str(Path(__file__).resolve().parent))
import CellCaption
import HgrPreprocess


# Apple //e 16-color LoRes/DHGR palette (RGB), index = 4-bit color value.
# Must match CassoEmuCore/Video/AppleLoResMode.cpp::kLoResColors.
DHGR_PALETTE_RGB = [
    (  0,   0,   0),   #  0 Black
    (221,  34, 102),   #  1 Magenta
    (  0,   0, 153),   #  2 Dark Blue
    (221,   0,  68),   #  3 Purple
    (  0,  34,   0),   #  4 Dark Green
    ( 85,  85,  85),   #  5 Grey 1
    (  0,  34, 204),   #  6 Medium Blue
    (102, 170, 255),   #  7 Light Blue
    (136,  85,   0),   #  8 Brown
    (255,  68,   0),   #  9 Orange
    (170, 170, 170),   # 10 Grey 2
    (255, 136, 136),   # 11 Pink
    (  0, 221,   0),   # 12 Light Green
    (255, 255,   0),   # 13 Yellow
    ( 68, 255, 221),   # 14 Aquamarine
    (255, 255, 255),   # 15 White
]

DHGR_COLOR_W = 140     # color cells across
DHGR_DOT_W   = 560     # dots across (4 per cell)
DHGR_ROWS    = 192

CELL_BLACK   = 0
CELL_WHITE   = 15

# Chrome. The title is the same font at 2x, which is why its band is
# roughly twice as tall as the caption's.
TITLE_TEXT     = "CASSO"
TITLE_SCALE    = 2
TITLE_TOP      = 3
TITLE_BAND_H   = TITLE_TOP + CellCaption.GLYPH_H * TITLE_SCALE + 2
CAPTION_TOP    = 2                                    # within the caption band
CAPTION_BAND_H = CAPTION_TOP + CellCaption.GLYPH_H + 3

CAPTION_COLOR  = "(FOR COLOR MONITORS)"
CAPTION_MONO   = "(FOR MONOCHROME MONITORS)"

# The photo box: everything the two bands leave. Fitting the picture
# into it rather than letting the bands sit on top costs some size but
# keeps the casque and the wattles -- the two things that make the bird
# recognizable -- out from under the text.
PHOTO_TOP      = TITLE_BAND_H
PHOTO_H        = DHGR_ROWS - CAPTION_BAND_H - PHOTO_TOP

# Monochrome tone curve. The 1-bit dither has no midtones of its own --
# every gray is a dot ratio -- so the photo needs more contrast and more
# local detail going in than a continuous-tone display would want.
MONO_GAMMA     = 1.40
MONO_CONTRAST  = 1.35
MONO_SHARPEN   = 1.20


def hgr_row_offset(row: int) -> int:
    return 1024 * (row & 7) + 128 * ((row >> 3) & 7) + 40 * (row >> 6)


def band_rows():
    """The scanlines the title and caption bands own."""
    return (list(range(0, TITLE_BAND_H))
          + list(range(DHGR_ROWS - CAPTION_BAND_H, DHGR_ROWS)))


def chrome_cells(caption: str) -> set:
    """Every cell the title and caption light, for either image."""
    title = CellCaption.stamp(TITLE_TEXT, TITLE_TOP, DHGR_COLOR_W,
                              scale=TITLE_SCALE)
    lower = CellCaption.stamp(caption,
                              DHGR_ROWS - CAPTION_BAND_H + CAPTION_TOP,
                              DHGR_COLOR_W)
    return title | lower


def load_photo(mode: str) -> Image.Image:
    repo = Path(__file__).resolve().parent.parent
    src  = repo / "Assets" / "3a Mrs Cassowary closeup 8167.jpg"
    if not src.exists():
        raise FileNotFoundError(f"source image not found: {src}")
    return Image.open(src).convert(mode).crop(HgrPreprocess.DEFAULT_CROP)


def fit_photo(photo: Image.Image, canvas_w: int) -> tuple:
    """Scale the photo to fill PHOTO_H scanlines at the display's aspect,
    and return it with the top-left corner to paste it at.

    `canvas_w` is the horizontal resolution being drawn at -- 140 cells
    for the color image, 560 dots for the monochrome one. Both span the
    same physical screen width as HGR's 280 pixels, so the scale factor
    from display pixels to canvas units is canvas_w / 280."""
    src_w, src_h = photo.size
    new_h = PHOTO_H
    new_w = max(1, round(new_h * (src_w / src_h) * (canvas_w / 280.0)))

    if new_w > canvas_w:
        new_w = canvas_w
        new_h = max(1, round(new_w * (src_h / src_w) * (280.0 / canvas_w)))

    scaled = photo.resize((new_w, new_h), Image.LANCZOS)
    return scaled, ((canvas_w - new_w) // 2, PHOTO_TOP + (PHOTO_H - new_h) // 2)


def build_palette_image() -> Image.Image:
    """Create a reference image using the //e palette so PIL can
    quantize against it."""
    pal = []
    for r, g, b in DHGR_PALETTE_RGB:
        pal.extend([r, g, b])
    pal.extend([0] * (768 - len(pal)))
    p_img = Image.new("P", (1, 1))
    p_img.putpalette(pal)
    return p_img


def build_color_cells() -> Image.Image:
    """The 16-color image as a 140x192 P-mode canvas of palette indices."""
    canvas = Image.new("RGB", (DHGR_COLOR_W, DHGR_ROWS), (0, 0, 0))
    photo, at = fit_photo(load_photo("RGB"), DHGR_COLOR_W)
    canvas.paste(photo, at)

    quantized = canvas.quantize(palette=build_palette_image(),
                                dither=Image.FLOYDSTEINBERG)
    pixels = quantized.load()

    for row in band_rows():
        for cell in range(DHGR_COLOR_W):
            pixels[cell, row] = CELL_BLACK
    for cell, row in chrome_cells(CAPTION_COLOR):
        pixels[cell, row] = CELL_WHITE

    return quantized


def dither_1bit(gray: Image.Image) -> Image.Image:
    """Serpentine Floyd-Steinberg to pure black and white.

    Serpentine (alternating scan direction) rather than left-to-right
    because the dot grid is strongly anisotropic -- 560 dots across but
    only 192 scanlines down -- and a single scan direction pushes its
    error the same way on every line, which shows up as horizontal
    streaking in flat areas like the background foliage."""
    width, height = gray.size
    rows = [list(map(float, gray.crop((0, y, width, y + 1)).get_flattened_data()))
            for y in range(height)]

    out    = Image.new("L", (width, height), 0)
    pixels = out.load()

    for y in range(height):
        forward = (y % 2) == 0
        step    = 1 if forward else -1
        order   = range(width) if forward else range(width - 1, -1, -1)

        for x in order:
            old = rows[y][x]
            new = 255.0 if old >= 128.0 else 0.0
            pixels[x, y] = int(new)

            err = old - new
            for nx, ny, share in ((x + step, y,     7.0 / 16.0),
                                  (x - step, y + 1, 3.0 / 16.0),
                                  (x,        y + 1, 5.0 / 16.0),
                                  (x + step, y + 1, 1.0 / 16.0)):
                if 0 <= nx < width and 0 <= ny < height:
                    rows[ny][nx] += err * share

    return out


def build_mono_dots() -> Image.Image:
    """The monochrome image as a 560x192 L-mode canvas of 0 / 255 dots."""
    canvas = Image.new("L", (DHGR_DOT_W, DHGR_ROWS), 0)
    photo, at = fit_photo(load_photo("L"), DHGR_DOT_W)
    canvas.paste(photo, at)

    canvas = canvas.filter(ImageFilter.UnsharpMask(
        radius=2, percent=int(MONO_SHARPEN * 100), threshold=2))
    canvas = ImageEnhance.Contrast(canvas).enhance(MONO_CONTRAST)
    canvas = canvas.point([min(255, int(255 * ((i / 255.0) ** (1.0 / MONO_GAMMA))))
                           for i in range(256)])

    dots   = dither_1bit(canvas)
    pixels = dots.load()

    for row in band_rows():
        for x in range(DHGR_DOT_W):
            pixels[x, row] = 0

    # The chrome is placed on the CELL grid even here, so that the
    # caption survives the color decode too -- a color monitor showing
    # this image needs to be able to read why it looks wrong.
    for cell, row in chrome_cells(CAPTION_MONO):
        for dot in range(4):
            pixels[cell * 4 + dot, row] = 255

    return dots


def encode_dhgr(get_dot) -> tuple:
    """Pack a 560x192 dot predicate into 8 KB aux + 8 KB main.

    80 bytes per scanline: aux[0] main[0] aux[1] main[1] ... aux[39]
    main[39]; 7 dots per byte, bit 0 = leftmost dot, bit 7 ignored."""
    aux_buf  = bytearray(8192)
    main_buf = bytearray(8192)

    for row in range(DHGR_ROWS):
        base = hgr_row_offset(row)
        for dot in range(DHGR_DOT_W):
            if not get_dot(dot, row):
                continue
            byte_idx = dot // 7
            bit_idx  = dot - byte_idx * 7
            if (byte_idx & 1) == 0:
                aux_buf[base + (byte_idx >> 1)]  |= (1 << bit_idx)
            else:
                main_buf[base + (byte_idx >> 1)] |= (1 << bit_idx)

    return bytes(aux_buf), bytes(main_buf)


def main():
    repo    = Path(__file__).resolve().parent.parent
    out_dir = repo / "Apple2" / "Demos"

    color = build_color_cells()
    cpix  = color.load()
    c_aux, c_main = encode_dhgr(
        lambda dot, row: (cpix[dot // 4, row] >> (dot % 4)) & 1)

    mono  = build_mono_dots()
    mpix  = mono.load()
    m_aux, m_main = encode_dhgr(lambda dot, row: mpix[dot, row] != 0)

    for name, data in (("dhgr-cassowary-aux.bin",       c_aux),
                       ("dhgr-cassowary-main.bin",      c_main),
                       ("dhgr-cassowary-mono-aux.bin",  m_aux),
                       ("dhgr-cassowary-mono-main.bin", m_main)):
        (out_dir / name).write_bytes(data)
        print(f"wrote {name} ({len(data)} bytes)")

    # Previews at the on-screen aspect (560x384), each showing what its
    # own monitor would show.
    color.convert("RGB").resize((DHGR_DOT_W, DHGR_ROWS * 2), Image.NEAREST) \
         .save(out_dir / "dhgr-cassowary-preview.png")
    mono.convert("RGB").resize((DHGR_DOT_W, DHGR_ROWS * 2), Image.NEAREST) \
        .save(out_dir / "dhgr-cassowary-mono-preview.png")
    print("wrote dhgr-cassowary-preview.png, dhgr-cassowary-mono-preview.png")

    return 0


if __name__ == "__main__":
    sys.exit(main())
