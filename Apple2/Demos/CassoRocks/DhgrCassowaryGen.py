#!/usr/bin/env python3
"""
Generate the casso-rocks demo's two DHGR cassowary images.

Reads:  Assets/3a Mrs Cassowary closeup 8167.jpg
Writes: dhgr-cassowary-aux.bin        (8 KB, 16-color)
        dhgr-cassowary-main.bin       (8 KB, 16-color)
        dhgr-cassowary-mono-aux.bin   (8 KB, 560x192 1-bit)
        dhgr-cassowary-mono-main.bin  (8 KB, 560x192 1-bit)
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

They are exactly complementary, and nothing on the disk can DETECT
which monitor is attached -- no Apple II can -- so the demo ships both
and asks which one to show. The title still goes through CellCaption's
cell grid, because the cycle wraps and either answer can end up looking
at the other pair.

The same argument, at half the horizontal resolution, produces the HGR
pair; see HgrCassowaryGen. Page geometry for all four is in
DemoImageLayout, so switching between any two moves nothing but the
picture.
"""

import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.stderr.write("PIL/Pillow required: pip install Pillow\n")
    sys.exit(1)

# Reach the sibling pipeline modules.
sys.path.insert(0, str(Path(__file__).resolve().parent))
import DemoImageLayout as Layout


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

CELL_BLACK = 0
CELL_WHITE = 15

# Monochrome tone curve. The 1-bit dither has no midtones of its own --
# every gray is a dot ratio -- so the photo needs more contrast and more
# local detail going in than a continuous-tone display would want.
MONO_GAMMA    = 1.40
MONO_CONTRAST = 1.35
MONO_SHARPEN  = 1.20


def build_palette_image():
    """Create a reference image using the //e palette so PIL can
    quantize against it."""
    pal = []
    for r, g, b in DHGR_PALETTE_RGB:
        pal.extend([r, g, b])
    pal.extend([0] * (768 - len(pal)))
    p_img = Image.new("P", (1, 1))
    p_img.putpalette(pal)
    return p_img


def build_color_cells():
    """The 16-color image as a 140x192 P-mode canvas of palette indices."""
    canvas = Image.new("RGB", (Layout.CELLS, Layout.ROWS), (0, 0, 0))
    photo, at = Layout.fit_photo(
        Layout.load_photo("RGB", Layout.CROP_PORTRAIT), Layout.CELLS)
    canvas.paste(photo, at)

    quantized = canvas.quantize(palette=build_palette_image(),
                                dither=Image.FLOYDSTEINBERG)
    pixels = quantized.load()

    for row in Layout.band_rows():
        for cell in range(Layout.CELLS):
            pixels[cell, row] = CELL_BLACK
    for cell, row in Layout.chrome_cells():
        pixels[cell, row] = CELL_WHITE

    return quantized


def build_mono_dots():
    """The monochrome image as a 560x192 L-mode canvas of 0 / 255 dots."""
    canvas = Image.new("L", (Layout.DOTS, Layout.ROWS), 0)
    photo, at = Layout.fit_photo(
        Layout.load_photo("L", Layout.CROP_PORTRAIT), Layout.DOTS)
    canvas.paste(photo, at)
    canvas = Layout.apply_tone(canvas, MONO_GAMMA, MONO_CONTRAST, MONO_SHARPEN)

    dots   = Layout.dither_1bit(canvas)
    pixels = dots.load()

    for row in Layout.band_rows():
        for x in range(Layout.DOTS):
            pixels[x, row] = 0

    # The title is placed on the CELL grid even here, so it survives
    # the color decode too -- the cycle wraps, so a color monitor can
    # end up showing this image.
    for cell, row in Layout.chrome_cells():
        for dot in range(4):
            pixels[cell * 4 + dot, row] = 255

    return dots


def encode_dhgr(get_dot):
    """Pack a 560x192 dot predicate into 8 KB aux + 8 KB main.

    80 bytes per scanline: aux[0] main[0] aux[1] main[1] ... aux[39]
    main[39]; 7 dots per byte, bit 0 = leftmost dot, bit 7 ignored."""
    aux_buf  = bytearray(8192)
    main_buf = bytearray(8192)

    for row in range(Layout.ROWS):
        base = Layout.hgr_row_offset(row)
        for dot in range(Layout.DOTS):
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
    #  The assets sit beside this script now, so the output folder is
    #  simply this one.
    out_dir = Path(__file__).resolve().parent

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
    color.convert("RGB").resize((Layout.DOTS, Layout.ROWS * 2), Image.NEAREST) \
         .save(out_dir / "dhgr-cassowary-preview.png")
    mono.convert("RGB").resize((Layout.DOTS, Layout.ROWS * 2), Image.NEAREST) \
        .save(out_dir / "dhgr-cassowary-mono-preview.png")
    print("wrote dhgr-cassowary-preview.png, dhgr-cassowary-mono-preview.png")

    return 0


if __name__ == "__main__":
    sys.exit(main())
