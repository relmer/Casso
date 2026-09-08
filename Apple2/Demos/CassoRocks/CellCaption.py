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

WHY CONSTRUCTED LETTERS, AND NOT A BITMAP FONT OR A REAL FACE. This drew
a hand-cut 4-cell-wide bitmap font for a long time, on the argument that
anti-aliased TrueType pushed through the color quantizer fringes on color
and dissolves into the dither on monochrome. The argument holds against
anti-aliased text and not against outlines: rendered large and then
THRESHOLDED to whole cells, any curve lands on the same all-on / all-off
grid a bitmap font does. What changed is the size. The title is now
turned on its side, so its cap height is measured ACROSS the cell band --
twenty cells -- rather than down seven scanlines. At that size the
hand-cut font read as a stack of blocks.

There is no face in the tree to render instead: the theme fonts are
one-byte placeholders. Rather than depend on whatever is installed on the
machine that runs this, or commit a font binary for five letters, the
letters are CONSTRUCTED from arcs and strokes at 1000 units of cap height
and downsampled. They are geometric-sans caps, which is what a five-letter
title on a machine like this wants anyway, and they are the same on every
machine.

THE CELL IS NOT SQUARE. A cell is four dots wide and one scanline tall,
which on the 560x384 the previews are drawn at is 4 units by 2: twice as
wide as a scanline is tall. So the letters are constructed upright on a
square grid and resampled onto the cell/scanline grid, and the aspect is
dealt with once, here, rather than by hunting for proportions that happen
to come out right after the squash.
"""

import math

from PIL import Image, ImageDraw


CELL_UNITS     = 4     # a cell is four dots wide
SCANLINE_UNITS = 2     # a scanline is two dots tall at the display's aspect

#  Construction grid. Big enough that the downsample to twenty-odd cells
#  is an area average rather than a point sample, which is what keeps the
#  curves alive through the threshold.
CAP     = 1000
WEIGHT  = 150          # stroke, in cap units
TRACK   = 70           # space between letters
BEARING = 25           # space inside each letter's own box

THRESHOLD = 128

#  Advance widths, before bearings. Round letters are drawn a touch wider
#  than the flat ones so they read the same size, the usual overshoot.
#  Turned on its side the whole word has to fit 192 scanlines, so the
#  setting is tighter than a geometric sans would normally be.
WIDTHS = {'A': 800, 'C': 780, 'O': 800, 'S': 700}


def _draw_o(draw, x, w):
    draw.ellipse([x, 0, x + w, CAP], outline=255, width=WEIGHT)


def _draw_c(draw, x, w):
    #  PIL measures from 3 o'clock and sweeps clockwise, so 40 -> 320
    #  runs down the right, around the bottom, up the left and over the
    #  top, leaving the aperture on the right where a C wants it.
    draw.arc([x, 0, x + w, CAP], 40, 320, fill=255, width=WEIGHT)


#  The S's CENTERLINE, in its own box: x and y both 0..1, y downward,
#  starting at the top-right terminal. Two bowls and a spine drawn as
#  arcs read as a pair of C's kissing, because nothing actually crosses
#  the waist; stroking a path does what the shape needs, and it is the
#  only letter here whose skeleton is not a circle or a triangle.
S_PATH = [(0.97, 0.20), (0.87, 0.05), (0.55, 0.00), (0.23, 0.06),
          (0.05, 0.21), (0.09, 0.38), (0.33, 0.47), (0.67, 0.54),
          (0.91, 0.63), (0.95, 0.79), (0.77, 0.94), (0.45, 1.00),
          (0.13, 0.94), (0.03, 0.79)]


def _spline(points, steps=12):
    """A Catmull-Rom spline through `points`, sampled for stroking."""
    pad = [points[0]] + list(points) + [points[-1]]
    out = []

    for i in range(len(pad) - 3):
        p0, p1, p2, p3 = pad[i:i + 4]
        for s in range(steps):
            t  = s / float(steps)
            t2 = t * t
            t3 = t2 * t
            out.append(tuple(
                0.5 * ((2 * a1) +
                       (-a0 + a2) * t +
                       (2 * a0 - 5 * a1 + 4 * a2 - a3) * t2 +
                       (-a0 + 3 * a1 - 3 * a2 + a3) * t3)
                for a0, a1, a2, a3 in zip(p0, p1, p2, p3)))

    out.append(points[-1])
    return out


def _draw_s(draw, x, w):
    #  The centerline sits half a stroke inside the letter box on every
    #  side, so the ink lands on the box rather than half a stroke past
    #  it.
    half = WEIGHT / 2.0
    path = [(x + half + px * (w - WEIGHT), half + py * (CAP - WEIGHT))
            for px, py in S_PATH]
    points = _spline(path)

    #  A dot at every sample as well as the polyline: PIL draws each
    #  segment as its own quad, and consecutive quads leave hairline
    #  seams inside the bend that the threshold would turn into holes.
    draw.line(points, fill=255, width=WEIGHT)
    for px, py in points:
        draw.ellipse([px - half, py - half, px + half, py + half], fill=255)


def _draw_a(draw, x, w):
    #  Two legs and a bar, with the apex ROUNDED OVER: a triangle brought
    #  to a true point puts a spike on top of a word whose other letters
    #  all end bluntly, and at eighteen cells the point is the first
    #  thing the threshold eats anyway.
    #
    #  The legs lean, so the width measured HORIZONTALLY that leaves a
    #  stroke of WEIGHT across them is WEIGHT / cos(lean). Drawing them
    #  as quads rather than thick lines is what puts the feet flat on the
    #  baseline instead of cut square to the lean.
    apex  = x + w / 2.0
    lean  = (w / 2.0) / float(CAP)
    inset = WEIGHT / 2.0 * (1.0 + lean * lean) ** 0.5
    top   = inset                       # the apex cap's center

    draw.polygon([(apex - inset, top), (apex + inset, top),
                  (x + 2 * inset, CAP), (x, CAP)], fill=255)
    draw.polygon([(apex - inset, top), (apex + inset, top),
                  (x + w, CAP), (x + w - 2 * inset, CAP)], fill=255)
    draw.ellipse([apex - inset, top - inset, apex + inset, top + inset],
                 fill=255)

    #  The bar reaches the legs' outer edges at its own height and no
    #  further, so it does not hang off the sides.
    bar  = CAP * 0.66
    mid  = (bar + WEIGHT * 0.45 - top) / (CAP - top)
    edge = (w / 2.0 - inset) * mid + inset
    draw.rectangle([apex - edge, bar, apex + edge, bar + WEIGHT * 0.9],
                   fill=255)


_LETTERS = {'A': _draw_a, 'C': _draw_c, 'O': _draw_o, 'S': _draw_s}


def _render_ink(text):
    """`text` constructed upright and cropped to its ink, as an L image."""
    boxes = [WIDTHS[ch] + 2 * BEARING for ch in text]
    total = sum(boxes) + TRACK * (len(text) - 1)

    img  = Image.new("L", (total, CAP + 4), 0)
    draw = ImageDraw.Draw(img)

    x = 0
    for ch, box in zip(text, boxes):
        if ch not in _LETTERS:
            raise ValueError(f"no construction for {ch!r}")
        _LETTERS[ch](draw, x + BEARING, box - 2 * BEARING)
        x += box + TRACK

    ink = img.getbbox()
    if ink is None:
        raise ValueError(f"nothing drawn for {text!r}")
    return img.crop(ink)


def measure_up(text, cap_cells):
    """How many scanlines `text` spans when turned a quarter turn to the
    left at `cap_cells` of cap height."""
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

