#!/usr/bin/env python3
"""
Display text drawn on the DHGR color-cell grid.

WHY THE CELL GRID. A DHGR framebuffer is read two completely different
ways depending on the monitor: a color monitor groups every 4 dots into
one 16-color cell, a monochrome monitor shows all 560 dots individually.
Text drawn on the CELL grid survives both. A cell with all four dots lit
is palette index 15 (white) to a color monitor and four lit dots to a
monochrome one; a cell with none lit is black to both. So text built only
from all-on and all-off cells is legible through either decode -- which
is what lets each of the demo's cassowary images stay readable on the
monitor it was NOT authored for.

WHY A REAL FACE, AFTER A HAND-CUT BITMAP FONT. This drew a 4-cell-wide
bitmap font for a long time, on the argument that anti-aliased TrueType
pushed through the color quantizer fringes on color and dissolves into
the dither on monochrome. The argument holds against anti-aliased text
and not against the outlines: rendered large and then THRESHOLDED to
whole cells, a face lands on the same all-on / all-off grid the bitmap
font did. What changed is the size. The title is now turned on its side,
so its cap height is measured ACROSS the cell band -- eighteen cells --
rather than down seven scanlines, and at that size the hand-cut font read
as a stack of blocks.

WHY THIS FACE. Century Gothic is a geometric sans in the Futura line,
which is the shape language the period's own poster lettering used, and
its round caps stay round through the threshold where a grotesque's
subtler curves go lumpy. Bold, because at eighteen cells a text weight
comes out spindly and a monochrome monitor's bloom eats it.

The font is READ OFF THE HOST rather than committed. Casso is a Windows
project and these images are authored by hand and committed, so the
generator runs on a developer's machine or not at all -- and if the face
is missing it says so and stops rather than quietly substituting another
one, which would change the shipped artwork without anyone deciding to.

THE CELL IS NOT SQUARE. A cell is four dots wide and one scanline tall,
which on the 560x384 the previews are drawn at is 4 units by 2: twice as
wide as a scanline is tall. So the text is set upright at whatever size
the face likes and resampled onto the cell/scanline grid, and the aspect
is dealt with once, here, rather than by hunting for a point size that
happens to come out right after the squash.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


#  Century Gothic Bold. Present on a machine with Microsoft Office, which
#  is where the face ships; see the module docstring for why that is an
#  acceptable dependency and what happens when it is not met.
FONT_NAME = "GOTHICB.TTF"
FONT_DIRS = [Path(r"C:\Windows\Fonts"),
             Path.home() / "AppData/Local/Microsoft/Windows/Fonts"]

CELL_UNITS     = 4     # a cell is four dots wide
SCANLINE_UNITS = 2     # a scanline is two dots tall at the display's aspect

#  Big enough that the downsample to eighteen-odd cells is an area
#  average rather than a point sample, which is what keeps the thin parts
#  of a curve alive through the threshold.
RENDER_PX = 400

THRESHOLD = 128


def font_path():
    """Where the title face lives, or a message saying it does not."""
    for d in FONT_DIRS:
        candidate = d / FONT_NAME
        if candidate.exists():
            return candidate

    raise FileNotFoundError(
        f"{FONT_NAME} (Century Gothic Bold) not found in "
        + ", ".join(str(d) for d in FONT_DIRS)
        + ". It ships with Microsoft Office. Install it, or change "
          "FONT_NAME here -- but a different face changes the shipped "
          "artwork, so decide it rather than let it happen.")


def _render_ink(text):
    """`text` set upright and cropped to its ink, as an L image."""
    font  = ImageFont.truetype(str(font_path()), RENDER_PX)
    probe = Image.new("L", (RENDER_PX * (len(text) + 2), RENDER_PX * 3), 0)
    ImageDraw.Draw(probe).text((RENDER_PX, RENDER_PX), text,
                               fill=255, font=font)

    ink = probe.getbbox()
    if ink is None:
        raise ValueError(f"nothing drawn for {text!r}")
    return probe.crop(ink)


def measure_up(text, cap_cells):
    """How many scanlines `text` spans when turned a quarter turn to the
    left at `cap_cells` of cap height.

    All-caps text has no descenders, so the ink box IS the cap box and
    the two agree."""
    w, h = _render_ink(text).size
    return max(1, round(w / h * cap_cells * CELL_UNITS / float(SCANLINE_UNITS)))


def stamp_up(text, left, cap_cells, rows_tall):
    """The set of (cell, row) positions `text` lights turned a quarter
    turn to the left: it reads bottom to top, with the tops of the letters
    facing the left edge.

    `left` is the first cell column it occupies and `cap_cells` its cap
    height, measured across the band. It is centered down `rows_tall`
    scanlines.
    """
    ink    = _render_ink(text)
    length = measure_up(text, cap_cells)

    if length > rows_tall:
        raise ValueError(f"{text!r} at {cap_cells} cells runs {length} "
                         f"scanlines, past the {rows_tall} available")

    #  Upright, on the target grid: `length` scanlines along the text and
    #  `cap_cells` cells across it. Resampling into that box is what
    #  applies the cell aspect.
    fitted = ink.resize((length, cap_cells), Image.LANCZOS).load()

    top = (rows_tall - length) // 2
    lit = set()

    for across in range(cap_cells):
        for along in range(length):
            if fitted[along, across] >= THRESHOLD:
                #  A quarter turn left: the letter tops (across = 0) face
                #  the left edge, and the start of the text lands at the
                #  bottom.
                lit.add((left + across, top + length - 1 - along))

    return lit
