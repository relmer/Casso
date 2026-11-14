"""Apple ProFile (A9M0005) as a solid model: the wide, low 5 MB and 10 MB hard
disk of the Apple III and Lisa, on the Apple II ProFile interface card. X
right, Y back, Z up; front face at y = 0.

THE DIMENSIONS are Apple's: 17.28 in wide, 4.39 in high, 8.81 in deep
(apple3.org technote 1128, and the Computer History Museum's catalog entry
102681317 at 17 1/4 x 4 1/2 x 9 in).

MODELED FROM A PHOTOGRAPH, a ProFile on a Lisa:
commons.wikimedia.org/wiki/File:Apple_Lisa.jpg (CC BY-SA 3.0). Measured on it:

  - THE FRONT is plain, with no vents, and its top and bottom edges are
    rounded generously, about an eighth of the height.
  - THE BADGE, two dark plates, a square one with the rainbow Apple and a
    long one with "proFile", sits at the lower left, from 10% to 24% of the width and from 18% to 36% of the
    height.
  - THE READY PLATE, small and dark with its lamp, sits at the lower right,
    from 87% to 91% of the width, level with the badge.
  - The top is flat.

For the owner's review (T108). Sub-mesh identity is by part NAME and color:
`lamp` is the ready lamp in the drive-lamp color; `front_anchor` is the
front plane the scene lines devices up on.
"""

import os

import cadquery as cq
from cadkit import KD, Model

from cad_badge import badge

INCH        = 25.4
W, D, H     = 17.28 * INCH, 8.81 * INCH, 4.39 * INCH
CORNER_ANG  = 0.14
FRONT_R     = H * 0.12

CASE        = (0.860, 0.830, 0.750)       # Apple III and Lisa beige
PLATE       = (0.120, 0.115, 0.110)
LETTERING   = (0.880, 0.870, 0.840)
FOOT        = (0.150, 0.140, 0.130)

m = Model()

case = (cq.Workplane("XY")
        .box(W, D, H, centered=(False, False, False))
        .edges("|X").edges("<Y").fillet(FRONT_R)
        .edges("|X").edges(">Y").fillet(FRONT_R * 0.4)
        .edges("|Z").fillet(FRONT_R * 0.3))

m.add("case", case, CASE, angular=CORNER_ANG)

# The badge plate, lower left, with the rainbow Apple at its left end and
# "proFile" to its right.
PLATE_X0, PLATE_X1 = W * 0.10, W * 0.24
PLATE_Z0, PLATE_Z1 = H * 0.18, H * 0.36
plate_h            = PLATE_Z1 - PLATE_Z0

logo_w = plate_h

m.add("logo_plate",
      cq.Workplane("XY")
        .box(logo_w, 0.6, plate_h, centered=(False, False, False))
        .translate((PLATE_X0, -0.4, PLATE_Z0)),
      PLATE)
m.add("badge_plate",
      cq.Workplane("XY")
        .box(PLATE_X1 - PLATE_X0 - logo_w - 2.0, 0.6, plate_h, centered=(False, False, False))
        .translate((PLATE_X0 + logo_w + 2.0, -0.4, PLATE_Z0)),
      PLATE)

badge(m, PLATE_X0 + logo_w * 0.2, PLATE_Z0 + plate_h * 0.18, plate_h * 0.64, y = -0.7)

text = cq.Workplane("XZ").text("proFile", plate_h * 0.55, 0.3, font="Arial", halign="left", valign="center")
m.add("lettering",
      text.translate((PLATE_X0 + logo_w + 4.5, -0.6, PLATE_Z0 + plate_h * 0.5)),
      LETTERING)

# The ready plate, lower right, with its lamp.
READY_X0, READY_X1 = W * 0.87, W * 0.91

m.add("ready_plate",
      cq.Workplane("XY")
        .box(READY_X1 - READY_X0, 0.6, plate_h, centered=(False, False, False))
        .translate((READY_X0, -0.4, PLATE_Z0)),
      PLATE)
m.add("lamp",
      cq.Workplane("XY")
        .box(5.0, 0.5, 2.5, centered=(False, False, False))
        .translate(((READY_X0 + READY_X1) * 0.5 - 2.5, -0.7, PLATE_Z0 + plate_h * 0.25)),
      KD["drive_lamp"])

m.add("front_anchor",
      cq.Workplane("XY").box(1.0, 1.0, 1.0).translate((W * 0.5, 4.0, H * 0.5)),
      KD["front_anchor"])

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
