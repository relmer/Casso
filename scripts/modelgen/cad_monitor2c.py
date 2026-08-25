"""Apple Monitor //c (order A2M4090, model G0905) as a real solid model.

241.3 x 259.1 x 185.4 mm (W x D x H) -- 9.5 x 10.2 x 7.3 inches, the published
figures for the 9-inch green-phosphor monitor Apple shipped with the //c. X
right, Y back, Z up; the case's front plane is y = 0 and the bezel stands
proud of it.

MODELED FROM PHOTOGRAPHS -- a straight-on front, a rear three-quarter, and two
side elevations. See specs/018-3d-desk-scene/reference/README.md.

The case is TWO MASSES. The front one carries the tube: full width and height,
rounded on every edge, about two fifths of the depth. The rear one is narrower
and much shorter, sits on the same base line, and is louvered along both
flanks and across its lid, with a molded carrying handle at the very back.
What joins them is a step, not a taper.

THE FRONT'S WHOLE READ IS THAT IT HAS NO EDGES. A bezel plate stands proud of
the case and rolls back to it through a quarter-round that carries all the way
around the perimeter -- and, crucially, THROUGH THE CORNERS: the roll's
sections are rounded rectangles whose corner radius shrinks with their inset,
so the left flank's angle sweeps into the top's across the corner instead of
meeting it at a crease. Lofting sharp rectangles, which is what this generator
used to do, put a hard diagonal at each of the four corners; nothing on the
real molding does that.

The screen recess is a lofted CUT from the plate hole back to the opening, so
the inner lip's slope is the cut's own wall, the interior corners are curves
by construction, and the opening is genuinely open. The overhang is SHALLOW --
the tube sits close behind the bezel on this monitor.

The scene stamps the cassowary onto the chin and derives the display sphere
from the glass mesh, so s_kBrand* in DeskSceneModel.cpp are tied to the plate
plane and the chin band here.
"""

import math

import cadquery as cq
from cadkit import KD, Model, round_rect_wire, sag_sheet

# ---------------------------------------------------------------- dimensions

INCH = 25.4

W, D, H = 9.5 * INCH, 10.2 * INCH, 7.3 * INCH

# How much of the depth the tube's housing takes before the case steps down to
# the electronics box behind it.
FRONT_D = 0.47 * D

R_CASE  = 16.0                    # the front mass's corner radius, in the face

# THE BEZEL ANGLES FORWARD, and it has to do so over most of its width or it
# does not read as angled at all. It was a 10 mm roll onto 27 mm of flat land,
# which is a flat bezel with a rounded edge. Half again as wide and half again
# as much rise gives a band you can see leaning toward you, with just enough
# land left around the opening to sit the marks on.
SHOULDER = 14.0                   # how far the bezel plate is inset from the case
RISE     = 8.0                    # how far it stands proud of the case front
CAP_STEPS = 7                     # facets in the roll-over from plate to case

# THE OPENING IS SET BY ITS BORDERS, not by a picture aspect. The frame reads
# as a frame when its left, right and top bands are the same width -- so those
# are the numbers, and the aperture is whatever they leave. Sized from a 4:3
# tube instead, the side bands came out half again as wide as the top and the
# whole front looked like a letterbox.
#
# The aperture is wider than 4:3 as a result, which is fine and is what the
# photograph shows: what you see inside the bezel is the tube's whole glass,
# black mask included, and the scene fits the picture inside that.
BAND = 26.0                       # left, right and top -- all one width
CHIN = 34.0                       # deeper, because the marks live in it

OX0, OX1 = BAND, W - BAND
OZ0, OZ1 = CHIN, H - BAND
SCR_W, SCR_H = OX1 - OX0, OZ1 - OZ0
R_OPEN   = 9.0                    # the opening's own corners: softened, not rounded off

LIP      = 5.0                    # the recess mouth, out from the opening
GLASS_IN = 5.0                    # glass inset from the opening

# THE TUBE'S CURVATURE, and the plane that follows from it.
#
# The sheet is a spherical cap, so its crown stands `SAG` in front of its rim,
# and where the RIM goes decides whether the glass sits in the bezel or bursts
# out of it. At the old three-times-half-diagonal radius the sag was 16.8 mm
# on a screen this size and the crown ended up a centimeter PROUD of the
# plate -- the tube poking through its own front panel. It went unseen because
# the number was set against a bigger opening and never re-derived.
#
# So the rim is SOLVED rather than picked: put the crown exactly on the plate
# face and let the rim fall where it must. The bezel then overhangs the
# glass's edge and the middle comes up flush, which is what a tube in a bezel
# does. Move the plate or the screen and this follows.
SAG_SCALE = 5.8                   # sphere radius, in half-diagonals

_GLASS_HALF = math.hypot((SCR_W - GLASS_IN * 2.0) * 0.5,
                         (SCR_H - GLASS_IN * 2.0) * 0.5)
_GLASS_R    = SAG_SCALE * _GLASS_HALF
SAG         = _GLASS_R - math.sqrt(_GLASS_R * _GLASS_R - _GLASS_HALF * _GLASS_HALF)

GLASS_Y = -RISE + SAG             # the rim: crown lands on the plate face

# The box behind. Narrower and much shorter than the tube housing, sharing its
# base line, and reaching back to the full depth.
REAR_W   = 190.0
REAR_H   = 112.0
R_REAR   = 14.0
REAR_Y0  = FRONT_D - 25.0         # overlapped into the front mass, so the union solids
REAR_X0  = (W - REAR_W) * 0.5

# Louvers: along the depth on both flanks, across the width on the lid.
LOUV_W    = 2.2                   # slot width
LOUV_DEEP = 2.0
LOUV_PITCH = 5.4                  # centre to centre

# The handle pocket at the very back of the lid. Named up here because the lid
# louvers have to stop short of it, and a count that does not follow the
# pocket is one that grows through it the moment the case gets deeper.
GRIP_Y0, GRIP_Y1 = D - 46.0, D - 24.0
GRIP_INSET       = 44.0
GRIP_DEEP        = 14.0

PLAT     = (0.870, 0.862, 0.835)
CAVITY   = (0.055, 0.055, 0.062)
LAMPRING = (0.045, 0.045, 0.050)

m = Model()


def case_wire(y, inset, radius):
    """The case outline at depth y, drawn in from the full section by `inset`.
    The radius shrinks with the inset so the band between two of these is a
    constant width the whole way round, corners included."""
    return round_rect_wire(y, inset, W - inset, inset, H - inset, radius)


# -------------------------------------------------------------------- shell

# The roll-over from the proud plate to the case, as a quarter ellipse: at the
# plate the surface faces the viewer, at the case it runs parallel to the
# flank. Stepped rather than splined because a ruled loft through sections
# this close is predictable, and predictable is worth more here than exact --
# under flat shading a handful of facets across a 5 mm roll reads as round.
cap_sections = []

for step in range(CAP_STEPS):
    t     = step / float(CAP_STEPS - 1)
    angle = t * math.pi * 0.5
    inset = SHOULDER * (1.0 - math.sin(angle))

    cap_sections.append(case_wire(-RISE * math.cos(angle), inset, R_CASE - inset))

# ...AND THE TUBE HOUSING IS THE SAME LOFT, not a second solid unioned onto
# it. The roll's last section and the housing's first are the same rectangle
# in the same plane, and abutting two solids on coincident faces gives OCC an
# invalid result -- it unions without complaint and then every boolean after
# it quietly misbehaves. The tell was a cut that RAISED the volume. Run the
# section straight back instead and there is one solid, made once.
cap_sections.append(case_wire(FRONT_D, 0.0, R_CASE))

shell = cq.Workplane(obj=cq.Solid.makeLoft(cap_sections, True))

# The tube housing's own back rim, rounded BEFORE the box goes on. Selecting
# it afterwards means asking for every edge near that plane, which by then is
# two concentric rings -- the rim and the seam the box cuts into it -- and
# filleting the pair returns a solid OCC reports as INVALID. Nothing throws;
# the booleans after it just quietly stop working, and the screen recess ends
# up not cut. Fillet it while it is still the only thing there.
shell = shell.edges(">Y").fillet(5.0)

# The electronics box behind it. It overlaps well into the housing, so the
# union has no coincident faces to trip over.
shell = shell.union(
    cq.Workplane(obj=cq.Solid.extrudeLinear(
        cq.Face.makeFromWires(round_rect_wire(REAR_Y0, REAR_X0, REAR_X0 + REAR_W,
                                              0.0, REAR_H, R_REAR)),
        cq.Vector(0.0, D - REAR_Y0, 0.0))))

# And the box's own back rim.
shell = shell.edges(">Y").fillet(4.0)

# THE SCREEN RECESS. Lofted from the plate's hole back to the opening, both
# rounded, so the lip is the cut's own wall and the interior corners are
# curves rather than mitres. Shallow: seven millimeters from the plate face to
# the glass, where it used to be sixteen and the tube read as sunk in a well.
shell = shell.cut(cq.Workplane(obj=cq.Solid.makeLoft(
    [round_rect_wire(-RISE - 1.0, OX0 - LIP, OX1 + LIP, OZ0 - LIP, OZ1 + LIP,
                     R_OPEN + LIP),
     round_rect_wire(GLASS_Y, OX0, OX1, OZ0, OZ1, R_OPEN)],
    True)))

shell = shell.cut(cq.Workplane(obj=cq.Solid.extrudeLinear(
    cq.Face.makeFromWires(round_rect_wire(GLASS_Y, OX0, OX1, OZ0, OZ1, R_OPEN)),
    cq.Vector(0.0, 46.0, 0.0))))

# ------------------------------------------------------------------ louvers

# Along the depth on both flanks. They stop short of the box's ends and of its
# lid, the way cooling slots in a molding do.
for i in range(13):
    z = 16.0 + i * 6.6

    for x in (REAR_X0 - 1.0, REAR_X0 + REAR_W - LOUV_DEEP + 1.0):
        shell = shell.cut(
            cq.Workplane("XY")
              .box(LOUV_DEEP, (D - 26.0) - (REAR_Y0 + 18.0), LOUV_W,
                   centered=(False, False, False))
              .translate((x, REAR_Y0 + 18.0, z)))

# Across the width on the lid, filling whatever the case leaves between the
# step at the front and the handle pocket at the back.
_lid_y0 = REAR_Y0 + 30.0
_lid_n  = int((GRIP_Y0 - 8.0 - _lid_y0) / LOUV_PITCH)

for i in range(_lid_n):
    y = _lid_y0 + i * LOUV_PITCH

    shell = shell.cut(
        cq.Workplane("XY")
          .box(REAR_W - 30.0, LOUV_W, LOUV_DEEP,
               centered=(False, False, False))
          .translate((REAR_X0 + 15.0, y, REAR_H - LOUV_DEEP)))

# The molded carrying handle: a pocket sunk into the lid at the very back,
# with the strip of case behind it left as the grip.
#
# A plain filleted box, NOT a round_rect_wire extruded upward -- that helper
# draws its wire in the XZ plane at a given depth, so extruding one along +Z
# makes a degenerate solid. Cutting with it silently emptied the whole shell:
# the generator still wrote a mesh, the part just had no triangles in it.
shell = shell.cut(
    cq.Workplane("XY")
      .box(REAR_W - GRIP_INSET * 2.0, GRIP_Y1 - GRIP_Y0, GRIP_DEEP + 10.0,
           centered=(False, False, False))
      .translate((REAR_X0 + GRIP_INSET, GRIP_Y0, REAR_H - GRIP_DEEP))
      .edges("|Z").fillet(6.0))

# THE REAR PANEL. A recessed bay across the lower back carrying the mains
# inlet, four trim controls and a screw boss -- the arrangement the rear
# three-quarter photograph shows. Nothing in this scene ever sees a monitor's
# back; it is here because the part has one.
PANEL_X0, PANEL_X1 = REAR_X0 + 16.0, REAR_X0 + REAR_W - 12.0
PANEL_Z0, PANEL_Z1 = 10.0, 42.0

shell = shell.cut(
    cq.Workplane("XY")
      .box(PANEL_X1 - PANEL_X0, 3.0, PANEL_Z1 - PANEL_Z0, centered=(False, False, False))
      .translate((PANEL_X0, D - 2.0, PANEL_Z0))
      .edges("|Y").fillet(3.0))

# The mains inlet, at the panel's left end.
shell = shell.cut(
    cq.Workplane("XY")
      .box(24.0, 12.0, 16.0, centered=(False, False, False))
      .translate((PANEL_X0 + 6.0, D - 10.0, PANEL_Z0 + 6.0))
      .edges("|Y").fillet(2.0))

# Four trim controls in a row beside it, each in its own slot.
for i in range(4):
    x = PANEL_X0 + 46.0 + i * 17.0

    shell = shell.cut(
        cq.Workplane("XY")
          .box(9.0, 8.0, 13.0, centered=(False, False, False))
          .translate((x, D - 6.0, PANEL_Z0 + 8.0))
          .edges("|Y").fillet(1.5))

m.add("shell", shell, PLAT)

# Cavity back: the dark plane behind the glass.
m.add("cavity",
      cq.Workplane("XY").box(SCR_W, 2.0, SCR_H, centered=(False, False, False))
        .translate((OX0, GLASS_Y + 1.0, OZ0)),
      CAVITY)

# --------------------------------------------------------------------- glass

# True spherical sag -- the period 9-inch tube's curvature, and the sphere the
# scene derives its input mapping from. Inset far enough to stay inside the
# opening's rounded corners.
m.add_triangles("glass",
                sag_sheet(OX0 + GLASS_IN, OX1 - GLASS_IN,
                          OZ0 + GLASS_IN, OZ1 - GLASS_IN,
                          front_y=GLASS_Y, radius_scale=SAG_SCALE),
                KD["glass"])

# --------------------------------------------------------------- power lamp

# The //c-family indicator: a tall narrow rhombus leaning right at the switch
# bar's proportions (kLedWDp 8 : kLedHDp 25) and lean (tan 0.176), extruded,
# seated in a hairline recess. Analytic triangles rather than a CAD solid --
# a sheared prism fights every workplane, and ten quads state it exactly.
#
# ALL BUT FLUSH, like the drive's: it is a matte window in the panel, not a
# bulb standing off it. The standoff the shading needs belongs to the light
# and lives there -- see kLampLightStandoffMm.


def slanted_prism(x, z0, height, width, lean, y_front, y_back):
    z1 = z0 + height
    face = [(x, z0), (x + width, z0), (x + width + lean, z1), (x + lean, z1)]
    tris = []
    f3 = [(px, y_front, pz) for px, pz in face]
    tris.append((f3[0], f3[1], f3[2]))
    tris.append((f3[0], f3[2], f3[3]))
    for i in range(4):
        ax, az = face[i]
        bx, bz = face[(i + 1) % 4]
        tris.append(((ax, y_front, az), (bx, y_front, bz), (bx, y_back, bz)))
        tris.append(((ax, y_front, az), (bx, y_back, bz), (ax, y_back, az)))
    return tris


PLATE_Y = -RISE                   # the flat band the marks sit on
LCX, LCZ = W - 44.0, 15.0
LENS_W, LENS_H = 4.2, 13.125
LEAN = LENS_H * 0.176
RECESS_RIM = 0.4

m.add_triangles("lampring",
                slanted_prism(LCX - RECESS_RIM, LCZ - RECESS_RIM,
                              LENS_H + RECESS_RIM * 2.0, LENS_W + RECESS_RIM * 2.0,
                              LEAN, PLATE_Y - 0.40, PLATE_Y),
                LAMPRING)

m.add_triangles("lamp",
                slanted_prism(LCX, LCZ, LENS_H, LENS_W, LEAN,
                              PLATE_Y - 0.35, PLATE_Y),
                KD["monitor_lamp"])


# --------------------------------------------------------------- front anchor
#
# The FRAME's front plane -- the case's own front section, behind the proud
# plate that carries the screen. The drives line up with this, not with the
# plate and not with the model origin.
m.add("front_anchor",
      cq.Workplane("XY")
        .box(0.4, 0.4, 0.4, centered=(True, True, True))
        .translate((W * 0.5, 0.0, H * 0.25)),
      KD["front_anchor"])


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "Monitor2c.mesh"),
                    os.path.join(out, "Monitor2c.mtl"), "Monitor2c.mtl")
    print(f"Monitor2c (CAD): {nv} verts, {nt} tris")
