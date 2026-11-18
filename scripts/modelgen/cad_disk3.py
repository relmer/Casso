"""Apple Disk /// (A3M0004), the Apple ///'s external drive, as a solid
model. 160 x 229 x 99 mm (W x D x H), feet included. X right, Y back, Z
up; the upper front face is at y = 0, and the block below the slot is
proud of it.

MODELED FROM PHOTOGRAPHS. Resources/Models/Disk3/README.md has the list of
them and the measurements taken from each.

The front is a beige face with chamfered vertical corners and a rounded
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
from cad_badge import badge, read_branding

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
TOP_R         = 7.0       # the front top edge, rounded
EDGE_C        = 3.5       # the top side, rear top and bottom edges, chamfered
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

# The rear recess and the label in it, from disk3-rear-rr3483512.jpg at 0.1
# mm per pixel. The label's contents are from disk3-rear-label-oc.jpg at
# 0.035 mm per pixel, measured from the label's top left as seen from behind.
REAR_X0, REAR_X1 = 18.5, 141.4
REAR_Z0, REAR_Z1 = 35.5, 92.0
REAR_DEEP        = 8.0

RL_X0, RL_X1     = 19.2, 140.7
RL_Z0, RL_Z1     = 36.7, 81.2

STRIPE_U0        = 77.1
STRIPE_PITCH     = 1.93
STRIPE_W         = 0.4

FONT_GOTHIC      = r"C:\Windows\Fonts\GOTHIC.TTF"
FONT_GOTHIC_BOLD = r"C:\Windows\Fonts\GOTHICB.TTF"
FONT_ARIAL       = r"C:\Windows\Fonts\arial.ttf"
FONT_ARIAL_BOLD  = r"C:\Windows\Fonts\arialbd.ttf"
FONT_COURIER     = r"C:\Windows\Fonts\cour.ttf"

# Four square rubber feet, measured from the bottom photograph.
FOOT_W   = 22.0
FOOT_XS  = (25.9, 113.6)
FOOT_YS  = (55.6, 179.6)

# The case and latch colors are the photographs' plastic, white-balanced
# against each photograph's background: a lighter, warmer cream than the
# Disk II's beige, with a latch a little paler and grayer than the case.
CASE     = (0.905, 0.840, 0.700)
LATCH    = (0.905, 0.860, 0.755)
LABEL_RD = (0.860, 0.150, 0.165)
BLUE     = (0.200, 0.420, 0.760)
SLOT_DK  = (0.055, 0.055, 0.062)
RECESS   = (0.160, 0.130, 0.110)
BLACK    = (0.070, 0.070, 0.075)
WHITE    = (0.920, 0.920, 0.900)
METAL    = (0.560, 0.565, 0.575)
RUBBER   = (0.180, 0.176, 0.170)


def box(x0, y0, z0, x1, y1, z1):
    return (cq.Workplane("XY")
              .box(x1 - x0, y1 - y0, z1 - z0, centered=(False, False, False))
              .translate((x0, y0, z0)))


m = Model()

# ---------------------------------------------------------------------- case

# The edges are treated as the ProFile's are: the front top edge and the
# rear corners are rounded, and every other edge is a 45-degree chamfer.
case = box(0.0, 0.0, FOOT_H, W, D, TOP)
case = case.edges("|Z and <Y").chamfer(FRONT_CHAMFER)
case = case.edges("|Z and >Y").fillet(REAR_R)
case = case.faces(">Z").edges(cq.selectors.BoxSelector((-1.0, -1.0, TOP - 1.0),
                                                       (W + 1.0, FRONT_CHAMFER + 0.5, TOP + 1.0))).fillet(TOP_R)
case = case.faces(">Z").edges(cq.selectors.BoxSelector((-1.0, FRONT_CHAMFER + 0.5, TOP - 1.0),
                                                       (W + 1.0, D + 1.0, TOP + 1.0))).chamfer(EDGE_C)
case = case.faces("<Z").edges().chamfer(EDGE_C)

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

# The recess's dark lining, below the latch. Every visible face of it is
# inside the case's cut faces rather than on them, so the two never share a
# plane: the floor's top is LINING_T above the notch's floor.
LINING_T = 0.6

m.add("recess",
      box(NOTCH_X0, NOTCH_D - 1.0, NOTCH_Z0, NOTCH_X0 + NOTCH_W, NOTCH_D, TAB_Z0)
        .union(box(NOTCH_X0, BLOCK_FRONT + 0.5, NOTCH_Z0 - 1.0, NOTCH_X0 + NOTCH_W, NOTCH_D, NOTCH_Z0 + LINING_T))
        .union(box(NOTCH_X0, BLOCK_FRONT + 0.5, NOTCH_Z0, NOTCH_X0 + LINING_T, NOTCH_D, TAB_Z0))
        .union(box(NOTCH_X0 + NOTCH_W - LINING_T, BLOCK_FRONT + 0.5, NOTCH_Z0, NOTCH_X0 + NOTCH_W, NOTCH_D, TAB_Z0)),
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

def lettering(text, font, width, cx, cz, y0, back=False, thick=0.12):
    """`text` set `width` mm wide and centered on (cx, cz), standing `thick`
    out from y0 toward the viewer: the front's at -Y, or the back's at +Y.
    Set at a probe size first, so each line's size is the one that makes it
    as wide as the photograph's."""
    probe = cq.Workplane("XZ").text(text, 10.0, thick, fontPath=font, halign="center", valign="center")
    size  = 10.0 * width / probe.val().BoundingBox().xlen
    wp    = cq.Workplane("XZ").text(text, size, thick, fontPath=font, halign="center", valign="center")

    if back:
        wp = wp.mirror("YZ")

    bb = wp.val().BoundingBox()
    dy = (y0 - bb.ymin) if back else (y0 - bb.ymax)

    return wp.translate((cx - (bb.xmin + bb.xmax) * 0.5, dy, cz - (bb.zmin + bb.zmax) * 0.5))


# The drive label, from the close-up of it in disk3-front-oc.jpg at 0.052 mm
# per pixel: a black plate with "disk ///" across its top, and a white field
# with "DRIVE" over the drive's number. The number differs per drive, so the
# scene stamps it (DeskSceneModel::StampDisk3Number).
m.add("label",
      box(LABEL_X0, -0.4, LABEL_Z0, LABEL_X1, 0.5, LABEL_Z1).edges("|Y").fillet(1.5),
      BLACK)
m.add("label_field",
      box(10.7, -0.55, 70.95, 23.55, 0.5, 80.3).edges("|Y").fillet(1.0),
      WHITE)
m.add("label_logotype", lettering("disk ///", FONT_ARIAL_BOLD, 9.8, 16.8, 82.75, -0.4), WHITE)
m.add("label_word",     lettering("DRIVE",    FONT_ARIAL,      6.6, 17.15, 78.1, -0.55), BLACK)

# The logo plate on the block, with the badge at its left.
m.add("logo_plate",
      box(PLATE_X0, BLOCK_FRONT - 0.4, PLATE_Z0, PLATE_X1, BLOCK_FRONT + 0.5, PLATE_Z1).edges("|Y").fillet(2.0),
      BLACK)
badge(m, PLATE_X0 + 2.5, PLATE_Z0 + 2.0, PLATE_Z1 - PLATE_Z0 - 4.0, y=BLOCK_FRONT - 0.7)

# ---------------------------------------------------------------------- back

#
# Seen from behind, the viewer's left is the model's +X. Positions on the
# back are given as the photographs measure them: `u` from the left as seen
# from behind and `t` or `v` down from the top.

YP = D - REAR_DEEP + 0.5      # the metal plate's face
YL = YP + 0.1                 # the label's face
YW = YL + 0.1                 # the white panels' face


def back_place(wp, y0):
    """Moves `wp` so its back-facing side starts at y0."""
    return wp.translate((0.0, y0 - wp.val().BoundingBox().ymin, 0.0))


def recess_box(u0, t0, u1, t1, y0, y1):
    return box(REAR_X1 - u1, y0, REAR_Z1 - t1, REAR_X1 - u0, y1, REAR_Z1 - t0)


def label_box(u0, v0, u1, v1, y0, y1):
    return box(RL_X1 - u1, y0, RL_Z1 - v1, RL_X1 - u0, y1, RL_Z1 - v0)


def label_text(text, font, u0, u1, v, y0):
    return lettering(text, font, u1 - u0, RL_X1 - (u0 + u1) * 0.5, RL_Z1 - v, y0, back=True)


m.add("rear_plate", box(REAR_X0, D - REAR_DEEP, REAR_Z0, REAR_X1, YP, REAR_Z1), METAL)

# The connector above the label: the blue header in its opening, the
# plate's screw, and the black socket at the right.
m.add("rear_opening",   recess_box(1.0, 0.3, 44.5, 9.0, YP, YP + 0.05), BLACK)
m.add("rear_header",    recess_box(1.6, 0.9, 43.9, 8.4, YP, YP + 1.5), BLUE)
m.add("rear_header_in", recess_box(4.0, 2.4, 41.5, 6.9, YP, YP + 1.55), BLACK)
m.add("rear_socket",    recess_box(83.5, 0.5, 90.0, 9.0, YP, YP + 2.0), BLACK)
m.add("rear_screw",
      back_place(cq.Workplane("XZ").center(REAR_X1 - 62.5, REAR_Z1 - 4.0).circle(3.6).extrude(1.0), YP),
      (0.700, 0.705, 0.715))
m.add("rear_screw_slot",
      recess_box(60.3, 3.7, 64.7, 4.3, YP, YP + 1.1).union(recess_box(62.2, 1.8, 62.8, 6.2, YP, YP + 1.1)),
      (0.300, 0.300, 0.310))

# The label, from disk3-rear-label-oc.jpg: red, with white panels lettered in
# red and white lettering on the red, and white stripes across its right
# end. The mark at the left of "apple computer inc." is Casso's silhouette,
# as on the front.
m.add("rear_label", label_box(0.0, 0.0, RL_X1 - RL_X0, RL_Z1 - RL_Z0, YP, YL), LABEL_RD)

# The warning panel runs on into the arrow's head through a neck, with a
# round-ended slot in from the panel's top and bottom on each side of it.
def neck_slot(v_end, toward_top):
    r      = 2.1
    center = v_end - r if toward_top else v_end + r
    rect   = label_box(63.5, 6.0, 67.7, center, YL - 1.0, YW + 1.0) if toward_top else \
             label_box(63.5, center, 67.7, 26.0, YL - 1.0, YW + 1.0)
    disc   = back_place(cq.Workplane("XZ").center(RL_X1 - 65.6, RL_Z1 - center).circle(r).extrude(YW - YL + 2.0), YL - 1.0)

    return rect.union(disc)


m.add("rear_warning",
      label_box(2.3, 7.35, 63.5, 24.3, YL, YW).edges("|Y").fillet(1.5)
        .union(label_box(62.0, 7.35, 67.9, 24.3, YL, YW))
        .cut(neck_slot(12.3, True))
        .cut(neck_slot(19.9, False))
        .union(back_place(cq.Workplane("XZ")
                            .polyline([(RL_X1 - 67.7, RL_Z1 - 8.26),
                                       (RL_X1 - 74.3, RL_Z1 - 16.0),
                                       (RL_X1 - 67.7, RL_Z1 - 23.9)])
                            .close().extrude(YW - YL), YL)),
      WHITE)
m.add("rear_serial", label_box(2.37, 36.1, 74.0, 42.7, YL, YW).edges("|Y").fillet(1.5), WHITE)

stripes = None
u       = STRIPE_U0

while u + STRIPE_W < RL_X1 - RL_X0 - 0.5:
    stripe  = label_box(u, 0.0, u + STRIPE_W, RL_Z1 - RL_Z0, YL, YW)
    stripes = stripe if stripes is None else stripes.union(stripe)
    u      += STRIPE_PITCH

m.add("rear_stripes", stripes, WHITE)

m.add("rear_bracket",
      label_box(2.07, 3.6, 9.27, 3.95, YL, YW)
        .union(label_box(2.07, 1.96, 2.42, 3.95, YL, YW))
        .union(label_box(31.2, 3.6, 38.6, 3.95, YL, YW))
        .union(label_box(38.25, 1.96, 38.6, 3.95, YL, YW)),
      WHITE)

for name, text, font, u0, u1, v, ink in (
        ("floppy",   "FLOPPY DISKS",                         FONT_GOTHIC,      10.2, 30.2,  4.1, WHITE),
        ("warning",  "WARNING",                              FONT_GOTHIC,      23.8, 42.1, 10.9, LABEL_RD),
        ("connect",  "CONNECT THIS CABLE",                   FONT_GOTHIC,      16.5, 49.7, 14.5, LABEL_RD),
        ("only",     "ONLY TO APPLE ///",                    FONT_GOTHIC,      19.4, 46.6, 17.95, LABEL_RD),
        ("ports",    "“FLOPPY DISKS” CONNECTORS",  FONT_GOTHIC,      10.0, 55.7, 21.3, LABEL_RD),
        ("company",  "apple computer inc.",                  FONT_GOTHIC_BOLD, 15.9, 54.8, 28.7, WHITE),
        ("registered", "®",                             FONT_GOTHIC,      16.4, 17.2, 31.2, WHITE),
        ("street",   "10260 BANDLEY DRIVE",                  FONT_GOTHIC,      24.7, 40.7, 31.7, WHITE),
        ("city",     "CUPERTINO, CALIFORNIA 95014",          FONT_GOTHIC,      21.1, 44.5, 33.7, WHITE),
        ("serial",   "SERIAL NO. A3M0004-",                  FONT_GOTHIC,      14.7, 47.6, 38.3, LABEL_RD),
        ("number",   "07727",                                FONT_COURIER,     50.3, 62.3, 38.3, LABEL_RD),
        ("made",     "MADE IN U.S.A",                        FONT_GOTHIC,      32.2, 43.2, 41.0, LABEL_RD)):
    m.add(f"rear_text_{name}", label_text(text, font, u0, u1, v, YW if ink == LABEL_RD else YL), ink)


def back_silhouette(u0, v0, height, rgb, name):
    """Casso's silhouette in one color, on the label, read from behind."""
    rows, _ = read_branding()
    cell    = height / len(rows)
    solids  = []

    for row, bits in enumerate(rows):
        col = 0

        while col < 64:
            if not (bits >> col) & 1:
                col += 1
                continue

            run = col

            while run < 64 and (bits >> run) & 1:
                run += 1

            solids.append(label_box(u0 + col * cell, v0 + row * cell, u0 + run * cell, v0 + row * cell + cell + 0.02,
                                    YL, YW).val())
            col = run

    m.add(name, cq.Compound.makeCompound(solids), rgb)


back_silhouette(11.2, 25.6, 5.7, WHITE, "rear_mark")

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
