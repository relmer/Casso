"""Apple Disk II drive, parametric. Real unit is ~155 x 86 x 222 mm
(W x H x D). Front: beige lip around a recessed black faceplate carrying
the slot, the door bar with its latch tab, a badge plaque upper-left, an
IN-USE LED lower-left, and the rainbow mark lower-right (matching the 2D
widget's design language). Top: two shallow lid channels. Sides: nine
vent slits toward the rear. X right, Y back, Z up; front face at y=0."""

from meshkit import Mesh

W, H, D = 155.0, 86.0, 222.0

m = Mesh()

beige      = m.color("beige",      (0.833, 0.784, 0.659))
beige_dk   = m.color("beige_dk",   (0.760, 0.710, 0.590))
plate      = m.color("plate",      (0.100, 0.100, 0.110))
slot_dk    = m.color("slot_dk",    (0.035, 0.035, 0.045))
door_c     = m.color("door",       (0.160, 0.160, 0.180))
latch_c    = m.color("latch",      (0.230, 0.230, 0.250))
led_red    = m.color("led_red",    (0.900, 0.120, 0.100))
badge_c    = m.color("badge",      (0.900, 0.870, 0.780))
foot_c     = m.color("foot",       (0.150, 0.140, 0.130))

rainbow = [m.color(f"rb{i}", c) for i, c in enumerate([
    (0.20, 0.65, 0.27),   # green
    (0.98, 0.80, 0.08),   # yellow
    (0.96, 0.51, 0.12),   # orange
    (0.91, 0.18, 0.14),   # red
    (0.58, 0.25, 0.60),   # purple
    (0.17, 0.45, 0.85),   # blue
])]

# Case body: straight walls; the lid stays flat (the real case's edge
# radius is far below this mesh's resolution).
m.box(0, 0, 0, W, D, H, beige)

# Front lip: four beige slabs standing 3 mm proud around the faceplate
# opening, so the black plate reads as recessed without boolean holes.
LIP = 7.0
m.box(0,       -3.0, 0,       W,   0, LIP, beige)            # bottom lip
m.box(0,       -3.0, H - LIP, W,   0, H,   beige)            # top lip
m.box(0,       -3.0, 0,       LIP, 0, H,   beige)            # left lip
m.box(W - LIP, -3.0, 0,       W,   0, H,   beige)            # right lip

# Faceplate: black plate 2 mm behind the lip front.
m.box(LIP, -1.0, LIP, W - LIP, 0.5, H - LIP, plate)

# Slot: near-black strip proud of the plate.
SLOT_Z0, SLOT_Z1 = 46.0, 52.0
m.box(14.0, -1.7, SLOT_Z0, W - 14.0, -1.0, SLOT_Z1, slot_dk)

# Door bar above the slot, latch tab centered on it.
m.box(14.0, -2.3, SLOT_Z1, W - 14.0, -1.0, SLOT_Z1 + 9.0, door_c)
m.box(W / 2 - 12.0, -3.1, SLOT_Z1 + 1.5, W / 2 + 12.0, -2.3, SLOT_Z1 + 7.5, latch_c)

# Badge plaque, upper-left of the faceplate. The scene stamps the DRIVE
# number onto it (per-drive text cannot live in the shared model).
m.box(13.0, -1.8, 64.0, 52.0, -1.0, 74.0, badge_c)

# IN-USE LED, lower-left -- placed after its label ("IN USE" + arrow, stamped
# by the scene code to the LED's left, matching the 2D widget's layout).
m.cylinder(68.0, -2.6, 16.0, 3.2, 2.0, led_red, axis="y", segments=12)

# (The rainbow mark is stamped by the scene code from CassoBranding's own
# cassowary silhouette -- the same mark as the 2D widget -- not generated
# here as bare stripes.)

# Lid channels: two slim darker strips running front to back, reading as
# the real lid's shallow recessed channels.
for x0, x1 in [(24.0, 62.0), (93.0, 131.0)]:
    m.box(x0, 18.0, H - 0.01, x1, D - 24.0, H + 0.35, beige_dk)

# Side vents: nine slits per side, rear half.
for side_x0, side_x1 in [(-0.35, 0.1), (W - 0.1, W + 0.35)]:
    for i in range(9):
        y0 = 146.0 + i * 7.0
        m.box(side_x0, y0, 20.0, side_x1, y0 + 3.2, 66.0, beige_dk)

# Rubber feet.
for fx in (16.0, W - 16.0):
    for fy in (18.0, D - 18.0):
        m.cylinder(fx, fy, -2.2, 6.0, 2.2, foot_c, axis="z", segments=12)

if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    m.emit(os.path.join(out, "DiskII.obj"), os.path.join(out, "DiskII.mtl"), "DiskII.mtl")
    print(f"DiskII: {len(m.verts)} verts, {len(m.tris)} tris")
