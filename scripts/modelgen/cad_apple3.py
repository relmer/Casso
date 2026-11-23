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
FIN_BOT_Z          = 30.5
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
RUBBER   = (0.120, 0.110, 0.100)


def box(x0, y0, z0, x1, y1, z1):
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


def d_shell(u, w, h, name):
    """A D connector: the metal shell, and the black insert inside it."""
    def trapezoid(width, height, y0, y1):
        top, bot = width * 0.5, width * 0.5 - height * 0.18
        x, z     = rear_x(u), CONN_Z
        wp       = (cq.Workplane("XZ").polyline([(x - top, z + height * 0.5), (x + top, z + height * 0.5),
                                                 (x + bot, z - height * 0.5), (x - bot, z - height * 0.5)])
                      .close().extrude(y1 - y0))
        bb       = wp.val().BoundingBox()

        return wp.translate((0.0, y0 - bb.ymin, 0.0))

    m.add(name, trapezoid(w, h, YS, YS + 1.0), METAL)
    m.add(f"{name}_insert", trapezoid(w - 2.4, h - 2.4, YS, YS + 4.0), BLACK)


def header(u, name):
    """The floppy disk connector: a 26-position shrouded header, as on the Disk ///."""
    x      = rear_x(u)
    shell  = (box(x - 20.17, YS, CONN_Z - 4.3, x + 20.17, YS + 4.0, CONN_Z + 4.3)
                .cut(box(x - 16.5, YS + 1.4, CONN_Z - 3.175, x + 16.5, YS + 5.0, CONN_Z + 3.175))
                .cut(box(x - 2.25, YS + 1.4, CONN_Z, x + 2.25, YS + 5.0, CONN_Z + 5.0)))
    pins   = None

    for row in (-1.27, 1.27):
        for i in range(13):
            px  = x + (i - 6) * 2.54
            pin = box(px - 0.32, YS + 1.3, CONN_Z + row - 0.32, px + 0.32, YS + 3.2, CONN_Z + row + 0.32)
            pins = pin if pins is None else pins.union(pin)

    m.add(name, shell, BLUE)
    m.add(f"{name}_pins", pins, GOLD)


header(66.2, "floppy_disks")
d_shell(107.6, 20.7, 11.0, "port_a")
d_shell(140.8, 20.7, 11.0, "port_b")
d_shell(177.2, 27.6, 11.0, "color_video")
d_shell(251.2, 42.8, 11.0, "port_c")

m.add("bw_video", cq.Workplane("XZ").center(rear_x(207.0), CONN_Z).circle(4.5).extrude(-4.0).translate((0.0, YS, 0.0)), (0.750, 0.150, 0.150))
m.add("bw_video_pin", cq.Workplane("XZ").center(rear_x(207.0), CONN_Z).circle(1.6).extrude(-4.3).translate((0.0, YS, 0.0)), METAL)
m.add("audio", cq.Workplane("XZ").center(rear_x(220.8), CONN_Z).circle(3.0).extrude(-3.0).translate((0.0, YS, 0.0)), BLACK)

m.add("power_inlet", rear_box(327.9, CONN_Z - 3.8, 355.5, CONN_Z + 13.0, YS, YS + 4.0)
                       .cut(rear_box(331.0, CONN_Z - 1.0, 352.4, CONN_Z + 10.0, YS + 2.0, YS + 5.0)), BLACK)
m.add("power_switch", rear_box(367.3, CONN_Z, 397.3, CONN_Z + 12.4, YS, YS + 1.5), BLACK)

for text, size, u, z in (("FLOPPY DISKS", 1.8, 66.2, 26.0), ("PORT A", 1.8, 107.6, 26.0), ("PORT B", 1.8, 140.8, 26.0),
                         ("OUTPUT", 1.8, 200.0, 27.0), ("COLOR VIDEO", 1.6, 177.2, 23.8), ("B/W VIDEO", 1.6, 207.0, 23.8),
                         ("AUDIO", 1.6, 222.0, 23.8), ("PORT C", 1.8, 251.2, 26.0), ("RS-232-C", 1.6, 251.2, 23.8),
                         ("POWER", 1.6, 382.3, 27.0), ("OFF", 1.6, 376.0, CONN_Z + 6.2), ("ON", 1.6, 389.0, CONN_Z + 6.2)):
    m.add(f"label_{text.replace(' ', '_').replace('/', '_')}", rear_text(text, size, u, z, YS if text not in ("OFF", "ON") else YS + 1.5),
          INK if text not in ("OFF", "ON") else WHITE)


if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Apple3.mesh"), os.path.join(out, "Apple3.mtl"), "Apple3.mtl")
    print(f"Apple3 (CAD): {nv} verts, {nt} tris")
