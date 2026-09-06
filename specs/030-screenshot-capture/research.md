# Phase 0 Research: Screenshot capture modes, file output, and metadata

**Feature**: 030-screenshot-capture | **Date**: 2026-09-05

The spec carried no `[NEEDS CLARIFICATION]` markers -- the design was settled in
discussion before it was written. What follows is that reasoning recorded against the
code that justifies it, plus the codebase findings that the discussion did not cover.

This document exists mainly to stop the rejected alternatives being re-proposed. Several
of them are the obvious first idea.

---

## R-001: What a screenshot captures

**Decision**: Three modes -- `scene` (default), `crt`, `raw`.

**Rationale**: GH #132 reports that the screenshot is the unprocessed framebuffer and
ends by naming the open question: whether a screenshot should be the composited picture
alone or the whole window. It is genuinely three jobs, not one. Showing off a desk scene,
reporting "the scanlines are wrong", and extracting pixel-exact art want three different
images, and no single default serves all three.

**Alternatives considered**:

- *One mode, the composited picture.* Rejected: makes pixel-exact extraction impossible,
  and that is the only behavior that exists today.
- *A per-capture choice via an Edit submenu.* Rejected by the owner: three menu items
  plus a toolbar button plus a shortcut for a command most users invoke one way.

---

## R-002: Recovering rendered pixels

**Decision**: The command drives a synchronous paint (`InvalidateRect` + `UpdateWindow`,
through the existing `WM_PAINT` render entry point) and reads back before Present.

**Rationale**: The swap chain is created with `DXGI_SWAP_EFFECT_FLIP_DISCARD`
(`Dxui/Window/DxuiHwndSource.cpp:1564`). Under the flip model a presented back buffer's
contents are discarded by definition -- there is no already-rendered frame to read. But
the command arrives on the UI thread, which is the render thread, so the handler can
drive a frame itself rather than waiting for the next natural one. The operation stays
synchronous from the caller's point of view.

**Alternatives considered**:

- *Read the last presented frame.* Not possible under flip-discard.
- *Set a request flag and let the next natural frame service it.* Rejected: makes the
  command asynchronous, delays the confirmation notice by a frame, and adds a state
  machine for no benefit over driving the paint directly.
- *`CopyResource` the back buffer into a retained texture every frame so the last one is
  always readable.* Rejected: a full back-buffer copy at 60 Hz, forever, to serve an
  action taken at human frequency. Violates Principle IV.

---

## R-003: Live readback, not a one-shot offscreen render

**Decision**: `crt` and `scene` read back the live chain's output at the window's
resolution. There is no separate fixed-size render for capture.

**Rationale**: The owner requires phosphor persistence in the capture. Persistence is
carried across frames in a single texture with a primed flag
(`Casso/CrtPostProcess.cpp:586`), and `CrtPostProcess.cpp:734` already documents that it
holds nothing on the first frame at a new size. A one-shot render at a fixed capture
resolution is always that first frame, so it can never show a trail.

Reading the live chain also removes a whole class of drift: there is one CRT code path,
not a live one and a capture one that can diverge.

**Cost accepted**: capture resolution follows the window, so `scene` and `crt` output
varies in size. `raw` remains fixed, which is where determinism lives.

**Alternatives considered**:

- *One-shot offscreen render at a fixed 2x (1120x768).* Was the recommendation until
  persistence was required. It buys deterministic size and chrome exclusion, and it would
  also have supplied the mechanism GH #134 needs -- but it cannot produce trails.
- *Render at the live chain's size to reuse its primed persistence texture.* This is
  simply the live readback with extra steps.

---

## R-004: The readback mechanism itself

**Decision**: Create a `D3D11_USAGE_STAGING` texture with `D3D11_CPU_ACCESS_READ` sized
to the capture rectangle, `CopySubresourceRegion` the source region into it, `Map` for
read, copy rows out, `Unmap`, release. Created on demand per capture, never retained.

**Rationale**: This is new machinery. A survey of every `Map` call in `Casso/` and
`Dxui/` found `D3D11_MAP_WRITE_DISCARD` in all cases -- the tree uploads to the GPU and
has never read back from it. There is no existing pattern to follow and no existing
helper to reuse, so this is a genuine new capability rather than a variation.

**Consequences for the plan**: the mapped texture must be unmapped on every exit path,
which is exactly what the constitution's single-exit EHM discipline is for. Row pitch
from `D3D11_MAPPED_SUBRESOURCE` will not equal `width * 4` and must be honored per row.

**Alternatives considered**:

- *`IDXGISurface1::GetDC` + GDI blit.* Rejected: only valid on surfaces created with
  `GDI_COMPATIBLE`, and it drags GDI into a D3D path for nothing.
- *`PrintWindow` on the HWND.* Rejected: it is what the external screenshot scripts use,
  and it cannot see a composited window's content correctly -- it renders the DirectComposition
  hole as black.

---

## R-005: Raw capture is 560x384, not 280x192

**Decision**: `raw` captures the framebuffer as-is at 560x384.

**Rationale**: 280 is not the native grid. 80-column text and double hi-res are natively
560 dots wide (`CassoEmuCore/Video/AppleDoubleHiResMode.h:15`,
`Apple80ColTextMode.cpp:142`); reducing to 280 would destroy both, and DHGR mono
dithering is exactly the art people screenshot. The 280-native modes (40-column text,
LORES, HGR) write 2x horizontally into the same buffer. The vertical 384 is a 2x doubling
of the native 192 scanlines, applied uniformly so every mode shares one geometry
(`AppleHiResMode.cpp:93`).

So 560x384 is lossless and reversible: nothing is interpolated and every mode's native
content is exactly recoverable.

Note that the monochrome tint is applied to the framebuffer in core
(`CassoEmuCore/Video/MonochromeTint.h`), so `raw` carries the monitor's phosphor color.
"Raw" means no CRT chain, not no monitor character.

**Alternatives considered**:

- *560x192, the truly unscaled grid.* Rejected: its pixels are 1:2 and it displays wrong
  at 1:1 in every viewer. The doubling is lossless, so 560x384 gives up nothing.

---

## R-006: PNG only, no format choice

**Decision**: PNG. The user is not offered a format setting.

**Rationale**: Two of the three modes carry scanlines and bloom, which is continuous
tone, so the "PNG is smaller for flat pixel art" argument does not hold for them. The
argument that does hold is that a lossy encoder's artifacts cluster on high-contrast
edges, and high-contrast edges are precisely what a CRT render-bug report needs legible.
A setting whose second option is never the right answer only produces bad screenshots.

**Alternatives considered**:

- *A JPEG option in settings.* Rejected as above.
- *Format follows a file-dialog filter.* Moot once the file dialog was rejected (R-008).

---

## R-007: No `pHYs` chunk on screenshots

**Decision**: Do not stamp a physical resolution. Pass dpi 0/96, which WIC writes as
96/96 -- indistinguishable from absent, as `PngCodec.cpp:380` already notes.

**Rationale**: Casso presents the framebuffer at square pixels --
`EmulatorShell.cpp:1356` fits the picture at exactly 560:384 with no aspect correction
anywhere. A square-pixel PNG is therefore pixel-for-pixel what the user was looking at,
and there is no stretch to correct. Stamping a non-square `pHYs` to reach 4:3 would make
the file disagree with the application in the minority of viewers that honor the chunk,
while the majority that ignore it (browsers, GitHub, Explorer thumbnails) still show what
Casso shows. One file, two aspects, neither authoritative.

Separately, `pHYs` already means something specific in this codec: the printer's real
physical resolution, 288 or 576 dpi, which `PngCodec::ReadDpi` reads back with that
meaning. Giving one chunk a second meaning in a second caller is how a field stops being
trustworthy for either.

**Alternatives considered**:

- *Stamp a non-square `pHYs` for correct 4:3 aspect.* Rejected as above. If 560:384 is
  the wrong aspect for an Apple II, it is wrong on screen too, and the fix belongs in the
  picture fit where the CRT geometry and theme previews follow it. A correction applied
  only on export means the app disagrees with its own output.

---

## R-008: Auto-save, no file dialog

**Decision**: Write the file immediately to a configured folder with a generated name.

**Rationale**: Screenshot is a burst action on a toolbar button and a hotkey, taken while
something is moving on screen. A modal file dialog steals focus from the running machine
and turns one tap into five. This is why people press PrtScn rather than File > Save As.

The printer's Save deliberately keeps its dialog, and that stays right: it is a considered
once-per-printout delivery invoked from a preview window, not a capture taken mid-frame.
Different action, different answer.

**Alternatives considered**:

- *`IFileSaveDialog`, matching `WindowCommandManager::SavePrintoutAs`.* Rejected as
  above.
- *Dialog on first use, then remember.* Rejected: a mode change disguised as a prompt,
  and it still interrupts the first capture.

---

## R-009: Destination and filename

**Decision**: `<Pictures>\Casso Screenshots`, created on demand. Filename
`Casso YYYY-MM-DD HHMMSS.png`, deduplicated with a ` (n)` suffix.

**Rationale**: Directly parallel to the existing `<Pictures>\Casso Prints`
(`WindowCommandManager.cpp:1464`), so Casso's two output folders sit side by side and
share one naming grammar.

The word "Screenshot" is omitted because the folder supplies it. "Casso" is kept because
the folder is *not* the context that travels: a screenshot's whole life is being dragged
out of that folder into an issue or a chat, and at that moment `2026-09-05 143207.png` is
anonymous. Snipping Tool and the Xbox Game Bar keep the same apparently-redundant prefix
for the same reason.

**Alternatives considered**:

- *`Casso Screenshot <date> <time>.png`.* Rejected as redundant with the folder.
- *`<machine> <date> <time>.png`, e.g. `Apple IIe 2026-09-05 143207.png`.* Informative
  and sorts by machine, but drops the application attribution that makes a stray file
  identifiable. The machine is in the metadata instead.

---

## R-010: Metadata mechanism and content

**Decision**: PNG `tEXt` chunks written through
`IWICBitmapFrameEncode::GetMetadataQueryWriter`, as `/[<n>]tEXt/{str=Keyword}` with
`VT_LPSTR` values, set **before `WriteSource`**. Keywords are Latin-1 and at most 79
characters. The per-mode entry set is the contract in
[contracts/screenshot-metadata.md](contracts/screenshot-metadata.md).

**Corrected during implementation.** This entry originally said `/tEXt/{str=Keyword}`,
set any time before `Commit`. Both halves were wrong, and both fail *silently* -- WIC
returns `S_OK` and drops the data:

- Without the block index, every entry addresses the same PNG metadata block, so seven
  entries produce one chunk.
- Set after `WriteSource`, nothing is written at all: the encoder emits chunks as the
  pixels stream through it.

Neither is documented clearly. Both were caught only because the round-trip test parses
`tEXt` out of the PNG byte stream directly rather than reading it back through WIC --
a WIC-to-WIC round trip would have agreed with itself and passed.

**Rationale**: `tEXt` is the standard mechanism, so `exiftool` and ordinary image tools
read it with no knowledge of Casso. Four of the seven keywords are PNG-registered
(`Software`, `Source`, `Creation Time`, plus `Comment` which we do not use); the three
`Casso ` keywords are namespaced to avoid colliding with registered ones.

Three content decisions are worth recording:

- **`Casso Capture` is the highest-value entry**, and it exists only because there are
  three modes. Without it a flat 560x384 attached to an issue is ambiguous between "they
  were in raw mode" and "the CRT chain produced nothing".
- **`Casso Scene Pose` pays off a cost the codebase is already paying.**
  `GlobalUserPrefs.h:60` explains that the on-screen pose readout exists because a
  screenshot does not carry the angle it was taken from, and that it charges for this by
  printing numbers across the middle of the picture. In metadata every scene capture is
  self-describing and the picture stays clean. It reuses the readout's own format string
  (`EmulatorShell.cpp:2426`) so there is one formatter.
- **`Casso Monitor` is `CrtResolver::MakeKey`'s output**, i.e. the exact key the user's
  CRT overrides are stored under (`GlobalUserPrefs.h:126`). The string in the file is the
  string you look their adjustments up by.

**Hard exclusion**: no filesystem paths, no host name, user name, or hardware
identifiers. A screenshot's routine destination is a public issue. The GPU adapter string
would be tempting for render bugs and is excluded on the same ground -- it is
fingerprinting, and it can be asked for.

**Alternatives considered**:

- *`iTXt` for UTF-8 values.* Rejected: WIC's PNG encoder support for writing `iTXt` is
  less certain than for `tEXt`, and every value we emit is ASCII.
- *A single JSON blob in one `Comment` keyword.* Rejected: opaque to standard tools,
  which defeats the point of using the standard mechanism.
- *An `Casso CRT` entry for `raw` too.* Rejected: no CRT parameters applied to that
  image, so emitting them would assert something false.

---

## R-011: Where the settings live

**Decision**: Retitle the Printing settings page to cover both subjects, Printing section
first, a new Screenshots section second.

**Rationale**: The Display page is exclusively CRT display effects
(`Casso/Ui/Settings/DisplayPage.h`) and the owner ruled it out. Between a sixth tab for
three controls and sharing a page, sharing wins: "what Casso emits to the host" is a
coherent subject that the Printing page already half-occupies, and three controls do not
earn a tab. Printing stays first so a user navigating to printer settings still lands on
them.

**Alternatives considered**:

- *A new Screenshots tab.* Rejected as too thin, and it would grow the tab strip to six.
- *The Display page.* Rejected by the owner: wrong subject.
- *A new "Output" tab collecting every host destination.* Rejected: the only other
  candidate setting (the print save folder) does not exist -- print Save always prompts.

---

## R-012: Preferences stored as tokens

**Decision**: `screenshotMode` persists as the string `"scene"` / `"crt"` / `"raw"`.

**Rationale**: Matches the established convention for enumerated preferences in this file
-- `printDotStyle` stores `"ink"` / `"plain"`, `audioDownloadConsent` stores
`"ask"` / `"allow"` / `"decline"`. An index would silently repoint every existing user's
setting the first time the radio order changed, with no diagnostic.

---

## R-013: The overlay-hiding rule

**Decision**: For the capture frame only, hide the scene compass
(`EmulatorShell.cpp:2655`), the frame-rate readout, the scene-pose readout, and the
mouse-capture notice. Restore immediately after.

**Rationale**: One principle covers all four -- **hide what describes the application,
capture what describes the machine**. The compass's own comment calls it "furniture of
the window rather than part of the machines". The readouts are diagnostics.

A side effect worth having: scene captures no longer depend on which diagnostics the user
has switched on, so two captures of the same view are the same image.

The one-frame flicker as they drop and return reads as a shutter, which is the correct
affordance for a screenshot.

---

## R-014: GH #134 / #82 are out of scope, and why they cannot share the mechanism

**Decision**: The theme-preview CRT issues are not in this feature.

**Rationale**: They share a root cause with #132 -- every consumer that reads the
framebuffer directly misses the GPU chain -- but not a solution. #134 requires the
preview to show a theme that is **not** the active one, with that theme's parameters; its
own text says selecting Retro Terminal should show Retro Terminal's scanlines while Dark
Modern is still active. No readback of the live chain can show a chain that is not
running.

Nor can it be served by calling the existing chain twice with different parameters:
persistence is instance state (one carry-over texture and one primed flag,
`CrtPostProcess.cpp:586`), so a preview pass through the same instance would write its
output into the history the live picture reads next frame, corrupting the real trail.
#134 needs a second `CrtPostProcess` instance with its own targets.

**Note**: an earlier version of this design (the one-shot offscreen render, R-003) *would*
have supplied #134's mechanism. Choosing persistence gave that up. The trade was made
deliberately and should not be quietly re-opened when #134 is picked up -- #134 needs its
own machinery either way.

---

## R-015: `DxuiRadioGroup` has no per-option description

**Finding**: `DxuiRadioOption` carries a `rect` and a `label` and nothing else
(`Dxui/Widgets/DxuiRadio.h`). FR-032 requires descriptive text per option, because
`scene` / `crt` / `raw` are not self-explanatory from a label.

**Decision**: Extend `DxuiRadioOption` with an optional description line, and have the
group lay it out under the label with the same left inset.

**Alternatives considered**:

- *Three separate `DxuiLabel`s positioned under each radio by the page.* Rejected: the
  page would own the geometry of a widget's internals, and the next page that wants
  described radios would copy it.

---

## R-016: No folder picker exists yet

**Finding**: `CLSID_FileOpenDialog` is used once, for disk images
(`WindowCommandManager.cpp:1004`), without `FOS_PICKFOLDERS`. Folder selection is new.
Opening a folder in Explorer has a precedent in the `ShellExecuteW` call at
`Casso/Ui/Dialogs/DialogBodyContent.cpp:73`.

**Decision**: Add a folder-picker path (`FOS_PICKFOLDERS` on `IFileOpenDialog`) and reuse
`ShellExecuteW` for the "open folder" action. Both are unambiguously execution, not
decision, and stay in the shell.
