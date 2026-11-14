"""The 3.5-inch drive case the UniDisk 3.5 and the Apple 3.5 Drive have in
common, as a solid model: a molded box with a recessed front bezel, the disk
slot across it, an eject button below the slot's right end, and the in-use
lamp at the left. X right, Y back, Z up; front face at y = 0.

cad_unidisk35.py and cad_apple35.py call build() with their dimensions and
colors.

Sub-mesh identity is by part NAME and color: `lamp` is the in-use lamp in the
drive-lamp color, `slot` the disk slot, `eject` the eject button, and
`front_anchor` the front plane the scene lines devices up on.
"""

import cadquery as cq
from cadkit import KD, Model

INCH       = 25.4
CORNER_ANG = 0.14
SLOT_C     = (0.090, 0.090, 0.095)
FOOT_C     = (0.180, 0.180, 0.190)


def build(W, D, H, case_rgb, bezel_rgb, slot_w, slot_z, grooves=0):
    """W, D, H in mm. slot_w is the slot's width and slot_z its height above
    the base; grooves is the count of horizontal grooves across the front
    below the slot."""

    m      = Model()
    edge_r = INCH * 3.0 / 32.0

    case = (cq.Workplane("XY")
            .box(W, D, H, centered=(False, False, False))
            .edges("|Y").fillet(edge_r * 1.5)
            .edges("|Z").fillet(edge_r))

    # The bezel: the front 3 mm recessed 0.8 mm inside a 4 mm border.
    case = case.cut(cq.Workplane("XY")
                    .box(W - 8.0, 0.8 + 1.0, H - 8.0, centered=(False, False, False))
                    .translate((4.0, -1.0, 4.0)))

    # Horizontal grooves under the slot.
    for i in range(grooves):
        z = 8.0 + i * 3.2

        if z + 1.0 >= slot_z - 3.0:
            break

        case = case.cut(cq.Workplane("XY")
                        .box(W - 16.0, 1.4 + 0.8, 1.0, centered=(False, False, False))
                        .translate((8.0, -0.2, z)))

    m.add("case", case, case_rgb, angular=CORNER_ANG)
    m.add("bezel",
          cq.Workplane("XY")
            .box(W - 8.0, 0.3, H - 8.0, centered=(False, False, False))
            .translate((4.0, 0.5, 4.0)),
          bezel_rgb)

    # The slot: a dark opening 3.2 mm tall, centered across the front.
    slot_x0 = (W - slot_w) * 0.5
    m.add("slot",
          cq.Workplane("XY")
            .box(slot_w, 0.4, 3.2, centered=(False, False, False))
            .translate((slot_x0, 0.25, slot_z)),
          SLOT_C)

    # The eject button below the slot's right end, and the lamp at its left.
    m.add("eject",
          cq.Workplane("XY")
            .box(9.0, 1.4, 4.0, centered=(False, False, False))
            .translate((slot_x0 + slot_w - 11.0, -0.6, slot_z - 8.0)),
          bezel_rgb)
    m.add("lamp",
          cq.Workplane("XY")
            .box(5.0, 0.6, 2.0, centered=(False, False, False))
            .translate((slot_x0, 0.2, slot_z - 7.0)),
          KD["drive_lamp"])

    m.add("front_anchor",
          cq.Workplane("XY").box(1.0, 1.0, 1.0).translate((W * 0.5, 2.0, H * 0.5)),
          KD["front_anchor"])

    foot_r, foot_h, foot_in = 5.0, 2.0, 12.0

    for fx in (foot_in, W - foot_in):
        for fy in (foot_in, D - foot_in):
            m.add(f"foot_{fx:.0f}_{fy:.0f}",
                  cq.Workplane("XY")
                    .cylinder(foot_h, foot_r, centered=(True, True, False))
                    .translate((fx, fy, -foot_h)),
                  FOOT_C)

    return m
