"""Apple //c external 5.25 Drive (A2M4020) as a real solid model. The low
platinum unit styled to match the //c, 152 x 216 x 70 mm (W x D x H).
X right, Y back, Z up; front lip face at y=-2.5.

A faithful port of the hand-built generator's coordinates (see cad_diskii.py
for why the construction moved to a CAD kernel). Unlike the Disk II there is
no black faceplate: the front is platinum with a shallower recessed panel and
only the slot is dark. The wide flip lever hangs BELOW the slot.

Sub-mesh identity is by Kd VALUE, and this drive wears the PLATINUM-ERA set
(DeskSceneModel's kDriveDoorAltKd / kDriveLatchAltKd / kDriveLampAltKd): the
lever and its grip tab form the door assembly the scene swings on eject, and
the lamp green is deliberately OFF the monitor lamp's green, which it would
otherwise be identified as -- putting the drive's lamp in the monitor's
sub-mesh.
"""

import cadquery as cq
from cadkit import KD, Model

# ---------------------------------------------------------------- dimensions

W, H, D = 152.0, 70.0, 216.0

LIP = 6.0                          # platinum margin around the panel pocket

SLOT_Z0, SLOT_Z1 = 42.0, 47.0

PLAT    = (0.870, 0.862, 0.835)
PLAT_DK = (0.790, 0.782, 0.755)
PANEL   = (0.830, 0.822, 0.795)
SLOT_DK = (0.060, 0.060, 0.070)
FOOT    = (0.320, 0.310, 0.300)

RAINBOW = [(0.20, 0.65, 0.27), (0.98, 0.80, 0.08), (0.96, 0.51, 0.12),
           (0.91, 0.18, 0.14), (0.58, 0.25, 0.60), (0.17, 0.45, 0.85)]

m = Model()

# --------------------------------------------------------------------- case

# One solid from the proud lip face back, edges filleted, the panel pocket
# CUT into the front (see cad_diskii.py on why the pocket must reach past
# the panel solid's rear face).
case = (cq.Workplane("XY")
        .box(W, D + 2.5, H, centered=(False, False, False))
        .translate((0.0, -2.5, 0.0))
        .edges("|Y").fillet(2.2)
        .edges("|X").fillet(1.4))

case = case.cut(
    cq.Workplane("XY")
      .box(W - LIP * 2.0, 4.5, H - LIP * 2.0, centered=(False, False, False))
      .translate((LIP, -3.5, LIP)))

m.add("case", case, PLAT)

# The recessed platinum panel filling the pocket floor -- the //c drive
# keeps its case color on the face; only the slot is dark.
m.add("panel",
      cq.Workplane("XY").box(W - LIP * 2.0, 1.8, H - LIP * 2.0, centered=(False, False, False))
        .translate((LIP, -0.8, LIP)),
      PANEL)

# ------------------------------------------------- faceplate furniture

# Slot: upper-middle.
m.add("slot",
      cq.Workplane("XY").box(W - 26.0, 0.6, SLOT_Z1 - SLOT_Z0, centered=(False, False, False))
        .translate((13.0, -1.4, SLOT_Z0)),
      SLOT_DK)

# The wide flip lever below the slot, its grip tab centered on it. Both
# carry the platinum-era door identities -- this is the assembly the scene
# swings on eject.
m.add("lever",
      cq.Workplane("XY").box(52.0, 2.6, 10.0, centered=(False, False, False))
        .translate((W / 2.0 - 26.0, -3.4, SLOT_Z0 - 12.0))
        .edges("|Y").fillet(1.2),
      KD["drive_door_alt"])

m.add("tab",
      cq.Workplane("XY").box(22.0, 0.6, 7.0, centered=(False, False, False))
        .translate((W / 2.0 - 11.0, -4.0, SLOT_Z0 - 10.5)),
      KD["drive_latch_alt"])

# Activity lamp, lower-left: green, //c family style.
#
# IT HAS TO STAND PROUD OF THE PANEL. It was set with its lens at y -0.4
# against a panel face at -0.8, so it sat four tenths of a millimeter INSIDE
# the surface it was meant to light -- invisible, and a light source that
# lights nothing: the shader weighs every surface by dot(n, L), and a lamp in
# the plane of the panel gives that panel a term of zero. The Disk II's LED
# had exactly this fault and the fix is the same one: the standoff IS the
# lighting.
LAMP_PROUD = 1.2

m.add("lamp",
      cq.Workplane("XY").cylinder(1.8, 2.8, direct=(0, 1, 0), centered=(True, True, False))
        .translate((22.0, -0.8 - LAMP_PROUD, 14.0)),
      KD["drive_lamp_alt"])

# Rainbow brand, lower-right: six small stripes.
for i, c in enumerate(RAINBOW):
    m.add(f"rb{i}",
          cq.Workplane("XY").box(14.0, 0.6, 2.4, centered=(False, False, False))
            .translate((126.0, -1.4, 26.0 - (i + 1) * 2.4)),
          c)

# ------------------------------------------------------------- lid and sides

# //c-style side ribs: shallow horizontal strips along both flanks.
for s, (sx0, sx1) in enumerate([(-0.3, 0.1), (W - 0.1, W + 0.3)]):
    for i in range(4):
        m.add(f"rib{s}_{i}",
              cq.Workplane("XY").box(sx1 - sx0, D - 46.0, 4.0, centered=(False, False, False))
                .translate((sx0, 26.0, 18.0 + i * 12.0)),
              PLAT_DK)

# Lid recess strip echoing the //c handle groove.
m.add("lid",
      cq.Workplane("XY").box(W - 40.0, D - 46.0, 0.3, centered=(False, False, False))
        .translate((20.0, 20.0, H - 0.01)),
      PLAT_DK)

# Feet: the ground footprint the contact shadow is sized from.
for fx in (16.0, W - 16.0):
    for fy in (18.0, D - 18.0):
        m.add(f"foot{fx:.0f}_{fy:.0f}",
              cq.Workplane("XY").cylinder(2.0, 5.5, centered=(True, True, False))
                .translate((fx, fy, -2.0)),
              FOOT)


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Disk2c.mesh"),
                    os.path.join(out, "Disk2c.mtl"), "Disk2c.mtl")
    print(f"Disk2c (CAD): {nv} verts, {nt} tris")
