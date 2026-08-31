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
  - THE REAR IS A LECTERN, not a wall. The top rear of the case is sheared
    off at a slope, and that sloped face carries a recessed panel between two
    full-depth side cheeks: vent slots across its upper band, a blank spec
    plate over their middle, and the tube's BELL emerging through its lower
    half -- a gray hopper in the bezel's color, not the case's, wearing the
    cassowary where the real unit wears its maker's sticker. Below the slope
    a vertical strip carries the control panel: a lighter inset plate with
    the mains inlet, three recessed thumbwheels, the video-in RCA, and an
    engraved icon over each control.

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
import sys

EMU_ASPECT = 280.0 / 192.0        # the emulator display's own aspect
DIAG_MM    = 11.0 * 25.4          # visible diagonal; the 12-in figure is the
                                  # tube class, not the picture (CRT Database)

GLASS_W = DIAG_MM * EMU_ASPECT / math.hypot(EMU_ASPECT, 1.0)
GLASS_H = DIAG_MM / math.hypot(EMU_ASPECT, 1.0)

MARGIN    = 19.0                  # case face to its screen opening, all sides
GAP       = 6.0                   # even gap, case opening to bezel, all round
GROOVE_W  = 2.0                   # the groove dividing the reveal off
GROOVE_R  = GROOVE_W * 0.5        # ...cut as a HALF-ROUND channel, so its
                                  # depth is its half-width and not a free
                                  # parameter. Anything deeper than this
                                  # would put the channel's widest point
                                  # below the surface -- an undercut, which
                                  # is not a shape that comes out of a tool.

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

# THE SAME RADIUS, deliberately -- not radius-plus-gap. Offsetting the
# opening's corner by the gap keeps the clearance constant around the bend,
# which is the machinist's answer; but molded plastic is drawn with one
# corner radius shared between a part and the opening that receives it, and
# radius-plus-gap read as the frame's corners being visibly rounder than the
# bezel sitting in them.
OPEN_FILLET  = BEZEL_FILLET
MOUTH_R      = 14.0               # the opening's corner radius: a tube face
                                  # is a rounded rectangle, and the funnel
                                  # follows it around
# HOW FINELY THE ROUNDED WORK IS TESSELLATED, in radians per segment.
#
# This was 0.03, and 0.03 was the right answer to the question being asked
# at the time. The scene shaded every triangle by its own face normal, so a
# curved surface came out in bands and the only cure available was more
# triangles. Backing off to 0.06 was already visibly banded on the funnel.
#
# The app averages vertex normals now (MeshNormals, at bake time), so a
# curve shades continuously across however few triangles describe it and
# the two questions have come apart. 0.14 with smoothed normals renders
# indistinguishably from 0.03 without them, at 111,220 triangles instead of
# 926,142.
#
# 0.14 is also the Monitor //c's value, so the two agree again. The linear
# tolerance still bounds the sag, so a larger radius picks up the extra
# segments it needs without this having to know about it.
CORNER_ANG   = 0.14

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
# The case, LESS THE BELL. The reference meshes put the case's own depth
# just under its height, with the tube's bell running well past the rear
# face -- the earlier 1.18 put all of that depth into the case and left the
# bell buried flush, a monitor with a backpack molded into its shirt.
D         = H * 0.95
BELL_BACK = 55.0                  # how far the bell runs past the case

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

# The cutter starts slightly proud of the face so the boolean never has to
# resolve coincident faces. What that leaves at the far end is the notch's
# REAR WALL, and anything mounted inside the pocket hangs off it -- so name
# it once here rather than letting each part guess its own depth.
NOTCH_OVERCUT = 0.5
NOTCH_REAR_Y  = NOTCH_D - NOTCH_OVERCUT

# ------------------------------------------------------------ mold relief
#
# Shared by every molded mark on the case, and by the brand recess, which is
# why they live up here with the dimensions rather than beside the first icon
# that happens to use them.
RIDGE_W  = 1.0                             # stroke width of the relief
RIDGE_H  = 0.5                             # proud of the face. The history
                                           # here is worth keeping: at 0.45
                                           # the relief barely read, and at
                                           # 1.0 a FILLED glyph still did not,
                                           # so this went to 2.5 to give the
                                           # side walls something to show.
                                           # That reasoning expired when the
                                           # scene started casting real
                                           # shadows -- a stroke now reads by
                                           # what it throws across the face
                                           # beside it, not by how much wall
                                           # it can turn toward the camera,
                                           # and 2.5 mm of relief on a 1 mm
                                           # stroke was always more than the
                                           # real case carries.
BRAND_D  = RIDGE_H                         # the badge is as thick as the
                                           # icons are proud, BY DESIGN, and
                                           # sits in a recess of the same
                                           # depth so its face finishes flush
                                           # with the frame. Named separately
                                           # only so the two uses are legible
                                           # -- it tracks RIDGE_H and is meant
                                           # to. DeskSceneModel's
                                           # s_kMon2BrandThickMm is a literal
                                           # that CANNOT track it, so changing
                                           # this means changing that too.
# Front-edge round-over on all mold relief. DERIVED, because it is bounded by
# two different dimensions and a fixed number silently violates one of them:
# it has to stay under half the stroke WIDTH (or opposite fillets collide) and
# under the relief's HEIGHT (or there is no wall left to round). Thinning
# RIDGE_H to 0.5 with this pinned at 0.35 asked OCC for a round-over 70% as
# tall as the feature, and every fillet in the file failed at once.
RELIEF_ROUND = min (0.35, RIDGE_W * 0.45, RIDGE_H * 0.4)

# The button's face sits this far behind the case face, with clearance left
# behind it inside the notch. Its thickness is what is LEFT of the notch's
# depth once both are taken, so deepening the notch thickens the button and
# the setback stays put instead of the face sinking into the pocket.
BTN_SETBACK = 1.0
BTN_BACKGAP = 2.0

# The button's BACK FACE is the fixed end -- it rides against the rear of the
# notch and always has -- so halving its depth takes the front forward, not
# the back backward. Anchoring the front instead would have slid the whole
# button out of the pocket it sits in.
BTN_REAR_Y  = NOTCH_D - BTN_BACKGAP
BTN_D       = (NOTCH_D - BTN_SETBACK - BTN_BACKGAP) * 0.5

# Equal margins beside the notch: groove's inner edge to the notch's left
# equals the notch's right to the frame's right edge, the edge roll counted
# INSIDE the margin rather than appended to it.
NX0       = DX + GROOVE_W + NOTCH_MGN
NZ1       = H
NZ0       = H - NOTCH_H

# NOTE: sub-mesh identity is by Kd VALUE (DeskSceneModel::kKdEpsilon = 0.02).
# Case colors must stay clear of the palette in cadkit.KD.
BEIGE     = (0.845, 0.796, 0.670)     # the //e case color
BEIGE_DK  = (0.735, 0.692, 0.582)
# Sampled off a photo of a well lit A2M2010, comparing only surfaces that
# face the camera under the same light: the frame's top strip against the
# bezel's top band, and the frame's right strip against the power button
# beside it. Both parts land ~0.82 of the frame, so the button is the same
# value as the bezel rather than a step below it. The part that is easy to
# get backwards is the hue -- these get WARMER as they darken (B/R falls
# 0.894 -> 0.832 -> 0.781), and a beige that desaturates on the way down
# is exactly what reads as //c platinum instead of a classic //e.
BEZEL     = (0.720, 0.644, 0.531)     # 0.82 of the frame, a touch warmer
BEZEL_DK  = (0.726, 0.654, 0.507)     # the power button: warmer still
CAVITY    = (0.105, 0.098, 0.086)

# The rear's own parts. Chosen clear of the loader's finish markers
# (kPlatePebbledKd, kPlateRecessKd, and cadkit.KD) by more than kKdEpsilon in
# at least one channel -- a color inside that band is an IDENTITY, and a
# thumbwheel that happened to land on the pebble marker would come back
# repainted matte black with a molded grain.
LABEL_GRAY = (0.600, 0.585, 0.555)    # the blank spec plate

# The rear is A DIFFERENT PLASTIC from the case: the vent surface, the
# control panel, its embossed marks, and the bell are ONE molding in ONE
# color. The bell used to sit a step darker, and the step read as two parts
# where the reference shows one.
PANEL_GRAY = (0.560, 0.545, 0.520)
BELL_GRAY  = PANEL_GRAY
DARK_PART  = (0.085, 0.085, 0.090)    # wheels, inlet, RCA barrel and bore
RCA_RING   = (0.920, 0.910, 0.890)

m = Model()

# --------------------------------------------------------------------- case

# A solid box, edges softened, then hollowed from the front and cut through.
#
# BOTH SETS OF EDGES, and both here at the top rather than after the cuts.
# The four long corners running front to back take the big radius; the front
# face's whole perimeter -- where it meets the top, the sides and the
# underside -- takes a smaller one. Rounded now, while the case is still a
# plain box, they cost one fillet each and cannot fail; left until after the
# cavity, the notch, the groove and the slope have all been taken out of it,
# the same edges have no selector that names them and a blanket fillet across
# what is left refuses outright.
CASE_FRONT_R = 1.5

case = (cq.Workplane("XY")
        .box(W, D, H, centered=(False, False, False))
        .edges("|Y").fillet(3.0)
        .edges("<Y").fillet(CASE_FRONT_R))

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

# The stylistic divider groove down the front of the right strip, cut as a
# half-round channel rather than a square trench.
#
# A square trench on a face lit from above shows one thin dark line where the
# floor meets the far wall, and nothing else -- both of its walls are parallel
# to the light and neither takes a highlight, so the divider was nearly
# invisible. A round channel turns some of its surface toward the light all
# the way down, so it carries a highlight along one flank and shade along the
# other. Same reasoning as the mold relief on the icons, inverted: proud
# there, sunk here.
case = case.cut(
    cq.Workplane("XY")
      .cylinder(H + 2.0, GROOVE_R, direct=(0, 0, 1), centered=(True, True, False))
      .translate((DX + GROOVE_R, 0.0, -1.0)))

# The power notch: cut from the FRONT and through the TOP, which is what lets
# the button be pressed from above.
case = case.cut(
    cq.Workplane("XY").box(NOTCH_W, NOTCH_D, NOTCH_H + 2.0, centered=(False, False, False))
      .translate((NX0, -NOTCH_OVERCUT, NZ0)))

# The notch's MOUTH is broken over, not left as a knife edge. Selected by a
# thin slab across the front face around the opening rather than by a face
# selector, because after this many cuts the case has no edge set that names
# this opening on its own. A refusal is survivable -- the notch is merely
# sharp then -- but it says so, the way the front relief does.
NOTCH_EDGE_R = 0.7

try:
    case = (case.edges(cq.selectors.BoxSelector(
                (NX0 - 2.0, -NOTCH_OVERCUT - 1.0, NZ0 - 2.0),
                (NX0 + NOTCH_W + 2.0, 1.5, H + 2.0)))
                .fillet(NOTCH_EDGE_R))
except Exception as exc:
    print(f"WARNING: power notch mouth: round-over FAILED "
          f"({type(exc).__name__}: {exc}) -- shipping it SHARP",
          file=sys.stderr)

# THE NOTCH HAS TWO MOUTHS. It is cut from the front AND through the top, so
# the top face carries an opening of its own, and the slab above reaches only
# 1.5 mm back from the front face -- it rounds the front opening and the first
# sliver of the top one, and never sees the edge where the notch's BACK WALL
# meets the top of the case, 25 mm behind it. That edge shipped as a knife
# edge while the sides around it were broken over, which is exactly how it
# read: soft everywhere but across the back.
#
# Its own selector and its own try, for the reason the first one has: the case
# has no edge set that names this opening, and a refusal should say so rather
# than take the front's round-over down with it.
try:
    case = (case.edges(cq.selectors.BoxSelector(
                (NX0 - 2.0, NOTCH_REAR_Y - 1.5, H - 1.5),
                (NX0 + NOTCH_W + 2.0, NOTCH_REAR_Y + 1.5, H + 1.5)))
                .fillet(NOTCH_EDGE_R))
except Exception as exc:
    print(f"WARNING: power notch rear top: round-over FAILED "
          f"({type(exc).__name__}: {exc}) -- shipping it SHARP",
          file=sys.stderr)

# The brand recess, down the strip below the icons: a shallow rounded-corner
# pocket exactly RIDGE_H deep. The scene stands the cassowary in it at the
# same RIDGE_H thickness, so the mark's face finishes FLUSH with the frame
# around it -- an inlaid badge rather than a decal lying on the surface.
#
# COUPLED TO THE SCENE. DeskSceneModel's s_kMon2Brand* constants decide where
# the mark is drawn; these decide where the hole is. They have to agree, so
# both sides carry a pointer to the other. The extent below is the silhouette's
# own drawn bounds (cols 3..33 and rows 5..53 of a 36x54 grid at 24 mm tall,
# which is 13.78 x 21.78 mm) plus a margin, centered on the reveal axis --
# the mark is centroid-centered on that same axis, so its visual mass sits in
# the middle of the pocket even though its bounding box does not.
BRAND_MGN  = 2.2
BRAND_HALF = 8.412 + BRAND_MGN             # the silhouette's wider side
BRAND_Z0   = 22.000 - BRAND_MGN
BRAND_Z1   = 43.778 + BRAND_MGN

case = case.cut(
    cq.Workplane("XY")
      .box(BRAND_HALF * 2.0, BRAND_D + 1.0, BRAND_Z1 - BRAND_Z0,
           centered=(False, False, False))
      .translate((REVEAL_CX - BRAND_HALF, -1.0, BRAND_Z0))
      .edges("|Y").fillet(RELIEF_ROUND))

# ...and break the rim where the pocket meets the face, so the recess reads
# as molded rather than milled. The cutter's own fillet rounds the four
# CORNERS in plan; this rounds the LIP, which is a different edge loop and
# has to be taken on the case after the cut. Selected by bounding box: the
# only edges sitting on the face plane inside the pocket's footprint are the
# four sides of its mouth and the corner arcs joining them.
try:
    case = case.edges(
        cq.selectors.BoxSelector(
            (REVEAL_CX - BRAND_HALF - 0.5, -0.4, BRAND_Z0 - 0.5),
            (REVEAL_CX + BRAND_HALF + 0.5,  0.4, BRAND_Z1 + 0.5))).fillet(RELIEF_ROUND)
except Exception as exc:
    print(f"WARNING: brand recess: lip round-over FAILED "
          f"({type(exc).__name__}: {exc}) -- the pocket keeps a sharp rim",
          file=sys.stderr)

# ------------------------------------------------------------------ the rear
#
# The case's top rear is SHEARED OFF at a slope, and everything on that slope
# is built flat and tilted into place: the recess, its vents, and the spec
# plate are all constructed against a vertical rear face at y = D -- plain
# boxes -- then rotated about the slope's hinge line as one gesture. Working
# in the tilted plane directly means every cutter needs its own rotated
# frame; working flat and tilting the finished cutter needs one.
#
# Coordinates on the slope are measured ALONG it from the hinge, so the
# pre-tilt z axis is arc length up the face.
STRIP_TOP  = 84.0                     # the vertical rear strip below the slope
REAR_RUN   = 68.0                     # how far forward the slope's top lands
SLOPE_LEN  = math.hypot (REAR_RUN, H - STRIP_TOP)
SLOPE_ANG  = math.degrees (math.atan2 (REAR_RUN, H - STRIP_TOP))
REAR_RIM   = 15.0                     # case border above the dark column

# THE DARK COLUMN. Everything dark on the rear -- the vents, the bell, the
# control panel -- lives in ONE CRT-centered column just wider than the
# bell, and the case's beige runs up to its edges. The case is wider than
# its screen by the front's right-hand reveal, so the column rides the
# screen's axis and the beige stands one full reveal wider on the rear's
# left than on its right.
BELL_CX = DX * 0.5

# ONE WIDTH FOR EVERY DARK THING BACK HERE, and the bell is one of them.
# The rear molding, the vent recess, the control panel and the bell all
# take their width from this half-width, so the dark column never changes
# width between the vents and the receptacle. The bell had been a separate
# 210 against a 200 column and bulged five millimeters past it on each
# side -- the last remaining step, and the one that made every width fix
# above look like it had not worked.
#
# The recess OPENING is COL_HW; the molding runs DARK_BLEED wider so the
# opening's walls are dark too; and BELL_HW equals COL_HW + DARK_BLEED, so
# the bell's flank and the molding's edge are the same line. The bell's
# flat top -- its half-width less its corner radius -- still has to cover
# the recess opening, which is what keeps the recess walls landing on the
# bell instead of past it.
COL_HW  = 94.0


def tilt_rear(wp):
    """Rotate a pre-tilt cutter or part built against the vertical y = D
    plane into the sloped rear face, about the slope's hinge line."""
    return wp.rotate ((0.0, D, STRIP_TOP), (1.0, D, STRIP_TOP), SLOPE_ANG)


# The shear itself: everything behind the sloped plane, above the strip.
case = case.cut (tilt_rear (
    cq.Workplane ("XY")
      .box (W + 2.0, 220.0, H + 150.0 - STRIP_TOP, centered=(False, False, False))
      .translate ((-1.0, D, STRIP_TOP))))

# The recessed vent surface, inset a rim's width from the slope's edges --
# and NOT PARALLEL TO THE SLOPE: it sits an inch into the housing where it
# meets the bell and an inch and a half at the top, so it leans at its own
# angle. Built with a SECOND rotation composed onto the slope's: cutters
# stand against the vertical plane at the bell-junction depth, lean by the
# surface's own tilt, then ride the slope like everything else back here.
VENT_IN_TOP = 1.5 * 25.4
VENT_IN_BOT = 1.0 * 25.4
# THE POCKET ENDS AT THE TOP OF THE BELL -- its side walls included. The
# bell's top edge crosses the slope 94 up from the hinge, and the rim aims
# a millimeter and a half below that: enough overlap that the bell hides
# the rim inside its own width, small enough that BESIDE the bell the
# walls read as ending at the bell's top line. The fourteen-millimeter
# tuck this replaces hid the rim behind the bell and hung the walls ten
# visible millimeters below its top on either side.
#
# The aim is in SLOPE units, and PKT_Z0 is a CUTTER-frame coordinate --
# the frame is tilted about a line VENT_IN_BOT below the surface, so the
# rim lands VENT_IN_BOT * tan(VENT_TILT) higher than the coordinate says,
# and VENT_TILT itself depends on PKT_Z0. Two terms of fixed-point
# iteration settle it to a hundredth; the strap that haunted this rim came
# from doing this conversion by eye.
# WELL BELOW THE BELL'S TOP, because the recess has no floor of its own --
# THE BELL'S TOP IS ITS FLOOR. Aimed just under the bell's top edge, the
# pocket stopped a millimeter or two short of it and the case left in that
# gap read as a pale band across the bottom of the recess, with the bell
# starting below it. Dropped well under, the pocket's own bottom and the
# stub of wall beneath it are both inside the bell -- which is wider than
# the recess -- so what closes the recess is the bell's surface and nothing
# else.
_RIM_AIM = 94.0 - 26.0

_pkt = _RIM_AIM
for _ in range (4):
    _pkt = _RIM_AIM - VENT_IN_BOT * ((1.5 * 25.4 - VENT_IN_BOT) /
                                     ((SLOPE_LEN - REAR_RIM) - _pkt))

PKT_Z0      = STRIP_TOP + _pkt
PKT_Z1      = STRIP_TOP + SLOPE_LEN - REAR_RIM
VENT_TILT   = math.degrees (math.atan2 (VENT_IN_TOP - VENT_IN_BOT, PKT_Z1 - PKT_Z0))


def vent_face(wp):
    """The composed transform for everything on the vent surface: its own
    lean about the surface's bottom edge, then the slope's tilt."""
    return tilt_rear (wp.rotate ((0.0, D - VENT_IN_BOT, PKT_Z0),
                                 (1.0, D - VENT_IN_BOT, PKT_Z0), VENT_TILT))


# The cutter's BOTTOM corners are square. Rounded, each one leaves a wedge
# of case standing at the pocket's lower rim beside the bell -- a beige tab
# cutting across the dark just above it, the exact width of the corner
# radius. The top corners keep their rounds, which is where they read.
# A FUNCTION, because the apron below is trimmed by this same cutter --
# the rim line is then shared geometry, not two frames' approximations of
# it -- but shifted a millimeter UP-SLOPE, so the apron's end reaches past
# the rim and into the tub's bottom wall. Trimmed exactly at the rim, the
# apron's end face and the pocket's wall were coplanar, and a coplanar
# joint between separate solids renders as a black slit wherever the eye
# lines up with it. Overlap is what a one-piece molding actually is.
# ONE WIDTH FOR THE WHOLE REAR. Every opening back here -- the vent recess,
# the control panel -- is exactly this wide and starts at exactly this x, so
# the dark column's edges are ONE STRAIGHT LINE from the vents to the
# receptacle. The previous rear had three widths within six millimeters of
# each other (recess 216, molding 222, panel 215) and every pair of them
# drew a step where they met.
#
# The dark MOLDING runs DARK_BLEED wider on each side than the openings, so
# the material around an opening's walls is dark too -- but it is the same
# bleed above and below, so it moves the column's edge without bending it.
DARK_X0   = BELL_CX - COL_HW
DARK_W    = COL_HW * 2.0
# JUST ENOUGH TO OWN THE WALLS, and no more. The molding is the material
# around the recess, so it has to reach a little past the opening or the
# walls themselves would render in the case's beige. But every millimeter
# of that reach also surfaces on the case's OUTER face as a dark border
# around the recess -- plastic seen edge-on, which the real molding does
# not show there because it is all inside. Three millimeters drew a band
# you could measure; three tenths is under a pixel at any zoom the scene
# reaches, so the walls are dark and the outer face stays case-colored.
DARK_BLEED = 0.3

# THE BELL, built here so the molding can union it in below.
#
# The pocket is deliberately NOT cut by it. That was tried, to make the
# recess end on the bell's contour, and it is what produced the line across
# the bell: sparing the case where the bell sits leaves case material lying
# exactly along the bell's own surface, and two coincident surfaces render
# as a sliver of whichever wins the depth test -- visible in a diagnostic
# render as the case's color cutting straight across the bell. Cutting the
# pocket right through instead leaves that space empty for the bell to fill,
# so the bell's surface is the only one there.
# THE BELL IS COLUMN-WIDE WHERE IT EMERGES, not where its front wire is.
# The wire sits 46 mm inside the case; by the time the bell reaches the
# surface the taper has already narrowed it, so a wire cut to the column's
# width put the visible bell UNDER width -- and left a sliver of recess
# floor either side of it, which is the line that kept crossing the bell.
# The front wire is therefore the column plus the taper already spent
# (0.34 of it) at the emergence point solved from the slope and the bell's
# own top edge.
#
# The corner radius has to be smaller than the bleed, too: the radius eats
# the flat top the recess walls land on, and the bleed is all the margin
# there is.
BELL_TAPER   = 3.0
BELL_EMERGE  = 0.3404                 # solved: slope x bell-top crossing
BELL_HW      = COL_HW + DARK_BLEED + BELL_EMERGE * BELL_TAPER
BELL_R       = 0.2

_bell_solid = cq.Solid.makeLoft ([
    round_rect_wire (D - 80.0, BELL_CX - BELL_HW, BELL_CX + BELL_HW,
                     90.0, 180.0, BELL_R),
    round_rect_wire (D + BELL_BACK,
                     BELL_CX - (BELL_HW - BELL_TAPER), BELL_CX + (BELL_HW - BELL_TAPER),
                     95.0, 150.0, BELL_R),
])


def _pocket_cutter(zshift):
    return vent_face (
        cq.Workplane ("XY")
          .box (DARK_W, VENT_IN_BOT + 80.0, PKT_Z1 - PKT_Z0,
                centered=(False, False, False))
          .edges ("|Y").fillet (6.0)
          .union (cq.Workplane ("XY")
                    .box (DARK_W, VENT_IN_BOT + 80.0, 20.0,
                          centered=(False, False, False)))
          .translate ((DARK_X0, D - VENT_IN_BOT, PKT_Z0 + zshift)))


case = case.cut (_pocket_cutter (0.0))

# AND THE BELL'S OWN VOLUME COMES OUT OF THE CASE. The bell is a solid that
# passes through the case's rear wall, so unless the case gives up that
# space the two carry surfaces lying on each other -- and where two surfaces
# coincide the depth test picks per pixel, which draws as a hairline of the
# loser cutting across the winner. That is the line that ran across the bell,
# and a diagnostic render (case red, bell blue) showed it as exactly that: a
# red thread lying on blue.
#
# Sparing the case where the bell sits was tried first and makes it worse --
# it guarantees the coincidence rather than removing it. Cutting the bell
# out leaves that space empty for the bell to fill, so along every surface
# the bell shows, the bell is the only thing there.
case = case.cut (cq.Workplane (obj=_bell_solid))


# THE VENTS: one row of SIMPLE VERTICAL HOLES straight through the plastic,
# thirteen a side of the spec plate, ending just above where the bell
# starts -- the bell covers none of them.
VENT_SLOT_W = 2.0

# Tall enough to FILL the notch: the band clears the bell below it by a
# margin, and it now stands off the notch's top wall by the same margin
# instead of leaving a third of the recess blank.
VENT_SLOT_H = 38.0
# Eleven a side, not thirteen: the bank has to fit between the spec plate
# and the wall, and once both of those moved inward thirteen slots could
# only fit by running under the plate.
VENT_N      = 11
VENT_BAND_Z = STRIP_TOP + SLOPE_LEN - 62.0
# INSIDE THE RECESS, with room to spare. The bank's outer edge was 100 --
# fine when the recess wall stood at 108, a slit cut half into the wall
# once the wall came in to 94. Measured off the wall now, not left as an
# absolute that a later width change could silently invalidate.
VENT_IN_X0  = 53.0                    # bank inner edge, clear of the plate
VENT_IN_X1  = COL_HW - 9.0            # ...and outer, clear of the wall
VENT_PITCH  = (VENT_IN_X1 - VENT_IN_X0 - VENT_SLOT_W) / (VENT_N - 1)

_slots = []

for _side in (-1.0, 1.0):
    for _k in range (VENT_N):
        _x = BELL_CX + _side * (VENT_IN_X0 + _k * VENT_PITCH) - VENT_SLOT_W * 0.5

        _slots.append (cq.Workplane ("XY")
                         .box (VENT_SLOT_W, 30.0, VENT_SLOT_H,
                               centered=(False, False, False))
                         .translate ((_x, D - VENT_IN_BOT - 15.0, VENT_BAND_Z))
                         .val())

case = case.cut (vent_face (cq.Workplane (obj=cq.Compound.makeCompound (_slots))))

# WHAT THE SLOTS LOOK INTO. A vent cut through plastic shows whatever is
# behind it, and behind it here is more case -- so the slits read as beige
# scratches rather than as openings. A real louvre looks into an unlit
# cabinet, so a near-black panel sits a few millimeters back of the recess
# floor, wide enough to back both banks and nothing else.
m.add ("vent_back",
       vent_face (cq.Workplane ("XY")
                    .box (DARK_W - 6.0, 1.5, VENT_SLOT_H + 14.0,
                          centered=(False, False, False))
                    .translate ((DARK_X0 + 3.0, D - VENT_IN_BOT - 7.0,
                                 VENT_BAND_Z - 7.0))),
       CAVITY, angular=CORNER_ANG)

# ------------------------------------------------------- circumference line
#
# An engraved line rings the case in the X-Z plane, a quarter inch behind
# the power notch's rear wall -- the molding's parting line, the one mark
# the big smooth flanks carry. Cut as slab-minus-inner-prism so the groove
# follows the case's rounded corners instead of breaking at them.
RING_Y = NOTCH_D + 0.25 * 25.4
RING_W = 1.5
RING_D = 1.2

case = case.cut (
    cq.Workplane ("XY")
      .box (W + 20.0, RING_W, H + 20.0, centered=(False, False, False))
      .translate ((-10.0, RING_Y - RING_W * 0.5, -10.0))
      .cut (cq.Workplane (obj=cq.Solid.extrudeLinear (
                cq.Face.makeFromWires (round_rect_wire (
                    RING_Y - RING_W,
                    RING_D, W - RING_D, RING_D, H - RING_D,
                    max (0.5, 3.0 - RING_D))),
                cq.Vector (0.0, RING_W + 2.0, 0.0)))))

# ...and the reveal's divider does not stop at the top of the front face. It
# turns the corner and runs back across the roof until it meets the ring,
# which is what closes the reveal as a shape: a strip drawn off the front and
# over the top, rather than a line that dies at an edge. Same half-round
# channel, same radius, same axis in x -- only the direction it runs changes.
case = case.cut (
    cq.Workplane ("XY")
      .cylinder (RING_Y + 1.0, GROOVE_R, direct=(0, 1, 0), centered=(True, True, False))
      .translate ((DX + GROOVE_R, -1.0, H)))

# ------------------------------------------------------------ control panel
#
# The vertical strip under the slope carries the controls on a LIGHTER inset
# plate -- the real panel is its own molding, a shade off the case around it.
# The pocket is cut here; the plate itself is a separate part below, and the
# icons engrave into the plate rather than the case.
PANEL_W   = DARK_W                    # THE SAME WIDTH as the vent recess
PANEL_X0  = DARK_X0                   # ...and the same left edge
PANEL_Z0  = 16.0
PANEL_Z1  = STRIP_TOP                 # to the hinge, meeting the liner
PANEL_IN  = 2.0                       # the pocket
PANEL_SET = 0.3                       # the plate's face behind the case face

case = case.cut (
    cq.Workplane ("XY")
      .box (PANEL_W, PANEL_IN + 4.0, PANEL_Z1 - PANEL_Z0, centered=(False, False, False))
      .edges ("|Y").fillet (4.0)
      .translate ((PANEL_X0, D - PANEL_IN, PANEL_Z0)))

# Control positions, left to right AS READ FROM BEHIND: mains inlet, then
# three thumbwheels, then the RCA, an engraved icon over each of the four
# rightmost. A reader standing behind the monitor sees model +x on their
# LEFT, so the inlet takes the positive offset -- laid out the other way the
# whole row came out mirrored against the photographs.
AC_CX    = BELL_CX + 70.0
CTRL_CXS = (BELL_CX + 32.0, BELL_CX, BELL_CX - 32.0)
RCA_CX   = BELL_CX - 70.0
CTRL_CZ  = PANEL_Z0 + 20.0            # the controls' row
ICON_CZ  = PANEL_Z1 - 17.0            # the icons' row

# Each knob stands out of a counterbore whose interior goes BLACK -- the
# gap around a knob and the hole it protrudes through, cut into the case
# here while it is still being cut. The dark cup parts come later.
KNOB_BORE_R = 7.2
KNOB_BORE_D = 2.5

for _cx in CTRL_CXS:
    case = case.cut (
        cq.Workplane ("XY")
          .cylinder (KNOB_BORE_D + 1.5, KNOB_BORE_R, direct=(0, 1, 0), centered=(True, True, False))
          .translate ((_cx, D - KNOB_BORE_D, CTRL_CZ)))

# The RCA's counterbore, the Monitor //c's own.
case = case.cut (
    cq.Workplane ("XY")
      .cylinder (3.0, 6.6, direct=(0, 1, 0), centered=(True, True, False))
      .translate ((RCA_CX, D - 1.5, CTRL_CZ)))

# THE MAINS INLET IS THE MONITOR //c's RECEPTACLE, by direction: the same
# recessed rounded rectangle with its top corners clipped at forty-five
# degrees, cut eight millimeters into the strip.
AC_W, AC_H, AC_CLIP, AC_DEEP = 24.0, 17.0, 5.0, 8.0

_acx0, _acx1 = AC_CX - AC_W * 0.5, AC_CX + AC_W * 0.5
_acz0, _acz1 = CTRL_CZ - AC_H * 0.5, CTRL_CZ + AC_H * 0.5

_ac_face = cq.Face.makeFromWires (cq.Wire.makePolygon ([
    cq.Vector (_acx0, D + 0.5, _acz0),
    cq.Vector (_acx1, D + 0.5, _acz0),
    cq.Vector (_acx1, D + 0.5, _acz1 - AC_CLIP),
    cq.Vector (_acx1 - AC_CLIP, D + 0.5, _acz1),
    cq.Vector (_acx0 + AC_CLIP, D + 0.5, _acz1),
    cq.Vector (_acx0, D + 0.5, _acz1 - AC_CLIP),
    cq.Vector (_acx0, D + 0.5, _acz0),
]))

case = case.cut (cq.Workplane (obj=cq.Solid.extrudeLinear (
    _ac_face, cq.Vector (0.0, -(AC_DEEP + 0.5), 0.0))))

# ------------------------------------------------------------ the underside
#
# From the reference photographs of the bottom: two bands of vent slats
# across the middle, each slat running front to back, and six square feet in
# two rows of three. The feet are parts (they stand PROUD of the underside);
# the slats are cuts, gathered into one compound per band.
BOT_SLAT_W  = 2.5
BOT_PITCH   = 6.5
BOT_SLAT_L  = 26.0
BOT_BANDS_Y = (88.0, 128.0)
BOT_X0      = 45.0
BOT_X1      = W - 45.0

_bslats = []

for _by in BOT_BANDS_Y:
    _x = BOT_X0

    while _x + BOT_SLAT_W <= BOT_X1:
        _bslats.append (cq.Workplane ("XY")
                          .box (BOT_SLAT_W, BOT_SLAT_L, 4.0, centered=(False, False, False))
                          .translate ((_x, _by, -2.0))
                          .val())
        _x += BOT_PITCH

case = case.cut (cq.Workplane (obj=cq.Compound.makeCompound (_bslats)))

# ------------------------------------------------------ the contrast wheel
#
# A thumbwheel buried in the RIGHT flank, standing a couple of millimeters
# proud through a hole in the case, on the same center line as the contrast
# mark on the front -- the mark names the control, so the two share
# CONTRAST_CZ rather than each carrying its own copy of H * 0.5.
#
# The //c's wheel is the same idea with its axis turned: that one spins
# about the vertical and shows a wide, short sliver, while this one spins
# about the DEPTH axis and shows a tall, narrow one, which is what the
# photographs of this machine show. Turning the axis is also what lets the
# wheel be thicker without growing: the thickness runs front to back now,
# where the //c's ran top to bottom.
CONTRAST_CZ  = H * 0.5
WHEEL_R      = 18.0
WHEEL_T      = 12.0                   # far thicker than the //c's five
WHEEL_PROUD  = 2.2
WHEEL_CX     = W - WHEEL_R + WHEEL_PROUD

# A wheel this size no longer fits between the front face and the parting
# line: at WHEEL_R it wants 38 mm of depth and there are only 32 before the
# groove. So it sits far enough back that its opening never breaks the FRONT
# face -- a wheel poking out the front would be nonsense -- and the groove is
# simply interrupted where the opening crosses it, which is what a molding
# does when a hole lands on its parting line.
WHEEL_CY     = 23.0

# SMOOTH. It was knurled for a while, to break up a broad white specular
# band -- and that was a fix for the wrong fault: the wash was a stale .mtl
# leaving the mesh naming a material that did not exist, which falls back to
# white. With the material actually shipped, the barrel carries its tint on
# its own, and the ribs were left reading as coarse steps at this size.

# The opening is CONCENTRIC WITH THE WHEEL and a millimeter clear all round,
# the //c's lesson: the gap reads dark, the case shadows the rim across it,
# and the wheel is seen to come THROUGH the case rather than to lie on it.
case = case.cut (cq.Workplane ("XY")
                   .cylinder (WHEEL_T + 2.0, WHEEL_R + 1.0, direct=(0, 1, 0))
                   .translate ((WHEEL_CX, WHEEL_CY, CONTRAST_CZ)))

# Finer than the default, and note it is the ANGULAR tolerance doing the
# work: at 3 mm the chords never sag far enough for the linear one to bite,
# so the stock setting spent about three segments on a quarter turn and the
# corners read as facets meeting at an angle rather than as rounds.
# THE REAR MOLDING IS NOT CLADDING. Four generations of liner plates and
# aprons tried to dress the pocket and the slope in dark plastic, and every
# one of them met its neighbors at an edge it had to be aligned to: too
# narrow left beige threads, too wide drew a jog, coplanar rendered a black
# slit, overlapped hung a dark strap over the recess. A joint between
# separate solids is a seam by construction, and the molding has no seams
# because a real molding IS ONE PIECE.
#
# So the molding is made of the case itself: a region solid is intersected
# with the finished case -- pocket cuts, vent slots, everything -- and that
# intersection becomes the dark part while the case loses the same region.
# Its surfaces are the case's own surfaces, recolored; there is nothing to
# align, and no angle from which a joint can show, because there is no
# joint. The region's own boundary surfaces lie buried inside the material
# except where they cross the case's skin, and a crossing on a continuous
# surface is a clean color line -- the molding's real edge against the
# shell.
#
# The region: the pocket, inflated three millimeters so the intersection
# keeps a wall-following shell of material around it, unioned with a slab
# lying under the slope from the hinge up past the rim, column-wide.
_mold_x0 = DARK_X0 - DARK_BLEED
_mold_w  = DARK_W + DARK_BLEED * 2.0

_molding_region = (
    # the vent recess, with the bleed around its walls
    # ROUNDED LIKE THE POCKET IT SURROUNDS. A square-cornered region against
    # a six-millimeter-radius recess overhangs at each corner by the radius,
    # and that overhang surfaces as a grey wedge sitting past the round on
    # the case. The region carries the same radius plus its own bleed, so
    # its corner follows the recess's instead of cutting the corner off.
    vent_face (
        cq.Workplane ("XY")
          .box (_mold_w, VENT_IN_BOT + 83.0, PKT_Z1 - PKT_Z0 + DARK_BLEED * 2.0,
                centered=(False, False, False))
          .edges ("|Y").fillet (6.0 + DARK_BLEED)
          .translate ((_mold_x0, D - VENT_IN_BOT - 3.0, PKT_Z0 - DARK_BLEED)))
      # the slope between the recess and the hinge
      .union (tilt_rear (
          cq.Workplane ("XY")
            .box (_mold_w, 6.0, (PKT_Z0 - STRIP_TOP) + 14.0,
                  centered=(False, False, False))
            .translate ((_mold_x0, D - 5.8, STRIP_TOP - 2.0))))
      # ...and on down the vertical strip, over the control panel, so the
      # column is one dark run from the vents to the receptacle
      .union (
          cq.Workplane ("XY")
            .box (_mold_w, 34.0, (STRIP_TOP - PANEL_Z0) + 6.0,
                  centered=(False, False, False))
            .translate ((_mold_x0, D - 30.0, PANEL_Z0 - 3.0))))

# THE BELL IS UNIONED IN, not added beside. It is the same molding and the
# same color, but as a SEPARATE SOLID its surface met the recess floor at a
# boundary, and a boundary between two solids renders as a hairline however
# exactly they are aligned -- the line that kept crossing the bell, still
# there after the widths were made to agree, because agreement is not the
# same as being one piece. Unioned, there is no interior face left to draw.
m.add ("rear_molding",
       case.intersect (_molding_region).union (cq.Workplane (obj=_bell_solid)),
       PANEL_GRAY, angular=CORNER_ANG)

case = case.cut (_molding_region)

m.add("case", case, BEIGE, angular=CORNER_ANG)

# THE APRON: the dark molding does not stop at the hinge. From the control
# panel's top edge it continues up the SLOPE -- through the bell, which
# emerges out of it -- until it meets the recess, so the dark column reads
# as one piece from the vents to the receptacle. A thin plate lying on the
# sloped face, standing two tenths proud, in the same plastic as everything
# else back here. The bell interpenetrates it; both are one color, and
# interpenetrating solids in one scene cost nothing.
# The wheel itself, in the power button's warmer gray -- it is the same
# molding family as the button, not the case's beige...
m.add ("contrast_wheel",
       cq.Workplane ("XY")
         .cylinder (WHEEL_T, WHEEL_R, direct=(0, 1, 0))
         .translate ((WHEEL_CX, WHEEL_CY, CONTRAST_CZ)),
       BEZEL_DK, angular=0.05)

# ...and the opening wears a dark sleeve, so the clearance around the wheel
# reads as a deep cut on every side rather than as beige seen edge-on. It is
# clipped back of the outer wall, since a sleeve flush with the flank stands
# proud of it as a dark collar sitting on the case.
m.add ("wheel_moat",
       cq.Workplane ("XY")
         .cylinder (WHEEL_T + 1.9, WHEEL_R + 0.9, direct=(0, 1, 0))
         .translate ((WHEEL_CX, WHEEL_CY, CONTRAST_CZ))
         .cut (cq.Workplane ("XY")
                 .cylinder (WHEEL_T + 0.4, WHEEL_R + 0.35, direct=(0, 1, 0))
                 .translate ((WHEEL_CX, WHEEL_CY, CONTRAST_CZ)))
         .cut (cq.Workplane ("XY")
                 .box (40.0, 60.0, 60.0, centered=(False, True, True))
                 .translate ((W - 0.6, WHEEL_CY, CONTRAST_CZ))),
       DARK_PART, angular=0.05)

# --------------------------------------------------------------- rear parts

# THE BELL: the tube's rear housing, emerging through the recess and running
# back to the strip's plane. Lofted between two VERTICAL sections -- a wide
# tall one buried inside the case and the narrow low rear face -- so the top
# slopes down toward the back, the sides draw in, and the bottom stays
# nearly level, exactly the hopper the photographs show. IN THE BEZEL'S
# COLOR: the bell, the tilting bezel, and the power button are the same gray
# molding family on the real unit, and the case's beige is not in it.
BELL_REAR_Y = D + BELL_BACK           # WELL PAST the case: the reference
                                      # meshes show the bell overhanging the
                                      # control strip by a third of the
                                      # case's own depth

# On the tube's axis (BELL_CX, set with the column), edges barely broken --
# the real housing is an angular molding, not a pillow -- and the sides
# nearly PARALLEL: a few degrees of draw, not a funnel. The top keeps its
# strong slope; that is the bell's whole silhouette.
# The crown stops at 180: the vents are NEVER touched or hidden by the
# bell, and what hides them from a straight-on view is not where the bell
# meets the surface but the highest point of its whole silhouette -- the
# buried front section's top edge, which projects over the band however
# far back the actual emergence sits.
bell = _bell_solid

# (the bell is part of "rear_molding" above -- one solid, no seam)

# The cassowary on the bell's rear face, where the real unit wears its
# maker's sticker: read out of CassoBranding.cpp exactly as the Disk IIc's
# lid mark is, so the bird keeps its one definition.
def read_branding():
    import io, os, re

    src = io.open (os.path.join (os.path.dirname (os.path.abspath (__file__)),
                                 "..", "..", "Casso", "Ui", "Chrome", "CassoBranding.cpp"),
                   encoding="cp1252").read()

    rows_m    = re.search (r"s_kSilhouette\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
    rows      = [int (h, 16) for h in re.findall (r"0x([0-9A-Fa-f]+)ULL", rows_m.group (1))]
    stripes_m = re.search (r"s_kStripeColors\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
    stripes   = [tuple (int (h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))
                 for h in re.findall (r"0x[Ff]{2}([0-9A-Fa-f]{6})", stripes_m.group (1))]

    return rows, stripes


BELL_BIRD_H  = 22.0
BELL_BIRD_CZ = 122.0

_rows, _stripes = read_branding()
_cell     = BELL_BIRD_H / len (_rows)
_nonzero  = [i for i, r in enumerate (_rows) if r]
_first    = _nonzero[0]
_cols     = [c for bits in _rows for c in range (64) if (bits >> c) & 1]
_collo    = min (_cols)
_colhi    = max (_cols)
_bird_x0  = BELL_CX - ((_collo + _colhi + 1) * 0.5) * _cell
_bird_z1  = BELL_BIRD_CZ + BELL_BIRD_H * 0.5 + _first * _cell
_by_stripe = {}

for _row, _bits in enumerate (_rows):
    if not _bits:
        continue

    _banded = min (_nonzero[-1], max (_first, _row))
    _stripe = ((_banded - _first) * len (_stripes)) // (_nonzero[-1] - _first + 1)
    _rz0    = _bird_z1 - (_row + 1) * _cell
    _col    = 0

    while _col < 64:
        if not (_bits >> _col) & 1:
            _col += 1
            continue

        _run = _col
        while _run < 64 and (_bits >> _run) & 1:
            _run += 1

        _by_stripe.setdefault (_stripe, []).append (
            cq.Workplane ("XY")
              .box ((_run - _col) * _cell, 0.35, _cell + 0.02,
                    centered=(False, False, False))
              .translate ((_bird_x0 + _col * _cell, BELL_REAR_Y - 0.2, _rz0))
              .val())
        _col = _run

for _stripe, _solids in sorted (_by_stripe.items()):
    m.add (f"bellbird{_stripe}", cq.Compound.makeCompound (_solids), _stripes[_stripe])

# The spec plate: a blank rounded plate over the middle of the vent band,
# where the real unit rivets its ratings label. Blank on purpose -- the
# scene's branding is the cassowary, not a wall of small print.
m.add ("rear_label",
       vent_face (cq.Workplane ("XY")
                    .box (94.0, 1.4, 30.0, centered=(False, False, False))
                    .edges ("|Y").fillet (2.0)
                    # Centered on the vent band: the slots run 62 to 24
                    # below the slope's top, so a 30-tall plate shares their
                    # center at 58.
                    .translate ((BELL_CX - 47.0, D - VENT_IN_BOT + 1.2,
                                 STRIP_TOP + SLOPE_LEN - 58.0))),
       LABEL_GRAY, angular=CORNER_ANG)

# ------------------------------------------------- the embossed control row
#
# EMBOSSED, NOT ENGRAVED. The Monitor II speaks in mold relief -- its power
# icon, its brightness mark, its tilt arrows all stand proud of the case --
# and the rear panel speaks the same language: REAR_RIDGE strokes standing
# RIDGE_H off the plate, front edges rounded over exactly as the front's
# marks are. The //c engraves; this machine does not.
ICON_S  = 13.0
BOX_R   = 0.9

# Thinner than the front's relief: the rear marks are finer work on the
# real panel, and the front's full millimeter read as piping back here.
REAR_RIDGE = 0.6

PANEL_FACE_Y = D - PANEL_SET          # the plate's face the relief stands on

# The plate ends AT the hinge. It used to run 11 mm past it to bridge a
# beige seam, back when the pocket above was an open cavern -- but the
# pocket ends at the bell's top now, so the case above the hinge is solid
# slope, and a plate standing past the corner would stand proud of it.
panel = (cq.Workplane ("XY")
         .box (PANEL_W - 1.0, PANEL_IN - PANEL_SET, PANEL_Z1 - PANEL_Z0 - 1.0,
               centered=(False, False, False))
         .edges ("|Y").fillet (3.6)
         .translate ((PANEL_X0 + 0.5, D - PANEL_IN, PANEL_Z0 + 0.5)))


# ...and FLUSH where it meets the hinge. PANEL_SET holds the plate a third
# of a millimeter behind the case face, which is the recessed look the
# panel wants everywhere the icons are -- but at the hinge that setback
# leaves the case's own beige edge standing proud of the dark, and THAT
# pale line is what still read as a break in the one-piece rear molding.
# The top of the plate comes out to the case face, so the edge has nothing
# left to catch.
panel = panel.union (cq.Workplane ("XY")
                       .box (PANEL_W - 1.0, PANEL_IN, 8.0,
                             centered=(False, False, False))
                       .translate ((PANEL_X0 + 0.5, D - PANEL_IN, PANEL_Z1 - 8.0)))

# The plate opens for the knobs, the RCA, and the receptacle -- the case
# behind carries the counterbores; the plate just clears them.
for _cx in CTRL_CXS:
    panel = panel.cut (
        cq.Workplane ("XY")
          .cylinder (12.0, KNOB_BORE_R, direct=(0, 1, 0), centered=(True, True, False))
          .translate ((_cx, D - 11.0, CTRL_CZ)))

panel = panel.cut (
    cq.Workplane ("XY")
      .cylinder (12.0, 6.6, direct=(0, 1, 0), centered=(True, True, False))
      .translate ((RCA_CX, D - 11.0, CTRL_CZ)))
panel = panel.cut (cq.Workplane (obj=cq.Solid.extrudeLinear (
    _ac_face, cq.Vector (0.0, -(AC_DEEP + 0.5), 0.0))))

m.add ("ctrl_panel", panel, PANEL_GRAY, angular=CORNER_ANG)


# The rear's strokes are thinner than the front's, so they get their own
# round-over: RELIEF_ROUND is sized against RIDGE_W and is too generous for
# a REAR_RIDGE stroke.
REAR_ROUND = min (0.18, REAR_RIDGE * 0.45, RIDGE_H * 0.4)


def _relief(solid):
    """Round a relief's outward-facing edges so the strokes CATCH LIGHT --
    a sharp-topped ridge takes the same shade as the plate it stands on and
    the glyph disappears into the panel.

    One stroke at a time, not the whole glyph at once: a single fillet
    across a compound fails as a unit, so the kernel refusing one arc used
    to leave an entire icon sharp. Per-stroke, a refusal costs only that
    stroke."""
    pieces = []

    for _s in solid.val().Solids():
        try:
            pieces.append (cq.Workplane (obj=_s).edges (">Y").fillet (REAR_ROUND).val())
        except Exception:
            print ("WARNING: rear relief: round-over FAILED on one stroke, keeping it sharp")
            pieces.append (_s)

    return cq.Workplane (obj=cq.Compound.makeCompound (pieces))


def ridge_box(cx, cz, w, h, rot_deg=0.0):
    """A relief stroke standing RIDGE_H proud of the plate, optionally
    rotated about its own y-axis center."""
    s = (cq.Workplane ("XY")
         .box (w, RIDGE_H, h, centered=(True, False, True))
         .translate ((0.0, PANEL_FACE_Y, 0.0)))

    if rot_deg != 0.0:
        s = s.rotate ((0, 0, 0), (0, 1, 0), rot_deg)

    return s.translate ((cx, 0.0, cz))


def ridge_frame(cx, cz, w, h, r):
    """A rounded-rectangle relief OUTLINE: outer minus inner."""
    outer = cq.Solid.extrudeLinear (
        cq.Face.makeFromWires (round_rect_wire (PANEL_FACE_Y,
                                                cx - w * 0.5, cx + w * 0.5,
                                                cz - h * 0.5, cz + h * 0.5, r)),
        cq.Vector (0.0, RIDGE_H, 0.0))
    inner = cq.Solid.extrudeLinear (
        cq.Face.makeFromWires (round_rect_wire (PANEL_FACE_Y - 0.1,
                                                cx - w * 0.5 + REAR_RIDGE, cx + w * 0.5 - REAR_RIDGE,
                                                cz - h * 0.5 + REAR_RIDGE, cz + h * 0.5 - REAR_RIDGE,
                                                max (0.4, r - REAR_RIDGE))),
        cq.Vector (0.0, RIDGE_H + 0.2, 0.0))

    return cq.Workplane (obj=outer).cut (cq.Workplane (obj=inner))


def ridge_ring(cx, cz, r):
    """A circular relief outline: an annulus one ridge wide."""
    outer = (cq.Workplane ("XY")
               .cylinder (RIDGE_H, r, direct=(0, 1, 0), centered=(True, True, False))
               .translate ((cx, PANEL_FACE_Y, cz)))
    inner = (cq.Workplane ("XY")
               .cylinder (RIDGE_H + 0.2, r - REAR_RIDGE, direct=(0, 1, 0), centered=(True, True, False))
               .translate ((cx, PANEL_FACE_Y - 0.1, cz)))

    return outer.cut (inner)


def ridge_tri(cx, cz, w, h, rot_deg):
    """A solid triangular relief, apex toward rot_deg (0 = up)."""
    s = cq.Workplane (obj=cq.Solid.extrudeLinear (
        cq.Face.makeFromWires (cq.Wire.makePolygon ([
            cq.Vector (-w * 0.5, PANEL_FACE_Y, -h * 0.5),
            cq.Vector (w * 0.5, PANEL_FACE_Y, -h * 0.5),
            cq.Vector (0.0, PANEL_FACE_Y, h * 0.5),
            cq.Vector (-w * 0.5, PANEL_FACE_Y, -h * 0.5),
        ])),
        cq.Vector (0.0, RIDGE_H, 0.0)))

    if rot_deg != 0.0:
        s = s.rotate ((0, 0, 0), (0, 1, 0), rot_deg)

    return s.translate ((cx, 0.0, cz))


CRT_W = ICON_S - 3.4
CRT_H = (ICON_S - 3.4) * 0.72
# A tube's face BULGES: the screens bow OUTWARD, top, bottom, and sides.
# Bowed inward they read as an hourglass, which is the opposite of a
# picture tube and the opposite of the panel photograph.
CRT_BOW   = 0.14                      # top and bottom, as a share of height
CRT_BOW_X = 0.04                      # the sides, as a share of width


def bowed_screen_wire(y, x0, x1, z0, z1):
    """The closed outline of a picture tube's face in the plane at `y`: four
    arcs bulging outward, meeting at the corners.

    Built from three-point arcs rather than by biting cylinders out of a
    rectangle. The cylinder route is the obvious one and it is wrong here
    for the reason the //c work already recorded: an X-axis cylinder placed
    with `centered` flags lands somewhere other than where the arithmetic
    says, and the glyph comes out with one side eaten away."""
    cx   = (x0 + x1) * 0.5
    cz   = (z0 + z1) * 0.5
    bowZ = (z1 - z0) * CRT_BOW
    bowX = (x1 - x0) * CRT_BOW_X
    tl   = cq.Vector (x0, y, z1)
    tr   = cq.Vector (x1, y, z1)
    br   = cq.Vector (x1, y, z0)
    bl   = cq.Vector (x0, y, z0)

    return cq.Wire.assembleEdges ([
        cq.Edge.makeThreePointArc (tl, cq.Vector (cx, y, z1 + bowZ), tr),
        cq.Edge.makeThreePointArc (tr, cq.Vector (x1 + bowX, y, cz), br),
        cq.Edge.makeThreePointArc (br, cq.Vector (cx, y, z0 - bowZ), bl),
        cq.Edge.makeThreePointArc (bl, cq.Vector (x0 - bowX, y, cz), tl),
    ])


def crt_outline(cx, cz, w, h):
    """A CRT screen OUTLINE, one stroke wide. Every screen on this panel is
    drawn the same way, so the shape is one helper rather than four
    hand-built variants."""
    outer = cq.Workplane (obj=cq.Solid.extrudeLinear (
        cq.Face.makeFromWires (bowed_screen_wire (PANEL_FACE_Y,
                                                  cx - w * 0.5, cx + w * 0.5,
                                                  cz - h * 0.5, cz + h * 0.5)),
        cq.Vector (0.0, RIDGE_H, 0.0)))
    inner = cq.Workplane (obj=cq.Solid.extrudeLinear (
        cq.Face.makeFromWires (bowed_screen_wire (PANEL_FACE_Y - 0.1,
                                                  cx - w * 0.5 + REAR_RIDGE,
                                                  cx + w * 0.5 - REAR_RIDGE,
                                                  cz - h * 0.5 + REAR_RIDGE,
                                                  cz + h * 0.5 - REAR_RIDGE)),
        cq.Vector (0.0, RIDGE_H + 0.2, 0.0)))

    return outer.cut (inner)


def crt_inner_z(cz, h):
    """Where an arrowhead has to stop to TOUCH a bowed screen's wall. The
    bulge pushes the top and bottom edges OUT at their centers, which is
    exactly where a centered arrow meets them, so the flat rectangle's
    half-height is short by one bow."""
    reach = h * 0.5 + h * CRT_BOW - REAR_RIDGE

    return (cz + reach, cz - reach)


# The glyphs, read off the real panel left to right. VERTICAL HOLD is the
# picture rolling out of frame: one screen with a second, slightly smaller
# screen riding a quarter of a screen-height above it.
#
# The two are separate strokes that OVERLAP rather than one cut out of the
# other, so the larger screen's top arc carries on THROUGH the smaller's
# opening -- which is the whole reading of the glyph: a picture caught mid
# roll, not two screens stacked.
_vh_off = CRT_H * 0.10
_vh_up  = CRT_H * 0.15

vhold = cq.Workplane (obj=cq.Compound.makeCompound ([
    ridge_frame (CTRL_CXS[0], ICON_CZ, ICON_S, ICON_S, BOX_R).val(),
    crt_outline (CTRL_CXS[0], ICON_CZ - _vh_off, CRT_W, CRT_H).val(),
    crt_outline (CTRL_CXS[0], ICON_CZ + _vh_up, CRT_W * 0.86, CRT_H * 0.86).val(),
]))

m.add ("icon_vhold", _relief (vhold), PANEL_GRAY, angular=CORNER_ANG)

# VERTICAL SIZE: the screen with a DOUBLE-headed arrow standing in it, one
# head up and one down on a single stem, each head just touching the wall
# it points at.
_vs_top, _vs_bot = crt_inner_z (ICON_CZ, CRT_H)
_vs_head         = 2.0

vsize = cq.Workplane (obj=cq.Compound.makeCompound ([
    ridge_frame (CTRL_CXS[1], ICON_CZ, ICON_S, ICON_S, BOX_R).val(),
    crt_outline (CTRL_CXS[1], ICON_CZ, CRT_W, CRT_H).val(),
    ridge_box (CTRL_CXS[1], ICON_CZ, REAR_RIDGE,
               (_vs_top - _vs_head) - (_vs_bot + _vs_head)).val(),
    ridge_tri (CTRL_CXS[1], _vs_top - _vs_head * 0.5, 2.8, _vs_head, 0.0).val(),
    ridge_tri (CTRL_CXS[1], _vs_bot + _vs_head * 0.5, 2.8, _vs_head, 180.0).val(),
]))

m.add ("icon_vsize", _relief (vsize), PANEL_GRAY, angular=CORNER_ANG)

# BRIGHTNESS: the circle with eight short rays that touch nothing. The
# rotation lesson still applies: about +y, pointing a ray outward takes 90
# MINUS its bearing.
_sun = [ridge_frame (CTRL_CXS[2], ICON_CZ, ICON_S, ICON_S, BOX_R).val(),
        ridge_ring (CTRL_CXS[2], ICON_CZ, 2.2).val()]

for _k in range (8):
    _ang = _k * 45.0
    _rx  = CTRL_CXS[2] + 4.0 * math.cos (math.radians (_ang))
    _rz  = ICON_CZ + 4.0 * math.sin (math.radians (_ang))

    _sun.append (ridge_box (_rx, _rz, REAR_RIDGE, 1.6, 90.0 - _ang).val())

m.add ("icon_sun", _relief (cq.Workplane (obj=cq.Compound.makeCompound (_sun))),
       PANEL_GRAY, angular=CORNER_ANG)

# VIDEO IN: the screen -- CENTERED in its border, not shouldered aside --
# with an arrow arriving through one wall and flying to the READER'S
# RIGHT. The reader stands behind the monitor, so their right is model
# MINUS x: the head rotates -90 about +y, and that sign is the whole
# difference between an input mark and an output one.
_vi_w = CRT_W - 1.4

video = cq.Workplane (obj=cq.Compound.makeCompound ([
    ridge_frame (RCA_CX, ICON_CZ, ICON_S, ICON_S, BOX_R).val(),
    crt_outline (RCA_CX, ICON_CZ, _vi_w, CRT_H).val(),
    ridge_box (RCA_CX + 3.4, ICON_CZ, 5.2, REAR_RIDGE).val(),
    ridge_tri (RCA_CX - 0.2, ICON_CZ, 2.0, 2.4, -90.0).val(),
]))

m.add ("icon_video", _relief (video), PANEL_GRAY, angular=CORNER_ANG)

# ------------------------------------------------------- knobs, jack, mains

# THE KNOBS: knurled dark cylinders standing five millimeters proud of the
# plate, each rising out of its black-lined counterbore. The knurl is
# sixteen ribs riding the rim, compounded with the barrel rather than
# unioned onto it -- interpenetrating solids in one part tessellate fine
# and cost nothing.
KNOB_R     = 5.0
KNOB_PROUD = 5.0
KNOB_BLACK = (0.030, 0.030, 0.030)
KNOB_GRAY  = (0.240, 0.240, 0.250)

for _i, _cx in enumerate (CTRL_CXS):
    # The black interior of the hole the knob protrudes through.
    m.add (f"knob_cup{_i}",
           cq.Workplane ("XY")
             .cylinder (KNOB_BORE_D - 0.15, KNOB_BORE_R - 0.05, direct=(0, 1, 0),
                        centered=(True, True, False))
             .translate ((_cx, D - KNOB_BORE_D + 0.05, CTRL_CZ)),
           KNOB_BLACK, angular=0.05)

    _ribs = [cq.Workplane ("XY")
               .cylinder (KNOB_BORE_D + KNOB_PROUD, KNOB_R, direct=(0, 1, 0),
                          centered=(True, True, False))
               .translate ((_cx, D - KNOB_BORE_D, CTRL_CZ))
               .val()]

    for _k in range (16):
        _ribs.append (cq.Workplane ("XY")
                        .box (0.8, KNOB_BORE_D + KNOB_PROUD - 0.6, 0.6,
                              centered=(True, False, True))
                        .translate ((0.0, 0.0, KNOB_R))
                        .rotate ((0, 0, 0), (0, 1, 0), _k * 22.5)
                        .translate ((_cx, D - KNOB_BORE_D, CTRL_CZ))
                        .val())

    m.add (f"knob{_i}", cq.Compound.makeCompound (_ribs), KNOB_GRAY, angular=0.05)

# THE RCA, the Monitor //c's whole recipe: counterbore already cut, a dark
# cup lining it, then barrel, insulator ring, and bore.
m.add ("rca_cup",
       cq.Workplane ("XY")
         .cylinder (1.35, 6.55, direct=(0, 1, 0), centered=(True, True, False))
         .translate ((RCA_CX, D - 1.45, CTRL_CZ)),
       (0.200, 0.200, 0.220), angular=0.05)
m.add ("rca_body",
       cq.Workplane ("XY")
         .cylinder (4.5, 4.6, direct=(0, 1, 0), centered=(True, True, False))
         .translate ((RCA_CX, D - 1.0, CTRL_CZ)),
       DARK_PART, angular=0.05)
m.add ("rca_ring",
       cq.Workplane ("XY")
         .cylinder (5.1, 3.0, direct=(0, 1, 0), centered=(True, True, False))
         .translate ((RCA_CX, D - 1.0, CTRL_CZ)),
       RCA_RING, angular=0.05)
m.add ("rca_bore",
       cq.Workplane ("XY")
         .cylinder (5.5, 1.3, direct=(0, 1, 0), centered=(True, True, False))
         .translate ((RCA_CX, D - 1.0, CTRL_CZ)),
       DARK_PART, angular=0.05)

# THE MAINS RECEPTACLE, the //c's socket brought over whole: the dark
# open-fronted liner in the clipped-corner cavity, and the three plated
# blades, LOW - HIGH - LOW.
AC_SILVER = (0.760, 0.765, 0.780)
AC_SOCKET = (0.200, 0.200, 0.220)


def ac_profile(inset, y):
    """The receptacle profile drawn `inset` inside the cavity, at depth y."""
    return cq.Wire.makePolygon ([
        cq.Vector (_acx0 + inset, y, _acz0 + inset),
        cq.Vector (_acx1 - inset, y, _acz0 + inset),
        cq.Vector (_acx1 - inset, y, _acz1 - AC_CLIP - inset * 0.41),
        cq.Vector (_acx1 - AC_CLIP - inset * 0.41, y, _acz1 - inset),
        cq.Vector (_acx0 + AC_CLIP + inset * 0.41, y, _acz1 - inset),
        cq.Vector (_acx0 + inset, y, _acz1 - AC_CLIP - inset * 0.41),
        cq.Vector (_acx0 + inset, y, _acz0 + inset),
    ])


m.add ("ac_liner",
       cq.Workplane (obj=cq.Solid.extrudeLinear (
           cq.Face.makeFromWires (ac_profile (0.25, D - 0.2)),
           cq.Vector (0.0, -(AC_DEEP - 0.4), 0.0)))
         .cut (cq.Workplane (obj=cq.Solid.extrudeLinear (
             cq.Face.makeFromWires (ac_profile (1.2, D + 0.3)),
             cq.Vector (0.0, -(AC_DEEP - 1.4), 0.0)))),
       AC_SOCKET)

for _dx, _dz in ((-6.0, -2.5), (0.0, 3.0), (6.0, -2.5)):
    m.add (f"acpin{int (_dx)}",
           cq.Workplane ("XY")
             .box (2.0, 5.0, 4.5, centered=(True, False, False))
             .translate ((AC_CX + _dx, D - AC_DEEP, CTRL_CZ + _dz - 2.25)),
           AC_SILVER)

# THE FEET: six square pads in two rows of three, standing proud of the
# underside -- they are what the monitor actually rests on.
FOOT_S   = 22.0
FOOT_H   = 2.8
FOOT_RGB = (0.100, 0.100, 0.100)

for _fi, (_fx, _fy) in enumerate (((40.0, 45.0), (W * 0.5, 45.0), (W - 40.0, 45.0),
                                   (40.0, D - 45.0), (W * 0.5, D - 45.0), (W - 40.0, D - 45.0))):
    m.add (f"foot{_fi}",
           cq.Workplane ("XY")
             .box (FOOT_S, FOOT_S, FOOT_H + 1.0, centered=(True, True, False))
             .edges ("|Z").fillet (2.5)
             .translate ((_fx, _fy, -FOOT_H)),
           FOOT_RGB)

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

BEZEL_FRONT_R = 1.2

bezel = (cq.Workplane("XY")
         .box(BX1 - BX0, PROTRUDE + CAVITY_D * 0.5, BZ1 - BZ0, centered=(False, False, False))
         .translate((BX0, -PROTRUDE, BZ0))
         .edges("|Y").fillet(BEZEL_FILLET)
         .edges("<Y").fillet(BEZEL_FRONT_R))

# Rounded loft sections, so the funnel's own corners -- the ones running
# front-to-back from the band down to the tube -- come out radiused without
# having to select four diagonal edges after the fact.
#
# The FRONT section's radius is the mouth's plus the distance the funnel
# travels inward, which is what makes the two sections a true constant-width
# offset of each other. Given both the same radius, the funnel measures its
# intended 11 mm across the straight runs but 15.6 mm across the corner
# diagonal -- 41% wider, so the corner rakes back at a visibly shallower angle
# than the sides do. That shows up as a hard step in the shading where the
# corner meets the straight run, reading as a dark band cutting across the
# bezel.
#
# The MOUTH is not GX0..GX1 either. The tube's rim sits GLASS_INSET inside
# that, and a mouth cut level with GX0 therefore left an annular gap between
# the two -- through which the case's near-black cavity lining showed as a
# dark band lying over the funnel's inner edge, right where the bezel should
# have met the glass. So the mouth comes in past the rim by MOUTH_LAP and the
# bezel laps OVER it, the way a real bezel holds a tube.
GLASS_INSET  = 1.0
MOUTH_LAP    = 0.5

MX0, MX1     = GX0 + GLASS_INSET + MOUTH_LAP, GX1 - GLASS_INSET - MOUTH_LAP
MZ0, MZ1     = GZ0 + GLASS_INSET + MOUTH_LAP, GZ1 - GLASS_INSET - MOUTH_LAP
MOUTH_RR     = MOUTH_R - GLASS_INSET - MOUTH_LAP

FUNNEL_FRONT_R = MOUTH_RR + FUNNEL_RUN

# HOW DEEP THE TUBE IS SEATED. Its rim was left on the mouth's own plane,
# which put the sheet's CROWN -- a full sag ahead of the rim -- out past the
# front of the bezel that is supposed to contain it. Seen from the side the
# tube appeared to burst through the bezel's face. Seated back by this much,
# the crown sits behind the bezel's front face.
#
# It lives up here because the funnel below is cut to REACH it.
GLASS_SET = 4.5

# THE FUNNEL DOES NOT STOP AT THE MOUTH. It carries on at the same rake until
# it is inside the glass, so the bezel and the tube TOUCH.
#
# Stopping at the mouth and boring straight back from there left an open ring
# between the two: the rim is seated GLASS_SET back, the bezel lapped it by
# MOUTH_LAP, and a lap of half a millimeter over a seat of four and a half
# stops hiding that ring at atan(0.5 / 4.5) -- SIX DEGREES off normal. Past
# that the scene can be spun to look down the bore, and the bore is cut from
# the bezel, so what showed was CASE-COLORED PLASTIC around the picture: from
# above it filled the top of the opening, from below the bottom.
#
# Widening the lap only moves that threshold; carrying the funnel down onto
# the glass removes the ring itself, and there is then no angle that can see
# between them. The two solids simply interpenetrate -- no boolean is wanted
# or needed, the depth buffer resolves it -- so the run is taken past contact
# rather than exactly to it.
#
# STEEPER THAN THE FUNNEL IT CONTINUES, which is the whole trick. Run on at
# the visible 60 degrees it reaches the glass 2.6mm inboard of the mouth, and
# the picture band leaves only 4.5mm of clearance at the top and bottom edges
# -- so the bezel arrived within 1.4mm of the raster, swallowed most of the
# dark tube margin, and at sixty degrees of yaw the two surfaces went nearly
# tangent to the view ray and z-fought into a ragged beige seam along the
# picture. That was a worse fault than the ring it fixed.
#
# Nothing requires the hidden run to match the visible rake: it lives behind
# the mouth lip and is only ever seen edge-on. At 80 degrees it still lands
# inside the glass -- it reaches GLASS_SET deep 0.79mm in, where the sagging
# rim has only risen 0.24mm to meet it -- and gives back 1.8mm of margin.
CONTACT_RAKE_DEG = 80.0

FUNNEL_BITE = GLASS_SET / math.tan(math.radians(CONTACT_RAKE_DEG))

CX0, CX1    = MX0 + FUNNEL_BITE, MX1 - FUNNEL_BITE
CZ0, CZ1    = MZ0 + FUNNEL_BITE, MZ1 - FUNNEL_BITE
CONTACT_RR  = max(MOUTH_RR - FUNNEL_BITE, 0.2)
CONTACT_Y   = -PROTRUDE + TUBE_DROP + GLASS_SET

funnel = cq.Solid.makeLoft([
    round_rect_wire(-PROTRUDE,             BAND_X0, BAND_X1, BAND_Z0, BAND_Z1, FUNNEL_FRONT_R),
    round_rect_wire(-PROTRUDE + TUBE_DROP, MX0,     MX1,     MZ0,     MZ1,     MOUTH_RR),
    round_rect_wire(CONTACT_Y,             CX0,     CX1,     CZ0,     CZ1,     CONTACT_RR),
])

bezel = bezel.cut(cq.Workplane(obj=funnel))

# The tunnel behind the mouth carries the SAME rounded profile. Cut square,
# its corners stood proud of the funnel's rounded ones and left a wedge of
# bezel hanging into each corner of the opening. It starts where the funnel
# ends, not at the mouth, or it would hollow out the reach the funnel just
# made.
tunnel = cq.Solid.makeLoft([
    round_rect_wire(CONTACT_Y,             CX0, CX1, CZ0, CZ1, CONTACT_RR),
    round_rect_wire(CONTACT_Y + CAVITY_D,  CX0, CX1, CZ0, CZ1, CONTACT_RR),
])

bezel = bezel.cut(cq.Workplane(obj=tunnel))

m.add("bezel", bezel, BEZEL, angular=CORNER_ANG)

# --------------------------------------------------------------------- tube

# The sheet the live display maps onto, filling the funnel's inner mouth: a
# section of a sphere, its RIM on the mouth's plane and its center bulging
# forward from there toward the band. sag_sheet wants the radius in half
# diagonals of the SHEET, so convert the faceplate's physical radius here
# rather than carrying a second, hand-tuned number.
GLASS_HALF_DIAG = math.hypot((GX1 - GX0 - GLASS_INSET * 2.0) * 0.5,
                             (GZ1 - GZ0 - GLASS_INSET * 2.0) * 0.5)

m.add_triangles("glass",
                sag_sheet(GX0 + GLASS_INSET, GX1 - GLASS_INSET,
                          GZ0 + GLASS_INSET, GZ1 - GLASS_INSET,
                          front_y=-PROTRUDE + TUBE_DROP + GLASS_SET,
                          radius_scale=FACE_R / GLASS_HALF_DIAG),
                KD["glass"])

# ------------------------------------------------------------- tube skirt

# THE FACEPLATE DOES NOT STOP AT THE PICTURE. A real tube's glass runs on
# past the bezel and dies inside the cabinet; ours stopped a hair outside the
# mouth, and a surface that stops has an edge you can see past.
#
# That edge is the fault. The rim is seated GLASS_SET back at the corners but
# bulges FORWARD of the mouth everywhere else -- 9.67mm proud at the top and
# bottom midpoints -- so along those edges it floats in the middle of the
# funnel opening with nothing sealing it. From a steep angle the line of
# sight goes over the rim and straight into the monitor's interior, above the
# top edge of the picture and below the bottom edge of it. No lap on the
# BEZEL can close that, because the thing with the hole in it is the tube.
#
# So the sheet is continued outward to the funnel's front opening, where it
# is buried in bezel however you look at it. It is a SEPARATE PART on
# purpose: the scene derives its display sphere and its picture band from the
# bounding box of the part named "glass", so growing that part would grow the
# raster with it and push the picture under the bezel.
#
# Same sphere, not merely a similar one. sag_sheet measures front_y at the
# CORNERS and bulges forward from there, so a wider sheet on one sphere needs
# its corners set back by the difference of the two sags -- otherwise it is a
# different, deeper dome that would burst through the glass it hides behind.
SKIRT_X0, SKIRT_X1 = BAND_X0, BAND_X1
SKIRT_Z0, SKIRT_Z1 = BAND_Z0, BAND_Z1
SKIRT_BURY         = 0.3          # behind the glass, so the two never fight

SKIRT_HALF_DIAG = math.hypot((SKIRT_X1 - SKIRT_X0) * 0.5,
                             (SKIRT_Z1 - SKIRT_Z0) * 0.5)


def _sag(rr):
    return FACE_R - math.sqrt(max(FACE_R ** 2 - rr * rr, 0.0))


SKIRT_FRONT_Y = (-PROTRUDE + TUBE_DROP + GLASS_SET
                 + _sag(SKIRT_HALF_DIAG) - _sag(GLASS_HALF_DIAG)
                 + SKIRT_BURY)

m.add_triangles("tube_skirt",
                sag_sheet(SKIRT_X0, SKIRT_X1, SKIRT_Z0, SKIRT_Z1,
                          front_y=SKIRT_FRONT_Y,
                          radius_scale=FACE_R / SKIRT_HALF_DIAG),
                CAVITY)

# ------------------------------------------------------- power button + LED

# The button, locked down: it fills the lower part of the notch and stands
# proud of the notch floor, not of the case.
#
# The WHOLE TOP RIM is rounded -- front and rear edges as well as the two
# sides. The bottom of the button travels down into the frame when it is
# pushed, so that end is a sliding fit inside the notch and never presents a
# molded edge to round; rounding it read as a free-standing tab rather than
# something that disappears into the case. Everything above the frame line,
# though, is molded surface, and a molded outside corner in plastic is the
# exception rather than the rule.
#
# AND THE BOTTOM IS BURIED, not floated. It used to start a millimeter above
# the notch floor, which is a real slot under a real button: the power LED
# sits higher up the rear wall, and the shallow rays that clear the button's
# bottom edge came out through that slot and lit a strip of floor in front of
# it. The button correctly shadowed everything else, so the light read as
# leaking out from under the button -- which is exactly what it was doing.
# Seating the bottom below the floor closes the slot; a sliding fit has
# nowhere for light to get through.
BTN_BURY = 2.0

button = (cq.Workplane("XY")
          .box(NOTCH_W - 3.0, BTN_D, NOTCH_H - 8.5 + 1.0 + BTN_BURY,
               centered=(False, False, False))
          .translate((NX0 + 1.5, BTN_REAR_Y - BTN_D, NZ0 - BTN_BURY))
          .edges(">Z").fillet(1.5))

m.add("button", button, BEZEL_DK)

# The LED sits ABOVE it, uncovered because the button is down. Wide and
# short -- long axis left to right.
# Trimmed twice on review against the reference: 5% off the first pass,
# then another 20% -- a power LED is a sliver, not a light bar.
#
# Mounted ON the notch's rear wall. It used to start 1.6 mm behind the case
# face, which left it floating 21.9 mm clear of the wall it is supposed to
# be attached to -- a lamp hanging in the mouth of the pocket rather than
# fixed at the back of it.
#
# Trimmed once more, and by half: at 14.4 it spanned most of the notch's
# width and read as a light bar rather than an indicator.
LED_T = 1.4
LED_W = 7.2
led = (cq.Workplane("XY")
       .box(LED_W, LED_T, 3.5, centered=(False, False, False))
       .translate((NX0 + (NOTCH_W - LED_W) * 0.5, NOTCH_REAR_Y - LED_T,
                   NZ0 + NOTCH_H - 5.7)))

m.add("led", led, KD["monitor_lamp"])

# Rounds the front edge of a piece of mold relief.
#
# A square-edged ridge shows the light only on the wall that happens to face
# it: the up triangle's walls rake toward the lamp and read, the down one's
# rake away and vanish, so the two glyphs did not look like the same mark. A
# rounded-over edge always turns SOME of its surface toward the light, so it
# carries a highlight along its whole length whichever way it runs -- which
# is also how a molded part actually comes out of a tool, since a sharp
# outside corner in plastic is the exception, not the rule.
#
# Only the front edges are rounded. The side walls stay square where they
# meet the face, which is what keeps the relief looking seated in the panel
# rather than glued on.
#
# A failed fillet still returns the solid rather than taking the whole model
# down -- but it SAYS SO, loudly. This used to fall back in silence, and the
# tilt icons shipped square-edged for it: the only symptom was that they
# looked flat, which got diagnosed as a lighting fault twice before anyone
# asked whether the round-over had actually been applied. A fallback nobody
# can see is worse than a crash.
def round_front(solid, radius=RELIEF_ROUND, name="relief"):
    try:
        return solid.edges("<Y").fillet(radius)
    except Exception as exc:
        print(f"WARNING: {name}: front-edge round-over FAILED "
              f"({type(exc).__name__}: {exc}) -- shipping it SQUARE-EDGED, "
              f"which will read as flat rather than molded",
              file=sys.stderr)
        return solid


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

m.add("icon_sq",   round_front(icon_sq,   name="icon_sq"),   BEIGE)
m.add("icon_ring", round_front(icon_ring, name="icon_ring"), BEIGE)
m.add("icon_bar",  round_front(icon_bar,  name="icon_bar"),  BEIGE)

# -------------------------------------------------- molded brightness icon
#
# Half way up the case on the power icon's center line: the standard
# brightness mark, a circle with its RIGHT half filled solid. Same bounding
# square and same circle as the power icon, so the two read as one family of
# controls rather than two unrelated marks.
#
# The ring and the filled half are separate solids, each rounded on its own.
# Rounding their union is what fails (see the tilt icons), and keeping them
# apart is also truer: the straight edge down the circle's diameter, where
# the filled half meets the open one, is a real molded edge and should carry
# its own round-over.

BRT_CZ = CONTRAST_CZ                       # the wheel's center line
BRT_X0 = ICON_CX - ICON_S * 0.5
BRT_Z0 = BRT_CZ - ICON_S * 0.5

brt_sq = (cq.Workplane("XY")
          .box(ICON_S, RIDGE_H, ICON_S, centered=(False, False, False))
          .translate((BRT_X0, -RIDGE_H, BRT_Z0)))
brt_sq = brt_sq.cut(
    cq.Workplane("XY")
      .box(ICON_S - RIDGE_W * 2.0, RIDGE_H + 1.0, ICON_S - RIDGE_W * 2.0,
           centered=(False, False, False))
      .translate((BRT_X0 + RIDGE_W, -RIDGE_H - 0.5, BRT_Z0 + RIDGE_W)))

brt_ring = (cq.Workplane("XY")
            .cylinder(RIDGE_H, ICON_R, direct=(0, 1, 0), centered=(True, True, False))
            .translate((ICON_CX, -RIDGE_H, BRT_CZ)))
brt_ring = brt_ring.cut(
    cq.Workplane("XY")
      .cylinder(RIDGE_H + 1.0, ICON_R - RIDGE_W, direct=(0, 1, 0), centered=(True, True, False))
      .translate((ICON_CX, -RIDGE_H - 0.5, BRT_CZ)))

# The filled half: the whole disc with everything LEFT of center taken away.
brt_half = (cq.Workplane("XY")
            .cylinder(RIDGE_H, ICON_R, direct=(0, 1, 0), centered=(True, True, False))
            .translate((ICON_CX, -RIDGE_H, BRT_CZ)))
brt_half = brt_half.cut(
    cq.Workplane("XY")
      .box(ICON_R + 1.0, RIDGE_H + 1.0, ICON_R * 2.0 + 2.0, centered=(False, False, False))
      .translate((ICON_CX - ICON_R - 1.0, -RIDGE_H - 0.5, BRT_CZ - ICON_R - 1.0)))

m.add("brt_sq",   round_front(brt_sq,   name="brt_sq"),   BEIGE)
m.add("brt_ring", round_front(brt_ring, name="brt_ring"), BEIGE)
m.add("brt_half", round_front(brt_half, name="brt_half"), BEIGE)

# --------------------------------------------------- bezel tilt icons
#
# The bezel assembly pivots a few degrees about a horizontal axis through
# the tube's center, and you tilt it by pushing the band itself. These mark
# which way: an up glyph on the top band, a down glyph on the bottom one,
# both on the flat face pointing at the user. Same mold-relief treatment as
# the power icon -- an outline with the glyph inside, raised in the bezel's
# own plastic and read entirely through the shading of its side walls.
#
# WIDE, NOT SQUARE, which is what the real monitor carries. The width is the
# power icon's, so the two match across the front; the height is whatever
# the flat band leaves once a margin is taken off each edge. The band is
# 12.7 mm and the power square is 16.8 mm, so a square at that size cannot
# fit -- the rest of the bezel's width is the raked funnel angling back to
# the tube, which is not a face a glyph can sit on. Constraining only the
# height squats the triangle, and that squat triangle is what the real one
# looks like.

TILT_MGN = 1.5                             # band edge to the icon
TILT_W   = ICON_S                          # the power icon's width, matched
TILT_H   = BAND - TILT_MGN * 2.0           # all the height the band leaves
TILT_RW  = RIDGE_W                         # and the power icon's stroke
TILT_RH  = RIDGE_H                         # and its depth
TILT_CX  = (BX0 + BX1) * 0.5               # centered on the bezel
TILT_FY  = -PROTRUDE                       # the band's front plane
TILT_GAP = TILT_RW                         # inner margin == the bar's own
                                           # thickness, so the glyph sits in
                                           # the outline by exactly the
                                           # weight of the line it touches


def tilt_icon(z_bottom, pointing_up):
    """Outlined rectangle with a triangle whose tip touches a bar, in relief.

    z_bottom is the icon's lower edge; pointing_up picks which way the
    triangle points and so which end of the rectangle the bar sits against.
    The two are exact mirrors about the icon's horizontal center line.
    """
    x0 = TILT_CX - TILT_W * 0.5
    z0 = z_bottom

    ring = (cq.Workplane("XY")
            .box(TILT_W, TILT_RH, TILT_H, centered=(False, False, False))
            .translate((x0, TILT_FY - TILT_RH, z0)))
    ring = ring.cut(
        cq.Workplane("XY")
          .box(TILT_W - TILT_RW * 2.0, TILT_RH + 1.0, TILT_H - TILT_RW * 2.0,
               centered=(False, False, False))
          .translate((x0 + TILT_RW, TILT_FY - TILT_RH - 0.5, z0 + TILT_RW)))

    # The glyph's box, inset from the outline's inner edge by TILT_GAP.
    gx0 = x0 + TILT_RW + TILT_GAP
    gx1 = x0 + TILT_W - TILT_RW - TILT_GAP
    gz0 = z0 + TILT_RW + TILT_GAP
    gz1 = z0 + TILT_H - TILT_RW - TILT_GAP

    # Mirror the SAME layout rather than writing each case out: measure the
    # bar and the triangle from the pointing end, then flip about the
    # glyph's center if we are pointing down. Written as two branches the
    # pair drifted -- the down triangle ended a stroke shorter than the up
    # one, which is half of why they did not read as the same mark.
    bar_lo = gz1 - TILT_RW                 # bar hugs the pointing end
    apex   = bar_lo                        # tip lands ON the bar
    base   = gz0

    def flip(z):
        return gz0 + gz1 - z

    if pointing_up:
        bar_z0, apex_z, base_z = bar_lo, apex, base
    else:
        bar_z0, apex_z, base_z = flip(bar_lo) - TILT_RW, flip(apex), flip(base)

    bar = (cq.Workplane("XY")
           .box(gx1 - gx0, TILT_RH, TILT_RW, centered=(False, False, False))
           .translate((gx0, TILT_FY - TILT_RH, bar_z0)))

    tri = (cq.Workplane("XZ")
           .polyline([(gx0, base_z),
                      (gx1, base_z),
                      (TILT_CX, apex_z)])
           .close()
           .extrude(TILT_RH)
           .translate((0.0, TILT_FY, 0.0)))

    # Round each stroke BEFORE unioning, the way the power icon does with its
    # three separate solids. Filleting the union instead fails outright -- OCC
    # raises "ChFi3d_Builder: only 2 faces" where the triangle's apex runs into
    # the bar -- and round_front's fallback then handed back the whole glyph
    # square-edged without saying so. That is why these read as flat bright
    # lines next to a power icon that reads as relief: the round-over written
    # to fix exactly this had never once been applied to them.
    tag = "tilt_up" if pointing_up else "tilt_down"
    return (round_front(ring, name=f"{tag} ring")
            .union(round_front(bar, name=f"{tag} bar"))
            .union(round_front(tri, name=f"{tag} triangle")))


m.add("tilt_up",
      tilt_icon((BAND_Z1 + BZ1) * 0.5 - TILT_H * 0.5, True),
      BEZEL)
m.add("tilt_down",
      tilt_icon((BZ0 + BAND_Z0) * 0.5 - TILT_H * 0.5, False),
      BEZEL)

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

# ------------------------------------------------------------ front anchor
#
# The FRAME's front plane, which the drives line up with. It is not the
# model's frontmost point -- the bezel band stands PROTRUDE proud of the
# case, and the tube's own bulge reaches a couple of millimeters past even
# that -- and lining the drives up with the frontmost point marched them
# most of an inch toward the viewer. Straddling the plane, so the midpoint
# of the marker's depth names it whatever shape the marker is.
m.add("front_anchor",
      cq.Workplane("XY")
        .box(0.4, 0.4, 0.4, centered=(True, True, True))
        .translate((REVEAL_CX, 0.0, H * 0.25)),
      KD["front_anchor"])

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

if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Monitor2.mesh"),
                    os.path.join(out, "Monitor2.mtl"), "Monitor2.mtl")
    print(f"Monitor2 (CAD): {nv} verts, {nt} tris")
    print(f"  case {W:.1f} x {H:.1f} x {D:.1f} mm, glass {GLASS_W:.1f} x {GLASS_H:.1f}")
    print(f"  reveal axis x = {REVEAL_CX:.1f}  <- s_kMon2BrandCenterXMm")
    print(f"  notch depth {NOTCH_D:.2f}, button {BTN_D:.2f} thick, setback {BTN_SETBACK:.1f}")
