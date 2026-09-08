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

The title and the picture are placed AS ONE GROUP, centered together,
with the title hard against the picture's left edge. Centering them
separately pools the leftover margin between them, which reads as a
title that has drifted off on its own. The group's width depends on the
crop, so the HGR pair and the DHGR pair land in different columns; the
two images of a PAIR always agree, which is what the keystroke switches
between.

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

# The title, set in the CellCaption face and turned a quarter turn to
# the left so it climbs the edge reading bottom to top. Turned, the CAP
# HEIGHT is what costs cells and the length of the word is what costs
# scanlines: eighteen cells of cap height sets CASSO 158 scanlines long,
# which fills the 192-line edge without running out of it.
TITLE_TEXT = "CASSO"
TITLE_CAP  = 18
TITLE_GAP  = 3     # cells between the title and the picture

# The photo box: full height, and as wide as the crop makes it.
PHOTO_TOP = 0
PHOTO_H   = ROWS

# The picture's left edge is pinned to a multiple of SEVEN CELLS, which
# is a whole number of HGR bytes. Seven pixels to a byte and one half-dot
# shift shared across all of them means a byte holding both title and
# photo would let the photo's shift choice drag the title's last column
# half a dot off the cell grid -- and the title's whole legibility rests
# on staying on that grid. One cell is two HGR pixels, so seven cells is
# fourteen pixels is two bytes.
_SNAP_CELLS = 7


def load_photo(mode, crop):
    if not SRC.exists():
        raise FileNotFoundError(f"source image not found: {SRC}")
    return Image.open(SRC).convert(mode).crop(crop)


def photo_left_cells(photo):
    """Which 140-grid cell the picture starts at.

    Computed from the crop rather than from a rounded width, so both
    images of a pair land in the same column whatever resolution each is
    drawn at."""
    src_w, src_h = photo.size
    wide  = PHOTO_H * (src_w / src_h) * (CELLS / float(HGR_PIX))
    group = TITLE_CAP + TITLE_GAP + wide
    want  = (CELLS - group) / 2.0 + TITLE_CAP + TITLE_GAP

    snapped = int(want / _SNAP_CELLS + 0.5) * _SNAP_CELLS
    floor   = -(-(TITLE_CAP + TITLE_GAP) // _SNAP_CELLS) * _SNAP_CELLS
    return max(floor, snapped)


def lay_out(photo, canvas_w):
    """Place the picture and the title on a `canvas_w`-wide page.

    Returns the scaled picture, the corner to paste it at, the 140-grid
    cells the title lights, and the columns left of the picture -- which
    carry the title and nothing else, so they are cleared and kept out of
    the dither.

    `canvas_w` is the horizontal resolution being drawn at. All of them
    span the same physical width as HGR's 280 pixels, so the scale from
    display pixels to canvas units is canvas_w / 280."""
    units = canvas_w / float(CELLS)
    left  = round(photo_left_cells(photo) * units)

    src_w, src_h = photo.size
    new_h = PHOTO_H
    new_w = max(1, round(new_h * (src_w / src_h) * (canvas_w / float(HGR_PIX))))

    if new_w > canvas_w - left:
        new_w = canvas_w - left
        new_h = max(1, round(new_w * (src_h / src_w) * (float(HGR_PIX) / canvas_w)))

    title = CellCaption.stamp_up(
        TITLE_TEXT, photo_left_cells(photo) - TITLE_GAP - TITLE_CAP,
        TITLE_CAP, ROWS)

    scaled = photo.resize((new_w, new_h), Image.LANCZOS)
    return (scaled,
            (left, PHOTO_TOP + (PHOTO_H - new_h) // 2),
            title,
            list(range(0, left)))


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
