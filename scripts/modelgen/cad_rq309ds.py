"""Panasonic RQ-309DS portable cassette recorder (1974), the deck Apple ][
owners actually loaded tapes from. 140 x 70 x 260 mm (W x H x D), per
radiomuseum.org. X right, Y back, Z up; the key end faces the viewer at y = 0.

MODELED FROM PHOTOGRAPHS of the black-key version (an eBay listing; see
Resources/Models/CassetteRecorder/README.md for the links). The straight-down
top view is 270 px across the 140 mm case and 517 px from handle to back, so
every Y below is a photo row read as millimeters from the front, not a number
picked by eye:

    handle      0 -  11     chrome band wrapped round the front end
    keys       11 -  44     six black keys, flat-topped, stadium-shaped dishes
    strip      44 -  83     silver: black "Panasonic" band with the mic slots
                            at its left, then the RECORD ... EJECT legend row
    door       86 - 154     smoked clear lid, the cassette showing through
    grille    155 - 257     fine-perforated silver plate

It lies FLAT, the way it sat beside a computer: the controls are all on top.
The case is black pebbled plastic and stands a narrow black rim around the
top plates.

Sub-mesh identity is by part NAME; `window` is the cassette door and
`keys_*` are the transport.
"""

import math

import cadquery as cq
from cadkit import Model

W, H, D = 140.0, 70.0, 260.0

# The slope carries the keys: it falls from full height at the back of the key
# row to about three fifths height at the front.
SLOPE_Y = 44.0
FRONT_H = 42.0
SLOPE_A = math.degrees(math.atan2(H - FRONT_H, SLOPE_Y))
EDGE_R  = 4.0
RIM     = 5.0                       # black rim around the top plates

# Colors. Every one is kept more than 0.02 in some channel from cadkit.KD.
BODY    = (0.085, 0.085, 0.092)
KEY     = (0.040, 0.040, 0.044)
KEY_DISH= (0.090, 0.090, 0.098)
SILVER  = (0.760, 0.765, 0.770)
GRILLE  = (0.700, 0.700, 0.690)
PERF    = (0.200, 0.200, 0.205)
BAND    = (0.050, 0.050, 0.055)
PRINT   = (0.940, 0.940, 0.940)
DOOR    = (0.075, 0.095, 0.120)     # the smoked lid's frame
CASSETTE= (0.240, 0.245, 0.255)
CASS_LBL= (0.880, 0.880, 0.870)
HUB     = (0.030, 0.030, 0.030)
BRASS   = (0.700, 0.600, 0.380)
CHROME  = (0.860, 0.870, 0.880)


def box(x0, x1, y0, y1, z0, z1):
    return (cq.Workplane("XY")
            .box(x1 - x0, y1 - y0, z1 - z0, centered=False)
            .translate((x0, y0, z0)))


def text(s, size, x, y, z, halign="center", bold=False):
    return (cq.Workplane("XY").workplane(offset=z)
            .text(s, size, 0.3, halign=halign, valign="center",
                  kind="bold" if bold else "regular", font="Arial")
            .translate((x, y, 0)))


def body():
    prof = (cq.Workplane("YZ")
            .polyline([(0, 0), (D, 0), (D, H), (SLOPE_Y, H), (0, FRONT_H)])
            .close()
            .extrude(W))
    prof = prof.edges("|Z").fillet(EDGE_R)
    return prof.edges("|X").fillet(2.5)


def on_slope(solid, y_front):
    """Tilts a part built lying flat at z = 0 onto the key slope, its front
    edge at y_front."""
    z = FRONT_H + (H - FRONT_H) * y_front / SLOPE_Y
    return solid.rotate((0, 0, 0), (1, 0, 0), SLOPE_A).translate((0, y_front, z))


def perforation(x0, x1, y0, y1, z, pitch=2.0, dot=0.8):
    """The grille's holes as dark dots: thousands of real holes would cost
    a quarter of a million triangles for a texture the eye reads as gray."""
    tris = []
    row = 0
    y = y0 + pitch / 2
    while y < y1 - dot:
        x = x0 + pitch / 2 + (pitch / 2 if row % 2 else 0.0)
        while x < x1 - dot:
            a, b = (x, y, z), (x + dot, y, z)
            c, d = (x + dot, y + dot, z), (x, y + dot, z)
            tris += [(a, b, c), (a, c, d)]
            x += pitch
        y += pitch * 0.866
        row += 1
    return tris


def build():
    m = Model()
    top = H
    x0, x1 = RIM, W - RIM

    # The cassette well under the door: without it the cassette is inside
    # the solid and the door shows nothing.
    well = box(RIM + 4, W - RIM - 4, 90.0, 144.0, H - 9.0, H + 1.0)
    m.add("body", body().cut(well), BODY, angular=0.14)
    m.add("well_floor", box(RIM + 4, W - RIM - 4, 90.0, 144.0, H - 9.0, H - 8.5), PERF)

    # Grille.
    m.add("grille", box(x0, x1, 155.0, D - RIM, top - 0.5, top + 0.6), GRILLE)
    m.add_triangles("grille_perf", perforation(x0 + 1, x1 - 1, 156.0, D - RIM - 1, top + 0.62), PERF)

    # Cassette door: a smoked frame with the cassette standing below it. The
    # renderer has no translucency, so the lid is drawn as its frame and the
    # cassette as seen through it, a shade darker than in the open.
    dy0, dy1 = 86.0, 154.0
    door = box(x0, x1, dy0, dy1, top - 0.5, top + 1.0)
    door = door.cut(box(x0 + 4, x1 - 4, dy0 + 4, dy1 - 10, top - 1, top + 2))
    m.add("window", door, DOOR)
    m.add("door_trim", box(x0, x1, dy1 - 0.8, dy1, top + 1.0, top + 1.3), SILVER)
    m.add("door_print",
          box(x0 + 6, x0 + 30, dy1 - 7.5, dy1 - 3.5, top + 1.0, top + 1.1)
          .cut(box(x0 + 6.5, x0 + 29.5, dy1 - 7.0, dy1 - 4.0, top + 0.9, top + 1.2))
          .union(text("AUTO STOP", 2.6, x0 + 18, dy1 - 5.5, top + 1.0))
          .union(text("AC/BATTERY", 2.6, x0 + 46, dy1 - 5.5, top + 1.0)),
          PRINT)

    cz = top - 4.0
    cx0, cx1 = x0 + 6, x1 - 6
    m.add("cassette", box(cx0, cx1, dy0 + 6, dy1 - 12, cz - 2, cz), CASSETTE)
    m.add("cassette_label", box(W / 2 - 9, W / 2 + 9, dy1 - 32, dy1 - 22, cz, cz + 0.2), CASS_LBL)
    hubs = None
    for hx in (W / 2 - 22, W / 2 + 22):
        hub = cq.Workplane("XY").workplane(offset=cz).center(hx, dy1 - 27).circle(5.0).extrude(0.3)
        hubs = hub if hubs is None else hubs.union(hub)
    m.add("cassette_hubs", hubs, HUB)
    mech = None
    for hx in (W / 2 - 12, W / 2 + 6, W / 2 + 18):
        roller = cq.Workplane("XY").workplane(offset=cz).center(hx, dy0 + 12).circle(2.6).extrude(1.5)
        mech = roller if mech is None else mech.union(roller)
    m.add("transport", mech, BRASS)

    # The silver strip and what is printed on it.
    sy0, sy1 = 44.0, 83.0
    m.add("strip", box(x0, x1, sy0, sy1 + 3.0, top - 0.5, top + 0.6), SILVER)
    by0, by1 = 64.0, 78.0
    m.add("band", box(x0 + 1, x1 - 1, by0, by1, top + 0.6, top + 0.8), BAND)
    m.add("brand", text("Panasonic", 6.5, W / 2 + 8, (by0 + by1) / 2, top + 0.8, bold=True), PRINT)
    slots = None
    for i in range(7):
        sx = x0 + 4 + i * 3.0
        slot = box(sx, sx + 1.6, by0 + 2, by1 - 2, top + 0.8, top + 1.0)
        slots = slot if slots is None else slots.union(slot)
    m.add("mic_slots", slots, SILVER)

    kx0, kx1 = x0 + 1.0, x1 - 1.0
    pitch = (kx1 - kx0) / 6.0
    legends = ["RECORD", "REW", "FF", "PLAY", "STOP", "EJECT"]
    legend = None
    for i, s in enumerate(legends):
        t = text(s, 3.0, kx0 + pitch * (i + 0.5), sy0 + 7, top + 0.6)
        legend = t if legend is None else legend.union(t)
    m.add("legend", legend, BAND)

    # Keys: rounded black blocks with FLAT tops, parallel to the grille, standing
    # up out of the slope; each carries a stadium-shaped dish -- a rounded
    # rectangle closed by a semicircle at each end.
    kw = pitch - 2.0
    ky0, ky1 = 6.0, SLOPE_Y - 1.0
    kl = ky1 - ky0
    ktop = top + 3.0
    for i in range(6):
        kx = kx0 + i * pitch + 1.0
        key = (box(kx, kx + kw, ky0, ky1, FRONT_H - 6.0, ktop)
               .edges("|Z").fillet(2.0)
               .faces(">Z").edges().fillet(1.2))
        dw = kw * 0.62
        dish = (cq.Workplane("XY").workplane(offset=ktop - 0.8)
                .center(kx + kw / 2, ky0 + kl / 2).slot2D(kl * 0.72, dw, 90).extrude(2.0))
        m.add(f"keys_{i}", key.cut(dish), KEY, angular=0.2)
        floor = (cq.Workplane("XY").workplane(offset=ktop - 0.8)
                 .center(kx + kw / 2, ky0 + kl / 2).slot2D(kl * 0.72, dw, 90).extrude(0.05))
        m.add(f"key_dish_{i}", floor, KEY_DISH, angular=0.2)
    # Chrome handle: a flat band wrapped round the front end and a little way
    # down each side.
    hz0, hz1 = 10.0, 24.0
    band = (cq.Workplane("XY")
            .rect(W + 3.0, 16.0).extrude(hz1 - hz0)
            .edges("|Z").fillet(EDGE_R + 1.0)
            .translate((W / 2, 6.5, hz0)))
    band = band.cut(box(1.0, W - 1.0, -1.0, 40.0, hz0 - 1, hz1 + 1))
    m.add("handle", band, CHROME, angular=0.15)

    # The tone and volume thumbwheels in a recess in the back end.
    m.add("wheel_well", box(W / 2 - 34, W / 2 + 34, D - 0.3, D + 0.2, 26.0, 46.0), KEY)
    wheels = None
    for wx in (W / 2 - 17, W / 2 + 17):
        wheel = (cq.Workplane("YZ").workplane(offset=wx - 6).center(D - 4, 40.0)
                 .polygon(24, 18.0).extrude(12.0))
        wheels = wheel if wheels is None else wheels.union(wheel)
    m.add("wheels", wheels, BAND)
    m.add("wheel_legend",
          text("LOW-TONE-HIGH", 2.2, W / 2 - 17, D + 0.3, 29.0)
          .rotate((W / 2 - 17, D + 0.3, 29.0), (W / 2 - 17 + 1, D + 0.3, 29.0), 90)
          .union(text("MIN-VOLUME-MAX", 2.2, W / 2 + 17, D + 0.3, 29.0)
                 .rotate((W / 2 + 17, D + 0.3, 29.0), (W / 2 + 17 + 1, D + 0.3, 29.0), 90)),
          SILVER)

    return m


if __name__ == "__main__":
    nv, nt = build().emit("CassetteRecorder.mesh", "CassetteRecorder.mtl", "CassetteRecorder.mtl")
    print(f"{nv} vertices, {nt} triangles")
