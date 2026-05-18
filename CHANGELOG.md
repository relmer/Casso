# Changelog

All notable changes to Casso are documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/).
Versioned entries use `MAJOR.MINOR.BUILD` from [Version.h](CassoCore/Version.h).
Entries before versioning was introduced use dates only.

## [1.3.747] — Disk II Debug Window polish (spec 006 smoke-test fixes)

### Fixed
- **Cycle count column always read 0** — the `DiskIIEvent.cycle` field
  was never populated because each Push* helper zeroed it at struct
  init. The dialog now holds a `const uint64_t *` pointing at the
  CPU's running cycle accumulator and stamps the value at SPSC-push
  time. EmulatorShell wires the pointer at dialog-open time and
  re-points it across `SwitchMachine`.
- **Drive radio off-by-one** — selecting "Drive 1" hid every event
  because the predicate compared the 1-based UI value (`1`) to the
  0-based internal drive index (`0`). Now compares
  `event.drive == (driveFilter - 1)`. Events without a drive index
  (motor, head, address-mark, data-read on the shared spindle)
  bypass the drive predicate so they aren't hidden when filtering
  by drive — symmetric with the track / sector predicates.
- **Reset didn't clear the debug view** — `SoftReset` /
  `PowerCycle` re-zeroed the Uptime anchor but left the deque
  (and any in-flight ring events) full of pre-reset rows. New
  `DiskIIDebugDialog::ClearEvents` drains the ring, wipes the
  deque + filtered indices, and rebuilds the LV count. Pause /
  resume state is preserved (a paused user can still inspect
  post-reset events after resuming).
- **Auto-sized columns only fit header text** — initial sizing used
  `LVSCW_AUTOSIZE_USEHEADER`. Replaced with a custom
  `MeasureColumnContentWidth` that walks the deque measuring each
  cell via `LVM_GETSTRINGWIDTH` and takes max (header, widest cell)
  + 16 px padding. The Detail column always flexes to fill the LV
  client remainder (re-flowed on `WM_SIZE` so dialog resize moves
  free space into Detail rather than leaving it dead at the right
  edge).
- **ListView flashed on filter checkbox toggles** —
  `InvalidateListView` now short-circuits when the projection is
  unchanged and wraps the SetItemCount + InvalidateRect in
  `WM_SETREDRAW(FALSE/TRUE)` so the LV repaints once at the end
  of the rebuild.

### Changed
- **Event labels** switched from `ALL CAPS` to `Sentence case`
  (`Motor command on`, `Head step`, `Audio loop started`, …).
- **Column headers**: `Wall` → `Time`, `Cycle` → `Cycle count`.
  New `Drive` column between `Cycle count` and `Event` carrying the
  user-facing 1-based drive number; redundant `drive=N` text was
  removed from Detail strings for `Drive select` /
  `Disk inserted` / `Disk ejected` / all `Audio *` events.
- **Head-step / head-bump detail strings** now spell out
  `quarter-track <prev> -> <new>` and
  `at quarter-track <position>` instead of the cryptic `qt=`.
- **Filter checkbox** `raw qt` renamed to
  `Quarter-track steps`.
- **Track / Sector filter inputs** are now labelled (`Track filter:`,
  `Sector filter:`) and document their syntax in a hover tooltip.

### Tests
- Existing 417-test suite green after refactor. `FilterState`
  drive-radio test updated for the off-by-one fix (UI value 1
  matches internal index 0). `DiskIIDebugDialogColumnTests`
  expanded to cover the six-column model.



## [1.3.730] — Disk II Debug Window

### Added
- **Disk II Debug Window (spec 006)**: modeless live event log of
  motor / head / address-mark / data-mark / drive-select / insert-
  eject / audio events from the active Disk II controller. Opens via
  **View → Disk II Debug...** or **Ctrl+Shift+D**. Filterable by
  event type, drive, track, sector, and audio outcome
  (started / restarted / continued / silent). Auto-tail scrolling
  when the user is at the bottom; pause / resume / clear controls.
  Ctrl+C copies the selected rows tab-separated in visible-column
  order. Right-click the column header to show / hide individual
  columns (in-session only; defaults restore on dialog re-open per
  NFR-006). Uptime column re-zeroes on every soft-reset and power-
  cycle. The menu item is grayed out automatically on machines
  without a Disk II controller (FR-001a); when more than one Disk II
  controller is wired, the title becomes "Disk II Debug (controller
  #0 only)" (FR-017). Events emitted before the dialog opens are
  not retained -- open it *before* the operation you want to
  investigate.
- **`IDiskIIEventSink` / `IDriveAudioEventSink`**: two new sink
  interfaces (controller side + audio side) the debug dialog
  implements simultaneously; the shell attaches and revokes both
  in the same lifecycle window. Sinks are nullptr-default and
  controller behavior with no sink attached is byte-identical to
  pre-feature (SC-007, SC-010).
- **`DiskIIEventRing`**: lock-free SPSC ring (4096 capacity)
  buffering producer-side events between the CPU thread and the
  UI-thread drain timer; overflow returns false without corruption
  and coalesces into a single `[N events lost]` marker on the next
  drain (FR-010).
- **`DiskIIAddressMarkWatcher`**: two state machines that decode
  address marks (with volume number) and data marks from the
  controller's nibble stream; bad checksums and mid-stream resync
  are tolerated without false positives.
- **Track / sector filter syntax**: integers, decimals, ranges,
  lists, and an opt-in raw quarter-track mode. Unparseable tokens
  are rejected with an inline squiggle (RichEdit) and listed in a
  hover label rather than crashing the filter.
- **Apple2/Demos/**: four bundled WOZ fixtures (Apple Stellar
  Invaders, Choplifter, Hard Hat Mack, Karateka) for manual A/B
  observation of the debug stream.

### Changed
- `IDriveAudioSink` audio-event method names normalized so the
  controller-side and audio-side sinks present a parallel surface
  to `DiskIIDebugDialog`.
- `EmulatorShell` owns a shell-wide uptime anchor (`steady_clock`)
  re-zeroed on `SoftReset` / `PowerCycle`; the debug dialog reads it
  on every drain so the Uptime column tracks the active //e's
  power-on age, not the host process.
- `Window` base class grows a virtual `OnInitMenuPopup` hook to
  support FR-001a runtime menu-item gating.

### Tests
- 156 new tests across the spec-006 surface: SPSC ring (push fills,
  pop drains, wrap, overflow), address-mark watcher (stock cadence,
  bad checksum, mid-stream resync), projection (FormatEvent column
  shapes, DrainAndProject FIFO + EventsLost ordering, rolling cap),
  filter state and the track/sector predicate parser, RichEdit
  squiggle helpers, clipboard payload builder, column-model
  planner, FR-001a enablement decision, FR-004a uptime-reset path.

## [1.3.684] — Disk II mechanism dropdown + per-machine persistence

### Added
- **Options dialog mechanism dropdown (FR-006 / SC-010)**:
  *View → Options...* now offers a "Disk II mechanism" combobox
  with "Shugart SA400" (default) and "Alps 2124A" entries. Flipping
  the dropdown reloads every registered drive's sample buffers via
  `DriveAudioMixer::SetMechanism` and takes effect on the next
  audio frame -- no restart, no disk remount.
- **`DriveAudioMixer::SetMechanism / SetSampleLoadContext /
  GetMechanism / IsValidMechanism`**: the mixer now owns the
  asset-load context (devices dir + sample rate) and a
  validated-on-set mechanism string. Bad input returns
  `E_INVALIDARG` without mutating mixer state (SC-010 invariant).
- **Per-machine persistence (Q4)**: both the Drive Audio toggle
  and the active mechanism round-trip through
  `HKCU\Software\relmer\Casso\Machines\<MachineName>\` using new
  `RegistrySettings::ReadDword / WriteDword` helpers. Defaults are
  enabled + Shugart when the registry is empty. State is reapplied
  at `EmulatorShell::Initialize` after the machine config loads,
  before the CPU thread first touches the mixer.

### Changed
- `OptionsDialog::Show` signature gains a current/out mechanism
  pair; the procedurally-built `DLGTEMPLATE` adds STATIC and
  COMBOBOX entries (atoms 0x0082 / 0x0085).
- `EmulatorShell` Options OK handler diffs both knobs separately
  so changing one does not rewrite the other's registry value.

### Tests
- `UnitTest/Audio/DriveAudioMixerMechanismTests.cpp` adds four
  tests covering: invalid mechanism (no state change),
  multi-source reload, Alps→Shugart round trip with distinct
  amplitudes, and pre-context SetMechanism (defers load).

## [1.3.682] — Disk II audio bootstrap (consent-gated OGG fetch)

### Added
- **Bootstrap fetch (FR-017, FR-018, NFR-006)**: on first launch with
  a machine that has a Disk II controller, Casso offers (TaskDialog
  with three command links: *Download* / *Skip* / *Don't ask again
  this session*) to download the OpenEmulator drive-noise samples
  from `raw.githubusercontent.com/openemulator/libemulation`,
  decode them in memory with `stb_vorbis`, resample to 44.1 kHz
  via linear interpolation, and write 16-bit mono PCM WAVs to
  `Devices/DiskII/<Mechanism>/`. The compressed `.ogg` bytes are
  discarded before the function returns — no `.ogg` files ever
  touch disk (NFR-006).
- The consent dialog explicitly discloses GPL-3 licensing and
  recipient obligations and links to OpenEmulator's COPYING file
  and the GPL-3 text. *Don't ask again this session* is per-process
  and resets at next launch (deleting the per-mechanism subfolders
  re-triggers the prompt).
- Five Shugart sounds (motor, head step, head stop, door open,
  door close) and three Alps sounds (motor, head step, head stop;
  Alps drives have no door) covered by `s_kDiskAudioCatalog` in
  `AssetBootstrap.cpp`.
- `CassoEmuCore/External/stb_vorbis.c` vendored from
  [github.com/nothings/stb](https://github.com/nothings/stb)
  (public domain / MIT). Included exclusively through
  `StbVorbisWrapper.cpp` which disables PCH, code analysis, and a
  documented set of upstream-rejected warnings so the rest of the
  codebase stays clean. Compiled with
  `STB_VORBIS_NO_PUSHDATA_API` + `STB_VORBIS_NO_STDIO` to drop the
  half of the library we don't need.

### Changed
- `AssetBootstrap::DownloadHttp` now treats `expectedSize == 0` as
  "no integrity check" (only "non-empty"), enabling the OGG fetch
  to reuse the existing WinHTTP plumbing.

### Tests
- `UnitTest/EmuTests/DiskAudioFetchTests.cpp` adds four tests:
  null / garbage-bytes guards for `StbVorbisWrapper`, a WAV
  write + `DiskIIAudioSource::LoadSamples` round-trip that asserts
  a non-silent motor loop after decode, and the FR-019 per-file
  precedence rule. The network-touching `AssetBootstrap` glue is
  exercised by the manual integration test in T138 (per
  constitution §II — automated tests do not hit the network).

## [1.3.675] — Per-machine asset directory layout

### Changed
- **Per-machine ROM directories**: ROM images now live under
  `Machines/<MachineName>/` (e.g., `Machines/Apple2e/Apple2e.rom`,
  `Machines/Apple2e/Apple2e_Video.rom`) instead of a single
  top-level `ROMs/` folder. Shared device boot ROMs (Disk II
  controller firmware) live under `Devices/<Family>/` (e.g.,
  `Devices/DiskII/Disk2.rom`). The in-app missing-ROM downloader
  and `scripts/FetchRoms.ps1` both target the new layout. The
  Apple II / II+ character generator and the //e character
  generator are duplicated into each owning machine's folder so
  every machine's asset set is self-contained (a handful of bytes
  of redundancy in exchange for portability).
- **Machine configs moved**: `Machines/Apple2.json` →
  `Machines/Apple2/Apple2.json` (and the same pattern for
  `Apple2Plus`, `Apple2e`). `Casso.exe`'s `--machine` flag still
  takes the bare machine identifier (e.g., `--machine Apple2e`);
  the loader resolves the new nested path internally. Embedded
  default-config extraction (`AssetBootstrap::EnsureMachineConfigs`)
  writes to the per-machine subdir on first run.
- **`.gitignore` is now a whitelist** for `Machines/**` and
  `Devices/**`: only `*.json` manifests are tracked, ROMs and
  future drive-audio WAVs stay out of source control without
  per-file rules.
- `Assets/Sounds/DiskII/README.md` moved to
  `Devices/DiskII/README.md` to co-locate documentation with the
  device's other assets.

### Migration
Users with an existing install:

- The old top-level `ROMs/` directory is **no longer searched**.
- After updating, either delete `ROMs/` and re-run
  `scripts/FetchRoms.ps1` (which now places files in the new
  layout), or move each ROM file into the corresponding new
  location (see the table at the top of `scripts/FetchRoms.ps1`).

## [1.3.670] — Disk II audio (motor / head / door, stereo, Options dialog)

### Added
- **Disk II mechanical audio**: motor hum (looping while
  `m_motorOn`), head-step click (per quarter-track movement), track-0
  / max-track bump (when the stepper energizes against the travel
  stop), and disk insert / eject door sounds.
- **Step-vs-seek discrimination** (FR-005): contiguous step bursts
  fuse into a continuous seek buzz instead of N overlapping clicks.
  OpenEmulator-style cycle-gap timer; threshold = 16,368 cycles
  (~16 ms at 1.023 MHz), idle clear = 51,150 cycles (~50 ms).
- **Stereo mixing into the existing WASAPI pipeline**: speaker is
  centered (equal-power), drives are panned per-drive using
  equal-power coefficients. Single-drive profiles play centered;
  two-drive profiles place Drive 1 left-of-center and Drive 2
  right-of-center. Per-channel clamp to `[-1, +1]`; downmix to mono
  when the device is mono.
- **View → Options... dialog** (new, runtime-built `DLGTEMPLATE`)
  hosting the "Drive Audio" check toggle; default-on, takes effect
  within one audio frame.
- **Cold-boot mount suppression** (FR-013): command-line / restored /
  autoload mounts do not fire the disk-insert sound. User-initiated
  mid-session mounts and all eject events fire normally.
- **Generic drive-audio abstraction**: `IDriveAudioSink`,
  `IDriveAudioSource`, `DriveAudioMixer`, and `DiskIIAudioSource` are
  decoupled so future drive types (`//c` internal 5.25, DuoDisk,
  Apple 5.25 Drive, Apple /// drive, ProFile, …) can plug into the
  same mixer without modifying it or the sink interface (FR-016).
- `Assets/Sounds/DiskII/` directory with a `README.md` documenting the
  expected sample set (PascalCase WAVs decoded at startup via
  `IMFSourceReader` to mono float32 at the WASAPI device rate). The
  directory may be absent or empty — the emulator launches and runs
  normally with the affected sounds silently muted (FR-009).

### Changed
- `WasapiAudio` now negotiates stereo float32 from WASAPI (falls back
  to mono if the device demands it). The internal pending-samples
  buffer is interleaved stereo; mono devices downmix at drain time.
  `SubmitFrame` gained two optional parameters: a `DriveAudioMixer*`
  and the current CPU cycle count, both of which preserve
  pre-feature behavior when omitted (FR-011 / SC-006).
- `DiskIIController` exposes `SetAudioSink (IDriveAudioSink*)` and
  fires `OnMotorStart` / `OnMotorStop` / `OnHeadStep(qt)` /
  `OnHeadBump` at the documented call sites. Mount/eject door events
  fire from the shell layer (with cold-boot suppression) rather than
  the controller.

### Tests
- 34 new unit tests in `UnitTest/Audio/` and
  `UnitTest/Devices/DiskIIControllerAudioTests.cpp` covering source
  state machines, mixer panning / clamp, controller event firing,
  step-vs-seek timing, and graceful-degradation behavior. All
  tests use in-memory buffers and a recording mock sink — no host
  filesystem reads, no audio device (constitution §II).
- All pre-existing speaker tests pass identically (FR-011 / SC-006).

## [1.3.660] — 2026-05-14 — Demo first-frame ~2x faster (boot reorder)

### Changed (demo)
- **Disk layout reordered so DHGR loads first.** Previously the disk
  was laid out cassowary→stage2→bands→dhgr-aux→dhgr-main, and stage 2
  init read all 9 tracks before showing the cassowary (~2.25s).
  Now: dhgr-aux→stage2→dhgr-main→cassowary→bands. Stage 1 reads
  3 tracks (DHGR aux + stage 2/lores), stage 2 reads 2 more tracks
  (DHGR main) and immediately enters DHGR mode — visible at ~5
  tracks (~1.25s, ~45% faster). HGR1 cassowary and HGR2 bands
  load in the BACKGROUND while the user is looking at the
  cassowary; both done by the time the user can react with a
  keystroke.
- **HGR1 cassowary now loads directly to its `$A000` stash
  location**, eliminating the boot-time `$2000`→`$A000` memcpy.

### Fixed
- **`enter_dhgr` was clobbering X.** `copy_block` (called by
  `enter_dhgr`) used X as its 32-page counter, leaving X=0 on
  return. With the new background-load phase needing X=$60 (slot 6
  << 4) for indexed disk-controller soft-switch reads, this caused
  the next `lda $C087,x` head-step to read the wrong address —
  head never moved, RWTS spun forever in `chk_w` waiting for a
  non-existent disk byte. The previous flow happened to call
  `enter_dhgr` AFTER all disk I/O was done so the bug was latent.
  Stage 2 now reloads `ldx #$60` after `enter_dhgr` and before
  resuming disk I/O; documented in a new comment.

### Tests
- Demo test cycle budget shrunk from 60M to 10M cycles
  (~9.8 sec emulated vs ~58 sec). Test runtime dropped from
  ~9s to <1s. Full suite now ~93s instead of ~180s.

## [1.3.652] — 2026-05-14 — DHGR cassowary matches HGR framing + title

### Changed (demo)
- **DHGR cassowary now uses the same crop, letterbox, and "Casso"
  title as the HGR cassowary.** `DhgrCassowaryGen.py` now imports
  `HgrPreprocess` and feeds the source through
  `HgrPreprocess.crop_and_fit` (HGR's 280×192 letterbox + title
  pipeline) before resizing to DHGR's 140×192 color resolution
  for quantization. On screen the two modes show the bird at
  identical framing — only the colour treatment differs (HGR's
  6-color per-byte classification vs DHGR's 16-color
  Floyd-Steinberg dither).

### Fixed
- **`HgrPreprocess.py` was broken.** A prior edit had pasted the
  body of `paint_title` into `generate_dhgr_bands` after its
  `return` statement and lost the `def paint_title(...)` line, so
  any invocation with a title argument would fail with
  `NameError: paint_title is not defined`. The committed
  `cassowary.hgr` was generated before the regression and nobody
  noticed because the generator isn't wired into CI; only the
  consumed bytes are. Restored as a proper top-level function and
  generalised the centering to use `canvas.width` so the same
  helper works for HGR (280) and any future width.

## [1.3.651] — 2026-05-14 — Demo cycle reorder, DHGR aspect, exit garbage, amber mono

### Changed (demo)
- **DHGR cassowary is now mode 0** — first thing you see at boot,
  in glorious 16-color dithered double-hi-res. Cycle order:
  DHGR → HGR1 → HGR2 → LoRes → exit. Stage 2 init now stashes the
  HGR1 cassowary at main `$A000` so mode 1 can restore it after
  DHGR's `$2000` clobber.
- **DHGR cassowary aspect ratio fixed.** The image was previously
  squashed horizontally because the source photo (880×1600
  portrait) was force-resized to 140×192 without aspect
  correction. Now uses `ImageOps.fit` to centre-crop to the
  display's 4:3 aspect first (560:384, what the renderer
  actually shows), then resamples to 140×192.

### Fixed
- **Amber Monochrome mode now actually shows amber, not blue.**
  After the framebuffer-format switch from RGBA to BGRA, the
  monochrome-tint code in `EmulatorShell::RenderFramebuffer`
  was reading R/G/B from the wrong byte positions AND
  reconstructing in the wrong order, so amber's `(L, L×0.75,
  0)` triple landed as `B=L, G=L×0.75, R=0` — a cyan-blue
  pixel. Refactored into `Video/MonochromeTint.h` (new pure
  header in CassoEmuCore) so the BGRA arithmetic is now
  unit-tested.
- **Demo exit no longer leaves 80-col garbage on screen.** The
  previous fix attempted to clear AUX text page 1 by toggling
  RAMWRT, but with 80STORE on (which DHGR mode set) the writes
  to `$0400-$07FF` were still routed by PAGE2, not RAMWRT —
  so AUX never got cleared. `do_exit` now turns 80STORE off
  first, then RAMWRT-toggles its way through both pages via
  a new shared `clear_text_page1` subroutine.

### Added (tests)
- **`MonochromeTintTests`** (7 new tests). Pins the green/amber/
  white tint helpers against the Rec.601 luma weights and BGRA
  byte order. Suite is now 1053/1053.
- **Demo test was rewritten for the new mode order** and now
  also verifies the cassowary stash at main `$A000` and the
  DHGR boot-landing state (DHGR aux at aux `$2000`, DHGR main
  at main `$2000`).

### Refactor
- **`Video/MonochromeTint.h`** — new header in CassoEmuCore.
  Provides `Luminance()`, `TintGreenMono()`, `TintAmberMono()`,
  `TintWhiteMono()` as pure inline functions over the BGRA
  pixel format. EmulatorShell now calls these instead of
  open-coding the byte-shuffling.

### Known limitations
- HGR (single hi-res) cassowary still uses per-byte palette
  classification with no error diffusion — looks blocky next
  to the dithered DHGR version. A real fix would require
  rewriting `HgrPreprocess.py` to do bit-on/bit-off Floyd-
  Steinberg within each byte's palette-pair constraint;
  deferred.

## [1.3.645] — 2026-05-14 — DHGR cassowary + clean exit from DHGR mode

### Added (demo)
- **DHGR mode now shows a cassowary**, not just test bars. New
  generator `scripts/DhgrCassowaryGen.py` quantizes the source
  photo (`Assets/3a Mrs Cassowary closeup 8167.jpg`) to the //e
  16-color LoRes/DHGR palette via Floyd-Steinberg dithering and
  encodes it into the DHGR aux+main interleaved byte stream.
  Resulting payloads (`Apple2/Demos/dhgr-cassowary-{aux,main}.bin`,
  8 KB each) replace the test-bars patterns on tracks 6+7 / 8+9
  of the demo disk. A preview PNG is also emitted alongside so
  the quantization can be sanity-checked without booting the
  demo.

### Fixed (demo)
- **Exit from DHGR mode no longer leaves garbage glyphs on the
  BASIC screen.** Two issues:
  1. The reset handler doesn't clear the screen, only the soft
     switches. With 80COL still on after DHGR, BASIC was showing
     main+aux text page 1 interleaved — main had the LoRes
     pattern, aux still held power-on PRNG noise we never
     touched. `do_exit` now clears AUX text page 1 (under
     RAMWRT-on) in addition to main before jumping through
     the reset vector.
  2. `JMP $E000` (Applesoft cold start) is fragile even after
     manual soft-switch cleanup — it JMPs through stale work-
     area pointers and lands in video memory. `JMP ($FFFC)`
     (the //e reset vector) goes through `RESET.MGR` which
     does the full power-on cleanup before entering Applesoft
     and is the canonical way to bail to BASIC from any video
     state.

### Removed
- `Apple2/Demos/dhgr-bars-{aux,main}.bin` — superseded by the
  cassowary payload. The generator (`scripts/DhgrBarsGen.py`)
  is kept for future test-pattern needs (regenerates the .bin
  files on demand).

## [1.3.640] — 2026-05-14 — DHGR mode joins the demo cycle

### Added (demo)
- **Mode 3: DHGR** — 16-color test bars rendered through the //e
  Double Hi-Res pipeline (560x192 monochrome / 140x192 in 16
  colors, aux+main interleaved at $2000-$3FFF). Cycles cleanly
  from LoRes via any keystroke. Adds 16 KB of new payload to the
  demo disk: tracks 6+7 hold the DHGR aux pattern (8 KB) staged
  to main $6000-$7FFF, tracks 8+9 hold the DHGR main pattern
  (8 KB) staged to main $8000-$9FFF.
- **Demo exit now goes through the //e reset vector** (`JMP
  ($FFFC)`) instead of jumping directly to `$E000`. The reset
  handler does the full power-on cleanup so any mode of the
  cycle can land on a vanilla BASIC prompt — important now that
  the cycle includes DHGR (which leaves 80STORE / 80COL / DHIRES
  on, and Applesoft cold start at `$E000` doesn't tolerate that
  state and ends up executing video memory).

### How the DHGR-from-disk technique works
- Stage 1's RWTS uses pages `$02-$03` as a 6-and-2 secondary-nibble
  scratch buffer. Loading directly to aux RAM via that RWTS
  doesn't work because RAMWRT-on routes the scratch writes to
  aux while the readbacks still fetch from main, scrambling
  the GCR decode.
- The fix is a **two-step staged load**: read the DHGR aux bytes
  into a main-RAM scratch area (no soft-switch wrangling, RWTS
  works normally), then memcpy into aux $2000 with RAMWRT
  toggled around just the copy loop. The copy uses indirect-
  indexed `(zp),y` so the page-counter increments live in zero
  page, which is ALTZP-routed (always main with ALTZP off) and
  unaffected by RAMWRT. Self-modifying-code variants would
  break here because `inc`'s writeback would land in aux,
  corrupting the running code.
- Stage 2 grew from 125 to 232 bytes (still in one sector); the
  new `copy_block` subroutine is 29 bytes and is reusable for any
  future feature that needs to populate aux RAM from disk.

### Added (tooling)
- **`scripts/DhgrBarsGen.py`** — generates the 16-color DHGR test
  pattern (8 KB aux + 8 KB main) by walking the 560-dot row,
  picking each 4-dot group's color from the bar containing the
  group's center dot, and packing nibbles LSB-first into the
  aux/main interleaved byte stream.

## [1.3.632] — 2026-05-14 — Loosen perf-stability tolerance for hosted CI runners

### Changed (tests)
- **`CycleEmulation_StableRunToRun` tolerance bumped 30% → 60%.** A
  GitHub Actions runner produced a 42% outlier on an otherwise
  clean run (median 13.28 ms, worst 18.83 ms vs 17.27 ms tolerance)
  and failed the build. Shared cloud hardware can stall any given
  process for tens of ms without warning; the test still catches
  any real perf regression, but no longer trips on hypervisor
  scheduling hiccups.

## [1.3.627] — 2026-05-14 — Framebuffer format swap to BGRA + byte-order tests

### Changed (rendering)
- **Framebuffer format is now `DXGI_FORMAT_B8G8R8A8_UNORM`** (was
  `R8G8B8A8_UNORM`). On-screen colors are visually identical;
  every Windows pixel surface (CF_DIB clipboard, GDI bitmaps, BMP,
  WIC) natively uses BGRA, so image-export paths no longer need to
  swizzle R/B on the way out. All palette literals (NTSC HGR, LoRes,
  DHGR) re-encoded into the human-natural `0xAARRGGBB` form;
  see new `Video/PixelFormat.h` for the project-wide convention.

### Fixed
- **Menu → Copy Screenshot now produces correct colors.** The
  clipboard path used `CF_DIB` (BGRA) but blindly memcpy'd from the
  RGBA framebuffer, so screenshots had R and B swapped — most
  visible on the HGR cassowary demo (orange head appeared blue).
  With the framebuffer format change above, the screenshot copy is
  now a straight memcpy with no swizzle, so this class of bug can
  no longer arise.

### Added (tests)
- **`PaletteByteOrderTests`** (8 new tests). Decomposes every named
  palette color via `Video/PixelFormat.h` extractors and asserts
  the bytes match the human-documented R/G/B intent. Catches both
  hand-typed nibble swaps and any future format flip that isn't
  propagated to the palette literals. Brings the suite to 1046
  tests.

### Fixed (demo)
- **Exit to BASIC no longer leaves LoRes garbage on the text page.**
  After cycling past the LoRes test pattern, page 1 still held the
  LoRes byte pattern. Once we flipped back to TEXT mode those bytes
  rendered as character codes — anything in `$40-$7F` is in the
  flash range, so half the screen was blinking nonsense around the
  `]` prompt. Stage 2 now clears `$0400-$07FF` to `$A0` (space)
  before `JMP $E000`. Stage 2 size is now 125 bytes (still in one
  sector).

### Changed (docs)
- **README screenshot updated** to show the HGR cassowary demo
  instead of the older GR color-bands placeholder. Retired
  `Assets/Apple ][ GR Color Bands.png`.

## [1.3.619] — 2026-05-14 — Demo: TEXT mode on exit + American spellings

### Fixed (demo)
- **Cycling past LoRes now actually drops to a usable BASIC prompt.**
  The previous revision did `JMP $E000` (Applesoft cold start) but
  Applesoft's cold start doesn't reset the video soft-switches, so
  we landed at the `]` prompt with the screen still rendering as
  LoRes graphics — characters typed afterward updated text page 1
  but were invisible (or appeared as colored blocks). Now stage 2
  flips `TXT` on (and clears `HIRES`/`PAGE2` for good measure)
  before the `JMP $E000`.

### Changed (docs / source)
- **Spelling: standardized on American English** in source, comments,
  CHANGELOG, and stage-2 demo header. Replaced `colour` →
  `color`, `artefact` → `artifact`, `behaviour` → `behavior`,
  `synthesise` → `synthesize` in newly authored content.

## [1.3.618] — 2026-05-14 — LoRes test pattern + ESC-to-BASIC exit

### Added (demo)
- **LoRes (Apple `GR`) 16-color bar test pattern.** New
  `scripts/HgrPreprocess.py --pattern lores-bars` emits a 1 KB
  text-page-1 ($0400-$07FF) image with 16 horizontal stripes of
  LoRes palette indices 0..15. The pattern is shipped on the
  bootable demo disk on track 3 logical sectors 1-4 (which stage
  1 of casso-rocks reads to $1100-$14FF as part of staging
  stage 2).
- **Demo cycle now walks the standard Apple //e graphics modes
  with one keystroke each, then exits to Applesoft.** Stage 2
  starts in HGR1 (cassowary), advances on each keystroke through
  HGR2 (the existing 6-color bands), GR (the new LoRes bars),
  and on the next keystroke `JMP $E000` lands at Applesoft cold
  start (`]` prompt). Pressing **ESC** at any time also exits.
  Verified end-to-end in
  `BootDiskTests::CassoRocks_DemoDisk_DisplaysHgrCassowary`.

### Notes
- DHGR is intentionally not part of this cycle yet. Loading the
  DHGR aux page via stage 1's RWTS doesn't work directly: stage 1
  uses zero-page-adjacent pages $02-$03 as a 6-and-2 secondary-
  nibble buffer (writes via `STA $0256,Y` then reads back via
  `LDA $0300,Y`), and forcing RAMWRT-on around the disk read
  would route those scratch writes to aux while the reads still
  come from main, scrambling the expand. A clean way to add DHGR
  later would be to load the aux pattern into a main-RAM scratch
  area first (e.g. $6000-$7FFF) and then memcpy it to aux with
  RAMWRT toggled around just the copy.
- The //e text mode is monochrome on stock hardware (no per-glyph
  color), so there's no "TEXT" color test to add.

## [1.3.603] — 2026-05-14 — HGR color fix + 6-color test pattern + 2-stage demo

### Fixed (video)
- **HGR/LoRes/DHGR color palettes were rendering with R and B swapped**
  due to a byte-layout mismatch between the `0xAARRGGBB` notation
  used in `CassoEmuCore/Video/NtscColorTable.h`,
  `AppleLoResMode.cpp`, and `AppleDoubleHiResMode.cpp` and the
  `DXGI_FORMAT_R8G8B8A8_UNORM` swap-chain format set by
  `D3DRenderer.cpp`. Symptom: HGR `BLUE` rendered as orange and
  vice versa (anything in the //e's blue/orange palette pair came
  out swapped); LoRes and DHGR color indices 1 (Magenta), 2 (Dark
  Blue), 7 (Light Blue), 8 (Brown) all rendered as red shades.
  Violet/Green and the greys happened to be R/B-symmetric and
  rendered correctly by accident, hiding the bug from any HGR
  content that didn't lean on the blue/orange pair.
  Constants are now stored in R8G8B8A8 byte layout (so the
  little-endian `uint32_t` literal reads as `0xAA BB GG RR`).
  Affected golden-hash tests (`DhrTestPattern_HashMatches_Golden`,
  `US4_MixedMode_80Col_GoldenOutput`) and pixel-equality tests
  (`Render_SinglePixelPalette*`, `HiRes_NTSCArtifact_*`,
  `LoRes_TopBottomNybbles_*`) were updated with the corrected
  expected values.

### Added (demo)
- **Two-stage `casso-rocks` boot disk now toggles between the
  cassowary and a synthetic 6-color HGR test pattern on every
  keystroke.** Stage 1 (boot sector) loads the cassowary into HGR
  page 1 and reads track 3 (which holds 16 identical copies of
  stage 2) into `$1000-$1FFF`, then JMPs to the canonical stage 2
  copy at `$1000`. Stage 2 loads tracks 4+5 (the test bands) into
  HGR page 2, flips into HGR1, and runs a tiny self-modifying-code
  polling loop that flips PAGE2 between cassowary and bands on
  each keystroke. Stage 2 calls back into stage 1's still-resident
  RWTS subroutines (`load_a`, `load_b`, `read_track`,
  `read_sector`, `chk`, `wait_d5_aa`) via hard-coded entry-point
  addresses. Both source files (`Apple2/Demos/casso-rocks.a65` +
  `casso-rocks-stage2.a65`) plus the regenerated `casso-rocks.dsk`
  and the new `test-bands.hgr` framebuffer are committed.
- **`scripts/HgrPreprocess.py --pattern bands`** generates an 8 KB
  HGR framebuffer with 6 horizontal stripes
  (black/violet/green/white/blue/orange) covering all NTSC artifact
  colors the //e renderer can produce. Useful for diagnosing
  palette / byte-layout issues end-to-end through the disk + RWTS
  + renderer pipeline.

### Fixed (RWTS)
- **`read_track` in the casso-rocks demo now reads 18 sectors per
  track instead of 16** so that a phantom address mark caused by
  LSS resync immediately after a head step doesn't leave one real
  sector unread. Duplicate reads of the same logical sector just
  overwrite the destination page with identical bytes; the cost is
  ~25 KCycles per track. Symptom before the fix: stage 2's first
  read after the head step from track 3 to track 4 reliably
  dropped logical sector 3, leaving 256 bytes of HGR page 2 as
  random startup data. Only matters for boot-loader-style RWTS
  that doesn't have the standard DOS 3.3 head-settle delay; real
  DOS / ProDOS code is unaffected.

### Fixed (shell)
- **`Reset` and `Power Cycle` menu commands now re-read mounted slot-6
  disk images from the host filesystem.** Previously a Reset left the
  in-memory disk byte buffer untouched and a Power Cycle re-mounted
  using the cached path but didn't pick up external rewrites in a way
  the user could rely on (the dev workflow "regenerate `.dsk` outside
  Casso, hit Reset/Power Cycle to see the new image" silently kept
  showing the old image). Both menu commands now go through a new
  `RemountSlot6Disks` helper that snapshots the per-drive source
  paths and re-runs `MountDiskInSlot6` against each one, so the
  controller picks up whatever the file currently contains. Reset
  still preserves user RAM (real Apple ][ Ctrl-Reset semantics);
  Power Cycle still re-seeds DRAM. Auto-flush of dirty in-memory
  bytes still runs first so live writes aren't lost.

### Changed (demo)
- **Lighter title font in `scripts/HgrPreprocess.py`.** The previous
  Segoe UI Semibold 18px was too chunky on the cassowary HGR;
  defaulted to Segoe UI Regular 18px with no extra stroke. Two new
  CLI flags expose tuning without editing the script:
  `--title-size N` (default 18) and `--title-stroke N` (default 0;
  bump to 1 for a heavier look).

## [1.3.582] — 2026-05-13 — Reset/PowerCycle reload disks; lighter title font

### Fixed (shell)
- **`Reset` and `Power Cycle` menu commands now re-read mounted slot-6
  disk images from the host filesystem.** Previously a Reset left the
  in-memory disk byte buffer untouched and a Power Cycle re-mounted
  using the cached path but didn't pick up external rewrites in a way
  the user could rely on (the dev workflow "regenerate `.dsk` outside
  Casso, hit Reset/Power Cycle to see the new image" silently kept
  showing the old image). Both menu commands now go through a new
  `RemountSlot6Disks` helper that snapshots the per-drive source
  paths and re-runs `MountDiskInSlot6` against each one, so the
  controller picks up whatever the file currently contains. Reset
  still preserves user RAM (real Apple ][ Ctrl-Reset semantics);
  Power Cycle still re-seeds DRAM. Auto-flush of dirty in-memory
  bytes still runs first so live writes aren't lost.

### Changed (demo)
- **Lighter title font in `scripts/HgrPreprocess.py`.** The previous
  Segoe UI Semibold 18px was too chunky on the cassowary HGR;
  defaulted to Segoe UI Regular 18px with no extra stroke. Two new
  CLI flags expose tuning without editing the script:
  `--title-size N` (default 18) and `--title-stroke N` (default 0;
  bump to 1 for a heavier look).

## [1.3.581] — 2026-05-13 — HGR cassowary: better crop & color fidelity

### Changed
- **Tightened the cassowary crop** to capture the casque + head + neck
  through wattles, framed similar to the standard photo crop instead of
  the wider center-strip the first encoder produced. Source crop box
  is now baked into `scripts/HgrPreprocess.py` as `DEFAULT_CROP` and
  can still be overridden with `--crop`.
- **Per-byte HGR palette selection** in `scripts/HgrPreprocess.py`
  replaces the previous monochrome bit-pack. The encoder now
  classifies each source pixel into the 6-color HGR palette
  ({black, white, violet, green, blue, orange}), votes on a per-byte
  palette pair (bit 7 = 0 → violet+green, bit 7 = 1 → blue+orange,
  with blue/orange weighted 2× so a single color pixel doesn't lose
  to neighbouring leaf-greens), then places ON bits at the absolute
  pixel positions whose NTSC artifact phase matches the target
  color. Result: the leafy background renders solid green, the
  casque/wattles render orange/violet, and the head/neck reads as
  blue instead of every byte collapsing to a wash of white +
  green/violet stripes.
- **Letterbox-fit** added to the preprocessor for portrait subjects:
  fit the cropped image entirely inside 280×192 with black side
  bars instead of further center-cropping to landscape, so the
  cassowary keeps its full vertical proportion. Behavior can be
  reverted to the old fill-the-screen mode with `--no-letterbox`.

### Notes
- Apple ][ HGR's per-7-pixel-byte palette restriction is fundamental:
  within any 7-pixel column you get either {black, white, violet,
  green} OR {black, white, blue, orange}, never both. Thin features
  that straddle a palette boundary (a blue feather next to a green
  leaf) will compromise — one or the other gets the right color, or
  both wash to white via NTSC artifacting. The encoder is a
  best-effort first pass; future work could add Floyd-Steinberg
  error diffusion across byte boundaries.

## [1.3.579] — 2026-05-13 — HGR cassowary demo

### Added
- **HGR cassowary demo on the bootable demo disk.** The
  `Apple2/Demos/casso-rocks.a65` boot sector now contains a from-
  scratch 6502 RWTS that, after the disk2.rom boot PROM hands off
  control, steps the head from track 0 to tracks 1 and 2, reads all
  16 sectors of each via direct LSS scanning of D5/AA/96 address
  marks and D5/AA/AD data prologues, performs the standard 6-and-2
  expansion in-place, and lands the 8 KB cassowary framebuffer at
  `$2000-$3FFF`. It then flips `TXTOFF` and `HIRESON` to reveal the
  image. The whole loader (including the 32-sector RWTS) fits in
  the 255-byte sector-0 boot stage. The framebuffer is generated
  deterministically by `scripts/HgrPreprocess.py` (Pillow,
  Floyd-Steinberg dither) from `Assets/3a Mrs Cassowary closeup
  8167.jpg` and committed alongside the source as
  `Apple2/Demos/cassowary.hgr`. Test renamed to
  `BootDiskTests::CassoRocks_DemoDisk_DisplaysHgrCassowary` and
  verifies both the soft-switch state (graphics on, mixed off,
  page2 off, hires on) and that `$2000-$3FFF` matches the on-disk
  framebuffer byte-for-byte.

## [1.3.577] — 2026-05-13 — //e 80-col cursor: investigation closed

### Documented (video)
- **80-col cursor at the BASIC prompt is intentionally a steady
  solid block, not a blink.** Stack-trace investigation confirmed
  Casso is now ROM-faithful: the //e enhanced firmware's wrapper
  at `$C905-$C90E` toggles the cursor ON, blocks in `$CB15` (the
  no-timeout keyboard poll), then toggles OFF on keypress. There
  is no outer software-blink loop. Per UTAIIe ch. 8 the //e video
  circuitry physically disables the FLASH attribute whenever
  `ALTCHARSET=1` (forced ON in 80-col mode), so the hardware blink
  used in 40-col mode is gone and the firmware does not replace
  it. 40-col mode keeps its checkerboard (`$7F`) flashing cursor
  via the normal video FLASH path. No code change required.

### Removed (test)
- Removed the temporary `Pr3_DiagnoseCursorBlink` diagnostic test;
  it served its purpose during investigation and is no longer
  needed now that the 80-col cursor behavior is understood.

## [1.3.576] — 2026-05-13 — //e missing 80-col cursor fix

### Fixed (video)
- **The 80-col cursor (and any inverse-character cell) is now
  visible on the //e.** The //e enhanced video ROM stores the
  inverse-range slots ($00-$3F, in BOTH primary and alt sets)
  already in their visual / pre-inverted bitmap form (UTAIIe
  Tables 8.2/8.3 — slot $20 holds the bitmap of "inverse SPACE"
  = solid block, not the bitmap of normal " "). The text-mode
  renderers were applying their own XOR-inversion on top of that
  (the ][/][+ Decode2K convention), which re-inverted the
  pre-inverted bytes back to empty cells. Symptom: the BASIC
  cursor at main $0480 (= $20, inverse-space) rendered as
  invisible; any inverse-text screen output was blank. Fix:
  detect the //e ROM via `CharacterRomData::HasAltCharSet()` and
  skip the renderer's XOR-inversion for inverse slots when it's
  loaded; flash slots ($40-$7F primary) keep their XOR-toggle
  because those bytes alias the inverse range and the toggle is
  what alternates between stored-inverse and XORed-normal phase.
  Affects both `AppleTextMode` (40-col) and `Apple80ColTextMode`
  (80-col). New `VideoRenderTests::IIeRom_AppleTextMode_*` and
  `IIeRom_Apple80ColTextMode_InverseSpace_RendersSolidBlock` pin
  the contract using the real `Apple2e_Video.rom`.

## [1.3.575] — 2026-05-13 — //e PR#3 cursor investigation

### Added (test)
- New `Pr3AuxClearTest::Pr3_StaticCursor_Lands_At_Main0480` pins the
  byte-level state of the //e PR#3 cursor cell as Casso currently
  produces it: prompt `]` ($DD) at aux $0480, inverse-space ($20)
  at main $0480, OURCH=1, CV=1.

### Investigation (incomplete)
- The `Pr3AuxClearTest::Pr3_Clears_AuxTextPage1_AllRows` FIXME
  comment previously hypothesized that the //e cursor-blink loop
  was "missing a VBL interrupt or 1MHz mouse-card clock". The
  comment now reflects a wider-scope investigation: an end-to-end
  scan of the main //e ROM ($C100-$FFFC) found no `LDA $C019`, no
  `EOR #$40`, no `ORA #$40`. **However**, on real //e hardware in
  80-column mode the cursor IS supposed to blink — that loop lives
  in the 80-column firmware at $C300-$C3FF, which is bank-switched
  in via the `INTC8ROM` / `SLOTC3ROM` soft switches when `PR#3` is
  active and is NOT in the main-ROM scan range. The PC trace
  observed spinning in main ROM's BASIC keyboard poll
  ($CB15-$CB1E) instead of the slot-3 firmware suggests Casso's
  bank-switch for the 80-column firmware isn't taking effect after
  `PR#3` — that is the actual bug to chase next.

## [1.3.574] — 2026-05-13 — PowerCycle drive-state fix

### Fixed (disk)
- **Ctrl+Shift+R no longer leaves the drives empty.** The
  `IDM_MACHINE_POWERCYCLE` handler now snapshots the source paths of
  every disk currently mounted in `DiskImageStore` slot 6 before
  calling `EmulatorShell::PowerCycle`, then re-mounts them via
  `MountDiskInSlot6` once the controller has cycled. Previously the
  controller's `PowerCycle` re-pointed each engine at its empty
  internal sentinel disk, so a manual cold boot would leave Drive 1
  / Drive 2 ejected and the boot ROM had no nibbles to read. Test:
  unchanged controller-level `PowerCycleUnmountsAndFlushesAllDisks`
  contract continues to pass; the shell-level remount is wired in
  `EmulatorShell::ProcessCommands`.
- **Drive tooltip nibble counters reset on PowerCycle.**
  `DiskIINibbleEngine::Reset` now zeroes `m_readNibbles` and
  `m_writeNibbles` alongside the rest of the engine state, so the
  status-bar tooltip's `R:`/`W:` columns restart at 0 after Ctrl+Shift+R
  instead of carrying the pre-cycle counts. New
  `DiskIINibbleEngineTests::ResetClearsLifetimeNibbleCounters`
  pins the contract.

## [1.3.573] — 2026-05-13 — Friendly first-run bootstrap

### Added
- **Friendly first-run boot disk.** When a machine config has a Disk
  ][ controller (e.g. //e) and no disk is mounted in drive 1 from
  the CLI, the registry, or the user's session, Casso now prompts
  the user to download a stock Apple system master disk from the
  Asimov archive (https://www.apple.asimov.net) — DOS 3.3 System
  Master (680-0210-A, 1982) or ProDOS Users Disk (680-0224-C). Both
  are size- and host-pinned. ][/][+ configs (which have no Disk ][
  slot) are unaffected. A stale registry entry pointing at a
  deleted disk is treated as "no disk" — the entry gets cleared
  and the user is prompted again, instead of silently swallowing
  the boot.
- **Disk-image registry paths are now stored relative to `Casso.exe`.**
  When the disk lives under or beside the exe (the common case for
  the default `Disks/` peer dir), the per-machine `Disk1` /
  `Disk2` registry values hold a path like `Disks\Foo.dsk` instead
  of `D:\source\Casso\Disks\Foo.dsk`. The `Casso.exe` + `Disks/`
  tree is now portable across moves. Disks the user explicitly
  mounted from elsewhere (`E:\Games\Foo.dsk`) continue to be
  remembered as absolute paths. Existing absolute entries are
  read back as-is and rewritten relative on the next save.
- **Per-machine registry helpers extracted to `Casso/DiskSettings.h/.cpp`**
  so `Main.cpp` can read the saved disk before deciding whether to
  offer a boot-disk download. Disk-path conversion lives there; the
  registry layout (HKCU\Software\relmer\Casso\Machines\<machine>\
  {Disk1,Disk2}) is unchanged.
- **Apple-styled display strings throughout.** All user-visible
  Apple references now use the styling Apple's marketing did
  (`Apple ][`, `Apple ][+`, `Apple //e`, `Disk ][`). Comments still
  use plain ASCII (`Apple II`) since they're developer-facing.
- **Friendly first-run ROM bootstrap.** When a needed Apple ROM
  image is missing, Casso now lists the missing files and offers to
  download them from the AppleWin project in a single Yes/No dialog
  (HTTPS via WinHTTP), instead of failing with a terse error and
  exiting. The download placement honors the existing repo layout
  when present, otherwise drops the files into `ROMs/` next to
  `Casso.exe`. The set of ROMs Casso fetches is decided strictly
  from the JSON config embedded in `Casso.exe` for the chosen
  machine — if you've edited your on-disk `Machines/<machine>.json`
  to add extra slot ROMs, sourcing those is on you. New
  `AssetBootstrapTests` verify each shipped machine's required ROM
  list and disk-controller status end-to-end (loads `Casso.exe` as
  a resource module, parses its embedded JSON, asserts exact ROM
  filenames + slot 6 disk-ii presence).
- **Embedded default machine configs.** `Apple2.json`,
  `Apple2Plus.json`, and `Apple2e.json` are now bundled as resources
  in `Casso.exe` and extracted on first run when no `Machines/`
  directory is found, so a loose `Casso.exe` is enough to get a
  picker on screen.
- **In-house bootable demo disk** under `Apple2/Demos/`. The
  `casso-rocks.a65` source assembles to a 45-byte sector-0 program
  that displays "CASSO ROCKS!" centered on the text screen. New
  `BootDiskTests::CassoRocks_DemoDisk_PrintsBanner` runtime-assembles
  the demo through Casso's own assembler, builds a synthetic `.dsk`,
  boots through the real `Disk2.rom`, and verifies the banner — and
  also emits `casso-rocks.dsk` next to the source for direct GUI use.
  Replaces the project's previous reliance on the copyrighted DOS 3.3
  master image for end-to-end boot validation.

### Changed (CI)
- **ROM and machine-config filenames now use `<MachineType>[_Suffix]`
  casing**: `Apple2.rom`, `Apple2Plus.rom`, `Apple2e.rom`,
  `Apple2_Video.rom`, `Apple2e_Video.rom`, `Disk2.rom` (and
  `Apple2eEnhanced.rom` / `Disk2_13Sector.rom` in the AppleWin
  catalog); machine configs renamed to `Machines/Apple2.json`,
  `Apple2Plus.json`, `Apple2e.json`. Affects machine configs,
  `scripts/FetchRoms.ps1`, the embedded resources + in-app
  downloader catalog, fixtures, tests, and docs. Existing local
  `ROMs/` directories should be renamed to match (case-only on
  Windows requires a two-step `git mv`). The `--machine` CLI flag,
  registry-stored last machine, and `fs::exists` lookups remain
  case-insensitive on NTFS, so old values still resolve.
- `Disks/` (local disk image cache, may contain copyrighted images)
  is now `.gitignore`d.
- `CatalogReproductionTest` and `Pr3AuxClearTest` now resolve repo
  files via an upward walk (matching `BackwardsCompatTests`) instead
  of hardcoded `C:\Users\…` paths, and skip cleanly when their input
  disk image / ROM is absent (CI runners don't have them).

## [1.3.536] — 2026-05-10 — Disk II + //e text fidelity

### Fixed (disk)
- **DOS 3.3 boots from `.dsk` images.** Disk II nibblization corrected:
  10-bit sync nibbles (`PackSyncNibbleBits`); standard data-field XOR
  convention (each on-disk nibble = `encoded[i] XOR encoded[i-1]`,
  checksum nibble = final raw encoded byte); standard Disk II LSS read
  latch model with continuous-shift + 7 µs data-ready hold.
- **CATALOG works on real DOS 3.3 master disk.** Two latent bugs in
  the Disk II controller surfaced once boot succeeded:
  - Motor spindown delay added (~1 second per UTAIIe ch. 9 / AppleWin
    `SPINNING_CYCLES`). DOS RWTS toggles motor off / on between
    commands and depends on the disk physically continuing to spin
    during that window.
  - Phase-stepper now uses the adjacency-pull model (`±2` quarter-tracks
    per single-magnet pull, `±1` for the four "two-adjacent-magnets-on"
    states `$3/$6/$C/$9`) instead of the old "highest set bit"
    approximation, which accumulated drift across multi-track seeks
    and landed CATALOG's 17-track seek on the wrong sectors.
- **`DiskIIController::Tick`** now actually runs on the GUI CPU thread
  (previously was wired only in tests).
- **Per-machine disk auto-mount** persists in
  `HKCU\Software\relmer\Casso\Machines\{machine}\Disk1|Disk2` and is
  restored on machine switch / power cycle.
- **PowerCycle before MountCommandLineDisks** at startup: PowerCycle
  ejects every drive, so the previous order silently discarded the
  freshly-mounted image.

### Fixed (//e video)
- **PR#3 (80-column mode) renders blank cells, not garbage.** The
  `Decode4K` path for the //e enhanced video ROM was loading the
  alternate character set from the wrong half of the 4 KB file. Now
  matches UTAIIe ch. 8 (Sather) Tables 8.2 / 8.3: alt set's 256 chars
  all live in the first 2 KB of the file. Bug was latent until Phase
  12 added ALTCHARSET support to the 80-col renderer (audit M13).

### Fixed (//e UI / keyboard)
- **Edit > Copy Text** now reads the text page through `MemoryBus`
  rather than `m_cpu->GetMemory()`. The MMU owns its own RAM device(s)
  on the //e, so writes from firmware land in the bus-side buffer; the
  CPU's internal `memory[]` mirror was a stale copy unrelated to what
  appears on screen.
- **Alt key** routes through the emulated keyboard so Open / Closed
  Apple modifiers work; the Win32 layer no longer eats the modifier.
- **Soft reset** preserves modifier-key latches.

### Added (UI)
- **Drive 1 / Drive 2 activity LEDs and tooltips** in the status bar.
  Tooltips show mount path, current track, and read / write nibble
  counters.

### Added (tests)
- Real-ROM boot-decoder tests (`BootRomDecoderTests`) — drive the
  actual `Disk2.rom` slot 6 firmware on the emulated 6502 against
  synthetic disks; gates the on-disk format against the real Apple
  firmware's checksum routines.
- Direct-bus readback tests (`DiskReadbackTests`) — all 35 tracks ×
  16 sectors round-trip bit-perfect through the nibblizer + LSS
  without a CPU.
- End-to-end CATALOG repro test (`CatalogReproductionTest`) — boots
  real `dos33-master.dsk`, types `CATALOG`, asserts directory listing
  is printed (no I/O ERROR).
- 80-col PR#3 alt-set decoder gates (`Pr3AuxClearTest`).

### Known follow-ups
- 80-col cursor invisible after PR#3 (cursor-blink loop never runs;
  likely VBL-interrupt or similar timer wiring).
- Tooltip stats stale after PowerCycle.
- Disk subsystem broken after PowerCycle (counters don't advance).
- Bursty cosmetic update of nibble counters in the status bar.



## [1.3.509] — 2026-05-09 — Apple //e fidelity (spec 004, Phases 0-16)

The bulk of this entry completes Apple //e fidelity work begun in
`[1.3.416]`. After this release the //e cold-boots to BASIC, runs Disk II
images (`.dsk`/`.do`/`.po`/`.woz`), renders 80-column text and Double
Hi-Res, honours auxiliary RAM and the Language Card state machine,
distinguishes soft reset from power cycle, and exposes a cycle-accurate
IRQ/NMI infrastructure.

### Added (CPU + interrupts — Phase 1)
- **`Cpu6502`** adapter implementing the new `ICpu` and `I6502DebugInfo`
  contracts. Lets the emulator core be re-targeted without reaching into
  legacy `EmuCpu` internals.
- **`InterruptController`** with up to 32 named sources, edge/level
  semantics, and per-source assert/clear. Wired into the CPU dispatch so
  `IRQ` and `NMI` vectors fire on the next instruction boundary.
- **IRQ / NMI dispatch path** validated against the 6502 hardware spec
  (7-cycle entry, status-register I-bit set on entry, B-bit clear on
  vectoring, vector fetch from `$FFFE/$FFFF` and `$FFFA/$FFFB`).

### Added (memory + Language Card — Phase 2 / 3)
- **`AppleIIeMmu`** owns the //e bank-switching state (`RDRAMRD`,
  `RDRAMWRT`, `RDCXROM`, `RDC3ROM`, `RDALTZP`, `RD80STORE`, `RDPAGE2`,
  `RDHIRES`) and replaces the legacy `AuxRamCard`. `Apple2.json` and
  `Apple2Plus.json` continue using their legacy banks; `Apple2e.json` is
  the only config that wires the MMU.
- **64 KB auxiliary RAM** mapped through the MMU. Aux Zero Page / Stack
  toggled via `ALTZP`. 80STORE forces the page-2 / Hi-Res text + graphics
  windows onto the aux bank when set.
- **Audit-correct Language Card state machine** with read-source decoded
  from bits 0 **and** 1 (the old bit-0-only path missed `$C083`),
  bank-1 / bank-2 selection per `BSRBANK2`, and write-enable latched on
  the second consecutive read of an `$C08x` write-enable address.
- **`INTCXROM` physical remap** — `$C100-$CFFF` switches between the
  internal //e ROM and slot peripheral ROMs.

### Added (reset — Phase 4)
- **`SoftReset` vs. `PowerCycle`** semantics on every device and on the
  CPU. Soft reset preserves RAM contents, leaves the Language Card in
  its current bank state, keeps soft switches that survive Ctrl-Reset on
  real hardware, and re-vectors via `$FFFC`. Power cycle re-randomises
  RAM, returns Language Card / MMU / video-mode bits to their cold-boot
  defaults, and clears the keyboard latch + IRQ controller.
- **`EmulatorShell` reset dispatch** routed through a single `Reset(IDM)`
  contract (`IDM_RESET_SOFT` / `IDM_RESET_POWER`) so the menu, debug
  console, and remote (headless) command paths all funnel through one
  authoritative path.

### Added (video timing + RDVBLBAR — Phase 5)
- **`VideoTiming`** model: 65 cycles per scanline × 262 scanlines =
  17,030 cycles per frame; tracks current scanline, cycle-in-frame, and
  vertical-blank window. Exposed to soft-switch reads so `RDVBLBAR`
  ($C019) reflects real hardware polarity (bit 7 = 1 outside vblank).
- **FR-033** (vblank polarity) covered with dedicated tests.

### Added (keyboard + soft-switch read surface — Phase 6, baseline 1.3.416)
- **Open Apple / Closed Apple / Shift modifiers** at `$C061-$C063`,
  wired to host **Left Alt / Right Alt / Shift**.
- Strobe-clear isolation (`$C010` only) and a consolidated
  `$C011-$C01F` status-read surface where bit 7 is sourced from the
  canonical owner.

### Added (cold boot to BASIC — Phase 7, US1 MVP)
- **//e cold boot reaches the AppleSoft prompt.** `EmulatorShell` now
  pumps reset → boot wait → `]` prompt detection. Verified via the
  HOME / `PRINT "HELLO"` / `PR#3` 80-column / Open-Apple modifier
  scenarios in `Phase7ColdBootTests` and `EmulatorShellColdBootTests`.
- **Scraper / injector helpers** for headless tests: video-text scraper
  reads the canonical 40/80-column buffer; keyboard injector queues
  ASCII strings at the bus level without host-window dependencies.

### Added (US3 //e memory + Language Card scenarios — Phase 8)
- 24 acceptance scenarios in `EmuValidationSuiteTests` covering aux RAM
  hot-swap, ALTZP, 80STORE + PAGE2 + HIRES interactions, Language Card
  bank-1 / bank-2 / write-enable transitions, and `INTCXROM` slot ROM
  remapping. Validates SC-006 / SC-007.

### Added (Disk II nibble engine + WOZ — Phase 9 / 10)
- **`DiskIINibbleEngine` rewrite** — cycle-accurate bit-stream model
  (4 µs / bit at 1.023 MHz), Q3 sample timing, Q6/Q7 latch, write-protect
  flag, and per-track read/write head. Replaces the previous
  byte-oriented stub.
- **`NibblizationLayer`** for `.dsk` / `.do` / `.po` images. 16-sector
  6&2 group code with valid prologue / epilogue triplets, address-field
  + data-field checksums, DOS 3.3 vs. ProDOS sector skews, and a
  reverse `Denibblize` path for write-back.
- **`WozLoader`** for WOZ v1 and v2 images including TMAP / TRKS chunks,
  variable-length tracks (`bitCount`, not byte count), large-track
  support up to ~51,200 bits, and signature validation against the
  WOZ-spec `kSigV1` / `kSigV2` headers.

### Added (DiskImageStore + headless wiring — Phase 11)
- **`DiskImageStore`** — uniform handle layer. Open / GetTrackBitCount /
  ReadBit / WriteBit / IsDirty / Save. Supports both nibblized and WOZ
  images behind one interface.
- **Auto-flush on eject** and on shell shutdown, with dirty-tracking so
  unmodified images are not rewritten.
- **`HeadlessHost`** test harness — drives the emulator without a host
  window; lets test fixtures schedule cycles, read framebuffers, inject
  keystrokes, and mount / eject disks deterministically.

### Added (text + DHR video — Phase 12)
- **`Apple80ColTextMode`** with `ALTCHARSET`, `FLASH` half-second blink
  cadence, and composed mixed-mode (top 160 lines graphics, bottom 32
  lines text) from a single shared character ROM source.
- **`AppleDoubleHiResMode`** — 560×192 monochrome / 140×192 16-color
  Double Hi-Res with proper aux/main interleave (aux byte first, then
  main, packing 7 pixels per byte pair). DHR mode-select gated on
  `RDHIRES & RD80VID & RDDHIRES`.
- **Golden-hash framebuffer tests** that re-execute canonical software
  patterns and compare exact frame hashes; covers BASIC `]` prompt,
  GR / HGR / HGR2 mode patterns, and 80-column DOS catalogues.

### Added (disk boot end-to-end — Phase 13)
- 8 disk-boot integration scenarios: synthetic DOS 3.3 boot, mixed-mode
  scroll, 80-column ProDOS catalogue, write-protect honoured, save +
  reload round-trip, WOZ copy-protected sample boot, multi-sided image
  fallthrough, and motor-off head-park.

### Added (backwards-compat — Phase 14)
- `BackwardsCompatTests` regression-protect the unchanged Apple ][ and
  ][+ behavior: keyboard latch, soft-switch surface, video modes, no
  MMU activity, no aux RAM, no IRQ controller. Audit log
  (`audit-backwards-compat.md`) documents the verification.

### Added (perf budget — Phase 15)
- **Performance gate** — `PerformanceTests` measures emulator throughput
  on a workload of `kPerfMeasureCycles` and asserts elapsed wall-clock
  ≤ `kPerformanceCeilingMs`. Stability run (`kStabilityRunCount`)
  enforces ≤ `kStabilityToleranceFraction` variance. Released-only
  (skipped in Debug). Documented in `phase15-perf-protocol.md`.

### Added (constitution audits + final gate — Phase 16)
- 8 constitution audits under `specs/004-apple-iie-fidelity/audit-*.md`
  covering header comments, macro arguments, function spacing,
  EHM-on-fallible, scope blocks, function size, declaration alignment,
  and magic numbers. All audits report PASS.
- 23 declaration-alignment cleanups (whitespace only, T130) across 16
  files.
- Dormann functional test PASS, Harte single-step suite PASS.
- 0 warnings / 0 errors on all four configurations
  (Debug/Release × x64/ARM64) with `/W3 /WX /sdl /analyze`.

### Tests
- Test count: **1013 / 1013 passing** in Release (1012 / 1012 in Debug —
  the +1 is the `PerformanceTests` sentinel that skips in Debug).
  Confirmed clean across x64 Debug + Release and ARM64 Debug + Release.
  Code analysis 0/0 on all four configurations.
- New test surface (selected): `Cpu6502Tests`, `InterruptControllerTests`,
  `MmuTests`, `LanguageCardTests`, `ResetSemanticsTests`,
  `EmulatorShellResetTests`, `VideoTimingTests`, `Phase7ColdBootTests`,
  `EmuValidationSuiteTests` (US3, US5), `DiskIINibbleEngineTests`,
  `NibblizationTests`, `WozLoaderTests`, `DiskImageStoreTests`,
  `DiskIITests`, `Phase11IntegrationTests`, `Apple80ColTextModeTests`,
  `AppleDoubleHiResModeTests`, `Phase12GoldenHashTests`,
  `Phase13DiskBootTests`, `BackwardsCompatTests`, `PerformanceTests`,
  `HeadlessHostTests`, `PrngTests`.

### Notes
- Closes spec 004 (Apple //e fidelity), Phases 0 through 16. SC-001
  through SC-009 met. The headless test harness (`HeadlessHost`,
  `FixtureProvider`, scraper / injector helpers) is now the canonical
  path for emulator integration tests.

## [1.3.416] — 2026-05-06

### Added (Apple //e fidelity — Phase 6: keyboard + soft-switch read surface)
- **Open Apple / Closed Apple / Shift modifiers** are now reachable at the
  expected //e addresses:
  - `$C061` — Open Apple (bit 7 = pressed). Wired to host **Left Alt**.
  - `$C062` — Closed Apple (bit 7 = pressed). Wired to host **Right Alt**.
  - `$C063` — Shift key (bit 7 = pressed). Wired to host **Shift**.
  Previously the modifier-key fields existed on `AppleIIeKeyboard` but the
  device's bus range stopped at `$C01F`, making them dead code.
- **Strobe-clear isolation**. Reads of `$C011-$C01F` (BSRBANK2 / BSRREADRAM /
  RDRAMRD / RDRAMWRT / RDCXROM / RDALTZP / RDC3ROM / RD80STORE / RDVBLBAR /
  RDTEXT / RDMIXED / RDPAGE2 / RDHIRES / RDALTCHAR / RD80VID) no longer
  clear the keyboard strobe. Only `$C010` clears it, matching the //e
  hardware. (Audit §4 C-item closed.)
- **Consolidated `$C011-$C01F` status read surface** in
  `AppleIIeSoftSwitchBank::ReadStatusRegister()`. Bit 7 is sourced from the
  canonical owner of each flag (LanguageCard for BSRBANK2/BSRREADRAM, MMU for
  RDRAMRD/RDRAMWRT/RDCXROM/RDALTZP/RDC3ROM/RD80STORE, VideoTiming for
  RDVBLBAR, the bank for the display-mode flags), and bits 0-6 mirror the
  keyboard latch (floating-bus behavior).
- **`AppleIIeKeyboard` is now a `$C000-$C063` facade** that forwards
  non-owned addresses to its sibling devices (soft-switch bank for
  `$C00C-$C00F` / `$C011-$C01F` / `$C050-$C05F`; speaker for `$C030-$C03F`).
  This preserves the unchanged ][/][+ behavior where `AppleKeyboard` only
  owns `$C000-$C01F`.

### Tests
- `+10` keyboard tests in `KeyboardTests.cpp` covering modifier reachability,
  strobe-clear isolation, and audit-closure assertions.
- `+15` new tests in `SoftSwitchReadSurfaceTests.cpp` — one per `$C011-$C01F`
  address — that assert (a) bit 7 reflects the canonical source, (b) bits
  0-6 mirror the keyboard latch, (c) the read does not clear strobe, and (d)
  repeat reads do not perturb state.
- Test count: **906 / 906 passing** (was 881; +25). Confirmed clean across
  x64 Debug + Release and ARM64 Debug + Release; code analysis 0/0.

### Notes
- Closes the foundational Apple //e fidelity work (spec 004 Phases 0-6).
  Phase 7 (User Story 1 MVP cold boot) is the next planned increment.

## [1.2.315] — 2026-05-04

### Added
- **Character generator ROM loading** — text mode renderers now load the real
  Apple character ROM file (`Apple2_Video.rom` for II/II+, `Apple2e_Video.rom`
  for the //e) instead of the embedded 96-character fallback. Fixes:
  - **//e cursor** is now visible (the cursor character was outside our embedded
    range)
  - **//e boot logo** "Apple ][" displays fully (the missing characters were also
    outside our embedded range)
  - All 256 character codes render correctly across inverse, flash, and normal regions
- **CharacterRomData** loader (`CassoEmuCore/Video/CharacterRomData.h/.cpp`)
  decodes both 2KB (II/II+) and 4KB (//e enhanced) ROM formats including the
  bit-reversed 2KB layout and the //e's primary + alt char set arrangement.
  Falls back to embedded $20-$5F glyphs if no ROM file is configured.

## [1.1.311] — 2026-05-04

### Changed
- **Machine config schema v2** — breaking change. Refactored from a single `memory[]`
  array with conditional fields into clear sections:
  - `ram[]` — RAM regions with `address` + `size` (and optional `bank`)
  - `systemRom` — singular system ROM (`address` + `file`; size derived from file)
  - `characterRom` — character generator ROM (file only)
  - `internalDevices[]` — motherboard I/O (just `type`)
  - `slots[]` — expansion cards (`slot`, optional `device`, optional `rom`)
  All three machine configs (`Apple2.json`, `Apple2Plus.json`, `Apple2e.json`)
  migrated to the new schema.
- **ROM size validation** — system ROM file size now determines the end address
  automatically (no more start/end mismatch bugs)
- **Slot ROM auto-mapping** — slot ROMs auto-map to `$C000 + slot * 0x100`,
  required to be exactly 256 bytes

### Added
- **FetchRoms.ps1** — expanded to download all peripheral card ROMs from AppleWin:
  Disk II 13-sector, Mockingboard, Mouse Interface, Parallel printer, Super Serial
  Card, ThunderClock Plus, HDC SmartPort, Hard Disk drivers, Apple //e Enhanced
  system ROM
- **Apple //e Disk II slot ROM** — `Disk2.rom` now loads at $C600-$C6FF (slot 6)
  via the new schema, satisfying the //e autostart scan

## [1.0.307] — 2026-05-04

### Added
- **Machine picker dialog** — modal Win32 ListView showing all `Machines/*.json` configs
  with display names from the JSON `name` field; shown at startup if no last-used
  machine, when clicking the status bar Machine panel, or via File > Switch Machine
- **Last-used machine persistence** — stored in registry at `HKCU\Software\relmer\Casso`
- **Hot-swap machine switching** — pause CPU, tear down devices/bus/cpu/video,
  reload config, reinitialize, resume — works from menu, status bar, or startup
- **Random RAM on cold boot** — RAM ($0000-$BFFF) initialized with random values to
  match real DRAM power-on behavior (Apple II shows random characters at boot)
- **80STORE soft switch tracking** — IIe keyboard intercepts $C000/$C001 writes to
  track 80STORE state; video mode selection suppresses page2 when 80STORE is active
- **ROM size validation** — RomDevice rejects ROM files that don't match the configured
  address range size, with a clear error message
- **Illegal opcode handling** — CPU treats illegal opcodes as 1-byte NOPs (2 cycles)
  with a debug log message instead of crashing

### Fixed
- **//e boot** — corrected ROM start address from $C100 to $C000 (16KB ROM); slot ROM
  trimmed to $C100-$CFFF to avoid shadowing I/O space at $C000-$C0FF
- **Language Card state machine** — corrected read source decoding to use both bits 0
  and 1 (was using only bit 0); $C083 now correctly enables Read RAM + Write Enable
- **CpuOperations RMW operations** — Decrement, Increment, RotateLeft, RotateRight now
  use ReadByte/WriteByte instead of direct memory[] access, so they correctly route
  through the bus for I/O-mapped addresses
- **EmuCpu memory routing** — reads and writes for $C000+ now go through the
  MemoryBus, so the LanguageCardBank is consulted for $D000-$FFFF (was reading stale
  ROM from memory[] which caused //e BASIC to fail)
- **CreateMemoryDevices aux RAM handling** — RAM regions with a `bank` field (e.g.
  "aux") are skipped in main RAM creation; aux memory is handled by AuxRamCard
- **MemoryBus::Validate** — overlapping I/O devices are now warnings (logged via
  DEBUGMSG) instead of errors; first-match-wins is the correct hardware behavior

### Changed
- **Machine display names** — "Apple ][", "Apple ][ plus", "Apple //e"
- **File menu** — "Open Machine Config" renamed to "Switch Machine..." and ungrayed
- **Status bar** — clicking the Machine panel opens the picker dialog
- **Cpu::Reset** — removed all hardcoded test instructions and PC=$8000 setup;
  now just initializes registers/flags/memory
- **Cpu member initializers** — moved from constructor initializer list to in-class
  defaults
- **VS Code IntelliSense config** — added CassoEmuCore/Pch.h to forcedInclude so
  `<random>` and other STL headers resolve correctly

### Removed
- **Cpu::Run()** — dead code (never called); CLI uses its own StepOne loop

## [1.0.244] — 2026-05-03

### Added
- **Apple II platform emulator (Casso.exe)** — GUI-based Apple II, II+, and IIe emulator
  with D3D11 rendering, WASAPI audio, data-driven JSON machine configs, and keyboard input
- **CPU thread architecture** — dedicated CPU thread for 6502 execution and audio,
  UI thread for Win32 messages and D3D Present with vsync
- **Status bar** — shows CPU type, clock speed (MHz), machine name, and device count;
  clicking devices shows a popup listing all bus-mapped devices with address ranges
- **Edit menu** — Copy Text (reads 40×24 text screen as ASCII), Copy Screenshot
  (framebuffer as DIB bitmap), Paste (Ctrl+V feeds clipboard into keyboard)
- **Cycle-accurate instruction timing** — baseCycles in Microcode with runtime page-cross
  and branch-taken penalties
- **Pending audio buffer** — decouples PCM generation from WASAPI drain to prevent stutter
- **DPI-scaled debug console font** — uses GetDpiForWindow + MulDiv

### Changed
- **Project rename** — Casso65Core → CassoCore, Casso65EmuCore → CassoEmuCore,
  Casso65Emu → Casso, Casso65 → CassoCli; repo renamed to relmer/Casso
- **Exact NTSC timing** — CPU clock 1,022,727 Hz (was 1,023,000), cycles/frame 17,030
  (was 17,050); derived from 14.31818 MHz crystal
- **Speaker amplitude** — ±0.25f (was ±1.0f) to match reference audio levels
- **WASAPI buffer** — 100ms (was 33ms) for jitter headroom
- **D3D vsync** — Present(1) on UI thread, Present(0) was double-gating with frame timer
- **using namespace std** + **namespace fs** in both Emu Pch.h files
- **In-class member initialization** preferred over constructor initializer lists
- **Casso65Emu flattened** — removed Audio/, Shell/, Resources/, shaders/ subdirectories
- **machines/ → Machines/, roms/ → ROMs/** — directory casing standardized

### Fixed
- **Mixed-mode text flicker** — framebuffer race condition; CPU thread now copies completed
  framebuffer to UI buffer under mutex
- **Hi-res NTSC colors** — two-pass renderer correctly handles cross-byte-boundary adjacent
  pixels; HCOLOR=3 renders as solid white
- **Power cycle** — now clears RAM ($0000-$BFFF) for cold boot with APPLE ][ banner
- **Paste drops characters** — DrainPasteBuffer now checks keyboard strobe before feeding
  next character
- **Duplicate AddDevice** — every device was registered twice on the memory bus
- **Bus overlap detection** — Validate() uses CBRN with specific conflicting address ranges
- **Title bar garbage** — em-dash encoded as UTF-8 in source, replaced with \\u2014 escape
- **Debug console newlines** — bare LF converted to CRLF for Win32 EDIT control
- **Audio buzz during boot** — capped submission to one frame, pre-filled silence
- **Green screen** — CPU opcode fetch now uses virtual ReadByte through MemoryBus
- **Black screen** — D3D11 shaders implemented via runtime D3DCompile
- **ParseHexAddress** — overflow and invalid-char validation added

## [0.9.32] — 2026-04-28

### Added
- Tom Harte SingleStepTests — per-opcode validation against 151 legal-opcode test sets (10,000 vectors each)
- `run` subcommand — load and execute a binary or assembly source from the CLI
- **Full AS65-compatible assembler** — from-scratch reimplementation of Frank A. Kingswood's AS65
  - All 56 mnemonics, all 14 addressing modes
  - Two-pass assembly with forward-reference resolution
  - Full expression evaluator: `+ - * / % & | ^ ~ << >>`, `<`/`>` byte selectors, current-PC `*`
  - Constants: `equ`, `=`, `set` with forward-reference chains
  - Conditional assembly: `if`/`ifdef`/`ifndef`/`else`/`endif`
  - Macros: `name macro`…`endm`, positional `\1`–`\9` and named parameters, `local`, `exitm`, `\?` unique suffix
  - Directives: `.org`, `.byte`, `.word`, `.text`, `.ds`, `.dd`, `.align`, `.end`, `.error`, `.include`
  - Struct definitions with `struct`/`end struct` and dot-qualified member access
  - Character map (`.cmap`) for custom character encodings
  - Three-segment model: `code`/`data`/`bss`
  - Binary file includes: `.bin`, `.s19`, `.hex` (incbin-style)
  - Backslash line continuation in macros
  - Colon-less labels (AS65 compatibility)
  - Listing output: `-l [file]`, `-c` cycle counts, `-m` macro expansion, `-t` symbol table
  - Output formats: flat binary (default), `-s` Motorola S-record, `-s2` Intel HEX
  - Warning control: `--warn`, `--no-warn`, `--fatal-warnings`
  - Flag concatenation: `-tlfile` ≡ `-t -l file` (AS65 style)
  - 10-test conformance suite verifying AS65 parity
- Dormann and Harte regression test suites with on-demand runner scripts

### Fixed
- BCD flag behavior (N/V/Z flags match real 6502 hardware)
- JMP indirect page-boundary wrap bug
- JSR operand read overlapping stack push
- DEY/PLA opcode table swap
- STY missing source register in absolute mode
- Addressing mode wrapping for zero-page indexed modes
- Assembler: `equ` forward-reference chain resolution, `ifdef`/`ifndef`, `-s2` Intel HEX output, listing column layout, `.org` gap fill
- LDX/INC/DEC addressing mode table entries
- STY missing Absolute addressing mode (#37)

## 2026-04-25

### Changed
- Project renamed from **My6502** to **Casso**

## 2026-04-24

### Added
- **Assembler v1** (spec 001) — basic two-pass assembler with labels, branches, directives, expressions, listing output, and CLI subcommands
- `LoadBinary()` — load pre-assembled binaries into CPU memory
- CI pipeline with GitHub Actions (x64 + ARM64, Debug + Release)

### Fixed
- `ShiftLeft`/`ShiftRight` dispatch (was calling `RotateLeft`/`RotateRight`)
- `BIT` instruction V/N flags (were read from AND result instead of operand)
- `Compare` carry flag for boundary values
- `PushWord`/`PopWord` read/write outside stack page

## 2026-04-23

### Added
- Extracted `CassoCore` static library from monolithic project
- `UnitTest` project with 66 initial tests (Microsoft Native CppUnitTest)
- Build/test automation scripts and VS Code tasks

## 2024-12-15

### Added
- Stack and memory helpers, rewritten addressing mode resolution
- `BRK` (software interrupt) implementation

### Fixed
- `LDX`, `DEX`, `BMI`, `BPL`, `INX` behavior corrections

## 2024-12-08

### Added
- Flag manipulation (`CLC`, `SEC`, `CLI`, `SEI`, `CLV`, `CLD`, `SED`), register transfers (`TAX`, `TXA`, `TAY`, `TYA`, `TXS`, `TSX`), `NOP`

## 2024-11-24 — 2024-11-30

### Added
- Initial 6502 emulator: fetch-decode-execute cycle, all 56 standard mnemonics
- **Group 01**: `ORA`, `AND`, `EOR`, `ADC`, `STA`, `LDA`, `CMP`, `SBC` — all 8 addressing modes
- **Group 00**: `BIT`, `JMP`, `STY`, `LDY`, `CPY`, `CPX`
- **Group 10**: `ASL`, `LSR`, `ROL`, `ROR`, `STX`, `LDX`, `DEC`, `INC`
- All 14 addressing modes (immediate, zero page, ZP,X, ZP,Y, absolute, abs,X, abs,Y, (ZP,X), (ZP),Y, indirect, relative, accumulator, implied, jump absolute)
