#!/usr/bin/env python3
"""
Bitmap text drawn on the DHGR color-cell grid.

WHY A CUSTOM FONT INSTEAD OF A TRUETYPE ONE. A DHGR framebuffer is read
two completely different ways depending on the monitor: a color monitor
groups every 4 dots into one 16-color cell, a monochrome monitor shows
all 560 dots individually. Anti-aliased TrueType text pushed through the
color quantizer survives neither cleanly -- it fringes on color and
dissolves into the dither on monochrome.

Text drawn on the CELL grid survives both. A cell with all four dots lit
is palette index 15 (white) to a color monitor and four lit dots to a
monochrome one; a cell with none lit is black to both. So a caption built
only from all-on and all-off cells is legible through either decode --
which is what lets the demo's two cassowary images each say which monitor
they were authored for, and be readable on the monitor they were NOT
authored for.

The cost is horizontal resolution: 140 cells across, versus 280 pixels
for HGR text or 560 dots for monochrome DHGR. Hence a 4-cell-wide
proportional font rather than the Apple's own 5x7 -- at 5 cells plus a
gap the longer of the two captions does not fit on a line.
"""

# Glyphs are '#'/'.' rows, GLYPH_H tall, each a cell wide. Widths vary
# per glyph (I and T are narrow, M and W wide) because the budget is
# tight: "(FOR MONOCHROME MONITORS)" is 117 of the 140 available cells
# proportionally, and would not fit at a fixed 4-cell advance.
GLYPHS = {
    ' ': ["..",   "..",   "..",   "..",   "..",   "..",   ".."  ],
    '(': [".#",   "#.",   "#.",   "#.",   "#.",   "#.",   ".#"  ],
    ')': ["#.",   ".#",   ".#",   ".#",   ".#",   ".#",   "#."  ],
    '-': ["....", "....", "....", "####", "....", "....", "...."],
    '.': [".",    ".",    ".",    ".",    ".",    ".",    "#"   ],
    'A': [".##.", "#..#", "#..#", "####", "#..#", "#..#", "#..#"],
    'B': ["###.", "#..#", "#..#", "###.", "#..#", "#..#", "###."],
    'C': [".###", "#...", "#...", "#...", "#...", "#...", ".###"],
    'D': ["###.", "#..#", "#..#", "#..#", "#..#", "#..#", "###."],
    'E': ["####", "#...", "#...", "###.", "#...", "#...", "####"],
    'F': ["####", "#...", "#...", "###.", "#...", "#...", "#..."],
    'G': [".###", "#...", "#...", "#.##", "#..#", "#..#", ".###"],
    'H': ["#..#", "#..#", "#..#", "####", "#..#", "#..#", "#..#"],
    'I': ["###",  ".#.",  ".#.",  ".#.",  ".#.",  ".#.",  "###" ],
    'J': ["..##", "...#", "...#", "...#", "#..#", "#..#", ".##."],
    'K': ["#..#", "#.#.", "##..", "##..", "#.#.", "#.#.", "#..#"],
    'L': ["#...", "#...", "#...", "#...", "#...", "#...", "####"],
    'M': ["#...#","##.##","#.#.#","#.#.#","#...#","#...#","#...#"],
    'N': ["#..#", "##.#", "##.#", "#.##", "#.##", "#..#", "#..#"],
    'O': [".##.", "#..#", "#..#", "#..#", "#..#", "#..#", ".##."],
    'P': ["###.", "#..#", "#..#", "###.", "#...", "#...", "#..."],
    'Q': [".##.", "#..#", "#..#", "#..#", "#.#.", "#.#.", ".#.#"],
    'R': ["###.", "#..#", "#..#", "###.", "#.#.", "#..#", "#..#"],
    'S': [".###", "#...", "#...", ".##.", "...#", "...#", "###."],
    'T': ["###",  ".#.",  ".#.",  ".#.",  ".#.",  ".#.",  ".#." ],
    'U': ["#..#", "#..#", "#..#", "#..#", "#..#", "#..#", ".##."],
    'V': ["#...#","#...#","#...#","#...#",".#.#.",".#.#.","..#.."],
    'W': ["#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"],
    'X': ["#...#","#...#",".#.#.","..#..",".#.#.","#...#","#...#"],
    'Y': ["#...#","#...#",".#.#.","..#..","..#..","..#..","..#.."],
    'Z': ["####", "...#", "..#.", ".#..", "#...", "#...", "####"],
}

GLYPH_H = 7    # rows, one scanline each
GAP     = 1    # cells between glyphs


def glyph(ch):
    return GLYPHS.get(ch.upper(), GLYPHS[' '])


def text_width(text):
    """Width of `text` in cells, at scale 1."""
    total = 0
    for i, ch in enumerate(text):
        total += len(glyph(ch)[0])
        if i < len(text) - 1:
            total += GAP
    return total


def stamp(text, top, cells_wide, scale=1):
    """The set of (cell, row) positions `text` lights, centered across
    `cells_wide` cells with its first row at scanline `top`. `scale`
    magnifies both axes, so scale=2 is a 14-scanline title."""
    width = text_width(text) * scale
    left  = (cells_wide - width) // 2
    lit   = set()
    x     = left

    for ch in text:
        rows = glyph(ch)
        for r in range(GLYPH_H):
            for c in range(len(rows[0])):
                if rows[r][c] != '#':
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        lit.add((x + c * scale + sx, top + r * scale + sy))
        x += (len(rows[0]) + GAP) * scale

    return lit
