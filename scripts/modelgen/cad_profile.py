"""Apple ProFile (A9M0005) as a solid model: the wide, low 5 MB and 10 MB hard
disk of the Apple III and Lisa, on the Apple II ProFile interface card. X
right, Y back, Z up; front face at y = 0. Replaces gen_profile.py.

THE DIMENSIONS are Apple's: 17.28 in wide, 4.39 in high, 8.81 in deep
(apple3.org technote 1128, and the Computer History Museum's catalog entry
102681317 at 17 1/4 x 4 1/2 x 9 in).

THE FRONT follows gen_profile.py: a full-width band of vent slots low on the
face, the "ProFile" nameplate at the lower left above it, and the ready lamp
at the lower right. The top has a shallow stepped panel inside a border.
These proportions are not from a measured photograph and are for the owner's
review (T108).

Sub-mesh identity is by part NAME and color: `lamp` is the ready lamp in the
drive-lamp color; `front_anchor` is the front plane the scene lines devices
up on.
"""

import os

import cadquery as cq
from cadkit import KD, Model

# ---------------------------------------------------------------- dimensions

INCH        = 25.4
W, D, H     = 17.28 * INCH, 8.81 * INCH, 4.39 * INCH
EDGE_R      = INCH * 3.0 / 32.0
CORNER_ANG  = 0.14

CASE        = (0.833, 0.784, 0.659)       # Apple III beige
PANEL       = (0.790, 0.742, 0.620)
GRILLE      = (0.240, 0.230, 0.210)
NAME        = (0.300, 0.220, 0.160)       # the brown "ProFile" lettering
FOOT        = (0.150, 0.140, 0.130)

m = Model()

# ------------------------------------------------------------------ the case

case = (cq.Workplane("XY")
        .box(W, D, H, centered=(False, False, False))
        .edges("|Z").fillet(EDGE_R * 2.0)
        .edges(">Z").fillet(EDGE_R)
        .edges("<Z").fillet(EDGE_R * 0.5))

# The vent band: slots 10 mm wide on a 16 mm pitch, 16 to 40 mm up.
SLOT_W, SLOT_PITCH, SLOT_Z0, SLOT_Z1 = 10.0, 16.0, 16.0, 40.0
SLOT_DEPTH = 2.0
slots      = []
x          = 26.0

while x + SLOT_W <= W - 26.0:
    case = case.cut(cq.Workplane("XY")
                    .box(SLOT_W, SLOT_DEPTH + 1.0, SLOT_Z1 - SLOT_Z0, centered=(False, False, False))
                    .translate((x, -1.0, SLOT_Z0)))
    slots.append(cq.Workplane("XY")
                 .box(SLOT_W, 0.6, SLOT_Z1 - SLOT_Z0, centered=(False, False, False))
                 .translate((x, SLOT_DEPTH - 0.6, SLOT_Z0))
                 .val())
    x += SLOT_PITCH

# The top's stepped panel, 26 mm in from the sides and 30 mm from front and back.
case = case.cut(cq.Workplane("XY")
                .box(W - 52.0, D - 60.0, 1.0, centered=(False, False, False))
                .translate((26.0, 30.0, H - 0.6)))

m.add("case", case, CASE, angular=CORNER_ANG)
m.add("panel",
      cq.Workplane("XY")
        .box(W - 52.0, D - 60.0, 0.2, centered=(False, False, False))
        .translate((26.0, 30.0, H - 0.6)),
      PANEL)
m.add("grille", cq.Compound.makeCompound(slots), GRILLE)

# -------------------------------------------------------------- the nameplate

m.add("nameplate",
      cq.Workplane("XY")
        .box(86.0, 0.6, 14.0, centered=(False, False, False))
        .translate((24.0, -0.4, 52.0)),
      NAME)

# ------------------------------------------------------------------ the lamp

m.add("lamp",
      cq.Workplane("XZ")
        .circle(4.0)
        .extrude(1.2)
        .translate((W - 40.0, 0.4, 58.0)),
      KD["drive_lamp"])

# ------------------------------------------------------------------ anchors

m.add("front_anchor",
      cq.Workplane("XY").box(1.0, 1.0, 1.0).translate((W * 0.5, 2.0, H * 0.5)),
      KD["front_anchor"])

# --------------------------------------------------------------- underside

FOOT_R, FOOT_H = 7.0, 2.4

for fx in (30.0, W - 30.0):
    for fy in (22.0, D - 22.0):
        m.add(f"foot_{fx:.0f}_{fy:.0f}",
              cq.Workplane("XY")
                .cylinder(FOOT_H, FOOT_R, centered=(True, True, False))
                .translate((fx, fy, -FOOT_H)),
              FOOT)


if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "ProFile.mesh"),
                    os.path.join(out, "ProFile.mtl"), "ProFile.mtl")
    print(f"ProFile (CAD): {nv} verts, {nt} tris")
