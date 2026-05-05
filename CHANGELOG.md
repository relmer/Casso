# Changelog

All notable changes to Casso are documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/).
Versioned entries use `MAJOR.MINOR.BUILD` from [Version.h](CassoCore/Version.h).
Entries before versioning was introduced use dates only.

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
