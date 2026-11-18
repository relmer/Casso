# Contract: desk models

Model space is millimeters, X right, Y back, Z up, front face at y = 0, as
for the existing models. Each model has a generator in `scripts/modelgen/`,
output under `Resources/Models/<Name>/`, a README listing its sources, and a
preview render checked before the model is wired into the app.

## Sources and their order

Photographs and Apple's service and owner's manuals come first. The design
patent's drawings are used only for a view that is missing from the
photographs, and never for a dimension that is in a photograph or a manual.

- **POG**: Apple /// Plus Owner's Guide, appendix D and chapter 2
- **MON**: Monitor III Owner's Manual
- **PFS**: "Apple III ProFile Hard Drive Specifications"
- **TCD**: photographs of a real Apple /// with a ProFile, Trinity College
  Dublin computer science collection, catalog entry TCD-SCSS-T.20251212.001
  (`fig01` to `fig15b`, saved in `%USERPROFILE%\Downloads\Casso\apple3\photos\tcd`,
  never in the repo). `fig06` is a square-on view of the whole rear;
  `fig07` and `fig08` are close-ups of the connector strip; `fig03` is the
  front; `fig13a` is the base.
- **WMC**: Wikimedia Commons photographs, saved in
  `%USERPROFILE%\Downloads\Casso\apple3\photos\commons`: "Apple III+ back.jpg"
  (a square-on studio view of the rear, the best single source for the back
  panel), "Apple III+.jpg" and "Apple Museum (Prague) Apple III (1980).jpg"
  (both with a Monitor /// on top)
- **PAT**: design patent Des. 268,584 (`Apple 3 Design Patent Case.pdf`)
- **MPAT**: the Monitor ///'s design patent, Des. 273,191 (front, top, right
  side, rear and bottom views)

**Scale of the photographs**: `fig06` has the ProFile resting on the
Apple ///. Apple's figures for the ProFile are 438.9 mm wide and 111.5 mm
high (PFS). Its width and the Apple ///'s 444.5 mm width (POG) produce the
same scale to within 0.5 percent, and the ProFile's height in the photograph
matches that scale, so one scale applies to the whole rear view.

Values below marked **estimate** were read from the photographs by eye at
that scale and are good to about 3 mm. Before a generator uses one, it is
replaced by a value measured in pixels by a script, with the script and the
image listed in the model's README. SC-010's 1 mm tolerance applies to
documented dimensions; a measured dimension is listed with the tolerance
its source supports.

## Apple /// body (`Apple3`, `cad_apple3.py`)

| Dimension | Value | Source |
|---|---|---|
| Width | 444.5 mm (17.5 in) | POG appendix D |
| Depth of the whole machine, keyboard included | 462.3 mm (18.2 in) | POG appendix D |
| Height at the rear, desk to the top of the fins | about 148 mm (5.8 in), estimate: 147 mm on WMC, 151 mm on TCD `fig06` | WMC "Apple III+ back.jpg"; TCD `fig06` |
| Apple's stated height | 121.9 mm (4.8 in) | POG appendix D |
| Finned upper section, height | about 114 mm, estimate | WMC; TCD `fig06` |
| Connector strip below the fins, height | about 34 mm, estimate | WMC; TCD `fig06` |
| Feet | about 3 mm, estimate | TCD `fig06`, `fig13a` |
| Construction | cast aluminum base, molded plastic cover | POG appendix D |
| Depth of the modeled body (front of the upper body to the back) | about 0.56 of the whole depth, about 257 mm, estimate | PAT Fig. 4 (no side photograph yet) |
| Top surface | flat for about 220 mm back from the front slope, then the top of the fins | PAT Fig. 2, Fig. 4 |
| Front face | sloped back; badge at the lower left, about 42 to 133 mm from the left edge and about 25 mm high; drive at the right, the slot spanning about 293 to 437 mm from the left and the door about 341 to 386 mm, estimates | TCD `fig03` (taken from above, so heights are foreshortened) |
| Text interlace switch (Plus) | left side, about halfway back, just above the bottom of the cover | POG p. 16 |

**The stated height and the photograph differ.** Apple's 4.8 inches is not
the height of the rear body: the photographs, scaled by the ProFile and by the body's width, put
the top of the fins about 148 mm above the desk, and the patent's front view
has the same proportion (0.35 of the width). The 4.8 inches may be measured
at another point of the case. The model uses the photograph. A square-on
side photograph has not been found yet. It is the source for the body's
depth, the slope of the front face and the height of the top's front edge.

The model ends at the front of the upper body. The sloped keyboard section
is not modeled; the cut face is closed with a plain surface in the case
color.

## Back panel

The rear is a dark finned casting above a light connector strip. Labels on
the strip, left to right from the rear: FLOPPY DISKS, PORT A, PORT B,
OUTPUT (COLOR VIDEO, B/W VIDEO, AUDIO), PORT C RS-232-C; then, on a
separate plate at the right, the power inlet and a POWER rocker switch
marked O and I (TCD `fig07`, `fig08`; POG Figure 2-1).

Horizontal positions are of each item's center, from the left edge of the
case as seen from the rear. Every position is an estimate: the first figure
is from WMC "Apple III+ back.jpg" and the second from TCD `fig06`. The two
differ by up to 9 mm, which is the uncertainty until a script measures them.

| Item | Form | Center from left (WMC / TCD) | Source of the form |
|---|---|---|---|
| Floppy disks | 26-pin shrouded header (a D connector on the Plus) | 68 / 63 mm | TCD `fig07`; POG chapter 2 |
| Port A | 9-pin D, female | 110 / 104 mm | TCD `fig07`; SPEC |
| Port B | 9-pin D, female | 142 / 136 mm | TCD `fig07`; SPEC |
| Color video | 15-pin D, female | 180 / 171 mm | TCD `fig07`; SPEC |
| B/W video | RCA jack | 210 / 202 mm | TCD `fig07`; SPEC |
| Audio | miniature phone jack | 222 / 214 mm | TCD `fig07`; SPEC |
| Port C, RS-232-C | 25-pin D, female | 253 / 245 mm | TCD `fig08`; SPEC |
| Power inlet | 3-pin IEC, two screws | 344 / 337 mm | TCD `fig08` |
| Power switch | rocker, OFF and ON | 388 / 386 mm | TCD `fig08` |
| Slot openings 1 to 4 | four tall openings in the finned section, between ribs | 174 to 270 / 171 to 265 mm, from about 42 mm to 125 mm above the desk | TCD `fig06`, `fig07` |
| Connector plate | light, with printed labels | 34 to 283 mm | TCD `fig06` |
| Power plate | light, separate | 309 to 404 mm | TCD `fig06` |

The connectors' centers are about 19 mm above the bottom of the base.
Neighboring 9-pin connectors are about 32 mm apart, which matches the
same pair measured on a photograph of the main board
(`AppleIIIPlusBoardRear.jpg`) scaled by D connector mounting hole spacing.

The fins are vertical ribs of even pitch across the whole rear; a few have
light tabs at the top. Their count and pitch are measured by script from
`fig06`.

A card installed by default (the ProFile interface card) is modeled where
its connector appears in its slot opening.

## Apple Monitor /// (`Monitor3`, `cad_monitor3.py`)

| Dimension | Value | Source |
|---|---|---|
| Width | 436.6 mm (17 3/16 in) | MON p. 6 |
| Height | 269.9 mm (10 5/8 in) | MON p. 6 |
| Depth | 317.5 mm (12 1/2 in) | MON p. 6 |
| Display zone | 214.3 by 160.3 mm (8 7/16 by 6 5/16 in) | MON p. 6 |
| Tube | 12 in (30.5 cm) diagonal; 78 degrees horizontal, 61 vertical, 90 diagonal deflection | MON p. 6 |
| Glare filter | a microporous filter in front of the tube | MON p. 4 |
| Front | power switch and contrast knob, right of the tube | MON p. 3 |
| Back | hand grip, brightness, vertical hold, vertical size, RCA video input, power cord | MON p. 3 |
| Case form | a front face tilted back, the tube and a control panel to its right behind one bezel, four feet, and a tapered tube housing at the rear | MPAT Fig. 2 to Fig. 6; WMC photographs |

Phosphor: green (A3M0039). The catalog entry is `AppleMonitorIII`, default
color mode green. The monitor weighs 16.5 pounds and rests directly on the
Apple /// (MON p. 4, p. 6). Its width is 7.9 mm less than the body's.

The monitor in the TCD photographs is a Monitor II, so they are not a
source for this model.

## Disk /// (`Disk3`, `cad_disk3.py`)

The Disk /// (A3M0004) is the external drive, a separate unit on a cable.
Up to three daisy-chain from the FLOPPY DISKS connector as `.D2` to `.D4`.
The drive built into the chassis is the same mechanism and is part of the
body model, not this one.

| Dimension | Value | Source |
|---|---|---|
| Width, height, depth | about 6 by 4 by 9 in (152 by 102 by 229 mm) | a Computer History Museum catalog entry for an external Apple /// drive; not an Apple document, and to be confirmed |
| Door, latch, lamp positions | | not yet sourced |

## ProFile

From 048. Apple's figures are 438.9 mm wide, 111.5 mm high and 223.8 mm
deep (PFS), against the generator's 439 by 110 by 226 for the 5 MB; the
generator is checked against PFS when 048 arrives. The ProFile's rear, with
its 25-pin connector, power inlet and switch, is in TCD `fig06`.

## Fit rules (unit-tested on the measured metrics)

- The ProFile's footprint is inside the body's top, centered in X, with its
  back flush with the back of the fins, as in TCD `fig06`.
- The monitor's feet are inside the top of whatever it rests on. The
  monitor is deeper than the ProFile and the modeled body, so its case may
  overhang the item below; the feet's positions are measured from MPAT's
  bottom view and the WMC photographs.
- Each item's bottom is at the top surface height of the item below. No gap
  and no intersection.
- Side-row devices do not overlap the stack's bounding box.

## Color identity

Sub-meshes are split by material color, per `scripts/modelgen/README.md`.
The body uses the existing drive lamp, door and latch colors for its
built-in drive so the existing interaction code applies. New colors, each
at least one channel step clear of the palette: the dark casting of the
rear fins, the light connector plates, the connector shells, and the badge.

## Interactions

| Target | Behavior |
|---|---|
| Built-in drive door, external Disk /// door | the Disk II model's open, close, drop and eject |
| Drive lamps | lit while that drive is selected with the motor on |
| Monitor power lamp | as on the other monitors |
| ProFile | from 048 |
