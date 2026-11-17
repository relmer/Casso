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
import math
import os
import re

import cadquery as cq
from cadkit import KD, Model
from cad_champ import champ50
from cad_iec import c14, c14_cut

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
        .box(W - 2.0, D - 6.0, SEAM_GAP * 2.0, centered=(False, False, False))
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
# From a straight-on photograph of the back (ineedths.com, listing
# 374651785599, image 6, 1600 px wide), laid out as seen from behind and
# mirrored across the centerline into the model's x. The back runs from 90
# px to 1515 px across and from 450 px to 795 px down, so positions are read
# off it in pixels (BX, BZ); the camera is above the drive, so the scale
# down the back is H / 345 mm a pixel and across it W / 1425.
#
# One plastic panel set into the case fills most of the back: vent slots at
# the left, in two banks either side of a bar, with a lock loop below them;
# a metal plate in a window at the middle with the two 50-contact SCSI
# connectors (cad_champ.py), a SCSI icon beside each; the barcode label and
# the SCSI ID selector under them; vent slots at the upper right over the
# IEC inlet (cad_iec.py), and the power rocker with its | and O.

BACK_DARK  = (0.060, 0.060, 0.060)
BACK_METAL = (0.720, 0.720, 0.720)
BACK_LABEL = (0.950, 0.945, 0.930)
BACK_INK   = (0.080, 0.080, 0.085)
ROCKER     = (0.170, 0.170, 0.175)


def BX(px):
    return (px - 90.0) * W / 1425.0


def BZ(py):
    return (795.0 - py) * H / 345.0


def back(solid):
    return solid.mirror("YZ", (W * 0.5, 0, 0))


def back_add(name, solid, rgb):
    m.add(name, back(solid), rgb)


def pbox(px0, py0, px1, py1, y0, depth):
    return (cq.Workplane("XY").box(BX(px1) - BX(px0), depth, BZ(py0) - BZ(py1), centered=(False, False, False))
            .translate((BX(px0), y0, BZ(py1))))


def cut_case(solid):
    global lid, bottom

    lid    = lid.cut(back(solid))
    bottom = bottom.cut(back(solid))


def lettering(s, size, cx, cz, y, font = "Arial", kind = "regular", depth = 0.2):
    # Lettering centered at (cx, cz), raised `depth` toward +Y from y.
    g  = cq.Workplane("XY").text(s, size, depth, font=font, kind=kind, halign="center", valign="center").val()
    bb = g.BoundingBox()

    return (g.translate(cq.Vector(-(bb.xmin + bb.xmax) * 0.5, -(bb.ymin + bb.ymax) * 0.5, 0))
            .rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), 90)
            .mirror("XZ")
            .translate(cq.Vector(cx, y, cz)))


# The panel, in a recess 3 mm deep, its face 1 mm inside the case's.
PANEL     = (128, 493, 1505, 790)
PANEL_Y   = D - 1.0
RECESS_Y  = D - 3.0

cut_case(pbox(*PANEL, RECESS_Y, 4.0).edges("|Y").fillet(2.0))
panel = pbox(*PANEL, RECESS_Y, PANEL_Y - RECESS_Y).edges("|Y").fillet(2.0)

# Vent slots, cut through the panel and 6 mm into the case, dark at the end.
VENT_PITCH = 29.0
VENT_DEEP  = 6.0
vents      = ([(155 + i * VENT_PITCH, 512, 12, 645) for i in range(12)]
              + [(155 + i * VENT_PITCH, 655, 12, 785) for i in range(3, 12)]
              + [(1040 + i * VENT_PITCH, 500, 11, 605) for i in range(14)])

for k, (px, py0, w, py1) in enumerate(vents):
    slot  = pbox(px, py0, px + w, py1, PANEL_Y - VENT_DEEP, VENT_DEEP + 1.0)
    panel = panel.cut(slot)
    cut_case(slot)
    back_add(f"vent_floor{k}", pbox(px - 1, py0 - 1, px + w + 1, py1 + 1, PANEL_Y - VENT_DEEP - 0.2, 0.3), BACK_DARK)

# The connectors' window, and the metal plate behind it, 3 mm in.
WINDOW  = (522, 502, 962, 687)
PLATE_Y = PANEL_Y - 3.0
opening = pbox(*WINDOW, PLATE_Y - 0.5, 5.0)
panel   = panel.cut(opening)
cut_case(opening)
back_add("connector_plate", pbox(*WINDOW, PLATE_Y - 0.6, 0.6), BACK_METAL)

# The IEC inlet, earth pin at the top, its face just proud of the panel.
IEC_AT  = (BX(1166), PANEL_Y, BZ(707))
inlet   = c14_cut(proud = 0.5).translate(IEC_AT)
panel   = panel.cut(inlet)
cut_case(inlet)

# The power switch's bezel opening.
SWITCH  = (1300, 650, 1397, 747)
sw_cut  = pbox(*SWITCH, PANEL_Y - 8.0, 9.0)
panel   = panel.cut(sw_cut)
cut_case(sw_cut)

# The | and O beside it, engraved MARK_DEEP into the panel and centered on
# one line.
MARK_DEEP = 0.8
MARK_X    = BX(1424)
panel     = panel.cut(cq.Workplane("XY").box(0.9, MARK_DEEP + 1.0, BZ(662) - BZ(682), centered=(True, False, False))
                      .translate((MARK_X, PANEL_Y - MARK_DEEP, BZ(682))))
panel     = panel.cut(cq.Workplane("XZ").circle(2.4).circle(1.8).extrude(-(MARK_DEEP + 1.0))
                      .translate((MARK_X, PANEL_Y - MARK_DEEP, BZ(732))))

# The ~ over the inlet, centered on its earth pin, engraved the same way.
panel     = panel.cut(lettering("~", 6.0, IEC_AT[0], BZ(635), PANEL_Y - MARK_DEEP, depth=MARK_DEEP + 1.0))

# The lock loop sits centered in the plain area at the lower left, below
# the upper vents and left of the lower ones, with a chain of two links
# engraved over it. The loop and the chain are centered as a group.
VOID_X0, VOID_X1 = BX(128), BX(155 + 3 * VENT_PITCH)
VOID_Z0, VOID_Z1 = BZ(790), BZ(645)
LOOP_W, LOOP_H   = 11.0, 12.0
CHAIN_H, CHAIN_GAP = 6.0, 2.5
LOOP_CX          = (VOID_X0 + VOID_X1) * 0.5
LOOP_Z0          = (VOID_Z0 + VOID_Z1 - (LOOP_H + CHAIN_GAP + CHAIN_H)) * 0.5
CHAIN_CZ         = LOOP_Z0 + LOOP_H + CHAIN_GAP + CHAIN_H * 0.5


def link(cz):
    # One link of the chain: a rounded ring, its stroke cut into the panel.
    w, h, t = 3.4, 4.0, 0.6
    ring    = (cq.Workplane("XZ").rect(w, h).extrude(-(MARK_DEEP + 1.0)).edges("|Y").fillet(w * 0.5 - 0.01)
               .cut(cq.Workplane("XZ").rect(w - 2 * t, h - 2 * t).extrude(-(MARK_DEEP + 1.0)).edges("|Y")
                    .fillet(w * 0.5 - t - 0.01)))

    return ring.translate((LOOP_CX, PANEL_Y - MARK_DEEP, cz))


for cz in (CHAIN_CZ - 1.25, CHAIN_CZ + 1.25):
    panel = panel.cut(link(cz))

back_add("back_panel", panel, CASE)

# The two SCSI connectors on the plate, their bails lying open over the
# panel.
for name, py in (("scsi_upper", 540), ("scsi_lower", 645)):
    at = (BX(742), PLATE_Y, BZ(py))
    champ50(m, name, lambda solid, at = at: back(solid.translate(at)), bail_y = PANEL_Y - PLATE_Y + 1.0)

# A SCSI icon raised on the panel beside each connector: a diamond with an
# arrow pointing in.
def scsi_icon(cx, cz):
    w, h, t = 8.0, 6.4, 0.6
    outer   = cq.Workplane("XZ").polyline([(-w / 2, 0), (0, h / 2), (w / 2, 0), (0, -h / 2)]).close().extrude(-0.3)
    inner   = (cq.Workplane("XZ").polyline([(-w / 2 + t * 1.6, 0), (0, h / 2 - t * 1.25), (w / 2 - t * 1.6, 0), (0, -h / 2 + t * 1.25)])
               .close().extrude(-0.3))
    arrow   = (cq.Workplane("XZ").polyline([(-2.0, 0), (-0.6, 1.2), (-0.6, 0.4), (1.8, 0.4), (1.8, -0.4), (-0.6, -0.4), (-0.6, -1.2)])
               .close().extrude(-0.3))

    return outer.cut(inner).union(arrow).translate((cx, PANEL_Y, cz))


for k, py in enumerate((545, 650)):
    back_add(f"scsi_icon{k}", scsi_icon(BX(1012), BZ(py)), CASE)

# The lock loop: a tab standing out from the panel, its outer face rounded
# over at the top and bottom and at the sides, with an opening through it
# from side to side, between the panel and the outer face, for a lock's
# cable.
LOOP_OUT  = 6.5
LOOP_HOLE = 4.0          # the opening's depth out from the panel
loop      = (cq.Workplane("XY").box(LOOP_W, LOOP_OUT + 0.5, LOOP_H, centered=(True, False, False))
             .translate((LOOP_CX, PANEL_Y - 0.5, LOOP_Z0)))
loop      = loop.edges("|X").edges(">Y").fillet(2.8)
loop      = loop.edges("|Z").edges(">Y").fillet(1.8)
loop      = loop.cut(cq.Workplane("XY").box(LOOP_W + 2.0, LOOP_HOLE + 0.6, LOOP_H - 5.0, centered=(True, False, False))
                     .edges("|X").fillet(0.8)
                     .translate((LOOP_CX, PANEL_Y - 0.6, LOOP_Z0 + 2.5)))
back_add("lock_loop", loop, CASE)

# The barcode label, with the serial number in Code 39 and printed under it.
CODE39 = {
    "0": "nnnwwnwnn", "1": "wnnwnnnnw", "2": "nnwwnnnnw", "4": "nnnwwnnnw", "6": "nnwwwnnnn",
    "9": "nnwwnnwnn", "F": "nnwnwwnnn", "H": "wnnnnwwnn", "M": "wnwnnnnwn", "S": "nnwnnnwwn",
    "*": "nwnnwnwnn",
}
SERIAL = "F9241SHM2604"

back_add("barcode_label", pbox(538, 700, 858, 790, PANEL_Y, 0.3), BACK_LABEL)

elements = []
for ch in "*" + SERIAL + "*":
    elements += [(k % 2 == 0, 2.5 if e == "w" else 1.0) for k, e in enumerate(CODE39[ch])]
    elements.append((False, 1.0))

units     = sum(u for _, u in elements)
bar_x0    = BX(588)
bar_unit  = (BX(812) - BX(588)) / units
x         = bar_x0
bars      = []

for is_bar, u in elements:
    if is_bar:
        bars.append(cq.Workplane("XY").box(u * bar_unit, 0.15, BZ(712) - BZ(758), centered=(False, False, False))
                    .translate((x, PANEL_Y + 0.3, BZ(758))).val())

    x += u * bar_unit

back_add("barcode", cq.Workplane("XY").add(cq.Compound.makeCompound(bars)), BACK_INK)
back_add("barcode_text", lettering("*" + SERIAL + "*", 2.4, (BX(588) + BX(812)) * 0.5, BZ(773), PANEL_Y + 0.3, font="Courier New"), BACK_INK)

# The SCSI ID selector, its window, and the hole under it.
back_add("scsi_id_selector", pbox(902, 712, 930, 738, PANEL_Y, 0.6), BACK_DARK)
back_add("scsi_id_window", pbox(909, 718, 923, 731, PANEL_Y + 0.6, 0.1), (0.85, 0.85, 0.85))
back_add("scsi_id_hole", cq.Workplane("XZ").circle(0.9).extrude(-0.1).translate((BX(912), PANEL_Y + 0.05, BZ(755))), BACK_DARK)

# The IEC inlet's parts.
c14(m, "iec", lambda solid: back(solid.translate(IEC_AT)), proud = 0.5)

# The power rocker in its black bezel, the bezel's face SW_SINK inside the
# panel's. The rocker's face is two flat halves meeting at its middle in a
# gentle hollow, ROCK_ANGLE degrees between them. It is switched off:
# rocked ROCK_TURN degrees so the O half lies level and set in, and the |
# half rises from it to the top, which stands out.
SW_SINK      = 2.0
SW_FACE      = PANEL_Y - SW_SINK
SW_X0, SW_Z0 = BX(SWITCH[0]), BZ(SWITCH[3])
bezel        = (cq.Workplane("XY").box(BX(SWITCH[2]) - SW_X0, 6.0, BZ(SWITCH[1]) - SW_Z0, centered=(False, False, False))
                .edges("|Y").fillet(0.8).translate((SW_X0, SW_FACE - 6.0, SW_Z0)))
bezel        = bezel.cut(pbox(1313, 663, 1384, 734, SW_FACE - 5.0, 6.0))
back_add("switch_bezel", bezel, BACK_DARK)

ROCK_ANGLE = 150.0
ROCK_TURN  = (180.0 - ROCK_ANGLE) * 0.5
ROCK_HALF  = (BZ(665) - BZ(730)) * 0.5
ROCK_MID   = (BZ(665) + BZ(730)) * 0.5
ROCK_CREST = ROCK_HALF * math.tan(math.radians(ROCK_TURN))
ROCK_Y     = SW_FACE + 0.3 - ROCK_HALF * math.sin(math.radians(ROCK_TURN))    # the pivot
rocker     = (cq.Workplane("YZ")
              .polyline([(ROCK_Y - 6.0, ROCK_MID - ROCK_HALF), (ROCK_Y, ROCK_MID - ROCK_HALF),
                         (ROCK_Y - ROCK_CREST, ROCK_MID), (ROCK_Y, ROCK_MID + ROCK_HALF),
                         (ROCK_Y - 6.0, ROCK_MID + ROCK_HALF)]).close()
              .extrude(BX(1382) - BX(1315)).translate((BX(1315), 0, 0)))
rocker     = (rocker.edges("|X").filter(lambda e: abs(e.Center().y - (ROCK_Y - ROCK_CREST)) < 0.01).fillet(4.0)
              .rotate(cq.Vector(0, ROCK_Y, ROCK_MID), cq.Vector(1, ROCK_Y, ROCK_MID), -ROCK_TURN))
back_add("switch_rocker", rocker, ROCKER)

# ---------------------------------------------------------------- the bottom
#
# From the underside photograph (hard-disk-20sc-03.jpg): grooves front to
# back, the product label in the middle, the "Made in USA" sticker toward the
# front left, and a row of vent slots near the front edge.

for i in range(16):
    x = W * 0.08 + i * (W * 0.84) / 15 - 0.7
    bottom = bottom.cut(cq.Workplane("XY").box(1.4, D * 0.55, 1.6, centered=(False, False, False))
                        .translate((x, D * 0.36, -0.8)))

m.add("case_lid",    lid,    CASE, angular=CORNER_ANG)
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
