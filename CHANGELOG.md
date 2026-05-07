# Changelog

All notable changes to Casso are documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/).
Versioned entries use `MAJOR.MINOR.BUILD` from [Version.h](CassoCore/Version.h).
Entries before versioning was introduced use dates only.

## [Unreleased] — Apple //e fidelity (spec 004, Phases 0-16)

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
  `RDHIRES`) and replaces the legacy `AuxRamCard`. `apple2.json` and
  `apple2plus.json` continue using their legacy banks; `apple2e.json` is
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
- **`AppleDoubleHiResMode`** — 560×192 monochrome / 140×192 16-colour
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
  ][+ behaviour: keyboard latch, soft-switch surface, video modes, no
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
  keyboard latch (floating-bus behaviour).
- **`AppleIIeKeyboard` is now a `$C000-$C063` facade** that forwards
  non-owned addresses to its sibling devices (soft-switch bank for
  `$C00C-$C00F` / `$C011-$C01F` / `$C050-$C05F`; speaker for `$C030-$C03F`).
  This preserves the unchanged ][/][+ behaviour where `AppleKeyboard` only
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
  Apple character ROM file (`apple2-video.rom` for II/II+, `apple2e-enhanced-video.rom`
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
  All three machine configs (`apple2.json`, `apple2plus.json`, `apple2e.json`)
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
- **Apple //e Disk II slot ROM** — `disk2.rom` now loads at $C600-$C6FF (slot 6)
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
