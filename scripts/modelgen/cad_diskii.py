"""Apple Disk II drive (A2M0003) as a real solid model. 155 x 220 x 96 mm
(W x D x H), X right, Y back, Z up, front lip face at y=-3.

A faithful PORT of the hand-built generator's coordinates, not a redesign:
the desk scene stamps onto this model at fixed model-space positions -- the
DRIVE-n text on the badge plaque, the IN-USE label beside the LED, the
write-protect padlock, the eject/body region boxes -- so every feature stays
where the scene expects it. What changes is construction: the faceplate
recess is a boolean POCKET in the case rather than four proud slabs faking
one, and the case edges carry real fillets (the real unit is mostly square
with a slight rounding all round).

Sub-mesh identity is by Kd VALUE (DeskSceneModel::kKdEpsilon): the door bar
and latch tab carry the door identities so the scene swings them on eject,
and the LED carries the drive-lamp identity so it lights and glows.
"""

import math

import cadquery as cq
from cadkit import KD, Model

# ---------------------------------------------------------------- dimensions

W, H, D = 155.0, 96.0, 220.0

# Faceplate features were laid out against an 86 mm case; they are placed as
# fractions of the real height so the front keeps its proportions.
FZ = H / 86.0

# The black face runs the FULL front. Nothing beige borders it -- the metal
# wrap is the only thing outside the black, which is why this is 0 rather
# than the 7 mm margin the front carried while the case was one plastic box.
# A beige surround that wide was the single loudest wrong note on the front:
# it read as a plastic picture-frame the real drive does not have.
LIP      = 0.0
POCKET_D = 2.0                    # how far the black plate sits behind the front

SLOT_Z0, SLOT_Z1 = 46.0 * FZ, 52.0 * FZ
SLOT_X0, SLOT_X1 = 14.0, W - 14.0

PLATE_Y = -1.0                    # the black drive face

# ------------------------------------------------------------- the slot frame
#
# A pronounced rectangle around the disk opening, standing proud of the face:
# a flat band, then a 45-degree bevel falling back to the slot on all four
# sides. The bevel is most of what the front reads as -- it catches a
# highlight the whole way round the opening, which a square-cut slot cannot.
FRAME_PROUD = 1.5                 # how far the frame stands off the face
FRAME_FLAT  = 2.0                 # the flat band's width, in the X/Z plane
BEVEL_LEN   = 2.5                 # measured ALONG the slope, not projected
BEVEL_RUN   = BEVEL_LEN / math.sqrt (2.0)     # so 1.768 in each of Y and X/Z

FRAME_Y  = PLATE_Y - FRAME_PROUD
FRAME_X0 = SLOT_X0 - (FRAME_FLAT + BEVEL_RUN)
FRAME_X1 = SLOT_X1 + (FRAME_FLAT + BEVEL_RUN)
FRAME_Z0 = SLOT_Z0 - (FRAME_FLAT + BEVEL_RUN)
FRAME_Z1 = SLOT_Z1 + (FRAME_FLAT + BEVEL_RUN)

# ------------------------------------------------------------------ the door
#
# The door wears the FACE'S OWN CONTOUR: flush with the plate above the frame,
# stepped proud where it crosses it. That proud band is the handle -- there is
# no separate latch. It ends at the frame's bottom edge, and it is solid where
# the slot would be, since a door with a slot in it would not hold a disk in.
DOOR_W  = 39.0
DOOR_T  = 2.0
DOOR_X0 = (W - DOOR_W) * 0.5
DOOR_Z0 = FRAME_Z0
DOOR_Z1 = 84.4
DOOR_BACK = PLATE_Y + DOOR_T      # both steps share one back plane

# The finger notch behind the door: a wedge, flush with the face at its bottom
# and NOTCH_DEEP at the top, which is where the door comes to rest. It cuts
# the frame and the face alike, so an open door leaves a gap in the frame.
NOTCH_W    = DOOR_W + 1.0         # a hair wider, so the door never binds
NOTCH_DEEP = 38.0
NOTCH_WALL = 1.0                  # thickness of the dark lining around it

# The notch runs BELOW the slot, down to the in-use lamp's bottom edge -- the
# recess is what your fingers go into to pull a diskette out, so it has to
# reach below the opening rather than stopping at the door. Bottom-aligned
# with the lamp, which is the line the faceplate already has there.
LED_R      = 1.55
# 3.0 threw too wide an aura: the standoff is what gives the faceplate its
# dot(n, L), so the protrusion and the size of the pool are the same number.
LED_PROUD  = 2.0
LED_BARREL = LED_PROUD - LED_R
LED_X, LED_Z = 45.0, 28.9

NOTCH_Z0   = LED_Z - LED_R

# The diskette slot proper: a void running back into the drive's core, deep
# enough that its far end goes dark and a diskette's back edge has somewhere
# to sit.
SLOT_DEPTH = 34.0
SLOT_WALL  = 1.2                  # thickness of the black lining around it

# ------------------------------------------------------------- the enclosure
#
# The metal case is one wrapped sheet around the drive body plus a separate
# plate closing the rest of the bottom.
#
# It OVERHANGS the front, so the black face sits down inside a shallow metal
# lip instead of flush with it. That lip is the ONLY thing bordering the black
# face -- no plastic shows around it at all -- and it catches light the whole
# way round, which a flush joint cannot.
#
# The bottom being TWO pieces is the detail worth having. The wrap's flange
# ends stop short of each other and a separate plate fills the span between
# them, sitting BOTTOM_DROP lower, so the two seams read from the front as a
# pair of fine gaps in the bottom edge.
#
SHELL_T     = 1.25                # sheet thickness
SHELL_PROUD = 0.75                # how far the metal stands forward of the face
BOTTOM_DROP = 1.25                # how far the bottom plate hangs below the wrap
SEAM_GAP    = 0.70                # the visible gap where wrap meets bottom plate

# The wrap's flanges reach SEAM_INSET in from each outer side, so the separate
# plate covers the BULK of the bottom rather than a strip down the middle.
# Measured from the metal's own outer faces, since that is the edge a ruler
# laid across the real drive starts from.
INCH        = 25.4
SEAM_INSET  = 0.8 * INCH

SEAM_L      = -SHELL_T + SEAM_INSET
SEAM_R      = W + SHELL_T - SEAM_INSET

# The metal stands proud of the BLACK FACE, which is the drive's front now
# that no plastic borders it.
SHELL_Y0    = PLATE_Y - SHELL_PROUD

BEIGE    = (0.833, 0.784, 0.659)
BEIGE_DK = (0.760, 0.710, 0.590)
# Painted sheet metal: the same beige the plastic is meant to match, a shade
# cooler and darker because it never quite does -- which is exactly what makes
# the join between the two visible on the real drive.
SHELL    = (0.806, 0.762, 0.648)
PLATE    = (0.100, 0.100, 0.110)
SLOT_DK  = (0.035, 0.035, 0.045)
BADGE    = (0.900, 0.870, 0.780)
FOOT     = (0.150, 0.140, 0.130)

m = Model()

# --------------------------------------------------------------------- case

# The drive BODY. Nothing of it shows from the front any more -- the black
# face covers the whole front and the metal covers everything outside that --
# but it is still what the notch is cut into and what the metal wraps, so it
# stays a solid rather than becoming a shell that would have nothing to hold
# the finger notch.
case = (cq.Workplane("XY")
        .box(W, D + 3.0, H, centered=(False, False, False))
        .translate((0.0, -3.0, 0.0))
        .edges("|Y").fillet(1.8)
        .edges("|X").fillet(1.2))

# The pocket reaches 4 mm INTO the case body, not just through the proud
# lip: the black plate solid lives at y -1..0, and a shallower cut leaves
# solid beige coincident with it -- the plate ends up buried inside case
# material and the front renders beige.
case = case.cut(
    cq.Workplane("XY")
      .box(W - LIP * 2.0, 5.0, H - LIP * 2.0, centered=(False, False, False))
      .translate((LIP, -4.0, LIP)))

# The finger notch: a wedge running back into the drive, nothing deep at the
# frame's bottom edge and NOTCH_DEEP where the door comes to rest. Cut from
# the case AND the plate -- cutting only the case would leave the black plate
# spanning the opening, and the notch would render as a painted rectangle.
# The FLOOR slopes; the mouth does not. At the bottom the floor is the drive
# face itself -- which is why an open door leaves a gap in the frame, the
# frame being proud of that face -- and it falls back to NOTCH_DEEP where the
# door comes to rest. Measured from the face, not from the frame.
# THE RAMP STARTS BEHIND THE DOOR, not at the face. Starting it at the face
# put it INSIDE the door slab for the first 1.9 mm of the door's height --
# the ramp climbs to y +1.5 by z 50 while the door occupies y -1..+1 -- and
# two solids sharing a volume is what drew the thin triangle across the
# closed door's bottom. The door needs somewhere to BE, so the pocket it
# retracts into has to begin behind it.
# DRAWN ON "YZ", NOT "XZ". The profile's two coordinates are DEPTH and
# HEIGHT, and on an XZ workplane the first one is read as global X -- so the
# wedge was built in the plane of the drive's FACE instead of in depth, and
# its sloped edge ran from (x 1, z 47.6) to (x 37, z 84.4) as a diagonal
# straight across the faceplate. That was the triangle that had been sitting
# on the door and, with the door open, on the face behind it.
#
# On YZ the local axes are global Y then Z, which is what the profile means,
# and the extrude runs along X to sweep the notch across the door's width.
def notch_wedge (pad):
    """The notch profile, optionally grown by `pad` on every closed side.

    The FRONT stays put whatever the pad: the notch is open to the world, and
    growing that edge would push the mouth out past the faceplate. Everything
    else -- floor, top, both flanks -- moves out, which is what makes the
    padded copy a lining shell once the true notch is cut back out of it.
    """
    return (cq.Workplane("YZ")
            .polyline ([(FRAME_Y,                        NOTCH_Z0 - pad),
                        (DOOR_BACK + pad,                NOTCH_Z0 - pad),
                        (PLATE_Y + NOTCH_DEEP + pad,     DOOR_Z1 + pad),
                        (FRAME_Y,                        DOOR_Z1 + pad)])
            .close()
            .extrude (NOTCH_W + pad * 2.0)
            .translate (((W - NOTCH_W) * 0.5 - pad, 0.0, 0.0)))


notch = notch_wedge (0.0)

# The cavity the CASE gives up is the padded one, not the notch. The lining
# has to occupy real space: cutting the case to the notch's own size left the
# liner sharing a volume with case material that was still there, so the
# beige won and the pocket kept reading as bare case with an open door.
#
# So the case loses notch + wall, the liner fills that wall back in, and what
# is finally open is the notch itself.
notch_outer = notch_wedge (NOTCH_WALL)

# The slot's void, defined HERE because it has to be taken out of the case
# and the notch liner as well as shown as a lining -- a cavity that is only
# subtracted from the thing lining it is still solid drive behind.
slot_cavity = (cq.Workplane("XY")
               .box(SLOT_X1 - SLOT_X0, SLOT_DEPTH, SLOT_Z1 - SLOT_Z0,
                    centered=(False, False, False))
               .translate((SLOT_X0, FRAME_Y + BEVEL_RUN, SLOT_Z0)))

case = case.cut (notch_outer).cut (slot_cavity)

m.add("case", case, BEIGE)

# ---------------------------------------------------------------- enclosure

# The wrap: a tube around the body, open at both ends. Hollowed with the
# body's OWN volume rather than an inset box, so the metal is exactly
# SHELL_T everywhere by construction and cannot drift out of true if the
# body's dimensions change.
#
# The outer corners are filleted by the body's radius PLUS the sheet
# thickness, which is what an offset surface actually does -- filleting both
# to the same radius would leave the metal thin at the corners.
#
# Only the VERTICAL edges are rounded. The horizontal ones are left crisp on
# purpose: a fillet along the front edge would eat the proud lip, which is
# 1.25 mm of material and the whole point of the overhang.
shell_outer = (cq.Workplane("XY")
               .box(W + SHELL_T * 2.0, D - SHELL_Y0, H + SHELL_T * 2.0,
                    centered=(False, False, False))
               .translate((-SHELL_T, SHELL_Y0, -SHELL_T))
               .edges("|Y").fillet(1.8 + SHELL_T))

# The INNER corner is rounded too, by the outer radius LESS the sheet
# thickness. Bent metal keeps its thickness through a turn -- it cannot gain
# any -- so a sharp-cornered cavity inside a filleted shell was quietly
# thickening the wrap at all four corners, exactly where a real case is at
# its most obviously constant.
#
# 1.8 is also the body's own fillet, which is not a coincidence: the metal
# wraps that body, so its inner surface is the body's outer surface.
shell_cav = (cq.Workplane("XY")
             .box(W, D - SHELL_Y0 + 2.0, H, centered=(False, False, False))
             .translate((0.0, SHELL_Y0 - 1.0, 0.0))
             .edges("|Y").fillet(1.8))

shell = shell_outer.cut (shell_cav)

# Take the bottom plate's span out of the wrap. The cut runs well below the
# sheet so it clears the plate at its dropped height too -- the two must not
# be left sharing a face, or the seam disappears into a coincident surface
# and the join stops reading at any angle.
bottom_span = (cq.Workplane("XY")
               .box(SEAM_R - SEAM_L, D - SHELL_Y0 + 2.0, SHELL_T + BOTTOM_DROP + 2.0,
                    centered=(False, False, False))
               .translate((SEAM_L, SHELL_Y0 - 1.0, -SHELL_T - BOTTOM_DROP - 2.0)))

m.add("shell", shell.cut (bottom_span), SHELL)

# The separate bottom plate, narrower than the span it fills by SEAM_GAP at
# each end. Those two slots ARE the gaps -- they are what shows from the
# front, and they exist because the piece is a different piece, not because
# anything was drawn on.
m.add("shell_bottom",
      cq.Workplane("XY")
        .box((SEAM_R - SEAM_GAP) - (SEAM_L + SEAM_GAP), D - SHELL_Y0, SHELL_T,
             centered=(False, False, False))
        .translate((SEAM_L + SEAM_GAP, SHELL_Y0, -SHELL_T - BOTTOM_DROP)),
      SHELL)

# The black faceplate filling the pocket floor, a hair behind the lip.
plate = (cq.Workplane("XY")
         .box(W - LIP * 2.0, 2.0, H - LIP * 2.0, centered=(False, False, False))
         .translate((LIP, PLATE_Y, LIP)))

# Two finishes, one plastic. Above the frame's bottom edge the face carries
# the molded pebble grain; below it, the same black in a smooth matte. The
# split is the frame's own bottom, so the two never disagree about where the
# boundary is.
plate_hi = (cq.Workplane("XY")
            .box(W - LIP * 2.0, 2.0, (LIP + H - LIP * 2.0) - FRAME_Z0, centered=(False, False, False))
            .translate((LIP, PLATE_Y, FRAME_Z0)))

m.add("plate_pebbled", plate.intersect (plate_hi).cut (notch), KD["plate_pebbled"])
m.add("plate",         plate.cut (plate_hi).cut (notch),        PLATE)

# ------------------------------------------------- faceplate furniture

# The slot frame: a slab over the frame rectangle with the mouth LOFTED
# through it -- wide at the face, narrowing to the slot one bevel-run back.
# Built as the shape it is rather than chamfered on afterwards, because the
# chamfer would have to be picked out of an edge selection and a mis-picked
# edge is exactly the kind of failure that renders as "looks a bit off".
slot_cx = (SLOT_X0 + SLOT_X1) * 0.5
slot_cz = (SLOT_Z0 + SLOT_Z1) * 0.5
slot_w  = SLOT_X1 - SLOT_X0
slot_h  = SLOT_Z1 - SLOT_Z0

frame = (cq.Workplane("XY")
         .box(FRAME_X1 - FRAME_X0, PLATE_Y - FRAME_Y + 0.6, FRAME_Z1 - FRAME_Z0,
              centered=(False, False, False))
         .translate((FRAME_X0, FRAME_Y, FRAME_Z0)))

mouth = (cq.Workplane("XZ", origin=(slot_cx, FRAME_Y, slot_cz))
         .rect(slot_w + BEVEL_RUN * 2.0, slot_h + BEVEL_RUN * 2.0)
         .workplane(offset=-BEVEL_RUN)
         .rect(slot_w, slot_h)
         .loft())

throat = (cq.Workplane("XY")
          .box(slot_w, 12.0, slot_h, centered=(False, False, False))
          .translate((SLOT_X0, FRAME_Y + BEVEL_RUN, SLOT_Z0)))

m.add("frame", frame.cut (mouth).cut (throat).cut (notch), KD["plate_pebbled"])

# The slot is a VOID cut into the core of the drive, not a dark rectangle
# painted behind the bevel. It used to be a 0.6 mm panel floating at y 0.47,
# which is inside the door's own slab -- so it both read as a flat card and
# fought the door for the same pixels.
#
# A real cavity is what makes it read as a hole: light falls off down its
# length, so an empty drive is mostly black at the back, and there is
# somewhere for the back edge of a diskette to sit when one is inserted.
#
# It is cut through the notch pocket and on into the case, since the pocket
# is only a few mm deep at the slot's height and would otherwise floor the
# opening almost immediately.
# The cavity's walls are black plastic, not the beige they would otherwise
# expose. Grown by the wall thickness and hollowed by the cavity itself, so
# the lining is exactly the surfaces the eye can see down the slot.
slot_shell = (cq.Workplane("XY")
              .box(slot_w + SLOT_WALL * 2.0, SLOT_DEPTH + SLOT_WALL,
                   slot_h + SLOT_WALL * 2.0, centered=(False, False, False))
              .translate((SLOT_X0 - SLOT_WALL, FRAME_Y + BEVEL_RUN,
                          SLOT_Z0 - SLOT_WALL)))

m.add("slot", slot_shell.cut (slot_cavity), SLOT_DK)

# The door. IDENTITY COLOR: the scene splits this out and swings it, so its
# y/z extents are part of the contract.
door = (cq.Workplane("XY")
        .box(DOOR_W, DOOR_BACK - PLATE_Y, DOOR_Z1 - FRAME_Z1, centered=(False, False, False))
        .translate((DOOR_X0, PLATE_Y, FRAME_Z1)))

# The handle band stops at the FACE, not at the door's back plane: it is the
# part that stands proud, so it has nothing behind it to occupy. Running it
# back to the door's own back plane buried it in the plate, which rendered as
# z-fighting stripes down the middle of the drive.
door = door.union (
    cq.Workplane("XY")
      .box(DOOR_W, PLATE_Y - FRAME_Y, FRAME_Z1 - FRAME_Z0, centered=(False, False, False))
      .translate((DOOR_X0, FRAME_Y, FRAME_Z0)))

# The notch's walls are black plastic, not the beige shell behind them, and
# they carry the same pebble grain as the face. Line the pocket, then cut the
# notch out of the liner so what is left IS the walls.

# THE SAME WEDGE, GROWN, MINUS THE WEDGE -- so the lining hugs the notch on
# every closed side and is exactly NOTCH_WALL thick everywhere.
#
# It was a plain BOX around the notch's bounding volume, which was already the
# wrong shape and became visibly wrong once the notch moved to the plane it
# belonged in: the pocket's walls and sloped floor were left as bare case, so
# an open door showed BEIGE where the drive's own dark interior should be.
# A box cannot line a wedge; only the wedge can.
liner = notch_outer.cut (notch).cut (slot_cavity)

m.add("notch_liner", liner, KD["plate_pebbled"])

m.add("door", door, KD["drive_door"])

# No badge plaque. The drive-number sticker is white lettering straight onto
# the black face -- a plaque under it was invention, and at this size it read
# as a raised gray slab where the real drive has nothing at all. (The "DRIVE
# A:/B:" stickers DO sit on a light patch, but those are a different label and
# we are deliberately not modeling them.)

# IN-USE LED, lower-left, with its label stamped by the scene to its left.
# PROUD of the plate. It used to sit at y -0.6 with the plate face at -1.0,
# which buried the lens inside the faceplate -- the drive has never actually
# shown its activity light, and nothing said so because an unlit LED is dark
# anyway.
# Measured off a front-on photograph: a 3 mm lens sitting just right of the
# text, not the 6.4 mm one twice that far across the face.
#
# A ROUND-TOPPED LED, barrel plus dome, standing LED_PROUD off the face --
# the shape everyone pictures. It was a flat-ended cylinder, and the flat end
# is why the drive's own faceplate took no light from it: a lens flush with
# a surface, or facing straight out of one, cannot illuminate that surface.
# The protrusion is the whole mechanism, so it is geometry rather than a
# fudge factor in the renderer.
#
# Total reach is barrel + radius, so the barrel carries the remainder and the
# dome's tip lands exactly LED_PROUD off the plate however the radius moves.
led = (cq.Workplane("XY")
       .cylinder(LED_BARREL, LED_R, direct=(0, 1, 0), centered=(True, True, False))
       .translate((LED_X, PLATE_Y - LED_BARREL, LED_Z)))

led = led.union (
    cq.Workplane("XY")
      .sphere(LED_R)
      .translate((LED_X, PLATE_Y - LED_BARREL, LED_Z)))

m.add("led", led, KD["drive_lamp"])

# ------------------------------------------------------------- lid and sides

# These all ride on the METAL now, not on the body -- the body's top and
# sides are under the wrap, and anything left at the old offsets would be
# sealed inside it. Each is pushed out by the sheet thickness.

# Lid channels: two slim darker strips running front to back.
for i, (x0, x1) in enumerate([(24.0, 62.0), (93.0, 131.0)]):
    m.add(f"channel{i}",
          cq.Workplane("XY").box(x1 - x0, D - 42.0, 0.35, centered=(False, False, False))
            .translate((x0, 18.0, H + SHELL_T - 0.01)),
          BEIGE_DK)

# Side vents: nine slits per side, rear half.
for s, (sx0, sx1) in enumerate([(-SHELL_T - 0.35, -SHELL_T + 0.1),
                                (W + SHELL_T - 0.1, W + SHELL_T + 0.35)]):
    for i in range(9):
        m.add(f"vent{s}_{i}",
              cq.Workplane("XY").box(sx1 - sx0, 3.2, 46.0 * FZ, centered=(False, False, False))
                .translate((sx0, 146.0 + i * 7.0, 20.0 * FZ)),
              BEIGE_DK)

# Rubber feet: the ground footprint the contact shadow is sized from. They sit
# under the WRAP's flanges rather than under the separate bottom plate, which
# is where the real drive puts them -- the plate is not what carries the
# weight -- so they drop by the sheet thickness, not by the plate's.
for fx in (16.0, W - 16.0):
    for fy in (18.0, D - 18.0):
        m.add(f"foot{fx:.0f}_{fy:.0f}",
              cq.Workplane("XY").cylinder(2.2, 6.0, centered=(True, True, False))
                .translate((fx, fy, -2.2 - SHELL_T)),
              FOOT)


if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "DiskII.mesh"),
                    os.path.join(out, "DiskII.mtl"), "DiskII.mtl")
    print(f"DiskII (CAD): {nv} verts, {nt} tris")
