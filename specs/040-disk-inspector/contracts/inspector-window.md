# Contract: Inspector window, menus, export and clipboard

**Covers**: FR-001, FR-007, FR-025, FR-029, FR-034, FR-055 to FR-061,
FR-072, FR-073, FR-080. **Decisions**: research R19 to R25, R28.

All labels use sentence case. The spec gives the exact wording of every label
and message it quotes; this contract lists where each goes. Any label the spec
does not give goes to the owner before it ships.

## Entry points

| Host | Where | Item | Enabled when |
|---|---|---|---|
| Casso | Storage menu, drive 1's group, after salvage | "Inspect disk 1..." | Machine has a Disk II in slot 6 |
| Casso | Storage menu, drive 2's group, after salvage | "Inspect disk 2..." | As above, and drive 2 attached |
| Casso | Each drive's right-click menu (drive band, fullscreen strip, desk scene) | "Inspect disk N..." | As above |
| Explorer | Right-click on a disk image (list and folder tree) | "Inspect disk image" | Format in FR-003; writable or not |
| Explorer | List background inside an image, any depth | "Inspect disk image" | As above |
| Explorer | Right-click with exactly two disk images selected | "Compare disk images" | Both in FR-003's formats |
| Explorer | Preview pane small platter | click | Analysis done |

Casso has at most one window; opening it for a drive switches it and brings it
to the front. Explorer has at most one window per image.

Window title: "Disk inspector - " and the image's file name. Opens at 980×660,
minimum 640×460, both scaled for the display.

## Layout

Toolbar, comparison bar (while comparing), platter column (platter, zoom
controls, legend, overlays, hint, disk tabs Tracks, Findings, File map, Image,
and Differences while comparing), track column (track header, strip with
controls and hint, sector row, track tabs Sector data, Nibbles, Fields, Flux
timing). Order and contents of each: FR-007.

## Keyboard

| Focus | Key | Action |
|---|---|---|
| Platter | Up, Down | Quarter track toward 0, toward 39.75 |
| Platter | Page Up, Page Down | Next whole track in that direction |
| Platter, sector row | Left, Right, Home, End | Previous, next, first, last sector in passing order, wrapping at the index |
| Platter | +, -, 0 | Zoom in, out, fit |
| Platter (zoomed) | Ctrl+arrows | Pan |
| Strip | Left, Right | Pan |
| Nibbles, Sector data | Shift+arrows, Ctrl+A | Extend selection, select all |
| Any | Ctrl+C | Copy (table below) |
| Sector data (editing) | Ctrl+Z, Ctrl+Y | Undo, redo pending changes |
| Any | Ctrl+G, Ctrl+F, F3, Shift+F3 | Go to, Find, Find next, Find previous |

Ctrl+G and the Find keys are not in the spec; the owner approved them on
2026-10-08.
Every command is reachable with Tab and the keys above (SC-011).

## Clipboard

| Source | Format |
|---|---|
| Sector data, hex column | Hex digits, space-separated, 16 per line |
| Sector data, text column | Characters, high bit masked, "." for unprintable |
| "Copy sector" | Hex dump: offset, 16 bytes, text |
| Nibbles tab, strip | Hex nibbles, space-separated |
| Tracks, Fields, Findings, Image, Differences rows, file list, histogram | Tab-separated, header row first |
| "Copy map" | One line per track, one letter per sector, then the key |

## Export files

| Form | Default file name | Contents |
|---|---|---|
| Sectors | `<image> T17 sectors.bin` (track range: `T03-T05`) | 256 bytes per sector in the chosen order; missing sectors as zeros, listed before writing |
| Track nibbles | `<image> T17.25 nibbles.bin` | One byte per framed nibble, one turn from the index |
| Track bits | `<image> T17.25.woz` | WOZ 2.1 holding only that quarter track, as a bit or flux record, unchanged |

Written through `DurableCommit` (`CreateNew`; `Replace` only after the user
confirms). The image is never changed.

## Saved state

Per host, separately: placement and size, mode, disk tab, track tab, timing
range, overlays, "Show deleted files"; Casso also Follow head, last drive, and
whether the window was open (reopened on the last drive at launch). Never
saved: zoom, pan, selection, decode settings, comparison options.
