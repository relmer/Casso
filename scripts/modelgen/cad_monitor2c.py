"""Apple Monitor //c (order A2M4090, model G0905) as a real solid model.

241.3 x 259.1 x 185.4 mm (W x D x H) -- 9.5 x 10.2 x 7.3 inches, the published
figures for the 9-inch green-phosphor monitor Apple shipped with the //c. X
right, Y back, Z up; the case's front plane is y = 0 and the bezel stands
proud of it.

MODELED FROM PHOTOGRAPHS -- a straight-on front, a rear three-quarter, and two
side elevations. See specs/018-3d-desk-scene/reference/README.md.

The case is TWO MASSES. The front one carries the tube: full width and height,
rounded on every edge, about two fifths of the depth. The rear one is narrower
and much shorter, sits on the same base line, and is louvered along both
flanks and across its lid, with a molded carrying handle at the very back.
What joins them is a step, not a taper.

THE FRONT'S WHOLE READ IS THAT IT HAS NO EDGES. A bezel plate stands proud of
the case and rolls back to it through a quarter-round that carries all the way
around the perimeter -- and, crucially, THROUGH THE CORNERS: the roll's
sections are rounded rectangles whose corner radius shrinks with their inset,
so the left flank's angle sweeps into the top's across the corner instead of
meeting it at a crease. Lofting sharp rectangles, which is what this generator
used to do, put a hard diagonal at each of the four corners; nothing on the
real molding does that.

The screen recess is a lofted CUT from the plate hole back to the opening, so
the inner lip's slope is the cut's own wall, the interior corners are curves
by construction, and the opening is genuinely open. The overhang is SHALLOW --
the tube sits close behind the bezel on this monitor.

The scene stamps the cassowary onto the chin and derives the display sphere
from the glass mesh, so s_kBrand* in DeskSceneModel.cpp are tied to the plate
plane and the chin band here.
"""

import math

import cadquery as cq
from cadkit import KD, Model, round_rect_wire, sag_sheet

# ---------------------------------------------------------------- dimensions

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

W, D, H = 9.5 * INCH, 10.2 * INCH, 7.3 * INCH

# How much of the depth the tube's housing takes before the case steps down to
# the electronics box behind it.
FRONT_D = 0.47 * D

R_CASE  = 16.0                    # the front mass's corner radius, in the face

# ONE BROKEN-EDGE RADIUS FOR THE WHOLE MONITOR, and it is the Disk IIc's --
# three sixteenths of an inch. Corner radius IN THE FACE and the radius an
# EDGE is rolled with are two different things, and only the first of them
# wants to be large: 16 mm reads as a molding's corner, while a 14-by-8 roll
# on the same molding's edge reads as a pillow. Every edge on this case takes
# the same one, the way a real tool would cut them.
EDGE_R = INCH * 3.0 / 16.0

CAP_STEPS = 9                     # facets across a rolled edge

# ...and the rolls are where the banding was: a five-facet quarter round is
# five flat values under flat shading, and on a rim this size that reads as
# five stripes rather than one curve.

# THE FRONT IS TWO BANDS, and they are different things. Calling both of them
# "the bezel" is what kept this front wrong.
#
#   THE FRAME is the wide band you actually look at, the same width the whole
#   way round, and it ANGLES TOWARD THE VIEWER as it goes inward -- about ten
#   degrees, so its inner edge is the most proud point on the monitor.
#
#   THE BEZEL is the narrower band inside it, angling back the other way at a
#   much steeper rate, down to the tube. A third of an inch, front to back.
#
# The crease between them is an edge like any other and takes the same roll.
# MEASURED off a straight-on photograph of a real unit, scaled by the
# monitor's own 241.3 mm width in its own front plane. At the screen's
# mid-height the bands come out: frame 23.1 mm on the left and 24.3 on the
# right, bezel 7.9 and 7.3, glass 178.7 -- so a border of 31.0 mm, split about
# three to one between the frame and the bezel. FRAME below is measured from
# the outer roll's tangent, so the visible band is FRAME + EDGE_R.
#
# The photograph is within a degree of head on: the left and right bezel bands
# project the same width, which fixes the bezel's WIDTH exactly and says
# nothing at all about its DEPTH -- a well's wall only shows off-axis. A
# quarter inch of depth is the guess that goes with a 7.6 mm band, giving the
# forty degrees the slope's shading reads as.
FRAME     = 19.0                  # the frame's width, uniform on all four sides
FRAME_ANG = math.radians(10.0)    # ...and how far it leans toward the viewer

# THE BEZEL IS A FRACTION OF THE FRAME, and specifically of the frame's
# APPARENT width -- what the two bands measure as, looking at the front. So
# BEZ_W is the bezel's extent in X, not the length of its slant: the slant is
# longer, by however much the depth adds, and that is the hypotenuse rather
# than the number being set here. Written as the ratio because that is the
# thing being judged; the millimeters are a consequence.
BEZ_RATIO = 1.0 / 6.0
BEZ_W     = (EDGE_R + FRAME) * BEZ_RATIO
BEZ_DEPTH = INCH / 4.0            # the depth, which no head-on photo can give

# AND THE CREASE BETWEEN THEM IS NEARLY SHARP. Every other edge on this case
# is broken at the Disk IIc's three sixteenths, and that is right for an edge
# you could run a thumb along -- but this one is a change of DIRECTION in the
# molding's face, not an arris on its outside, and rolling it at five
# millimeters turned a hard line into a soft trough. A little more than the
# flare on the drive's vent slots is all it wants: enough to catch a highlight
# and stop the crease reading as ink, and no more.
CREASE_R = 0.45
CREASE_STEPS = 3                  # facets across it, at this radius
BEZ_ANG   = math.atan2(BEZ_DEPTH, BEZ_W)

# The border is therefore the same all round, and the opening is what it
# leaves. The marks that used to want a deep chin now sit on the frame itself,
# which is broad enough for them and barely sloped.
BORDER = EDGE_R + FRAME + BEZ_W

OX0, OX1 = BORDER, W - BORDER
OZ0, OZ1 = BORDER, H - BORDER
SCR_W, SCR_H = OX1 - OX0, OZ1 - OZ0

# Corner radius against inset: a band of constant width shrinks its radius by
# its own width, which goes negative long before the opening. Floored, so the
# outlines stay monotone and the screen keeps corners a molding would have.
R_FLOOR  = 6.0
R_OPEN   = R_FLOOR

# The glass all but fills the opening. Five millimeters of inset left a ring
# of bare CAVITY showing round the tube -- dark, and as deep as the throat --
# which read as well depth that no part of the monitor actually has. Two is
# enough to keep the rectangular sheet inside the opening's rounded corners
# (which it does with room to spare at R_OPEN 6) and no more.
# THE TUBE RUNS PAST THE OPENING, and the bezel's lip covers the difference.
#
# It used to stop 2 mm short of the hole all round, which sounds tidy and is
# exactly backwards: the picture is inset from the GLASS by the faceplate
# border, so a glass that stops short of the opening leaves the picture short
# of it twice over -- and worst at the CORNERS, where a rectangular raster is
# already retreating from a rounded mouth. Six and a half millimeters of black
# in the corners against four along the sides, all of it aperture nobody sees
# a tube through.
#
# Run the glass PAST the hole instead and the surplus disappears behind the
# lip, which is what a lip is for. Two millimeters over brings the raster's
# corners onto the bezel's corner arc within a millimeter, and still leaves a
# couple of millimeters of faceplate showing along the edges -- a tube in a
# bezel, rather than a picture hiding inside one.
GLASS_OVER = 2.0                  # how far the glass runs PAST the opening
GLASS_IN   = -GLASS_OVER          # ...as an inset, which is what the math wants
# HOW FAR THE CROWN SITS BEHIND THE FRAME'S NOSE -- solved, not chosen; see
# where CROWN_SET is worked out, below the planes it depends on.
#
# A spherical tube in a rectangular hole is tightest on its SHORT axis, and by
# a long way: at this radius the sides drop 11.8 mm of sag before they reach
# the opening and the top and bottom only 5.8. So the top and bottom edges are
# what decide whether the glass clears the bezel's lip -- and a crown picked by
# eye left them 0.02 mm in FRONT of it, which is dark biting into the bezel
# across the top middle and along the bottom, with the sides untouched.
CROWN_CLEAR  = 1.0                # margin at the tightest point
OVERLAY_LIFT = 0.5                # mirrors DeskScene::kMaskLiftMm

# THE TUBE'S CURVATURE IS THE MONITOR II's RULE, applied to a smaller tube.
#
# cad_monitor2.py works the faceplate radius from the CRT patents' reference,
# R = 1.767 x the screen diagonal, times the 0.96 it measured off a photograph
# of a real unit -- and that is a rule about tubes, not about one cabinet, so
# it carries straight across to this one. Nine inches instead of twelve; same
# family, same era, same generation of glass.
#
# WHICH MEANS THE SAG NO LONGER FITS INSIDE THE BEZEL, and it does not have to.
# The rim goes wherever the sphere puts it and the CROWN is what gets placed:
# just behind the frame's nose, where a tube's face sits. The bezel's inner
# lip then overhangs the glass's edge by ten millimeters or so, which is what
# a lip is for. The two were tied together only because the rim used to be
# pinned to the opening plane, and that is what kept sending the tube out
# through the front of its own case.
TUBE_DIAG = 9.0 * INCH            # the tube class, not the picture
FACE_R    = 0.96 * 1.767 * TUBE_DIAG

_GLASS_HALF = math.hypot((SCR_W - GLASS_IN * 2.0) * 0.5,
                         (SCR_H - GLASS_IN * 2.0) * 0.5)
SAG         = FACE_R - math.sqrt(FACE_R * FACE_R - _GLASS_HALF * _GLASS_HALF)
SAG_SCALE   = FACE_R / _GLASS_HALF

# The planes the front is built between. OPEN_Y is where the bezel's wall ends
# -- the hole -- and GLASS_Y is where the tube's RIM sits, which is deeper,
# because the crown is what has to land near the front and the sphere decides
# the rest.
FRAME_OUT_Y = -EDGE_R                                   # where the outer roll ends
FRAME_IN_Y  = FRAME_OUT_Y - FRAME * math.tan(FRAME_ANG)  # the most proud point
OPEN_Y      = FRAME_IN_Y + BEZ_DEPTH                     # the opening

# The crown's setback, solved off the binding edge -- see CROWN_CLEAR.
_BIND_R   = SCR_H * 0.5 + GLASS_OVER      # the short side's midpoint
_BIND_SAG = FACE_R - math.sqrt(FACE_R * FACE_R - _BIND_R * _BIND_R)
CROWN_SET = max(0.5, BEZ_DEPTH + CROWN_CLEAR + OVERLAY_LIFT - _BIND_SAG)

GLASS_Y     = FRAME_IN_Y + CROWN_SET + SAG               # the tube's rim

# The box behind. Narrower and much shorter than the tube housing, CENTERED
# on it in both axes -- the photographs show clear front-mass margin on every
# side of it, not a box sharing the base line. It reaches back to the full
# depth.
REAR_W   = 190.0
REAR_H   = 112.0
R_REAR   = 14.0
REAR_Y0  = FRONT_D - 25.0         # overlapped into the front mass, so the union solids
REAR_X0  = (W - REAR_W) * 0.5
REAR_Z0  = (H - REAR_H) * 0.5
REAR_Z1  = REAR_Z0 + REAR_H

# Louvers: along the depth on both flanks, across the width on the lid.
# THE VENTS, exactly as the case is cut: sixteen lines across the lid and
# sixteen across the underside, eleven down each flank, each set CENTERED on
# its own face -- n lines and n+1 equal gaps, so the face reads
# gap-line-gap-...-line-gap and the count fixes the rhythm. All of them run
# front to rear, stop short of the step onto the front mass, and terminate in
# a single groove that rings the box's whole circumference one gap in from
# its rear plane. The front mass answers with a ring of its own, a gap and a
# half forward of its rear face.
#
# ONE gap unit serves every face's margins and both rings, taken from the
# flanks -- the per-face gaps differ by a couple of millimeters, and a ring
# has one position, not one per face.
LOUV_W      = 2.2                 # slot width
LOUV_DEEP   = 2.0
VENT_N_LID  = 16                  # lid and underside
VENT_N_SIDE = 11                  # each flank

# The carrying handle: a pocket sunk into the FRONT mass's rear face, up near
# its top -- which is where a hand actually goes when the monitor is lifted,
# wrapped over the case's crown with the fingers hooking in behind. It was on
# the rear box's lid, which is both the wrong part and a place the photographs
# show plain louvered plastic.
GRIP_W     = 110.0                # the pocket's width, centered
GRIP_Z0    = H - 32.0             # ...and its bottom edge, near the top
GRIP_H     = 18.0
GRIP_DEEP  = 16.0                 # how far forward into the housing it reaches

PLAT     = (0.870, 0.862, 0.835)
CAVITY   = (0.055, 0.055, 0.062)

m = Model()


def case_wire(y, inset):
    """The case outline at depth y, drawn in from the full section by `inset`.

    The radius shrinks with the inset so the band between two of these is a
    constant width the whole way round, corners included -- floored, because
    an inset deeper than the corner radius would otherwise ask for a negative
    one and the outlines would stop being nested."""
    return round_rect_wire(y, inset, W - inset, inset, H - inset,
                           max(R_FLOOR, R_CASE - inset))


# -------------------------------------------------------------------- shell

# THE FRAME first, because it is the most proud thing on the monitor: one
# ruled band from its inner outline forward-and-inward to its outer one, ten
# degrees. Two rounded rectangles, so the lean sweeps THROUGH the corners
# instead of the left flank's angle meeting the top's at a diagonal.
cap_sections = [case_wire(FRAME_IN_Y, EDGE_R + FRAME)]

# Then the outer rim, rolled at EDGE_R: at the frame the surface faces the
# viewer, at the flank it runs parallel to it. Stepped rather than splined
# because a ruled loft through sections this close is predictable, and
# predictable is worth more here than exact -- under flat shading a few facets
# across a five millimeter roll read as round.
for step in range(CAP_STEPS):
    angle = (step / float(CAP_STEPS - 1)) * math.pi * 0.5

    cap_sections.append(case_wire(-EDGE_R * math.cos(angle),
                                  EDGE_R * (1.0 - math.sin(angle))))

# ...AND THE TUBE HOUSING IS THE SAME LOFT, not a second solid unioned onto
# it. The roll's last section and the housing's first are the same rectangle
# in the same plane, and abutting two solids on coincident faces gives OCC an
# invalid result -- it unions without complaint and then every boolean after
# it quietly misbehaves. The tell was a cut that RAISED the volume. Run the
# section straight back instead and there is one solid, made once.
cap_sections.append(case_wire(FRONT_D, 0.0))

shell = cq.Workplane(obj=cq.Solid.makeLoft(cap_sections, True))

# The tube housing's own back rim, rounded BEFORE the box goes on. Selecting
# it afterwards means asking for every edge near that plane, which by then is
# two concentric rings -- the rim and the seam the box cuts into it -- and
# filleting the pair returns a solid OCC reports as INVALID. Nothing throws;
# the booleans after it just quietly stop working, and the screen recess ends
# up not cut. Fillet it while it is still the only thing there.
shell = shell.edges(">Y").fillet(EDGE_R)

# The electronics box behind it. It overlaps well into the housing, so the
# union has no coincident faces to trip over.
shell = shell.union(
    cq.Workplane(obj=cq.Solid.extrudeLinear(
        cq.Face.makeFromWires(round_rect_wire(REAR_Y0, REAR_X0, REAR_X0 + REAR_W,
                                              REAR_Z0, REAR_Z1, R_REAR)),
        cq.Vector(0.0, D - REAR_Y0, 0.0))))

# And the box's own back rim.
shell = shell.edges(">Y").fillet(EDGE_R)

# THE BEZEL, cut off the frame's inner edge: a tangent ARC out of the frame's
# lean and into the steeper run down to the tube, so the two bands meet on a
# curve rather than a crease.
#
# Solved in the (inset, depth) section rather than guessed. The frame runs
# inward and FORWARD at FRAME_ANG; the bezel runs inward and BACK at BEZ_ANG;
# the turn between them is the sum, and a fillet of CREASE_R tangent to both
# starts a tangent length back along the frame -- which is why the cut has to
# begin OUTBOARD of the corner and bite into the frame. That is what rolling a
# convex edge means, and no amount of filleting after the fact does it.
_corner  = (EDGE_R + FRAME, FRAME_IN_Y)
_turn    = FRAME_ANG + BEZ_ANG
_tangent = CREASE_R * math.tan(_turn * 0.5)

# Unit directions, going inward, in (inset, depth).
_fdir = (math.cos(FRAME_ANG), -math.sin(FRAME_ANG))
_bdir = (math.cos(BEZ_ANG), math.sin(BEZ_ANG))

# The arc's center is EDGE_R off the frame along its inward normal.
_p1     = (_corner[0] - _tangent * _fdir[0], _corner[1] - _tangent * _fdir[1])
_center = (_p1[0] + CREASE_R * math.sin(FRAME_ANG),
           _p1[1] + CREASE_R * math.cos(FRAME_ANG))
_phi0   = math.atan2(_p1[1] - _center[1], _p1[0] - _center[0])

bez_sections = [case_wire(FRAME_IN_Y - 8.0, _p1[0])]

for step in range(CREASE_STEPS):
    phi = _phi0 + _turn * (step / float(CREASE_STEPS - 1))

    bez_sections.append(case_wire(_center[1] + CREASE_R * math.sin(phi),
                                  _center[0] + CREASE_R * math.cos(phi)))

bez_sections.append(round_rect_wire(OPEN_Y, OX0, OX1, OZ0, OZ1, R_OPEN))

shell = shell.cut(cq.Workplane(obj=cq.Solid.makeLoft(bez_sections, True)))

# The throat, opened out to clear the tube: the glass now runs past the mouth,
# and a sheet inside solid case is a sheet fighting the case for depth.
shell = shell.cut(cq.Workplane(obj=cq.Solid.extrudeLinear(
    cq.Face.makeFromWires(round_rect_wire(OPEN_Y,
                                          OX0 - GLASS_OVER - 1.0, OX1 + GLASS_OVER + 1.0,
                                          OZ0 - GLASS_OVER - 1.0, OZ1 + GLASS_OVER + 1.0,
                                          R_OPEN)),
    cq.Vector(0.0, 46.0, 0.0))))

# ------------------------------------------------------------------ louvers

# The shared gap unit, and the run every vent makes: from one gap behind the
# step to the terminating ring one gap in from the box's rear plane.
G_UNIT   = (REAR_H - VENT_N_SIDE * LOUV_W) / (VENT_N_SIDE + 1)
VENT_Y0  = FRONT_D + G_UNIT
VENT_Y1  = D - G_UNIT
VENT_RUN = VENT_Y1 - VENT_Y0


def vent_positions(span, count):
    """Line start offsets for `count` lines and count+1 equal gaps across
    `span`, which is what centers the set by construction."""
    gap = (span - count * LOUV_W) / (count + 1)

    return [gap + i * (LOUV_W + gap) for i in range(count)]


# The flanks: eleven lines each, centered on the box's height.
for dz in vent_positions(REAR_H, VENT_N_SIDE):
    for x in (REAR_X0 - 1.0, REAR_X0 + REAR_W - LOUV_DEEP + 1.0):
        shell = shell.cut(
            cq.Workplane("XY")
              .box(LOUV_DEEP + 1.0, VENT_RUN, LOUV_W, centered=(False, False, False))
              .translate((x, VENT_Y0, REAR_Z0 + dz)))

# The lid and the underside: sixteen lines each, centered on the box's width.
for dx in vent_positions(REAR_W, VENT_N_LID):
    for z in (REAR_Z1 - LOUV_DEEP, REAR_Z0 - 1.0):
        shell = shell.cut(
            cq.Workplane("XY")
              .box(LOUV_W, VENT_RUN, LOUV_DEEP + 1.0, centered=(False, False, False))
              .translate((REAR_X0 + dx, VENT_Y0, z)))

# THE TERMINATING RING: one continuous groove around the box's circumference,
# which the vents run into and end at. Cut the way the drive's seam is -- an
# outer slab minus an inner rounded prism -- so the groove follows the box's
# rounded corners instead of breaking at them.
shell = shell.cut(
    cq.Workplane("XY")
      .box(REAR_W + 20.0, LOUV_W, REAR_H + 20.0, centered=(False, False, False))
      .translate((REAR_X0 - 10.0, VENT_Y1 - LOUV_W, REAR_Z0 - 10.0))
      .cut(cq.Workplane(obj=cq.Solid.extrudeLinear(
               cq.Face.makeFromWires(round_rect_wire(
                   VENT_Y1 - LOUV_W - 1.0,
                   REAR_X0 + LOUV_DEEP, REAR_X0 + REAR_W - LOUV_DEEP,
                   REAR_Z0 + LOUV_DEEP, REAR_Z1 - LOUV_DEEP,
                   R_REAR - LOUV_DEEP)),
               cq.Vector(0.0, LOUV_W + 2.0, 0.0)))))

# ...AND THE FRONT MASS'S OWN RING, a gap and a half forward of its rear
# face -- the one line the big smooth housing carries.
shell = shell.cut(
    cq.Workplane("XY")
      .box(W + 20.0, LOUV_W, H + 20.0, centered=(False, False, False))
      .translate((-10.0, FRONT_D - 1.5 * G_UNIT - LOUV_W, -10.0))
      .cut(cq.Workplane(obj=cq.Solid.extrudeLinear(
               cq.Face.makeFromWires(round_rect_wire(
                   FRONT_D - 1.5 * G_UNIT - LOUV_W - 1.0,
                   LOUV_DEEP, W - LOUV_DEEP,
                   LOUV_DEEP, H - LOUV_DEEP,
                   R_CASE - LOUV_DEEP)),
               cq.Vector(0.0, LOUV_W + 2.0, 0.0)))))

# THE CARRYING HANDLE, sunk into the front mass's rear face near its crown --
# the strip of case the pocket leaves above itself is the grip. Rear box and
# pocket never meet: the box tops out at REAR_Z1 and the pocket starts above
# it, on the band of rear face the centered box leaves exposed.
shell = shell.cut(
    cq.Workplane("XY")
      .box(GRIP_W, GRIP_DEEP + 4.0, GRIP_H, centered=(False, False, False))
      .translate(((W - GRIP_W) * 0.5, FRONT_D - GRIP_DEEP, GRIP_Z0))
      .edges("|Y").fillet(6.0))

# THE REAR PANEL, from a photograph of a real unit's back. Viewed from the
# REAR -- which is +y looking forward, so model x runs right-to-left in that
# view -- the order is: mains inlet, vertical size, vertical position,
# brightness, composite video in. The three trims are PROTRUDING knobs, the
# video input is an RCA jack, and the inlet is the standard PC receptacle: a
# recessed rounded rectangle with its top corners clipped at forty-five
# degrees, holding three male blades, LOW - HIGH - LOW.
#
# Under each control, its function engraved -- the Monitor II's mold-relief
# strokes INVERTED: the same one-millimeter strokes, cut half a millimeter
# INTO the panel instead of standing off it, so they read by the shadow they
# hold rather than the highlight they catch.
# THE WHOLE BAY IS SUNK A FULL INCH into the back of the case, and the knobs
# stand a centimeter off its floor -- so nothing reaches the rear plane, and
# the monitor sets flat against a wall without a control touching it.
PANEL_X0, PANEL_X1 = REAR_X0 + 14.0, REAR_X0 + REAR_W - 14.0
PANEL_Z0, PANEL_Z1 = REAR_Z0 + 8.0, REAR_Z0 + 52.0
PANEL_DEEP         = INCH
KNOB_PROUD         = 10.0               # off the bay floor, still inside the bay
PANEL_Y            = D - PANEL_DEEP     # the bay floor everything sits on

CTRL_Z  = PANEL_Z0 + 30.0               # the control row's center line
ICON_CZ = PANEL_Z0 + 11.0               # the engraved row's center line
ICON_S  = 7.0                           # icon box side
STROKE  = 1.0                           # engraved stroke width (RIDGE_W's twin)
CUT_D   = 0.5                           # ...and depth (RIDGE_H's)

# Rear-view left to right; model x descends.
AC_CX     = PANEL_X1 - 26.0
KNOB_CX   = [AC_CX - 42.0, AC_CX - 68.0, AC_CX - 94.0]
RCA_CX    = AC_CX - 122.0

# The bay itself.
shell = shell.cut(
    cq.Workplane("XY")
      .box(PANEL_X1 - PANEL_X0, PANEL_DEEP + 1.0, PANEL_Z1 - PANEL_Z0,
           centered=(False, False, False))
      .translate((PANEL_X0, PANEL_Y, PANEL_Z0))
      .edges("|Y").fillet(3.0))

# THE MAINS INLET: the receptacle profile drawn as it is -- a rounded
# rectangle whose top two corners are clipped at forty-five degrees -- and
# cut eight millimeters into the bay floor.
AC_W, AC_H, AC_CLIP, AC_DEEP = 24.0, 17.0, 5.0, 8.0

_acx0, _acx1 = AC_CX - AC_W * 0.5, AC_CX + AC_W * 0.5
_acz0, _acz1 = CTRL_Z - AC_H * 0.5, CTRL_Z + AC_H * 0.5

_ac_face = cq.Face.makeFromWires(cq.Wire.makePolygon([
    cq.Vector(_acx0, PANEL_Y + 0.5, _acz0),
    cq.Vector(_acx1, PANEL_Y + 0.5, _acz0),
    cq.Vector(_acx1, PANEL_Y + 0.5, _acz1 - AC_CLIP),
    cq.Vector(_acx1 - AC_CLIP, PANEL_Y + 0.5, _acz1),
    cq.Vector(_acx0 + AC_CLIP, PANEL_Y + 0.5, _acz1),
    cq.Vector(_acx0, PANEL_Y + 0.5, _acz1 - AC_CLIP),
    cq.Vector(_acx0, PANEL_Y + 0.5, _acz0),
]))

shell = shell.cut(cq.Workplane(obj=cq.Solid.extrudeLinear(
    _ac_face, cq.Vector(0.0, -(AC_DEEP + 0.5), 0.0))))

# Its three blades, standing up from the receptacle's back wall: LOW, HIGH,
# LOW, which is the earth pin above the pair of line blades.
for dx, dz in ((-6.0, -2.5), (0.0, 3.0), (6.0, -2.5)):
    m.add(f"acpin{int(dx)}",
          cq.Workplane("XY")
            .box(2.0, 5.0, 4.5, centered=(True, False, False))
            .translate((AC_CX + dx, PANEL_Y - AC_DEEP, CTRL_Z + dz - 2.25)),
          (0.10, 0.10, 0.10))

# THE THREE KNOBS, protruding case-colored cylinders: size, position,
# brightness.
for i, cx in enumerate(KNOB_CX):
    m.add(f"knob{i}",
          cq.Workplane("XY")
            .cylinder(KNOB_PROUD, 4.2, direct=(0, 1, 0), centered=(True, True, False))
            .translate((cx, PANEL_Y, CTRL_Z)),
          PLAT)

# THE RCA JACK: dark barrel, white insulator ring, dark center bore.
m.add("rca_body",
      cq.Workplane("XY")
        .cylinder(4.5, 4.6, direct=(0, 1, 0), centered=(True, True, False))
        .translate((RCA_CX, PANEL_Y, CTRL_Z)),
      (0.12, 0.12, 0.13))
m.add("rca_ring",
      cq.Workplane("XY")
        .cylinder(5.1, 3.0, direct=(0, 1, 0), centered=(True, True, False))
        .translate((RCA_CX, PANEL_Y, CTRL_Z)),
      (0.92, 0.91, 0.89))
m.add("rca_bore",
      cq.Workplane("XY")
        .cylinder(5.5, 1.3, direct=(0, 1, 0), centered=(True, True, False))
        .translate((RCA_CX, PANEL_Y, CTRL_Z)),
      (0.10, 0.10, 0.11))


# ------------------------------------------------- the engraved icon row
#
# Each cutter is a stroke solid exactly as the Monitor II builds its relief,
# subtracted instead of added. Cuts land in the bay floor at PANEL_Y.

def engrave(solid):
    """Cut one stroke into the bay floor."""
    global shell
    shell = shell.cut(solid)


def stroke_box(cx, cz, w, h, rot_deg=0.0):
    """A stroke cutter centered at (cx, cz) on the panel, optionally rotated
    about its own y-axis center -- rays and slashes come from here."""
    s = (cq.Workplane("XY")
         .box(w, CUT_D + 0.5, h, centered=(True, False, True))
         .translate((0.0, PANEL_Y - CUT_D, 0.0)))

    if rot_deg != 0.0:
        s = s.rotate((0, 0, 0), (0, 1, 0), rot_deg)

    return s.translate((cx, 0.0, cz))


def outline_ring(cx, cz, w, h, r):
    """A rounded-rectangle OUTLINE cutter: outer minus inner."""
    y = PANEL_Y - CUT_D

    outer = cq.Solid.extrudeLinear(
        cq.Face.makeFromWires(round_rect_wire(y, cx - w * 0.5, cx + w * 0.5,
                                              cz - h * 0.5, cz + h * 0.5, r)),
        cq.Vector(0.0, CUT_D + 0.5, 0.0))
    inner = cq.Solid.extrudeLinear(
        cq.Face.makeFromWires(round_rect_wire(y - 0.1,
                                              cx - w * 0.5 + STROKE, cx + w * 0.5 - STROKE,
                                              cz - h * 0.5 + STROKE, cz + h * 0.5 - STROKE,
                                              max(0.4, r - STROKE))),
        cq.Vector(0.0, CUT_D + 0.7, 0.0))

    return cq.Workplane(obj=outer).cut(cq.Workplane(obj=inner))


# Mains: the alternating-current tilde, three joined slashes.
engrave(stroke_box(AC_CX - 1.8, ICON_CZ - 0.5, STROKE, 3.4, 35.0))
engrave(stroke_box(AC_CX,       ICON_CZ + 0.4, STROKE, 3.4, -35.0))
engrave(stroke_box(AC_CX + 1.8, ICON_CZ - 0.5, STROKE, 3.4, 35.0))

# Vertical size: a screen with arrowheads pushing its top and bottom out.
engrave(outline_ring(KNOB_CX[0], ICON_CZ, ICON_S, ICON_S * 0.8, 1.6))
engrave(stroke_box(KNOB_CX[0], ICON_CZ, STROKE, 3.2))
engrave(stroke_box(KNOB_CX[0], ICON_CZ + 1.5, 2.2, STROKE))
engrave(stroke_box(KNOB_CX[0], ICON_CZ - 1.5, 2.2, STROKE))

# Vertical position: the same screen with the bar swept to one side.
engrave(outline_ring(KNOB_CX[1], ICON_CZ, ICON_S, ICON_S * 0.8, 1.6))
engrave(stroke_box(KNOB_CX[1], ICON_CZ, STROKE, 3.6, 20.0))

# Brightness: the sun -- a core and eight rays.
engrave(stroke_box(KNOB_CX[2], ICON_CZ, 2.4, 2.4))
for k in range(8):
    ang = k * 45.0
    rx  = KNOB_CX[2] + 3.0 * math.cos(math.radians(ang))
    rz  = ICON_CZ + 3.0 * math.sin(math.radians(ang))
    engrave(stroke_box(rx, rz, 0.8, 1.6, ang + 90.0))

# Video in: a screen inside a frame.
engrave(outline_ring(RCA_CX, ICON_CZ, ICON_S, ICON_S, 0.8))
engrave(outline_ring(RCA_CX, ICON_CZ, ICON_S - 2.6, ICON_S - 3.2, 1.4))

m.add("shell", shell, PLAT, angular=CORNER_ANG)

# Cavity back: the dark plane behind the glass.
#
# THE MOUTH'S OWN FOOTPRINT, rounded corners and all -- not a plain box.
#
# It was a square-cornered rectangle, and a square corner inside a rounded
# mouth sticks out past it by r(sqrt2 - 1): two and a half millimeters even at
# the opening's own size, and five once the box was widened to follow the
# glass. That surplus is what showed as dark wedges biting into the bezel at
# the bottom corners -- the backing plate, not the tube, and visible in each
# corner because that is the only place a square outline can outrun a round
# one.
#
# There is nothing to widen it for, either: the cavity has to back what can be
# SEEN, and what can be seen is the mouth. The glass's overhang is behind the
# lip and needs no backing at all.
m.add("cavity",
      cq.Workplane(obj=cq.Solid.extrudeLinear(
          cq.Face.makeFromWires(round_rect_wire(GLASS_Y + 1.0,
                                                OX0, OX1, OZ0, OZ1, R_OPEN)),
          cq.Vector(0.0, 2.0, 0.0))),
      CAVITY)

# --------------------------------------------------------------------- glass

# True spherical sag -- the period 9-inch tube's curvature, and the sphere the
# scene derives its input mapping from. Inset far enough to stay inside the
# opening's rounded corners.
m.add_triangles("glass",
                sag_sheet(OX0 + GLASS_IN, OX1 - GLASS_IN,
                          OZ0 + GLASS_IN, OZ1 - GLASS_IN,
                          front_y=GLASS_Y, radius_scale=SAG_SCALE),
                KD["glass"])

# --------------------------------------------------------------- power lamp

# The //c-family indicator: a tall narrow rhombus leaning right at the switch
# bar's proportions (kLedWDp 8 : kLedHDp 25) and lean (tan 0.176), extruded,
# seated in a hairline recess. Analytic triangles rather than a CAD solid --
# a sheared prism fights every workplane, and ten quads state it exactly.
#
# ALL BUT FLUSH, like the drive's: it is a matte window in the panel, not a
# bulb standing off it. The standoff the shading needs belongs to the light
# and lives there -- see kLampLightStandoffMm.


def slanted_prism(x, z0, height, width, lean, y_front, y_back):
    z1 = z0 + height
    face = [(x, z0), (x + width, z0), (x + width + lean, z1), (x + lean, z1)]
    tris = []
    f3 = [(px, y_front, pz) for px, pz in face]
    tris.append((f3[0], f3[1], f3[2]))
    tris.append((f3[0], f3[2], f3[3]))
    for i in range(4):
        ax, az = face[i]
        bx, bz = face[(i + 1) % 4]
        tris.append(((ax, y_front, az), (bx, y_front, bz), (bx, y_back, bz)))
        tris.append(((ax, y_front, az), (bx, y_back, bz), (ax, y_back, az)))
    return tris


# THE MARKS SIT ON THE FRAME, WHICH LEANS. Anchored at their middle they would
# sink into the slope above that line, and a case that eats half a stamp is
# worse than one the stamp stands a fraction off -- so both are hung from the
# surface at their own TOP edge, the proud end, and float a hair at the
# bottom. Head on, which is how this scene is seen, the plate hides its own
# gap.
MARK_Z0, MARK_Z1 = 6.5, 19.5


def frame_y_at(inset):
    """The frame's surface depth at a given inset from the case outline."""
    return FRAME_OUT_Y - (inset - EDGE_R) * math.tan(FRAME_ANG)


PLATE_Y = frame_y_at(MARK_Z1)     # the proud end of the marks' band

# THE INDICATOR IS THE DISK IIc's, part for part -- same 2.0 by 8.0 lens, same
# 2.4 of lean, same 0.35 proud. They are the same indicator on the same
# machine and there is no reason for the monitor to have its own dimensions
# for it. The drive's has no recess ring either, so this one loses the one it
# had.
#
# RIGHT-ALIGNED WITH THE TUBE: the lens's rightmost point -- the top of the
# lean, not the bottom -- lands on the opening's right edge.
LENS_W, LENS_H = 2.0, 8.0
LEAN           = 2.4

# FLUSH, like the drive's -- and six hundredths rather than zero for the same
# reason: coincident faces are a z-fight, not a flush fit.
LENS_PROUD     = 0.06

LCZ = MARK_Z0 + (MARK_Z1 - MARK_Z0 - LENS_H) * 0.5
LCX = OX1 - (LENS_W + LEAN)

m.add_triangles("lamp",
                slanted_prism(LCX, LCZ, LENS_H, LENS_W, LEAN,
                              frame_y_at(LCZ + LENS_H) - LENS_PROUD,
                              frame_y_at(LCZ) + 1.0),
                KD["monitor_lamp"])


# --------------------------------------------------------------- front anchor
#
# The FRAME's front plane -- the case's own front section, behind the proud
# plate that carries the screen. The drives line up with this, not with the
# plate and not with the model origin.
m.add("front_anchor",
      cq.Workplane("XY")
        .box(0.4, 0.4, 0.4, centered=(True, True, True))
        .translate((W * 0.5, 0.0, H * 0.25)),
      KD["front_anchor"])


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Monitor2c.mesh"),
                    os.path.join(out, "Monitor2c.mtl"), "Monitor2c.mtl")
    print(f"Monitor2c (CAD): {nv} verts, {nt} tris")
