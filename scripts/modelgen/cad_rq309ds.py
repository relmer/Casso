"""Panasonic RQ-309DS portable cassette recorder (1974), the deck Apple ][
owners actually loaded tapes from. 140 x 70 x 260 mm (W x H x D), per
radiomuseum.org. X right, Y back, Z up; the key end faces the viewer at y = 0.

MODELED FROM PHOTOGRAPHS of the black-key version (an eBay listing; see
Resources/Models/CassetteRecorder/README.md for the links) and radiomuseum.org's
left-side profile. The 260 mm depth runs from the chrome handle's front to the
back of the case.

THE SIDE PROFILE IS MEASURED, not drawn by eye: the profile photo is 464 px
from handle to back and 120 px from desk to top, 0.56 mm a pixel along and
0.58 up. From it:

    handle      0 -  17     chrome carry handle, standing proud of the front
                            face; its bar 15 mm deep and 7 mm tall, 30-37 mm up,
                            with arms running 43 mm back along both sides
    front face  17          vertical, 25-49 mm up
    top slope   17 - 51.5   up to the top face, about 31 degrees
    bottom      17 - 40     down to the bottom face, about 47 degrees
    back        260         its top and bottom edges rounded

and from the top view, front to back along the top: the keys over the top
slope, the silver legend plate, the smoked door, then the grille.

It lies FLAT, the way it sat beside a computer: the controls are all on top.
The case is black pebbled plastic and stands a narrow black rim around the
top plates.

Sub-mesh identity is by part NAME: `door_glass` and `door_print` are the door
the scene swings open, `cassette*` the cassette it shows only with a tape in,
`keys_*` the transport, and `chrome_*` polished metal.
"""

import math

import cadquery as cq
from cadkit import Model

W, H, D = 140.0, 70.0, 260.0

# The side profile, measured from the photograph (see the docstring).
YF      = 17.0                      # the front face, behind the handle
FACE_Z0 = 25.0                      # the front face's bottom
FACE_Z1 = 49.0                      # and its top
TOP_Y   = 51.5                      # where the top slope meets the top face
BOT_Y   = 40.0                      # where the bottom slope meets the bottom face
BACK_R  = 8.0                       # the back edges' rounding
EDGE_R  = 4.0
RIM     = 5.0                       # black rim around the top plates

# Colors. Every one is kept more than 0.02 in some channel from cadkit.KD.
BODY    = (0.200, 0.200, 0.205)    # dark gray, not black, so it shows in the light
KEY     = (0.185, 0.185, 0.190)    # a shade off the case; clear of drive_door and drive_latch
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
            .polyline([(YF, FACE_Z0), (BOT_Y, 0), (D, 0), (D, H), (TOP_Y, H), (YF, FACE_Z1)])
            .close()
            .extrude(W))
    prof = prof.edges("|Z").fillet(EDGE_R)
    back = cq.selectors.BoxSelector((-1, D - 0.1, -1), (W + 1, D + 0.1, H + 1))
    prof = prof.edges("|X").edges(back).fillet(BACK_R)
    return prof


def strut(x, y_back, z_top, radius, thick):
    """One of the door's hinge struts: a quarter arc seen from the side, from
    the door's back edge round and down to the hinge under the grille, as
    wide in X as the door is thick."""
    cy, cz = y_back, z_top - thick / 2 - radius
    ro, ri = radius + thick / 2, radius - thick / 2
    k = 0.70710678
    arc = (cq.Workplane("YZ")
           .moveTo(cy, cz + ro)
           .threePointArc((cy + ro * k, cz + ro * k), (cy + ro, cz))
           .lineTo(cy + ri, cz)
           .threePointArc((cy + ri * k, cz + ri * k), (cy, cz + ri))
           .close()
           .extrude(thick))
    return arc.translate((x, 0, 0))


def perforation(x0, x1, y0, y1, z, pitch=2.0, dot=0.8):
    """The grille's holes as dark round dots: thousands of real holes would cost
    a quarter of a million triangles for a texture the eye reads as gray."""
    tris = []
    row = 0
    y = y0 + pitch / 2
    while y < y1 - dot:
        x = x0 + pitch / 2 + (pitch / 2 if row % 2 else 0.0)
        while x < x1 - dot:
            # An octagon reads as round at any distance the scene is viewed
            # from, at six triangles a hole rather than the square's two.
            cx, cy, r = x + dot / 2, y + dot / 2, dot / 2
            ring = [(cx + r * math.cos(math.pi * k / 4), cy + r * math.sin(math.pi * k / 4), z)
                    for k in range(8)]
            tris += [(ring[0], ring[k], ring[k + 1]) for k in range(1, 7)]
            x += pitch
        y += pitch * 0.866
        row += 1
    return tris


def build():
    m = Model()
    top = H
    x0, x1 = RIM, W - RIM

    # The door's opening, front to back.
    dy0, dy1 = 90.0, 154.0
    DOOR_T   = 1.5                       # the pane's thickness
    STRUT_R  = 10.0                      # the struts' arc, seen from the side

    # The seat the pane drops into, flush, and the cassette well under it:
    # without them the cassette is inside the solid and the door shows nothing.
    seat = box(x0, x1, dy0, dy1, top - DOOR_T, top + 1.0)
    well = box(x0 + 4, x1 - 4, dy0 + 4, dy1 - 6, top - 9.0, top + 1.0)
    m.add("body", body().cut(seat).cut(well), BODY, angular=0.14)
    m.add("well_floor", box(x0 + 4, x1 - 4, dy0 + 4, dy1 - 6, top - 9.0, top - 8.5), PERF)

    # Grille.
    m.add("grille", box(x0, x1, dy1 + 2.0, D - RIM, top - 0.5, top + 0.6), GRILLE)
    m.add_triangles("grille_perf", perforation(x0 + 1, x1 - 1, dy1 + 3.0, D - RIM - 1, top + 0.62), PERF)

    # THE DOOR IS ONE PIECE OF SMOKED PLASTIC, frameless, flush with the top
    # face; the scene draws it see-through. Two struts of the same plastic,
    # 3 mm in from its sides, arc from its back edge down under the grille to
    # the hinge, which is why it swings up in an arc rather than about its own
    # back edge.
    pane = box(x0, x1, dy0, dy1, top - DOOR_T, top)
    for sx in (x0 + 3.0, x1 - 3.0 - DOOR_T):
        pane = pane.union(strut(sx, dy1, top, STRUT_R, DOOR_T))
    m.add("door_glass", pane, DOOR, angular=0.2)
    m.add("door_print",
          box(x0 + 6, x0 + 30, dy1 - 7.5, dy1 - 3.5, top, top + 0.1)
          .cut(box(x0 + 6.5, x0 + 29.5, dy1 - 7.0, dy1 - 4.0, top - 0.1, top + 0.2))
          .union(text("AUTO STOP", 2.6, x0 + 18, dy1 - 5.5, top))
          .union(text("AC/BATTERY", 2.6, x0 + 46, dy1 - 5.5, top)),
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
    sy0, sy1 = TOP_Y + 1.5, dy0 - 3.0
    m.add("strip", box(x0, x1, sy0, sy1 + 3.0, top - 0.5, top + 0.6), SILVER)
    by0, by1 = sy1 - 15.0, sy1 - 1.0
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

    # Keys: 5 mm black plates with FLAT tops, flush with the top plates and
    # parallel to the grille. They do not reach down into the slope: each is
    # hinged at the back, just under the edge of the legend plate, and
    # cantilevers forward over the slope.
    #
    # The recess is a stadium -- a rounded rectangle closed by a semicircle at
    # each end -- cut STRAIGHT DOWN into the key, flat at the bottom. Both of
    # its edges are rounded over a little: where the wall meets the key face,
    # and where it meets the floor.
    kw = pitch - 2.0
    ky0, ky1 = YF + 2.0, sy0 + 1.0
    kl = ky1 - ky0
    ktop = top + 0.6
    KEY_T = 5.0
    RECESS_D = 2.0
    RIM_R, FLOOR_R = 0.5, 0.6
    rw = kw * 0.31                          # the half width of the stadium
    rl = kl * 0.72                          # its overall length
    for i in range(6):
        kx = kx0 + i * pitch + 1.0
        cx, cy = kx + kw / 2, ky0 + kl / 2
        key = (box(kx, kx + kw, ky0, ky1, ktop - KEY_T, ktop)
               .edges("|Z").fillet(2.0)
               .faces(">Z").edges().fillet(0.8))
        tool = (cq.Workplane("XY").workplane(offset=ktop - RECESS_D)
                .center(cx, cy).slot2D(rl, 2.0 * rw, 90).extrude(RECESS_D + 1.0)
                .faces("<Z").edges().fillet(FLOOR_R))
        key = key.cut(tool)
        rim = cq.selectors.BoxSelector((cx - rw - 0.1, cy - rl / 2 - 0.1, ktop - 0.01),
                                       (cx + rw + 0.1, cy + rl / 2 + 0.1, ktop + 0.01))
        key = key.edges(rim).fillet(RIM_R)
        hinge = box(kx + 2.0, kx + kw - 2.0, ky1 - 4.0, ky1 - 0.5, top - 6.0, ktop - KEY_T + 0.5)
        m.add(f"keys_{i}", key.union(hinge), KEY, angular=0.25)
    # THE CARRY HANDLE, polished chrome: a bar across the front, standing
    # proud of the front face by its own depth, on two arms that run back
    # along the sides into the case. Measured from the side profile.
    HANDLE_Z0, HANDLE_Z1 = 30.0, 37.0
    ARM_T, ARM_BACK      = 1.5, YF + 43.0
    handle = box(-ARM_T, W + ARM_T, 2.0, YF, HANDLE_Z0, HANDLE_Z1)
    handle = handle.edges("|Z").fillet(4.0)
    handle = handle.union(box(-ARM_T, 0.0, 2.0, ARM_BACK, HANDLE_Z0, HANDLE_Z1))
    handle = handle.union(box(W, W + ARM_T, 2.0, ARM_BACK, HANDLE_Z0, HANDLE_Z1))
    m.add("chrome_handle", handle, CHROME, angular=0.15)

    # The tone and volume thumbwheels in a recess in the back end.
    m.add("wheel_well", box(W / 2 - 34, W / 2 + 34, D - 0.3, D + 0.2, 26.0, 46.0), KEY)
    wheels = None
    for wx in (W / 2 - 17, W / 2 + 17):
        wheel = (cq.Workplane("YZ").workplane(offset=wx - 6).center(D - 4, 40.0)
                 .polygon(24, 18.0).extrude(12.0))
        wheels = wheel if wheels is None else wheels.union(wheel)
    m.add("wheels", wheels, BAND)
    # Read from BEHIND, so tone is on the viewer's left there -- the +X side --
    # and the text is turned to face +Y, not -Y, or it reads mirrored.
    def back_text(s, x):
        return (text(s, 2.2, x, D + 0.3, 29.0)
                .rotate((x, D + 0.3, 29.0), (x + 1, D + 0.3, 29.0), 90)
                .rotate((x, D + 0.3, 29.0), (x, D + 0.3, 30.0), 180))

    m.add("wheel_legend",
          back_text("LOW-TONE-HIGH", W / 2 + 17).union(back_text("MIN-VOLUME-MAX", W / 2 - 17)),
          SILVER)
    return m


if __name__ == "__main__":
    nv, nt = build().emit("CassetteRecorder.mesh", "CassetteRecorder.mtl", "CassetteRecorder.mtl")
    print(f"{nv} vertices, {nt} triangles")
