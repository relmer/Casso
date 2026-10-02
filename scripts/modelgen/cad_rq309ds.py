"""Panasonic RQ-309DS portable cassette recorder (1974), the deck Apple ][
owners actually loaded tapes from. 140 x 70 x 260 mm (W x H x D), per
radiomuseum.org. X right, Y back, Z up; the key end faces the viewer at y = 0.

MODELED FROM PHOTOGRAPHS (radiomuseum.org, panasonic_rq_309ds_2799386.jpg top
view and panasonic_rq_309ds_2799388.jpg side view). The top view is
150 x 290 px for the 140 x 260 mm case plus handle, so it is read as fractions
of the depth below rather than picked by eye.

It lies FLAT, the way it sat beside a computer: the controls are all on top.

  - a dark navy body, a slab that runs full height at the back and falls
    away on a slope over the last fifth toward the keys;
  - a silver top deck carrying, back to front: a black-framed speaker grille
    (about 36% of the depth), the smoked cassette window (22%), a black label
    strip that reads "National Panasonic", then the row of six piano keys --
    five cream, the last (stop/eject) blue-green;
  - a chrome carrying handle folded across the front end.

Sub-mesh identity is by part NAME; `window` is where the scene can show the
tape turning, `keys_*` are the transport.
"""

import cadquery as cq
from cadkit import Model

W, H, D = 140.0, 70.0, 260.0

# The slope: the side view shows the top line dropping over the front ~12%
# to about two thirds height. The top view shows it silver, like the deck.
SLOPE_Y = 30.0
FRONT_H = 46.0
EDGE_R  = 3.0

BODY    = (0.090, 0.100, 0.150)    # dark navy, not black
DECK    = (0.680, 0.685, 0.690)    # brushed silver, clear of drive_door_alt
FRAME   = (0.060, 0.060, 0.065)
GRILLE  = (0.420, 0.425, 0.430)
WINDOW  = (0.075, 0.095, 0.120)    # smoked lid, clear of glass and plate_recess
LABEL   = (0.080, 0.080, 0.085)
KEY     = (0.900, 0.880, 0.720)    # cream
KEY_ALT = (0.180, 0.420, 0.380)    # stop/eject
CHROME  = (0.820, 0.830, 0.840)

INSET = 6.0                         # deck margin from the body's sides
DECK_T = 1.2


def body():
    # Side profile (Y,Z), extruded across X.
    prof = (cq.Workplane("YZ")
            .polyline([(0, 0), (D, 0), (D, H), (SLOPE_Y, H), (0, FRONT_H)])
            .close()
            .extrude(W))
    return prof.edges("|X").fillet(EDGE_R)


def deck_plate(y0, y1):
    return (cq.Workplane("XY")
            .box(W - 2 * INSET, y1 - y0, DECK_T, centered=False)
            .translate((INSET, y0, H)))


def flat_box(x0, x1, y0, y1, z0, z1):
    return (cq.Workplane("XY")
            .box(x1 - x0, y1 - y0, z1 - z0, centered=False)
            .translate((x0, y0, z0)))


def build():
    m = Model()
    top = H + DECK_T

    m.add("body", body(), BODY)
    m.add("deck", deck_plate(SLOPE_Y, D - 3.0), DECK)
    lip = (cq.Workplane("YZ")
           .polyline([(1.0, FRONT_H - 0.6), (SLOPE_Y, H - 0.6), (SLOPE_Y, H + DECK_T), (1.0, FRONT_H + DECK_T)])
           .close().extrude(W - 2 * INSET).translate((INSET, 0, 0)))
    m.add("lip", lip, DECK)

    # Speaker grille: back 36% of the depth, in a black frame.
    gy0, gy1 = D - 98.0, D - 8.0
    m.add("grille_frame", flat_box(INSET + 2, W - INSET - 2, gy0, gy1, top, top + 1.0), FRAME)
    perf = flat_box(INSET + 6, W - INSET - 6, gy0 + 4, gy1 - 4, top + 0.6, top + 1.3)
    holes = cq.Workplane("XY").workplane(offset=top).rarray(4.0, 4.0, 24, 19).circle(1.0).extrude(2.0)
    holes = holes.translate((W / 2, (gy0 + gy1) / 2, 0))
    m.add("grille", perf.cut(holes), GRILLE)

    # Cassette window.
    wy0, wy1 = gy0 - 62.0, gy0 - 3.0
    m.add("window_frame", flat_box(INSET + 2, W - INSET - 2, wy0, wy1, top, top + 0.8), FRAME)
    m.add("window", flat_box(INSET + 8, W - INSET - 8, wy0 + 5, wy1 - 6, top + 0.5, top + 1.2), WINDOW)

    # Label strip.
    ly0, ly1 = wy0 - 19.0, wy0 - 3.0
    m.add("label", flat_box(INSET + 2, W - INSET - 2, ly0, ly1, top, top + 0.6), LABEL)

    # Six piano keys straddling the top of the slope, the front ones stepping
    # down with it so each pokes the same height above the case.
    kx0, kx1 = INSET + 4.0, W - INSET - 4.0
    pitch = (kx1 - kx0) / 6.0
    ky0, ky1 = SLOPE_Y - 4.0, ly0 - 2.0
    for i in range(6):
        x0 = kx0 + i * pitch + 0.8
        key = flat_box(x0, x0 + pitch - 1.6, ky0, ky1, H - 12.0, top + 5.0)
        m.add(f"keys_{i}", key, KEY_ALT if i == 5 else KEY, angular=0.2)

    # Chrome handle folded across the front end: a bar on two short arms.
    r = 3.0
    bar = (cq.Workplane("YZ").circle(r).extrude(W - 10.0)
           .translate((5.0, -4.0, FRONT_H * 0.55)))
    arm_l = flat_box(2.0, 6.0, -4.0, 14.0, FRONT_H * 0.55 - 2.0, FRONT_H * 0.55 + 2.0)
    arm_r = flat_box(W - 6.0, W - 2.0, -4.0, 14.0, FRONT_H * 0.55 - 2.0, FRONT_H * 0.55 + 2.0)
    m.add("handle", bar.union(arm_l).union(arm_r), CHROME, angular=0.15)

    return m


if __name__ == "__main__":
    nv, nt = build().emit("CassetteRecorder.mesh", "CassetteRecorder.mtl", "CassetteRecorder.mtl")
    print(f"{nv} vertices, {nt} triangles")
