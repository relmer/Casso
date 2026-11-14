"""The case the UniDisk 3.5 and the Apple 3.5 Drive share, as a solid model.
X right, Y back, Z up; front face at y = 0.

MODELED FROM PHOTOGRAPHS, for the owner's review (T108):

  - The UniDisk 3.5, straight on: retrotechwiki.com/File:Apple_Unidisk_3.5.jpg
  - The Apple 3.5 Drive (A9M0106), front, back and bottom:
    computers.popcorn.cx/apple/peripherals/drives/floppy/a9m0106-01.jpg to -03

What the photographs settle:

  - THE TWO SHELLS. The lid comes down over the top sixth of the front as a
    band, standing a little proud of the lower front.
  - THE SLOT, directly under the band, from 11% to 88% of the width, about
    5 mm tall.
  - THE EJECT BLOCK to the slot's right: a small square boss under the band,
    with a pinhole.
  - THE BADGE, the rainbow Apple, at the lower left of the front.
  - THE TOP, ten grooves front to back inside a plain border.
  - The two drives are one case in two colors: the UniDisk 3.5 in the //c's
    Snow White, the Apple 3.5 Drive in Platinum. The dimensions are the Apple
    3.5 Drive's from the Computer History Museum's catalog entry 102735399,
    4 3/4 in wide, 2 1/4 in high and 8 1/4 in deep.

The activity lamp is a small red window at the left end of the band, where
the teardown at ifixit.com puts the LED.

Sub-mesh identity is by part NAME and color: `lamp` is the activity lamp in
the drive-lamp color and `front_anchor` the front plane the scene lines
devices up on.
"""

import cadquery as cq
from cadkit import KD, Model

from cad_badge import badge

INCH       = 25.4
W, D, H    = 4.75 * INCH, 8.25 * INCH, 2.25 * INCH
CORNER_ANG = 0.14
EDGE_R     = INCH * 3.0 / 32.0

SLOT_C     = (0.070, 0.070, 0.075)
FOOT_C     = (0.180, 0.180, 0.190)

BAND_Z     = H * 0.83          # the lid's lower edge on the front
RECESS     = 2.5               # the lower front behind the band


def build(case_rgb):
    m = Model()

    lower = (cq.Workplane("XY")
             .box(W, D - RECESS, BAND_Z, centered=(False, False, False))
             .translate((0, RECESS, 0))
             .edges("|Z").fillet(EDGE_R)
             .edges("<Z").fillet(EDGE_R * 0.5))

    lid = (cq.Workplane("XY")
           .box(W, D, H - BAND_Z, centered=(False, False, False))
           .translate((0, 0, BAND_Z))
           .edges("|Z").fillet(EDGE_R)
           .edges(">Z").fillet(EDGE_R))

    # Ten grooves front to back inside a border: 10 mm at the sides, 14 mm
    # at the front, 8 mm at the back.
    grooves, groove_w, depth = 10, 4.0, 2.0
    x0, x1 = 10.0, W - 10.0

    for i in range(grooves):
        x = x0 + (i + 0.5) * (x1 - x0) / grooves - groove_w * 0.5
        lid = lid.cut(cq.Workplane("XY")
                      .box(groove_w, D - 22.0, depth + 1.0, centered=(False, False, False))
                      .translate((x, 14.0, H - depth)))

    m.add("case_lower", lower, case_rgb, angular=CORNER_ANG)
    m.add("case_lid",   lid,   case_rgb, angular=CORNER_ANG)

    # The slot, a dark opening in the lower front right under the band.
    slot_x0, slot_x1 = W * 0.11, W * 0.88
    slot_h           = 5.0
    m.add("slot",
          cq.Workplane("XY")
            .box(slot_x1 - slot_x0, 0.4, slot_h, centered=(False, False, False))
            .translate((slot_x0, RECESS - 0.25, BAND_Z - slot_h)),
          SLOT_C)

    # The eject block: a boss to the slot's right with a pinhole.
    block_x0 = W * 0.895
    m.add("eject",
          cq.Workplane("XY")
            .box(W * 0.075, RECESS + 0.6, 8.0, centered=(False, False, False))
            .translate((block_x0, -0.6, BAND_Z - 8.5)),
          case_rgb)
    m.add("eject_hole",
          cq.Workplane("XZ").circle(0.6).extrude(0.3)
            .translate((block_x0 + W * 0.0375, -0.6, BAND_Z - 4.5)),
          SLOT_C)

    # The lamp at the band's left end.
    m.add("lamp",
          cq.Workplane("XY")
            .box(3.5, 0.5, 1.6, centered=(False, False, False))
            .translate((W * 0.05, -0.3, BAND_Z + 1.6)),
          KD["drive_lamp"])

    badge(m, W * 0.10, H * 0.10, 11.0, y = RECESS - 0.35)

    m.add("front_anchor",
          cq.Workplane("XY").box(1.0, 1.0, 1.0).translate((W * 0.5, RECESS + 4.0, H * 0.5)),
          KD["front_anchor"])

    foot_w, foot_d, foot_h, foot_in = 9.0, 14.0, 2.0, 10.0

    for fx in (foot_in, W - foot_in - foot_w):
        for fy in (foot_in, D - foot_in - foot_d):
            m.add(f"foot_{fx:.0f}_{fy:.0f}",
                  cq.Workplane("XY")
                    .box(foot_w, foot_d, foot_h, centered=(False, False, False))
                    .translate((fx, fy, -foot_h)),
                  FOOT_C)

    return m
