"""Apple Monitor //c, parametric. Shell ~248 x 200 x 280 mm on a low
pedestal stand; platinum case tapering toward the back. The front reads like
the real unit: a proud bezel plate with a sloped shoulder rolling back into
the case (no hard box edges facing the viewer), an inner lip beveling down
into the screen recess, rounded-corner fillets over the tube corners, curved
CRT glass (true spherical sag -- the mesh the live display texture maps
onto), a chin carrying the brand (stamped by the scene code from the shared
CassoBranding silhouette) and a //c-style pill power lamp, and vent grooves
on the lid. X right, Y back, Z up; the bezel plate front sits at y=-10.

The screen opening is genuinely open -- the shell starts BEHIND the cavity,
and the bezel sheets bridge the front. A solid front face would occlude
every part of the glass deeper than it.
"""

from meshkit import Mesh
import math

W, H, D = 248.0, 200.0, 280.0
STAND_H = 26.0
Z0 = STAND_H                     # shell bottom sits on the stand

PROUD = 10.0                     # bezel plate front plane (y = -PROUD)
RIM = 9.0                        # shoulder inset from the case outline
CHIN = 40.0                      # chin band below the opening
BZ = 20.0                        # bezel width left/right
TOPB = 22.0                      # bezel width on top
BEVEL_IN = 6.0                   # inner lip width around the opening
SHELL_FRONT = 10.0               # case body front plane (behind the shoulder)

OX0, OX1 = BZ, W - BZ            # opening rect
OZ0, OZ1 = Z0 + CHIN, Z0 + H - TOPB

m = Mesh()

plat      = m.color("plat",      (0.870, 0.862, 0.835))
plat_dk   = m.color("plat_dk",   (0.780, 0.772, 0.745))
bezel_c   = m.color("bezel",     (0.845, 0.837, 0.810))
cavity_c  = m.color("cavity",    (0.070, 0.075, 0.080))
glass_c   = m.color("glass",     (0.050, 0.090, 0.070))
lamp_c    = m.color("lamp",      (0.290, 0.870, 0.380))
foot_c    = m.color("foot",      (0.320, 0.310, 0.300))

# Shell: tapers toward the back (narrower and lower at the rear). Starts
# behind the front shoulder; the bezel sheets carry the face.
m.hexahedron(
    [(0, SHELL_FRONT, Z0), (W, SHELL_FRONT, Z0), (W - 10, D, Z0 + 6), (10, D, Z0 + 6),
     (0, SHELL_FRONT, Z0 + H), (W, SHELL_FRONT, Z0 + H), (W - 14, D - 6, Z0 + H - 12), (14, D - 6, Z0 + H - 12)],
    plat)

# Bezel plate: the flat front frame at y=-PROUD, from the shoulder inset to
# the inner-lip edge around the opening.
PX0, PX1 = RIM, W - RIM                       # plate outer rect
PZ0, PZ1 = Z0 + RIM, Z0 + H - RIM
HX0, HX1 = OX0 - BEVEL_IN, OX1 + BEVEL_IN     # plate hole rect
HZ0, HZ1 = OZ0 - BEVEL_IN, OZ1 + BEVEL_IN

YF = -PROUD

m.add_quad((PX0, YF, PZ0), (PX1, YF, PZ0), (HX1, YF, HZ0), (HX0, YF, HZ0), bezel_c)   # chin face
m.add_quad((HX0, YF, HZ1), (HX1, YF, HZ1), (PX1, YF, PZ1), (PX0, YF, PZ1), bezel_c)   # top face
m.add_quad((PX0, YF, PZ0), (HX0, YF, HZ0), (HX0, YF, HZ1), (PX0, YF, PZ1), bezel_c)   # left face
m.add_quad((HX1, YF, HZ0), (PX1, YF, PZ0), (PX1, YF, PZ1), (HX1, YF, HZ1), bezel_c)   # right face

# Shoulder: sloped sheets rolling from the plate's outer edge back and
# outward to the case outline -- the soft rounded-front read.
YB = SHELL_FRONT

m.add_quad((0, YB, Z0), (W, YB, Z0), (PX1, YF, PZ0), (PX0, YF, PZ0), plat)             # bottom
m.add_quad((PX0, YF, PZ1), (PX1, YF, PZ1), (W, YB, Z0 + H), (0, YB, Z0 + H), plat)     # top
m.add_quad((0, YB, Z0), (PX0, YF, PZ0), (PX0, YF, PZ1), (0, YB, Z0 + H), plat)         # left
m.add_quad((PX1, YF, PZ0), (W, YB, Z0), (W, YB, Z0 + H), (PX1, YF, PZ1), plat)         # right

# Inner lip: sloped sheets from the plate hole down into the recess, meeting
# the opening at the cavity front.
YC = 6.0

m.add_quad((HX0, YF, HZ0), (HX1, YF, HZ0), (OX1, YC, OZ0), (OX0, YC, OZ0), bezel_c)    # below opening
m.add_quad((OX0, YC, OZ1), (OX1, YC, OZ1), (HX1, YF, HZ1), (HX0, YF, HZ1), bezel_c)    # above opening
m.add_quad((HX0, YF, HZ0), (OX0, YC, OZ0), (OX0, YC, OZ1), (HX0, YF, HZ1), bezel_c)    # left of opening
m.add_quad((OX1, YC, OZ0), (HX1, YF, HZ0), (HX1, YF, HZ1), (OX1, YC, OZ1), bezel_c)    # right of opening

# Screen cavity: dark recess behind the opening.
m.box(OX0, 6.0, OZ0, OX1, 8.0, OZ1, cavity_c)

# CRT glass: rectangular panel with true spherical sag, bulging toward the
# viewer out of the recess. Sphere radius ~2x the diagonal reads as the
# period tube's gentle curvature.
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

# Tube corner fillets: near-black patches in front of the glass corners so
# the picture's corners round off the way a real tube's do. Two triangles
# approximate each quarter arc.
FILLET = 14.0
YFIL = 4.5
K = 1.0 - math.sqrt(0.5)         # arc midpoint pullback fraction

for cx, cz, sx, sz in [(gx0, gz1, 1.0, -1.0), (gx1, gz1, -1.0, -1.0),
                       (gx0, gz0, 1.0, 1.0),  (gx1, gz0, -1.0, 1.0)]:
    corner = (cx, YFIL, cz)
    edge_x = (cx + sx * FILLET, YFIL, cz)
    edge_z = (cx, YFIL, cz + sz * FILLET)
    mid    = (cx + sx * FILLET * K, YFIL, cz + sz * FILLET * K)
    m.add_tri(corner, edge_x, mid, cavity_c)
    m.add_tri(corner, mid, edge_z, cavity_c)

# Power lamp: the //c-style horizontal pill, chin right.
m.box(W - 48.0, YF - 0.6, Z0 + 15.0, W - 32.0, YF, Z0 + 20.0, lamp_c)

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
