"""Apple DuoDisk (A9M0106), parametric: the //e-width dual 5.25 unit,
~386 x 89 x 222 mm. One beige case, one full-width recessed black
faceplate carrying two side-by-side drive bays -- each with its own slot,
door bar, latch, and IN-USE LED -- with a center divider and a badge
plaque centered above the bays. X right, Y back, Z up; front at y=0."""

from meshkit import Mesh

W, H, D = 386.0, 89.0, 222.0

m = Mesh()

beige    = m.color("beige",    (0.833, 0.784, 0.659))
beige_dk = m.color("beige_dk", (0.760, 0.710, 0.590))
plate    = m.color("plate",    (0.100, 0.100, 0.110))
slot_dk  = m.color("slot_dk",  (0.035, 0.035, 0.045))
door_c   = m.color("door",     (0.160, 0.160, 0.180))
latch_c  = m.color("latch",    (0.230, 0.230, 0.250))
led_red  = m.color("led_red",  (0.900, 0.120, 0.100))
badge_c  = m.color("badge",    (0.900, 0.870, 0.780))
foot_c   = m.color("foot",     (0.150, 0.140, 0.130))

# Case body.
m.box(0, 0, 0, W, D, H, beige)

# Front lip.
LIP = 8.0
m.box(0,       -3.0, 0,       W,   0, LIP, beige)
m.box(0,       -3.0, H - LIP, W,   0, H,   beige)
m.box(0,       -3.0, 0,       LIP, 0, H,   beige)
m.box(W - LIP, -3.0, 0,       W,   0, H,   beige)

# Center divider between the two bays.
DIV_W = 10.0
m.box(W / 2 - DIV_W / 2, -3.0, 0, W / 2 + DIV_W / 2, 0, H, beige)

# Faceplate behind everything.
m.box(LIP, -1.0, LIP, W - LIP, 0.5, H - LIP, plate)

# Two drive bays.
SLOT_Z0, SLOT_Z1 = 40.0, 46.0
for bay_x0, bay_x1 in [(LIP, W / 2 - DIV_W / 2), (W / 2 + DIV_W / 2, W - LIP)]:
    inset = 8.0
    sx0, sx1 = bay_x0 + inset, bay_x1 - inset
    cx = (bay_x0 + bay_x1) / 2
    m.box(sx0, -1.7, SLOT_Z0, sx1, -1.0, SLOT_Z1, slot_dk)
    m.box(sx0, -2.3, SLOT_Z1, sx1, -1.0, SLOT_Z1 + 9.0, door_c)
    m.box(cx - 11.0, -3.1, SLOT_Z1 + 1.5, cx + 11.0, -2.3, SLOT_Z1 + 7.5, latch_c)
    m.cylinder(bay_x0 + 16.0, -2.6, 16.0, 3.0, 2.0, led_red, axis="y", segments=12)

# Badge plaque, centered above the bays.
m.box(W / 2 - 34.0, -1.8, H - LIP - 14.0, W / 2 + 34.0, -1.0, H - LIP - 4.0, badge_c)

# Lid channel: single wide shallow recess strip.
m.box(30.0, 18.0, H - 0.01, W - 30.0, D - 24.0, H + 0.35, beige_dk)

# Side vents.
for side_x0, side_x1 in [(-0.35, 0.1), (W - 0.1, W + 0.35)]:
    for i in range(9):
        y0 = 146.0 + i * 7.0
        m.box(side_x0, y0, 20.0, side_x1, y0 + 3.2, 68.0, beige_dk)

# Feet.
for fx in (22.0, W - 22.0):
    for fy in (18.0, D - 18.0):
        m.cylinder(fx, fy, -2.2, 6.0, 2.2, foot_c, axis="z", segments=12)

if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    m.emit(os.path.join(out, "DuoDisk.mesh"), os.path.join(out, "DuoDisk.mtl"), "DuoDisk.mtl")
    print(f"DuoDisk: {len(m.verts)} verts, {len(m.tris)} tris")
