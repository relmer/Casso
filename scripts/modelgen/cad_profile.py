"""Apple ProFile (A9M0005) as a solid model, all six sides: the wide, low 5 MB
and 10 MB hard disk of the Apple III and Lisa, on the Apple II ProFile
interface card. X right, Y back, Z up; front face at y = 0.

THE DIMENSIONS are Apple's: 17.28 in wide, 4.39 in high, 8.81 in deep
(apple3.org technote 1128, and the Computer History Museum's catalog entry
102681317 at 17 1/4 x 4 1/2 x 9 in).

MODELED FROM PHOTOGRAPHS, for the owner's review (T108):

  - Front: the Powerhouse Museum, Sydney, object 457105
    (collection.powerhouse.com.au/object/457105), image 501435, and the
    owner's close-up of the front plates.
  - Back: RR Auction lot 3504391, cdn.rrauction.com/auction/724/3504391_10.jpg,
    a straight-on photograph 2000 px wide. The case runs from 61 px to 1940
    px across and its bottom is at 632 px, so a pixel is W / 1879 mm, and
    every position on the back below is read off it in pixels (BX, BZ).
  - A ProFile on a Lisa: commons.wikimedia.org/wiki/File:Apple_Lisa.jpg
    (CC BY-SA 3.0).

  EDGES. The front's top edge is rounded over, and the front's vertical
  edges and the top's side edges are 45-degree chamfers, one band that
  sweeps up each front corner and over the rounded edge onto the top. The
  rear's vertical edges are rounded; the top's rear edge is a chamfer that
  runs round them into the side chamfers. The bottom's rear edge has a
  small round, and the front's bottom edge a chamfer.

  FRONT. Plain. At the lower left, two dark plates a little apart in
  shallow pockets with rounded corners: a square one with the badge and a
  long one with "profile", "pro" in outlined letters and "file" in solid
  ones. At the lower right, a square plate of the same kind with "READY"
  over a red lamp.

  TOP. A large shallow recessed panel inside a border.

  BACK. Two recessed brown metal panels, each with a row of vent slots in
  the case above it and a screw at each top corner. The left panel has
  vertical vents and the model and serial label. The right panel has vents,
  a DB-25 socket in a cutout, the caution label, an IEC power inlet and the
  ON/OFF rocker switch. The case's seam runs round the back and sides a
  little over a quarter of the way up.

  SIDES. The seam, and a group of vent slots low at the back of each side.

  BOTTOM. Six rubber feet, three across the front and three across the back
  (the Powerhouse record).

Sub-mesh identity is by part NAME and color: `lamp` is the ready lamp in the
drive-lamp color; `front_anchor` is the front plane the scene lines devices
up on.
"""

import os

import cadquery as cq
from cadkit import KD, Model

from cad_badge import badge, badge_ink
from cad_iec import c14, c14_cut

INCH        = 25.4
W, D, H     = 17.28 * INCH, 8.81 * INCH, 4.39 * INCH
CORNER_ANG  = 0.14
FRONT_R     = H * 0.14
CHAMFER     = 5.0
REAR_R      = 12.0
BOTTOM_R    = 1.5
E           = 0.01

GOTHIC_BOLD = "C:/Windows/Fonts/GOTHICB.TTF"

CASE        = (0.880, 0.850, 0.760)       # Apple III and Lisa beige
PLATE       = (0.215, 0.205, 0.195)
EMBOSS      = (0.640, 0.650, 0.690)
PANEL       = (0.400, 0.370, 0.320)
DARK        = (0.060, 0.060, 0.060)
LABEL       = (0.950, 0.945, 0.925)
CREAM       = (0.900, 0.870, 0.780)
SERIAL      = (0.940, 0.910, 0.820)
GRAYLABEL   = (0.850, 0.860, 0.810)
BAND        = (0.330, 0.310, 0.290)
INK         = (0.080, 0.080, 0.085)       # clear of the desk scene's finish markers
YELLOW      = (0.950, 0.720, 0.100)
SILVER      = (0.780, 0.780, 0.800)
METAL       = (0.650, 0.650, 0.650)
INSERT      = (0.150, 0.150, 0.155)
RUBBER      = (0.120, 0.110, 0.100)

SEAM_Z      = H * 0.28

m = Model()


# ------------------------------------------------------- the back's measures

PX = W / 1879.0


def BX(px):
    # A photograph x on the back to mm from the left edge as seen from behind.
    return (px - 61.0) * PX


def BZ(py):
    # A photograph y on the back to mm above the bottom.
    return (632.0 - py) * PX


# ------------------------------------------------------------- the helpers

def edges(body, pred):
    return [e for e in body.Edges() if pred(e.BoundingBox())]


def rrect(w, h, r, depth):
    # A rounded rectangle in XZ from (0, 0), `depth` deep toward +Y.
    return (cq.Workplane("XY").box(w, depth, h, centered=(False, False, False))
            .edges("|Y").fillet(r))


def flat(solid, cx, cz, y0):
    # Turn geometry drawn in XY (reading +X, up +Y, raised +Z from 0) onto a
    # face: up becomes +Z, the raise points -Y, and its top is at y0.
    bb = solid.BoundingBox()

    return (solid.translate(cq.Vector(-(bb.xmin + bb.xmax) * 0.5, -(bb.ymin + bb.ymax) * 0.5, 0))
            .rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), 90)
            .translate(cq.Vector(cx, y0, cz)))


def glyphs(s, size, font = "Arial", kind = "regular", path = None, depth = 0.15):
    wp = cq.Workplane("XY").text(s, size, depth, font=font, fontPath=path, kind=kind,
                                 halign="left", valign="bottom")

    return wp.val()


def shrink(face, t):
    # The faces inside the face's outline pulled in by `t`. A narrow part of
    # a letter can pinch the pulled-in outline into more than one loop, so
    # every loop is a face; the largest has the letter's holes, grown by `t`.
    def offsets(wire, inward):
        size = wire.BoundingBox().DiagonalLength

        for d in (-t, t):
            try:
                loops = wire.offset2D(d, "arc")
            except Exception:
                continue

            if loops and (max(w.BoundingBox().DiagonalLength for w in loops) < size) == inward:
                return sorted(loops, key=lambda w: -w.BoundingBox().DiagonalLength)

        return []

    loops = offsets(face.outerWire(), True)
    holes = [offsets(w, False) for w in face.innerWires()]
    faces = []

    if not loops or any(not h for h in holes):
        return faces

    for i, loop in enumerate(loops):
        try:
            faces.append(cq.Face.makeFromWires(loop, [h[0] for h in holes] if i == 0 else []))
        except Exception:
            pass

    return faces


def logotype(width, stroke, depth = 0.2):
    # "profile" as on the ProFile: "pro" in outlined letters and "file" in
    # solid ones, in a geometric bold face, `width` mm wide, lying in XY.
    word   = glyphs("profile", 10.0, path=GOTHIC_BOLD, depth=depth)
    split  = glyphs("pro", 10.0, path=GOTHIC_BOLD, depth=depth).BoundingBox().xmax
    scale  = width / word.BoundingBox().xlen
    pieces = []

    for face in cq.Workplane().add(word).faces("<Z").vals():
        solid = cq.Solid.extrudeLinear(face, cq.Vector(0, 0, depth))

        if face.Center().x < split:
            for inner in shrink(face, stroke / scale):
                solid = solid.cut(cq.Solid.extrudeLinear(inner, cq.Vector(0, 0, depth)))

        pieces.append(solid)

    return cq.Compound.makeCompound(pieces).scale(scale)


# ------------------------------------------------------------------ the case
#
# The rear's vertical edges are rounded first, so the top's edge is one
# smooth run from the front corner, over the rounded front edge, along the
# side, round the rear corner and across the back, and one chamfer follows
# all of it.

def top_run(b):
    if b.xmax < E or b.xmin > W - E:
        if b.zmax < E or (b.xlen < E and b.ylen < E and b.ymin > E):
            return False

        return b.zmax > CHAMFER

    return b.zmin > H - E and b.ymin > D - REAR_R - E


s = cq.Workplane("XY").box(W, D, H, centered=(False, False, False)).val()
s = s.fillet(REAR_R, edges(s, lambda b: b.xlen < E and b.ylen < E and b.ymin > D - E))
s = s.fillet(FRONT_R, edges(s, lambda b: b.ylen < E and b.zlen < E and b.ymax < E and b.zmin > H - E))
s = s.chamfer(CHAMFER, None, edges(s, lambda b: b.ylen < E and b.zlen < E and b.ymax < E and b.zmax < E))
s = s.chamfer(CHAMFER, None, edges(s, top_run))
s = s.fillet(BOTTOM_R, edges(s, lambda b: b.zmax < E and b.ymax > CHAMFER + E))
case = cq.Workplane("XY").add(s)

# The seam round the sides and back.
seam = (cq.Workplane("XY").box(W + 2.0, D - 5.0, 0.8, centered=(False, False, False))
        .translate((-1.0, 6.0, SEAM_Z)))
seam = seam.cut(cq.Workplane("XY").box(W - 1.2, D - 0.6, 2.0, centered=(False, False, False))
                .edges("|Z").edges(">Y").fillet(REAR_R - 0.6)
                .translate((0.6, 0.0, SEAM_Z - 0.5)))
case = case.cut(seam)

# The top's recessed panel.
case = case.cut(cq.Workplane("XY").box(W * 0.86, D * 0.72, 1.6, centered=(False, False, False))
                .edges("|Z").fillet(4.0)
                .translate((W * 0.07, D * 0.17, H - 1.2)))

# Sides: vents low at the back.
for x in (-1.0, W - 2.0):
    for i in range(8):
        case = case.cut(cq.Workplane("XY").box(3.0, 2.4, H * 0.12, centered=(False, False, False))
                        .translate((x, D * 0.80 + i * 4.5, H * 0.08)))


# The back is laid out as seen from behind, in the photograph's pixels, and
# mirrored across the centerline into the model's x.

def back(solid):
    return solid.mirror("YZ", (W * 0.5, 0, 0))


def back_add(name, solid, rgb):
    m.add(name, back(solid), rgb)


def back_box(px0, py0, px1, py1, y0, depth, inset = 0.0):
    px0, py0, px1, py1 = px0 + inset, py0 + inset, px1 - inset, py1 - inset

    return (cq.Workplane("XY").box(BX(px1) - BX(px0), depth, BZ(py0) - BZ(py1), centered=(False, False, False))
            .translate((BX(px0), y0, BZ(py1))))


PANELS  = ((140, 265, 822, 570), (925, 265, 1866, 572))
BACK_Y  = D - 2.6          # the panels, recessed into the case
FACE    = BACK_Y + 0.6     # their outer face, where the details sit
DB_CUT  = (1497, 278, 1750, 358)
SWITCH  = (1727, 415, 1797, 555)

for px0, py0, px1, py1 in PANELS:
    case = case.cut(back(back_box(px0, py0, px1, py1, D - 3.0, 6.0)))

# The vent rows in the case above the panels.
VENT_ROWS = ((198, 738), (1005, 1785))

for px0, px1 in VENT_ROWS:
    n = int((px1 - px0) / 21.5) + 1

    for i in range(n):
        x = BX(px0 + i * 21.5)
        case = case.cut(back(cq.Workplane("XY").box(2.3, 9.0, BZ(222) - BZ(250), centered=(False, False, False))
                             .translate((x, D - 7.0, BZ(250)))))

# Pockets behind the socket's cutout, the inlet and the switch, a little
# inside the parts' outlines so the parts cover their edges.
for px0, py0, px1, py1 in (DB_CUT, SWITCH):
    case = case.cut(back(back_box(px0, py0, px1, py1, D - 12.0, 12.0, inset = 2.0)))

IEC_CX, IEC_CZ = BX(1537.5), BZ(492)
IEC_PROUD      = 1.5
IEC_SPACE      = c14_cut(earth_up = False, proud = IEC_PROUD).translate((IEC_CX, FACE, IEC_CZ))
case           = case.cut(back(IEC_SPACE))

# Pockets for the front plates.
PLATE_S            = 15.4
PLATE_Z0           = 23.9
LOGO_X0            = 38.5
GAP                = 2.0
NAME_W             = 42.5
READY_X0           = 381.0
POCKET             = 2.5
PLATE_FACE         = 2.0
PLATE_R            = 1.5
FRONT_PLATES       = ((LOGO_X0, PLATE_S), (LOGO_X0 + PLATE_S + GAP, NAME_W), (READY_X0, PLATE_S))

for x0, w in FRONT_PLATES:
    case = case.cut(rrect(w, PLATE_S, PLATE_R, POCKET + 1.0).translate((x0, -1.0, PLATE_Z0)))

m.add("case", case, CASE, angular=CORNER_ANG)

# The vent openings show dark behind them.
for px0, px1 in VENT_ROWS:
    back_add(f"vent_shadow_{px0}",
             cq.Workplane("XY").box(BX(px1) - BX(px0) + 4.0, 0.5, BZ(222) - BZ(250) + 2.0, centered=(False, False, False))
               .translate((BX(px0) - 1.0, D - 7.0, BZ(250) - 1.0)),
             DARK)

for px0, py0, px1, py1 in (DB_CUT, SWITCH):
    back_add(f"pocket_{px0}", back_box(px0, py0, px1, py1, D - 12.5, 0.6), DARK)

# ---------------------------------------------------------------- the front

for name, (x0, w) in zip(("logo_plate", "name_plate", "ready_plate"), FRONT_PLATES):
    m.add(name, rrect(w, PLATE_S, PLATE_R, POCKET - PLATE_FACE + 0.2).translate((x0, PLATE_FACE, PLATE_Z0)), PLATE)

# The badge, centered on its plate.
logo_h                      = PLATE_S * 0.66
ink_x, ink_z, ink_w, ink_h  = badge_ink(logo_h)

badge(m, LOGO_X0 + PLATE_S * 0.5 - ink_x - ink_w * 0.5,
      PLATE_Z0 + PLATE_S * 0.5 - ink_z - ink_h * 0.5, logo_h, y = PLATE_FACE - 0.4)

name_x0 = LOGO_X0 + PLATE_S + GAP
m.add("name_lettering", flat(logotype(NAME_W * 0.745, 0.45), name_x0 + NAME_W * 0.5, PLATE_Z0 + PLATE_S * 0.55, PLATE_FACE),
      EMBOSS)

READY_CX = READY_X0 + PLATE_S * 0.5
m.add("ready_lettering", flat(glyphs("READY", 3.0, kind="bold"), READY_CX, PLATE_Z0 + PLATE_S * 0.68, PLATE_FACE), LABEL)
m.add("lamp",
      cq.Workplane("XY").box(PLATE_S * 0.40, 0.5, PLATE_S * 0.14, centered=(False, False, False))
        .translate((READY_CX - PLATE_S * 0.20, PLATE_FACE - 0.3, PLATE_Z0 + PLATE_S * 0.25)),
      KD["drive_lamp"])

# ----------------------------------------------------------------- the back

def back_text(s, cap, px, py, rgb, name, y, font = "Arial", kind = "regular", centered = False, turn = 0):
    # Lettering on the back, `cap` mm tall in capitals, its left edge (or
    # center) at photograph x `px` and its middle at photograph y `py`.
    g  = glyphs(s, cap / 0.716, font=font, kind=kind)

    if turn:
        g = g.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), turn)

    bb = g.BoundingBox()
    cx = BX(px) if centered or turn else BX(px) + bb.xlen * 0.5

    back_add(name, flat(g, cx, BZ(py), y + 0.15), rgb)


def back_rule(px0, px1, py, y, name, vertical_to = None):
    if vertical_to is None:
        solid = back_box(px0, py - 0.75, px1, py + 0.75, y, 0.15)
    else:
        solid = back_box(px0 - 0.75, py, px0 + 0.75, vertical_to, y, 0.15)

    back_add(name, solid, INK)


for i, (px0, py0, px1, py1) in enumerate(PANELS):
    panel = back_box(px0, py0, px1, py1, BACK_Y, 0.6)

    if i == 1:
        for cutout in (DB_CUT, SWITCH):
            panel = panel.cut(back_box(*cutout, BACK_Y - 1.0, 3.0, inset = 2.0 if cutout != DB_CUT else 0.0))

        panel = panel.cut(IEC_SPACE)

    back_add(f"back_panel_{i}", panel, PANEL)

# A Phillips screw at each panel's top corners.
for i, (px, py) in enumerate(((155, 280), (785, 280), (950, 280), (1848, 285))):
    head = cq.Workplane("XZ").circle(2.1).extrude(-0.8).translate((BX(px), FACE, BZ(py)))

    for w, h in ((2.4, 0.5), (0.5, 2.4)):
        head = head.cut(cq.Workplane("XY").box(w, 1.0, h).translate((BX(px), FACE + 0.8, BZ(py))))

    back_add(f"screw_{i}", head, METAL)


def panel_vents(px0, px1, py0, py1, name):
    n = int((px1 - px0) / 27.0) + 1

    for i in range(n):
        back_add(f"{name}{i}", back_box(px0 + i * 27.0, py0, px0 + i * 27.0 + 12.0, py1, FACE - 0.1, 0.3), DARK)


panel_vents(192, 742, 283, 410, "left_vent")
panel_vents(1045, 1480, 283, 388, "right_vent")

# The model and serial label.
LY = FACE
back_add("model_label", back_box(264, 411, 711, 545, LY, 0.3), CREAM)
back_add("model_band", back_box(264, 411, 711, 450, LY + 0.3, 0.1), BAND)
back_add("model_logotype", flat(logotype(BX(392) - BX(292), 0.15, 0.15), BX(342), BZ(436), LY + 0.55), LABEL)

ink_x, ink_z, ink_w, ink_h = badge_ink(BZ(415) - BZ(445))
badge(m, BX(666) - ink_x - ink_w * 0.5, BZ(430) - ink_z - ink_h * 0.5, BZ(415) - BZ(445),
      y = LY + 0.3, prefix = "back_badge", color = SILVER, place = back)

back_add("serial_sticker", back_box(447, 452, 639, 473, LY + 0.3, 0.1), SERIAL)
back_text("101793", 3.0, 573, 457, INK, "serial_text", LY + 0.4, font="Courier New", kind="bold", centered=True)
back_text("Model Number", 2.3, 295, 464, INK, "model_caption", LY + 0.3, kind="bold")
back_text("A9M0005", 3.5, 295, 481.5, INK, "model_text", LY + 0.3, kind="bold")
back_rule(264, 711, 490, LY + 0.3, "model_rule")
for i, line in enumerate(("Apple Computer", "Cupertino, California 95014", "Made in USA")):
    back_text(line, 2.0, 487, 501 + i * 13, INK, f"maker_text{i}", LY + 0.3)
back_text("825-0114", 1.2, 296, 531, INK, "model_part", LY + 0.3)

# The caution label.
back_add("caution_label", back_box(959, 397, 1406, 532, LY, 0.3), GRAYLABEL)
back_add("caution_band", back_box(959, 397, 1406, 437, LY + 0.3, 0.1), BAND)
back_text("Caution", 5.1, 991, 418.5, YELLOW, "caution_text", LY + 0.4, kind="bold")
back_rule(1142, None, 437, LY + 0.3, "caution_divider", vertical_to = 532)
back_rule(1145, 1406, 485, LY + 0.3, "caution_rule")
tri    = cq.Workplane("XY").polyline([(0, 0), (BX(1076) - BX(1015), 0), ((BX(1076) - BX(1015)) * 0.5, BZ(451) - BZ(504))]).close()
stroke = 1.4
outer  = tri.extrude(0.15).val()
inner  = (cq.Workplane("XY").polyline([(stroke * 1.73, stroke), (BX(1076) - BX(1015) - stroke * 1.73, stroke),
                                       ((BX(1076) - BX(1015)) * 0.5, BZ(451) - BZ(504) - stroke * 2.0)])
          .close().extrude(0.15).val())
back_add("caution_triangle", flat(outer.cut(inner), BX(1045.5), BZ(477.5), LY + 0.45), YELLOW)
back_text("825-0113-B", 1.2, 987, 516.5, INK, "caution_part", LY + 0.3)
for i, (line, py) in enumerate((("Delicate Instrument", 457), ("Handle with Care", 471), ("Connect this unit", 493),
                                ("only to specified", 506), ("Apple Computer interface", 520))):
    back_text(line, 2.1, 1181, py, INK, f"caution_line{i}", LY + 0.3)

# The DB-25 socket: a plated flange with two mounting holes, the D shell,
# and the insert with thirteen holes over twelve, at 2.77 mm and 2.84 mm.
DB_CX, DB_CZ = BX(1622), BZ(318)
DB_FLANGE    = FACE - 0.6
flange       = rrect(53.04, 12.55, 1.0, 1.0).translate((DB_CX - 26.52, DB_FLANGE, DB_CZ - 6.275))

for hx in (-23.52, 23.52):
    flange = flange.cut(cq.Workplane("XZ").circle(1.525).extrude(-3.0).translate((DB_CX + hx, DB_FLANGE - 1.0, DB_CZ)))


def d_shape(top, height, r, depth):
    # A D-sub's trapezoid, wider at the top, with 10-degree sides, as a core
    # trapezoid grown by `r` all round, so every outline with the same core
    # has concentric corners.
    core_top    = top - 2.0 * r
    core_h      = height - 2.0 * r
    core_bottom = core_top - 2.0 * core_h * 0.1763

    return (cq.Workplane("XZ")
            .polyline([(-core_top * 0.5, core_h * 0.5), (core_top * 0.5, core_h * 0.5),
                       (core_bottom * 0.5, -core_h * 0.5), (-core_bottom * 0.5, -core_h * 0.5)]).close()
            .offset2D(r, "arc").extrude(-depth))


DB_R    = 1.4                  # the insert's corner radius
DB_WALL = 0.5

shell = (d_shape(37.38 + 2.0 * DB_WALL, 6.92 + 2.0 * DB_WALL, DB_R + DB_WALL, 6.0)
         .cut(d_shape(37.38, 6.92, DB_R, 6.0))
         .translate((DB_CX, DB_FLANGE + 1.0, DB_CZ)))
back_add("db25_flange", flange.union(shell), METAL)

insert = d_shape(37.3, 6.84, DB_R - 0.04, 1.0).translate((DB_CX, DB_FLANGE + 4.0, DB_CZ))

for dz, count in ((1.42, 13), (-1.42, 12)):
    for i in range(count):
        insert = insert.cut(cq.Workplane("XZ").circle(0.55).extrude(-2.0)
                            .translate((DB_CX - (count - 1) * 2.77 * 0.5 + i * 2.77, DB_FLANGE + 3.5, DB_CZ + dz)))

back_add("db25_insert", insert, INSERT)
back_add("db25_cutout", back_box(*DB_CUT, D - 3.0, 0.3), DARK)
back_add("db25_back", d_shape(37.3, 6.84, DB_R - 0.04, 0.4).translate((DB_CX, DB_FLANGE + 3.4, DB_CZ)), DARK)

# The IEC inlet, turned with its earth pin at the bottom, standing out from
# the panel.
c14(m, "iec", lambda solid: back(solid.translate((IEC_CX, FACE, IEC_CZ))), earth_up = False, proud = IEC_PROUD)

# The rocker switch: a black bezel, and the rocker with ON at the top and
# OFF at the bottom printed on it, turned to read upward.
SW_X0, SW_Z0 = BX(SWITCH[0]), BZ(SWITCH[3])
bezel        = rrect(BX(SWITCH[2]) - SW_X0, BZ(SWITCH[1]) - SW_Z0, 0.8, 4.0).translate((SW_X0, FACE - 2.0, SW_Z0))
bezel        = bezel.cut(back_box(1740, 430, 1787, 544, FACE - 1.0, 4.0))
back_add("switch_bezel", bezel, DARK)

ROCK_Y = FACE + 1.6
rocker = back_box(1742, 432, 1785, 542, FACE - 1.0, ROCK_Y - FACE + 1.0)
rocker = rocker.cut(back_box(1742, 432, 1785, 456, ROCK_Y - 0.8, 1.0))
back_add("switch_rocker", rocker, (0.170, 0.170, 0.175))
back_text("ON", 2.2, 1765, 472, LABEL, "switch_on", ROCK_Y, kind="bold", turn=90)
back_text("OFF", 2.2, 1765, 516, LABEL, "switch_off", ROCK_Y, kind="bold", turn=90)

# ------------------------------------------------------------- the anchors

m.add("front_anchor",
      cq.Workplane("XY").box(1.0, 1.0, 1.0).translate((W * 0.5, 10.0, H * 0.5)),
      KD["front_anchor"])

# ---------------------------------------------------------------- underside

FOOT_R, FOOT_H = 7.0, 3.0

for fx in (W * 0.07, W * 0.50, W * 0.93):
    for fy in (D * 0.12, D * 0.88):
        m.add(f"foot_{fx:.0f}_{fy:.0f}",
              cq.Workplane("XY").cylinder(FOOT_H, FOOT_R, centered=(True, True, False))
                .translate((fx, fy, -FOOT_H)),
              RUBBER)


if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "ProFile.mesh"),
                    os.path.join(out, "ProFile.mtl"), "ProFile.mtl")
    print(f"ProFile (CAD): {nv} verts, {nt} tris")
