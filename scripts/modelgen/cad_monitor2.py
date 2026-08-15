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
from cadkit import KD, Model, round_rect_wire, sag_sheet

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
DIAG_MM    = 11.0 * 25.4          # visible diagonal; the 12-in figure is the
                                  # tube class, not the picture (CRT Database)

GLASS_W = DIAG_MM * EMU_ASPECT / math.hypot(EMU_ASPECT, 1.0)
GLASS_H = DIAG_MM / math.hypot(EMU_ASPECT, 1.0)

MARGIN    = 19.0                  # case face to its screen opening, all sides
GAP       = 6.0                   # even gap, case opening to bezel, all round
GROOVE_W  = 2.0                   # the groove dividing the reveal off
GROOVE_D  = 1.2

# ------------------------------------------------------------------ the bezel
#
# The tube sits in an assembly that stands PROUD of the case, not sunk into
# it, and it has three distinct surfaces:
#
#   - Outer edges: flat walls running straight back into the case, no taper.
#   - Front: a flat band facing the user, 1/2 in wide, standing 3/4 in in
#     front of the case face.
#   - Inner: a funnel raked back from the band's inner edge to the tube's
#     rim, so the whole front slopes inward toward the picture on all sides.
#
# The rake is measured from the screen plane, where 90 degrees would run
# STRAIGHT back into the case: at 75 it is a little shy of that, so the
# funnel is deep and narrow rather than a wide shallow dish.
#
# Nothing here is a free choice. The band stands 3/4 in proud, the tube's rim
# sits in the case's own front plane, and the rake then FIXES the funnel's
# width between them -- which fixes the bezel frame, the case opening, and
# so the whole case.
PROTRUDE     = 0.75 * 25.4        # bezel front face, proud of the case face
BAND         = 0.50 * 25.4        # the flat front band's width
RAKE_DEG     = 60.0               # 90 would be straight back into the case
TUBE_DROP    = PROTRUDE           # so the tube's rim lands on the case face
BEZEL_FILLET = 3.0                # the bezel's outer corners
OPEN_FILLET  = BEZEL_FILLET + GAP  # and the case opening's, one gap outside
MOUTH_R      = 14.0               # the opening's corner radius: a tube face
                                  # is a rounded rectangle, and the funnel
                                  # follows it around
CORNER_ANG   = 0.03               # radians per segment where those radii live

# ------------------------------------------------------------ the faceplate
#
# CRT patents quote faceplate curvature against a reference radius
# R = 1.767 x the screen diagonal, with real tubes landing between 1.2R and
# 8R (the late flat-square sets).
#
# This one sits just BELOW that range, and from measurement rather than
# taste. A tube's mask edge lies on the sphere, so each edge's midpoint
# stands forward of its own corners and perspective magnifies it outward:
# a curved faceplate photographs with bowed edges, a flat one does not.
# Measured off a photo of the real unit, the top edge bows 20.9 px on a
# 706 px chord, which at the shot's ~300 mm camera distance works out to
# about 21 mm of corner sag -- R near 0.96 of the reference. That the answer
# lands outside the patents' range is consistent, not contradictory: those
# describe the flat-square generation, and this tube predates it.
#
# Note this is NOT the tube's depth: how far back the funnel and neck run is
# set by the deflection angle (about 90 degrees in this class), a separate
# parameter entirely. Tying the face's radius to the cabinet's depth would
# give a dome, not a screen.
FACE_R = 0.96 * 1.767 * DIAG_MM

# The reveal is sized from the power notch outward: a 1-inch notch with a
# quarter-inch margin to each side, 1.5 inches of reveal in all. The strip
# adds the groove that divides it from the symmetric front margin.
NOTCH_W   = 25.4                  # power button notch: 1 in
NOTCH_MGN = 6.35                  # margin beside the notch: 1/4 in
STRIP_W   = GROOVE_W + NOTCH_W + NOTCH_MGN * 2.0

FUNNEL_RUN = TUBE_DROP / math.tan(math.radians(RAKE_DEG))
BEZEL_FW   = BAND + FUNNEL_RUN     # the bezel frame's total width, derived

OPEN_W = GLASS_W + (GAP + BEZEL_FW) * 2.0
OPEN_H = GLASS_H + (GAP + BEZEL_FW) * 2.0

W = OPEN_W + MARGIN * 2.0 + STRIP_W
H = OPEN_H + MARGIN * 2.0
D = H * 1.18                      # the real case's depth-to-height proportion

DX  = W - STRIP_W                 # the reveal's position

# The reveal's axis: the strip measured from the groove's INNER edge to the
# frame's right edge, which is the power notch's center line. The brand mark
# and the debug ruler both hang off this.
REVEAL_CX = (DX + GROOVE_W + W) * 0.5
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

NOTCH_H   = 15.0                  # shallow: just enough throw for the button
NOTCH_D   = 25.4                  # 1 in front to back

# The button's face sits this far behind the case face, with clearance left
# behind it inside the notch. Its thickness is what is LEFT of the notch's
# depth once both are taken, so deepening the notch thickens the button and
# the setback stays put instead of the face sinking into the pocket.
BTN_SETBACK = 1.0
BTN_BACKGAP = 2.0
BTN_D       = NOTCH_D - BTN_SETBACK - BTN_BACKGAP

# Equal margins beside the notch: groove's inner edge to the notch's left
# equals the notch's right to the frame's right edge, the edge roll counted
# INSIDE the margin rather than appended to it.
NX0       = DX + GROOVE_W + NOTCH_MGN
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
#
# Its corners are rounded to hold the same gap the straight sides do. Cut
# square, the opening's corners stood outside the bezel's filleted ones and
# each corner showed a black triangle of bare cavity.
case = case.cut(
    cq.Workplane("XY").box(OX1 - OX0, CAVITY_D, OZ1 - OZ0, centered=(False, False, False))
      .edges("|Y").fillet(OPEN_FILLET)
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

# Finer than the default, and note it is the ANGULAR tolerance doing the
# work: at 3 mm the chords never sag far enough for the linear one to bite,
# so the stock setting spent about three segments on a quarter turn and the
# corners read as facets meeting at an angle rather than as rounds.
m.add("case", case, BEIGE, angular=CORNER_ANG)

# The cavity's inner surfaces, so the recess around the bezel reads dark
# rather than as more case. A thin shell lining the pocket.
lining = (cq.Workplane("XY")
          .box(OX1 - OX0, CAVITY_D - 2.0, OZ1 - OZ0, centered=(False, False, False))
          .edges("|Y").fillet(OPEN_FILLET)
          .translate((OX0, 1.0, OZ0))
          .faces("<Y").shell(-1.2))

m.add("cavity", lining, CAVITY)

# -------------------------------------------------------------------- bezel

# Its own assembly, kept separate because the real one pivots a few degrees
# about a horizontal axis through the tube's center.
#
# A block from the protruding front face back into the case gives the flat
# outer walls in one step. What shapes it is the CUT: a loft that starts as
# the band's inner opening at the front face and narrows to the tube's rim
# one drop back -- that lofted wall IS the raked funnel -- continued straight
# back so the space behind the tube is hollow.
BAND_X0, BAND_X1 = BX0 + BAND, BX1 - BAND
BAND_Z0, BAND_Z1 = BZ0 + BAND, BZ1 - BAND

bezel = (cq.Workplane("XY")
         .box(BX1 - BX0, PROTRUDE + CAVITY_D * 0.5, BZ1 - BZ0, centered=(False, False, False))
         .translate((BX0, -PROTRUDE, BZ0))
         .edges("|Y").fillet(BEZEL_FILLET))

# Rounded loft sections, so the funnel's own corners -- the ones running
# front-to-back from the band down to the tube -- come out radiused without
# having to select four diagonal edges after the fact.
funnel = cq.Solid.makeLoft([
    round_rect_wire(-PROTRUDE,             BAND_X0, BAND_X1, BAND_Z0, BAND_Z1, MOUTH_R),
    round_rect_wire(-PROTRUDE + TUBE_DROP, GX0,     GX1,     GZ0,     GZ1,     MOUTH_R),
])

bezel = bezel.cut(cq.Workplane(obj=funnel))

# The tunnel behind the mouth carries the SAME rounded profile. Cut square,
# its corners stood proud of the funnel's rounded ones and left a wedge of
# bezel hanging into each corner of the opening.
tunnel = cq.Solid.makeLoft([
    round_rect_wire(-PROTRUDE + TUBE_DROP,             GX0, GX1, GZ0, GZ1, MOUTH_R),
    round_rect_wire(-PROTRUDE + TUBE_DROP + CAVITY_D,  GX0, GX1, GZ0, GZ1, MOUTH_R),
])

bezel = bezel.cut(cq.Workplane(obj=tunnel))

m.add("bezel", bezel, BEZEL, angular=CORNER_ANG)

# --------------------------------------------------------------------- tube

# The sheet the live display maps onto, filling the funnel's inner mouth: a
# section of a sphere, its RIM on the mouth's plane and its center bulging
# forward from there toward the band. sag_sheet wants the radius in half
# diagonals of the SHEET, so convert the faceplate's physical radius here
# rather than carrying a second, hand-tuned number.
GLASS_HALF_DIAG = math.hypot((GX1 - GX0 - 2.0) * 0.5, (GZ1 - GZ0 - 2.0) * 0.5)

m.add_triangles("glass",
                sag_sheet(GX0 + 1.0, GX1 - 1.0, GZ0 + 1.0, GZ1 - 1.0,
                          front_y=-PROTRUDE + TUBE_DROP,
                          radius_scale=FACE_R / GLASS_HALF_DIAG),
                KD["glass"])

# ------------------------------------------------------- power button + LED

# The button, locked down: it fills the lower part of the notch and stands
# proud of the notch floor, not of the case.
button = (cq.Workplane("XY")
          .box(NOTCH_W - 3.0, BTN_D, NOTCH_H - 8.5, centered=(False, False, False))
          .translate((NX0 + 1.5, BTN_SETBACK, NZ0 + 1.0))
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

# ------------------------------------------------------------ brand anchor
#
# Where the cassowary goes, carried BY THE MODEL. The scene draws the mark
# itself (it is a multi-color stamp, not one Kd), but it reads this marker
# for the axis to center it on and throws the geometry away. Sizing the
# reveal used to move that axis while the scene's copy of the number stayed
# put, and the mark drifted off the line every time; now there is only one
# number and the generator owns it. Buried inside the case wall so it is
# never visible.
m.add("brand_anchor",
      cq.Workplane("XY")
        .box(0.4, 0.4, 0.4, centered=(True, True, True))
        .translate((REVEAL_CX, D * 0.5, H * 0.5)),
      KD["brand_anchor"])

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
DEBUG_CENTER_LINE = False
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
    nv, nt = m.emit(os.path.join(out, "Monitor2.mesh"),
                    os.path.join(out, "Monitor2.mtl"), "Monitor2.mtl")
    print(f"Monitor2 (CAD): {nv} verts, {nt} tris")
    print(f"  case {W:.1f} x {H:.1f} x {D:.1f} mm, glass {GLASS_W:.1f} x {GLASS_H:.1f}")
    print(f"  reveal axis x = {REVEAL_CX:.1f}  <- s_kMon2BrandCenterXMm")
    print(f"  notch depth {NOTCH_D:.2f}, button {BTN_D:.2f} thick, setback {BTN_SETBACK:.1f}")
