"""Apple Monitor II (A2M2010) as a real solid model.

Built with a CAD kernel rather than emitted quad by quad, because the shape
kept coming out wrong the other way and the failures were invisible in the
source: a case built as a closed box quietly put a face across the whole
front and hid the screen behind it. Here the screen opening is a BOOLEAN CUT,
so it is a hole by construction, and the case's soft edges are FILLETS rather
than hand-built chamfer strips.

Modeled from photographs of the real unit:

  - Square-edged beige box in the //e case color, lightly rounded all round.
  - The tube does not sit in the case. It sits in a SEPARATE, DARKER BROWN
    BEZEL, inset behind the case's front opening with an even gap all the way
    around it. That gap and its shadow are most of what the front reads as.
  - A stylistic vertical groove divides a narrow strip off the RIGHT of the
    front. The even gap is symmetric only over the part left of that groove;
    the strip carries the power button at the top and the brand at the bottom.
  - The power button is a NOTCH in the top right. It presses down from above
    and locks there, and dropping out of the way is what uncovers the green
    power LED behind it -- a wide, short rectangle, long axis left to right.
    Modeled with the button already down, which is the state a running
    machine is in.

X right, Y back, Z up; the case front sits at y=0.
"""

import cadquery as cq
from cadkit import KD, Model, sag_sheet

# ---------------------------------------------------------------- dimensions
#
# BUILT OUTWARD FROM THE TUBE. The visible CRT carries the emulator's own
# aspect (280:192 = 35:24) at the advertised 12-inch tube's 11.5-inch visible
# diagonal, and every case dimension derives from it: glass -> bezel frame ->
# recess gap -> front frame margin -> case. Nothing else decides the case's
# shape; a case chosen first and squared off is how the screen ended up a
# giant square with the picture letterboxed inside it.

import math

EMU_ASPECT = 280.0 / 192.0        # the emulator display's own aspect
DIAG_MM    = 11.5 * 25.4          # visible tube diagonal, advertised 12-in class

GLASS_W = DIAG_MM * EMU_ASPECT / math.hypot(EMU_ASPECT, 1.0)
GLASS_H = DIAG_MM / math.hypot(EMU_ASPECT, 1.0)

MARGIN    = 19.0                  # case face to its screen opening, all sides
GAP       = 6.0                   # even gap, case opening to bezel, all round
BEZEL_FW  = 16.0                  # the bezel's own frame width
STRIP_W   = 52.0                  # the divided strip on the right
GROOVE_W  = 2.0                   # the reveal dividing it off
GROOVE_D  = 1.2

OPEN_W = GLASS_W + (GAP + BEZEL_FW) * 2.0
OPEN_H = GLASS_H + (GAP + BEZEL_FW) * 2.0

W = OPEN_W + MARGIN * 2.0 + STRIP_W
H = OPEN_H + MARGIN * 2.0
D = H * 1.18                      # the real case's depth-to-height proportion

DX  = W - STRIP_W                 # the reveal's position
OX0 = MARGIN
OX1 = OX0 + OPEN_W
OZ0 = MARGIN
OZ1 = H - MARGIN

CAVITY_D  = 60.0                  # how deep the case is hollowed behind the front
RECESS    = 13.0                  # how far the bezel stands behind the front

BX0, BX1  = OX0 + GAP, OX1 - GAP
BZ0, BZ1  = OZ0 + GAP, OZ1 - GAP
GX0, GX1  = BX0 + BEZEL_FW, BX1 - BEZEL_FW
GZ0, GZ1  = BZ0 + BEZEL_FW, BZ1 - BEZEL_FW

NOTCH_W   = 30.0                  # power button notch, in the right strip
NOTCH_H   = 15.0                  # shallow: just enough throw for the button
NOTCH_D   = 10.0                  # 1 cm front to back

# Equal margins beside the notch: groove's inner edge to the notch's left
# equals the notch's right to the frame's right edge. Centering on the whole
# strip instead put the groove-side margin a millimeter tight while the
# right side carried its margin PLUS the edge roll, reading wider still.
NX0       = DX + GROOVE_W + (STRIP_W - GROOVE_W - NOTCH_W) * 0.5
NZ1       = H
NZ0       = H - NOTCH_H

# NOTE: sub-mesh identity is by Kd VALUE (DeskSceneModel::kKdEpsilon = 0.02).
# Case colors must stay clear of the palette in cadkit.KD.
BEIGE     = (0.833, 0.784, 0.659)     # the //e case color
BEIGE_DK  = (0.700, 0.652, 0.540)
BEZEL     = (0.548, 0.494, 0.396)     # the darker brown bezel
BEZEL_DK  = (0.430, 0.386, 0.306)
CAVITY    = (0.105, 0.098, 0.086)

m = Model()

# --------------------------------------------------------------------- case

# A solid box, edges softened, then hollowed from the front and cut through.
case = (cq.Workplane("XY")
        .box(W, D, H, centered=(False, False, False))
        .edges("|Y").fillet(3.0))

# The screen cavity: a pocket back from the front face. Cutting it is what
# makes the opening a hole -- the failure mode of the hand-built version was
# a case front that was quietly solid.
case = case.cut(
    cq.Workplane("XY").box(OX1 - OX0, CAVITY_D, OZ1 - OZ0, centered=(False, False, False))
      .translate((OX0, -1.0, OZ0)))

# The stylistic divider groove down the front of the right strip.
case = case.cut(
    cq.Workplane("XY").box(GROOVE_W, GROOVE_D, H, centered=(False, False, False))
      .translate((DX, 0.0, 0.0)))

# The power notch: cut from the FRONT and through the TOP, which is what lets
# the button be pressed from above.
case = case.cut(
    cq.Workplane("XY").box(NOTCH_W, NOTCH_D, NOTCH_H + 2.0, centered=(False, False, False))
      .translate((NX0, -0.5, NZ0)))

m.add("case", case, BEIGE)

# The cavity's inner surfaces, so the recess around the bezel reads dark
# rather than as more case. A thin shell lining the pocket.
lining = (cq.Workplane("XY")
          .box(OX1 - OX0, CAVITY_D - 2.0, OZ1 - OZ0, centered=(False, False, False))
          .translate((OX0, 1.0, OZ0))
          .faces("<Y").shell(-1.2))

m.add("cavity", lining, CAVITY)

# -------------------------------------------------------------------- bezel

# Its own assembly: a frame with the tube aperture cut through it, standing
# at the back of the recess. The real one pivots a few degrees about a
# horizontal axis through the tube's center, so it is kept a separate solid.
bezel = (cq.Workplane("XY")
         .box(BX1 - BX0, 16.0, BZ1 - BZ0, centered=(False, False, False))
         .translate((BX0, RECESS, BZ0))
         .edges("|Y").fillet(3.0))

bezel = bezel.cut(
    cq.Workplane("XY").box(GX1 - GX0, 20.0, GZ1 - GZ0, centered=(False, False, False))
      .translate((GX0, RECESS - 2.0, GZ0)))

m.add("bezel", bezel, BEZEL)

# --------------------------------------------------------------------- tube

# The sheet the live display maps onto. A 12-inch tube is far flatter than
# the 9-inch //c's: too tight a sphere curls the corners away hard enough
# that the raster reads as a disc instead of a rectangle.
m.add_triangles("glass",
                sag_sheet(GX0 + 1.0, GX1 - 1.0, GZ0 + 1.0, GZ1 - 1.0,
                          front_y=RECESS + 13.0, radius_scale=9.0),
                KD["glass"])

# ------------------------------------------------------- power button + LED

# The button, locked down: it fills the lower part of the notch and stands
# proud of the notch floor, not of the case.
button = (cq.Workplane("XY")
          .box(NOTCH_W - 3.0, NOTCH_D - 3.0, NOTCH_H - 8.5, centered=(False, False, False))
          .translate((NX0 + 1.5, 1.0, NZ0 + 1.0))
          .edges("|Y").fillet(1.5))

m.add("button", button, BEZEL_DK)

# The LED sits ABOVE it, uncovered because the button is down. Wide and
# short -- long axis left to right.
# Trimmed twice on review against the reference: 5% off the first pass,
# then another 20% -- a power LED is a sliver, not a light bar.
led = (cq.Workplane("XY")
       .box(14.4, 1.4, 3.5, centered=(False, False, False))
       .translate((NX0 + (NOTCH_W - 14.4) * 0.5, 1.6, NZ0 + NOTCH_H - 5.7)))

m.add("led", led, KD["monitor_lamp"])

# ------------------------------------------------------- molded power icon
#
# Below the button on the reveal strip: the power symbol as MOLD RELIEF, not
# ink -- raised ridges in the case's own plastic, one color with the strip,
# read entirely through the shading of their side faces. A square (75% of
# the button's width), a circle inside it that does not quite touch the
# square, and a vertical bar inside that which does not quite touch the
# circle.

ICON_S   = (NOTCH_W - 3.0) * 0.75          # square side: 75% of the button width
ICON_CX  = NX0 + NOTCH_W * 0.5            # centered under the button
ICON_TOP = NZ0 - 2.0                       # 2 mm below the notch
ICON_CZ  = ICON_TOP - ICON_S * 0.5
RIDGE_W  = 1.5                             # stroke width of the relief
RIDGE_H  = 1.0                             # a full millimeter proud: at 0.45
                                           # the relief barely read at all

IX0 = ICON_CX - ICON_S * 0.5
IZ0 = ICON_TOP - ICON_S

icon_sq = (cq.Workplane("XY")
           .box(ICON_S, RIDGE_H, ICON_S, centered=(False, False, False))
           .translate((IX0, -RIDGE_H, IZ0)))
icon_sq = icon_sq.cut(
    cq.Workplane("XY")
      .box(ICON_S - RIDGE_W * 2.0, RIDGE_H + 1.0, ICON_S - RIDGE_W * 2.0,
           centered=(False, False, False))
      .translate((IX0 + RIDGE_W, -RIDGE_H - 0.5, IZ0 + RIDGE_W)))

ICON_R = ICON_S * 0.5 - RIDGE_W - 1.0      # ...which does not quite touch

icon_ring = (cq.Workplane("XY")
             .cylinder(RIDGE_H, ICON_R, direct=(0, 1, 0), centered=(True, True, False))
             .translate((ICON_CX, -RIDGE_H, ICON_CZ)))
icon_ring = icon_ring.cut(
    cq.Workplane("XY")
      .cylinder(RIDGE_H + 1.0, ICON_R - RIDGE_W, direct=(0, 1, 0), centered=(True, True, False))
      .translate((ICON_CX, -RIDGE_H - 0.5, ICON_CZ)))

BAR_H = (ICON_R - RIDGE_W - 1.0) * 2.0     # ...which does not quite touch

icon_bar = (cq.Workplane("XY")
            .box(RIDGE_W, RIDGE_H, BAR_H, centered=(False, False, False))
            .translate((ICON_CX - RIDGE_W * 0.5, -RIDGE_H, ICON_CZ - BAR_H * 0.5)))

m.add("icon_sq",   icon_sq,   BEIGE)
m.add("icon_ring", icon_ring, BEIGE)
m.add("icon_bar",  icon_bar,  BEIGE)

# ---------------------------------------------- calibration ruler (debug)
#
# A 3-inch vertical magenta line up the REVEAL's axis, rising from the case
# bottom: an in-scene ruler for judging the brand stamp's centering. The
# reveal's axis is the strip's, measured from the groove's INNER edge to the
# frame's right edge -- the same line the power notch centers on, so the
# ruler, button, and molded icon all share one column (the user's annotated
# centerline pinned this definition; measuring from the screen opening
# instead read visibly left). Because the ruler lives ON the case it suffers
# the exact same perspective as the stamp at every height, so "the mark's
# center of gravity sits on the line" can be judged straight off a capture.
# Not part of the product model -- flip the flag off and regenerate before
# shipping.
REVEAL_CX = (DX + GROOVE_W + W) * 0.5

DEBUG_CENTER_LINE = True
if DEBUG_CENTER_LINE:
    m.add("calibration",
          cq.Workplane("XY")
            .box(1.2, 0.6, 76.2, centered=(False, False, False))
            .translate((REVEAL_CX - 0.6, -0.6, 0.0)),
          (1.0, 0.0, 1.0))

# Rear lid vents, as shallow cuts rather than proud strips.
for i in range(9):
    m.add(f"vent{i}",
          cq.Workplane("XY")
            .box(W - 130.0, 7.0, 1.0, centered=(False, False, False))
            .translate((65.0, D - 92.0 + i * 9.0, H - 1.0)),
          BEIGE_DK)


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Monitor2.obj"),
                    os.path.join(out, "Monitor2.mtl"), "Monitor2.mtl")
    print(f"Monitor2 (CAD): {nv} verts, {nt} tris")
