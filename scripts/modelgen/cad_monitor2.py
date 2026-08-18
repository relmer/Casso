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

RELIEF_ROUND = 0.35                        # front-edge round-over on all
                                           # mold relief; must stay under
                                           # half the stroke width

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
# rather than glued on. Returns the solid untouched if the kernel cannot
# take the radius -- a fillet that fails on one glyph should not take the
# whole model down with it.
def round_front(solid, radius=RELIEF_ROUND):
    try:
        return solid.edges("<Y").fillet(radius)
    except Exception:
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
RIDGE_W  = 1.0                             # stroke width of the relief
RIDGE_H  = 2.5                             # proud of the face. At 0.45 the
                                           # relief barely read; at 1.0 a
                                           # filled glyph still did not, since
                                           # its top face takes the same light
                                           # as the plastic around it and only
                                           # the side walls show. Depth is the
                                           # only lever on a filled shape.

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

m.add("icon_sq",   round_front(icon_sq),   BEIGE)
m.add("icon_ring", round_front(icon_ring), BEIGE)
m.add("icon_bar",  round_front(icon_bar),  BEIGE)

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

    return round_front(ring.union(bar).union(tri))


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
