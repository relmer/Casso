"""The 50-contact Centronics ("Champ", micro ribbon) receptacle with wire
bail locks: the low-density SCSI device connector, one part for every model
that has one.

THE DIMENSIONS are figure 7 of the SCSI-2 standard (X3.131-1994, 50-contact
shielded low-density SCSI device connector, the same as SCSI-1's alternative
2 connector), staff.uni-mainz.de/tacke/scsi/Figures/s2fg07.gif:

  - E5, the bail lock posts' spacing: 74.85.
  - E6, the D shell's outside width: 64.29 max. E9, its height: 12.04 max.
    E12, the angle of its sides from vertical: 15 degrees.
  - E10, the shell's depth in front of the flange: 9.78 max.
  - E7 and E8, the flange: 83.06 by 15.24.
  - E1, the opening inside the shell: 56.39 min.
  - E11, the contact pitch: 2.16, twenty-five contacts in each of two rows.

The insert's slot, the contacts and the bails are drawn from photographs of
the Apple Hard Disk 20SC's connectors. The plug's tongue goes into the slot,
and the contacts line its top and bottom, twenty-five along each side.
Each contact is one strip bent into an L: the long leg runs into the slot and
forms its wall, and the short leg turns up or down, away from the slot,
across the insert's face. Each bail is one open loop of wire bent into a
rounded arrowhead, its two ends hinged in the post above and below its
center, so it swings only about the vertical axis through them. BAIL_PATH
has the loop, drawn from the photograph.

Local frame: X right, Z up, the flange's front face at y = 0 looking toward
+Y (a back panel's outward direction), the wide side of the D at the top.
"""

import math

import cadquery as cq

POST_SPACING    = 74.85
SHELL_W         = 64.29
SHELL_H         = 12.04
SHELL_ANGLE     = 15.0
SHELL_OUT       = 9.78
FLANGE_W        = 83.06
FLANGE_H        = 15.24
FLANGE_T        = 1.0
OPEN_W          = 56.39
PITCH           = 2.16
CONTACTS        = 25

SHELL_WALL      = 0.6
SHELL_R         = 1.6          # the shell's outside corner radius
INSERT_BACK     = 2.0          # the insert's face behind the shell's mouth
SLOT_W          = CONTACTS * PITCH + 1.6
CONTACT_W       = 1.3
CONTACT_T       = 0.6          # the strip's thickness
TONGUE_GAP      = 2.0          # between the two rows, for the plug's tongue
SLOT_H          = TONGUE_GAP + 2.0 * CONTACT_T
SLOT_DEPTH      = 5.0
LIP             = 0.6          # the short leg past the slot's wall, across the insert's face
BEND_R          = 0.5          # the outside of the L's bend
POST_R          = 2.2
POST_OUT        = 4.0
WIRE_R          = 0.6
# The bail's loop as (outboard of the post's center, above it, the corner's
# radius), in mm, from the upper hinge round the tip to the lower hinge:
# straight lengths with every corner rounded, symmetric above and below.
BAIL_PATH       = [(0.0, 3.0, 0.0), (2.0, 3.0, 1.0), (5.0, 6.2, 2.0), (17.5, 0.0, 2.6),
                   (5.0, -6.2, 2.0), (2.0, -3.0, 1.0), (0.0, -3.0, 0.0)]
BAIL_OPEN       = 12.0         # degrees the open bail is swung out, away from the panel

METAL           = (0.760, 0.760, 0.770)
INSERT          = (0.110, 0.110, 0.115)
SLOT            = (0.030, 0.030, 0.035)
GOLD            = (0.800, 0.660, 0.300)
WIRE            = (0.800, 0.800, 0.810)


def d_outline(top, height, r, y0, y1):
    # The D: a trapezoid, wider at the top, sides SHELL_ANGLE from vertical,
    # grown from a core by `r` so outlines with the same core are concentric.
    tan       = math.tan(math.radians(SHELL_ANGLE))
    core_top  = top - 2.0 * r
    core_h    = height - 2.0 * r
    core_bot  = core_top - 2.0 * core_h * tan
    pts       = [(-core_top * 0.5, core_h * 0.5), (core_top * 0.5, core_h * 0.5),
                 (core_bot * 0.5, -core_h * 0.5), (-core_bot * 0.5, -core_h * 0.5)]

    return (cq.Workplane("XZ").polyline(pts).close().offset2D(r, "arc")
            .extrude(-(y1 - y0)).translate((0, y0, 0)))


def champ_cut(behind = 12.0):
    # The space behind the flange the connector's body takes, for a host to
    # cut from its panel.
    return d_outline(SHELL_W, SHELL_H, SHELL_R, -behind, 1.0)


def bail(side, y):
    # One bail on the post at x = side * POST_SPACING / 2, hinged at `y`:
    # BAIL_PATH with its corners rounded, swept as round wire outboard to
    # the left or right, and swung BAIL_OPEN degrees about the vertical axis
    # through its hinges, away from the panel.
    px    = side * POST_SPACING * 0.5
    pts   = [cq.Vector(px + side * u, y, z) for u, z, _ in BAIL_PATH]
    path  = cq.Wire.makePolygon(pts)

    for r in sorted({r for _, _, r in BAIL_PATH if r > 0}, reverse=True):
        at   = [cq.Vector(px + side * u, y, z) for u, z, rr in BAIL_PATH if rr == r]
        path = path.fillet(r, [v for v in path.Vertices() if any((v.Center() - a).Length < 0.01 for a in at)])

    start = path.tangentAt(0.0)
    ring  = cq.Workplane(cq.Plane(origin=pts[0], xDir=cq.Vector(0, 1, 0).cross(start), normal=start)).circle(WIRE_R)
    rod   = ring.sweep(cq.Workplane("XY").add(path), transition="round")

    for end in (pts[0], pts[-1]):
        rod = rod.union(cq.Workplane("XY").add(cq.Solid.makeSphere(WIRE_R, end)))

    return rod.rotate(cq.Vector(px, y, 0), cq.Vector(px, y, 1), side * BAIL_OPEN)


def champ50(model, prefix, place, bail_y = 2.6):
    # Add the connector's parts. `place` maps a local solid into the model.
    # The bails lie open in the plane `bail_y` in front of the flange, which
    # clears the panel when the flange is set back behind it.
    flange = (cq.Workplane("XY").box(FLANGE_W, FLANGE_T, FLANGE_H, centered=(True, False, True))
              .edges("|Y").fillet(1.0).translate((0, -FLANGE_T, 0)))
    shell  = (d_outline(SHELL_W, SHELL_H, SHELL_R, 0.0, SHELL_OUT)
              .cut(d_outline(SHELL_W - 2.0 * SHELL_WALL, SHELL_H - 2.0 * SHELL_WALL,
                             SHELL_R - SHELL_WALL, -1.0, SHELL_OUT + 1.0)))
    posts  = None

    for side in (-1, 1):
        post = (cq.Workplane("XZ").circle(POST_R).extrude(-POST_OUT)
                .translate((side * POST_SPACING * 0.5, 0, 0)))
        post = post.cut(cq.Workplane("XY").box(0.6, 1.0, POST_R * 1.6)
                        .translate((side * POST_SPACING * 0.5, POST_OUT, 0)))
        posts = post if posts is None else posts.union(post)

    face   = SHELL_OUT - INSERT_BACK
    insert = d_outline(SHELL_W - 2.0 * SHELL_WALL, SHELL_H - 2.0 * SHELL_WALL,
                       SHELL_R - SHELL_WALL, 0.0, face)
    insert = insert.cut(cq.Workplane("XY").box(SLOT_W, SLOT_DEPTH + 1.0, SLOT_H)
                        .translate((0, face - SLOT_DEPTH * 0.5 + 0.5, 0)))
    floor  = cq.Workplane("XY").box(SLOT_W, 0.2, SLOT_H).translate((0, face - SLOT_DEPTH + 0.1, 0))

    # One contact, the upper row's, as an L in profile across the strip's
    # width: the long leg on the slot's wall, the short leg up the face,
    # and the bend between them rounded inside and out.
    zi, zo = SLOT_H * 0.5 - CONTACT_T, SLOT_H * 0.5
    ell    = (cq.Workplane("YZ")
              .polyline([(face - SLOT_DEPTH, zi), (face + CONTACT_T, zi), (face + CONTACT_T, zo + LIP),
                         (face, zo + LIP), (face, zo), (face - SLOT_DEPTH, zo)]).close()
              .extrude(CONTACT_W).translate((-CONTACT_W * 0.5, 0, 0)))

    def at(y, z):
        return lambda e: abs(e.Center().y - y) < 0.01 and abs(e.Center().z - z) < 0.01

    ell = ell.edges("|X").filter(at(face + CONTACT_T, zi)).fillet(BEND_R)
    ell = ell.edges("|X").filter(at(face, zo)).fillet(BEND_R - CONTACT_T * 0.5).val()

    contacts = []
    for row in (1, -1):
        for i in range(CONTACTS):
            x = (i - (CONTACTS - 1) * 0.5) * PITCH
            c = ell.translate(cq.Vector(x, 0, 0))
            contacts.append(c if row > 0 else c.mirror("XY"))

    bails = bail(-1, bail_y).union(bail(1, bail_y))
    parts = [(f"{prefix}_flange", flange.union(shell).union(posts), METAL),
             (f"{prefix}_insert", insert, INSERT),
             (f"{prefix}_slot", floor, SLOT),
             (f"{prefix}_contacts", cq.Workplane("XY").add(cq.Compound.makeCompound(contacts)), GOLD),
             (f"{prefix}_bails", bails, WIRE)]

    for name, solid, rgb in parts:
        model.add(name, place(solid), rgb)
