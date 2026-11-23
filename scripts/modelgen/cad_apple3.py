"""The Apple /// body, as a solid model, from the front of the upper body to
the back; the sloped keyboard section in front of it is not modeled. 444.5 mm
wide (Apple's figure), about 282 mm deep and 142 mm high. X right, Y back, Z
up; the bottom of the front face is at y = 0.

MODELED FROM PHOTOGRAPHS. Resources/Models/Apple3/README.md has the list of
them and the measurements taken from each.

The body is a cream molded cover on a brown cast base. The cover's sides are
flat, its top is flat with a shallow inset panel, and its front face leans
back about 10 degrees. Below the face, the cover's lower edge slopes down to
the base. The face has the logo plates at the lower left and the built-in
drive at the right: the slot in a beveled channel, the latch in its notch
and the busy lamp in the finger recess below it. The rear is a finned brown
casting that wraps round the back corners, with four slot openings, and
under the fins the light connector plate and the power plate between the
casting's legs.

The part name is the identity: `lever` is the latch the scene moves, `lamp`
the busy lamp, `slot` the opening.
"""

import math
import os

import cadquery as cq

from cadkit import KD, Model
from cad_badge import badge

# ---------------------------------------------------------------- dimensions
#
# The profile is from a3-side-right-rr3483512.jpg at 0.265 mm per pixel,
# scaled by the whole machine's documented depth of 462.3 mm. Heights are
# from the desk; depths from the bottom of the front face.
W          = 444.5
FOOT_H     = 3.0
TOP        = 141.8

FACE_BOT_Z = 44.8         # the face's bottom edge, at y = 0
FACE_TOP_Y = 17.2         # how far back the face's top edge is
KINK_Y     = 79.5         # where the cover's lower edge stops sloping
KINK_Z     = 27.8
COVER_END  = 238.5        # the cover's back, where the fins start
COVER_BOT  = 28.4         # the cover's lower edge at its back
D          = 282.2        # the fins' back face

FACE_TILT  = math.atan2(FACE_TOP_Y, TOP - FACE_BOT_Z)
FACE_LEN   = math.hypot(FACE_TOP_Y, TOP - FACE_BOT_Z)

PANEL_X0, PANEL_X1 = 30.0, W - 30.0
PANEL_Y0, PANEL_Y1 = 60.0, 225.0
PANEL_DEEP         = 0.8

# The face's features, from disk3-beside-a3-front-rr3483512.jpg at 0.32 mm
# per pixel. `x` is from the left as seen from the front; `v` is up the face
# from its bottom edge.
SLOT_W             = 137.8                  # the Disk /// is scaled from this slot
SLOT_X0            = 313.0 - SLOT_W * 0.5
SLOT_V0, SLOT_V1   = 47.4, 51.9
CHANNEL_X0, CHANNEL_X1 = SLOT_X0 - 7.5, SLOT_X0 + SLOT_W + 7.5
CHANNEL_V0, CHANNEL_V1 = 45.0, 54.5
CHANNEL_D          = 1.2
NOTCH_X0, NOTCH_X1 = 292.0, 334.0
NOTCH_V0, NOTCH_V1 = 16.7, 85.1
NOTCH_D            = 14.0
LATCH_X0, LATCH_X1 = 293.5, 332.5
LATCH_V1           = 83.5
TAB_V0, TAB_V1     = 40.1, 51.8
TAB_PROUD          = 3.0
LAMP_X, LAMP_V     = 313.0, 26.0
LAMP_W, LAMP_H     = 5.0, 3.6
PLATE1_X0, PLATE1_X1 = 34.5, 86.0
PLATE2_X0, PLATE2_X1 = 90.6, 112.0
PLATE_V0, PLATE_V1   = 13.4, 36.7

# The rear, from a3plus-back.jpg at 0.276 mm per pixel. `u` is from the left
# as seen from behind, `z` from the desk.
FIN_TOP_Z          = TOP
FIN_BOT_Z          = 32.0
FIN_PITCH          = 12.7
FIN_GROOVE_W       = 6.2
FIN_GROOVE_D       = 8.0
SIDE_FIN_PITCH     = 11.0
SIDE_FIN_GROOVE_W  = 5.0
SIDE_FIN_GROOVE_D  = 3.0
REAR_R             = 5.5
OPENING_U0, OPENING_U1 = 172.5, 269.0
OPENING_Z0, OPENING_Z1 = 42.8, 125.6
OPENING_RIBS       = (197.3, 223.0, 248.4)
OPENING_RIB_W      = 5.0
TABS_U             = (47.5, 150.7, 291.2, 394.7)
STRIP_Z1           = FIN_BOT_Z
STRIP_DEEP         = 12.0
LEGS               = ((16.6, 34.5), (285.7, 316.0), (407.0, 433.0))
PLATE_U            = (34.5, 285.7)
POWER_U            = (316.0, 407.0)
CONN_Z             = 11.9

FONT_GOTHIC = r"C:\Windows\Fonts\GOTHIC.TTF"

CASE     = (0.905, 0.840, 0.700)    # the Disk ///'s cream
LATCH    = (0.905, 0.860, 0.755)
BROWN    = (0.190, 0.145, 0.110)    # the base, the fins and the logo plates
SLOT_DK  = (0.055, 0.055, 0.062)
BLACK    = (0.070, 0.070, 0.075)
WHITE    = (0.920, 0.920, 0.900)
PLATE_LT = (0.800, 0.780, 0.720)    # the connector and power plates
METAL    = (0.620, 0.625, 0.635)
BLUE     = (0.200, 0.420, 0.760)
GOLD     = (0.780, 0.640, 0.300)
INK      = (0.200, 0.200, 0.210)
INSERT   = (0.230, 0.225, 0.220)    # a D connector's insert
RUBBER   = (0.120, 0.110, 0.100)


def box(x0, y0, z0, x1, y1, z1):
    x0, x1 = min(x0, x1), max(x0, x1)
    y0, y1 = min(y0, y1), max(y0, y1)
    z0, z1 = min(z0, z1), max(z0, z1)
    return (cq.Workplane("XY")
              .box(x1 - x0, y1 - y0, z1 - z0, centered=(False, False, False))
              .translate((x0, y0, z0)))


def profile(points, x0=0.0, x1=W):
    """A side profile, (y, z) points, run across from x0 to x1."""
    return (cq.Workplane("YZ").polyline(points).close().extrude(x1 - x0)
              .translate((x0, 0.0, 0.0)))


def on_face(wp):
    """Moves a part built against a flat face at y = 0, with z up that face
    from its bottom edge, onto the sloped face."""
    return (wp.rotate((0, 0, 0), (1, 0, 0), -math.degrees(FACE_TILT))
              .translate((0.0, 0.0, FACE_BOT_Z)))


def face_box(x0, v0, x1, v1, n0, n1):
    """A box on the face: n is into the body, so a negative n is in front of the face."""
    return on_face(box(x0, n0, v0, x1, n1, v1))


def face_pocket(x0, x1, v0, v1, depth):
    """A pocket `depth` deep into the face, its walls at 45 degrees."""
    outer = cq.Wire.makePolygon([cq.Vector(x0 - depth, -0.01, v0 - depth), cq.Vector(x1 + depth, -0.01, v0 - depth),
                                 cq.Vector(x1 + depth, -0.01, v1 + depth), cq.Vector(x0 - depth, -0.01, v1 + depth)], close=True)
    inner = cq.Wire.makePolygon([cq.Vector(x0, depth, v0), cq.Vector(x1, depth, v0),
                                 cq.Vector(x1, depth, v1), cq.Vector(x0, depth, v1)], close=True)

    return on_face(cq.Workplane("XY").add(cq.Solid.makeLoft([outer, inner], True)))


def rear_x(u):
    return W - u


def rear_box(u0, z0, u1, z1, y0, y1):
    return box(rear_x(u1), y0, z0, rear_x(u0), y1, z1)


def rear_text(text, size, u, z, y0):
    wp = cq.Workplane("XZ").text(text, size, 0.12, fontPath=FONT_GOTHIC, halign="center", valign="center").mirror("YZ")
    bb = wp.val().BoundingBox()

    return wp.translate((rear_x(u) - (bb.xmin + bb.xmax) * 0.5, y0 - bb.ymin, z - (bb.zmin + bb.zmax) * 0.5))


m = Model()

# ---------------------------------------------------------------------- base

base = profile([(0.0, FOOT_H), (COVER_END, FOOT_H), (COVER_END, COVER_BOT - 0.05), (KINK_Y, KINK_Z - 0.05), (0.0, FACE_BOT_Z - 0.05)])
m.add("base", base, BROWN)

for fx in (30.0, W - 60.0):
    for fy in (20.0, D - 50.0):
        m.add(f"foot_{fx:.0f}_{fy:.0f}", box(fx, fy, 0.0, fx + 30.0, fy + 30.0, FOOT_H + 0.5), RUBBER)

# --------------------------------------------------------------------- cover

cover = profile([(0.0, FACE_BOT_Z), (KINK_Y, KINK_Z), (COVER_END, COVER_BOT),
                 (COVER_END, TOP), (FACE_TOP_Y, TOP)])
cover = cover.cut(box(PANEL_X0, PANEL_Y0, TOP - PANEL_DEEP, PANEL_X1, PANEL_Y1, TOP + 1.0).edges("|Z").fillet(4.0))
cover = cover.cut(face_pocket(CHANNEL_X0, CHANNEL_X1, CHANNEL_V0, CHANNEL_V1, CHANNEL_D))
cover = cover.cut(face_pocket(NOTCH_X0, NOTCH_X1, NOTCH_V0, NOTCH_V1, CHANNEL_D))
cover = cover.cut(face_box(SLOT_X0, SLOT_V0, SLOT_X0 + SLOT_W, SLOT_V1, -5.0, 20.0))
cover = cover.cut(face_box(NOTCH_X0, NOTCH_V0, NOTCH_X1, NOTCH_V1, -5.0, NOTCH_D))
m.add("case", cover, CASE, angular=0.14)

# ------------------------------------------------------------- front: drive

m.add("slot", face_box(SLOT_X0, SLOT_V0, SLOT_X0 + SLOT_W, SLOT_V1, 1.0, 19.0), SLOT_DK)
m.add("recess",
      face_box(NOTCH_X0, NOTCH_V0, NOTCH_X1, TAB_V0, NOTCH_D - 1.0, NOTCH_D)
        .union(face_box(NOTCH_X0, NOTCH_V0 - 1.0, NOTCH_X1, NOTCH_V0 + 0.6, CHANNEL_D + 0.3, NOTCH_D))
        .union(face_box(NOTCH_X0, NOTCH_V0, NOTCH_X0 + 0.6, TAB_V0, CHANNEL_D + 0.3, NOTCH_D))
        .union(face_box(NOTCH_X1 - 0.6, NOTCH_V0, NOTCH_X1, TAB_V0, CHANNEL_D + 0.3, NOTCH_D)),
      BROWN)
m.add("lever",
      face_box(LATCH_X0, TAB_V1 - 1.0, LATCH_X1, LATCH_V1, 0.0, 4.0)
        .union(face_box(LATCH_X0, TAB_V0, LATCH_X1, TAB_V1, -TAB_PROUD, 4.0)),
      LATCH)
m.add("lamp_frame", face_box(LAMP_X - LAMP_W * 0.5 - 1.2, LAMP_V - LAMP_H * 0.5 - 1.2,
                             LAMP_X + LAMP_W * 0.5 + 1.2, LAMP_V + LAMP_H * 0.5 + 1.2, NOTCH_D - 1.6, NOTCH_D - 1.0), BLACK)
m.add("lamp", face_box(LAMP_X - LAMP_W * 0.5, LAMP_V - LAMP_H * 0.5,
                       LAMP_X + LAMP_W * 0.5, LAMP_V + LAMP_H * 0.5, NOTCH_D - 1.8, NOTCH_D - 1.0), KD["drive_lamp"])

# ----------------------------------------------------- front: logo plates
#
# Two dark plates: the first has Casso's silhouette in place of the logo,
# and the second the machine's "///".
m.add("logo_plate", face_box(PLATE1_X0, PLATE_V0, PLATE1_X1, PLATE_V1, -0.4, 0.5), BROWN)
m.add("model_plate", face_box(PLATE2_X0, PLATE_V0, PLATE2_X1, PLATE_V1, -0.4, 0.5), BROWN)

badge_parts = Model()
badge(badge_parts, PLATE1_X0 + 3.0, PLATE_V0 + 2.5, PLATE_V1 - PLATE_V0 - 5.0, y=-0.7)

for part in badge_parts.parts:
    m.add(part.name, on_face(part.solid), part.color)

slashes = (cq.Workplane("XZ").text("///", 14.0, 0.12, fontPath=r"C:\Windows\Fonts\arialbi.ttf",
                                   halign="center", valign="center"))
bb      = slashes.val().BoundingBox()
slashes = slashes.translate(((PLATE2_X0 + PLATE2_X1) * 0.5 - (bb.xmin + bb.xmax) * 0.5, -0.4 - bb.ymax,
                             (PLATE_V0 + PLATE_V1) * 0.5 - (bb.zmin + bb.zmax) * 0.5))
m.add("model_mark", on_face(slashes), WHITE)

# ---------------------------------------------------------------- rear fins

fins = box(0.0, COVER_END, FOOT_H, W, D, FIN_TOP_Z)
fins = fins.edges("|Z and >Y").fillet(REAR_R)

u = FIN_PITCH * 0.5

while u + FIN_GROOVE_W < W:
    fins = fins.cut(box(u, D - FIN_GROOVE_D, FIN_BOT_Z, u + FIN_GROOVE_W, D + 1.0, FIN_TOP_Z + 1.0))
    u   += FIN_PITCH

y = COVER_END + SIDE_FIN_PITCH * 0.5

while y + SIDE_FIN_GROOVE_W < D - REAR_R:
    fins = fins.cut(box(-1.0, y, FIN_BOT_Z, SIDE_FIN_GROOVE_D, y + SIDE_FIN_GROOVE_W, FIN_TOP_Z + 1.0))
    fins = fins.cut(box(W - SIDE_FIN_GROOVE_D, y, FIN_BOT_Z, W + 1.0, y + SIDE_FIN_GROOVE_W, FIN_TOP_Z + 1.0))
    y   += SIDE_FIN_PITCH

# The slot openings, and the strip under the fins between the legs.
fins = fins.cut(rear_box(OPENING_U0, OPENING_Z0, OPENING_U1, OPENING_Z1, D - 30.0, D + 1.0))
fins = fins.cut(rear_box(LEGS[0][1], FOOT_H - 1.0, LEGS[1][0], STRIP_Z1, D - STRIP_DEEP, D + 1.0))
fins = fins.cut(rear_box(LEGS[1][1], FOOT_H - 1.0, LEGS[2][0], STRIP_Z1, D - STRIP_DEEP, D + 1.0))
fins = fins.cut(rear_box(-1.0, FOOT_H - 1.0, LEGS[0][0], STRIP_Z1, D - STRIP_DEEP, D + 1.0))
fins = fins.cut(rear_box(LEGS[2][1], FOOT_H - 1.0, W + 1.0, STRIP_Z1, D - STRIP_DEEP, D + 1.0))
m.add("fins", fins, BROWN)

for i, ru in enumerate(OPENING_RIBS):
    m.add(f"opening_rib_{i}", rear_box(ru - OPENING_RIB_W * 0.5, OPENING_Z0, ru + OPENING_RIB_W * 0.5, OPENING_Z1,
                                       D - 30.0, D - FIN_GROOVE_D * 0.5), BROWN)

m.add("opening_back", rear_box(OPENING_U0, OPENING_Z0, OPENING_U1, OPENING_Z1, D - 30.5, D - 29.5), BLACK)

for i, tu in enumerate(TABS_U):
    m.add(f"fin_tab_{i}", rear_box(tu - 2.5, 132.5, tu + 2.5, 138.0, D - FIN_GROOVE_D - 0.5, D - FIN_GROOVE_D + 2.0), PLATE_LT)

# ---------------------------------------------------- rear: connector strip

YS = D - STRIP_DEEP + 0.5      # the plates' face

m.add("connector_plate", rear_box(PLATE_U[0], FOOT_H + 1.0, PLATE_U[1], STRIP_Z1 - 0.5, D - STRIP_DEEP, YS), PLATE_LT)
m.add("power_plate",     rear_box(POWER_U[0], FOOT_H + 1.0, POWER_U[1], STRIP_Z1 - 0.5, D - STRIP_DEEP, YS), PLATE_LT)


def fit_text(text, width, u, z, y0, font=FONT_GOTHIC, thick=0.12):
    """Lettering on the back, sized so the line is `width` wide and centered at (u, z)."""
    probe = cq.Workplane("XZ").text(text, 10.0, thick, fontPath=font, halign="center", valign="center")
    pb    = probe.val().BoundingBox()
    wp    = cq.Workplane("XZ").text(text, 10.0 * width / (pb.xmax - pb.xmin), thick, fontPath=font,
                                    halign="center", valign="center").mirror("YZ")
    bb    = wp.val().BoundingBox()

    return wp.translate((rear_x(u) - (bb.xmin + bb.xmax) * 0.5, y0 - bb.ymin, z - (bb.zmin + bb.zmax) * 0.5))


def rear_disc(u, z, r, y0, y1):
    y0, y1 = min(y0, y1), max(y0, y1)
    return cq.Workplane("XZ").center(rear_x(u), z).circle(r).extrude(-(y1 - y0)).translate((0.0, y0, 0.0))


def d_outline(u, z, top, height, y0, y1, r=0.0):
    """A D outline facing the back: `top` wide along its top edge, the sides
    leaning in 10 degrees toward the bottom."""
    y0, y1 = min(y0, y1), max(y0, y1)
    bot = top - 2.0 * height * math.tan(math.radians(10.0))
    x   = rear_x(u)
    wp  = (cq.Workplane("XZ").polyline([(x - top * 0.5, z + height * 0.5), (x + top * 0.5, z + height * 0.5),
                                        (x + bot * 0.5, z - height * 0.5), (x - bot * 0.5, z - height * 0.5)])
             .close().extrude(y1 - y0))
    if r > 0.0:
        wp = wp.edges("|Y").fillet(r)
    bb  = wp.val().BoundingBox()

    return wp.translate((0.0, y0 - bb.ymin, 0.0))


# D-subminiature female sockets, to the standard shell sizes: contacts on a
# 2.77 mm pitch in two rows 2.84 mm apart, the longer row on top. Insert
# widths along the top edge: shell E (9) 15.4 mm, A (15) 23.7 mm, B (25)
# 37.4 mm; all 6.6 mm high.
D_PITCH, D_ROW, D_HOLE_R, D_INSERT_H = 2.77, 2.84, 0.55, 6.6


def d_socket(name, u, z, positions, insert_w, outer_w, outer_h, metal):
    top_n  = (positions + 1) // 2
    holes  = None

    m.add(f"{name}_sockets", d_outline(u, z, insert_w - 0.4, D_INSERT_H - 0.4, YS + 0.4, YS), SLOT_DK)

    for row, n in ((D_ROW * 0.5, top_n), (-D_ROW * 0.5, positions - top_n)):
        for i in range(n):
            hu   = u + (i - (n - 1) * 0.5) * D_PITCH
            hole = rear_disc(hu, z + row, D_HOLE_R, YS + 5.0, YS - 1.0)
            holes = hole if holes is None else holes.union(hole)

    if metal:
        # A metal shell standing proud of a black insert.
        shell = (d_outline(u, z, outer_w, outer_h, YS + 1.0, YS, 1.6)
                   .union(d_outline(u, z, insert_w + 1.6, D_INSERT_H + 1.6, YS + 4.0, YS, 1.0))
                   .cut(d_outline(u, z, insert_w + 0.6, D_INSERT_H + 0.6, YS + 4.5, YS - 0.5, 0.8)))
        m.add(name, shell, METAL)
        m.add(f"{name}_insert", d_outline(u, z, insert_w, D_INSERT_H, YS + 3.2, YS, 0.6).cut(holes), INSERT)
    else:
        # A molded black housing with the insert set back in it.
        housing = (d_outline(u, z, outer_w, outer_h, YS + 3.0, YS, 2.2)
                     .cut(d_outline(u, z, insert_w + 1.2, D_INSERT_H + 1.2, YS + 3.5, YS + 1.0, 1.0))
                     .cut(d_outline(u, z, insert_w, D_INSERT_H, YS + 3.5, YS - 0.5)))
        m.add(name, housing, BLACK)
        m.add(f"{name}_insert", d_outline(u, z, insert_w, D_INSERT_H, YS + 2.2, YS, 0.6).cut(holes), INSERT)


def header(u, z, name):
    """The floppy disk connector: a 26-position shrouded header (3M 2500
    series), as on the Disk ///."""
    x      = rear_x(u)
    shell  = (box(x - 20.17, YS + 3.0, z - 4.3, x + 20.17, YS, z + 4.3)
                .cut(box(x - 16.5, YS + 4.0, z - 3.175, x + 16.5, YS + 0.4, z + 3.175))
                .cut(box(x - 2.25, YS + 4.0, z + 3.0, x + 2.25, YS + 0.4, z + 5.0)))
    pins   = None

    for row in (-1.27, 1.27):
        for i in range(13):
            px  = x + (i - 6) * 2.54
            pin = box(px - 0.32, YS + 2.2, z + row - 0.32, px + 0.32, YS + 0.4, z + row + 0.32)
            pins = pin if pins is None else pins.union(pin)

    m.add(name, shell, BLUE)
    m.add(f"{name}_pins", pins, GOLD)


def bracket(name, u0, u1, z_top, z_leg, gap):
    """A printed bracket over a connector: a line along the top, broken where
    its lettering sits, with a leg down at each end."""
    t    = 0.35
    y0   = YS + 0.12
    line = (rear_box(u0, z_top - t * 0.5, gap[0], z_top + t * 0.5, y0, YS)
              .union(rear_box(gap[1], z_top - t * 0.5, u1, z_top + t * 0.5, y0, YS))
              .union(rear_box(u0, z_leg, u0 + t, z_top + t * 0.5, y0, YS))
              .union(rear_box(u1 - t, z_leg, u1, z_top + t * 0.5, y0, YS)))
    m.add(name, line, INK)


def label(name, text, width, u, z, color=INK, y0=None, font=FONT_GOTHIC):
    m.add(f"label_{name}", fit_text(text, width, u, z, YS if y0 is None else y0, font), color)


def phillips(name, u, z):
    """A pan-head Phillips screw."""
    head  = rear_disc(u, z, 2.7, YS, YS + 1.3)
    cross = (rear_box(u - 1.6, z - 0.3, u + 1.6, z + 0.3, YS + 1.5, YS + 0.7)
               .union(rear_box(u - 0.3, z - 1.6, u + 0.3, z + 1.6, YS + 1.5, YS + 0.7)))
    m.add(name, head.cut(cross), METAL)


# Positions from a3plus-back.jpg; the connector standards fix the shapes.
# The plates face +Y, so a part proud of them reaches past YS.

header(66.8, 12.8, "floppy_disks")
d_socket("port_a",      107.9, 14.6,  9, 15.4, 20.5, 11.5, False)
d_socket("port_b",      141.0, 14.6,  9, 15.4, 20.5, 11.5, False)
d_socket("color_video", 178.0, 14.6, 15, 23.7, 28.9, 11.5, True)
d_socket("port_c",      251.6, 14.6, 25, 37.4, 43.2, 11.3, False)

# B/W video: an RCA socket. Audio: a 3.5 mm phone jack.
m.add("bw_video",        rear_disc(207.0, 16.2, 4.7, YS + 3.0, YS).cut(rear_disc(207.0, 16.2, 3.3, YS + 3.5, YS + 2.0)), METAL)
m.add("bw_video_insul",  rear_disc(207.0, 16.2, 3.3, YS + 2.2, YS).cut(rear_disc(207.0, 16.2, 1.35, YS + 2.5, YS - 1.0)), (0.700, 0.330, 0.360))
m.add("bw_video_socket", rear_disc(207.0, 16.2, 1.35, YS + 0.4, YS), SLOT_DK)
m.add("audio",           rear_disc(220.3, 15.1, 3.85, YS + 1.6, YS).cut(rear_disc(220.3, 15.1, 2.8, YS + 2.0, YS + 1.0)), METAL)
m.add("audio_insul",     rear_disc(220.3, 15.1, 2.8, YS + 1.1, YS).cut(rear_disc(220.3, 15.1, 1.75, YS + 1.5, YS - 1.0)), BLACK)
m.add("audio_socket",    rear_disc(220.3, 15.1, 1.75, YS + 0.3, YS), SLOT_DK)

bracket("bracket_floppy", 44.2, 90.0, 27.2, 21.0, (56.5, 77.7))
bracket("bracket_port_a", 96.7, 119.9, 27.2, 21.0, (102.6, 113.2))
bracket("bracket_port_b", 129.9, 153.0, 27.2, 21.0, (135.7, 146.3))
bracket("bracket_output", 161.0, 227.0, 27.3, 21.3, (188.2, 199.6))
bracket("bracket_port_c", 229.8, 273.7, 27.3, 21.3, (246.0, 257.4))
bracket("bracket_power",  369.2, 402.8, 29.3, 26.6, (380.8, 391.6))

label("floppy_disks", "FLOPPY DISKS", 17.2, 67.1, 27.2)
label("port_a",       "PORT A",        9.3, 107.9, 27.2)
label("port_b",       "PORT B",        9.3, 141.0, 27.2)
label("output",       "OUTPUT",        9.7, 193.9, 27.3)
label("color_video",  "COLOR VIDEO",  18.5, 178.0, 24.1)
label("bw_video",     "B/W VIDEO",    14.0, 207.3, 24.1)
label("audio",        "AUDIO",         8.5, 220.4, 24.1)
label("port_c",       "PORT C",        9.7, 251.7, 27.3)
label("rs232",        "RS-232-C",     12.6, 251.7, 24.1)
label("power",        "POWER",         9.2, 386.2, 29.3)
label("power_off",    "O",             1.9, 377.4, 27.4)
m.add("label_power_on", rear_box(393.95, 26.45, 394.25, 28.35, YS, YS + 0.12), INK)
label("rating",       "220 VAC, 50-60 Hz, 0.5 AMP", 36.8, 341.1, 6.1)
label("maker",        "Casso Computer", 27.0, 385.6, 7.1)

# Casso's silhouette in place of the logo, ahead of the maker's line. The
# badge is built facing the front, so it is turned round to face the back.
mark = Model()
badge(mark, 0.0, 0.0, 4.5, y=0.0, prefix="maker_mark")
for part in mark.parts:
    s = part.solid.rotate((0, 0, 0), (0, 0, 1), 180.0)
    m.add(part.name, s, INK)
mark_bb = None
for part in m.parts:
    if part.name.startswith("maker_mark"):
        bb      = part.solid.BoundingBox()
        mark_bb = bb if mark_bb is None else mark_bb.add(bb)
for part in m.parts:
    if part.name.startswith("maker_mark"):
        part.solid = part.solid.translate((rear_x(370.3) - (mark_bb.xmin + mark_bb.xmax) * 0.5,
                                           YS - mark_bb.ymin,
                                           7.15 - (mark_bb.zmin + mark_bb.zmax) * 0.5))

# The power inlet: an IEC 60320 C14 panel inlet, its flange fixed by a screw
# at each side. The recess has its top corners chamfered; the line and
# neutral blades are 14 mm apart, the earth blade 4 mm above them.
INLET_U, INLET_Z = 341.6, 19.1
IX               = rear_x(INLET_U)
inlet_recess     = (cq.Workplane("XZ")
                      .polyline([(IX - 12.25, INLET_Z - 8.0), (IX + 12.25, INLET_Z - 8.0),
                                 (IX + 12.25, INLET_Z + 5.0), (IX + 9.25, INLET_Z + 8.0),
                                 (IX - 9.25, INLET_Z + 8.0), (IX - 12.25, INLET_Z + 5.0)])
                      .close().extrude(10.0))
rb               = inlet_recess.val().BoundingBox()
inlet_recess     = inlet_recess.translate((0.0, YS + 2.0 - rb.ymax, 0.0))
m.add("power_inlet",
      rear_box(INLET_U - 15.6, INLET_Z - 11.5, INLET_U + 15.6, INLET_Z + 11.5, YS + 1.5, YS - 0.2)
        .edges("|Y").fillet(2.5).cut(inlet_recess), BLACK)
for part in m.parts:
    if part.name in ("power_plate", "base", "fins"):
        part.solid = part.solid.cut(inlet_recess)
for part in m.parts:
    if part.name in ("power_plate", "base", "fins"):
        part.solid = part.solid.cut(inlet_recess)
m.add("power_inlet_floor", rear_box(INLET_U - 12.3, INLET_Z - 8.1, INLET_U + 12.3, INLET_Z + 8.1, YS - 8.0, YS - 7.5), BLACK)
for k, (pu, pz) in enumerate(((INLET_U - 7.0, INLET_Z - 2.2), (INLET_U + 7.0, INLET_Z - 2.2), (INLET_U, INLET_Z + 1.8))):
    m.add(f"power_inlet_pin_{k}", rear_box(pu - 1.0, pz - 2.0, pu + 1.0, pz + 2.0, YS - 7.5, YS - 0.8), METAL)
phillips("power_inlet_screw_l", 321.1, 19.1)
phillips("power_inlet_screw_r", 361.8, 19.1)

# The power switch: a rocker in a black bezel, with OFF and ON on its face
# and a white bar at the ON end.
SW_U, SW_Z = 386.4, 18.9
m.add("power_switch_bezel",
      rear_box(SW_U - 17.15, SW_Z - 7.0, SW_U + 17.15, SW_Z + 7.0, YS + 1.5, YS)
        .cut(rear_box(SW_U - 12.9, SW_Z - 4.3, SW_U + 12.9, SW_Z + 4.3, YS + 2.0, YS - 0.5)), BLACK)
m.add("power_switch", rear_box(SW_U - 12.85, SW_Z - 4.25, SW_U + 12.85, SW_Z + 4.25, YS + 1.0, YS), (0.030, 0.030, 0.035))
m.add("power_switch_bar", rear_box(397.6, SW_Z - 4.25, 398.0, SW_Z + 4.25, YS + 1.1, YS + 0.95), WHITE)
label("switch_off", "OFF", 7.3, 378.9, 18.5, WHITE, YS + 1.0)
label("switch_on",  "ON",  5.5, 392.4, 18.5, WHITE, YS + 1.0)

# The screw at the top left corner of the fins.
m.add("fin_screw", cq.Workplane("XZ").center(rear_x(21.5), 135.8).polygon(6, 4.6).extrude(-2.0)
                     .translate((0.0, D - 0.3, 0.0)), BLACK)


if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Apple3.mesh"), os.path.join(out, "Apple3.mtl"), "Apple3.mtl")
    print(f"Apple3 (CAD): {nv} verts, {nt} tris")
