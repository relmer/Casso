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
    guided into it by hand.

The case is TWO HALVES meeting at the plane of the slot, with a fine gap
between them running the whole way round, and the lower half tapers inward to
its base on the left and right only. Feet run across the front and the back
rather than sitting at the corners, with a rubber pad at each rounded end.

Sub-mesh identity is by part NAME: `lever` and `tab` are the door assembly the
scene moves on eject, `lamp` is the activity indicator, `slot` the opening.
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

# A generous round on every edge -- an eighth of an inch. Molded ABS of this
# era has no crisp arrises anywhere, and too tight a radius is most of what
# made earlier passes read as a rendering rather than a thing.
EDGE_R = INCH / 8.0
CORNER_R = 4.5                    # the vertical corners, which are softer still

# Warm off-white, not the //e era's platinum: the photographs show a case that
# matches the //c's own cream.
CASE    = (0.884, 0.874, 0.846)
SLOT_DK = (0.055, 0.055, 0.062)
FOOT    = (0.320, 0.310, 0.300)
PAD     = (0.180, 0.176, 0.170)

# The latch is GRAY -- a //c keycap's gray, plainly a different part from the
# case rather than a tint of it. Made a shade off the cream it vanished into
# the front, which is the opposite of what it is: the one thing on this face
# you are meant to find with a thumb.
LATCH   = (0.700, 0.692, 0.668)
GRIP    = (0.655, 0.648, 0.625)

RAINBOW = [(0.20, 0.65, 0.27), (0.98, 0.80, 0.08), (0.96, 0.51, 0.12),
           (0.91, 0.18, 0.14), (0.58, 0.25, 0.60), (0.17, 0.45, 0.85)]

# ------------------------------------------------------------------ the front

# WHERE THE HALVES MEET decides the whole front, because the slot sits on
# that line and everything else is placed off the slot. The top shell is half
# the height of the bottom one, so the split lands two thirds up -- written
# as the fraction, because a literal 30.667 is a number nobody can check.
SPLIT_Z = H * 2.0 / 3.0

SLOT_Z0, SLOT_Z1 = SPLIT_Z, SPLIT_Z + 4.5    # the disk opening, on the seam
SLOT_X0, SLOT_X1 = 16.0, W - 16.0
SLOT_BEVEL       = 2.0            # the 45-degree lead-in, all four sides

# A THIRD OF THE SLOT, which divides the opening into three equal parts:
# segment, latch, segment. Written as the fraction rather than the answer, so
# the proportion survives the slot being resized.
LATCH_W      = (SLOT_X1 - SLOT_X0) / 3.0
LATCH_X0     = (W - LATCH_W) * 0.5
LATCH_PROUD  = 1.5                # how far it stands off the face
LATCH_SUNK   = 3.0                # how far it reaches back into the notch

# The notch is a little wider than the latch so the latch travels freely in
# it, and it runs from below the slot up over the top and back along the lid.
#
# It is DEEPER than the latch is thick, and deliberately: pressing eject
# pushes the latch back into the drive before it rises, and the difference
# between these two numbers is the room that press has to happen in. Sized to
# LATCH_TRAVEL_IN so the two cannot drift apart -- the pair of them being one
# number apiece in two files is how the latch came to travel through the back
# of its own notch.
LATCH_TRAVEL_IN = 4.0             # mirrored by kDisk2cDoorInMm
NOTCH_W      = LATCH_W + 4.0
NOTCH_X0     = (W - NOTCH_W) * 0.5
NOTCH_Z0     = SLOT_Z0 - 12.0     # its floor, well below the slot
NOTCH_D      = LATCH_SUNK + LATCH_TRAVEL_IN + 1.0
NOTCH_LID    = 16.0               # how far back along the lid
NOTCH_LID_D  = 4.0                # how far down into the lid

# The indicator. A slash, because that is the glyph Apple molded there and the
# same one the Monitor //c wears beside its lamp.
#
# ITS LENS IS ALL BUT FLUSH, because that is what the part is: a matte plastic
# window sitting in the front face, not a domed bulb standing off it. The
# earlier 1 mm standoff was there to make the LIGHT work -- the shader weighs
# each surface by dot(n, L), so a source in the plane of the panel lights none
# of it -- which was solving a lighting problem with geometry. The standoff
# belongs to the light and now lives there; see kLampLightStandoffMm.
LAMP_W, LAMP_H = 2.0, 8.0
LAMP_X, LAMP_Z = W - 24.0, 11.0
LAMP_LEAN      = 2.4
LAMP_PROUD     = 0.35

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

# --------------------------------------------------------------------- the lid

RIB_N     = 19
RIB_W     = 2.4
RIB_DEEP  = 1.1
RIB_X0    = 22.0
RIB_X1    = W - 14.0
RIB_Y0    = 40.0                  # clear of the front's plain band
RIB_Y1    = D - 20.0

# ------------------------------------------------------------------- the feet
#
# Two of them, across the front and the back, not four at the corners: a
# rectangle with a half-round at each end, extruded. A quarter inch tall, half
# an inch deep, set half an inch in from the face it is nearest and from both
# sides.
FOOT_H     = INCH / 4.0
FOOT_D     = INCH / 2.0
FOOT_INSET = INCH / 2.0
FOOT_R     = FOOT_D / 2.0
PAD_R      = 5.0                  # inside the half-round's own circumference
PAD_H      = 1.2

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
          .edges("|Z").fillet(RIB_W * 0.45))

# A grille along the TOP edge of the back face. Never seen in this scene, and
# in the model because the drive has one.
for i in range(20):
    x = 14.0 + (W - 28.0 - 2.0) * i / 19.0
    case = case.cut(
        cq.Workplane("XY")
          .box(2.0, 6.0, H * 0.30, centered=(False, False, False))
          .translate((x, D - 3.0, H * 0.58)))

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
case = case.cut(
    cq.Workplane("XY")
      .box(W + 20.0, D + 22.5, SEAM_GAP, centered=(False, False, False))
      .translate((-10.0, -12.5, SPLIT_Z - SEAM_GAP))
      .cut(cq.Workplane("XY")
             .box(W - SEAM_DEEP * 2.0, (D + 2.5) - SEAM_DEEP * 2.0, SEAM_GAP + 2.0,
                  centered=(False, False, False))
             .translate((SEAM_DEEP, -2.5 + SEAM_DEEP, SPLIT_Z - SEAM_GAP - 1.0))))

m.add("case", case, CASE)

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
latch = (cq.Workplane("XY")
         .box(LATCH_W, LATCH_PROUD + LATCH_SUNK, H - SLOT_Z0, centered=(False, False, False))
         .translate((LATCH_X0, -2.5 - LATCH_PROUD, SLOT_Z0))
         .edges("|Y").fillet(1.6))

# Its top runs back along the lid in the notch's upper leg, which is what a
# thumb pushes on.
latch = latch.union(
    cq.Workplane("XY")
      .box(LATCH_W, NOTCH_LID - 2.0, NOTCH_LID_D + 0.6, centered=(False, False, False))
      .translate((LATCH_X0, -2.5, H - NOTCH_LID_D))
      .edges("|Z").fillet(1.6))

m.add("lever", latch, LATCH)

# The thumb pad on its front. Placed and sized off the strip of latch that is
# actually ABOVE the slot, because that is all there is to put it on -- pinned
# to a fixed offset it ran off the top of the drive the moment the split
# moved.
TAB_Z0 = SLOT_Z1 + 1.0
m.add("tab",
      cq.Workplane("XY")
        .box(LATCH_W - 8.0, 1.2, (H - 2.0) - TAB_Z0, centered=(False, False, False))
        .translate((LATCH_X0 + 4.0, -2.5 - LATCH_PROUD - 1.2, TAB_Z0))
        .edges("|Y").fillet(1.0),
      GRIP)

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

# The rainbow, on the LID at the rear-left, where the photographs put the
# logo. Under the monitor in this scene's stack and so rarely seen, which is
# not a reason to put it somewhere it is not.
for i, c in enumerate(RAINBOW):
    m.add(f"rb{i}",
          cq.Workplane("XY").box(13.0, 2.2, 0.5, centered=(False, False, False))
            .translate((10.0, D - 30.0 + i * 2.2, H - 0.2)),
          c)

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

    m.add(f"foot_{name}", foot, FOOT)


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Disk2c.mesh"),
                    os.path.join(out, "Disk2c.mtl"), "Disk2c.mtl")
    print(f"Disk2c (CAD): {nv} verts, {nt} tris")
