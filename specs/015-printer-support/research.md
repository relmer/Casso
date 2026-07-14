# Research: Emulated Printer Support (ImageWriter II)

**Feature**: 015-printer-support | **Date**: 2026-07-07

Each entry: Decision / Rationale / Alternatives considered. Empirical items
name the experiment that confirms them.

## R-001: Parallel card register contract

**Decision**: `PrinterCard` claims the slot's 16-byte I/O window
(`$C080 + slot*$10`; slot 1 → `$C090-$C09F`). Write to `$C0n0` latches a data
byte into the fixed-capacity SPSC ring (strobe implied). Read of `$C0n1`
returns status with the "ready" bit asserted while the ring has headroom and
**de-asserted within a high-water margin of the ring's capacity** — so a guest
honoring the handshake stalls rather than overflows if the drain ever falls
behind. All other offsets in the window read as ready/echo and ignore writes,
tolerantly — Print Shop interface drivers vary in which offsets they poke.

**Ring sizing / FR-002 guarantee**: The ring is sized generously (≥ 64 KB —
orders of magnitude beyond the fastest sustained 6502 `STA $C0n0` loop,
≈ 255 KB/s, across many drain intervals). The consumer drains on the presenter
tick, so in normal operation the ring sits near-empty and ready stays
continuously asserted — the "always ready" behavior drivers expect. The
high-water de-assert exists for the one case a pure sizing argument cannot
cover: the drain runs on the UI thread, and a **modal host dialog (e.g. the
Windows print dialog) can hold that thread for seconds while the guest keeps
writing**. Backpressure — not a bigger buffer — is what makes FR-002's
"never loses or reorders a byte" hold unconditionally for a handshake-honoring
guest. Overflow past the guard remains a programming error (debug assert); it
is unreachable while the guard is honored.

**Rationale**: The emulated pipeline is effectively infinitely fast, so honest
handshake semantics reduce to "ready except under an unobservable high-water
guard"; tolerance across the window maximizes compatibility with the several
parallel-card drivers Print Shop ships (Apple II Parallel, Grappler+,
Epson APL).

**Alternatives**: Cycle-accurate strobe/ACK timing — rejected; no consumer
can observe it and FR-002 only requires no byte loss.

**Empirical confirmation**: the byte-capture debug sink (R-014) is built
first; booting Print Shop with each parallel interface selection and diffing
captures verifies which offsets its drivers actually touch. The exact ready
bit position(s) get locked down then — the card can assert ready on all bits
that any driver tests without conflict.

**LOCKED (2026-07-14, live Print Shop capture — T011)**: ready = `$83`,
busy = `$00`. Print Shop's Grappler+ driver probes `(status & $07) == $03`
before sending a single byte (Centronics SELECT and FAULT# high with
PAPER-OUT **low** — so the old all-bits-set `$FF` guess failed the probe and
produced "PRINTER TEST FAILED" with zero bytes written). Our firmware's CSW
loop and Print Shop's Apple II Parallel driver both poll bit 7 set; `$83`
satisfies every observed probe at once. The Apple II Parallel driver polls
`$C0n4` and streams regardless; Grappler+ reads the status via `LDA abs,X`
with `X = slot*16`. Two further captures locked graphics behavior: the
welcome message arrives as `ESC L <lo> <hi>` (binary little-endian column
count, 512 for the test) followed by exactly that many column bytes, with
**bit 7 as the TOP pin** — the reverse of `ESC G`'s documented LSB-top
order (the message prints upside down otherwise).

## R-002: Slot firmware strategy

**Decision**: Original firmware written as
`CassoEmuCore/Devices/Printer/ParallelFirmware.a65` (~100 bytes): `PR#n`
BASIC output hook plus Pascal 1.1 protocol signature bytes and entry points
(init/write/status). Checked in alongside a generated
`ParallelFirmware.h` containing (a) the assembled bytes and (b) the source
text as a string literal. A unit test (`FirmwareParityTests`) assembles the
source with the in-repo CassoCore assembler and asserts byte equality with
the embedded array — drift between source and bytes fails the build's test
gate, with no build-order coupling between CassoCli and CassoEmuCore.

**Rationale**: FR-003 requires original firmware built from in-repo source;
the parity test enforces provenance without custom build steps. Pascal
signature bytes make slot scanners and Pascal-protocol printing recognize the
card.

**Alternatives**: MSBuild custom step invoking CassoCli — rejected
(build-order coupling, ARM64/x64 cross complications); shipping Apple's ROM —
prohibited (copyright, FR-003).

## R-003: ImageWriter II command subset

**Decision**: Implement the subset defined by FR-005, sourced from the
*ImageWriter II Technical Reference Manual* command set (C. Itoh 8510 base +
Apple extensions): printable ASCII with the built-in draft font; CR, LF, FF;
the documented character-pitch selections (72–160 dpi dot densities);
line-spacing controls including half-height (1/144") feeds; the bit-image
graphics commands (fixed-count and repeat forms); `ESC K n` seven-color
selection; software reset. Everything else is consumed-and-ignored (FR-009)
with a debug-log channel for unknown sequences.

**Rationale**: Covers Print Shop (graphics + color), New Print Shop, and
BASIC/DOS text listings without chasing the long tail (custom character
download, proportional justification — out of scope per spec Assumptions).

**Empirical confirmation**: captured byte streams from Print Shop test
prints (R-014) are the acceptance oracle; the TRM is the reference for each
command's exact grammar. Golden unit streams are hand-authored per command
family plus real captured streams checked in as fixtures (they are our own
generated data, not Apple-copyrighted content).

## R-004: Native raster cell model

**Decision**: `PrintRaster` stores one byte per native grid cell
(1280 dots/row, 144 rows/inch), low 4 bits = ink bitfield {black, yellow,
red, blue}. Composites are *derived* at render time from multi-primary cells
(yellow+red → orange, yellow+blue → green, red+blue → purple; any+black or
3+ primaries → black-dominant). Rows allocated in page-length chunks as the
strip grows; hard cap 60 form lengths (FR-015).

**Rationale**: Overprint mixing (FR-007/FR-027) falls out of the bitfield —
the interpreter just ORs the current ribbon primary into struck cells,
exactly like physical double-striking. 4 bpp keeps a max strip ≈ 60 MB.

**Alternatives**: palette-indexed 7-color cells — rejected: loses strike
history, makes composite rules eager instead of declarative.

## R-005: Paper renderer (ink model)

**Decision**: Deterministic CPU renderer in CassoEmuCore. For each struck
cell, stamp a round splat of pin diameter 1/72" (≈ 8 px at 576 dpi) centered
on the cell's true position on a square-pixel canvas at the configured
output dpi (288/576). Coverage via a precomputed antialiased disc kernel per
subpixel phase (fixed tables → bit-identical output across runs and
architectures; no floating-point accumulation order hazards). Per-primary ink
layers composite multiplicatively (subtractive mixing) onto paper white, then
a fixed tileable ribbon-weave modulation (few percent amplitude) multiplies
ink opacity. "Plain" style replaces the disc kernel with a cell-sized
rectangle, same pipeline (FR-027's square-dot style is the same renderer
with a different stamp).

**Rationale**: Determinism is a spec MUST (golden tests); CPU keeps the
renderer in CassoEmuCore and unit-testable; kernel tables make it fast
enough (a 576-dpi page is ~29 MP of mostly-untouched white).

**Alternatives**: D2D/GPU render — rejected for determinism and test
isolation; per-dot analytic coverage at render time — rejected as needless
FLOP burn versus precomputed kernels.

## R-006: Golden testing without file I/O

**Decision**: Interpreter goldens assert on the native raster: per-test
SHA-256 (via existing repo hashing utility or a small FNV/CRC in test code)
plus targeted cell assertions for readability. Renderer goldens hash the
RGBA output for fixed inputs and spot-check pixels (dot center, edge
antialiase, composite color values). All expected values are compile-time
constants in test sources.

**Rationale**: Constitution forbids unit-test file access; hashes keep tests
exact without embedding megapixel images.

## R-007: PNG encode/decode

**Decision**: WIC (`IWICImagingFactory`, PNG encoder/decoder) in the Casso
shell (`HostPrintServices`, `PrintJobStore`). Set pHYs DPI metadata from the
configured output resolution so pasted/printed images carry true physical
size.

**Alternatives**: stb_image_write — unnecessary (WIC is Windows SDK, already
the platform norm); GDI+ — legacy.

## R-008: Clipboard formats

**Decision**: Copy places two formats: registered `"PNG"` clipboard format
(compressed bytes at configured dpi) set immediately, and `CF_DIB` announced
via delayed rendering (`SetClipboardData(CF_DIB, nullptr)` +
`WM_RENDERFORMAT`), materialized only on request and downscaled if the strip
exceeds a fixed DIB size cap.

**Rationale**: Spec FR-013 decision; modern consumers get full fidelity,
legacy consumers get a bounded bitmap, and the giant-DIB failure mode is
designed out.

## R-009: Windows printing path

**Decision v1**: Classic `PrintDlgEx` + GDI: for each 11" page span,
`StretchDIBits` the renderer output into the printer DC at true scale
(`GetDeviceCaps(LOGPIXELSX/Y)`), centered, clipped per FR-014. Page count
shown in the eject confirmation per FR-014.

**Spike (time-boxed, before UI polish tasks)**: WinRT `PrintManager` via
`IPrintManagerInterop` — the modern system dialog with live preview, fed by
D2D page callbacks. Adopt in place of PrintDlgEx if it (a) works unpackaged,
(b) coexists with the app's threading model. Outcome recorded in tasks.md;
either way the `HostPrintServices` seam isolates the choice.

## R-010: Pending-strip persistence

**Decision**: Per machine, under the existing per-machine user-state
directory: `PendingPrint/strip.png` (native-grid raster encoded as an
indexed PNG — lossless, tiny for mostly-white strips) plus
`PendingPrint/strip.json` sidecar (format version, paper-advance position,
page-boundary rows, cap state). Serialize/deserialize to memory buffers in
`PrintJobSerializer` (pure, unit-tested); `PrintJobStore` does the file I/O.
Save on clean exit and after eject/discard; missing/corrupt/unknown-version
files → start with empty paper, silently (spec: crash loss acceptable).

**Alternatives**: proprietary binary blob — rejected (PNG is already
required, inspectable, compresses ideally); saving the *rendered* strip —
rejected (native grid is the source of truth; render settings may change
between sessions).

## R-011: Audio samples and playback

**Decision**: Extend the `AssetBootstrap` startup downloader catalog with a
printer sample set (consent flow, OGG fetch, `FetchAndDecodeOgg` →
`WritePcmAsWav`, provenance/license recorded in the asset manifest). Event
samples: head-burst loop, line-feed, form-feed/platen, paper tear.
Sourcing: search retro-community channels for an authentic ImageWriter II
recording under CC0/CC-BY; fallback to a licensed period 9-pin recording
(Freesound CC0 / Pixabay royalty-free), sliced into event samples at
preparation time (hosted pre-sliced, like the per-mechanism Disk II sets).
`PrinterAudioSource : IDriveAudioSource` registers with the existing
`DriveAudioMixer`; volume/mute follows the drive-audio settings pattern.

## R-012: Presentation pacing (FR-031)

**Decision**: `PrinterPresenter` (shell) replays interpreter events at
approximately real ImageWriter II draft speed — ~250 chars/s head travel for
text, graphics rows at the equivalent head-pass rate, line feeds ~20 ms per
1/144" step burst — driving both the panel's paper animation and
`PrinterAudioSource` events from the same clock. The raster is always
complete ahead of the presentation; a bounded presentation queue coalesces
when the guest outruns the show by more than ~2 pages (jump-cut forward with
a fast-feed sound); eject/discard fast-forward instantly.

**Rationale**: Sight and sound stay in lockstep from one event stream; the
guest is never throttled (FR-002/FR-018); impatience has an escape hatch.

## R-013: Title recognition tiers

**Decision**: Three pure matchers in `TitleRecognizer`, evaluated at Drive 1
mount in confidence order (FR-025 > FR-022 > FR-023 stops at first hit):
1. Container metadata: `WozLoader` retains META key/values (currently
   discarded at load); Title matched case-insensitively against the
   signature list.
2. Filename: case-insensitive substring match on the image basename.
3. Filesystem: decoded catalog via the existing
   `NibblizationLayer::Denibblize` path → DOS 3.3 catalog file names
   (high-bit-masked ASCII; no volume names) and ProDOS volume/file names
   (plain ASCII), matched against per-title name signatures.
The signature list ships as a small embedded table (title, display name,
filename substrings, catalog names) — bundled data, not user-editable in v1.

## R-014: Byte-capture debug sink (build-first)

**Decision**: The card's byte ring drains to pluggable sinks; the first sink
implemented is capture-to-file (developer feature, menu-gated), producing
the corpus that drives R-001/R-003 verification and golden fixtures. It also
doubles as the FR-009 unknown-command diagnostics channel.

## R-015: Machine config and upgrade

**Decision**: New `ComponentRegistry` device type `"parallel-printer"`.
Embedded `Machines/Apple2e*.json` gain
`{ "slot": 1, "device": "parallel-printer", "capability": "optional" }`
(no ROM file — firmware is embedded). `MachineConfigUpgrade` adds the slot 1
entry to existing user configs when absent and no other device occupies
slot 1; the existing `enabled` flag carries per-machine disable
(Settings → Hardware, existing UI). Firmware bytes are installed via
`CxxxRomRouter::SetSlotRom` at machine build time.

## R-016: Chrome placement

**Decision**: Indicator renders as a small dxui control anchored in the
right corner dead space of the existing bottom command/drive bar (drives'
centered composition untouched; same vanishing-point skew constant applied).
The panel is a right-edge surface built on the spec-007 `ChromeLayout`
primitives: transient overlay style when auto-revealed, reserving the edge
inset (window resize, emulator pixels preserved) only when the user pins it.
Detailed geometry follows the DxuiWindow architecture from spec 013.

**Update (2026-07-09)**: the panel shipped as a **separate top-level
`DxuiWindow`** (peer of the main window, like the Disk II / Input debug
panels), NOT a docked `ChromeLayout` edge surface — the user confirmed it
should be its own Dxui window (and it doubles as print preview, FR-020). The
indicator remains a chrome control; clicking it opens the panel window.

## R-017: Preview presentation architecture (FR-032/033/034)

**Decision**: Decouple content from presentation. The printed content (strip
ink + paper furniture + head-column ink reveal) renders to a **2D texture** in
pure, unit-testable code; a **real-3D** presentation layer maps that texture
onto a curled-paper surface in front of a procedurally-built ImageWriter
chassis (anchored at the panel bottom, paper curling out of view above a
~1-page viewport). 3D is a *contained additive* path on the existing Dxui
**D3D11** pipeline (custom HLSL shaders + vertex buffers already present; the
text renderer already samples textures): add an MVP constant buffer, one
textured/lit shader, and two meshes (chassis + dynamically-curled paper). No
new engine, no image asset, no new third-party dependency.

**Rationale**: The hero visual — printout mapped onto curling paper receding in
perspective — is genuinely 3D and reads as flat trapezoids if faked in 2D. The
chassis and paper share one camera, so both go 3D together. It becomes the
pilot 3D primitive the drive widgets adopt when they move to true 3D — a
lower-stakes place to prove the path than always-visible drive chrome.
Isolating 3D to the final stage keeps the content pipeline testable and leaves
a flat 2D fallback available (FR-032). Caveat: the 3D scene needs a live user
eyeball for aesthetic tuning; the 2D content phases self-verify (testable math
+ capturable panel).

## R-018: Long-banner memory — delivery cap + incremental preview

**Decision**: Two independent bounds. (1) **Delivery** (PNG/clipboard) caps the
whole-strip render dpi against a fixed RGBA budget (`WholeStripDpi()`, ~512 MB),
dropping from 576 toward but not below the ~160×144 native grid, and renders the
strip ONCE — the clipboard PNG is encoded from that same image (was a double
render). Effectively lossless (no source detail above native). (2) **Preview**
renders only newly-produced rows into a persistent tile buffer inside a ~1-page
`PrinterViewport` (FR-033), so per-frame cost is flat regardless of strip
length. The interim shipped fix (activity-gated, strip-scaled refresh throttle)
is a stopgap the viewport replaces.

**Rationale**: A 60-page banner is ~4608 × ~700k px at 576 dpi; materializing it
(let alone twice) exhausts memory, and re-rendering it every frame is O(rows²).
Bounding delivery by budget and the preview by viewport fixes both at the root
(SC-010).
