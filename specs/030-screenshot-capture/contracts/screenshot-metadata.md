# Contract: Screenshot metadata

**Feature**: 030-screenshot-capture | **Date**: 2026-09-05 | **Version**: 1

This is a **published contract**, not an internal detail. Files written under it leave
the machine and get attached to issues, so:

- Entries MAY be added in a later version.
- An entry's keyword MUST NOT be renamed, and its meaning MUST NOT be repurposed.
- A reader MUST tolerate entries it does not recognize, and MUST tolerate the absence of
  any `Casso ` entry.
- Files written by version 1 must stay readable by every later version.

## Mechanism

PNG `tEXt` chunks, written through `IWICBitmapFrameEncode::GetMetadataQueryWriter` as
**`/[<n>]tEXt/{str=<Keyword>}`** with `VT_LPSTR` values, where `<n>` is the entry's
zero-based position.

Two details of WIC's behavior here were established by testing rather than
documentation, and both fail silently when got wrong -- the encode returns `S_OK` either
way:

- **The block index is required.** PNG metadata is a list of blocks and each `tEXt` chunk
  is its own block. An unindexed `/tEXt/{str=…}` query addresses the *same* block every
  time, so writing seven entries leaves one chunk in the file: the last.
- **Metadata must be set before `WriteSource`, not merely before `Commit`.** The encoder
  emits chunks as the pixels stream through it, so anything set afterwards is accepted and
  then absent from the output.

`tEXt` is chosen over `iTXt` because it is the format's oldest and most universally read
text mechanism, and every value emitted here is ASCII. Standard tools -- `exiftool`,
ImageMagick `identify -verbose`, most image viewers' properties panes -- read it with no
knowledge of Casso.

**Keyword constraints** (PNG specification): 1-79 characters, Latin-1, no leading,
trailing, or consecutive spaces. All keywords below comply.

**Namespacing**: `Software`, `Source` and `Creation Time` are PNG-registered keywords and
carry their registered meanings. Casso-specific entries are prefixed `Casso ` to avoid
colliding with registered keywords now or later.

## Entries by capture mode

`--` means the entry is not written for that mode. Entries are written in this order.

| # | Keyword | `scene` | `crt` | `raw` |
|---|---|---|---|---|
| 1 | `Software` | `Casso 1.22.0` | same | same |
| 2 | `Source` | `Apple //e` | same | same |
| 3 | `Creation Time` | `Sat, 05 Sep 2026 14:32:07 -0700` | same | same |
| 4 | `Casso Capture` | `scene` | `crt` | `raw` |
| 5 | `Casso Monitor` | `AppleMonitorII/GreenMono` | same | same |
| 6 | `Casso Scene Pose` | `yaw 12.5  pitch -8.0  zoom 1.00  pan 0.000 0.000` | -- | -- |
| 7 | `Casso CRT` | `brightness 1.00  contrast 1.05  gamma 1.00  scanlines 0.35  bloom 0.50/1.00  bleed 0.00  persistence 0.20` | same | -- |

## Entry definitions

### 1. `Software`

`"Casso "` followed by the release version. The same construction as the WOZ creator
stamp (`CassoEmuCore/Devices/Disk/WozLoader.cpp:59`), so there is one spelling of "who
made this" across every artifact Casso authors.

### 2. `Source`

The emulated machine's own display name, taken from the `name` field of its machine JSON
(`Apple //e`, `Apple ][+`, `Apple //c`) -- never the internal identifier (`Apple2e`).

The PNG specification defines `Source` as the device used to create the image. An
emulated Apple II is exactly that.

### 3. `Creation Time`

The capture's local wall-clock time in RFC 1123 form, including the UTC offset. Present
because filenames are routinely changed on upload and the timestamp in the name does not
survive that.

### 4. `Casso Capture`

The capture mode token: `scene`, `crt` or `raw`. Identical to the stored preference
token, so a report saying `Casso Capture: raw` maps directly onto the radio the user
selected.

**This is the entry that most changes how a file is read.** Without it a flat 560x384
image attached to an issue is ambiguous between "the reporter chose raw mode" and "the
CRT chain produced nothing", and a picture-only capture is ambiguous between "chrome is
missing" and "chrome was never in scope".

### 5. `Casso Monitor`

The monitor's frozen catalog identifier joined to the active color mode, in the exact
form produced by `CrtResolver::MakeKey` -- which is the key that monitor's CRT
adjustments are stored under in the user's preferences
(`Casso/Config/GlobalUserPrefs.h:126`). The string in the file is therefore the string
you look the user's adjustments up by.

Emitted for `raw` as well. The tube did not touch those pixels, but the color mode did --
the monochrome tint is applied in the framebuffer
(`CassoEmuCore/Video/MonochromeTint.h`) -- and one key is easier to read than a
conditional half-key.

### 6. `Casso Scene Pose`

Orbit yaw and pitch in degrees, zoom, and pan, written in the **same format string** the
on-screen pose readout uses (`Casso/EmulatorShell.cpp:2426`). One formatter, so a pose
read out of a file and a pose read off an old screenshot are the same text and restore
the same view.

`scene` only: the other two modes have no scene and therefore no pose.

### 7. `Casso CRT`

The resolved CRT parameters actually in force for the captured image. Present for the
same reason as the pose: "the effects render wrong" is the bug class these two modes
exist to report, and without the parameters the reader cannot distinguish a shader fault
from a slider set oddly.

`raw` emits nothing, because no CRT parameters were applied to that image and claiming
otherwise would assert something false.

## Excluded, deliberately

The following MUST NOT appear in any screenshot's metadata. A screenshot's routine
destination is a public issue.

| Excluded | Why |
|---|---|
| Any filesystem path | Carries the user's account name out of the machine |
| Host name, user name, domain | Identifies the person, not the software |
| GPU adapter / driver strings | Tempting for render bugs, but it is fingerprinting. It can be asked for |
| Mounted disk image paths | As above. A bare filename would be acceptable if ever needed; a path is not |
| Image dimensions | The PNG `IHDR` already records them; duplicating invites the two to disagree |
| `pHYs` physical resolution | See research R-007. Casso presents at square pixels, and this chunk already means printer dpi in the same codec |

## Reading these files

```text
exiftool "Casso 2026-09-05 143207.png"
magick identify -verbose "Casso 2026-09-05 143207.png"
```

Both report `tEXt` entries by keyword with no Casso-specific tooling.
