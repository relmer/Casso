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
    back        260         a short upright middle between two chamfers of
                            about 30 degrees to the vertical, each very slightly
                            convex: 23-47 mm up, the chamfers 12 mm and 11 mm deep

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
BACK_Z0 = 23.0                      # the back's upright band, bottom
BACK_Z1 = 47.0                      # and top
BACK_IN_LO = 12.0                   # how far the bottom chamfer steps in
BACK_IN_HI = 11.0                   # and the top one
BACK_BULGE = 1.2                    # the chamfers' gentle convexity
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
CASSETTE= (0.120, 0.120, 0.130)     # the shell
CASS_LBL= (0.930, 0.900, 0.800)     # cream paper
CASS_STRIPE = (0.780, 0.270, 0.150) # the label's colored band
CASS_LINE   = (0.550, 0.540, 0.500) # its writing lines
TAPE    = (0.300, 0.180, 0.100)     # oxide brown
HUB     = (0.920, 0.920, 0.910)     # white hubs
BRASS   = (0.700, 0.600, 0.380)
CHROME  = (0.330, 0.335, 0.345)     # dark base: the scene adds the sheen from above


def box(x0, x1, y0, y1, z0, z1):
    return (cq.Workplane("XY")
            .box(x1 - x0, y1 - y0, z1 - z0, centered=False)
            .translate((x0, y0, z0)))


def text(s, size, x, y, z, halign="center", bold=False):
    return (cq.Workplane("XY").workplane(offset=z)
            .text(s, size, 0.3, halign=halign, valign="center",
                  kind="bold" if bold else "regular", font="Arial")
            .translate((x, y, 0)))


def _bulge(p0, p1, amount):
    """The midpoint of p0-p1 pushed `amount` outward (to the right of the
    direction of travel), for a very slightly convex chamfer."""
    mx, my = (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    n = math.hypot(dx, dy)
    return (mx + dy / n * amount, my - dx / n * amount)


def body():
    lo0, lo1 = (D - BACK_IN_LO, 0.0), (D, BACK_Z0)
    hi0, hi1 = (D, BACK_Z1), (D - BACK_IN_HI, H)
    prof = (cq.Workplane("YZ")
            .moveTo(YF, FACE_Z0)
            .lineTo(BOT_Y, 0)
            .lineTo(*lo0)
            .threePointArc(_bulge(lo0, lo1, BACK_BULGE), lo1)
            .lineTo(*hi0)
            .threePointArc(_bulge(hi0, hi1, BACK_BULGE), hi1)
            .lineTo(TOP_Y, H)
            .lineTo(YF, FACE_Z1)
            .close()
            .extrude(W))
    return prof.edges("|Z").fillet(EDGE_R)


def strut(x, hinge_y, door_under_z, ro, ri, width):
    """One of the door's hinge struts, seen from the side a quarter of a ring
    in the lower-front quadrant about (hinge_y, door_under_z): it leaves the
    door's underside, drops into the cassette well, and curves back to end
    under the grille, where the hinge pin is. Broad seen from the side, as
    thin as the door seen from the front."""
    cy, cz = hinge_y, door_under_z
    k = 0.70710678
    arc = (cq.Workplane("YZ")
           .moveTo(cy - ro, cz)
           .threePointArc((cy - ro * k, cz - ro * k), (cy, cz - ro))
           .lineTo(cy, cz - ri)
           .threePointArc((cy - ri * k, cz - ri * k), (cy - ri, cz))
           .close()
           .extrude(width))
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
    HINGE_Y  = dy1 + 2.0                 # the hinge, just behind the door's back edge
    STRUT_RO = 13.0                      # the struts' ring, seen from the side:
    STRUT_RI = 3.0                       # about a centimeter broad

    # The seat the pane drops into, flush, and the cassette well under it:
    # without them the cassette is inside the solid and the door shows nothing.
    # The well spans the door's full depth: a compact cassette is 64 mm deep.
    seat = box(x0, x1, dy0, dy1, top - DOOR_T, top + 1.0)
    well = box(x0 + 4, x1 - 4, dy0 + 0.5, dy1 - 0.5, top - 9.0, top + 1.0)
    m.add("body", body().cut(seat).cut(well), BODY, angular=0.14)
    m.add("well_floor", box(x0 + 4, x1 - 4, dy0 + 0.5, dy1 - 0.5, top - 9.0, top - 8.5), PERF)

    # Grille, short of the rounded back.
    gy1 = D - BACK_IN_HI - 2.0
    m.add("grille", box(x0, x1, dy1 + 2.0, gy1, top - 0.5, top + 0.6), GRILLE)
    m.add_triangles("grille_perf", perforation(x0 + 1, x1 - 1, dy1 + 3.0, gy1 - 1, top + 0.62), PERF)

    # THE DOOR IS ONE PIECE OF SMOKED PLASTIC, frameless, flush with the top
    # face; the scene draws it see-through. Two struts of the same plastic,
    # 3 mm in from its sides, hang from its underside down into the well and
    # curve back under the grille to the hinge, just behind the door's back
    # edge -- so the door swings up about a point below and behind it.
    pane = box(x0, x1, dy0, dy1, top - DOOR_T, top)
    for sx in (x0 + 3.0, x1 - 3.0 - DOOR_T):
        pane = pane.union(strut(sx, HINGE_Y, top - DOOR_T, STRUT_RO, STRUT_RI, DOOR_T))
    m.add("door_glass", pane, DOOR, angular=0.2)
    m.add("door_print",
          box(x0 + 6, x0 + 30, dy1 - 7.5, dy1 - 3.5, top, top + 0.1)
          .cut(box(x0 + 6.5, x0 + 29.5, dy1 - 7.0, dy1 - 4.0, top - 0.1, top + 0.2))
          .union(text("AUTO STOP", 2.6, x0 + 18, dy1 - 5.5, top))
          .union(text("AC/BATTERY", 2.6, x0 + 46, dy1 - 5.5, top)),
          PRINT)

    # THE CASSETTE, a compact cassette lying label up with its tape edge
    # toward the keys, as it sits in the deck: dark shell, a paper label with
    # a stripe and writing lines, two hub holes and a window cut through it,
    # white toothed hubs in the holes, brown tape wound on the spools in the
    # window, and the tape itself running across the open front edge.
    cz       = top - 3.5                     # the shell's top
    ccx      = W / 2
    ccy      = (dy0 + dy1) / 2
    CW, CD   = 99.0, 62.0                    # a compact cassette, a hair under 100.4 x 63.8
    CT       = 5.0                           # what shows of its thickness above the well floor
    HUB_DX   = 21.25                         # hubs either side of center
    hub_y    = ccy + 3.0
    shell = (box(ccx - CW / 2, ccx + CW / 2, ccy - CD / 2, ccy + CD / 2, cz - CT, cz)
             .edges("|Z").fillet(2.5))
    m.add("cassette", shell, CASSETTE)

    LBL_HW, LBL_Y0, LBL_Y1 = 44.0, ccy - 20.0, ccy + 27.0
    label = box(ccx - LBL_HW, ccx + LBL_HW, LBL_Y0, LBL_Y1, cz, cz + 0.15).edges("|Z").fillet(2.0)
    holes = None
    for sx in (-1, 1):
        hole = cq.Workplane("XY").workplane(offset=cz - 0.1).center(ccx + sx * HUB_DX, hub_y).circle(6.6).extrude(0.4)
        holes = hole if holes is None else holes.union(hole)
    window = box(ccx - 12.0, ccx + 12.0, hub_y - 5.5, hub_y + 5.5, cz - 0.1, cz + 0.3).edges("|Z").fillet(2.0)
    label = label.cut(holes).cut(window)
    m.add("cassette_label", label, CASS_LBL)

    # The same openings go down through the shell's top, so the hubs and the
    # tape under them show.
    sink = None
    for sx in (-1, 1):
        s = cq.Workplane("XY").workplane(offset=cz - 2.2).center(ccx + sx * HUB_DX, hub_y).circle(6.6).extrude(3.0)
        sink = s if sink is None else sink.union(s)
    sink = sink.union(box(ccx - 12.0, ccx + 12.0, hub_y - 5.5, hub_y + 5.5, cz - 2.2, cz + 0.8).edges("|Z").fillet(2.0))
    m.parts[-2].solid = m.parts[-2].solid.cut(sink)

    stripe = (box(ccx - LBL_HW, ccx + LBL_HW, LBL_Y1 - 9.0, LBL_Y1 - 5.0, cz + 0.15, cz + 0.2))
    m.add("cassette_stripe", stripe, CASS_STRIPE)
    # Where a title is written on the label: across it, below the spindle
    # holes, two rows deep. Metadata only -- the scene writes the tape's name
    # here, in the upper row when it fits on one line. The writing lines are
    # ruled where each row's letters stand, a little over a quarter of the
    # row up from its foot.
    TITLE_Y0, TITLE_Y1 = LBL_Y0 + 0.5, hub_y - 7.0
    ROW_H              = (TITLE_Y1 - TITLE_Y0) / 2
    lines = None
    for row_top in (TITLE_Y1, TITLE_Y1 - ROW_H):
        ly = row_top - 0.72 * ROW_H
        ln = box(ccx - LBL_HW + 4, ccx + LBL_HW - 4, ly - 0.175, ly + 0.175, cz + 0.15, cz + 0.2)
        lines = ln if lines is None else lines.union(ln)
    m.add("cassette_lines", lines, CASS_LINE)

    m.add("cassette_title_anchor",
          box(ccx - LBL_HW + 4, ccx + LBL_HW - 4, TITLE_Y0, TITLE_Y1, cz + 0.2, cz + 0.25), CASS_LINE)

    # Tape wound on each spool, seen through the window and the hub holes.
    packs = None
    for sx in (-1, 1):
        pack = (cq.Workplane("XY").workplane(offset=cz - 1.6).center(ccx + sx * HUB_DX, hub_y)
                .circle(15.0).circle(6.0).extrude(1.0))
        packs = pack if packs is None else packs.union(pack)
    m.add("cassette_tape", packs.union(box(ccx - 38.0, ccx + 38.0, ccy - CD / 2 - 0.2, ccy - CD / 2 + 0.6,
                                           cz - CT + 0.8, cz - 1.0)), TAPE)

    # The hubs: white rings with six teeth pointing in.
    hubs = None
    for sx in (-1, 1):
        hx  = ccx + sx * HUB_DX
        hub = cq.Workplane("XY").workplane(offset=cz - 1.5).center(hx, hub_y).circle(6.0).circle(4.2).extrude(1.2)
        for k in range(6):
            a = math.pi * 2 * k / 6
            tooth = (box(-0.6, 0.6, 3.0, 4.4, 0, 1.2)
                     .rotate((0, 0, 0), (0, 0, 1), math.degrees(a))
                     .translate((hx, hub_y, cz - 1.5)))
            hub = hub.union(tooth)
        hubs = hub if hubs is None else hubs.union(hub)
    m.add("cassette_hubs", hubs, HUB)

    screws = None
    for sx, sy in ((-1, -1), (-1, 1), (1, -1), (1, 1)):
        screw = (cq.Workplane("XY").workplane(offset=cz)
                 .center(ccx + sx * (CW / 2 - 4.0), ccy + sy * (CD / 2 - 4.0)).circle(1.3).extrude(0.3))
        screws = screw if screws is None else screws.union(screw)
    m.add("cassette_screws", screws, SILVER)

    # The heads and capstan, down on the well floor: under a cassette they
    # are hidden, and they show only with the deck empty.
    mech = None
    for hx in (W / 2 - 12, W / 2 + 6, W / 2 + 18):
        roller = cq.Workplane("XY").workplane(offset=top - 8.5).center(hx, dy0 + 8).circle(2.6).extrude(3.0)
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
    # The legends, as printed: RECORD in reverse -- a black block with the
    # letters cut through to the silver -- and the transport keys each led by
    # its glyph. A thin bracket ties RECORD to PLAY, the two pressed together
    # to record; short risers at each end keep its long run clear of REW and
    # FF between them.
    ly, lz = sy0 + 7, top + 0.6
    LH = 3.0
    glyph_w = LH * 0.8
    legend = None

    def tri(x, y, w, h, pointing):
        pts = [(x, y - h / 2), (x, y + h / 2), (x + w, y)] if pointing > 0 \
              else [(x + w, y - h / 2), (x + w, y + h / 2), (x, y)]
        return cq.Workplane("XY").workplane(offset=lz).polyline(pts).close().extrude(0.3)

    def glyph(kind, right_x):
        gh = LH * 0.75
        if kind == "REW":
            return tri(right_x - glyph_w, ly, glyph_w / 2, gh, -1).union(tri(right_x - glyph_w / 2, ly, glyph_w / 2, gh, -1))
        if kind == "FF":
            return tri(right_x - glyph_w, ly, glyph_w / 2, gh, 1).union(tri(right_x - glyph_w / 2, ly, glyph_w / 2, gh, 1))
        if kind == "PLAY":
            return tri(right_x - glyph_w * 0.7, ly, glyph_w * 0.7, gh, 1)
        return box(right_x - gh * 0.8, right_x, ly - gh * 0.4, ly + gh * 0.4, lz, lz + 0.3)

    tops = {}
    for i, s in enumerate(legends):
        cx = kx0 + pitch * (i + 0.5)
        if s == "RECORD":
            t = text(s, LH, cx, ly, lz)
            bb = t.val().BoundingBox()
            block = box(bb.xmin - 0.8, bb.xmax + 0.8, ly - LH / 2 - 0.5, ly + LH / 2 + 0.5, lz, lz + 0.3)
            part = block.cut(text(s, LH, cx, ly, lz - 0.1).union(text(s, LH, cx, ly, lz + 0.1)))
            tops[s] = (cx, ly + LH / 2 + 0.5)
        elif s in ("REW", "FF", "PLAY", "STOP"):
            gap = 0.8
            t = text(s, LH, cx + (glyph_w + gap) / 2, ly, lz)
            bb = t.val().BoundingBox()
            part = t.union(glyph(s, bb.xmin - gap))
            tops[s] = ((bb.xmin - gap - glyph_w + bb.xmax) / 2, ly + LH / 2)
        else:
            part = text(s, LH, cx, ly, lz)
        legend = part if legend is None else legend.union(part)

    LINE_T = 0.3
    rise_y = ly + LH / 2 + 2.4
    (rx, ry), (px, py) = tops["RECORD"], tops["PLAY"]
    bracket = (box(rx - LINE_T / 2, rx + LINE_T / 2, ry + 0.3, rise_y + LINE_T, lz, lz + 0.3)
               .union(box(px - LINE_T / 2, px + LINE_T / 2, py + 0.3, rise_y + LINE_T, lz, lz + 0.3))
               .union(box(rx - LINE_T / 2, px + LINE_T / 2, rise_y, rise_y + LINE_T, lz, lz + 0.3)))
    m.add("legend", legend.union(bracket), BAND)

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
    STEM_IN, STEM_DROP = 5.0, 10.0
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
        # The stem that drops from the key's underside into the case: 5 mm in
        # from its sides and front, running all the way to its back.
        stem = (box(kx + STEM_IN, kx + kw - STEM_IN, ky0 + STEM_IN, ky1,
                    ktop - KEY_T - STEM_DROP, ktop - KEY_T + 0.5)
                .edges().fillet(0.8))
        m.add(f"keys_{i}", key.union(hinge).union(stem), KEY, angular=0.25)
    # THE CARRY HANDLE, polished chrome: a bar across the front, standing
    # proud of the front face by its own depth, on two arms that run back
    # along the sides into the case.
    #
    # ONE SOLID, made from one side profile pushed across the whole width --
    # the stretch between the arms runs inside the case and never shows. Built
    # from separate bar and arm pieces it showed a seam where they met.
    #
    # Both ends of the profile close the same way: top and bottom curve toward
    # each other, leaving the straight edges with no kink, into a blunt rounded
    # tip -- over 3 mm at the front, and as a longer shovel-like nose at the
    # back of each arm, whose taper starts directly below the door's front
    # edge. Every edge is then rounded over.
    HANDLE_Z0, HANDLE_Z1 = 30.0, 37.0
    zc                   = (HANDLE_Z0 + HANDLE_Z1) / 2
    ARM_T                = 2.0
    FRONT_Y, FRONT_RUN   = 2.0, 3.0
    NOSE_Y, NOSE_RUN     = dy0, 9.0

    # Each end is half an ellipse, which meets the straight top and bottom
    # with no kink. EXACT CURVES, NOT SPLINES: a mirror shows every ripple
    # in its surface, and the splines this was first drawn with, and the
    # fillets laid over them, came out wavy enough to read as a texture.
    half  = (HANDLE_Z1 - HANDLE_Z0) / 2
    span  = W + 2 * ARM_T
    y0    = FRONT_Y + FRONT_RUN

    def end_cap(cy, ry):
        return (cq.Workplane("YZ").workplane(offset=-ARM_T)
                .center(cy, zc).ellipse(ry, half).extrude(span))

    bar    = box(-ARM_T, W + ARM_T, y0, NOSE_Y, HANDLE_Z0, HANDLE_Z1)
    handle = bar.union(end_cap(y0, FRONT_RUN)).union(end_cap(NOSE_Y, NOSE_RUN))

    # Round over the edges at the two ends of the bar, where the profile
    # meets the side faces. The lengthwise seams are tangent and need none.
    handle = handle.edges("not |X").fillet(0.8)

    # Tessellated finely: a mirror shows its normals directly, and at the
    # default tolerance the curved ends came out lumpy.
    m.add("chrome_handle", handle, CHROME, tolerance=0.02, angular=0.08)

    # THE SHELL'S SEAM: a 2 mm channel where its top and bottom halves meet,
    # level with the arms' tips, from one tip round the back to the other.
    # The groove's inner wall is rounded at the back corners to follow the
    # case's own rounding, or the groove runs out where the corners curve away.
    seam_y = NOSE_Y + NOSE_RUN
    inner  = box(1.0, W - 1.0, seam_y - 1.0, D - 1.0, zc - 2.0, zc + 2.0)
    inner  = inner.edges("|Z").edges(cq.selectors.BoxSelector((-1, D - 2, 0), (W + 1, D, H))).fillet(EDGE_R - 1.0)
    m.parts[0].solid = (m.parts[0].solid
                        .cut(box(-1.0, W + 1.0, seam_y, D + 1.0, zc - 1.0, zc + 1.0).cut(inner)))

    # THE TONE AND VOLUME THUMBWHEELS, in a cutout taken out of the sloping
    # chin under the handle: 5 mm in from the sides and from the handle, back
    # to an upright wall straight above where the chin meets the bottom. Each
    # is a knurled disc lying flat behind its window in that wall, so a thumb
    # rolls it left and right; an embossed arrow above each window points down
    # at it, and the legend runs under both.
    #
    # Built in the wall's own frame -- x across, z up it, -y out of it -- then
    # moved onto it.
    CHIN_INSET = 5.0
    CHIN_TOP   = FACE_Z0 - 2.0           # clear of the slope's top edge: a cut level with it leaves a broken solid
    chin = box(CHIN_INSET, W - CHIN_INSET, -1.0, BOT_Y, -1.0, CHIN_TOP)
    m.parts[0].solid = m.parts[0].solid.cut(chin)

    # Round over the cutout's edges: every edge the cut left, but not the
    # case's own bottom edge, which the cutout runs out through.
    CUT_R    = 1.0
    cut_zone = cq.selectors.BoxSelector((CHIN_INSET - 0.5, -1.0, 0.5),
                                        (W - CHIN_INSET + 0.5, BOT_Y + 0.5, CHIN_TOP + 0.5))
    m.parts[0].solid = m.parts[0].solid.edges(cut_zone).fillet(CUT_R)

    origin = (BOT_Y, CHIN_TOP / 2)

    def on_face(solid):
        return solid.translate((0, origin[0], origin[1]))

    REC_D                  = 0.0
    WIN_HW, WIN_V0, WIN_V1 = 12.0, -2.0, 5.0
    WHEEL_R, TEETH         = 11.0, 72
    wheel_x = (W / 2 - 22.0, W / 2 + 22.0)

    pockets = None
    for wx in wheel_x:
        pocket = box(wx - WIN_HW, wx + WIN_HW, REC_D - 0.1, REC_D + 2 * WHEEL_R + 1, WIN_V0, WIN_V1)
        pockets = pocket if pockets is None else pockets.union(pocket)
    m.parts[0].solid = m.parts[0].solid.cut(on_face(pockets))

    # The windows are lined in black, so the wheels stand out against them.
    liners = None
    for wx in wheel_x:
        outer = box(wx - WIN_HW + 0.05, wx + WIN_HW - 0.05, REC_D + 0.3, REC_D + 2 * WHEEL_R + 0.95,
                    WIN_V0 + 0.05, WIN_V1 - 0.05)
        inner = box(wx - WIN_HW + 0.3, wx + WIN_HW - 0.3, REC_D, REC_D + 2 * WHEEL_R + 0.7,
                    WIN_V0 + 0.3, WIN_V1 - 0.3)
        liner = outer.cut(inner)
        liners = liner if liners is None else liners.union(liner)
    m.add("wheel_wells", on_face(liners), (0.02, 0.02, 0.02))

    # Each wheel's rim stands a few millimeters proud of its window. The
    # volume wheel is its own part, so the scene can turn it and the pointer
    # can take hold of it; the tone wheel is part of the case.
    WHEEL_PROUD = 2.5
    for wx, part in zip(wheel_x, ("wheels", "volume_wheel")):
        pts = []
        for k in range(TEETH * 2):
            a = math.pi * 2 * k / (TEETH * 2)
            r = WHEEL_R if k % 2 == 0 else WHEEL_R - 0.7
            pts.append((wx + r * math.cos(a), REC_D - WHEEL_PROUD + WHEEL_R + r * math.sin(a)))
        wheel = (cq.Workplane("XY").workplane(offset=WIN_V0 + 1.0)
                 .polyline(pts).close().extrude(WIN_V1 - WIN_V0 - 2.0))
        m.add(part, on_face(wheel), KEY, angular=0.25)

    # The volume wheel's position mark: a line across its rim in the same
    # silver as the arrow over it, turning with it. Placed where the wheel
    # sits at silent -- 60 degrees left of front -- so it sweeps to 60 degrees
    # right at full (the scene turns the wheel a third of a turn over the
    # range), always in the window.
    MARK_DEG = -90.0 - 60.0
    vx, vy   = wheel_x[1], REC_D - WHEEL_PROUD + WHEEL_R
    mark = (box(WHEEL_R - 1.2, WHEEL_R + 0.08, -0.4, 0.4, WIN_V0 + 0.95, WIN_V1 - 0.95)
            .rotate((0, 0, 0), (0, 0, 1), MARK_DEG)
            .translate((vx, vy, 0)))
    m.add("volume_wheel_mark", on_face(mark), SILVER)

    def face_text(s, x, v, size):
        return text(s, size, x, v, -REC_D).rotate((0, 0, 0), (1, 0, 0), 90)

    marks = None
    for wx, s in zip(wheel_x, ("LOW — TONE — HIGH", "MIN — VOLUME — MAX")):
        arrow = (cq.Workplane("XZ").workplane(offset=-REC_D)
                 .polyline([(wx - 1.0, WIN_V1 + 2.6), (wx + 1.0, WIN_V1 + 2.6), (wx, WIN_V1 + 1.0)])
                 .close().extrude(0.3))
        label = face_text(s, wx, WIN_V0 - 3.5, 2.6)
        mark  = arrow.union(label)
        marks = mark if marks is None else marks.union(mark)
    m.add("wheel_legend", on_face(marks), SILVER)
    return m


if __name__ == "__main__":
    nv, nt = build().emit("CassetteRecorder.mesh", "CassetteRecorder.mtl", "CassetteRecorder.mtl")
    print(f"{nv} vertices, {nt} triangles")
