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
| Width | 444.5 mm (17.5 in) | spec sheet |
| Height | 122.0 mm (4.8 in) | spec sheet |
| Depth of the whole machine, keyboard included | 462.3 mm (18.2 in) | spec sheet |
| Depth of the modeled body (front of the upper body to the back) | | |
| Top surface: flat area, edge radii, vent slots | | |
| Drive bay: position, opening, door, latch, lamp | | |
| Badge: position and size, Apple /// and Apple /// Plus | | |
| Feet and the body's height off the desk | | |

The model ends at the front of the upper body. The sloped keyboard section
is not modeled; the cut face is closed with a plain surface in the case
color.

### Back panel

Each connector is modeled with its real outline and at its measured
position. Positions are from the left edge and the bottom edge of the back
panel, viewed from behind.

| Item | Form | Position | Source |
|---|---|---|---|
| Power switch | rocker | | |
| Power inlet | 3-pin | | |
| Black and white video | RCA jack | | |
| Color video | DB-15 | | |
| Audio | 3.5 mm jack | | |
| Joystick port A | DB-9 | | |
| Joystick port B | DB-9 | | |
| RS-232-C | DB-25 | | |
| External floppy | 26-pin header | | |
| Slot openings 1 to 4 | rectangular cutouts | | |
| ProFile interface card | the card's cable connector in its slot opening | | |

The connector forms above are from memory and are confirmed from the
Owner's Guide and the Service Reference Manual with the positions.

### Apple Monitor /// (`Monitor3`, `cad_monitor3.py`)

| Dimension | Value | Source |
|---|---|---|
| Width, height, depth | | Computer History Museum catalog entries disagree (11.25 by 17 by 12.5 in; 11.5 by 16 by 9.5 in) |
| Tube | 12 in diagonal | Apple Monitor /// documentation, to cite |
| Glass: opening, sag radius, anti-glare mesh | | |
| Base footprint | | |
| Controls, power lamp, brand stamp | | |

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
