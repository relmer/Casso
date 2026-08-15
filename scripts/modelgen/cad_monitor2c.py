"""Apple Monitor //c (G090H) as a real solid model. Shell 248 x 280 x 200 mm
(W x D x H), X right, Y back, Z up; the bezel plate front sits at y=-10.

A faithful port of the hand-built generator's coordinates (see cad_diskii.py
for why construction moved to a CAD kernel). The //c monitor's read is the
soft front: a proud bezel plate whose sloped shoulder rolls back into the
case with no hard box edges facing the viewer. That shoulder is a LOFT here
-- plate rectangle at y=-10, case outline at y=+10, tapered back at y=D, one
ruled surface -- and the screen recess is a lofted CUT from the plate hole
down to the opening, so the inner lip's slope is the cut's own wall and the
opening is genuinely open by construction.

The scene stamps the brand onto the chin at fixed model coordinates and
derives the display sphere from the glass mesh, so the plate front plane,
chin band, opening rect and sag radius all keep the original values. The
power lamp is the //c-family rhombus: the switch bar's own proportions
(8 : 25) and ~10 degree lean, extruded, in a hairline recess.
"""

import cadquery as cq
from cadkit import KD, Model, rect_wire, sag_sheet

# ---------------------------------------------------------------- dimensions

W, H, D = 248.0, 200.0, 280.0

PROUD       = 10.0                # bezel plate front plane (y = -PROUD)
RIM         = 9.0                 # shoulder inset from the case outline
CHIN        = 34.0                # chin band below the opening
BZ          = 16.0                # bezel width left/right
TOPB        = 14.0                # bezel width on top
BEVEL_IN    = 5.0                 # inner lip width around the opening
SHELL_FRONT = 10.0                # case body front plane (behind the shoulder)

OX0, OX1 = BZ, W - BZ             # opening rect
OZ0, OZ1 = CHIN, H - TOPB

PX0, PX1 = RIM, W - RIM           # plate outer rect
PZ0, PZ1 = RIM, H - RIM
HX0, HX1 = OX0 - BEVEL_IN, OX1 + BEVEL_IN     # plate hole rect
HZ0, HZ1 = OZ0 - BEVEL_IN, OZ1 + BEVEL_IN

GLASS_INSET = 3.0
BASE_Y      = 6.0                 # glass sheet front plane

PLAT     = (0.870, 0.862, 0.835)
PLAT_DK  = (0.780, 0.772, 0.745)
CAVITY   = (0.055, 0.055, 0.062)
LAMPRING = (0.045, 0.045, 0.050)

m = Model()


# -------------------------------------------------------------------- shell

# One lofted solid: the proud plate rolls back to the case outline (the
# shoulder), the case tapers toward the rear (narrower and lower), all as
# ruled surfaces between three sections.
shell = cq.Solid.makeLoft(
    [rect_wire(-PROUD, PX0, PX1, PZ0, PZ1),
     rect_wire(SHELL_FRONT, 0.0, W, 0.0, H),
     rect_wire(D, 12.0, W - 12.0, 5.0, H - 12.0)],
    True)

# The screen recess, cut with its own loft: plate hole at the front sloping
# in to the opening at the cavity plane, then straight back. The inner lip
# IS this cut's wall.
recess = cq.Solid.makeLoft(
    [rect_wire(-PROUD - 0.5, HX0, HX1, HZ0, HZ1),
     rect_wire(BASE_Y, OX0, OX1, OZ0, OZ1)],
    True)

deeper = (cq.Workplane("XY")
          .box(OX1 - OX0, 40.0, OZ1 - OZ0, centered=(False, False, False))
          .translate((OX0, BASE_Y - 0.5, OZ0)))

shell = cq.Workplane(obj=shell).cut(cq.Workplane(obj=recess)).cut(deeper)

m.add("shell", shell, PLAT)

# Cavity back: the dark plane behind the glass.
m.add("cavity",
      cq.Workplane("XY").box(OX1 - OX0, 2.0, OZ1 - OZ0, centered=(False, False, False))
        .translate((OX0, BASE_Y, OZ0)),
      CAVITY)

# --------------------------------------------------------------------- glass

# True spherical sag, radius 3x the half diagonal -- the period 9-inch tube's
# gentle curvature, and the sphere the scene derives its input mapping from.
m.add_triangles("glass",
                sag_sheet(OX0 + GLASS_INSET, OX1 - GLASS_INSET,
                          OZ0 + GLASS_INSET, OZ1 - GLASS_INSET,
                          front_y=BASE_Y, radius_scale=3.0),
                KD["glass"])

# --------------------------------------------------------------- power lamp

# The //c-family indicator: a tall narrow rhombus leaning right at the switch
# bar's proportions (kLedWDp 8 : kLedHDp 25) and lean (tan 0.176), extruded,
# seated in a hairline recess. Analytic triangles rather than a CAD solid --
# a sheared prism fights every workplane, and ten quads state it exactly.

def slanted_prism(x, z0, height, width, lean, y_front, y_back):
    z1 = z0 + height
    face = [(x, z0), (x + width, z0), (x + width + lean, z1), (x + lean, z1)]
    tris = []
    f3 = [(px, y_front, pz) for px, pz in face]
    tris.append((f3[0], f3[1], f3[2]))
    tris.append((f3[0], f3[2], f3[3]))
    for i in range(4):
        ax, az = face[i]
        bx, bz = face[(i + 1) % 4]
        tris.append(((ax, y_front, az), (bx, y_front, bz), (bx, y_back, bz)))
        tris.append(((ax, y_front, az), (bx, y_back, bz), (ax, y_back, az)))
    return tris


LCX, LCZ = W - 40.0, 11.0
LENS_W, LENS_H = 4.2, 13.125
LEAN = LENS_H * 0.176
RECESS_RIM = 0.4

m.add_triangles("lampring",
                slanted_prism(LCX - RECESS_RIM, LCZ - RECESS_RIM,
                              LENS_H + RECESS_RIM * 2.0, LENS_W + RECESS_RIM * 2.0,
                              LEAN, -PROUD - 0.40, -PROUD),
                LAMPRING)

m.add_triangles("lamp",
                slanted_prism(LCX, LCZ, LENS_H, LENS_W, LEAN,
                              -PROUD - 1.30, -PROUD),
                KD["monitor_lamp"])

# ----------------------------------------------------------------- lid vents

# Darker strips across the lid, rear two-thirds. The lid slopes with the
# taper, so each strip rides at the lofted surface's local height.
for i in range(10):
    y0 = 96.0 + i * 16.0
    t  = max(0.0, (y0 - SHELL_FRONT) / (D - SHELL_FRONT))
    zt = H - 12.0 * t                 # lid height at this depth (H at front, H-12 at rear)
    m.add(f"vent{i}",
          cq.Workplane("XY").box(W - 52.0, 5.0, 0.5, centered=(False, False, False))
            .translate((26.0, y0, zt - 12.0)),
          PLAT_DK)


# --------------------------------------------------------------- front anchor
#
# The FRAME's front plane -- the shell's own front section, a centimeter
# BEHIND the proud plate that carries the screen. The drives line up with
# this, not with the plate and not with the model origin.
m.add("front_anchor",
      cq.Workplane("XY")
        .box(0.4, 0.4, 0.4, centered=(True, True, True))
        .translate((W * 0.5, SHELL_FRONT, H * 0.25)),
      KD["front_anchor"])


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Monitor2c.mesh"),
                    os.path.join(out, "Monitor2c.mtl"), "Monitor2c.mtl")
    print(f"Monitor2c (CAD): {nv} verts, {nt} tris")
