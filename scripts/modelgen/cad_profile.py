"""Apple ProFile (A9M0005) as a solid model, all six sides: the wide, low 5 MB
and 10 MB hard disk of the Apple III and Lisa, on the Apple II ProFile
interface card. X right, Y back, Z up; front face at y = 0.

THE DIMENSIONS are Apple's: 17.28 in wide, 4.39 in high, 8.81 in deep
(apple3.org technote 1128, and the Computer History Museum's catalog entry
102681317 at 17 1/4 x 4 1/2 x 9 in).

MODELED FROM PHOTOGRAPHS, for the owner's review (T108):

  - Front, top and back: the Powerhouse Museum, Sydney, object 457105
    (collection.powerhouse.com.au/object/457105), images 501435, 501368 and
    501362.
  - A ProFile on a Lisa: commons.wikimedia.org/wiki/File:Apple_Lisa.jpg
    (CC BY-SA 3.0).

What the photographs settle:

  EDGES. The front's top edge is rounded over. Every other edge -- the
  front's bottom, the four vertical corners, the top and bottom of each
  side, and the back's top and bottom -- is a 45-degree chamfer with only a
  slight softening.

  FRONT. Plain. At the lower left, two brownish plates a little apart: a
  square one with the rainbow Apple and a long one with "proFile" in lighter
  embossed letters. At the lower right, a small plate with "READY" over a
  red lamp.

  TOP. A large shallow recessed panel inside a border.

  BACK. Two recessed brown metal panels, each with a row of vent slots in
  the case above it and four screws. The left panel has vertical vents, the
  "proFile" name and Apple logo, and the model and serial label. The right
  panel has vents, a DB-25 socket, the voltage label, the yellow Caution
  label, an IEC power inlet and the ON/OFF rocker switch. The case's seam
  runs round the back and sides a little over a quarter of the way up.

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

from cad_badge import badge

INCH        = 25.4
W, D, H     = 17.28 * INCH, 8.81 * INCH, 4.39 * INCH
CORNER_ANG  = 0.14
FRONT_R     = H * 0.14
CHAMFER     = 5.0

CASE        = (0.880, 0.850, 0.760)       # Apple III and Lisa beige
PLATE       = (0.420, 0.385, 0.340)
EMBOSS      = (0.700, 0.680, 0.640)
PANEL       = (0.330, 0.290, 0.250)
DARK        = (0.060, 0.060, 0.060)
LABEL       = (0.950, 0.945, 0.925)
INK         = (0.120, 0.120, 0.130)
YELLOW      = (0.950, 0.780, 0.150)
BRASS       = (0.760, 0.660, 0.400)
METAL       = (0.650, 0.650, 0.650)
RUBBER      = (0.120, 0.110, 0.100)

SEAM_Z      = H * 0.27

m = Model()


def text(s, size, x, z, rgb, name, y = -0.5, font = "Arial", face = "front"):
    # Lettering on the front (facing -Y) or the back (facing +Y).
    wp = cq.Workplane("XZ").text(s, size, 0.25, font=font, halign="left", valign="center")

    if face == "back":
        wp = wp.mirror("YZ")

    m.add(name, wp.translate((x, y, z)), rgb)


# ------------------------------------------------------------------ the case

case = (cq.Workplane("XY")
        .box(W, D, H, centered=(False, False, False))
        .edges("|Z").chamfer(CHAMFER)
        .edges("|Y").chamfer(CHAMFER)
        .edges("|X").edges(">Y").chamfer(CHAMFER)
        .edges("|X").edges("<Y").edges("<Z").chamfer(CHAMFER)
        .edges("|X").edges("<Y").edges(">Z").fillet(FRONT_R))

# The seam round the sides and back.
case = case.cut(cq.Workplane("XY").box(W + 2.0, 1.2, 0.8, centered=(False, False, False))
                .translate((-1.0, D - 0.6, SEAM_Z)))

for x in (-0.6, W - 0.6):
    case = case.cut(cq.Workplane("XY").box(1.2, D - 10.0, 0.8, centered=(False, False, False))
                    .translate((x, 6.0, SEAM_Z)))

# The top's recessed panel.
case = case.cut(cq.Workplane("XY").box(W * 0.86, D * 0.72, 1.6, centered=(False, False, False))
                .edges("|Z").fillet(4.0)
                .translate((W * 0.07, D * 0.17, H - 1.2)))

# Back: the two recessed panels, and the vent rows above them. PANELS are
# as seen from behind, left to right, as in the photograph; MODEL_PANELS
# are the same in the model's x, which runs the other way seen from behind.
PANELS       = ((W * 0.037, W * 0.40), (W * 0.46, W * 0.955))
MODEL_PANELS = tuple((W - x1, W - x0) for x0, x1 in PANELS)
PANEL_Z0, PANEL_Z1 = H * 0.15, H * 0.73

for x0, x1 in MODEL_PANELS:
    case = case.cut(cq.Workplane("XY").box(x1 - x0, 6.0, PANEL_Z1 - PANEL_Z0, centered=(False, False, False))
                    .translate((x0, D - 3.0, PANEL_Z0)))

    n = int((x1 - x0 - 30.0) / 6.0)

    for i in range(n):
        case = case.cut(cq.Workplane("XY").box(2.6, 6.0, H * 0.06, centered=(False, False, False))
                        .translate((x0 + 15.0 + i * 6.0, D - 3.0, H * 0.78)))

# Sides: vents low at the back.
for x in (-1.0, W - 2.0):
    for i in range(8):
        case = case.cut(cq.Workplane("XY").box(3.0, 2.4, H * 0.12, centered=(False, False, False))
                        .translate((x, D * 0.80 + i * 4.5, H * 0.08)))

m.add("case", case, CASE, angular=CORNER_ANG)

# The vent openings show dark behind them.
for x0, x1 in MODEL_PANELS:
    m.add(f"vent_shadow_{x0:.0f}",
          cq.Workplane("XY").box(x1 - x0 - 20.0, 0.5, H * 0.06, centered=(False, False, False))
            .translate((x0 + 10.0, D - 3.4, H * 0.78)),
          DARK)

# ---------------------------------------------------------------- the front

PLATE_X0, PLATE_X1 = W * 0.10, W * 0.24
PLATE_Z0, PLATE_Z1 = H * 0.18, H * 0.36
plate_h            = PLATE_Z1 - PLATE_Z0
logo_w             = plate_h
gap                = 3.5

m.add("logo_plate",
      cq.Workplane("XY").box(logo_w, 0.6, plate_h, centered=(False, False, False))
        .translate((PLATE_X0, -0.4, PLATE_Z0)),
      PLATE)
m.add("name_plate",
      cq.Workplane("XY").box(PLATE_X1 - PLATE_X0 - logo_w - gap, 0.6, plate_h, centered=(False, False, False))
        .translate((PLATE_X0 + logo_w + gap, -0.4, PLATE_Z0)),
      PLATE)

badge(m, PLATE_X0 + logo_w * 0.18, PLATE_Z0 + plate_h * 0.16, plate_h * 0.68, y = -0.7)
text("proFile", plate_h * 0.55, PLATE_X0 + logo_w + gap + 3.0, PLATE_Z0 + plate_h * 0.5, EMBOSS, "name_lettering", y = -0.6)

READY_X0, READY_X1 = W * 0.875, W * 0.915
ready_w            = READY_X1 - READY_X0

m.add("ready_plate",
      cq.Workplane("XY").box(ready_w, 0.6, plate_h, centered=(False, False, False))
        .translate((READY_X0, -0.4, PLATE_Z0)),
      PLATE)
text("READY", plate_h * 0.22, READY_X0 + ready_w * 0.12, PLATE_Z0 + plate_h * 0.72, LABEL, "ready_lettering", y = -0.6)
m.add("lamp",
      cq.Workplane("XY").box(ready_w * 0.40, 0.5, plate_h * 0.14, centered=(False, False, False))
        .translate((READY_X0 + ready_w * 0.30, -0.7, PLATE_Z0 + plate_h * 0.25)),
      KD["drive_lamp"])

# ----------------------------------------------------------------- the back
#
# Laid out as seen from behind, as in the photograph, and mirrored across
# the centerline into the model's x. Lettering is made unmirrored so it reads
# correctly from behind after the mirror.


def back_add(name, solid, rgb):
    m.add(name, solid.mirror("YZ", (W * 0.5, 0, 0)), rgb)


def back_text(s, size, x, z, rgb, name, y):
    wp = cq.Workplane("XZ").text(s, size, 0.25, font="Arial", halign="left", valign="center")

    back_add(name, wp.translate((x, y, z)), rgb)


BACK_Y = D - 2.6          # the panels, recessed into the case
FACE   = BACK_Y + 0.6     # their outer face, where the details sit

for x0, x1 in PANELS:
    back_add(f"back_panel_{x0:.0f}",
          cq.Workplane("XY").box(x1 - x0, 0.6, PANEL_Z1 - PANEL_Z0, centered=(False, False, False))
            .translate((x0, BACK_Y, PANEL_Z0)),
          PANEL)

    for sx in (x0 + 5.0, x1 - 5.0):
        back_add(f"screw_{sx:.0f}",
              cq.Workplane("XZ").circle(2.2).extrude(-0.8).translate((sx, FACE, PANEL_Z1 - 5.0)),
              METAL)


def vents(x0, x1, name):
    n = int((x1 - x0) / 7.0)

    for i in range(n):
        back_add(f"{name}{i}",
              cq.Workplane("XY").box(3.2, 0.5, H * 0.20, centered=(False, False, False))
                .translate((x0 + i * 7.0, FACE, H * 0.48)),
              DARK)


vents(W * 0.07, W * 0.37, "left_vent")
vents(W * 0.52, W * 0.755, "right_vent")

# Left panel: the name, the Apple, and the model and serial label.
back_text("proFile", H * 0.06, W * 0.11, H * 0.43, LABEL, "back_name", FACE + 0.3)
back_add("back_apple",
      cq.Workplane("XZ").circle(H * 0.025).extrude(-0.4).translate((W * 0.31, FACE, H * 0.43)),
      LABEL)
back_add("model_label",
      cq.Workplane("XY").box(W * 0.24, 0.4, H * 0.17, centered=(False, False, False))
        .translate((W * 0.10, FACE, H * 0.22)),
      LABEL)
back_text("A9M0005", H * 0.045, W * 0.12, H * 0.34, INK, "model_text", FACE + 0.7)
back_text("Apple Computer  Cupertino, California", H * 0.03, W * 0.12, H * 0.27, INK, "maker_text", FACE + 0.7)

# Right panel: the DB-25 socket, the labels, the inlet and the switch.
db_x0, db_x1 = W * 0.775, W * 0.885
back_add("db25_shell",
      cq.Workplane("XY").box(db_x1 - db_x0, 1.5, H * 0.11, centered=(False, False, False))
        .edges("|Y").fillet(1.5)
        .translate((db_x0, FACE, H * 0.57)),
      BRASS)
back_add("db25_insert",
      cq.Workplane("XY").box((db_x1 - db_x0) * 0.86, 0.5, H * 0.07, centered=(False, False, False))
        .translate((db_x0 + (db_x1 - db_x0) * 0.07, FACE + 1.5, H * 0.59)),
      DARK)
back_add("voltage_label",
      cq.Workplane("XY").box(W * 0.10, 0.4, H * 0.11, centered=(False, False, False))
        .translate((W * 0.775, FACE, H * 0.42)),
      LABEL)
back_text("MADE IN U.S.A.", H * 0.025, W * 0.785, H * 0.50, INK, "voltage_text", FACE + 0.7)
back_add("caution_label",
      cq.Workplane("XY").box(W * 0.235, 0.4, H * 0.26, centered=(False, False, False))
        .translate((W * 0.51, FACE, H * 0.22)),
      LABEL)
back_add("caution_band",
      cq.Workplane("XY").box(W * 0.235, 0.5, H * 0.05, centered=(False, False, False))
        .translate((W * 0.51, FACE + 0.4, H * 0.43)),
      PANEL)
back_text("Caution", H * 0.04, W * 0.52, H * 0.455, YELLOW, "caution_text", FACE + 0.8)
back_add("caution_triangle",
      cq.Workplane("XZ").polyline([(0, 0), (H * 0.10, 0), (H * 0.05, H * 0.09)]).close().extrude(-0.3)
        .translate((W * 0.53, FACE + 0.4, H * 0.27)),
      YELLOW)
back_add("iec_inlet",
      cq.Workplane("XY").box(W * 0.085, 2.0, H * 0.17, centered=(False, False, False))
        .edges("|Y").chamfer(2.0)
        .translate((W * 0.75, FACE, H * 0.21)),
      DARK)
back_add("switch_frame",
      cq.Workplane("XY").box(W * 0.035, 2.0, H * 0.26, centered=(False, False, False))
        .translate((W * 0.89, FACE, H * 0.18)),
      DARK)
back_add("switch_rocker",
      cq.Workplane("XY").box(W * 0.025, 1.0, H * 0.10, centered=(False, False, False))
        .translate((W * 0.895, FACE + 2.0, H * 0.30)),
      (0.20, 0.20, 0.20))
back_text("ON", H * 0.03, W * 0.897, H * 0.40, LABEL, "switch_on", FACE + 2.3)
back_text("OFF", H * 0.03, W * 0.894, H * 0.22, LABEL, "switch_off", FACE + 2.3)

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
