"""Apple Hard Disk 20SC (M2604) as a solid model: the platinum SCSI drive that
sits beside or under the computer. X right, Y back, Z up; front face at y = 0.

MODELED FROM A PHOTOGRAPH, the owner's reference:
https://macs.billiehawkins.com/wp-content/uploads/2018/11/20sc.jpg

What the photograph settles, measured on it:

  - THE PROPORTIONS. The front face is 1190 px wide and 415 px tall
    including the lid's lip, so the height is about 0.31 of the width. The
    footprint is close to square, a little deeper than wide.
  - THE TWO SHELLS. The lid comes down over the upper 43% of the front as a
    band, with a fine seam between it and the lower face; the seam runs round
    the sides.
  - THE TOP. A plain border at the front and sides, then grooves running
    front to back across the rest of the lid, about thirty of them.
  - THE SIDES. Horizontal grooves along the lower shell.
  - THE LAMP. A short dark slit at the right end of the band, just above the
    seam.
  - THE BADGE. The rainbow Apple at the lower left of the front, with the
    model number to its right. The number is NOT modeled: the desk scene
    draws it as a face label from the attached image's size ("20SC", "32SC"),
    so one baked model serves every size. The logo is.

Not measured with a tape. The numbers below are the photograph's ratios
scaled to a width of 245 mm.

Sub-mesh identity is by part NAME and color: `lamp` is the activity
indicator in the drive-lamp color.
"""

import io
import os
import re

import cadquery as cq
from cadkit import KD, Model

# ---------------------------------------------------------------- dimensions

W, D, H = 245.0, 290.0, 76.0

INCH        = 25.4
EDGE_R      = INCH * 3.0 / 32.0          # molded ABS, no crisp edges
CORNER_ANG  = 0.14                        # see cad_disk2c.py: flat shading bands

CASE        = (0.835, 0.815, 0.760)       # platinum
SEAM        = (0.520, 0.505, 0.470)
FOOT        = (0.180, 0.180, 0.190)

# The seam: the lid covers the upper 43% of the front.
SPLIT_Z     = H * 0.57
SEAM_GAP    = 0.8

# ----------------------------------------------------------------- the case

m = Model()

bottom = (cq.Workplane("XY")
          .box(W, D, SPLIT_Z - SEAM_GAP * 0.5, centered=(False, False, False))
          .edges("|Z").fillet(EDGE_R * 2.0)
          .edges("<Z").fillet(EDGE_R))

lid = (cq.Workplane("XY")
       .box(W, D, H - SPLIT_Z - SEAM_GAP * 0.5, centered=(False, False, False))
       .translate((0, 0, SPLIT_Z + SEAM_GAP * 0.5))
       .edges("|Z").fillet(EDGE_R * 2.0)
       .edges(">Z").fillet(EDGE_R))

# The seam's shadow: a slab just inside the case, seen through the gap.
seam = (cq.Workplane("XY")
        .box(W - 2.0, D - 2.0, SEAM_GAP * 2.0, centered=(False, False, False))
        .translate((1.0, 1.0, SPLIT_Z - SEAM_GAP)))

# ----------------------------------------------------------- the lid's grooves
#
# Thirty grooves front to back, inside a plain border: 22 mm at the front,
# where the lid turns down into the band, and 12 mm at the sides and back.

GROOVES      = 30
GROOVE_W     = 1.6
GROOVE_DEPTH = 0.9
TOP_X0, TOP_X1 = 14.0, W - 14.0
TOP_Y0, TOP_Y1 = 22.0, D - 12.0

pitch = (TOP_X1 - TOP_X0) / GROOVES

for i in range(1, GROOVES):
    x = TOP_X0 + i * pitch - GROOVE_W * 0.5
    lid = lid.cut(cq.Workplane("XY")
                  .box(GROOVE_W, TOP_Y1 - TOP_Y0, GROOVE_DEPTH + 1.0, centered=(False, False, False))
                  .translate((x, TOP_Y0, H - GROOVE_DEPTH)))

# ---------------------------------------------------------- the sides' grooves

SIDE_GROOVES = 5
SIDE_GROOVE_H = 1.4
SIDE_GROOVE_DEPTH = 0.8

for i in range(SIDE_GROOVES):
    z = 8.0 + i * (SPLIT_Z - 16.0) / (SIDE_GROOVES - 1)

    for x in (-1.0, W - SIDE_GROOVE_DEPTH):
        bottom = bottom.cut(cq.Workplane("XY")
                            .box(SIDE_GROOVE_DEPTH + 1.0, D - 40.0, SIDE_GROOVE_H, centered=(False, False, False))
                            .translate((x, 20.0, z)))

m.add("case_lid",    lid,    CASE, angular=CORNER_ANG)
m.add("seam",        seam,   SEAM)

# ------------------------------------------------------------------ the lamp
#
# A short slit at the band's right end, 3 mm above the seam.

LAMP_W, LAMP_H = 11.0, 1.6
LAMP_X0        = W - 40.0
LAMP_Z0        = SPLIT_Z + 3.0

m.add("lamp",
      cq.Workplane("XY")
        .box(LAMP_W, 0.6, LAMP_H, centered=(False, False, False))
        .translate((LAMP_X0, -0.3, LAMP_Z0)),
      KD["drive_lamp"])

# ----------------------------------------------------------------- the badge


def read_branding():
    src = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "..", "..", "CassoEmuCore", "Ui", "Chrome",
                               "CassoBranding.cpp"), encoding="utf-8").read()

    sil_body = re.search(r"s_kSilhouette\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
    rows     = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)ULL", sil_body)]

    col_body = re.search(r"s_kStripeColors\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
    stripes  = [((int(v, 16) >> 16 & 0xFF) / 255.0,
                 (int(v, 16) >> 8 & 0xFF) / 255.0,
                 (int(v, 16) & 0xFF) / 255.0)
                for v in re.findall(r"0x[Ff]{2}([0-9A-Fa-f]{6})", col_body)]

    return rows, stripes


# Upright on the FRONT face, so row zero, the crown, is the highest z. The
# badge is 9 mm tall, its left edge 32 mm in from the left, its base 12 mm up.
BADGE_H  = 9.0
BADGE_X0 = 32.0
BADGE_Z0 = 12.0

_rows, _stripes = read_branding()
_cell    = BADGE_H / len(_rows)
_nonzero = [i for i, r in enumerate(_rows) if r]
_first, _last = _nonzero[0], _nonzero[-1]
_by_stripe = {}

for row, bits in enumerate(_rows):
    if not bits:
        continue

    banded = min(_last, max(_first, row))
    stripe = ((banded - _first) * len(_stripes)) // (_last - _first + 1)
    z0     = BADGE_Z0 + (len(_rows) - row - 1) * _cell
    col    = 0

    while col < 64:
        if not (bits >> col) & 1:
            col += 1
            continue

        run = col
        while run < 64 and (bits >> run) & 1:
            run += 1

        _by_stripe.setdefault(stripe, []).append(
            cq.Workplane("XY")
              .box((run - col) * _cell, 0.5, _cell + 0.02, centered=(False, False, False))
              .translate((BADGE_X0 + col * _cell, -0.3, z0))
              .val())
        col = run

for stripe, solids in sorted(_by_stripe.items()):
    m.add(f"badge{stripe}", cq.Compound.makeCompound(solids), _stripes[stripe])

# ------------------------------------------------------------------ the back
#
# From the rear photograph (computers.popcorn.cx/apple/peripherals/drives/
# hard-disk-20sc/hard-disk-20sc-02.jpg), laid out as seen from behind and
# mirrored across the centerline into the model's x: a grid of vents at the
# left; the two Centronics 50-pin SCSI connectors stacked left of center,
# each with its wire bails, the SCSI ID selector to their right and a
# barcode label under them; a row of vents at the upper right over the IEC
# power inlet and the rocker switch.

BACK_DARK  = (0.060, 0.060, 0.060)
BACK_METAL = (0.720, 0.720, 0.720)
BACK_LABEL = (0.950, 0.945, 0.930)


def back_add(name, solid, rgb):
    m.add(name, solid.mirror("YZ", (W * 0.5, 0, 0)), rgb)


def back_box(name, x0, x1, z0, z1, depth, rgb, y = 0.0):
    back_add(name,
             cq.Workplane("XY").box(x1 - x0, depth, z1 - z0, centered=(False, False, False))
               .translate((x0, D + y, z0)),
             rgb)


for i in range(12):
    back_box(f"back_vent_left{i}", W * 0.04 + i * W * 0.017, W * 0.04 + i * W * 0.017 + 1.6, H * 0.12, H * 0.52, 0.4, BACK_DARK)

for i in range(20):
    back_box(f"back_vent_right{i}", W * 0.60 + i * W * 0.019, W * 0.60 + i * W * 0.019 + 1.6, H * 0.38, H * 0.53, 0.4, BACK_DARK)

for name, z0 in (("scsi_upper", H * 0.36), ("scsi_lower", H * 0.18)):
    back_box(name + "_shell",  W * 0.28, W * 0.55, z0, z0 + H * 0.12, 2.5, BACK_METAL)
    back_box(name + "_insert", W * 0.30, W * 0.53, z0 + H * 0.025, z0 + H * 0.095, 0.5, BACK_DARK, y = 2.5)
    back_box(name + "_bail_l", W * 0.265, W * 0.275, z0 + H * 0.02, z0 + H * 0.10, 4.0, BACK_METAL)
    back_box(name + "_bail_r", W * 0.555, W * 0.565, z0 + H * 0.02, z0 + H * 0.10, 4.0, BACK_METAL)

back_box("scsi_id_selector", W * 0.575, W * 0.60, H * 0.12, H * 0.20, 1.5, BACK_DARK)
back_box("barcode_label",    W * 0.31,  W * 0.55, H * 0.03, H * 0.13, 0.3, BACK_LABEL)
back_box("iec_inlet",        W * 0.64,  W * 0.74, H * 0.10, H * 0.30, 2.0, BACK_DARK)
back_box("power_switch",     W * 0.83,  W * 0.88, H * 0.12, H * 0.30, 2.0, BACK_DARK)
back_box("power_rocker",     W * 0.84,  W * 0.87, H * 0.15, H * 0.24, 1.0, (0.20, 0.20, 0.20), y = 2.0)

# ---------------------------------------------------------------- the bottom
#
# From the underside photograph (hard-disk-20sc-03.jpg): grooves front to
# back, the product label in the middle, the "Made in USA" sticker toward the
# front left, and a row of vent slots near the front edge.

for i in range(16):
    x = W * 0.08 + i * (W * 0.84) / 15 - 0.7
    bottom = bottom.cut(cq.Workplane("XY").box(1.4, D * 0.55, 1.6, centered=(False, False, False))
                        .translate((x, D * 0.36, -0.8)))

m.add("case_bottom", bottom, CASE, angular=CORNER_ANG)

m.add("bottom_label",
      cq.Workplane("XY").box(W * 0.36, D * 0.11, 0.3, centered=(False, False, False))
        .translate((W * 0.40, D * 0.51, -0.25)),
      BACK_LABEL)
m.add("bottom_made_in_usa",
      cq.Workplane("XY").box(W * 0.17, D * 0.03, 0.3, centered=(False, False, False))
        .translate((W * 0.64, D * 0.31, -0.25)),
      BACK_LABEL)

for i in range(22):
    m.add(f"bottom_vent{i}",
          cq.Workplane("XY").box(1.4, D * 0.09, 0.4, centered=(False, False, False))
            .translate((W * 0.10 + i * W * 0.037, D * 0.16, -0.3)),
          BACK_DARK)

# ------------------------------------------------------------------ anchors
#
# The case's front plane, for the scene to line drives up on, buried inside
# the case.

m.add("front_anchor",
      cq.Workplane("XY").box(1.0, 1.0, 1.0).translate((W * 0.5, 2.0, H * 0.5)),
      KD["front_anchor"])

# --------------------------------------------------------------- underside

# Four rectangular feet near the corners, as on the underside photograph.
FOOT_W, FOOT_D, FOOT_H, FOOT_IN = 12.0, 22.0, 3.0, 10.0

for fx in (FOOT_IN, W - FOOT_IN - FOOT_W):
    for fy in (FOOT_IN, D - FOOT_IN - FOOT_D):
        m.add(f"foot_{fx:.0f}_{fy:.0f}",
              cq.Workplane("XY")
                .box(FOOT_W, FOOT_D, FOOT_H, centered=(False, False, False))
                .translate((fx, fy, -FOOT_H)),
              FOOT)


if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "HardDisk20SC.mesh"),
                    os.path.join(out, "HardDisk20SC.mtl"), "HardDisk20SC.mtl")
    print(f"HardDisk20SC (CAD): {nv} verts, {nt} tris")
