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
BTN_D       = NOTCH_D - BTN_SETBACK - BTN_BACKGAP

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
PANEL_GRAY = (0.820, 0.805, 0.770)    # the control panel's lighter inset
DARK_PART  = (0.085, 0.085, 0.090)    # wheels, inlet, RCA barrel and bore
RCA_RING   = (0.920, 0.910, 0.890)

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
STRIP_TOP  = 95.0                     # the vertical rear strip below the slope
REAR_RUN   = 72.0                     # how far forward the slope's top lands
SLOPE_LEN  = math.hypot (REAR_RUN, H - STRIP_TOP)
SLOPE_ANG  = math.degrees (math.atan2 (REAR_RUN, H - STRIP_TOP))
REAR_RIM   = 15.0                     # case border around the recess
REAR_DEEP  = 9.0                      # the recess, into the slope


def tilt_rear(wp):
    """Rotate a pre-tilt cutter or part built against the vertical y = D
    plane into the sloped rear face, about the slope's hinge line."""
    return wp.rotate ((0.0, D, STRIP_TOP), (1.0, D, STRIP_TOP), SLOPE_ANG)


# The shear itself: everything behind the sloped plane, above the strip.
case = case.cut (tilt_rear (
    cq.Workplane ("XY")
      .box (W + 2.0, 220.0, H + 150.0 - STRIP_TOP, centered=(False, False, False))
      .translate ((-1.0, D, STRIP_TOP))))

# The recessed panel, inset a rim's width from the slope's edges. Its corners
# round in the pre-tilt plane, which is what makes them round IN the face.
case = case.cut (tilt_rear (
    cq.Workplane ("XY")
      .box (W - REAR_RIM * 2.0, REAR_DEEP + 60.0, SLOPE_LEN - REAR_RIM * 2.0,
            centered=(False, False, False))
      .edges ("|Y").fillet (6.0)
      .translate ((REAR_RIM, D - REAR_DEEP, STRIP_TOP + REAR_RIM))))

# Vent slots across the recess's upper band: two rows of narrow vertical
# slots, cut as ONE compound rather than eighty booleans.
VENT_SLOT_W  = 2.5
VENT_PITCH   = 6.5
VENT_ROW_H   = 20.0
VENT_X0      = 60.0
VENT_X1      = W - 60.0
VENT_ROWS_Z  = (STRIP_TOP + SLOPE_LEN - 67.0, STRIP_TOP + SLOPE_LEN - 43.0)

_slots = []

for _rz in VENT_ROWS_Z:
    _x = VENT_X0

    while _x + VENT_SLOT_W <= VENT_X1:
        _slots.append (cq.Workplane ("XY")
                         .box (VENT_SLOT_W, REAR_DEEP + 8.0, VENT_ROW_H,
                               centered=(False, False, False))
                         .translate ((_x, D - REAR_DEEP - 6.0, _rz))
                         .val())
        _x += VENT_PITCH

case = case.cut (tilt_rear (cq.Workplane (obj=cq.Compound.makeCompound (_slots))))

# ------------------------------------------------------------ control panel
#
# The vertical strip under the slope carries the controls on a LIGHTER inset
# plate -- the real panel is its own molding, a shade off the case around it.
# The pocket is cut here; the plate itself is a separate part below, and the
# icons engrave into the plate rather than the case.
PANEL_W   = 300.0
PANEL_X0  = (W - PANEL_W) * 0.5
PANEL_Z0  = 16.0
PANEL_Z1  = STRIP_TOP - 16.0
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
AC_CX    = W * 0.5 + 118.0
CTRL_CXS = (W * 0.5 + 45.0, W * 0.5 - 5.0, W * 0.5 - 55.0)
RCA_CX   = W * 0.5 - 105.0
CTRL_CZ  = PANEL_Z0 + 20.0            # the controls' row
ICON_CZ  = PANEL_Z1 - 17.0            # the icons' row

# The wheel wells, drilled into the CASE here while it is still being cut --
# the plate's own holes are drilled where the plate is built, but a hole
# through a 1.7 mm plate with solid case behind it just shows beige where
# the wheel should be. The well is what the wheel stands in.
for _cx in CTRL_CXS:
    case = case.cut (
        cq.Workplane ("XY")
          .cylinder (14.0, 6.2, direct=(0, 1, 0), centered=(True, True, False))
          .translate ((_cx, D - 13.0, CTRL_CZ)))

# Finer than the default, and note it is the ANGULAR tolerance doing the
# work: at 3 mm the chords never sag far enough for the linear one to bite,
# so the stock setting spent about three segments on a quarter turn and the
# corners read as facets meeting at an angle rather than as rounds.
m.add("case", case, BEIGE, angular=CORNER_ANG)

# --------------------------------------------------------------- rear parts

# THE BELL: the tube's rear housing, emerging through the recess and running
# back to the strip's plane. Lofted between two VERTICAL sections -- a wide
# tall one buried inside the case and the narrow low rear face -- so the top
# slopes down toward the back, the sides draw in, and the bottom stays
# nearly level, exactly the hopper the photographs show. IN THE BEZEL'S
# COLOR: the bell, the tilting bezel, and the power button are the same gray
# molding family on the real unit, and the case's beige is not in it.
BELL_REAR_Y = D - 0.6                 # a hair inside the strip's plane

bell = cq.Solid.makeLoft ([
    round_rect_wire (D - 95.0, W * 0.5 - 128.0, W * 0.5 + 128.0, 96.0, 205.0, 18.0),
    round_rect_wire (BELL_REAR_Y, W * 0.5 - 96.0, W * 0.5 + 96.0, 101.0, 162.0, 12.0),
])

m.add ("bell", cq.Workplane (obj=bell), BEZEL, angular=CORNER_ANG)

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
BELL_BIRD_CZ = 138.0

_rows, _stripes = read_branding()
_cell     = BELL_BIRD_H / len (_rows)
_nonzero  = [i for i, r in enumerate (_rows) if r]
_first    = _nonzero[0]
_cols     = [c for bits in _rows for c in range (64) if (bits >> c) & 1]
_collo    = min (_cols)
_colhi    = max (_cols)
_bird_x0  = W * 0.5 - ((_collo + _colhi + 1) * 0.5) * _cell
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
       tilt_rear (cq.Workplane ("XY")
                    .box (94.0, 1.4, 38.0, centered=(False, False, False))
                    .edges ("|Y").fillet (2.0)
                    .translate ((W * 0.5 - 47.0, D - REAR_DEEP,
                                 STRIP_TOP + SLOPE_LEN - 69.0))),
       LABEL_GRAY, angular=CORNER_ANG)

# ------------------------------------------------- the engraved control row
#
# The Monitor //c's engraving grammar, brought back to the machine it grew
# from: hairline cuts exactly as deep as they are wide, floors rounded over
# so the line reads by the shadow it holds, glyphs boxed in rounded squares.
# Here they cut into the PANEL PLATE rather than a bay floor.
STROKE  = 0.35
CUT_D   = STROKE
ICON_S  = 13.0
BOX_R   = 0.9

ENGRAVE_ROUND = min (0.35, STROKE * 0.45)

PANEL_FACE_Y = D - PANEL_SET          # the plate's face, where cuts begin

panel = (cq.Workplane ("XY")
         .box (PANEL_W - 1.0, PANEL_IN - PANEL_SET, PANEL_Z1 - PANEL_Z0 - 1.0,
               centered=(False, False, False))
         .edges ("|Y").fillet (3.6)
         .translate ((PANEL_X0 + 0.5, D - PANEL_IN, PANEL_Z0 + 0.5)))


def engrave(solid):
    """Cut one mark into the panel plate, its floor rounded over first."""
    global panel

    try:
        solid = solid.edges ("<Y").fillet (ENGRAVE_ROUND)
    except Exception:
        print ("WARNING: engrave: floor round-over FAILED, cutting square")

    panel = panel.cut (solid)


def stroke_box(cx, cz, w, h, rot_deg=0.0):
    """A stroke cutter centered at (cx, cz) on the plate, optionally rotated
    about its own y-axis center."""
    s = (cq.Workplane ("XY")
         .box (w, CUT_D + 0.5, h, centered=(True, False, True))
         .translate ((0.0, PANEL_FACE_Y - CUT_D, 0.0)))

    if rot_deg != 0.0:
        s = s.rotate ((0, 0, 0), (0, 1, 0), rot_deg)

    return s.translate ((cx, 0.0, cz))


def outline_ring(cx, cz, w, h, r):
    """A rounded-rectangle OUTLINE cutter: outer minus inner."""
    y = PANEL_FACE_Y - CUT_D

    outer = cq.Solid.extrudeLinear (
        cq.Face.makeFromWires (round_rect_wire (y, cx - w * 0.5, cx + w * 0.5,
                                                cz - h * 0.5, cz + h * 0.5, r)),
        cq.Vector (0.0, CUT_D + 0.5, 0.0))
    inner = cq.Solid.extrudeLinear (
        cq.Face.makeFromWires (round_rect_wire (y - 0.1,
                                                cx - w * 0.5 + STROKE, cx + w * 0.5 - STROKE,
                                                cz - h * 0.5 + STROKE, cz + h * 0.5 - STROKE,
                                                max (0.4, r - STROKE))),
        cq.Vector (0.0, CUT_D + 0.7, 0.0))

    return cq.Workplane (obj=outer).cut (cq.Workplane (obj=inner))


def circle_ring(cx, cz, r):
    """A circle OUTLINE cutter: an annulus one stroke wide."""
    outer = (cq.Workplane ("XY")
               .cylinder (CUT_D + 0.5, r, direct=(0, 1, 0), centered=(True, True, False))
               .translate ((cx, PANEL_FACE_Y - CUT_D, cz)))
    inner = (cq.Workplane ("XY")
               .cylinder (CUT_D + 0.7, r - STROKE, direct=(0, 1, 0), centered=(True, True, False))
               .translate ((cx, PANEL_FACE_Y - CUT_D - 0.1, cz)))

    return outer.cut (inner)


def arrow_head(cx, cz, w, h, up):
    """A solid triangular arrowhead cutter, apex up or down."""
    y  = PANEL_FACE_Y - CUT_D
    zt = cz + (h * 0.5 if up else -h * 0.5)
    zb = cz - (h * 0.5 if up else -h * 0.5)

    return cq.Workplane (obj=cq.Solid.extrudeLinear (
        cq.Face.makeFromWires (cq.Wire.makePolygon ([
            cq.Vector (cx - w * 0.5, y, zb),
            cq.Vector (cx + w * 0.5, y, zb),
            cq.Vector (cx, y, zt),
            cq.Vector (cx - w * 0.5, y, zb),
        ])),
        cq.Vector (0.0, CUT_D + 0.5, 0.0)))


CRT_W = ICON_S - 3.6
CRT_H = (ICON_S - 3.6) * 0.75

# Vertical hold, over the first wheel: the screen alone in its box.
engrave (outline_ring (CTRL_CXS[0], ICON_CZ, ICON_S, ICON_S, BOX_R))
engrave (outline_ring (CTRL_CXS[0], ICON_CZ, CRT_W, CRT_H, 2.4))

# Vertical size, over the second: the screen with an up arrowhead rising
# out of a base stroke.
engrave (outline_ring (CTRL_CXS[1], ICON_CZ, ICON_S, ICON_S, BOX_R))
engrave (outline_ring (CTRL_CXS[1], ICON_CZ, CRT_W, CRT_H, 2.4))
engrave (arrow_head (CTRL_CXS[1], ICON_CZ + 1.2, 2.6, 1.6, True))
engrave (stroke_box (CTRL_CXS[1], ICON_CZ - 1.2, 3.2, STROKE))

# Brightness, over the third: the circle with eight short rays that touch
# nothing -- the //c rear's sun, the same rotation lesson included: a
# rotation about +y runs OPPOSITE the position angle in the x-z plane.
engrave (outline_ring (CTRL_CXS[2], ICON_CZ, ICON_S, ICON_S, BOX_R))
engrave (circle_ring (CTRL_CXS[2], ICON_CZ, 2.2))

for _k in range (8):
    _ang = _k * 45.0
    _rx  = CTRL_CXS[2] + 4.0 * math.cos (math.radians (_ang))
    _rz  = ICON_CZ + 4.0 * math.sin (math.radians (_ang))

    engrave (stroke_box (_rx, _rz, STROKE, 1.6, 90.0 - _ang))

# Video in, over the RCA: the screen with a stroke arriving from the left
# through its wall, ending in an arrowhead laid on its side.
engrave (outline_ring (RCA_CX, ICON_CZ, ICON_S, ICON_S, BOX_R))
engrave (outline_ring (RCA_CX + 0.9, ICON_CZ, CRT_W - 1.8, CRT_H, 2.0))
engrave (stroke_box (RCA_CX + 3.6, ICON_CZ, 3.4, STROKE))
engrave (stroke_box (RCA_CX + 1.2, ICON_CZ, 2.2, 2.2, 45.0))

# The thumbwheel holes, drilled through the plate into the case behind it.
for _cx in CTRL_CXS:
    panel = panel.cut (
        cq.Workplane ("XY")
          .cylinder (12.0, 6.0, direct=(0, 1, 0), centered=(True, True, False))
          .translate ((_cx, D - 11.0, CTRL_CZ)))

m.add ("ctrl_panel", panel, PANEL_GRAY, angular=CORNER_ANG)

# The wheels themselves: vertical-axis discs standing behind each hole, so
# what shows through is the rim edge-on -- which is all the real ones show.
for _i, _cx in enumerate (CTRL_CXS):
    m.add (f"ctrl_wheel{_i}",
           cq.Workplane ("XY")
             .cylinder (4.5, 8.0, direct=(0, 0, 1))
             .translate ((_cx, D - 10.5, CTRL_CZ)),
           DARK_PART, angular=0.05)

# The mains inlet: a dark molded block with the captive cord's boss -- the
# A2M2010 has no removable lead, so there is no receptacle to model, just
# the molding the cord leaves through.
m.add ("ac_inlet",
       cq.Workplane ("XY")
         .box (34.0, 4.0, 22.0, centered=(False, False, False))
         .edges ("|Y").fillet (3.0)
         .translate ((AC_CX - 17.0, D - 3.0, CTRL_CZ - 11.0)),
       DARK_PART)
m.add ("ac_boss",
       cq.Workplane ("XY")
         .cylinder (4.0, 5.0, direct=(0, 1, 0), centered=(True, True, False))
         .translate ((AC_CX, D - 1.0, CTRL_CZ)),
       DARK_PART, angular=0.05)

# The RCA jack, exactly the //c rear's recipe: dark barrel, white insulator
# ring, dark center bore.
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

funnel = cq.Solid.makeLoft([
    round_rect_wire(-PROTRUDE,             BAND_X0, BAND_X1, BAND_Z0, BAND_Z1, FUNNEL_FRONT_R),
    round_rect_wire(-PROTRUDE + TUBE_DROP, MX0,     MX1,     MZ0,     MZ1,     MOUTH_RR),
])

bezel = bezel.cut(cq.Workplane(obj=funnel))

# The tunnel behind the mouth carries the SAME rounded profile. Cut square,
# its corners stood proud of the funnel's rounded ones and left a wedge of
# bezel hanging into each corner of the opening.
tunnel = cq.Solid.makeLoft([
    round_rect_wire(-PROTRUDE + TUBE_DROP,             MX0, MX1, MZ0, MZ1, MOUTH_RR),
    round_rect_wire(-PROTRUDE + TUBE_DROP + CAVITY_D,  MX0, MX1, MZ0, MZ1, MOUTH_RR),
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
                          front_y=-PROTRUDE + TUBE_DROP,
                          radius_scale=FACE_R / GLASS_HALF_DIAG),
                KD["glass"])

# ------------------------------------------------------- power button + LED

# The button, locked down: it fills the lower part of the notch and stands
# proud of the notch floor, not of the case.
#
# Only the TOP corners are rounded. The bottom of the button travels down
# into the frame when it is pushed, so that end is a sliding fit inside the
# notch and never presents a molded edge to round -- rounding it read as a
# free-standing tab rather than something that disappears into the case.
button = (cq.Workplane("XY")
          .box(NOTCH_W - 3.0, BTN_D, NOTCH_H - 8.5, centered=(False, False, False))
          .translate((NX0 + 1.5, BTN_SETBACK, NZ0 + 1.0))
          .edges("|Y and >Z").fillet(1.5))

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
LED_T = 1.4
led = (cq.Workplane("XY")
       .box(14.4, LED_T, 3.5, centered=(False, False, False))
       .translate((NX0 + (NOTCH_W - 14.4) * 0.5, NOTCH_REAR_Y - LED_T,
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

BRT_CZ = H * 0.5                           # the case's vertical midpoint
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
