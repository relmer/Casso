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

m.add("case", case, BEIGE)

# The black faceplate filling the pocket floor, a hair behind the lip.
m.add("plate",
      cq.Workplane("XY").box(W - LIP * 2.0, 2.0, H - LIP * 2.0, centered=(False, False, False))
        .translate((LIP, -1.0, LIP)),
      PLATE)

# ------------------------------------------------- faceplate furniture

# Slot: near-black strip proud of the plate.
m.add("slot",
      cq.Workplane("XY").box(W - 28.0, 0.7, SLOT_Z1 - SLOT_Z0, centered=(False, False, False))
        .translate((14.0, -1.7, SLOT_Z0)),
      SLOT_DK)

# Door bar above the slot, latch tab centered on it. IDENTITY COLORS: the
# scene splits these into the door assembly and swings them on eject, with
# the hinge derived from the assembly's top-back edge -- so their y/z extents
# are part of the contract.
m.add("door",
      cq.Workplane("XY").box(W - 28.0, 1.3, 9.0 * FZ, centered=(False, False, False))
        .translate((14.0, -2.3, SLOT_Z1)),
      KD["drive_door"])

m.add("latch",
      cq.Workplane("XY").box(24.0, 0.8, 6.0 * FZ, centered=(False, False, False))
        .translate((W / 2.0 - 12.0, -3.1, SLOT_Z1 + 1.5 * FZ))
        .edges("|Y").fillet(1.0),
      KD["drive_latch"])

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
