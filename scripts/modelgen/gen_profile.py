"""Apple ProFile 5MB hard disk, parametric: the wide low slab,
~439 x 110 x 226 mm. Beige case with a plainer front: full-width shallow
grille band low on the face, "ProFile" nameplate lower-left, ready lamp
lower-right. X right, Y back, Z up; front at y=0."""

from meshkit import Mesh

W, H, D = 439.0, 110.0, 226.0

m = Mesh()

beige    = m.color("beige",    (0.833, 0.784, 0.659))
beige_dk = m.color("beige_dk", (0.760, 0.710, 0.590))
grille_c = m.color("grille",   (0.240, 0.230, 0.210))
name_c   = m.color("name",     (0.150, 0.150, 0.160))
lamp_c   = m.color("lamp",     (0.900, 0.120, 0.100))
foot_c   = m.color("foot",     (0.150, 0.140, 0.130))

# Case: front face leans back a touch (the ProFile's subtle front rake).
m.hexahedron(
    [(0, 0, 0), (W, 0, 0), (W, D, 0), (0, D, 0),
     (0, 4.0, H), (W, 4.0, H), (W, D, H), (0, D, H)],
    beige)

# Front grille: a low horizontal band of dark vent slots.
G_Z0, G_Z1 = 16.0, 40.0
for i in range(24):
    x0 = 26.0 + i * 16.0
    if x0 + 10.0 > W - 26.0:
        break
    m.box(x0, -0.8, G_Z0, x0 + 10.0, 0.4, G_Z1, grille_c)

# Nameplate, lower-left above the grille.
m.box(24.0, -1.2, 52.0, 110.0, -0.3, 66.0, name_c)

# Ready lamp, lower-right.
m.cylinder(W - 40.0, -1.8, 58.0, 4.0, 1.8, lamp_c, axis="y", segments=12)

# Lid: shallow full-width recess reading as the top's stepped panel.
m.box(26.0, 30.0, H - 0.01, W - 26.0, D - 30.0, H + 0.4, beige_dk)

# Feet.
for fx in (30.0, W - 30.0):
    for fy in (22.0, D - 22.0):
        m.cylinder(fx, fy, -2.4, 7.0, 2.4, foot_c, axis="z", segments=12)

if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    m.emit(os.path.join(out, "ProFile.obj"), os.path.join(out, "ProFile.mtl"), "ProFile.mtl")
    print(f"ProFile: {len(m.verts)} verts, {len(m.tris)} tris")
