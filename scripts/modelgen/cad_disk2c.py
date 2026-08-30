"""Apple Disk IIc (A2M4050Z) as a real solid model. The low off-white unit
styled to match the //c, 152 x 216 x 46 mm (W x D x H) over a pair of feet.
X right, Y back, Z up; front face at y = -2.5.

MODELED FROM PHOTOGRAPHS -- see specs/018-3d-desk-scene/reference/README.md,
which names them and says what each one settles.

The front is one composition and the pieces only make sense together:

  - a NOTCH is sunk into the face, slightly wider than the latch, running from
    below the slot up and THROUGH THE TOP of the drive, where it carries on a
    short way along the lid.
  - the LATCH sits in it, a third of the slot wide, spanning from the top of
    the drive down to the bottom of the slot when closed. What is left below
    it is open notch -- which is the finger recess, not a separate feature
    somebody added under a lever.
  - the SLOT is BEVELED at 45 degrees all round, because a diskette has to be
    guided into it by hand, and the latch's own front leans back on the way
    down so it draws out of that path rather than standing in it.

The case is TWO HALVES meeting at the plane of the slot, with a fine gap
between them running the whole way round, and the lower half tapers inward to
its base on the left and right only. Feet run across the front and the back
rather than sitting at the corners, with a rubber pad at each rounded end.

Sub-mesh identity is by part NAME: `lever` is the door the scene moves on
eject, `lamp` is the activity indicator, `slot` the opening.
Their colors are free to be the finish the photographs show -- which is why
the latch is a //c keycap's gray rather than a marker value.
"""

import cadquery as cq
from cadkit import KD, Model

# ---------------------------------------------------------------- dimensions
#
# THE HEIGHT WAS THE ERROR THAT MATTERED. At 70 mm on a 152 mm case this was a
# chunky box; the drive is a flat SLAB, about a quarter of its own width tall.
# No amount of correct detail on a case of the wrong shape reads as the right
# machine.
#
# The DEPTH went wrong and came back. Reading two underside shots cropped
# tightly to the drive, I took the IMAGE's aspect for the SUBJECT's and
# shortened the case by a fifth. A later underside with real margin around it
# measures about 1.45, which is where 216 already was. An aspect read off a
# photograph is only the subject's aspect if the subject's own bounding box is
# in the frame.
#
# Still not measured. A tape on a real one settles all three.
W, H, D = 152.0, 46.0, 216.0

INCH = 25.4

# HOW FINELY THE ROUNDED WORK IS TESSELLATED. The kernel's default is 0.3
# radians -- seventeen degrees a segment -- which on a corner this size is
# five or six facets, and under flat shading five facets are five bands. The
# Monitor II has used 0.03 since it went in; this is the same number, and the
# reason the //c pair looked faceted where it did not.
#
# ANGULAR ONLY. Tightening the LINEAR tolerance alongside it subdivides every
# flat face as well and took these two meshes from nine thousand triangles to
# two hundred thousand -- all of it spent on ground that was already flat.
CORNER_ANG = 0.14

# A generous round on every edge -- three sixteenths of an inch. Molded ABS of
# this era has no crisp arrises anywhere, and too tight a radius is most of
# what made earlier passes read as a rendering rather than a thing. An eighth
# was still too tight for that; this is half again as much.
EDGE_R = INCH * 3.0 / 16.0
CORNER_R = 6.75                   # the vertical corners, which are softer still

# ------------------------------------------------------------------- the feet
#
# Two of them, across the front and the back, not four at the corners: a
# rectangle with a half-round at each end, extruded. A quarter inch tall, half
# an inch deep, set half an inch in from the face it is nearest and from both
# sides.
#
# UP HERE because the split is measured over them -- see SPLIT_Z, which counts
# the feet as part of the bottom shell.
FOOT_H     = INCH / 4.0
FOOT_D     = INCH / 2.0
FOOT_INSET = INCH / 2.0
FOOT_R     = FOOT_D / 2.0
PAD_R      = 5.0                  # inside the half-round's own circumference
PAD_H      = 1.2
FOOT_TOTAL = FOOT_H + PAD_H       # how far the drive stands off the desk

# Warm off-white, not the //e era's platinum: the photographs show a case that
# matches the //c's own cream.
CASE    = (0.884, 0.874, 0.846)
SLOT_DK = (0.055, 0.055, 0.062)
# The foot BARS are molded in with the case and are the case's own color; only
# the PADS are rubber. They had been dark, which turned the underside into a
# pair of black stripes -- a part that is the same molding as the shell around
# it has no business being a different color from it.
PAD     = (0.180, 0.176, 0.170)

# The latch is GRAY -- a //c keycap's gray, plainly a different part from the
# case rather than a tint of it. Made a shade off the cream it vanished into
# the front, which is the opposite of what it is: the one thing on this face
# you are meant to find with a thumb.
LATCH   = (0.700, 0.692, 0.668)

# THE LID WEARS THE CASSOWARY, the same rainbow-striped bird every other
# device in this scene carries -- not a stack of colored bars standing in for
# one. The silhouette and the stripe colors are READ OUT OF CassoBranding.cpp
# rather than copied into this file: the bird has one definition, the C++ one,
# and a second hand-maintained copy is how the two would drift apart.
import io
import os
import re


def read_branding():
    src = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "..", "..", "Casso", "Ui", "Chrome",
                               "CassoBranding.cpp"), encoding="utf-8").read()

    sil_body = re.search(r"s_kSilhouette\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
    rows     = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)ULL", sil_body)]

    col_body = re.search(r"s_kStripeColors\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
    stripes  = [((int(v, 16) >> 16 & 0xFF) / 255.0,
                 (int(v, 16) >> 8 & 0xFF) / 255.0,
                 (int(v, 16) & 0xFF) / 255.0)
                for v in re.findall(r"0x[Ff]{2}([0-9A-Fa-f]{6})", col_body)]

    return rows, stripes

# ------------------------------------------------------------------ the front

# WHERE THE HALVES MEET decides the whole front, because the slot sits on
# that line and everything else is placed off the slot.
#
# The top shell is half the height of the bottom one, and the BOTTOM SHELL
# INCLUDES THE FEET -- what the eye compares is the two bands it can see, and
# the feet are part of the lower one whatever the molding says. Solved rather
# than written out, because a literal 28.15 is a number nobody can check:
#
#   H - SPLIT = (SPLIT + FOOT_TOTAL) / 2   ->   SPLIT = (2H - FOOT_TOTAL) / 3
SPLIT_Z = (2.0 * H - FOOT_TOTAL) / 3.0

# THE SEAM RUNS THROUGH THE MIDDLE OF THE SLOT, not under it: the two halves
# close around the opening, so half the slot is molded into each. Which makes
# the slot straddle the split rather than sit on top of it.
SLOT_H           = 4.5
SLOT_Z0, SLOT_Z1 = SPLIT_Z - SLOT_H * 0.5, SPLIT_Z + SLOT_H * 0.5
SLOT_X0, SLOT_X1 = 16.0, W - 16.0
SLOT_BEVEL       = 2.0            # the 45-degree lead-in, all four sides

# A THIRD OF THE SLOT, which divides the opening into three equal parts:
# segment, latch, segment. Written as the fraction rather than the answer, so
# the proportion survives the slot being resized.
LATCH_W      = (SLOT_X1 - SLOT_X0) / 3.0
LATCH_X0     = (W - LATCH_W) * 0.5
LATCH_PROUD  = 1.5                # how far it stands off the face
LATCH_SUNK   = 3.0                # how far it reaches back into the notch

# The latch's front LEANS BACK as it descends, so where it crosses the slot's
# opening it has drawn away from the diskette's path -- the slot's own lead-in
# carried across the piece that interrupts it, since a flat block standing
# proud in the middle of a chamfered mouth is a thing to catch on.
#
# ONE PLANE over the latch's whole height, not a recessed band with a ramp on
# top of it. The band version put a crease across the front at the slot's
# bevel line, and a crease between two nearly-parallel flat faces is a drawn
# line: flat shading gives each face one value, and the eye reads the boundary
# as ink. Filleting it only made it two fainter lines. Spread over twenty
# millimeters the same 1.6 mm of setback is four degrees, which shades as a
# gradient and has no edge in it anywhere.
LATCH_SLOT_BEV = 1.6

# The notch is a HAIR wider than the latch -- half a millimeter a side, the
# gap a molded part actually leaves around a moving one -- and it runs from
# below the slot up over the top and back along the lid.
#
# It is DEEPER than the latch is thick, and deliberately: pressing eject
# pushes the latch back into the drive before it rises, and the difference
# between these two numbers is the room that press has to happen in. Sized to
# LATCH_TRAVEL_IN so the two cannot drift apart -- the pair of them being one
# number apiece in two files is how the latch came to travel through the back
# of its own notch.
LATCH_TRAVEL_IN = 4.0             # mirrored by kDisk2cDoorInMm
NOTCH_W      = LATCH_W + 1.0
NOTCH_X0     = (W - NOTCH_W) * 0.5
NOTCH_Z0     = SLOT_Z0 - 12.0     # its floor, well below the slot
NOTCH_D      = LATCH_SUNK + LATCH_TRAVEL_IN + 1.5
NOTCH_LID    = 16.0               # how far back along the lid
NOTCH_LID_D  = 4.0                # how far down into the lid

# The indicator. A slash, because that is the glyph Apple molded there and the
# same one the Monitor //c wears beside its lamp.
#
# ITS LENS IS FLUSH, because that is what the part is: a matte plastic window
# sitting IN the front face, not a bulb standing off it.
#
# Not exactly zero, though -- six hundredths of a millimeter, which is the
# least that keeps the lens's face and the panel's from being coplanar. Two
# coincident faces at the same depth is not "flush", it is a z-fight, and at
# any zoom this scene offers 0.06 mm is far under a pixel.
#
# The standoff the SHADER needs is a separate thing and lives with the light
# -- see kLampLightStandoffMm. A source in the plane it lights lights none of
# it, and the answer to that is to move the source, not to grow a bump on the
# hardware.
LAMP_W, LAMP_H = 2.0, 8.0
LAMP_X, LAMP_Z = W - 24.0, 11.0
LAMP_LEAN      = 2.4
LAMP_PROUD     = 0.06

# ------------------------------------------------------------- the two halves
#
# They meet at the PLANE OF THE SLOT, and the join shows: a fine gap running
# the whole way round the case. Cut as a groove rather than built as two
# solids with air between them -- a real gap would look through the drive.
SEAM_GAP  = 0.5
SEAM_DEEP = 0.8

# The lower half draws in toward its base on the LEFT AND RIGHT ONLY -- two
# millimeters over the whole, so a millimeter a side. The front and back stay
# plumb.
TAPER = 1.0

# EVERY GROOVE'S MOUTH IS CHAMFERED rather than left as a bare arris. A sharp
# edge between two flat faces is a hard boundary between two flat values and
# nothing more; a chamfer is a third face with a normal of its own, so the
# edge catches its own sliver of light and the groove reads as something cut
# into a molding instead of drawn on one. Small -- three tenths of a
# millimeter -- because it is a broken edge, not a feature.
GROOVE_FLARE = 0.30

# And the latch's own edges. All of them small, except the one where its top
# meets its front: that one shares the case's radius, because the latch's top
# IS the drive's top for those forty millimeters and two different radii meeting
# in one line is the sort of thing that reads as a part that does not fit.
LATCH_ROUND = 0.6

# --------------------------------------------------------------------- the lid

RIB_N     = 19
RIB_W     = 2.4
RIB_DEEP  = 1.1
RIB_X0    = 22.0
RIB_X1    = W - 14.0
RIB_Y0    = 40.0                  # clear of the front's plain band
RIB_Y1    = D - 20.0

def MouthFlare(w, d, cx, cy, zMouth):
    """The chamfer ring at the mouth of a groove cut down into a face at
    zMouth: the groove's own footprint at the bottom, opened out by
    GROOVE_FLARE at the surface. Unioned into the cutter, so what it leaves
    behind is a broken edge rather than an arris."""
    return (cq.Workplane("XY", origin=(cx, cy, zMouth - GROOVE_FLARE))
            .rect(w, d)
            .workplane(offset=GROOVE_FLARE)
            .rect(w + GROOVE_FLARE * 2.0, d + GROOVE_FLARE * 2.0)
            .loft())


m = Model()

# --------------------------------------------------------------------- case

case = (cq.Workplane("XY")
        .box(W, D + 2.5, H, centered=(False, False, False))
        .translate((0.0, -2.5, 0.0))
        .edges("|Z").fillet(CORNER_R))

case = case.edges(">Z").fillet(EDGE_R)
case = case.edges("<Z").fillet(EDGE_R)

# The lower half's taper: a wedge off each flank, nothing at the split and the
# full draft by the time the base round starts.
#
# THE FIRST VERSION OF THIS CUT NOTHING AT ALL, and the reason is worth
# keeping because it is the same trap twice in one file. An XZ workplane's
# normal is -Y, so a NEGATIVE extrude runs BACKWARD, away from the viewer --
# the wedge was ending up at y 226..462 on a case that stops at 216, missing
# it entirely. Every flank was full width from base to lid, which is exactly
# what "I can't see any evidence of the taper" describes. Two lines below the
# lamp does the same arithmetic and gets it right, with a comment saying so.
#
# It also has to be open OUTWARD -- bounded inboard by the draft line and
# running away from the case on the other three sides. A wedge with its
# hypotenuse on the inboard side takes a slice out of the middle of the flank
# and leaves the outermost millimeter standing as a fin, which from outside
# is indistinguishable from no taper at all.
for sign, xEdge in ((-1.0, 0.0), (1.0, W)):
    xIn  = xEdge - sign * TAPER
    xOut = xEdge + sign * 12.0

    case = case.cut(
        cq.Workplane("XZ", origin=(0.0, 0.0, 0.0))
          .polyline([(xEdge, SPLIT_Z),
                     (xIn,   EDGE_R),
                     (xIn,   -10.0),
                     (xOut,  -10.0),
                     (xOut,  SPLIT_Z)])
          .close()
          .extrude(D + 20.0)
          .translate((0.0, D + 10.0, 0.0)))

# The lid's ribs: long shallow grooves front to back. Cut rather than laid on,
# so each one has walls that take the light differently from the lid between
# them -- painted stripes read as two tones of one flat surface.
for i in range(RIB_N):
    x = RIB_X0 + (RIB_X1 - RIB_X0 - RIB_W) * i / float(RIB_N - 1)

    case = case.cut(
        cq.Workplane("XY")
          .box(RIB_W, RIB_Y1 - RIB_Y0, RIB_DEEP + 2.0, centered=(False, False, False))
          .translate((x, RIB_Y0, H - RIB_DEEP))
          .edges("|Z").fillet(RIB_W * 0.45)
          .union(MouthFlare(RIB_W, RIB_Y1 - RIB_Y0,
                            x + RIB_W * 0.5, (RIB_Y0 + RIB_Y1) * 0.5, H)))

# A grille along the TOP edge of the back face. Never seen in this scene, and
# in the model because the drive has one.
#
# IT STOPS AT THE SEAM. The slots are molded into the TOP shell, so they end
# where that shell does -- crossing the parting line would mean one slot cut
# half into each half of a case that comes apart, which no molding does and
# which reads, correctly, as a mistake. Measured off the split rather than off
# the case height for the same reason everything else on this drive is: the
# split is what they belong to, so they follow it if it ever moves.
VENT_Z0 = SPLIT_Z + 1.5
VENT_Z1 = H - EDGE_R - 1.0
VENT_H  = VENT_Z1 - VENT_Z0

for i in range(20):
    x = 14.0 + (W - 28.0 - 2.0) * i / 19.0

    case = case.cut(
        cq.Workplane("XY")
          .box(2.0, 6.0, VENT_H, centered=(False, False, False))
          .translate((x, D - 3.0, VENT_Z0))
          .union(cq.Workplane("XZ", origin=(x + 1.0, D, VENT_Z0 + VENT_H * 0.5))
                   .rect(2.0 + GROOVE_FLARE * 2.0, VENT_H + GROOVE_FLARE * 2.0)
                   .workplane(offset=GROOVE_FLARE)
                   .rect(2.0, VENT_H)
                   .loft()))

# THE NOTCH: down the front and on over the top, one L-shaped cut. Its lower
# reach is the finger recess -- there is no separate pocket, the latch simply
# does not come down that far.
case = case.cut(
    cq.Workplane("XY")
      .box(NOTCH_W, NOTCH_D + 0.5, (H + 2.0) - NOTCH_Z0, centered=(False, False, False))
      .translate((NOTCH_X0, -3.0, NOTCH_Z0)))

case = case.cut(
    cq.Workplane("XY")
      .box(NOTCH_W, NOTCH_LID + 0.5, NOTCH_LID_D + 2.0, centered=(False, False, False))
      .translate((NOTCH_X0, -3.0, H - NOTCH_LID_D)))

# THE SLOT, beveled at forty-five degrees all round. Lofted from a mouth one
# bevel bigger than the opening back to the opening itself, because that is
# what the lead-in is: a diskette is pushed in by hand and the chamfer is what
# finds the hole for you. Built as the shape rather than chamfered afterwards,
# for the reason the Disk II's is -- a mis-picked edge renders as "off".
slot_cx = (SLOT_X0 + SLOT_X1) * 0.5
slot_cz = (SLOT_Z0 + SLOT_Z1) * 0.5
slot_w  = SLOT_X1 - SLOT_X0
slot_h  = SLOT_Z1 - SLOT_Z0

case = case.cut(
    cq.Workplane("XZ", origin=(slot_cx, -2.5, slot_cz))
      .rect(slot_w + SLOT_BEVEL * 2.0, slot_h + SLOT_BEVEL * 2.0)
      .workplane(offset=-SLOT_BEVEL)
      .rect(slot_w, slot_h)
      .loft())

case = case.cut(
    cq.Workplane("XY")
      .box(slot_w, 16.0, slot_h, centered=(False, False, False))
      .translate((SLOT_X0, -2.5 + SLOT_BEVEL, SLOT_Z0)))

# THE SEAM. A groove around the whole perimeter at the split, rather than two
# solids with daylight between them: the halves overlap inside a real drive,
# and modeling the gap as a hole would let the background through it.
# ITS INNER EDGE IS ROUNDED LIKE THE CASE. A rectangular cutter inset from a
# case with rounded corners does not stay inset AT the corners -- the
# rectangle's own corner reaches out past the case's arc, so the ring loses
# its floor exactly there and the seam shows a gap at all four corners. The
# inner shape has to be the outer one, offset.
case = case.cut(
    cq.Workplane("XY")
      .box(W + 20.0, D + 22.5, SEAM_GAP, centered=(False, False, False))
      .translate((-10.0, -12.5, SPLIT_Z - SEAM_GAP * 0.5))
      .cut(cq.Workplane("XY")
             .box(W - SEAM_DEEP * 2.0, (D + 2.5) - SEAM_DEEP * 2.0, SEAM_GAP + 2.0,
                  centered=(False, False, False))
             .translate((SEAM_DEEP, -2.5 + SEAM_DEEP, SPLIT_Z - SEAM_GAP * 0.5 - 1.0))
             .edges("|Z").fillet(CORNER_R - SEAM_DEEP)))

# ...and its two lips broken, by a second ring a flare taller and a flare
# shallower. Without it the seam is two hard arrises a half millimeter apart,
# which at any distance is one gray line; with it each lip has a facet that
# takes the light, and the joint reads as two parts meeting.
case = case.cut(
    cq.Workplane("XY")
      .box(W + 20.0, D + 22.5, SEAM_GAP + GROOVE_FLARE * 2.0, centered=(False, False, False))
      .translate((-10.0, -12.5, SPLIT_Z - SEAM_GAP * 0.5 - GROOVE_FLARE))
      .cut(cq.Workplane("XY")
             .box(W - GROOVE_FLARE * 2.0, (D + 2.5) - GROOVE_FLARE * 2.0,
                  SEAM_GAP + GROOVE_FLARE * 2.0 + 2.0, centered=(False, False, False))
             .translate((GROOVE_FLARE, -2.5 + GROOVE_FLARE,
                         SPLIT_Z - SEAM_GAP * 0.5 - GROOVE_FLARE - 1.0))
             .edges("|Z").fillet(CORNER_R - GROOVE_FLARE)))

m.add("case", case, CASE, angular=CORNER_ANG)

# THE NOTCH HAS NO LINING. It is a hollow in the case and it is the case's own
# color, which is what a hollow in a molding is.
#
# It was briefly painted a darker shade, to keep the latch readable against it
# once the monitor's shadow fell across both -- and that fixed the reading by
# telling a lie about the part, which is a trade worth refusing when there is
# another way to buy the same thing. The other way is EDGES: the latch is
# rounded on every arris now, so its outline carries a highlight of its own
# and does not need the background to be a different color to be found. What
# distinguishes a part from the hole it sits in is its edge, not its paint.

# ------------------------------------------------- faceplate furniture

# The slot's dark interior, behind the bevel.
m.add("slot",
      cq.Workplane("XY")
        .box(slot_w, 14.0, slot_h, centered=(False, False, False))
        .translate((SLOT_X0, -2.5 + SLOT_BEVEL + 0.6, SLOT_Z0)),
      SLOT_DK)

# THE LATCH, closed: from the top of the drive down to the bottom of the slot,
# sitting in the notch and standing a little proud of the face. It carries the
# door identity, so this is the piece the scene moves on eject.
#
# It is LATCH_SUNK deep, not the notch's full depth: the rest of the notch is
# the room the press travels into.
# ONE PROFILE EXTRUDED, not two boxes unioned. It was a front block and a lid
# block joined at the top, each filleted on its own axis -- so the front
# block's top corners got rounded away and the lid block stood past them,
# leaving a step where the two meet. A union does not remove a seam that its
# own fillets carved. Drawn as the L it is, the knee is just another corner of
# one outline.
LATCH_FRONT = -2.5 - LATCH_PROUD
LATCH_BACK  = -2.5 + LATCH_SUNK
LID_BACK    = -2.5 + NOTCH_LID - 2.0
LID_Z0      = H - NOTCH_LID_D
LATCH_TOP   = H + 0.6
BEV_Y       = LATCH_FRONT + LATCH_SLOT_BEV

latch = (cq.Workplane("YZ")
         .polyline([(BEV_Y,       SLOT_Z0),
                    (LATCH_BACK,  SLOT_Z0),
                    (LATCH_BACK,  LID_Z0),
                    (LID_BACK,    LID_Z0),
                    (LID_BACK,    LATCH_TOP),
                    (LATCH_FRONT, LATCH_TOP)])
         .close()
         .extrude(LATCH_W)
         .translate((LATCH_X0, 0.0, 0.0)))

# The top-front arris FIRST and at the case's own radius, because for these
# forty millimeters the latch's top is the drive's top, and two radii meeting
# in one line reads as a part that does not fit. First because a big fillet
# will not go in after the small ones have eaten the corners it needs.
latch = latch.edges("|X and >Z and <Y").fillet(EDGE_R)

# Then everything else, small. This is also what lets the notch behind it go
# back to being case-colored: a rounded arris carries a highlight along the
# whole silhouette, so the latch is found by its edge rather than by standing
# on a darker background.
latch = latch.edges("|X").fillet(LATCH_ROUND)
latch = latch.edges("<X or >X").fillet(LATCH_ROUND)

m.add("lever", latch, LATCH)

# The indicator, leaning right like the glyph. Drawn in the FRONT plane and
# extruded toward the viewer -- an XZ workplane's normal is -Y, so a positive
# extrude comes forward out of the face rather than sideways along it.
m.add("lamp",
      cq.Workplane("XZ")
        .polyline([(LAMP_X, LAMP_Z),
                   (LAMP_X + LAMP_W, LAMP_Z),
                   (LAMP_X + LAMP_W + LAMP_LEAN, LAMP_Z + LAMP_H),
                   (LAMP_X + LAMP_LEAN, LAMP_Z + LAMP_H)])
        .close()
        .extrude(LAMP_PROUD + 1.0)
        .translate((0.0, -1.5, 0.0)),
      KD["drive_lamp_alt"])

# The cassowary, on the LID at the rear-left, where the photographs put the
# logo. Under the monitor in this scene's stack and so rarely seen, which is
# not a reason to put something ELSE there.
#
# Upright for someone standing at the FRONT of the drive: a mark on a lid is
# read the way text on a lid is, top edge away from the reader -- so row zero
# of the grid, the bird's crown, lands at the largest y. One raised plate per
# contiguous bit run, striped by row exactly as BrandMask stripes the 3D
# stamps, gathered into one part per stripe so the mesh carries six materials
# rather than a hundred and fifty.
BIRD_H  = 19.0                    # front-to-back on the lid

_rows, _stripes = read_branding()
_cell    = BIRD_H / len(_rows)
_nonzero = [i for i, r in enumerate(_rows) if r]
_first, _last = _nonzero[0], _nonzero[-1]

# CENTERED IN THE PLAIN BAND between the lid's left edge and the first rib,
# with the same margin again off the back edge. Measured on the silhouette's
# INK, not its 64-bit grid -- the grid carries margins of its own as an
# encoding accident, and centering those centers nothing.
_cols   = [c for bits in _rows for c in range(64) if (bits >> c) & 1]
_collo, _colhi = min(_cols), max(_cols)
_inkw   = (_colhi - _collo + 1) * _cell
_marg   = (RIB_X0 - _inkw) * 0.5
BIRD_X0 = _marg - _collo * _cell
BIRD_Y1 = D - _marg + _first * _cell    # the crown, rearmost
_by_stripe = {}

for row, bits in enumerate(_rows):
    if not bits:
        continue

    banded = min(_last, max(_first, row))
    stripe = ((banded - _first) * len(_stripes)) // (_last - _first + 1)
    y0     = BIRD_Y1 - (row + 1) * _cell
    col    = 0

    while col < 64:
        if not (bits >> col) & 1:
            col += 1
            continue

        run = col
        while run < 64 and (bits >> run) & 1:
            run += 1

        _by_stripe.setdefault(stripe, []).append(
            cq.Workplane("XY")
              .box((run - col) * _cell, _cell + 0.02, 0.5,
                   centered=(False, False, False))
              .translate((BIRD_X0 + col * _cell, y0, H - 0.2))
              .val())
        col = run

for stripe, solids in sorted(_by_stripe.items()):
    m.add(f"bird{stripe}", cq.Compound.makeCompound(solids), _stripes[stripe])

# ---------------------------------------------------------------- underside

for name, fy in (("front", -2.5 + FOOT_INSET + FOOT_R), ("rear", D - FOOT_INSET - FOOT_R)):
    x0 = FOOT_INSET + FOOT_R
    x1 = W - FOOT_INSET - FOOT_R

    foot = (cq.Workplane("XY")
            .box(x1 - x0, FOOT_D, FOOT_H, centered=(False, False, False))
            .translate((x0, fy - FOOT_R, -FOOT_H)))

    for fx in (x0, x1):
        foot = foot.union(
            cq.Workplane("XY")
              .cylinder(FOOT_H, FOOT_R, centered=(True, True, False))
              .translate((fx, fy, -FOOT_H)))

        # The rubber pad at each end, inside the half-round's own circle.
        m.add(f"pad_{name}_{fx:.0f}",
              cq.Workplane("XY")
                .cylinder(PAD_H, PAD_R, centered=(True, True, False))
                .translate((fx, fy, -FOOT_H - PAD_H)),
              PAD)

    m.add(f"foot_{name}", foot, CASE)


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Disk2c.mesh"),
                    os.path.join(out, "Disk2c.mtl"), "Disk2c.mtl")
    print(f"Disk2c (CAD): {nv} verts, {nt} tris")
