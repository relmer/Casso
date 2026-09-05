# Phase 1 Data Model: Screenshot capture modes, file output, and metadata

**Feature**: 030-screenshot-capture | **Date**: 2026-09-05

Everything on this page except `CapturedImage` and the preference fields lives in
`CassoEmuCore/Capture/` and is pure data-in / data-out, so `UnitTest` drives all of it
without an `HWND` or a D3D device.

---

## ScreenshotMode

The user's choice of what a capture contains. An enum with a stable token spelling.

| Value | Token | Meaning |
|---|---|---|
| `Scene` | `"scene"` | The scene viewport region, CRT effects and desk scene included. **Default.** |
| `Crt` | `"crt"` | The picture with CRT effects, no scene furniture, no chrome. |
| `Raw` | `"raw"` | The framebuffer at 560x384, no CRT processing. |

**Rules**

- `ParseToken` maps an unrecognized or empty token to `Scene` rather than failing. A
  preferences file written by a newer build naming a mode this one does not have is a
  forward-compatibility case, not a user error.
- `FormatToken` round-trips exactly. Tokens are frozen: a released token is a stored
  preference value and renaming one silently repoints existing users (FR-041).

---

## CaptureSource

Where the pixels come from. Resolved from the mode plus the live presentation state, not
chosen by the user.

| Value | Read from | Selected when |
|---|---|---|
| `Framebuffer` | the CPU-side emulated framebuffer | `mode == Raw` |
| `PictureTarget` | the CRT chain's own offscreen target | `mode == Crt && deskSceneActive` |
| `BackBufferRegion` | a sub-rectangle of the back buffer | `mode == Crt && !deskSceneActive`, **or** `mode == Scene` (always) |

The three conditions are mutually exclusive and exhaustive; there is no other case.

**`Scene` is always `BackBufferRegion`**, whether or not a desk scene is active. The
chain's offscreen target holds the *picture*, not the scene -- the desk scene samples that
target onto the glass afterwards -- so the composed scene exists only in the back buffer.
Under a theme that draws no desk scene, `Scene` still reads the viewport rectangle out of
the back buffer, which is what the mode promises (spec Assumptions).

`Crt` is the mode that varies, because under a scene theme the picture is available
directly from the chain's offscreen target, while under a flat theme the chain composites
straight into the back buffer and the picture must be sub-rected out of it.

### Frame ordering

Which point in the frame a source is read at is part of the source, not an
implementation detail, because it is what makes FR-003 and FR-004 true:

| Source | Read at | Why |
|---|---|---|
| `Framebuffer` | any time, no paint needed | CPU-side; no overlay suppression either |
| `PictureTarget` | after the CRT composite | the target holds only the picture; nothing else ever lands in it |
| `BackBufferRegion`, `mode == Crt` | after the CRT composite, **before** the chrome panel walk | chrome has not painted yet, so the sub-rect is picture-only |
| `BackBufferRegion`, `mode == Scene` | **after** the chrome panel walk | the scene is complete; the viewport rect excludes the chrome bands, which lie outside it |

The overlays hidden per FR-007 are suppressed for the whole capture paint, so they are
absent regardless of which point is read.

---

## ScreenshotPlan

The resolved answer to "what happens when the user presses the button". Produced by
`ScreenshotPlan::Resolve` from the inputs below; consumed by the shell, which decides
nothing further.

**Inputs**

| Field | Type | Notes |
|---|---|---|
| `mode` | `ScreenshotMode` | from preferences |
| `saveFile` | `bool` | from preferences |
| `folder` | `fs::path` | from preferences; empty means default |
| `defaultPicturesFolder` | `fs::path` | supplied by the caller, never discovered here (Test Isolation) |
| `viewportPx` | `RECT` | the scene viewport |
| `picturePx` | `RECT` | where the picture lands inside the target |
| `framebufferSize` | `SIZE` | 560x384 |
| `deskSceneActive` | `bool` | false for compact / flat themes |
| `windowMinimized` | `bool` | |
| `when` | `SYSTEMTIME` | injected clock |
| `taken` | `function<bool(const fs::path &)>` | injected existence predicate |

**Outputs**

| Field | Type | Notes |
|---|---|---|
| `refusal` | `CaptureRefusal` | `None`, or `NothingRendered` when minimized and the mode needs rendered pixels |
| `source` | `CaptureSource` | |
| `sourceRectPx` | `RECT` | the region to read back; the whole framebuffer for `Raw` |
| `hideOverlays` | `bool` | true whenever `source` is not `Framebuffer` |
| `writeFile` | `bool` | `saveFile && refusal == None` |
| `outputPath` | `fs::path` | empty when `writeFile` is false |
| `folderMustBeCreated` | `bool` | the destination does not exist and the shell should create it |

**Rules**

- `Raw` is never refused. It reads the CPU framebuffer and does not care whether anything
  is on screen (spec Edge Cases).
- `Scene` under a theme with no desk scene still resolves to the viewport rectangle. The
  mode is not silently switched (spec Assumptions).
- `hideOverlays` is false for `Raw`: the overlays were never in the framebuffer, so
  hiding them would cost a repaint for nothing.
- `outputPath` is `folder / <base>` where `folder` falls back to
  `defaultPicturesFolder / "Casso Screenshots"` when empty (FR-042), and the base name
  comes from `PrintFileNaming` (below).
- `folderMustBeCreated` is part of the plan, not a shell judgment: the resolver marks the
  destination as needing creation and the shell performs the `create_directories` call.
  A folder configured earlier and since deleted is therefore recreated by policy rather
  than by a rescue path (spec Edge Cases, FR-014).
- Resolution is total: every combination of inputs yields a plan, including the refusals.
  There is no failure return.

---

## CaptureOutcome

What happened, recorded so the shell can report it without deciding what to say. The
shell performs the two sinks independently -- neither failure prevents the other
(FR-018) -- and records the result here.

| Field | Type | Notes |
|---|---|---|
| `refusal` | `CaptureRefusal` | carried through from the plan |
| `clipboardOk` | `bool` | false when the clipboard could not be opened or written |
| `fileWritten` | `bool` | false when saving was off, refused, or the write failed |
| `writeAttempted` | `bool` | distinguishes "saving is off" from "the write failed" |
| `path` | `fs::path` | the file actually written; empty otherwise |

`CaptureOutcome::DescribeResult (outcome)` is a **pure function in core** returning the
notice text: the filename on success, the refusal reason when refused, and the specific
failure otherwise. Putting it here rather than in the shell is what makes every branch of
FR-017, FR-018 and the minimized-window edge case testable without an `HWND` -- the shell
displays the string it is handed and chooses no wording.

The five inputs give eight reachable states (refusal, both sinks succeeded, clipboard
only by preference, clipboard only by write failure, file only, neither), and the test
covers each.

---

## ScreenshotFacts

What the metadata composer is told. Deliberately a flat record of already-formatted
strings and plain numbers: the composer's job is *which entries to emit for this mode*,
not how to render a monitor key or a pose.

| Field | Type | Source |
|---|---|---|
| `mode` | `ScreenshotMode` | |
| `versionString` | `string` | `"Casso " VERSION_STRING`, the same construction as the WOZ creator stamp |
| `machineDisplayName` | `string` | the machine JSON `name` field, e.g. `Apple //e` |
| `when` | `SYSTEMTIME` | injected clock |
| `utcOffsetMinutes` | `int` | injected, for the RFC 1123 zone |
| `monitorKey` | `string` | `CrtResolver::MakeKey` output |
| `scenePose` | `string` | the pose readout's own format; empty when unavailable |
| `crtParams` | `CrtParams` | resolved effect parameters |

---

## MetadataEntry

One `tEXt` chunk. A keyword and a value, both ASCII.

**Lives in `CassoEmuCore/Devices/Printer/PngMetadata.h`, beside `PngCodec`** -- not in
`Capture/`. The codec is the consumer that cannot do without it, and putting it in
`Capture/` would make the printer depend on the screenshot directory, inverting the
layering: `Capture/` is the new component that consumes the generalized printer pieces
(`PngCodec`, `PrintFileNaming`), never the reverse.

| Field | Type | Rules |
|---|---|---|
| `keyword` | `string` | 1-79 characters; no leading, trailing or consecutive spaces (PNG keyword rules) |
| `value` | `string` | ASCII; no newlines |

`ScreenshotMetadata::Compose (facts)` returns `vector<MetadataEntry>` in a **stable
order** -- the order in the contract table -- so a round-trip test can compare sequences
rather than sets, and so files written by successive builds diff cleanly.

Emission by mode is the contract in
[contracts/screenshot-metadata.md](contracts/screenshot-metadata.md). The composer is the
single authority for it; nothing else decides what a screenshot says.

---

## CapturedImage

The pixels in flight between readback and encode. Lives in the shell.

| Field | Type |
|---|---|
| `widthPx` / `heightPx` | `int` |
| `rgba` | `vector<Byte>`, tightly packed, top-down |

The staging texture's row pitch is not `width * 4` and is unpacked into this tight layout
during the copy-out, so nothing downstream carries a pitch.

The same buffer feeds both sinks: the clipboard DIB (rows emitted in reverse, since a
positive-height DIB is bottom-up) and `PngCodec::EncodeRgba`.

---

## Preference fields

Added to the global section of `GlobalUserPrefs`. Global rather than per-machine: they
describe the host and the user's habits, nothing about the emulated hardware.

| Key | Type | Default | Notes |
|---|---|---|---|
| `screenshotMode` | `string` | `"scene"` | token, never an index (FR-041) |
| `screenshotSaveFile` | `bool` | `true` | |
| `screenshotFolder` | `string` | `""` | empty means the default destination (FR-042) |

**Rules**

- Absent keys fall back to these defaults, per the struct's existing missing-field
  tolerance.
- Unknown keys continue to round-trip through `unknownPassthrough` untouched, so a file
  written by a newer build survives being loaded and re-saved here (FR-043).
- An unrecognized `screenshotMode` value loads as `Scene` and is **re-emitted as
  `"scene"`**, not preserved. It is a known key with an unknown value, which is different
  from an unknown key.

---

## Generalized: PrintFileNaming

Existing component, widened rather than duplicated.

`ComposePngPath (folder, when, taken)` becomes
`ComposeTimestampedPath (folder, baseName, extension, when, taken)`, with the print call
site passing `"Casso Print"` / `".png"` and screenshots passing `"Casso"` / `".png"`.

The deduplication policy -- bare name first, then ` (n)` -- is unchanged and stays the
single owner of collision handling for both outputs. Existing tests move to the new
signature; new cases cover the screenshot base name and a non-`.png` extension, so the
generalization is actually exercised rather than merely possible.

---

## Extended: PngCodec

`EncodeRgba (image, dpi, outPng)` gains an optional
`const vector<MetadataEntry> & textChunks` parameter, written through the frame's
metadata query writer before `Commit`.

The codec stays mechanical: it writes the entries it is handed and never decides what
they should be. The printer's existing call site passes none today and may later pass
`Software` for free.
