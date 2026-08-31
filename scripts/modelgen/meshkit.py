"""Tiny CSG-lite kit that emits Tinkercad-flavored OBJ/MTL (the subset
Casso's ObjMeshParser consumes: v + fan-triangulated f + usemtl/Kd).

Coordinates: millimeters. X = right, Y = back (away from viewer), Z = up.
Faces wind counter-clockwise viewed from outside (loader ignores winding,
but the preview renderer uses it for normals)."""

import math


class Mesh:
    def __init__(self):
        self.verts = []            # (x, y, z)
        self.tris = []             # (i0, i1, i2, colorname) 0-based
        self.colors = {}           # name -> (r, g, b) 0..1

    def color(self, name, rgb):
        self.colors[name] = rgb
        return name

    def add_tri(self, p0, p1, p2, color):
        base = len(self.verts)
        self.verts.extend([p0, p1, p2])
        self.tris.append((base, base + 1, base + 2, color))

    def add_quad(self, p0, p1, p2, p3, color):
        self.add_tri(p0, p1, p2, color)
        self.add_tri(p0, p2, p3, color)

    def hexahedron(self, c, color):
        """8 corners: bottom b0..b3 CCW viewed from above, then top t0..t3
        aligned above them.  c = [b0,b1,b2,b3,t0,t1,t2,t3]."""
        b0, b1, b2, b3, t0, t1, t2, t3 = c
        self.add_quad(b3, b2, b1, b0, color)   # bottom (viewed from below)
        self.add_quad(t0, t1, t2, t3, color)   # top
        self.add_quad(b0, b1, t1, t0, color)   # front  (y min side if CCW order)
        self.add_quad(b1, b2, t2, t1, color)   # right
        self.add_quad(b2, b3, t3, t2, color)   # back
        self.add_quad(b3, b0, t0, t3, color)   # left
        return self

    def box(self, x0, y0, z0, x1, y1, z1, color):
        return self.hexahedron(
            [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
             (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], color)

    def frustum_box(self, x0, y0, z0, x1, y1, z1, inset_top, color,
                    inset_front=0.0, inset_back=None):
        """Box whose top face shrinks inward by inset_top on left/right and by
        inset_front / inset_back on the front/back edges."""
        if inset_back is None:
            inset_back = inset_front
        return self.hexahedron(
            [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
             (x0 + inset_top, y0 + inset_front, z1),
             (x1 - inset_top, y0 + inset_front, z1),
             (x1 - inset_top, y1 - inset_back, z1),
             (x0 + inset_top, y1 - inset_back, z1)], color)

    def cylinder(self, cx, cy, cz, radius, height, color, axis="z", segments=16):
        """Closed prism along +axis starting at (cx,cy,cz)."""
        ring0, ring1 = [], []
        for i in range(segments):
            a = 2.0 * math.pi * i / segments
            dx, dy = radius * math.cos(a), radius * math.sin(a)
            if axis == "z":
                ring0.append((cx + dx, cy + dy, cz))
                ring1.append((cx + dx, cy + dy, cz + height))
            elif axis == "y":
                ring0.append((cx + dx, cy, cz + dy))
                ring1.append((cx + dx, cy + height, cz + dy))
            else:
                ring0.append((cx, cy + dx, cz + dy))
                ring1.append((cx + height, cy + dx, cz + dy))
        for i in range(segments):
            j = (i + 1) % segments
            self.add_quad(ring0[i], ring0[j], ring1[j], ring1[i], color)
        for i in range(1, segments - 1):
            self.add_tri(ring0[0], ring0[i + 1], ring0[i], color)
            self.add_tri(ring1[0], ring1[i], ring1[i + 1], color)
        return self

    def sphere_cap(self, cx, cy, cz, sphere_r, cap_r, color,
                   rings=6, segments=24, bulge="-y"):
        """Spherical cap (CRT glass): a disk of radius cap_r bulging toward
        -Y (out of the screen face) by the sagitta of sphere_r."""
        sag = sphere_r - math.sqrt(max(sphere_r * sphere_r - cap_r * cap_r, 0.0))
        prev_ring = None
        for r in range(rings + 1):
            f = r / rings
            rr = cap_r * f
            zz = (sphere_r - math.sqrt(max(sphere_r * sphere_r - rr * rr, 0.0)))
            ring = []
            for s in range(segments):
                a = 2.0 * math.pi * s / segments
                px = cx + rr * math.cos(a)
                pz = cz + rr * math.sin(a)
                py = cy - (sag - zz) if bulge == "-y" else cy + (sag - zz)
                ring.append((px, py, pz))
            if prev_ring is not None:
                for s in range(segments):
                    t = (s + 1) % segments
                    if r == 1:
                        self.add_tri(prev_ring[0], ring[s], ring[t], color)
                    else:
                        self.add_quad(prev_ring[s], prev_ring[t], ring[t], ring[s], color)
            prev_ring = ring if r > 0 else [(cx, cy - sag if bulge == "-y" else cy + sag, cz)] * segments
            if r == 0:
                prev_ring = [(cx, cy - sag if bulge == "-y" else cy + sag, cz)]
        return self

    def emit(self, obj_path, mtl_path, mtl_name):
        def cname(rgb):
            r, g, b = (int(round(c * 255)) for c in rgb)
            return f"color_{(r << 16) | (g << 8) | b}"

        with open(mtl_path, "w", newline="\n") as m:
            m.write("# Color definition for Casso model generator\n\n")
            emitted = set()
            for name, rgb in self.colors.items():
                key = cname(rgb)
                if key in emitted:
                    continue
                emitted.add(key)
                m.write(f"newmtl {key}\nKa 0 0 0 \n"
                        f"Kd {rgb[0]} {rgb[1]} {rgb[2]}\nd 1\nillum 0.0\n\n")

        with open(obj_path, "w", newline="\n") as o:
            o.write("# Casso model generator (Tinkercad-compatible subset)\n\n")
            o.write(f"mtllib {mtl_name}\n\n")
            for v in self.verts:
                o.write(f"v {v[0]:.4f} {v[1]:.4f} {v[2]:.4f}\n")
            o.write(f"# {len(self.verts)} vertices\n\n")
            current = None
            for i0, i1, i2, color in self.tris:
                key = cname(self.colors[color])
                if key != current:
                    o.write(f"\nusemtl {key}\ns 0\n")
                    current = key
                o.write(f"f {i0 + 1} {i1 + 1} {i2 + 1}\n")
            o.write(f"# {len(self.tris)} faces\n")
