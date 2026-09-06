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

**Namespacing**: `Software` and `Creation Time` are PNG-registered keywords and carry
their registered meanings. Everything else is prefixed `Casso ` to avoid colliding with
keywords the format registers now or later.

**`Source` is deliberately NOT used.** The specification defines it as the *device* used
to create the image -- a scanner or a camera. Nothing captured a screenshot: Casso
synthesized it, and the device that wrote the file is the host PC. The emulated machine
is a fact about the subject, not about capture hardware, so it goes in `Casso machine`
where it claims nothing the format did not intend. (`Source` is also close to unused in
the wild -- cameras write EXIF and PNG gained a dedicated `eXIf` chunk in 1.5 -- so
borrowing it would have bought little visibility in exchange for the stretch.)

## Entries by capture mode

`--` means the entry is not written for that mode. Entries are written in this order.

**One value per keyword.** The guarantee below operates on *keywords*: a composite value's
internal grammar sits outside it, so adding a parameter to a blob would silently change
the shape of something a reader had learned to parse, in a document that promises exactly
that will not happen. A field per keyword puts every value under the guarantee that is
actually written down.

Casso's own keywords are **sentence case**, with `CRT` upper because it is an initialism.
The three registered keywords are title case because the PNG specification spells them
that way, not because Casso chose it.

| # | Keyword | `scene` | `crt` | `raw` |
|---|---|---|---|---|
| 1 | `Software` | `Casso 1.22.0` | same | same |
| 2 | `Creation Time` | `Sat, 05 Sep 2026 14:32:07 -0700` | same | same |
| 3 | `Casso capture` | `scene` | `crt` | `raw` |
| 4 | `Casso machine` | `Apple //e` | same | same |
| 5 | `Casso monitor` | `AppleMonitorII/color` | same | same |
| 6 | `Casso scene yaw` | `12.5` | -- | -- |
| 7 | `Casso scene pitch` | `-8.0` | -- | -- |
| 8 | `Casso scene zoom` | `1.00` | -- | -- |
| 9 | `Casso scene pan X` | `0.000` | -- | -- |
| 10 | `Casso scene pan Y` | `0.000` | -- | -- |
| 11 | `Casso CRT brightness` | `1.05` | same | -- |
| 12 | `Casso CRT contrast` | `1.00` | same | -- |
| 13 | `Casso CRT gamma` | `1.00` | same | -- |
| 14 | `Casso CRT scanlines` | `0.20` | same | -- |
| 15 | `Casso CRT bloom strength` | `0.25` | same | -- |
| 16 | `Casso CRT bloom radius` | `0.80` | same | -- |
| 17 | `Casso CRT bleed` | `0.00` | same | -- |
| 18 | `Casso CRT persistence` | `0.00` | same | -- |

Totals: **18** entries for `scene`, **13** for `crt`, **5** for `raw`.

## Entry definitions

### 1. `Software`

`"Casso "` followed by the release version. The same construction as the WOZ creator
stamp (`CassoEmuCore/Devices/Disk/WozLoader.cpp:59`), so there is one spelling of "who
made this" across every artifact Casso authors.

### 2. `Creation Time`

The capture's local wall-clock time in RFC 1123 form, including the UTC offset. Present
because filenames are routinely changed on upload and the timestamp in the name does not
survive that.

The offset's sign belongs to the hour while the minutes stay positive, which is what a
half-hour zone west of Greenwich (`-0330`) gets wrong when written with a bare division.

### 3. `Casso capture`

The capture mode token: `scene`, `crt` or `raw`. Identical to the stored preference
token, so a report saying `Casso capture: raw` maps directly onto the radio the user
selected.

**This is the entry that most changes how a file is read.** Without it a flat 560x384
image attached to an issue is ambiguous between "the reporter chose raw mode" and "the
CRT chain produced nothing", and a picture-only capture is ambiguous between "chrome is
missing" and "chrome was never in scope".

### 4. `Casso machine`

The emulated machine's own display name, taken from the `name` field of its machine JSON
(`Apple //e`, `Apple ][+`, `Apple //c`) -- never the internal identifier (`Apple2e`).

### 5. `Casso monitor`

The monitor's frozen catalog identifier joined to the active color mode, in the exact
form produced by `CrtResolver::MakeKey` -- which is the key that monitor's CRT
adjustments are stored under in the user's preferences
(`Casso/Config/GlobalUserPrefs.h:126`). The string in the file is therefore the string
you look the user's adjustments up by.

Note the real form is lowercase in its second half (`AppleMonitorII/color`), because that
is what `MakeKey` emits; it is a key, not a label.

Emitted for `raw` as well. The tube did not touch those pixels, but the color mode did --
the monochrome tint is applied in the framebuffer
(`CassoEmuCore/Video/MonochromeTint.h`) -- and one key is easier to read than a
conditional half-key.

### 6-10. `Casso scene *`

The scene view as five numbers: orbit yaw and pitch in **degrees** to a tenth (the finest
step a drag produces), zoom to two decimals, and pan to three (which is what separates
two positions that look alike but frame differently).

`scene` captures only: the other two modes have no scene, and a pose recorded against a
picture is a fact about nothing. All five are skipped together when the desk scene has
not composed yet, rather than emitted as five zeros -- which would read as a real view
pointing at the origin.

Degrees rather than the radians the scene holds, because a person restoring a view acts
in degrees. The on-screen pose readout still renders its own one-line form; the file does
not use it.

### 11-18. `Casso CRT *`

The resolved CRT parameters actually in force for the captured image, one per entry.
Present for the same reason as the pose: "the effects render wrong" is the bug class
these modes exist to report, and without the parameters the reader cannot distinguish a
shader fault from a slider set oddly.

Bloom carries strength and radius separately, because either alone says little about what
the halation looked like.

`raw` emits none of them, because no CRT parameters were applied to that image and
claiming otherwise would assert something false.

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
