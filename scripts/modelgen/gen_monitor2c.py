"""Apple Monitor //c, parametric. Shell ~248 x 200 x 280 mm on a low
pedestal stand; platinum case tapering toward the back, chunky even bezel
with a recessed opening, curved CRT glass (true spherical sag -- the mesh
the live display texture will map onto), chin with the rainbow brand and
a power lamp, vent grooves on the lid. X right, Y back, Z up; bezel front
at y=0."""

from meshkit import Mesh
import math

W, H, D = 248.0, 200.0, 280.0
STAND_H = 26.0
Z0 = STAND_H                     # shell bottom sits on the stand

m = Mesh()

plat      = m.color("plat",      (0.870, 0.862, 0.835))
plat_dk   = m.color("plat_dk",   (0.780, 0.772, 0.745))
bezel_c   = m.color("bezel",     (0.845, 0.837, 0.810))
cavity_c  = m.color("cavity",    (0.070, 0.075, 0.080))
glass_c   = m.color("glass",     (0.050, 0.090, 0.070))
lamp_c    = m.color("lamp",      (0.290, 0.870, 0.380))
foot_c    = m.color("foot",      (0.320, 0.310, 0.300))

rainbow = [m.color(f"rb{i}", c) for i, c in enumerate([
    (0.20, 0.65, 0.27), (0.98, 0.80, 0.08), (0.96, 0.51, 0.12),
    (0.91, 0.18, 0.14), (0.58, 0.25, 0.60), (0.17, 0.45, 0.85),
])]

# Shell: tapers toward the back (narrower and lower at the rear). The body
# starts BEHIND the screen cavity (y=8) so the front is genuinely open --
# a solid front face would occlude every part of the glass deeper than it.
SHELL_FRONT = 8.0
m.hexahedron(
    [(0, SHELL_FRONT, Z0), (W, SHELL_FRONT, Z0), (W - 10, D, Z0 + 6), (10, D, Z0 + 6),
     (0, SHELL_FRONT, Z0 + H), (W, SHELL_FRONT, Z0 + H), (W - 14, D - 6, Z0 + H - 12), (14, D - 6, Z0 + H - 12)],
    plat)

# Bezel: even chunky lip standing proud around the screen opening.
CHIN = 46.0                       # chin band below the opening
BZ   = 24.0                       # bezel width on left/right/top
OX0, OX1 = BZ, W - BZ             # opening rect
OZ0, OZ1 = Z0 + CHIN, Z0 + H - BZ

m.box(0,       -4.0, Z0,        W,   0, Z0 + CHIN, bezel_c)      # chin band
m.box(0,       -4.0, OZ1,       W,   0, Z0 + H,    bezel_c)      # top band
m.box(0,       -4.0, Z0,        BZ,  0, Z0 + H,    bezel_c)      # left band
m.box(W - BZ,  -4.0, Z0,        W,   0, Z0 + H,    bezel_c)      # right band

# Front wall between the bezel and the recessed shell front: four slabs
# around the opening, so the case is solid everywhere EXCEPT the screen.
m.box(0,       0.0, Z0,        W,   SHELL_FRONT, Z0 + CHIN, plat)   # below opening
m.box(0,       0.0, OZ1,       W,   SHELL_FRONT, Z0 + H,    plat)   # above opening
m.box(0,       0.0, Z0,        OX0, SHELL_FRONT, Z0 + H,    plat)   # left of opening
m.box(OX1,     0.0, Z0,        W,   SHELL_FRONT, Z0 + H,    plat)   # right of opening

# Screen cavity: dark recess behind the opening.
m.box(OX0, 6.0, OZ0, OX1, 8.0, OZ1, cavity_c)

# CRT glass: rectangular panel with true spherical sag, bulging toward
# the viewer out of the recess. Sphere radius ~2x the diagonal reads as
# the period tube's gentle curvature.
GLASS_INSET = 5.0
gx0, gx1 = OX0 + GLASS_INSET, OX1 - GLASS_INSET
gz0, gz1 = OZ0 + GLASS_INSET, OZ1 - GLASS_INSET
gcx, gcz = (gx0 + gx1) / 2, (gz0 + gz1) / 2
half_diag = math.hypot((gx1 - gx0) / 2, (gz1 - gz0) / 2)
SPHERE_R = half_diag * 2.2
BASE_Y = 6.0

COLS, ROWS = 16, 12
for r in range(ROWS):
    for c in range(COLS):
        def pt(ci, ri):
            x = gx0 + (gx1 - gx0) * ci / COLS
            z = gz0 + (gz1 - gz0) * ri / ROWS
            rr = math.hypot(x - gcx, z - gcz)
            sag = SPHERE_R - math.sqrt(max(SPHERE_R * SPHERE_R - rr * rr, 0.0))
            max_sag = SPHERE_R - math.sqrt(max(SPHERE_R * SPHERE_R - half_diag * half_diag, 0.0))
            return (x, BASE_Y - (max_sag - sag), z)
        m.add_quad(pt(c, r), pt(c + 1, r), pt(c + 1, r + 1), pt(c, r + 1), glass_c)

# Chin details: rainbow brand left, power lamp right.
RB_X0, STRIPE_W = 22.0, 4.0
for i, col in enumerate(rainbow):
    x0 = RB_X0 + i * STRIPE_W
    m.box(x0, -4.6, Z0 + 16.0, x0 + STRIPE_W, -4.0, Z0 + 30.0, col)

m.cylinder(W - 34.0, -4.8, Z0 + 21.0, 3.4, 2.0, lamp_c, axis="y", segments=12)

# Lid vent grooves: darker strips across the top, rear two-thirds.
for i in range(10):
    y0 = 96.0 + i * 16.0
    m.box(26.0, y0, Z0 + H - 12.0 - 0.01, W - 26.0, y0 + 5.0, Z0 + H - 12.0 + 0.5, plat_dk)

# Pedestal stand: a narrower tapering base.
m.hexahedron(
    [(34, 30, 0), (W - 34, 30, 0), (W - 44, D - 50, 0), (44, D - 50, 0),
     (28, 22, STAND_H + 0.5), (W - 28, 22, STAND_H + 0.5),
     (W - 38, D - 44, STAND_H + 0.5), (38, D - 44, STAND_H + 0.5)],
    plat_dk)

# Stand feet.
for fx in (52.0, W - 52.0):
    for fy in (44.0, D - 66.0):
        m.cylinder(fx, fy, -2.0, 7.0, 2.0, foot_c, axis="z", segments=12)

if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    m.emit(os.path.join(out, "Monitor2c.obj"), os.path.join(out, "Monitor2c.mtl"), "Monitor2c.mtl")
    print(f"Monitor2c: {len(m.verts)} verts, {len(m.tris)} tris")
