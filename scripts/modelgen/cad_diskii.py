"""Apple Disk II drive (A2M0003) as a real solid model. 155 x 220 x 96 mm
(W x D x H), X right, Y back, Z up, front lip face at y=-3.

A faithful PORT of the hand-built generator's coordinates, not a redesign:
the desk scene stamps onto this model at fixed model-space positions -- the
DRIVE-n text on the badge plaque, the IN-USE label beside the LED, the
write-protect padlock, the eject/body region boxes -- so every feature stays
where the scene expects it. What changes is construction: the faceplate
recess is a boolean POCKET in the case rather than four proud slabs faking
one, and the case edges carry real fillets (the real unit is mostly square
with a slight rounding all round).

Sub-mesh identity is by Kd VALUE (DeskSceneModel::kKdEpsilon): the door bar
and latch tab carry the door identities so the scene swings them on eject,
and the LED carries the drive-lamp identity so it lights and glows.
"""

import math

import cadquery as cq
from cadkit import KD, Model

# ---------------------------------------------------------------- dimensions

W, H, D = 155.0, 96.0, 220.0

# Faceplate features were laid out against an 86 mm case; they are placed as
# fractions of the real height so the front keeps its proportions.
FZ = H / 86.0

LIP      = 7.0                    # beige margin around the faceplate pocket
POCKET_D = 2.0                    # how far the black plate sits behind the lip

SLOT_Z0, SLOT_Z1 = 46.0 * FZ, 52.0 * FZ
SLOT_X0, SLOT_X1 = 14.0, W - 14.0

PLATE_Y = -1.0                    # the black drive face

# ------------------------------------------------------------- the slot frame
#
# A pronounced rectangle around the disk opening, standing proud of the face:
# a flat band, then a 45-degree bevel falling back to the slot on all four
# sides. The bevel is most of what the front reads as -- it catches a
# highlight the whole way round the opening, which a square-cut slot cannot.
FRAME_PROUD = 1.5                 # how far the frame stands off the face
FRAME_FLAT  = 2.0                 # the flat band's width, in the X/Z plane
BEVEL_LEN   = 2.5                 # measured ALONG the slope, not projected
BEVEL_RUN   = BEVEL_LEN / math.sqrt (2.0)     # so 1.768 in each of Y and X/Z

FRAME_Y  = PLATE_Y - FRAME_PROUD
FRAME_X0 = SLOT_X0 - (FRAME_FLAT + BEVEL_RUN)
FRAME_X1 = SLOT_X1 + (FRAME_FLAT + BEVEL_RUN)
FRAME_Z0 = SLOT_Z0 - (FRAME_FLAT + BEVEL_RUN)
FRAME_Z1 = SLOT_Z1 + (FRAME_FLAT + BEVEL_RUN)

# ------------------------------------------------------------------ the door
#
# The door wears the FACE'S OWN CONTOUR: flush with the plate above the frame,
# stepped proud where it crosses it. That proud band is the handle -- there is
# no separate latch. It ends at the frame's bottom edge, and it is solid where
# the slot would be, since a door with a slot in it would not hold a disk in.
DOOR_W  = 39.0
DOOR_T  = 2.0
DOOR_X0 = (W - DOOR_W) * 0.5
DOOR_Z0 = FRAME_Z0
DOOR_Z1 = 84.4
DOOR_BACK = PLATE_Y + DOOR_T      # both steps share one back plane

# The finger notch behind the door: a wedge, flush with the face at its bottom
# and NOTCH_DEEP at the top, which is where the door comes to rest. It cuts
# the frame and the face alike, so an open door leaves a gap in the frame.
NOTCH_W    = DOOR_W + 1.0         # a hair wider, so the door never binds
NOTCH_DEEP = 38.0

BEIGE    = (0.833, 0.784, 0.659)
BEIGE_DK = (0.760, 0.710, 0.590)
PLATE    = (0.100, 0.100, 0.110)
SLOT_DK  = (0.035, 0.035, 0.045)
BADGE    = (0.900, 0.870, 0.780)
FOOT     = (0.150, 0.140, 0.130)

m = Model()

# --------------------------------------------------------------------- case

# One solid from the proud lip face back, edges filleted, faceplate pocket
# CUT into the front. A pocket cannot silently be a solid face, which is the
# failure the hand-built construction invited.
case = (cq.Workplane("XY")
        .box(W, D + 3.0, H, centered=(False, False, False))
        .translate((0.0, -3.0, 0.0))
        .edges("|Y").fillet(1.8)
        .edges("|X").fillet(1.2))

# The pocket reaches 4 mm INTO the case body, not just through the proud
# lip: the black plate solid lives at y -1..0, and a shallower cut leaves
# solid beige coincident with it -- the plate ends up buried inside case
# material and the front renders beige.
case = case.cut(
    cq.Workplane("XY")
      .box(W - LIP * 2.0, 5.0, H - LIP * 2.0, centered=(False, False, False))
      .translate((LIP, -4.0, LIP)))

# The finger notch: a wedge running back into the drive, nothing deep at the
# frame's bottom edge and NOTCH_DEEP where the door comes to rest. Cut from
# the case AND the plate -- cutting only the case would leave the black plate
# spanning the opening, and the notch would render as a painted rectangle.
# The FLOOR slopes; the mouth does not. At the bottom the floor is the drive
# face itself -- which is why an open door leaves a gap in the frame, the
# frame being proud of that face -- and it falls back to NOTCH_DEEP where the
# door comes to rest. Measured from the face, not from the frame.
notch = (cq.Workplane("XZ")
         .polyline ([(FRAME_Y,                 DOOR_Z0),
                     (PLATE_Y,                 DOOR_Z0),
                     (PLATE_Y + NOTCH_DEEP,    DOOR_Z1),
                     (FRAME_Y,                 DOOR_Z1)])
         .close()
         .extrude (NOTCH_W)
         .translate (((W - NOTCH_W) * 0.5, 0.0, 0.0)))

case = case.cut (notch)

m.add("case", case, BEIGE)

# The black faceplate filling the pocket floor, a hair behind the lip.
plate = (cq.Workplane("XY")
         .box(W - LIP * 2.0, 2.0, H - LIP * 2.0, centered=(False, False, False))
         .translate((LIP, PLATE_Y, LIP)))

# Two finishes, one plastic. Above the frame's bottom edge the face carries
# the molded pebble grain; below it, the same black in a smooth matte. The
# split is the frame's own bottom, so the two never disagree about where the
# boundary is.
plate_hi = (cq.Workplane("XY")
            .box(W - LIP * 2.0, 2.0, (LIP + H - LIP * 2.0) - FRAME_Z0, centered=(False, False, False))
            .translate((LIP, PLATE_Y, FRAME_Z0)))

m.add("plate_pebbled", plate.intersect (plate_hi).cut (notch), KD["plate_pebbled"])
m.add("plate",         plate.cut (plate_hi).cut (notch),        PLATE)

# ------------------------------------------------- faceplate furniture

# The slot frame: a slab over the frame rectangle with the mouth LOFTED
# through it -- wide at the face, narrowing to the slot one bevel-run back.
# Built as the shape it is rather than chamfered on afterwards, because the
# chamfer would have to be picked out of an edge selection and a mis-picked
# edge is exactly the kind of failure that renders as "looks a bit off".
slot_cx = (SLOT_X0 + SLOT_X1) * 0.5
slot_cz = (SLOT_Z0 + SLOT_Z1) * 0.5
slot_w  = SLOT_X1 - SLOT_X0
slot_h  = SLOT_Z1 - SLOT_Z0

frame = (cq.Workplane("XY")
         .box(FRAME_X1 - FRAME_X0, PLATE_Y - FRAME_Y + 0.6, FRAME_Z1 - FRAME_Z0,
              centered=(False, False, False))
         .translate((FRAME_X0, FRAME_Y, FRAME_Z0)))

mouth = (cq.Workplane("XZ", origin=(slot_cx, FRAME_Y, slot_cz))
         .rect(slot_w + BEVEL_RUN * 2.0, slot_h + BEVEL_RUN * 2.0)
         .workplane(offset=-BEVEL_RUN)
         .rect(slot_w, slot_h)
         .loft())

throat = (cq.Workplane("XY")
          .box(slot_w, 12.0, slot_h, centered=(False, False, False))
          .translate((SLOT_X0, FRAME_Y + BEVEL_RUN, SLOT_Z0)))

m.add("frame", frame.cut (mouth).cut (throat).cut (notch), KD["plate_pebbled"])

# The dark mouth behind the bevel, so the opening reads as a hole rather than
# a painted rectangle.
m.add("slot",
      cq.Workplane("XY").box(slot_w, 0.6, slot_h, centered=(False, False, False))
        .translate((SLOT_X0, FRAME_Y + BEVEL_RUN + 1.2, SLOT_Z0)),
      SLOT_DK)

# The door. IDENTITY COLOR: the scene splits this out and swings it, so its
# y/z extents are part of the contract.
door = (cq.Workplane("XY")
        .box(DOOR_W, DOOR_BACK - PLATE_Y, DOOR_Z1 - FRAME_Z1, centered=(False, False, False))
        .translate((DOOR_X0, PLATE_Y, FRAME_Z1)))

# The handle band stops at the FACE, not at the door's back plane: it is the
# part that stands proud, so it has nothing behind it to occupy. Running it
# back to the door's own back plane buried it in the plate, which rendered as
# z-fighting stripes down the middle of the drive.
door = door.union (
    cq.Workplane("XY")
      .box(DOOR_W, PLATE_Y - FRAME_Y, FRAME_Z1 - FRAME_Z0, centered=(False, False, False))
      .translate((DOOR_X0, FRAME_Y, FRAME_Z0)))

# The notch's walls are black plastic, not the beige shell behind them, and
# they carry the same pebble grain as the face. Line the pocket, then cut the
# notch out of the liner so what is left IS the walls.
liner = (cq.Workplane("XY")
         .box(NOTCH_W + 4.0, NOTCH_DEEP + 3.0, (DOOR_Z1 - DOOR_Z0) + 4.0,
              centered=(False, False, False))
         .translate(((W - NOTCH_W) * 0.5 - 2.0, PLATE_Y, DOOR_Z0 - 2.0)))

m.add("notch_liner", liner.cut (notch), KD["plate_pebbled"])

m.add("door", door, KD["drive_door"])

# Badge plaque, upper-left: the scene stamps DRIVE 1 / DRIVE 2 onto it.
m.add("badge",
      cq.Workplane("XY").box(39.0, 0.8, 10.0 * FZ, centered=(False, False, False))
        .translate((13.0, -1.8, 64.0 * FZ)),
      BADGE)

# IN-USE LED, lower-left; its label is stamped by the scene to its left.
m.add("led",
      cq.Workplane("XY").cylinder(2.0, 3.2, direct=(0, 1, 0), centered=(True, True, False))
        .translate((68.0, -0.6, 16.0 * FZ)),
      KD["drive_lamp"])

# ------------------------------------------------------------- lid and sides

# Lid channels: two slim darker strips running front to back.
for i, (x0, x1) in enumerate([(24.0, 62.0), (93.0, 131.0)]):
    m.add(f"channel{i}",
          cq.Workplane("XY").box(x1 - x0, D - 42.0, 0.35, centered=(False, False, False))
            .translate((x0, 18.0, H - 0.01)),
          BEIGE_DK)

# Side vents: nine slits per side, rear half.
for s, (sx0, sx1) in enumerate([(-0.35, 0.1), (W - 0.1, W + 0.35)]):
    for i in range(9):
        m.add(f"vent{s}_{i}",
              cq.Workplane("XY").box(sx1 - sx0, 3.2, 46.0 * FZ, centered=(False, False, False))
                .translate((sx0, 146.0 + i * 7.0, 20.0 * FZ)),
              BEIGE_DK)

# Rubber feet: the ground footprint the contact shadow is sized from.
for fx in (16.0, W - 16.0):
    for fy in (18.0, D - 18.0):
        m.add(f"foot{fx:.0f}_{fy:.0f}",
              cq.Workplane("XY").cylinder(2.2, 6.0, centered=(True, True, False))
                .translate((fx, fy, -2.2)),
              FOOT)


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "DiskII.mesh"),
                    os.path.join(out, "DiskII.mtl"), "DiskII.mtl")
    print(f"DiskII (CAD): {nv} verts, {nt} tris")
