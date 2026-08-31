"""Renders an OBJ/MTL pair to a shaded PNG through a tiny software
z-buffer rasterizer (orthographic), so interpenetrating CSG-style solids
resolve exactly the way the real D3D renderer's depth buffer will."""

import sys
import numpy as np
from PIL import Image


def load(obj_path, mtl_path):
    colors = {}
    name = None
    for line in open(mtl_path):
        t = line.split()
        if not t:
            continue
        if t[0] == "newmtl":
            name = t[1]
        elif t[0] == "Kd" and name:
            colors[name] = (float(t[1]), float(t[2]), float(t[3]))

    verts, tris, tri_colors = [], [], []
    current = (0.8, 0.8, 0.8)
    for line in open(obj_path):
        t = line.split()
        if not t:
            continue
        if t[0] == "v":
            verts.append((float(t[1]), float(t[2]), float(t[3])))
        elif t[0] == "usemtl":
            current = colors.get(t[1], current)
        elif t[0] == "f":
            idx = [int(p.split("/")[0]) - 1 for p in t[1:]]
            for i in range(1, len(idx) - 1):
                tris.append((idx[0], idx[i], idx[i + 1]))
                tri_colors.append(current)
    return np.array(verts, dtype=float), tris, tri_colors


def render(obj_path, mtl_path, png_path, elev=18, azim=-55, size=(990, 770)):
    verts, tris, tri_colors = load(obj_path, mtl_path)
    w, h = size

    er, ar = np.radians(elev), np.radians(azim)
    view = np.array([np.cos(er) * np.cos(ar), np.cos(er) * np.sin(ar), np.sin(er)])
    up_hint = np.array([0.0, 0.0, 1.0])
    right = np.cross(up_hint, view)
    right /= np.linalg.norm(right)
    up = np.cross(view, right)

    light = np.array([0.35, -0.7, 0.61])
    light /= np.linalg.norm(light)

    center = (verts.max(axis=0) + verts.min(axis=0)) / 2
    rel = verts - center
    px = rel @ right
    py = rel @ up
    pz = rel @ view                       # larger = closer to the camera

    span = max(px.max() - px.min(), py.max() - py.min()) * 1.15
    scale = min(w, h) / span
    sx = px * scale + w / 2
    sy = h / 2 - py * scale

    bg = np.array([0x20, 0x24, 0x30], dtype=float)
    frame = np.tile(bg, (h, w, 1)).astype(float)
    zbuf = np.full((h, w), -1e18)

    for (a, b, c), col in zip(tris, tri_colors):
        n = np.cross(verts[b] - verts[a], verts[c] - verts[a])
        ln = np.linalg.norm(n)
        if ln < 1e-9:
            continue
        n /= ln
        lam = max(float(n @ light), 0.0)
        amb = 0.42
        shade = np.array([min(1.0, ch * (amb + (1.0 - amb) * lam)) for ch in col]) * 255

        xs = np.array([sx[a], sx[b], sx[c]])
        ys = np.array([sy[a], sy[b], sy[c]])
        zs = np.array([pz[a], pz[b], pz[c]])

        x0, x1 = max(int(xs.min()), 0), min(int(xs.max()) + 1, w)
        y0, y1 = max(int(ys.min()), 0), min(int(ys.max()) + 1, h)
        if x0 >= x1 or y0 >= y1:
            continue

        gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if abs(d) < 1e-9:
            continue
        w0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / d
        w1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / d
        w2 = 1.0 - w0 - w1
        inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
        if not inside.any():
            continue
        z = w0 * zs[0] + w1 * zs[1] + w2 * zs[2]

        tile_z = zbuf[y0:y1, x0:x1]
        passed = inside & (z > tile_z)
        tile_z[passed] = z[passed]
        tile_c = frame[y0:y1, x0:x1]
        tile_c[passed] = shade

    Image.fromarray(frame.astype(np.uint8)).save(png_path)


if __name__ == "__main__":
    render(sys.argv[1], sys.argv[2], sys.argv[3],
           elev=float(sys.argv[4]) if len(sys.argv) > 4 else 18,
           azim=float(sys.argv[5]) if len(sys.argv) > 5 else -55)
