"""The IEC 60320 C14 appliance inlet, one part for every model with a mains
inlet, so the inlet is the same size, depth and pin layout on each.

THE DIMENSIONS are standard sheet C14 of IEC 60320-1 (IS/IEC 60320-1:2001,
page 58, law.resource.org/pub/in/bis/S05/is.iec.60320.1.2001.pdf), taking the
middle of each tolerance:

  - The recess is 24 +0.5 wide and 16 +0.5 high, 15 +0.5 deep. The two corners
    on the earth pin's side are cut at 45 degrees, 3 +0.5 along each edge,
    with R 2 max at the ends of each cut; the other two corners are R 3
    max.
  - The pins are 2 +-0.05 wide and 4 +-0.1 high. Line and neutral are 14 +-0.2
    apart, and the earth pin is 4 +-0.2 from their line toward the cut
    corners. The pins are 12 +1 -0.5 long from the recess floor, so their ends
    are 3 below the face, and the ends are beveled.
  - The face round the recess, outline 3 on the sheet, is 30.5 by 22.5, with
    its corners following the recess's.

Local frame: X right, Z up, the face at y = 0 looking toward +Y (a back
panel's outward direction), the recess going in toward -Y. `earth_up` puts
the cut corners and the earth pin at the top; the inlet turned over puts
them at the bottom, as some equipment mounts it.
"""

import cadquery as cq

OPEN_W, OPEN_H  = 24.25, 16.25
CUT             = 3.25
CUT_R           = 1.0
CORNER_R        = 2.0
FACE_W, FACE_H  = 30.5, 22.5
DEPTH           = 15.25
PIN_W, PIN_H    = 2.0, 4.0
PIN_PITCH       = 14.0
EARTH_RISE      = 4.0
PIN_LEN         = 12.25
PIN_BEVEL       = 0.6
BODY_BACK       = 3.0          # the body's floor, behind the recess

BODY            = (0.090, 0.090, 0.095)
FLOOR           = (0.050, 0.050, 0.055)
PIN             = (0.760, 0.765, 0.780)


def outline(grow, y0, y1):
    # The recess outline grown by `grow`, extruded from y0 to y1: the cut
    # corners at the top.
    w, h = OPEN_W * 0.5, OPEN_H * 0.5
    pts  = [(-w, -h), (w, -h), (w, h - CUT), (w - CUT, h), (-w + CUT, h), (-w, h - CUT)]
    body = (cq.Workplane("XZ").polyline(pts).close()
            .extrude(-(y1 - y0)).translate((0, y0, 0)))
    low  = body.edges("|Y").edges("<Z")
    body = low.fillet(CORNER_R)
    body = body.edges("|Y").edges(">Z").fillet(CUT_R)

    if grow:
        wire = body.faces(">Y").wires().val()
        body = (cq.Workplane("XY").add(cq.Solid.extrudeLinear(
                    cq.Face.makeFromWires(wire.offset2D(grow, "arc")[0]), cq.Vector(0, -(y1 - y0), 0))))

    return body


FACE_GROW = (FACE_W - OPEN_W) * 0.5          # equal to (FACE_H - OPEN_H) / 2


def turned(solid, earth_up):
    return solid if earth_up else solid.rotate((0, 0, 0), (0, 1, 0), 180)


def c14_cut(earth_up = True, proud = 0.0, behind = 1.0):
    # The space the inlet's body takes, for the host to cut from its case:
    # the face outline from `behind` past the body's back to `proud` + 1.
    return turned(outline(FACE_GROW, -(DEPTH + BODY_BACK + behind), proud + 1.0), earth_up)


def c14(model, prefix, place, earth_up = True, proud = 0.0):
    # Add the inlet's parts. `place` maps a local solid into the model; the
    # face is `proud` mm out from the host's surface at local y = 0.
    face_y = proud
    body   = outline(FACE_GROW, face_y - DEPTH - BODY_BACK, face_y)
    body   = body.cut(outline(0.0, face_y - DEPTH, face_y + 1.0))
    floor  = outline(-0.01, face_y - DEPTH, face_y - DEPTH + 0.1)
    parts  = [(f"{prefix}_body", body, BODY), (f"{prefix}_floor", floor, FLOOR)]

    for name, x, z in (("line", -PIN_PITCH * 0.5, -EARTH_RISE * 0.5),
                       ("neutral", PIN_PITCH * 0.5, -EARTH_RISE * 0.5),
                       ("earth", 0.0, EARTH_RISE * 0.5)):
        pin = (cq.Workplane("XY").box(PIN_W, PIN_LEN, PIN_H, centered=(True, False, True))
               .faces(">Y").edges().chamfer(PIN_BEVEL)
               .translate((x, face_y - DEPTH, z)))
        parts.append((f"{prefix}_pin_{name}", pin, PIN))

    for name, solid, rgb in parts:
        model.add(name, place(turned(solid, earth_up)), rgb)
