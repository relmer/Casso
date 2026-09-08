#!/usr/bin/env python3
"""
Shared page layout for the casso-rocks demo's cassowary images.

The demo ships the same photo four ways -- DHGR in color and in
monochrome, HGR in color and in monochrome -- and each pair has to be
interchangeable at a keystroke, so the title band, the caption band and
the photo box are defined once, here, rather than per generator.

COORDINATES ARE IN "CANVAS UNITS ACROSS THE SCREEN WIDTH". The four
images are authored at three different horizontal resolutions -- 140
DHGR color cells, 280 HGR pixels, 560 dots -- that all span the same
physical screen width, so every generator passes its own `canvas_w` and
the geometry scales. Vertical is always 192 scanlines.

The title is the exception: it is always placed on the 140-cell grid,
whatever the image's own resolution, because that is what makes it
legible through both the color and the monochrome decode. See
CellCaption for why.

The title used to run across the top, and the photo started under it.
The photo is a portrait, so height is the scarce axis and width was
going spare: the title band cost a tenth of the scanlines and the
columns beside the picture went unused. Turned a quarter turn left it
climbs the left edge instead, out of a margin that was empty anyway,
and the photo gets all 192 scanlines.

The images used to carry a caption naming the monitor they were drawn
for, on the same grid and for the same reason. The demo asks which
monitor it is talking to now, and shows the matching pair, so the
caption was telling the user something they had just said -- and it was
spending thirteen scanlines to do it. The photo box got them.
"""

from pathlib import Path

from PIL import Image, ImageEnhance, ImageFilter

import CellCaption


ROWS      = 192
CELLS     = 140        # DHGR color cells across the screen
HGR_PIX   = 280        # HGR pixels across the screen
DOTS      = 560        # half-dots across the screen

def _repo_root():
    """The folder holding Casso.sln, found by walking up.

    Counting directories broke the moment this file moved out of
    scripts/ into the demo's own folder, and would break again on the
    next move.
    """
    here = Path(__file__).resolve()
    for d in here.parents:
        if (d / "Casso.sln").exists():
            return d
    raise RuntimeError("no Casso.sln above " + str(here))


SRC = _repo_root() / "Assets" / "3a Mrs Cassowary closeup 8167.jpg"

# The portrait crop the DHGR pair uses: the full casque, head, neck and
# wattles. HGR gets a tighter one -- see HgrCassowaryGen for why.
CROP_PORTRAIT = (60, 40, 860, 1100)

# The title, drawn in the CellCaption font at 3x and turned a quarter
# turn to the left, so it climbs the left edge reading bottom to top.
# Rotated, the glyph height is what costs cells and the text length is
# what costs scanlines, so 3x fills the 192-line edge without crowding
# the photo: 21 cells wide, 72 scanlines tall.
TITLE_TEXT     = "CASSO"
TITLE_SCALE    = 3
TITLE_LEFT     = 3

# The band the title owns, ROUNDED UP TO A WHOLE NUMBER OF HGR BYTES.
# Seven pixels to a byte and one half-dot shift shared across all of
# them means a byte straddling the boundary would let the photo's shift
# choice drag the title's last column half a dot off the cell grid, and
# the title's whole legibility rests on staying on that grid. One cell
# is two HGR pixels, so the band has to be a multiple of seven cells.
_BAND_MIN      = TITLE_LEFT + CellCaption.GLYPH_H * TITLE_SCALE + 2
TITLE_BAND_W   = ((_BAND_MIN + 6) // 7) * 7

# The photo box: everything to the right of the band, full height.
PHOTO_TOP      = 0
PHOTO_H        = ROWS


def band_units(canvas_w):
    """The columns of a `canvas_w`-wide canvas the title band owns."""
    return list(range(0, band_width(canvas_w)))


def band_width(canvas_w):
    """How wide the title band is in `canvas_w` units."""
    return round(TITLE_BAND_W * canvas_w / float(CELLS))


def chrome_cells():
    """Every 140-grid cell the title lights."""
    return CellCaption.stamp_up(TITLE_TEXT, TITLE_LEFT, ROWS, scale=TITLE_SCALE)


def load_photo(mode, crop):
    if not SRC.exists():
        raise FileNotFoundError(f"source image not found: {SRC}")
    return Image.open(SRC).convert(mode).crop(crop)


def fit_photo(photo, canvas_w):
    """Scale the photo to fill PHOTO_H scanlines at the display's aspect,
    and return it with the top-left corner to paste it at.

    `canvas_w` is the horizontal resolution being drawn at. All of them
    span the same physical width as HGR's 280 pixels, so the scale from
    display pixels to canvas units is canvas_w / 280."""
    band  = band_width(canvas_w)
    avail = canvas_w - band

    src_w, src_h = photo.size
    new_h = PHOTO_H
    new_w = max(1, round(new_h * (src_w / src_h) * (canvas_w / float(HGR_PIX))))

    if new_w > avail:
        new_w = avail
        new_h = max(1, round(new_w * (src_h / src_w) * (float(HGR_PIX) / canvas_w)))

    scaled = photo.resize((new_w, new_h), Image.LANCZOS)
    return scaled, (band + (avail - new_w) // 2,
                    PHOTO_TOP + (PHOTO_H - new_h) // 2)


def apply_tone(canvas, gamma, contrast, sharpen):
    """The curve a one-bit dither wants: more local detail and more
    contrast than a continuous-tone display would need, because every
    gray it renders is a dot ratio rather than a level."""
    if sharpen:
        canvas = canvas.filter(ImageFilter.UnsharpMask(
            radius=2, percent=int(sharpen * 100), threshold=2))
    if contrast != 1.0:
        canvas = ImageEnhance.Contrast(canvas).enhance(contrast)
    if gamma != 1.0:
        canvas = canvas.point([min(255, int(255 * ((i / 255.0) ** (1.0 / gamma))))
                               for i in range(256)])
    return canvas


def dither_1bit(gray, skip_cols=()):
    """Serpentine Floyd-Steinberg to pure black and white.

    Serpentine (alternating scan direction) rather than left-to-right
    because a single scan direction pushes its error the same way on
    every line, which shows up as horizontal streaking in flat areas
    like the background foliage. Columns in `skip_cols` are left alone
    and take no error, so the title band stays clean.
    """
    width, height = gray.size
    skip = set(skip_cols)
    rows = [list(map(float, gray.crop((0, y, width, y + 1)).get_flattened_data()))
            for y in range(height)]

    out    = Image.new("L", (width, height), 0)
    pixels = out.load()

    for y in range(height):
        forward = (y % 2) == 0
        step    = 1 if forward else -1
        order   = range(width) if forward else range(width - 1, -1, -1)

        for x in order:
            if x in skip:
                continue

            old = rows[y][x]
            new = 255.0 if old >= 128.0 else 0.0
            pixels[x, y] = int(new)

            err = old - new
            for nx, ny, share in ((x + step, y,     7.0 / 16.0),
                                  (x - step, y + 1, 3.0 / 16.0),
                                  (x,        y + 1, 5.0 / 16.0),
                                  (x + step, y + 1, 1.0 / 16.0)):
                if 0 <= nx < width and ny < height and nx not in skip:
                    rows[ny][nx] += err * share

    return out


def hgr_row_offset(row):
    return 1024 * (row & 7) + 128 * ((row >> 3) & 7) + 40 * (row >> 6)
