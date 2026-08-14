"""Apple //c external 5.25 Drive (A2M4020), parametric: the low platinum
unit styled to match the //c, ~152 x 70 x 216 mm. Unlike the Disk II
there is no black faceplate -- the front is platinum with a recessed
panel, the slot sits upper-middle, a wide latch bar hangs BELOW the slot,
a green activity lamp sits lower-left, and the rainbow brand lower-right.
X right, Y back, Z up; front at y=0."""

from meshkit import Mesh

W, H, D = 152.0, 70.0, 216.0

m = Mesh()

plat     = m.color("plat",     (0.870, 0.862, 0.835))
plat_dk  = m.color("plat_dk",  (0.790, 0.782, 0.755))
panel_c  = m.color("panel",    (0.830, 0.822, 0.795))
slot_dk  = m.color("slot_dk",  (0.060, 0.060, 0.070))
# Identity colors, not free choices: the desk scene finds the door/latch
# assembly and the activity lamp by Kd (DeskSceneModel's kDriveDoorAltKd /
# kDriveLatchAltKd / kDriveLampAltKd -- the platinum-era set). The lamp green
# is deliberately OFF the monitor lamp's green, which it would otherwise be
# identified as, putting the drive's lamp in the monitor's sub-mesh.
latch_c  = m.color("latch",    (0.720, 0.712, 0.685))
tab_c    = m.color("tab",      (0.640, 0.632, 0.605))
lamp_c   = m.color("lamp",     (0.250, 0.845, 0.330))
foot_c   = m.color("foot",     (0.320, 0.310, 0.300))

rainbow = [m.color(f"rb{i}", c) for i, c in enumerate([
    (0.20, 0.65, 0.27), (0.98, 0.80, 0.08), (0.96, 0.51, 0.12),
    (0.91, 0.18, 0.14), (0.58, 0.25, 0.60), (0.17, 0.45, 0.85),
])]

# Case body: platinum, slight top chamfer via a thin cap.
m.box(0, 0, 0, W, D, H, plat)

# Front lip + recessed platinum panel (the //c drive keeps its case color
# on the face; only the slot is dark).
LIP = 6.0
m.box(0,       -2.5, 0,       W,   0, LIP, plat)
m.box(0,       -2.5, H - LIP, W,   0, H,   plat)
m.box(0,       -2.5, 0,       LIP, 0, H,   plat)
m.box(W - LIP, -2.5, 0,       W,   0, H,   plat)
m.box(LIP, -0.8, LIP, W - LIP, 0.4, H - LIP, panel_c)

# Slot: upper-middle.
SLOT_Z0, SLOT_Z1 = 42.0, 47.0
m.box(13.0, -1.4, SLOT_Z0, W - 13.0, -0.8, SLOT_Z1, slot_dk)

# Latch bar BELOW the slot: the //c drive's wide flip lever, with a slightly
# darker grip tab centered on it. Both carry platinum-era door identities, so
# the scene finds this assembly and swings it on eject the way it does the
# Disk II's black door.
m.box(W / 2 - 26.0, -3.4, SLOT_Z0 - 12.0, W / 2 + 26.0, -0.8, SLOT_Z0 - 2.0, latch_c)
m.box(W / 2 - 11.0, -4.0, SLOT_Z0 - 10.5, W / 2 + 11.0, -3.4, SLOT_Z0 - 3.5, tab_c)

# Activity lamp, lower-left (green, //c family style).
m.cylinder(22.0, -2.2, 14.0, 2.8, 1.8, lamp_c, axis="y", segments=12)

# Rainbow brand, lower-right: six small stripes.
RB_X0, RB_X1, RB_Z1, STRIPE = 126.0, 140.0, 26.0, 2.4
for i, c in enumerate(rainbow):
    z1 = RB_Z1 - i * STRIPE
    m.box(RB_X0, -1.4, z1 - STRIPE, RB_X1, -0.8, z1, c)

# //c-style side ribs: shallow horizontal grooves along both flanks.
for side_x0, side_x1 in [(-0.3, 0.1), (W - 0.1, W + 0.3)]:
    for i in range(4):
        z0 = 18.0 + i * 12.0
        m.box(side_x0, 26.0, z0, side_x1, D - 20.0, z0 + 4.0, plat_dk)

# Lid recess strip echoing the //c handle groove.
m.box(20.0, 20.0, H - 0.01, W - 20.0, D - 26.0, H + 0.3, plat_dk)

# Feet.
for fx in (16.0, W - 16.0):
    for fy in (18.0, D - 18.0):
        m.cylinder(fx, fy, -2.0, 5.5, 2.0, foot_c, axis="z", segments=12)

if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    m.emit(os.path.join(out, "Disk2c.obj"), os.path.join(out, "Disk2c.mtl"), "Disk2c.mtl")
    print(f"Disk2c: {len(m.verts)} verts, {len(m.tris)} tris")
