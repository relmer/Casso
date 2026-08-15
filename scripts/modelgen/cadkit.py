"""CAD export bridge: CadQuery solids -> the OBJ/MTL dialect ObjMeshParser
consumes (millimeter units, global vertex indices, flat Kd materials, no
normals or UVs).

Why this exists. The first generators emitted geometry by hand, one quad at a
time. That is unreviewable: nothing in a wall of add_quad calls makes "this
face covers the entire screen opening" visible, and shape errors only surface
in a screenshot. Modeling with a real kernel removes the whole class -- a hole
is a boolean cut, so it cannot accidentally be a solid face, and an edge
rounding is a fillet rather than a hand-built chamfer strip.

A part is a solid plus the material role the desk scene identifies it by
(DeskSceneModel matches Kd VALUES within +/-0.02, so the palette here is the
contract, not decoration). Each part tessellates independently and lands in
its own usemtl group.
"""

import cadquery as cq


# The identity palette the scene matches on. Anything not in it is ordinary
# opaque case material and may take any color clear of these.
KD = {
    "glass":          (0.050, 0.090, 0.070),
    "monitor_lamp":   (0.290, 0.870, 0.380),
    "drive_lamp":     (0.900, 0.120, 0.100),
    "drive_door":     (0.160, 0.160, 0.180),
    "drive_latch":    (0.230, 0.230, 0.250),
    "drive_lamp_alt": (0.250, 0.845, 0.330),
    "drive_door_alt": (0.720, 0.712, 0.685),
    "drive_latch_alt":(0.640, 0.632, 0.605),

    # Placement metadata, not scenery: a marker the scene reads to learn
    # where the brand mark belongs, then throws away instead of drawing.
    # Bury it inside a solid -- nothing should ever see it.
    "brand_anchor":   (0.980, 0.010, 0.640),
}


class Part:
    """One solid and the flat color it exports under.

    Two tolerances, and for small radii the SECOND is the one that matters.
    The linear one bounds how far a chord may sag from the true surface, but
    a 3 mm fillet's chords never sag far enough to trip it, so tightening it
    changes nothing at all (measured: identical triangle counts from 0.35
    all the way down to 0.01). The angular one bounds the turn per segment,
    and it is what actually decides whether a rounded corner reads as a round
    or as two flats meeting at an angle.
    """

    def __init__(self, name, solid, color, tolerance=0.35, angular=0.3):
        self.name = name
        self.solid = solid
        self.color = color
        self.tolerance = tolerance
        self.angular = angular


class Model:
    def __init__(self):
        self.parts = []
        self.raw = []          # analytic triangle soup (curved sheets)

    def add(self, name, solid, color, tolerance=0.35, angular=0.3):
        self.parts.append(Part(name, solid, color, tolerance, angular))
        return self

    def add_triangles(self, name, tris, color):
        """For surfaces that are mathematical rather than CAD features -- the
        CRT sheet is a spherical sag the scene FITS a sphere to, so it is
        generated analytically at a controlled density instead of being
        tessellated from a solid at whatever the kernel chooses."""
        self.raw.append((name, tris, color))
        return self

    def emit(self, obj_path, mtl_path, mtl_name):
        groups = []            # (material_name, [ (v0,v1,v2), ... ])
        colors = {}

        for part in self.parts:
            shape = part.solid.val() if hasattr(part.solid, "val") else part.solid
            verts, faces = shape.tessellate(part.tolerance, part.angular)
            tris = [tuple(tuple(verts[i]) for i in f) for f in faces]
            mat = f"mat_{len(groups)}"
            colors[mat] = part.color
            groups.append((mat, tris))

        for name, tris, color in self.raw:
            mat = f"mat_{len(groups)}"
            colors[mat] = color
            groups.append((mat, tris))

        with open(mtl_path, "w", newline="\n") as f:
            for mat, (r, g, b) in colors.items():
                f.write(f"newmtl {mat}\nKa 0 0 0 \nKd {r:g} {g:g} {b:g}\nd 1\n\n")

        index = {}
        lines_v = []
        lines_f = []

        for mat, tris in groups:
            lines_f.append(f"usemtl {mat}")
            for tri in tris:
                ids = []
                for p in tri:
                    key = (round(p[0], 4), round(p[1], 4), round(p[2], 4))
                    if key not in index:
                        index[key] = len(index) + 1
                        lines_v.append(f"v {key[0]:g} {key[1]:g} {key[2]:g}")
                    ids.append(index[key])
                if ids[0] != ids[1] and ids[1] != ids[2] and ids[0] != ids[2]:
                    lines_f.append(f"f {ids[0]} {ids[1]} {ids[2]}")

        with open(obj_path, "w", newline="\n") as f:
            f.write(f"mtllib {mtl_name}\n")
            f.write("\n".join(lines_v))
            f.write("\n")
            f.write("\n".join(lines_f))
            f.write("\n")

        return len(index), sum(len(t) for _, t in groups)


def rect_wire(y, x0, x1, z0, z1):
    """A rectangular wire at depth y, from explicit 3D corners -- loft
    sections built this way sidestep every workplane-orientation trap."""
    import cadquery as cq

    pts = [cq.Vector(x0, y, z0), cq.Vector(x1, y, z0),
           cq.Vector(x1, y, z1), cq.Vector(x0, y, z1), cq.Vector(x0, y, z0)]
    return cq.Wire.makePolygon(pts)


def round_rect_wire(y, x0, x1, z0, z1, r):
    """A rounded rectangle wire at depth y: straight runs joined by quarter
    arcs of radius r.

    Lofting between two of these rounds the resulting wall's corners BY
    CONSTRUCTION, which is far steadier than lofting sharp sections and then
    hunting the diagonal corner edges with a selector to fillet them.
    """
    import cadquery as cq

    def v(x, z):
        return cq.Vector(x, y, z)

    def corner(cx, cz, sx, sz):
        # Quarter arc about (cx, cz), from the x-side point to the z-side one.
        k = 0.70710678
        return cq.Edge.makeThreePointArc(v(cx + sx * r, cz),
                                         v(cx + sx * r * k, cz + sz * r * k),
                                         v(cx, cz + sz * r))

    edges = [
        cq.Edge.makeLine(v(x0 + r, z0), v(x1 - r, z0)),
        corner(x1 - r, z0 + r, +1, -1),
        cq.Edge.makeLine(v(x1, z0 + r), v(x1, z1 - r)),
        corner(x1 - r, z1 - r, +1, +1),
        cq.Edge.makeLine(v(x1 - r, z1), v(x0 + r, z1)),
        corner(x0 + r, z1 - r, -1, +1),
        cq.Edge.makeLine(v(x0, z1 - r), v(x0, z0 + r)),
        corner(x0 + r, z0 + r, -1, -1),
    ]

    return cq.Wire.assembleEdges(edges)


def sag_sheet(x0, x1, z0, z1, front_y, radius_scale, cols=48, rows=36):
    """A spherical-sag rectangular sheet bulging toward the viewer (-Y).

    The desk scene DERIVES its display sphere from this mesh, so it stays a
    clean analytic surface at a controlled density: the scene floats its own
    picture grid fractions of a millimeter above it, and a coarse sheet's
    chord crests poke through as dark bites at grazing angles.

    `radius_scale` is the sphere radius as a multiple of the sheet's half
    diagonal. Larger is FLATTER. A period 12-inch tube is far flatter than a
    9-inch one, and too small a value curls the picture's corners away hard
    enough that the raster reads as a disc rather than a rectangle.
    """
    import math

    cx, cz = (x0 + x1) * 0.5, (z0 + z1) * 0.5
    half_diag = math.hypot((x1 - x0) * 0.5, (z1 - z0) * 0.5)
    r = half_diag * radius_scale
    max_sag = r - math.sqrt(max(r * r - half_diag * half_diag, 0.0))

    def pt(ci, ri):
        x = x0 + (x1 - x0) * ci / cols
        z = z0 + (z1 - z0) * ri / rows
        rr = math.hypot(x - cx, z - cz)
        sag = r - math.sqrt(max(r * r - rr * rr, 0.0))
        return (x, front_y - (max_sag - sag), z)

    tris = []
    for ri in range(rows):
        for ci in range(cols):
            a, b, c, d = pt(ci, ri), pt(ci + 1, ri), pt(ci + 1, ri + 1), pt(ci, ri + 1)
            tris.append((a, b, c))
            tris.append((a, c, d))
    return tris
