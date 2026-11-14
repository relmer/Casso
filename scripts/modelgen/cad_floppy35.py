"""The case the UniDisk 3.5 and the Apple 3.5 Drive share, as a solid model,
all six sides. X right, Y back, Z up; front face at y = 0.

MODELED FROM PHOTOGRAPHS, for the owner's review (T108):

  - The UniDisk 3.5, straight on: retrotechwiki.com/File:Apple_Unidisk_3.5.jpg
  - The Apple 3.5 Drive (A9M0106), front, back and bottom:
    computers.popcorn.cx/apple/peripherals/drives/floppy/a9m0106-01.jpg to -03

What the photographs settle:

  FRONT. One flat face. The lid's seam crosses it a sixth of the way down.
  Directly under the seam, the slot runs from 11% to 83% of the width; to
  its right, the eject button is a rectangle as tall as the slot, and to its
  left the activity LED sits in line with it. The rainbow Apple is at the
  lower left.

  TOP. Ten grooves front to back inside a plain border.

  BACK. The lid comes down to about the middle of the back. Below the seam:
  the 19-pin daisy-chain socket in a rimmed opening at the lower left (the
  photograph has its rare dust cover on; the socket is modeled bare) with
  the drive mark above it; the Kensington lock loop right of center; and
  the cable's strain relief boot at the right. The iFixit teardown of the
  Apple 3.5 Drive lists the three.

  BOTTOM. A white label at the front end with the name; four felt pads at
  the corners; four screws; grooves along the length; and a shallow
  rounded panel at the back.

  SIDES. Plain, with the lid's seam falling from the front's height to the
  back's.

  The two drives are one case in two colors: the UniDisk 3.5 in the //c's
  Snow White, the Apple 3.5 Drive in Platinum. The dimensions are the Apple
  3.5 Drive's from the Computer History Museum's catalog entry 102735399,
  4 3/4 in wide, 2 1/4 in high and 8 1/4 in deep.

Sub-mesh identity is by part NAME and color: `lamp` is the activity LED in
the drive-lamp color and `front_anchor` the front plane the scene lines
devices up on.
"""

import math

import cadquery as cq
from cadkit import KD, Model

from cad_badge import badge

INCH       = 25.4
W, D, H    = 4.75 * INCH, 8.25 * INCH, 2.25 * INCH
CORNER_ANG = 0.14
EDGE_R     = INCH * 3.0 / 32.0

DARK       = (0.070, 0.070, 0.075)
FELT       = (0.820, 0.800, 0.760)
LABEL      = (0.950, 0.945, 0.930)
INK        = (0.150, 0.150, 0.160)
METAL      = (0.700, 0.700, 0.700)

SEAM_FRONT = H * 0.83          # the lid's lower edge on the front
SEAM_BACK  = H * 0.54          # and on the back
SEAM_W     = 0.7


def seam_cut(case):
    # The seam on the front, the back, and down each side between them.
    case = case.cut(cq.Workplane("XY").box(W + 2.0, 1.4, SEAM_W, centered=(False, False, False))
                    .translate((-1.0, -0.8, SEAM_FRONT)))
    case = case.cut(cq.Workplane("XY").box(W + 2.0, 1.4, SEAM_W, centered=(False, False, False))
                    .translate((-1.0, D - 0.6, SEAM_BACK)))

    drop  = SEAM_FRONT - SEAM_BACK
    angle = math.degrees(math.atan2(drop, D))
    run   = math.hypot(D, drop)

    for x in (-0.8, W - 0.6):
        side = (cq.Workplane("XY").box(1.4, run, SEAM_W, centered=(False, False, False))
                .rotate((0, 0, 0), (1, 0, 0), -angle)
                .translate((x, 0, SEAM_FRONT)))
        case = case.cut(side)

    return case


def build(case_rgb):
    m = Model()

    case = (cq.Workplane("XY")
            .box(W, D, H, centered=(False, False, False))
            .edges("|Z").fillet(EDGE_R)
            .edges(">Z").fillet(EDGE_R)
            .edges("<Z").fillet(EDGE_R * 0.5))

    case = seam_cut(case)

    # Top: ten grooves front to back, 10 mm in from the sides, 12 mm from
    # the front, 8 mm from the back.
    grooves, groove_w, depth = 10, 4.0, 2.0
    x0, x1 = 10.0, W - 10.0

    for i in range(grooves):
        x = x0 + (i + 0.5) * (x1 - x0) / grooves - groove_w * 0.5
        case = case.cut(cq.Workplane("XY")
                        .box(groove_w, D - 20.0, depth + 1.0, centered=(False, False, False))
                        .translate((x, 12.0, H - depth)))

    # Bottom: grooves along the length between the pads and screws.
    for i in range(8):
        x = W * 0.16 + i * (W * 0.68) / 7 - 0.8
        case = case.cut(cq.Workplane("XY")
                        .box(1.6, D * 0.62, 1.8, centered=(False, False, False))
                        .translate((x, D * 0.20, -0.8)))

    # Front: the slot under the seam.
    slot_x0, slot_x1 = W * 0.11, W * 0.83
    slot_h           = 5.0
    slot_z0          = SEAM_FRONT - 1.5 - slot_h
    case = case.cut(cq.Workplane("XY")
                    .box(slot_x1 - slot_x0, 8.0, slot_h, centered=(False, False, False))
                    .translate((slot_x0, -1.0, slot_z0)))

    m.add("case", case, case_rgb, angular=CORNER_ANG)
    m.add("slot_back",
          cq.Workplane("XY").box(slot_x1 - slot_x0, 0.5, slot_h, centered=(False, False, False))
            .translate((slot_x0, 6.5, slot_z0)),
          DARK)

    # The eject button: a rectangle as tall as the slot, to its right.
    m.add("eject",
          cq.Workplane("XY").box(W * 0.085, 1.0, slot_h, centered=(False, False, False))
            .translate((W * 0.85, -0.5, slot_z0)),
          case_rgb)
    m.add("eject_hole",
          cq.Workplane("XZ").circle(0.55).extrude(0.3)
            .translate((W * 0.8925, -0.6, slot_z0 + slot_h * 0.5)),
          DARK)

    # The activity LED, in line with the slot at its left.
    m.add("lamp",
          cq.Workplane("XY").box(3.5, 0.5, 2.0, centered=(False, False, False))
            .translate((W * 0.05, -0.3, slot_z0 + slot_h * 0.5 - 1.0)),
          KD["drive_lamp"])

    badge(m, W * 0.10, H * 0.10, 11.0)

    # Back: the socket's rimmed opening, the mark above it, the lock loop
    # and the strain relief boot, laid out as seen from behind, as in the
    # photograph, and mirrored across the centerline into the model's x.
    def back_add(name, solid, rgb):
        m.add(name, solid.mirror("YZ", (W * 0.5, 0, 0)), rgb)

    plate_x0, plate_x1 = W * 0.13, W * 0.57
    plate_z0, plate_z1 = H * 0.13, H * 0.46
    rim = (cq.Workplane("XY").box(plate_x1 - plate_x0, 0.6, plate_z1 - plate_z0, centered=(False, False, False))
           .edges("|Y").fillet(2.0)
           .translate((plate_x0, D, plate_z0)))
    inner = (cq.Workplane("XY").box(plate_x1 - plate_x0 - 2.0, 1.0, plate_z1 - plate_z0 - 2.0, centered=(False, False, False))
             .edges("|Y").fillet(1.5)
             .translate((plate_x0 + 1.0, D + 0.3, plate_z0 + 1.0)))
    back_add("back_plate", rim.cut(inner), case_rgb)

    # The 19-pin D socket in the opening: its metal shell and dark insert.
    db_w, db_h = 33.0, 9.0
    db_x0      = (plate_x0 + plate_x1) * 0.5 - db_w * 0.5
    db_z0      = (plate_z0 + plate_z1) * 0.5 - db_h * 0.5
    back_add("db19_opening",
          cq.Workplane("XY").box(plate_x1 - plate_x0 - 2.0, 0.3, plate_z1 - plate_z0 - 2.0, centered=(False, False, False))
            .translate((plate_x0 + 1.0, D + 0.05, plate_z0 + 1.0)),
          DARK)
    back_add("db19_shell",
          cq.Workplane("XZ").polyline([(0, db_h), (db_w, db_h), (db_w - 3.0, 0), (3.0, 0)]).close().extrude(-1.2)
            .translate((db_x0, D + 0.3, db_z0)),
          METAL)
    back_add("db19_insert",
          cq.Workplane("XZ").polyline([(1.5, db_h - 1.5), (db_w - 1.5, db_h - 1.5), (db_w - 3.8, 1.5), (3.8, 1.5)]).close().extrude(-0.4)
            .translate((db_x0, D + 1.5, db_z0)),
          DARK)
    back_add("back_mark",
          cq.Workplane("XY").box(3.0, 0.4, 4.0, centered=(False, False, False))
            .translate((W * 0.34, D, H * 0.50)),
          case_rgb)

    clip = (cq.Workplane("XY").box(W * 0.19, 9.0, H * 0.22, centered=(False, False, False))
            .translate((W * 0.62, D, H * 0.46)))
    clip = clip.cut(cq.Workplane("XY").box(W * 0.19 - 5.0, 10.0, H * 0.22 - 5.0, centered=(False, False, False))
                    .translate((W * 0.62 + 2.5, D - 0.5, H * 0.46 + 2.5)))
    back_add("lock_loop", clip, case_rgb)

    boot = (cq.Workplane("XY").box(W * 0.13, 12.0, H * 0.20, centered=(False, False, False))
            .edges("|Y").fillet(2.5)
            .translate((W * 0.82, D, H * 0.52)))
    back_add("cable_boot", boot, (0.740, 0.735, 0.715))

    # Bottom: the label, the felt pads, the screws, and the back panel.
    m.add("bottom_label",
          cq.Workplane("XY").box(W * 0.84, D * 0.13, 0.3, centered=(False, False, False))
            .translate((W * 0.08, D * 0.02, -0.25)),
          LABEL)
    m.add("bottom_label_text",
          cq.Workplane("XY").text("Apple 3.5 Drive", 4.0, 0.2, font="Times New Roman", halign="left", valign="center")
            .mirror("XZ")
            .translate((W * 0.36, D * 0.08, -0.45)),
          INK)

    for fx in (W * 0.06, W * 0.84):
        for fy in (D * 0.16, D * 0.76):
            m.add(f"pad_{fx:.0f}_{fy:.0f}",
                  cq.Workplane("XY").box(W * 0.10, D * 0.075, 2.0, centered=(False, False, False))
                    .translate((fx, fy, -2.0)),
                  FELT)

    for sx in (W * 0.23, W * 0.77):
        for sy in (D * 0.19, D * 0.72):
            m.add(f"screw_{sx:.0f}_{sy:.0f}",
                  cq.Workplane("XY").cylinder(0.4, 2.4, centered=(True, True, False))
                    .translate((sx, sy, -0.35)),
                  METAL)

    m.add("bottom_panel",
          cq.Workplane("XY").box(W * 0.46, D * 0.09, 0.3, centered=(False, False, False))
            .edges("|Z").fillet(2.0)
            .translate((W * 0.30, D * 0.80, -0.25)),
          case_rgb)

    m.add("front_anchor",
          cq.Workplane("XY").box(1.0, 1.0, 1.0).translate((W * 0.5, 6.0, H * 0.5)),
          KD["front_anchor"])

    return m
