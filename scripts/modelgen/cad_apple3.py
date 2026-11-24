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

COVER_ROLL_R = 14.0       # the face into the top
COVER_LIP_R  = 4.0        # the face's bottom edge
COVER_SIDE_R = 9.0        # the top, the face and the lower edge into the sides
FIN_TOP_R    = 8.0        # the fins' top edge, back and sides

# The keyboard section in front of the face, from the same side photograph:
# the base runs forward under it and its top rises from the front lip to the
# well behind the keyboard.
KB_FRONT_Y = -180.0       # the front of the base
KB_LIP_Z   = 12.0         # the base's top at its front
KB_BACK_Y  = -24.0        # the back of the keyboard housing
KB_BACK_Z  = 40.0         # the base's top under it
KB_THICK   = 20.0         # the housing, top to bottom
KB_SIDE    = 6.0          # its inset from the sides
KB_BEZEL   = 9.0          # the rim round the key well
KB_WELL_D  = 9.0          # the well's depth
KEY_PITCH  = 19.05
KEY_CAP    = 17.5
KEY_H      = 9.0

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
BEVEL_R            = 1.2                    # where a channel's bevel meets the face
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
BROWN    = (0.300, 0.250, 0.200)    # the base, the fins and the logo plates
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
KEYCAP   = (0.300, 0.290, 0.270)    # the dark gray keys
KEYCAP_T = (0.700, 0.620, 0.480)    # the tan keys


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
    """A pocket `depth` deep into the face, its walls at 45 degrees, the
    edge where a wall meets the face rolled over rather than sharp."""
    def ring(grow, y):
        return cq.Wire.makePolygon([cq.Vector(x0 - grow, y, v0 - grow), cq.Vector(x1 + grow, y, v0 - grow),
                                    cq.Vector(x1 + grow, y, v1 + grow), cq.Vector(x0 - grow, y, v1 + grow)], close=True)

    roll  = cq.Solid.makeLoft([ring(depth + BEVEL_R, -0.01), ring(depth + BEVEL_R * 0.3, BEVEL_R * 0.3), ring(depth, BEVEL_R)], False)
    wall  = cq.Solid.makeLoft([ring(depth, BEVEL_R - 0.01), ring(0.0, depth + 0.01)], True)

    return on_face(cq.Workplane("XY").add(roll).union(cq.Workplane("XY").add(wall)))


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

base = profile([(KB_FRONT_Y + 4.0, FOOT_H), (COVER_END, FOOT_H), (COVER_END, COVER_BOT - 0.05), (KINK_Y, KINK_Z - 0.05),
                (0.0, FACE_BOT_Z - 0.05), (KB_BACK_Y, KB_BACK_Z), (KB_FRONT_Y, KB_LIP_Z), (KB_FRONT_Y, FOOT_H + 4.0)])
base = base.edges("|X").edges(cq.selectors.BoxSelector((-1.0, KB_FRONT_Y - 1.0, -1.0), (W + 1.0, KB_FRONT_Y + 5.0, KB_LIP_Z + 1.0))).fillet(2.5)
m.add("base", base, BROWN)

for fx in (30.0, W - 60.0):
    for fy in (20.0, D - 50.0):
        m.add(f"foot_{fx:.0f}_{fy:.0f}", box(fx, fy, 0.0, fx + 30.0, fy + 30.0, FOOT_H + 0.5), RUBBER)

# ------------------------------------------------------------------ keyboard
#
# The keyboard, from the close-up in tcd/fig02.jpg: a cream housing lying on
# the base's slope, its top stepped down inside a rim, with the key well
# behind a wide front rest. The keys are on a 19.05 mm pitch: the main block
# of five rows, 14.25 keys wide, and three columns of numeric pad a key's
# width to its right. The keys are dark gray with white legends; ALPHA LOCK,
# the two command keys and the arrows are light gray, and two flat light
# blanks fill the bottom row beside the space bar. The command keys have
# Casso's silhouette, open and solid, in place of the logo.
KB_SLOPE = math.atan2(KB_BACK_Z - KB_LIP_Z, KB_BACK_Y - KB_FRONT_Y)
KB_RUN   = math.hypot(KB_BACK_Z - KB_LIP_Z, KB_BACK_Y - KB_FRONT_Y)
KB_LEN   = 146.0                            # the housing, front to back along the slope

KB_X0, KB_X1   = KB_SIDE, W - KB_SIDE
KB_EDGE_C      = 5.0                       # the outer edge's 45-degree chamfer
KB_STEP        = 14.0                      # the stepped-down panel, in from the edge
KB_STEP_D      = 1.2
MAIN_U         = 14.25
PAD_GAP_U      = 1.0
PAD_U          = 3.0
KEYS_W         = (MAIN_U + PAD_GAP_U + PAD_U) * KEY_PITCH
WELL_MARGIN    = 3.0
WELL_X0        = (W - KEYS_W) * 0.5 - WELL_MARGIN
WELL_X1        = W - WELL_X0
WELL_Y1        = KB_LEN - 11.0             # the well's back edge, along the slope
WELL_Y0        = WELL_Y1 - 5 * KEY_PITCH - 2 * WELL_MARGIN
KEYS_X0        = WELL_X0 + WELL_MARGIN
KEYS_Y1        = WELL_Y1 - WELL_MARGIN
KEY_Z0         = KB_THICK - KB_WELL_D
KEY_TOP_IN     = 2.3                       # how far each side of the top is in from the base
KEY_DISH       = 0.7

DARK_KEY   = (0.390, 0.330, 0.262)
LIGHT_KEY  = (0.700, 0.630, 0.500)
BLANK_KEY  = (0.720, 0.710, 0.680)
LEGEND     = (0.930, 0.925, 0.900)
FONT_KEY   = r"C:\Windows\Fonts\ARLRDBD.TTF"


def on_keyboard(wp):
    """Moves a part built flat, with y along the slope from the housing's
    front and z up from its bottom, onto the base's slope."""
    return (wp.rotate((0, 0, 0), (1, 0, 0), math.degrees(KB_SLOPE))
              .translate((0.0, KB_FRONT_Y, KB_LIP_Z))
              .translate((0.0, (KB_RUN - KB_LEN) * math.cos(KB_SLOPE), (KB_RUN - KB_LEN) * math.sin(KB_SLOPE))))


housing = box(KB_X0, 0.0, 0.0, KB_X1, KB_LEN, KB_THICK)
housing = housing.edges("|Z").fillet(6.0)
housing = housing.faces(">Z").edges().chamfer(KB_EDGE_C)
housing = housing.cut(box(KB_X0 + KB_STEP, KB_STEP, KB_THICK - KB_STEP_D, KB_X1 - KB_STEP, KB_LEN - KB_STEP * 0.6, KB_THICK + 1.0)
                        .edges("|Z").fillet(4.0))
# Two wells, the main block's and the pad's, with the housing between them.
MAIN_X1 = KEYS_X0 + MAIN_U * KEY_PITCH + WELL_MARGIN
PAD_X0W = KEYS_X0 + (MAIN_U + PAD_GAP_U) * KEY_PITCH - WELL_MARGIN
WELLS   = ((WELL_X0, MAIN_X1), (PAD_X0W, WELL_X1))
for wx0, wx1 in WELLS:
    housing = housing.cut(box(wx0, WELL_Y0, KEY_Z0, wx1, WELL_Y1, KB_THICK + 1.0).edges("|Z").fillet(2.0))

# The reset button: a gray plunger in a notch at the back right corner of
# the housing, below the rim so it is hard to press by accident.
RESET_X  = KB_X1 - 22.0
housing  = housing.cut(box(RESET_X - 7.0, KB_LEN - 6.0, KB_THICK - 9.0, RESET_X + 7.0, KB_LEN + 1.0, KB_THICK + 1.0))
m.add("keyboard", on_keyboard(housing), CASE, angular=0.14)
m.add("reset", on_keyboard(box(RESET_X - 5.0, KB_LEN - 5.0, KB_THICK - 9.0, RESET_X + 5.0, KB_LEN - 1.5, KB_THICK - 3.5)
                             .edges("|Y").fillet(1.0)), METAL)

# The well's floor, dark between the keys.
for i, (wx0, wx1) in enumerate(WELLS):
    m.add(f"key_well_{i}", on_keyboard(box(wx0, WELL_Y0, KEY_Z0 - 0.2, wx1, WELL_Y1, KEY_Z0 + 0.2)), (0.090, 0.088, 0.085))

key_parts   = {}
legend_part = []


def add_part(table, color, solid):
    table[color] = solid if color not in table else table[color].union(solid)


DECAL_LO, DECAL_HI = 0.12, 0.30           # the printed legends' layer, above the key top
KEY_ROUND          = 0.9                  # the keycaps' rounded edges
SPACE_BULGE        = 0.8                  # how far the space bar's top rises, front to back
LED_GLOW           = (1.000, 0.700, 0.660)
LENS_KEY           = (0.860, 0.850, 0.820)


def keycap_solid(x0, y0, w, d, h, convex=False):
    """A keycap: tapered sides, its top set back toward the row behind, and
    rounded edges. The top is dished side to side, the dish sized to the
    key's width so it is as deep on a wide key as on a letter. The space
    bar's top is instead rounded up from front to back. Returns the cap and
    a thin layer just above its top for the legends."""
    zt  = KEY_Z0 + h
    bot = cq.Wire.makePolygon([cq.Vector(x0, y0, KEY_Z0), cq.Vector(x0 + w, y0, KEY_Z0),
                               cq.Vector(x0 + w, y0 + d, KEY_Z0), cq.Vector(x0, y0 + d, KEY_Z0)], close=True)
    tx0, tx1 = x0 + KEY_TOP_IN, x0 + w - KEY_TOP_IN
    ty0, ty1 = y0 + KEY_TOP_IN + 1.2, y0 + d - KEY_TOP_IN + 0.4
    top = cq.Wire.makePolygon([cq.Vector(tx0, ty0, zt), cq.Vector(tx1, ty0, zt),
                               cq.Vector(tx1, ty1, zt), cq.Vector(tx0, ty1, zt)], close=True)
    cap = cq.Workplane("XY").add(cq.Solid.makeLoft([bot, top], True))
    cx  = (tx0 + tx1) * 0.5
    cy  = (ty0 + ty1) * 0.5

    if convex:
        prof = (cq.Workplane("YZ").moveTo(y0, KEY_Z0).lineTo(y0 + d, KEY_Z0).lineTo(ty1, zt - SPACE_BULGE)
                  .threePointArc((cy, zt), (ty0, zt - SPACE_BULGE)).close().extrude(w).translate((x0, 0.0, 0.0)))
        cap  = cap.intersect(prof)
        skin = None
        ztop = zt
    else:
        half = (tx1 - tx0) * 0.5
        r    = (half * half + KEY_DISH * KEY_DISH) / (2.0 * KEY_DISH)
        cz   = zt + r - KEY_DISH
        dish = (cq.Workplane("XZ").center(cx, cz).circle(r).extrude(-(d + 4.0)).translate((0.0, y0 - 2.0, 0.0)))
        cap  = cap.cut(dish)
        ztop = zt - KEY_DISH
        skin = box(x0, y0, ztop - 0.5, x0 + w, y0 + d, zt + 1.0)
        skin = (skin.intersect(cq.Workplane("XZ").center(cx, cz).circle(r - DECAL_LO).extrude(-(d + 4.0)).translate((0.0, y0 - 2.0, 0.0)))
                    .cut(cq.Workplane("XZ").center(cx, cz).circle(r - DECAL_HI).extrude(-(d + 4.0)).translate((0.0, y0 - 2.0, 0.0))))
        ztop = zt - KEY_DISH

    for radius in (KEY_ROUND, 0.7, 0.5):
        try:
            cap = cap.edges("not <Z").fillet(radius)
            break
        except Exception:
            pass

    return cap, skin, cx, cy, ztop


def flat_skin(x0, y0, x1, y1, z):
    return box(x0, y0, z + DECAL_LO, x1, y1, z + DECAL_HI)


def decal(solid, skin, out):
    """Prints `solid` on a surface: only the part of it inside the thin skin
    above that surface is kept."""
    skin_solid = skin.val()
    for piece in solid.solids().vals():
        printed = piece.intersect(skin_solid)
        if printed.Volume() <= 0.0:
            raise ValueError("a legend missed the key top")
        out.append(cq.Workplane("XY").add(printed))


def legend(text, size, cx, cy, z, skin, fit=0.0):
    wp = cq.Workplane("XY").text(text, size, 3.0, fontPath=FONT_KEY, halign="center", valign="center")
    bb = wp.val().BoundingBox()
    if fit > 0.0 and bb.xmax - bb.xmin > fit:
        size *= fit / (bb.xmax - bb.xmin)
        wp    = cq.Workplane("XY").text(text, size, 3.0, fontPath=FONT_KEY, halign="center", valign="center")
        bb    = wp.val().BoundingBox()
    decal(wp.translate((cx - (bb.xmin + bb.xmax) * 0.5, cy - (bb.ymin + bb.ymax) * 0.5, z - 1.5)), skin, legend_part)


def key(u, row, w_u, labels=(), color=DARK_KEY, word=False, mark=None, convex=False):
    x0 = KEYS_X0 + u * KEY_PITCH + (KEY_PITCH - KEY_CAP) * 0.5
    y0 = KEYS_Y1 - (row + 1) * KEY_PITCH + (KEY_PITCH - KEY_CAP) * 0.5
    w  = w_u * KEY_PITCH - (KEY_PITCH - KEY_CAP)
    cap, skin, cx, cy, zt = keycap_solid(x0, y0, w, KEY_CAP, KEY_H, convex)
    add_part(key_parts, color, cap)

    if word:
        lines = labels[0].split("\n")
        for i, line in enumerate(lines):
            legend(line, 2.8, cx, cy - 0.4 + (len(lines) - 1) * 1.7 - i * 3.4, zt, skin, fit=w - 2.0 * KEY_TOP_IN - 1.5)
    elif len(labels) == 2:
        legend(labels[0], 4.2, cx, cy + 2.0, zt, skin)
        legend(labels[1], 4.2, cx, cy - 2.8, zt, skin)
    elif len(labels) == 1 and labels[0] in ARROWS:
        decal(arrow_solid(ARROWS[labels[0]], cx, cy - 1.8, zt - 1.5), skin, legend_part)
    elif len(labels) == 1:
        legend(labels[0], 5.0, cx, cy - (3.0 if labels[0].isalnum() else 0.4), zt, skin)

    if mark is not None:
        outer = mark_solid(cx, cy, zt - 1.5, 6.0)
        if mark == "open":
            # The open key's silhouette is an outline.
            outer = outer.cut(mark_solid(cx, cy - 0.15, zt - 1.6, 4.2))
        decal(outer, skin, legend_part)


ARROWS = {"↑": 90.0, "←": 180.0, "→": 0.0, "↓": 270.0}


def arrow_solid(angle, cx, cy, z):
    """A bold arrow pointing at `angle` degrees, 7 mm long, 3 mm tall from z up."""
    pts = [(-3.5, -0.55), (0.9, -0.55), (0.9, -1.8), (3.5, 0.0), (0.9, 1.8), (0.9, 0.55), (-3.5, 0.55)]
    return (cq.Workplane("XY").polyline(pts).close().extrude(3.0)
              .rotate((0, 0, 0), (0, 0, 1), angle).translate((cx, cy, z)))


def mark_solid(cx, cy, z, height):
    """Casso's silhouette, a few millimeters tall from z up, centered at (cx, cy)."""
    sil  = Model()
    badge(sil, 0.0, 0.0, height, y=0.0, prefix="mark")
    flat = [part.solid.rotate((0, 0, 0), (1, 0, 0), -90.0) for part in sil.parts]
    bb   = flat[0].BoundingBox()
    for f in flat[1:]:
        bb = bb.add(f.BoundingBox())
    out = None
    for f in flat:
        # Stretched 10 times in z, so it stands through the print layer.
        solid = f.transformGeometry(cq.Matrix([[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 10.0, 0.0]]))
        sb    = solid.BoundingBox()
        solid = solid.translate(cq.Vector(cx - (bb.xmin + bb.xmax) * 0.5, cy - (bb.ymin + bb.ymax) * 0.5, z - sb.zmin))
        out   = cq.Workplane("XY").add(solid) if out is None else out.union(cq.Workplane("XY").add(solid))
    return out


def blank(u, row):
    """A flat filler in the bottom row, a little above the well's floor."""
    x0 = KEYS_X0 + u * KEY_PITCH + 0.6
    y0 = KEYS_Y1 - (row + 1) * KEY_PITCH + 0.6
    add_part(key_parts, BLANK_KEY, box(x0, y0, KEY_Z0, x0 + KEY_PITCH - 1.2, y0 + KEY_PITCH - 1.2, KEY_Z0 + 3.0)
                                     .edges("not <Z").fillet(0.6))


def power_lens(u, row):
    """The filler left of the space bar: a translucent white lens with the
    power lamp under it, the Disk ///'s lamp in size, glowing through it."""
    x0  = KEYS_X0 + u * KEY_PITCH + 0.6
    y0  = KEYS_Y1 - (row + 1) * KEY_PITCH + 0.6
    x1  = x0 + KEY_PITCH - 1.2
    top = KEY_Z0 + 3.0
    lx  = (x0 + x1) * 0.5
    ly  = y0 + (KEY_PITCH - 1.2) * 0.5
    lens = box(x0, y0, KEY_Z0, x1, y0 + KEY_PITCH - 1.2, top).edges("not <Z").fillet(0.6)
    add_part(key_parts, LENS_KEY, lens)
    lamp = box(lx - LAMP_W * 0.5, ly - LAMP_H * 0.5, top - 1.4, lx + LAMP_W * 0.5, ly + LAMP_H * 0.5, top - 0.6)
    glow = flat_skin(lx - LAMP_W * 0.5 - 0.4, ly - LAMP_H * 0.5 - 0.4, lx + LAMP_W * 0.5 + 0.4, ly + LAMP_H * 0.5 + 0.4, top)
    return lamp.union(glow)


PAIRS = [("!", "1"), ("@", "2"), ("#", "3"), ("$", "4"), ("%", "5"), ("^", "6"), ("&", "7"), ("*", "8"),
         ("(", "9"), (")", "0"), ("_", "-"), ("+", "=")]

key(0, 0, 1.0, ("ESCAPE",), word=True)
for i, pair in enumerate(PAIRS):
    key(1 + i, 0, 1.0, pair)
key(13, 0, 1.25, ("|", "\\"))

key(0, 1, 1.25, ("TAB",), word=True)
for i, ch in enumerate("QWERTYUIOP"):
    key(1.25 + i, 1, 1.0, (ch,))
key(11.25, 1, 1.0, ("{", "["))
key(12.25, 1, 1.0, ("}", "]"))
key(13.25, 1, 1.0, ("~", "`"))

key(0, 2, 1.75, ("CONTROL",), word=True)
for i, ch in enumerate("ASDFGHJKL"):
    key(1.75 + i, 2, 1.0, (ch,))
key(10.75, 2, 1.0, (":", ";"))
key(11.75, 2, 1.0, ("\"", "'"))
key(12.75, 2, 1.5, ("RETURN",), word=True)

key(0, 3, 2.25, ("SHIFT",), word=True)
for i, ch in enumerate("ZXCVBNM"):
    key(2.25 + i, 3, 1.0, (ch,))
key(9.25, 3, 1.0, ("<", ","))
key(10.25, 3, 1.0, (">", "."))
key(11.25, 3, 1.0, ("?", "/"))
key(12.25, 3, 1.0, ("SHIFT",), word=True)
key(13.25, 3, 1.0, ("\u2191",), color=LIGHT_KEY)

key(0, 4, 1.0, ("ALPHA\nLOCK",), color=LIGHT_KEY, word=True)
key(1, 4, 1.0, (), color=LIGHT_KEY, mark="open")
key(2, 4, 1.0, (), color=LIGHT_KEY, mark="solid")
power_led = power_lens(3, 4)
key(4, 4, 6.25, convex=True)
blank(10.25, 4)
key(11.25, 4, 1.0, ("\u2190",), color=LIGHT_KEY)
key(12.25, 4, 1.0, ("\u2192",), color=LIGHT_KEY)
key(13.25, 4, 1.0, ("\u2193",), color=LIGHT_KEY)

PAD = MAIN_U + PAD_GAP_U
for row, keys_in_row in enumerate([["7", "8", "9"], ["4", "5", "6"], ["1", "2", "3"]]):
    for col, ch in enumerate(keys_in_row):
        key(PAD + col, row, 1.0, (ch,))
key(PAD, 3, 2.0, ("0",))
key(PAD + 2, 3, 1.0, (".",))
key(PAD, 4, 1.0, ("\u2013",))
key(PAD + 1, 4, 2.0, ("ENTER",), word=True)

for color, solid in key_parts.items():
    name = {DARK_KEY: "keys", LIGHT_KEY: "keys_light", BLANK_KEY: "keys_blank", LENS_KEY: "power_lens"}[color]
    m.add(name, on_keyboard(solid), color, tolerance=0.02)

legends = cq.Workplane("XY").add(cq.Compound.makeCompound([part.val() for part in legend_part]))
m.add("key_legends", on_keyboard(legends), LEGEND, tolerance=0.02, angular=0.1)
m.add("power_led", on_keyboard(power_led), LED_GLOW)

# --------------------------------------------------------------------- cover

cover = profile([(0.0, FACE_BOT_Z), (KINK_Y, KINK_Z), (COVER_END, COVER_BOT),
                 (COVER_END, TOP), (FACE_TOP_Y, TOP)])

# The molding has no sharp edges: the face rolls into the top, and the top
# and the face roll into the sides. The back edge, against the fins, is left
# square.
cover = cover.edges("|X").edges(cq.selectors.BoxSelector((-1.0, FACE_TOP_Y - 1.0, TOP - 1.0), (W + 1.0, FACE_TOP_Y + 1.0, TOP + 1.0))).fillet(COVER_ROLL_R)
cover = cover.edges("|X").edges(cq.selectors.BoxSelector((-1.0, -1.0, FACE_BOT_Z - 1.0), (W + 1.0, 1.0, FACE_BOT_Z + 1.0))).fillet(COVER_LIP_R)
cover = cover.faces("<X or >X").edges(cq.selectors.BoxSelector((-1.0, -5.0, -1.0), (W + 1.0, COVER_END - 0.5, TOP + 1.0))).fillet(COVER_SIDE_R)
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

# The maker's word to the right of the silhouette, in light gray.
word = (cq.Workplane("XZ").text("apple", 10.0, 0.12, fontPath=r"C:\Windows\Fonts\GOTHICB.TTF",
                                halign="center", valign="center"))
wb   = word.val().BoundingBox()
k    = 30.0 / (wb.xmax - wb.xmin)
word = (cq.Workplane("XZ").text("apple", 10.0 * k, 0.12, fontPath=r"C:\Windows\Fonts\GOTHICB.TTF",
                                halign="center", valign="center"))
wb   = word.val().BoundingBox()
word = word.translate((PLATE1_X1 - 3.0 - 15.0 - (wb.xmin + wb.xmax) * 0.5, -0.4 - wb.ymax,
                       (PLATE_V0 + PLATE_V1) * 0.5 - (wb.zmin + wb.zmax) * 0.5))
m.add("logo_word", on_face(word), (0.780, 0.770, 0.740))

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
fins = fins.faces(">Z").edges(cq.selectors.BoxSelector((-1.0, COVER_END + 0.5, TOP - 1.0), (W + 1.0, D + 1.0, TOP + 1.0))).fillet(FIN_TOP_R)

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
