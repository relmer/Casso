"""Apple Disk IIc (A2M4050Z) as a real solid model. The low off-white unit
styled to match the //c, 152 x 182 x 70 mm (W x D x H). X right, Y back,
Z up; front face at y = -2.5.

MODELED FROM PHOTOGRAPHS -- see specs/018-3d-desk-scene/reference/README.md,
which names them and says what each one settles. The shape this replaces
was a generic low box with a full-width slot and a lever under it, which is
not what the drive looks like at all:

  - the LID IS A RIB FIELD, nineteen long grooves running front to back over
    most of it. Plain, the lid is the one surface that could belong to any
    beige box of the period; ribbed, it is a Disk IIc from across the room.
  - the SLOT IS TWO SEGMENTS, split by a central latch that stands proud of
    the face and rises up over the lid's leading edge. A full-width slot with
    a lever below it is a Disk II's arrangement wearing the wrong colors.
  - a FINGER RECESS steps back under the latch.
  - the `/` glyph marks the indicator at the lower right, the same mark the
    Monitor //c carries beside its lamp.
  - the LOGO IS ON THE LID at the rear-left, not on the front.

Sub-mesh identity is by part NAME: `lever` and `tab` are the door assembly the
scene swings on eject, `lamp` is the activity indicator, `slot` the opening.
Their colors are therefore free to be the finish the photographs show rather
than the marker values they used to have to carry -- which is why the latch is
a shade off the case here instead of the much darker platinum-era marker.
"""

import cadquery as cq
from cadkit import KD, Model

# ---------------------------------------------------------------- dimensions
#
# STILL UNMEASURED, but no longer left at a figure the evidence contradicts.
# Two underside photographs, both near enough rectified to read an aspect
# from, put the case between 1.18 and 1.25 times as deep as it is wide. It
# was 1.42 -- roughly a fifth too long, which is the kind of error that reads
# as "something about this is off" without ever looking like a mistake.
#
# 1.20 splits them. It is an ESTIMATE off photographs of unknown scale, which
# is exactly the move that earns confident wrong numbers, so it is written
# down as one: the Disk II's dimensions were only ever right once somebody
# put a tape on one, and this wants the same.
#
# Sanity check it survives: a 5.25 inch diskette is 133.35 mm square and has
# to go all the way in, leaving about 49 mm behind it for spindle, motor and
# board. Tight, and possible. At the old 216 there was 83 mm back there,
# which is more room than the mechanism has any use for.
W, H = 152.0, 70.0
D    = W * 1.20

# Warm off-white, not the //e era's platinum: the photographs show a case that
# matches the //c's own cream, and the latch a cool gray against it.
CASE    = (0.884, 0.874, 0.846)
CASE_DK = (0.812, 0.803, 0.778)
SLOT_DK = (0.055, 0.055, 0.062)
FOOT    = (0.320, 0.310, 0.300)

# The latch is a SHADE cooler than the case, not a different part in a
# different material. Identity is by part NAME now, so these are free to be
# the finish the photographs show rather than the marker colors they were.
LATCH   = (0.845, 0.836, 0.812)
GRIP    = (0.806, 0.798, 0.774)

RAINBOW = [(0.20, 0.65, 0.27), (0.98, 0.80, 0.08), (0.96, 0.51, 0.12),
           (0.91, 0.18, 0.14), (0.58, 0.25, 0.60), (0.17, 0.45, 0.85)]

# ------------------------------------------------------------------ the front

SLOT_Z0, SLOT_Z1 = 40.0, 45.0     # the disk opening
SLOT_X0, SLOT_X1 = 16.0, W - 16.0

LATCH_W      = 30.0               # the central block that splits the slot
LATCH_X0     = (W - LATCH_W) * 0.5
LATCH_Z0     = SLOT_Z0 - 13.0
LATCH_PROUD  = 4.2                # how far it stands off the face
LATCH_R      = 2.0

RECESS_W     = 44.0               # the finger pull under the latch
RECESS_H     = 11.0
RECESS_D     = 3.0

# The indicator. A slash, because that is the glyph Apple molded there and the
# same one the Monitor //c wears beside its lamp -- a round pilot light would
# be a different machine's idea. It stands proud for the reason every lamp in
# this scene does: the shader weighs each surface by dot(n, L), so a lens in
# the plane of the panel lights nothing.
LAMP_W, LAMP_H = 2.2, 11.0
LAMP_X, LAMP_Z = W - 26.0, 14.0
LAMP_LEAN      = 3.2              # the slash's top-right offset
LAMP_PROUD     = 1.0

# --------------------------------------------------------------------- the lid

RIB_N     = 19
RIB_W     = 2.4
RIB_DEEP  = 1.1
RIB_X0    = 22.0
RIB_X1    = W - 14.0
RIB_Y0    = 42.0                  # clear of the front's plain band
RIB_Y1    = D - 14.0

m = Model()

# --------------------------------------------------------------------- case

# Vertical corners first, then the whole top rim in one pass. Filleting the
# three edge directions in turn asks OCC to round edges the earlier rounds
# already consumed, and it refuses the lot rather than the one.
case = (cq.Workplane("XY")
        .box(W, D + 2.5, H, centered=(False, False, False))
        .translate((0.0, -2.5, 0.0))
        .edges("|Z").fillet(4.5))

case = case.edges(">Z").fillet(2.0)

# The lid's ribs: long shallow grooves front to back. Cut rather than laid on,
# so each one has walls that take the light differently from the lid between
# them -- painted stripes read as two tones of the same flat surface.
for i in range(RIB_N):
    x = RIB_X0 + (RIB_X1 - RIB_X0 - RIB_W) * i / float(RIB_N - 1)
    case = case.cut(
        cq.Workplane("XY")
          .box(RIB_W, RIB_Y1 - RIB_Y0, RIB_DEEP + 2.0, centered=(False, False, False))
          .translate((x, RIB_Y0, H - RIB_DEEP))
          .edges("|Z").fillet(RIB_W * 0.45))

# The finger recess under the latch, and the slot itself, both cut from the
# front. The slot is ONE cut -- the latch standing in front of it is what
# makes it read as two.
case = case.cut(
    cq.Workplane("XY")
      .box(RECESS_W, RECESS_D + 2.0, RECESS_H, centered=(False, False, False))
      .translate(((W - RECESS_W) * 0.5, -2.5, LATCH_Z0 - RECESS_H))
      .edges("|Y").fillet(2.0))

# THE SLOT IS A HOLE, so the case has to give it up. A dark solid left sitting
# inside solid case is buried, not an opening -- the lesson the Disk II's slot
# taught twice.
case = case.cut(
    cq.Workplane("XY")
      .box(SLOT_X1 - SLOT_X0, 14.0, SLOT_Z1 - SLOT_Z0, centered=(False, False, False))
      .translate((SLOT_X0, -3.0, SLOT_Z0)))

m.add("case", case, CASE)

# ------------------------------------------------- faceplate furniture

# The slot: a dark void behind the face, spanning the full opening. The latch
# sits in front of its middle.
m.add("slot",
      cq.Workplane("XY")
        .box(SLOT_X1 - SLOT_X0, 12.0, SLOT_Z1 - SLOT_Z0, centered=(False, False, False))
        .translate((SLOT_X0, -0.4, SLOT_Z0)),
      SLOT_DK)

# The latch. It carries the door identity, so this is the piece the scene
# swings on eject -- and it rises over the lid's leading edge, which is what
# gives it something to be gripped by.
latch = (cq.Workplane("XY")
         .box(LATCH_W, LATCH_PROUD + 2.5, (H + 1.5) - LATCH_Z0, centered=(False, False, False))
         .translate((LATCH_X0, -2.5 - LATCH_PROUD, LATCH_Z0))
         .edges("|Y").fillet(LATCH_R))

m.add("lever", latch, LATCH)

# The grip: a shallower step across the latch's lower half, which is the part
# a thumb actually bears on.
m.add("tab",
      cq.Workplane("XY")
        .box(LATCH_W - 6.0, 1.4, 13.0, centered=(False, False, False))
        .translate((LATCH_X0 + 3.0, -2.5 - LATCH_PROUD - 1.4, LATCH_Z0 + 3.0))
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

# Vent slots, the same field as the lid but open. Under the drive, so this is
# a matter of the part being what it is rather than of being seen.
for i in range(14):
    x = 20.0 + (W - 40.0 - 2.2) * i / 13.0
    m.add(f"vent{i}",
          cq.Workplane("XY")
            .box(2.2, D * 0.42, 0.8, centered=(False, False, False))
            .translate((x, D * 0.30, -0.4)),
          SLOT_DK)

# Feet: round pads at the corners, the ground footprint the contact shadow is
# sized from.
for fx in (18.0, W - 18.0):
    for fy in (16.0, D - 16.0):
        m.add(f"foot{fx:.0f}_{fy:.0f}",
              cq.Workplane("XY").cylinder(2.0, 7.0, centered=(True, True, False))
                .translate((fx, fy, -2.0)),
              FOOT)


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Disk2c.mesh"),
                    os.path.join(out, "Disk2c.mtl"), "Disk2c.mtl")
    print(f"Disk2c (CAD): {nv} verts, {nt} tris")
