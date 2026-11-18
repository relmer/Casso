"""Apple Disk /// (A3M0004), the Apple ///'s external drive, as a solid
model. 160 x 229 x 99 mm (W x D x H), feet included. X right, Y back, Z
up; the upper front face is at y = 0, and the block below the slot is
proud of it.

MODELED FROM PHOTOGRAPHS. Resources/Models/Disk3/README.md has the list of
them and the measurements taken from each.

The front is a beige face with chamfered vertical corners and a chamfered
top edge. The slot crosses it a little above half height. Below the slot a
block projects forward of the face. The logo plate is at the block's left,
and a dark finger recess with the busy lamp is in its middle. A latch is
in a notch above the slot, and its handle tab covers the slot's middle
third. The top has a shallow inset panel. A seam on the sides and the back
divides the cover from the base tray. The back has a recess for the
drive's connector and its red label.

The part name is the identity: `lever` is the latch the scene moves, `lamp`
the busy lamp, `slot` the opening.
"""

import os

import cadquery as cq

from cadkit import KD, Model
from cad_badge import badge

# ---------------------------------------------------------------- dimensions
#
# The width is scaled from the slot, which is taken to be the Apple ///'s
# built-in slot, 137.8 mm. The depth is the width times the footprint's
# aspect in the bottom photograph, 1.434.
W        = 159.9
D        = 229.0
FOOT_H   = 6.5
CASE_H   = 92.4
TOP      = FOOT_H + CASE_H

CORNER_ANG = 0.14

FRONT_CHAMFER = 9.0       # the vertical front corners
TOP_CHAMFER   = 7.0       # the top front edge
EDGE_R        = 3.5       # every other edge
REAR_R        = 6.0       # the rear vertical corners

# ------------------------------------------------------------------ the front

SLOT_W           = 137.8
SLOT_X0          = (W - SLOT_W) * 0.5
SLOT_Z0, SLOT_Z1 = 51.5, 56.0

# The block below the slot, proud of the upper face.
BLOCK_X0, BLOCK_X1 = 7.8, W - 7.8
BLOCK_Z0, BLOCK_Z1 = 19.0, 50.8
BLOCK_PROUD        = 2.5
BLOCK_FRONT        = -BLOCK_PROUD

# The notch, which runs down through the block as the finger recess.
NOTCH_W          = 41.5
NOTCH_X0         = (W - NOTCH_W) * 0.5
NOTCH_Z0         = 22.8
NOTCH_Z1         = 83.4
NOTCH_D          = 14.0

# The latch fills the notch above the slot; its tab covers the slot.
LATCH_W          = 38.4
LATCH_X0         = (W - LATCH_W) * 0.5
LATCH_Z1         = 82.3
TAB_Z0, TAB_Z1   = 46.4, 61.2
TAB_PROUD        = 3.0
LATCH_ROUND      = 0.6

# The busy lamp in its black frame, in the recess.
LAMP_W, LAMP_H   = 5.0, 3.6
LAMP_CX, LAMP_CZ = W * 0.5, 33.1
LAMP_FRAME       = 1.2

# The drive label at the upper left and the logo plate on the block.
LABEL_X0, LABEL_X1 = 9.3, 25.1
LABEL_Z0, LABEL_Z1 = 69.3, 85.6
PLATE_X0, PLATE_X1 = 12.7, 45.8
PLATE_Z0, PLATE_Z1 = 23.2, 39.3

# ------------------------------------------------------- top, seam and back

PANEL_INSET_X = 16.0
PANEL_FRONT   = 22.0
PANEL_BACK    = 14.0
PANEL_DEEP    = 0.8

SEAM_Z        = FOOT_H + 25.0
SEAM_GAP      = 0.6
SEAM_DEEP     = 0.8

REAR_X0, REAR_X1 = 19.0, 146.0
REAR_Z0, REAR_Z1 = 31.8, 76.3
REAR_DEEP        = 8.0

# Four square rubber feet, measured from the bottom photograph.
FOOT_W   = 22.0
FOOT_XS  = (25.9, 113.6)
FOOT_YS  = (55.6, 179.6)

# The case and latch colors are the photographs' plastic, white-balanced
# against each photograph's background: a lighter, warmer cream than the
# Disk II's beige, with a latch a little paler and grayer than the case.
CASE     = (0.905, 0.840, 0.700)
LATCH    = (0.905, 0.860, 0.755)
SLOT_DK  = (0.055, 0.055, 0.062)
RECESS   = (0.160, 0.130, 0.110)
BLACK    = (0.070, 0.070, 0.075)
WHITE    = (0.920, 0.920, 0.900)
METAL    = (0.560, 0.565, 0.575)
LABEL_RD = (0.780, 0.110, 0.120)
HEADER   = (0.200, 0.330, 0.620)
RUBBER   = (0.180, 0.176, 0.170)


def box(x0, y0, z0, x1, y1, z1):
    return (cq.Workplane("XY")
              .box(x1 - x0, y1 - y0, z1 - z0, centered=(False, False, False))
              .translate((x0, y0, z0)))


m = Model()

# ---------------------------------------------------------------------- case

case = box(0.0, 0.0, FOOT_H, W, D, TOP)
case = case.edges("|Z and <Y").chamfer(FRONT_CHAMFER)
case = case.edges("|Z and >Y").fillet(REAR_R)
case = case.edges("|X and >Z and <Y").chamfer(TOP_CHAMFER)
case = case.edges("not(<Y) and (>Z or <Z)").fillet(EDGE_R)

# The block below the slot, with its bottom edge rounded into the face.
block = box(BLOCK_X0, BLOCK_FRONT, BLOCK_Z0, BLOCK_X1, 1.0, BLOCK_Z1)
block = block.edges("|Y").fillet(1.5)
block = block.edges("<Y").fillet(0.8)
case  = case.union(block)

# The top's inset panel.
case = case.cut(box(PANEL_INSET_X, PANEL_FRONT, TOP - PANEL_DEEP,
                    W - PANEL_INSET_X, D - PANEL_BACK, TOP + 1.0)
                  .edges("|Z").fillet(4.0))

# The slot, beveled top and bottom, and the notch above and below it.
case = case.cut(box(SLOT_X0, -5.0, SLOT_Z0, W - SLOT_X0, 20.0, SLOT_Z1))
case = case.cut(box(NOTCH_X0, -5.0, NOTCH_Z0, NOTCH_X0 + NOTCH_W, NOTCH_D, NOTCH_Z1))

# The seam between the cover and the base tray: a groove on the sides and the
# back, stopping at the front corners.
case = case.cut(box(-5.0, FRONT_CHAMFER + 2.0, SEAM_Z - SEAM_GAP * 0.5,
                    W + 5.0, D + 5.0, SEAM_Z + SEAM_GAP * 0.5)
                  .cut(box(SEAM_DEEP, 0.0, SEAM_Z - 2.0,
                           W - SEAM_DEEP, D - SEAM_DEEP, SEAM_Z + 2.0)))

# The rear recess.
case = case.cut(box(REAR_X0, D - REAR_DEEP, REAR_Z0, REAR_X1, D + 1.0, REAR_Z1))

m.add("case", case, CASE, angular=CORNER_ANG)

# ----------------------------------------------------------- front furniture

m.add("slot", box(SLOT_X0, 1.0, SLOT_Z0, W - SLOT_X0, 19.0, SLOT_Z1), SLOT_DK)

# The recess's dark lining, below the latch.
m.add("recess",
      box(NOTCH_X0, NOTCH_D - 1.0, NOTCH_Z0, NOTCH_X0 + NOTCH_W, NOTCH_D, TAB_Z0)
        .union(box(NOTCH_X0, BLOCK_FRONT + 0.5, NOTCH_Z0 - 1.0, NOTCH_X0 + NOTCH_W, NOTCH_D, NOTCH_Z0))
        .union(box(NOTCH_X0, BLOCK_FRONT + 0.5, NOTCH_Z0, NOTCH_X0 + 0.6, NOTCH_D, TAB_Z0))
        .union(box(NOTCH_X0 + NOTCH_W - 0.6, BLOCK_FRONT + 0.5, NOTCH_Z0, NOTCH_X0 + NOTCH_W, NOTCH_D, TAB_Z0)),
      RECESS)

# The latch: a plate flush with the face from the top of the notch to the
# tab, and the tab standing proud across the slot.
latch = (box(LATCH_X0, 0.0, TAB_Z1 - 1.0, LATCH_X0 + LATCH_W, 4.0, LATCH_Z1)
           .union(box(LATCH_X0, -TAB_PROUD, TAB_Z0, LATCH_X0 + LATCH_W, 4.0, TAB_Z1)))
latch = latch.edges("|X").fillet(LATCH_ROUND)
latch = latch.edges("|Z").fillet(LATCH_ROUND)
m.add("lever", latch, LATCH)

m.add("lamp_frame",
      box(LAMP_CX - LAMP_W * 0.5 - LAMP_FRAME, NOTCH_D - 1.6, LAMP_CZ - LAMP_H * 0.5 - LAMP_FRAME,
          LAMP_CX + LAMP_W * 0.5 + LAMP_FRAME, NOTCH_D - 1.0, LAMP_CZ + LAMP_H * 0.5 + LAMP_FRAME),
      BLACK)
m.add("lamp",
      box(LAMP_CX - LAMP_W * 0.5, NOTCH_D - 1.8, LAMP_CZ - LAMP_H * 0.5,
          LAMP_CX + LAMP_W * 0.5, NOTCH_D - 1.0, LAMP_CZ + LAMP_H * 0.5),
      KD["drive_lamp"])

# The drive label: a black plate with a white field for the drive number.
m.add("label",
      box(LABEL_X0, -0.4, LABEL_Z0, LABEL_X1, 0.5, LABEL_Z1).edges("|Y").fillet(1.2),
      BLACK)
m.add("label_field",
      box(LABEL_X0 + 1.4, -0.55, LABEL_Z0 + 1.4, LABEL_X1 - 1.4, 0.5, LABEL_Z1 - 5.4).edges("|Y").fillet(0.8),
      WHITE)

# The logo plate on the block, with the badge at its left.
m.add("logo_plate",
      box(PLATE_X0, BLOCK_FRONT - 0.4, PLATE_Z0, PLATE_X1, BLOCK_FRONT + 0.5, PLATE_Z1).edges("|Y").fillet(2.0),
      BLACK)
badge(m, PLATE_X0 + 2.5, PLATE_Z0 + 2.0, PLATE_Z1 - PLATE_Z0 - 4.0, y=BLOCK_FRONT - 0.7)

# ---------------------------------------------------------------------- back

m.add("rear_plate", box(REAR_X0, D - REAR_DEEP, REAR_Z0, REAR_X1, D - REAR_DEEP + 0.5, REAR_Z1), METAL)
m.add("rear_label", box(REAR_X0 + 26.0, D - REAR_DEEP - 0.3, REAR_Z0 + 1.5, REAR_X1 - 3.0, D - REAR_DEEP + 0.6, REAR_Z0 + 30.0), LABEL_RD)
m.add("rear_header", box(REAR_X1 - 40.0, D - REAR_DEEP - 2.5, REAR_Z0 + 33.0, REAR_X1 - 3.0, D - REAR_DEEP + 0.6, REAR_Z0 + 40.5), HEADER)

# --------------------------------------------------------------------- feet

for fx in FOOT_XS:
    for fy in FOOT_YS:
        m.add(f"foot_{fx:.0f}_{fy:.0f}",
              box(fx, fy, 0.0, fx + FOOT_W, fy + FOOT_W, FOOT_H + 0.5).edges("|Z").fillet(1.5),
              RUBBER)


if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Disk3.mesh"), os.path.join(out, "Disk3.mtl"), "Disk3.mtl")
    print(f"Disk3 (CAD): {nv} verts, {nt} tris")
