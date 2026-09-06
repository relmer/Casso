"""Build the two-glyph symbol font behind Casso's open and closed Apple keys.

    U+E000  open   (outline)
    U+E001  closed (filled)

Both glyphs come from ONE drawing, a CC0 apple from SVG Repo kept in
Assets/Apple:

    apple-closed.svg   https://www.svgrepo.com/svg/69341/apple-logo

The closed apple is that artwork as drawn. The open apple is the same region
hollowed out -- the outline is the drawing minus an inward offset of itself, so
the two are the same silhouette at the same size differing only in fill, which
is exactly how the //e keycaps tell the Apple keys apart. Two separate drawings
could not promise that: their bodies, leaves and bites disagreed, and the
mismatch showed in adjacent rows of one dialog.

The hollowing is an inward offset rather than a scaled copy, so the line keeps
one weight the whole way round -- through the bite and the notch under the leaf,
where a scaled copy pinches.

Needs fontTools and pyclipper:  pip install fonttools pyclipper

Run from the repository root:

    python scripts/GenSymbolFont.py
"""

import math
import os
import sys
import xml.etree.ElementTree as ET

import pyclipper
from fontTools.fontBuilder import FontBuilder
from fontTools.misc.transform import Transform
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.svgLib.path import parse_path


UPM = 1000
CAP = 755           # a round shape reads small at cap height, so overshoot it
BEARING = 45        # air either side, in font units
MAX_ERR = 0.8       # cubic-to-quadratic tolerance, in font units

# Line weight of the open apple, in font units against the cap above. Around a
# tenth of cap height is what Segoe UI's own stems come to, so the outline sits
# at text weight instead of reading as a pale icon; it also clears a whole pixel
# at 13 px, below which the glyph turns grey rather than hollow. Much heavier
# and the ring closes up where the leaf meets the shoulder.
STROKE = 74

SOURCE = "apple-closed.svg"

# name, codepoint, hollow
GLYPHS = [
    ("appleOpen",   0xE000, True),
    ("appleClosed", 0xE001, False),
]

CLIP = 64           # clipper is integer-only; give it sub-unit precision


def record(svg_path):
    """Every path in the file, replayed into one recording."""
    root = ET.parse(svg_path).getroot()
    pen = RecordingPen()
    for el in root.iter():
        if el.tag.endswith("path") and el.get("d"):
            parse_path(el.get("d"), pen)
    if not pen.value:
        raise SystemExit("no path data in " + svg_path)
    return pen


def bounds(pen):
    bp = BoundsPen(None)
    pen.replay(bp)
    return bp.bounds


class FlattenPen:
    """Collects contours as point lists. Needed only on the hollowing path --
    offsetting a region is a polygon operation, so the curves have to go."""

    STEPS = 32

    def __init__(self):
        self.contours = []
        self._cur = []

    def moveTo(self, pt):
        self._cur = [pt]

    def lineTo(self, pt):
        self._cur.append(pt)

    def curveTo(self, *pts):
        p0 = self._cur[-1]
        c1, c2, p1 = pts[-3], pts[-2], pts[-1]
        for i in range(1, self.STEPS + 1):
            t = i / float(self.STEPS)
            u = 1.0 - t
            self._cur.append(
                (u**3 * p0[0] + 3 * u * u * t * c1[0] + 3 * u * t * t * c2[0] + t**3 * p1[0],
                 u**3 * p0[1] + 3 * u * u * t * c1[1] + 3 * u * t * t * c2[1] + t**3 * p1[1]))

    def qCurveTo(self, *pts):
        prev = self._cur[-1]
        for i in range(len(pts) - 1):
            c, nxt = pts[i], pts[i + 1]
            for j in range(1, self.STEPS + 1):
                t = j / float(self.STEPS)
                u = 1.0 - t
                self._cur.append((u * u * prev[0] + 2 * u * t * c[0] + t * t * nxt[0],
                                  u * u * prev[1] + 2 * u * t * c[1] + t * t * nxt[1]))
            prev = nxt

    def closePath(self):
        if len(self._cur) > 2:
            self.contours.append(self._cur)
        self._cur = []

    endPath = closePath

    def addComponent(self, *args):
        pass


def simplify(pts, tol):
    """Douglas-Peucker, so a flattened curve does not spend hundreds of points
    on deviation no rasterizer can show."""
    def walk(sub):
        if len(sub) < 3:
            return sub
        ax, ay = sub[0]
        bx, by = sub[-1]
        dx, dy = bx - ax, by - ay
        span = math.hypot(dx, dy)
        worst, at = 0.0, 0
        for i in range(1, len(sub) - 1):
            px, py = sub[i]
            if span < 1e-9:
                d = math.hypot(px - ax, py - ay)
            else:
                d = abs(dy * px - dx * py + bx * ay - by * ax) / span
            if d > worst:
                worst, at = d, i
        if worst <= tol:
            return [sub[0], sub[-1]]
        return walk(sub[:at + 1])[:-1] + walk(sub[at:])

    return walk(list(pts) + [pts[0]])[:-1]


def hollow(contours, stroke):
    """The region minus an inward offset of itself: a ring of even weight."""
    outer = [[(int(round(x * CLIP)), int(round(y * CLIP))) for x, y in c]
             for c in contours]
    outer = pyclipper.SimplifyPolygons(outer, pyclipper.PFT_NONZERO)

    off = pyclipper.PyclipperOffset()
    off.AddPaths(outer, pyclipper.JT_ROUND, pyclipper.ET_CLOSEDPOLYGON)
    inner = off.Execute(-int(round(stroke * CLIP)))

    clip = pyclipper.Pyclipper()
    clip.AddPaths(outer, pyclipper.PT_SUBJECT, True)
    if inner:
        clip.AddPaths(inner, pyclipper.PT_CLIP, True)
        ring = clip.Execute(pyclipper.CT_DIFFERENCE,
                            pyclipper.PFT_NONZERO, pyclipper.PFT_NONZERO)
    else:
        ring = outer

    return [simplify([(x / float(CLIP), y / float(CLIP)) for x, y in p], 0.4)
            for p in ring]


def area(pts):
    a = 0.0
    for i in range(len(pts)):
        x0, y0 = pts[i]
        x1, y1 = pts[(i + 1) % len(pts)]
        a += x0 * y1 - x1 * y0
    return a / 2.0


def build(pen, xform, hollowed):
    tt = TTGlyphPen(None)

    if not hollowed:
        pen.replay(TransformPen(Cu2QuPen(tt, MAX_ERR), xform))
        return tt.glyph()

    flat = FlattenPen()
    pen.replay(TransformPen(flat, xform))

    for contour in hollow(flat.contours, STROKE):
        pts = [(int(round(x)), int(round(y))) for x, y in contour]
        tt.moveTo(pts[0])
        for p in pts[1:]:
            tt.lineTo(p)
        tt.closePath()

    return tt.glyph()


def main(assets, out_path):
    pen = record(os.path.join(assets, SOURCE))
    x0, y0, x1, y1 = bounds(pen)

    scale = CAP / (y1 - y0)
    width = (x1 - x0) * scale
    advance = int(round(width)) + 2 * BEARING
    left = int(round((advance - width) / 2))

    # SVG runs y-down; the flip is what puts the glyph on the baseline.
    xform = Transform(scale, 0, 0, -scale, -x0 * scale + left, y1 * scale)

    glyphs = {".notdef": TTGlyphPen(None).glyph()}
    metrics = {".notdef": (advance, 0)}

    for name, _, hollowed in GLYPHS:
        glyphs[name] = build(pen, xform, hollowed)
        metrics[name] = (advance, left)
        print("%-12s %d x %d, %d points%s" %
              (name, round(width), CAP, len(glyphs[name].coordinates),
               ", hollowed %d" % STROKE if hollowed else ""))

    order = [".notdef"] + [n for n, _, _ in GLYPHS]
    fb = FontBuilder(UPM, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap({code: name for name, code, _ in GLYPHS})
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable({
        "familyName": "Casso Symbols",
        "styleName": "Regular",
        "uniqueFontIdentifier": "CassoSymbols-Regular",
        "fullName": "Casso Symbols",
        "psName": "CassoSymbols-Regular",
        "version": "1.000",
        "copyright": "Apple glyph from SVG Repo, CC0 1.0 Universal.",
    })
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800,
                usWinDescent=200, sCapHeight=700, sxHeight=500,
                achVendID="CSSO")
    fb.setupPost(isFixedPitch=0)
    fb.save(out_path)
    print("wrote %s, %d bytes, advance %d" %
          (out_path, os.path.getsize(out_path), advance))


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "Assets", "Apple"),
         sys.argv[2] if len(sys.argv) > 2 else
         os.path.join(root, "Resources", "Fonts", "CassoSymbols.ttf"))
