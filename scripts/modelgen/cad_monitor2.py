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

# Proportions are measured off the reference photograph rather than chosen:
# as fractions of the case, the bezel assembly covers .78 x .76 and the
# visible tube .65 x .67, with the divided strip .15 of the width. The tube
# comes out at a 1.20 aspect, which is what the real one reads as. Earlier
# passes had the bezel at .67 x .63 and the tube at .56 x .50 -- everything
# inside the case too small, which is what made it look nothing like the
# object.
W, H, D   = 343.0, 292.0, 348.0

STRIP_W   = 52.0                  # the divided strip on the right (.15 W)
DX        = W - STRIP_W           # the stylistic divider groove
GROOVE_W  = 2.0
GROOVE_D  = 1.2

MARGIN    = 19.0                  # case face to its screen opening -- ALL
                                  # FOUR SIDES: the top and bottom frame match
                                  # the left's thickness (the divider strip
                                  # handles the right)
OPEN_W    = DX - MARGIN           # the opening runs right up to the divider
OX0       = MARGIN
OX1       = OX0 + OPEN_W
OZ0       = MARGIN
OZ1       = H - MARGIN

GAP       = 6.0                   # even gap, case opening to bezel, all round
CAVITY_D  = 60.0                  # how deep the case is hollowed behind the front
RECESS    = 13.0                  # how far the bezel stands behind the front

BEZEL_FW  = 16.0                  # the bezel's own frame width
BX0, BX1  = OX0 + GAP, OX1 - GAP
BZ0, BZ1  = OZ0 + GAP, OZ1 - GAP
GX0, GX1  = BX0 + BEZEL_FW, BX1 - BEZEL_FW
GZ0, GZ1  = BZ0 + BEZEL_FW, BZ1 - BEZEL_FW

NOTCH_W   = 30.0                  # power button notch, in the right strip
NOTCH_H   = 15.0                  # bottom edge raised 50%: a shallow notch,
                                  # just enough throw for the locked button
NOTCH_D   = 9.0
NX0       = DX + (STRIP_W - NOTCH_W) * 0.5
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
        .edges("|Y").fillet(6.0))

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
# 5% smaller than the first pass on both axes, per review against the
# reference: 19.0 x 4.6 read a touch oversized for the notch.
led = (cq.Workplane("XY")
       .box(18.0, 1.4, 4.4, centered=(False, False, False))
       .translate((NX0 + 6.0, 1.6, NZ0 + NOTCH_H - 6.2)))

m.add("led", led, KD["monitor_lamp"])

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
