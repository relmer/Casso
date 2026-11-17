# Contract: desk models

Model space is millimeters, X right, Y back, Z up, front face at y = 0, as
for the existing models. Each model has a generator in `scripts/modelgen/`,
output under `Resources/Models/<Name>/`, a README listing its sources, and a
preview render checked before the model is wired into the app.

## Dimensions

**Source** is `spec sheet` (Apple's Apple /// specifications), `document`
(an Apple manual, to be cited by page), or `measured` (from a photograph, as
a fraction of a documented dimension). Empty cells are not yet sourced and
block the model's generator.

### Apple /// body (`Apple3`, `cad_apple3.py`)

| Dimension | Value | Source |
|---|---|---|
| Width | 444.5 mm (17.5 in) | Apple /// Plus Owner's Guide, appendix D; specifications sheet |
| Height | 121.9 mm (4.8 in) | the same |
| Depth of the whole machine, keyboard included | 462.3 mm (18.2 in) | the same |
| Construction | cast aluminum base, molded plastic cover | the same |
| Text interlace switch (Plus) | left side, about halfway back, just above the bottom of the cover | Apple /// Plus Owner's Guide p. 16 |
| Depth of the modeled body (front of the upper body to the back) | about 0.56 of the whole depth, about 257 mm | first estimate, measured on the patent's side view (Des. 268,584, Fig. 4) |
| Top surface | flat for about 220 mm back from the front slope, then a ribbed band about 30 mm deep at the rear; the front face slopes back | first estimate, patent Fig. 2 and Fig. 4 |
| Drive bay | right half of the front face, the slot centered about 0.79 of the width from the left; door, latch and lamp positions not yet measured | first estimate, patent Fig. 3 |
| Badge | lower left of the front face, about 0.10 to 0.19 of the width from the left; size not yet measured | first estimate, patent Fig. 3 |
| Feet | four, near the corners of the base; height not yet measured | patent Fig. 7 |

The model ends at the front of the upper body. The sloped keyboard section
is not modeled; the cut face is closed with a plain surface in the case
color.

### Back panel

Each connector is modeled with its real outline and at its measured
position. Positions are from the left edge and the bottom edge of the back
panel, viewed from behind.

| Item | Form | Position | Source |
|---|---|---|---|
| Power switch | rocker | about 0.86 to 0.93 of the width, in a separate right-hand section | first estimate, patent Fig. 6 |
| Power inlet | 3-pin | about 0.76 to 0.82 of the width | first estimate, patent Fig. 6 |
| Black and white video | RCA jack | about 143 mm right of the disk connector's center | first estimate, main board photograph |
| Color video | DB-15 | about 115 mm right of the disk connector's center | first estimate, main board photograph |
| Audio | miniature phone jack | between the black and white jack and the serial connector | order only, Plus Owner's Guide Figure 2-1 |
| Port A | DB-9 | about 44 mm right of the disk connector's center | first estimate, main board photograph |
| Port B | DB-9 | about 77 mm right of the disk connector's center | first estimate, main board photograph |
| RS-232-C | DB-25 female | about 180 mm right of the disk connector's center | first estimate, main board photograph |
| Disk drive | a D connector on the Plus (a 26-pin header behind it on the board); far left, its center about 0.13 of the width from the left edge | first estimate, main board photograph and patent Fig. 6 |
| Slot openings 1 to 4 | vertical cutouts in the finned upper section, above the connector strip | about 0.38 to 0.62 of the width; the slots' pitch is about 24 mm | first estimate, patent Fig. 6 and main board photograph |
| ProFile interface card | the card's cable connector in its slot opening | | |

Order, from the rear, left to right: disk drive connector, port A, port B,
color video, black and white video, audio, serial, power plug, power switch,
with the slot openings above the strip (Apple /// Plus Owner's Guide,
Figure 2-1). Connector types are from Apple's specifications sheet. The
positions come from the Service Reference Manual's assembly drawings
(chapter 16) and photographs, and are not yet measured.

The Monitor /// weighs 16.5 pounds and rests directly on the Apple ///
(Monitor III Owner's Manual p. 4, p. 6). Its 436.6 mm width is 7.9 mm
narrower than the body's 444.5 mm.

### How the first estimates were made, and what they are worth

No dimensioned drawing of the case has been found. The estimates above are
read by eye from two sources, and are good to a few millimeters at best:

- **The design patent's orthographic views** (`Apple 3 Design Patent
  Case.pdf`, Des. 268,584: top, front, both sides, rear, bottom), as
  fractions of the overall width or depth.
- **A photograph of the Apple /// Plus main board** (`AppleIIIPlusBoardRear.jpg`),
  scaled by the standard mounting hole spacing of its D connectors
  (47.04 mm for a 25-pin shell, 33.32 mm for 15 pins, 24.99 mm for 9 pins).

**The patent drawings and Apple's stated size disagree.** Apple gives 17.5 by
18.2 by 4.8 inches. In the patent's front view the height is about 0.35 of
the width, which is about 6.1 inches at a 17.5 inch width, and in its side
view the depth is about 0.91 of the width, against 1.04 from Apple's
figures. The patent was filed in November 1980 and may be of an earlier
case, or Apple's 4.8 inches may be measured somewhere other than the top of
the rear body. A model built from either alone would be wrong by an inch in
height. This has to be resolved before `cad_apple3.py` is written, from a
measured real unit, a dimensioned Apple drawing (the Service Reference
Manual's chapter 16 has assembly drawings that have not been read), or
front and side photographs taken square-on.

Each estimate is replaced by a value measured in pixels by a script, with
the script and the source image listed in the model's README, before the
generator uses it. SC-010's 1 mm tolerance applies to the documented
dimensions; a measured dimension is listed with the tolerance its source
supports.

### Apple Monitor /// (`Monitor3`, `cad_monitor3.py`)

| Dimension | Value | Source |
|---|---|---|
| Width | 436.6 mm (17 3/16 in) | Monitor III Owner's Manual p. 6 |
| Height | 269.9 mm (10 5/8 in) | the same |
| Depth | 317.5 mm (12 1/2 in) | the same |
| Display zone | 214.3 by 160.3 mm (8 7/16 by 6 5/16 in) | the same |
| Tube | 12 in (30.5 cm) diagonal; 78 degrees horizontal, 61 vertical, 90 diagonal deflection | the same |
| Glare filter | a microporous filter in front of the tube | Monitor III Owner's Manual p. 4 |
| Front | power switch and contrast knob, right of the tube | Monitor III Owner's Manual p. 3 |
| Back | hand grip, brightness, vertical hold, vertical size, RCA video input, power cord | the same |
| Glass sag radius; case edge radii and taper | | measured from the manual's drawings and photographs |
| Base footprint | | |
| Control and badge positions | | measured from the manual's drawings |

Phosphor: green (A3M0039). The catalog entry is `AppleMonitorIII`, default
color mode green.

### Disk /// (`Disk3`, `cad_disk3.py`)

| Dimension | Value | Source |
|---|---|---|
| Width, height, depth | | |
| Door, latch, lamp positions | | |

### ProFile

From 048. Its generator's dimensions (439 by 110 by 226 mm for the 5 MB) are
checked against the body's top surface here.

## Fit rules (unit-tested on the measured metrics)

- The ProFile's footprint is inside the body's flat top area, centered in X,
  with its back aligned as on the real pair.
- The monitor's base footprint is inside the top of whatever it rests on.
- Each item's bottom is at the top surface height of the item below. No gap
  and no intersection.
- Side-row devices do not overlap the stack's bounding box.

## Color identity

Sub-meshes are split by material color, per `scripts/modelgen/README.md`.
The body uses the existing drive lamp, door and latch colors for its
built-in drive so the existing interaction code applies, and a new back
panel connector color at least one channel step clear of the palette.

## Interactions

| Target | Behavior |
|---|---|
| Built-in drive door, external Disk /// door | the Disk II model's open, close, drop and eject |
| Drive lamps | lit while that drive is selected with the motor on |
| Monitor power lamp | as on the other monitors |
| ProFile | from 048 |
