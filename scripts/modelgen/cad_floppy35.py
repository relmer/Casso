"""The case the UniDisk 3.5 and the Apple 3.5 Drive share, as a solid model,
all six sides. X right, Y back, Z up; front face at y = 0.

MODELED FROM PHOTOGRAPHS, for the owner's review (T108):

  - The UniDisk 3.5, straight on: retrotechwiki.com/File:Apple_Unidisk_3.5.jpg
  - The Apple 3.5 Drive (A9M0106), front, back and bottom:
    computers.popcorn.cx/apple/peripherals/drives/floppy/a9m0106-01.jpg to -03

What the photographs settle:

  FRONT. One flat face. The lid's seam crosses it an eighth of the way down.
  Below the seam, the slot runs from 11% to 83% of the width; to
  its right, the eject button is a rectangle as tall as the slot, and to its
  left the activity LED sits in line with it. The rainbow Apple is at the
  lower left.

  TOP. Ten grooves front to back inside a plain border.

  BACK, from the iFixit teardown's straight-on photograph of the Apple 3.5
  Drive's back (ifixit.com/Teardown/92324, step 1), measured at 0.11 mm a
  pixel from the 4 3/4 in width, as seen from behind:
    - A recessed well with rounded corners at the lower left, 14.6 to 66.2
      mm from the left and 6.0 to 20.9 mm up, and in it the 19-pin
      daisy-chain socket: a D shell with rounded corners on a metal flange,
      a dark insert with ten receptacles over nine, and a hex screw post
      each side.
    - The floppy disk mark recessed into the case above the well.
    - The lock bracket right of the well, a hollow loop protruding from
      the back with its opening on top, 70.3 to 90.8 mm from the left and
      17.6 to 30.4 mm up, with a chain link recessed into its back face.
  The fixed cable is at the right; the cable and its boot are not modeled.

  The Apple 3.5 Drive's seam is two thirds of the way up, front and back,
  and its slot is right under the seam (the teardown's photographs); the
  UniDisk 3.5's are where its photograph puts them, below.

  BOTTOM. A white label at the front end with the name; four felt pads at
  the corners; four screws; grooves along the length; and a shallow
  rounded panel at the back.

  SIDES. Plain, with the lid's seam level along them.

  The two drives are one case in two colors: the UniDisk 3.5 in the //c's
  Snow White, the Apple 3.5 Drive in Platinum. The dimensions are the Apple
  3.5 Drive's from the Computer History Museum's catalog entry 102735399,
  4 3/4 in wide and 8 1/4 in deep. The case is 1.85 in high: the
  straight-on photograph puts the front's height at 0.39 of its width, the
  lid's seam at 0.88 of the height and the slot between 0.64 and 0.74.

Sub-mesh identity is by part NAME and color: `lamp` is the activity LED in
the drive-lamp color and `front_anchor` the front plane the scene lines
devices up on.
"""

import math

import cadquery as cq
from cadkit import KD, Model

from cad_badge import badge

INCH       = 25.4
W, D, H    = 4.75 * INCH, 8.25 * INCH, 1.85 * INCH    # the case; the feet add to it
CORNER_ANG = 0.14
EDGE_R     = INCH * 3.0 / 32.0

DARK       = (0.070, 0.070, 0.075)
FOOT_C     = (0.300, 0.300, 0.320)     # the clear feet, drawn translucent gray
LABEL      = (0.950, 0.945, 0.930)
INK        = (0.150, 0.150, 0.160)
METAL      = (0.700, 0.700, 0.700)
INSERT     = (0.180, 0.180, 0.190)     # clear of the desk scene's finish markers

SEAM_W     = 0.7


def seam_cut(case, seam_z):
    # The seam, level all the way round: front, back and both sides.
    case = case.cut(cq.Workplane("XY").box(W + 2.0, 1.4, SEAM_W, centered=(False, False, False))
                    .translate((-1.0, -0.8, seam_z)))
    case = case.cut(cq.Workplane("XY").box(W + 2.0, 1.4, SEAM_W, centered=(False, False, False))
                    .translate((-1.0, D - 0.6, seam_z)))

    for x in (-0.8, W - 0.6):
        case = case.cut(cq.Workplane("XY").box(1.4, D + 2.0, SEAM_W, centered=(False, False, False))
                        .translate((x, -1.0, seam_z)))

    return case


# THE BACK, laid out as seen from behind (x from the left as seen from
# behind, z up from the case's bottom), and placed in the model's x by
# mirroring across the centerline.
def back_x(x):
    return W - x


def back_box(x0, x1, z0, z1, y0, y1):
    return (cq.Workplane("XY")
            .box(x1 - x0, y1 - y0, z1 - z0, centered=(False, False, False))
            .translate((back_x(x1), y0, z0)))


WELL_X0, WELL_X1, WELL_Z0, WELL_Z1 = 14.6, 66.2, 6.0, 20.9
WELL_DEPTH                         = 5.0
WELL_R                             = 2.5


def back_cuts(case):
    # The socket's well, recessed into the back with rounded corners.
    well = (back_box(WELL_X0, WELL_X1, WELL_Z0, WELL_Z1, D - WELL_DEPTH, D + 1.0)
            .edges("|Y").fillet(WELL_R))
    case = case.cut(well)

    # The floppy disk mark above the well, recessed: the disk's outline, its
    # shutter at the top and its label at the bottom.
    cx, z0, w, h, depth, stroke = 40.4, 22.5, 4.7, 5.3, 0.4, 0.55
    ring = (back_box(cx - w / 2, cx + w / 2, z0, z0 + h, D - depth, D + 1.0)
            .cut(back_box(cx - w / 2 + stroke, cx + w / 2 - stroke, z0 + stroke, z0 + h - stroke, D - depth - 1.0, D + 2.0)))
    case = case.cut(ring)
    case = case.cut(back_box(cx - 1.1, cx + 0.9, z0 + h - stroke - 1.7, z0 + h - stroke, D - depth, D + 1.0))
    case = case.cut(back_box(cx - 1.4, cx + 1.4, z0 + stroke + 0.5, z0 + stroke + 1.0, D - depth, D + 1.0))

    return case


def back_parts(m, case_rgb):
    wall = D - WELL_DEPTH              # the well's back wall, where the socket mounts

    # The socket's metal flange, with the D shell protruding from it.
    flange_x0, flange_x1, flange_z0, flange_z1 = 18.6, 62.6, 8.4, 18.6
    m.add("db19_flange",
          back_box(flange_x0, flange_x1, flange_z0, flange_z1, wall, wall + 0.8)
            .edges("|Y").fillet(1.2),
          METAL)

    db_cx, db_cz       = 40.4, 13.4
    db_top             = 30.0
    db_h               = 7.7
    db_out             = 3.4

    # The shell and the insert are offsets of one core trapezoid, so the
    # shell's rim is the same width all round the insert and its corners are
    # concentric with the insert's.
    insert_r, shell_wall = 1.4, 0.7
    core_top            = db_top - 2.0 * (insert_r + shell_wall)
    # The sides taper 10 degrees from vertical, as a D connector's do.
    core_bottom         = core_top - 2.0 * (db_h - 2.0 * (insert_r + shell_wall)) * math.tan (math.radians (10.0))
    core_h              = db_h - 2.0 * (insert_r + shell_wall)

    def d_shape(offset, y0, y1):
        cx = back_x(db_cx)
        pts = [(cx - core_top / 2, db_cz + core_h / 2), (cx + core_top / 2, db_cz + core_h / 2),
               (cx + core_bottom / 2, db_cz - core_h / 2), (cx - core_bottom / 2, db_cz - core_h / 2)]

        return (cq.Workplane("XZ").polyline(pts).close().offset2D(offset, "arc").extrude(-(y1 - y0))
                .translate((0, y0, 0)))

    shell = (d_shape(insert_r + shell_wall, wall + 0.8, wall + 0.8 + db_out)
             .cut(d_shape(insert_r, wall + 0.5, wall + 0.8 + db_out + 1.0)))
    m.add("db19_shell", shell, METAL)

    # The insert, set back in the shell, with nineteen receptacles: ten over
    # nine on the D connectors' 2.77 mm pitch, through to the dark behind.
    insert_face = wall + 0.8 + db_out - 1.2
    insert      = d_shape(insert_r, wall + 1.6, insert_face)
    pitch       = 2.77

    for dz, count in ((db_h * 0.17, 10), (-db_h * 0.17, 9)):
        x_first = db_cx - (count - 1) * pitch * 0.5

        for i in range(count):
            insert = insert.cut(cq.Workplane("XZ").circle(0.5).extrude(-3.0)
                                .translate((back_x(x_first + i * pitch), wall + 1.0, db_cz + dz)))

    m.add("db19_insert", insert, INSERT)
    m.add("db19_back", d_shape(insert_r - 0.1, wall + 0.8, wall + 1.6), DARK)

    # The hex screw posts each side of the shell, with their threaded holes.
    for px in (21.8, 59.0):
        post = (cq.Workplane("XZ").polygon(6, 4.6).extrude(-3.6)
                .translate((back_x(px), wall + 0.8, db_cz)))
        post = post.cut(cq.Workplane("XZ").circle(0.9).extrude(-2.0)
                        .translate((back_x(px), wall + 3.0, db_cz)))
        m.add(f"db19_post_{px:.0f}", post, METAL)

    # The lock bracket: a hollow loop protruding from the back, its opening
    # on top for the lock's cable, with a chain link recessed into its back
    # face.
    lx0, lx1, lz0, lz1 = 70.3, 90.8, 17.6, 30.4
    out, wall_t         = 8.0, 2.2
    loop = (back_box(lx0, lx1, lz0, lz1, D - 0.5, D + out)
            .edges("|Z").fillet(1.5))
    loop = loop.cut(back_box(lx0 + wall_t, lx1 - wall_t, lz0 - 1.0, lz1 + 1.0, D - 1.0, D + out - wall_t))

    # The chain link is centered on the loop's back face, both ways.
    link_w, link_h, overlap, stroke, depth = 2.4, 3.6, 1.2, 0.5, 0.35
    ccx                                     = (lx0 + lx1) * 0.5
    chain_z0                                = (lz0 + lz1) * 0.5 - (2.0 * link_h - overlap) * 0.5

    for k in range(2):
        z0   = chain_z0 + k * (link_h - overlap)
        ring = (back_box(ccx - link_w / 2, ccx + link_w / 2, z0, z0 + link_h, D + out - depth, D + out + 1.0)
                .edges("|Y").fillet(link_w / 2 - 0.01))
        hole = (back_box(ccx - link_w / 2 + stroke, ccx + link_w / 2 - stroke, z0 + stroke, z0 + link_h - stroke,
                         D + out - depth - 1.0, D + out + 2.0)
                .edges("|Y").fillet(link_w / 2 - stroke - 0.01))
        loop = loop.cut(ring.cut(hole))

    m.add("lock_loop", loop, case_rgb)


def build(case_rgb, seam_frac=0.88, slot_top_frac=0.74):
    m       = Model()
    seam_z  = H * seam_frac            # the lid's lower edge, level all round

    case = (cq.Workplane("XY")
            .box(W, D, H, centered=(False, False, False))
            .edges("|Z").fillet(EDGE_R)
            .edges(">Z").fillet(EDGE_R)
            .edges("<Z").fillet(EDGE_R * 0.5))

    case = seam_cut(case, seam_z)
    case = back_cuts(case)

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
    slot_z0          = H * slot_top_frac - slot_h
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

    badge(m, W * 0.11, H * 0.10, 9.0)

    back_parts(m, case_rgb)

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
                  cq.Workplane("XY").box(W * 0.10, D * 0.075, 3.0, centered=(False, False, False))
                    .translate((fx, fy, -3.0)),
                  FOOT_C)

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
