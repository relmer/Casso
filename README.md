# Casso

[![CI](https://github.com/relmer/Casso/actions/workflows/ci.yml/badge.svg?branch=master&event=push)](https://github.com/relmer/Casso/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/github/license/relmer/Casso?cacheSeconds=300)](LICENSE)
<!--
[![Downloads](https://img.shields.io/github/downloads/relmer/Casso/total)](https://github.com/relmer/Casso/releases)
-->

## What's New

A few major capability waves landed between v1.3.509 and v1.3.853. Headlines below; see [CHANGELOG.md](CHANGELOG.md) for the granular history.

### UI Overhaul (v1.3.853)

Casso's entire chrome moved from the legacy Win32 menu bar / Win32 dialogs to a borderless, themed, in-process RmlUi shell — rendered straight onto the same D3D11 framebuffer that draws the emulator video:

- **Three built-in themes** — Skeuomorphic, Dark Modern, Retro Terminal — hot-swappable from **Settings → Theme** with no restart and no machine reset.
- **Custom themes are first-class.** Drop a directory under `Themes/<YourTheme>/` containing a `theme.json` plus the entry documents (`title_bar.rml`, `nav_layer.rml`, `settings.rml`, `drive_widgets.rml`) and matching `.rcss` — Casso picks it up on the next launch and the theme appears in the Settings dropdown. See [docs/themes/AUTHORING.md](docs/themes/AUTHORING.md) for a step-by-step guide.
- **Consolidated Settings panel** replaces the old `OptionsDialog` and `MachinePickerDialog`. Machine selection, emulation speed, video color mode, floppy sound + mechanism, write-protect, theme picker, and the new CRT controls live in one non-modal in-window panel.
- **Drag-and-drop disk mounting** — drop a `.dsk` / `.do` / `.po` / `.nib` onto either drive widget to insert it; click-to-browse on the same widget opens a file picker.
- **CRT post-processing** — scanlines, phosphor bloom, color bleed. Each effect toggles independently; a single brightness slider gates the master mix.
- **Auto-remount** of the last-inserted disks on machine load, so the typical "boot Apple ][+" flow is one click.
- **User preferences** persist in one `UserPrefs.json` file: global UI state under `global`, and per-machine deltas under `machines` keyed by display name.

<!-- TODO: capture screenshot -- caption: "Skeuomorphic theme: warm wood/brass title bar, drive widgets with idle/spin LEDs, Settings panel open over Apple //e DOS 3.3 boot." -->
<!-- TODO: capture screenshot -- caption: "Dark Modern theme: flat dark chrome, blue accent LEDs, Settings panel showing the Theme dropdown." -->
<!-- TODO: capture screenshot -- caption: "Retro Terminal theme: green-on-black VT323 font, scanlines + bloom enabled, NTSC color mode." -->

### Disk II Debug Window (v1.3.730)

A modeless live event log of every Disk II controller event the active machine emits. Open with **View → Disk II Debug...** or **Ctrl+Shift+D**:

- Motor on/off, head step / bump (with prev → new quarter-track), address marks (track / sector / volume), data reads, drive selects, insert / eject events, and audio decisions (started / restarted / continued / silent).
- Filter by event type, drive, track range, sector range, audio sub-category, or any combination; pause / resume / clear; Ctrl+C copies the selected rows tab-separated.
- Right-click the column header to show / hide the Wall, Uptime, Cycle, Event, or Detail columns.
- Open the dialog *before* the operation you want to investigate -- events emitted before the dialog opens are not retained.

### Disk II audio (v1.3.696)

Realistic mechanical sounds during disk activity, mixed into the WASAPI pipeline alongside the //e speaker:

- Stereo motor hum, head-step clicks, track-0 / max-track bumps, and disk insert / eject sounds.
- Per-drive equal-power stereo panning: single-drive profiles play centered; in two-drive profiles Drive 1 leans left, Drive 2 leans right.
- Step-vs-seek discrimination: contiguous step bursts during DOS RWTS recalibration fuse into a continuous seek buzz instead of N overlapping clicks.
- *View → Options...* dialog with a Drive Audio toggle (default on) and a Disk II mechanism dropdown (Shugart SA400 by default, or Alps 2124A). Both persist per-machine via the registry.
- First-run consent dialog downloads the actual recordings from the [OpenEmulator](https://github.com/openemulator/libemulation) project; OGGs are decoded in memory via vendored `stb_vorbis` and written as WAV (no `.ogg` retained on disk). Asked once per machine, persisted thereafter.
- Generic `IDriveAudioSink` / `IDriveAudioSource` / `DriveAudioMixer` abstraction so future drive types (//c internal 5.25, DuoDisk, Apple 5.25 Drive, ProFile, ...) plug in without touching the mixer.

### Friendly first-run bootstrap (v1.3.573)

Drop `Casso.exe` anywhere, double-click, and the rest happens:

- **Missing ROMs?** Casso lists what's needed and offers to download them from the AppleWin project in one click.
- **First time on the Apple //e?** Casso offers to download a stock Apple system master disk (DOS 3.3 or ProDOS) from the Asimov archive so the //e boots straight to a BASIC prompt instead of spinning forever waiting for a disk.
- **Lost your Machines/ folder?** The three default machine configs (`Apple ][`, `Apple ][+`, `Apple //e`) are bundled inside `Casso.exe` and extracted on demand.
- **Moved your install?** Per-machine remembered disks are stored as paths relative to `Casso.exe`, so the whole `Casso.exe` + `Disks/` tree is portable.

### Apple //e fidelity (v1.3.509)

Casso became a real Apple //e — not just a 6502 host that draws text:

- Cold boot to BASIC with audit-correct Language Card state machine, 64 KB auxiliary RAM, and the //e MMU (`INTCXROM` / `SLOTC3ROM` / `INTC8ROM` / `STORE80` / `RAMRD` / `RAMWRT` and the `ALTZP`-driven page-0/1 swap).
- 80-column text mode + Double Hi-Res with NTSC artifact colors.
- Cycle-accurate IRQ / NMI dispatch, soft reset vs. power cycle distinction, `RDVBLBAR` ($C019) wired into the video timing model.
- Disk II controller with full DOS 3.3 / ProDOS / WOZ v1 + v2 support, `.dsk` / `.do` / `.po` nibblization, and auto-flush on eject so user writes survive.
- Headless test harness (`HeadlessHost`) for deterministic integration tests of cold boot, disk boot, framebuffer hashing, and reset semantics.

## About

Casso is a retro / classic-machine platform emulator and from-scratch AS65-compatible 6502 assembler, written in C++. Today the platform emulator targets the Apple II family (][, ][+, //e); the abstractions are generic enough to host other 6502-based machines later.

![Casso emulating an Apple //e booting DOS 3.3](Assets/Apple%202e%20DOS%203.3%20boot.png)

The same cassowary photo rendered in HGR (6 colors, NTSC artifacting) and DHGR (16 colors, Floyd-Steinberg dithered) by the bootable [casso-rocks demo disk](Apple2/Demos):

| DHGR (16 colors) | HGR (6 colors) |
| :---: | :---: |
| ![Casso emulating an Apple //e showing the DHGR cassowary demo](Assets/Apple%202e%20DHGR%20Cassowary.png) | ![Casso emulating an Apple //e showing the HGR cassowary demo](Assets/Apple%202e%20HGR%20Cassowary.png) |

The project includes:

- **Apple II platform emulator** — GUI-based Apple II, II+, and //e emulator with D3D11 rendering, WASAPI audio, Disk II controller with realistic mechanical sounds, data-driven machine configs, 80-column text + Double Hi-Res, auxiliary RAM, audit-correct Language Card state machine, and cycle-accurate IRQ/NMI infrastructure.
- **6502 CPU emulator** — passes [Klaus Dormann's functional test suite](https://github.com/Klaus2m5/6502_65C02_functional_tests) and all 151 legal-opcode sets from [Tom Harte's SingleStepTests](https://github.com/SingleStepTests/ProcessorTests) (10,000 vectors each).
- **AS65-compatible assembler** — a from-scratch reimplementation of Frank A. Kingswood's AS65, intended as a drop-in replacement. Supports the complete AS65 syntax: macros, conditional assembly (`if`/`ifdef`/`ifndef`/`else`/`endif`), the full expression evaluator (arithmetic, bitwise, logical, shift, `<`/`>` byte selectors, current-PC `*`), `equ`/`=` constants, `include`, three-segment model (`code`/`data`/`bss`), AS65-style listing output, and AS65 command-line flags (`-l`, `-t`, `-s`, `-s2`, `-z`, `-c`, `-w`, `-d`, `-g`, ...) including flag concatenation (`-tlfile`).
- **CLI tool** — runs as an AS65-style assembler by default, or with the `run` subcommand to load and execute a binary or assembly source.
- **First-run asset bootstrap** — Casso fetches the ROMs, sample disks, and Disk II audio samples it needs on first launch (with user consent), so a fresh `Casso.exe` boots to a usable //e BASIC prompt with no manual setup.
- **Headless test harness** — `HeadlessHost` drives the emulator with no Win32 window, enabling deterministic integration tests for cold boot, disk boot, video framebuffer hashing, and reset semantics.
- **1100+ unit tests** — comprehensive coverage of CPU instruction encoding, addressing modes, arithmetic, branching, assembler features, audio pipeline (speaker + drive), //e MMU + Language Card, video timing, Disk II nibble engine, WOZ + nibblized image formats, 80-col + DHR video, reset semantics, perf budget, and backwards-compat for ][/][+ machines.

## Project Structure

```
Casso.sln
├── CassoCore/     Static library — CPU emulator, assembler, parser, opcode table
├── CassoEmuCore/  Static library — Apple II devices, video modes, audio generator + drive-audio mixer
├── Casso/         Win32 application — Apple II platform emulator (D3D11, WASAPI, Disk II audio)
├── CassoCli/      Console application — AS65-compatible assembler CLI with `run` subcommand
└── UnitTest/      Test DLL — Microsoft Native CppUnitTest (1100+ tests)
```

## Requirements

- Windows 10/11
- PowerShell 7 (`pwsh`) for build/test scripts
- Visual Studio 2026 (v18.x)
  - Workload: **Desktop development with C++**
  - Components: MSVC build tools, Windows SDK, C++ unit test framework
  - Optional: MSVC ARM64 build tools (for ARM64 builds)
- Optional: VS Code (repo includes `.vscode/` tasks)

## Quick Start

### Build

```powershell
# Build Debug for current architecture (Ctrl+Shift+B in VS Code)
.\scripts\Build.ps1

# Build Release
.\scripts\Build.ps1 -Configuration Release

# Build all platforms
.\scripts\Build.ps1 -Target BuildAllRelease

# Rebuild with code analysis (warnings as errors)
.\scripts\Build.ps1 -Configuration Release -RunCodeAnalysis
```

### Test

```powershell
# Build and run tests
.\scripts\RunTests.ps1

# Or use VS Code: Run Tests (current arch)
```

### Assemble and Run

```powershell
# Assemble a source file to a flat binary (AS65 mode — no subcommand)
.\x64\Debug\CassoCli.exe input.a65 -o output.bin

# Assemble with a listing file and a symbol table
.\x64\Debug\CassoCli.exe input.a65 -o output.bin -l listing.txt -t

# Output Motorola S-record (.s19) or Intel HEX (.hex)
.\x64\Debug\CassoCli.exe input.a65 -s   -o output.s19
.\x64\Debug\CassoCli.exe input.a65 -s2  -o output.hex

# Pre-define a symbol on the command line, generate cycle counts in the listing
.\x64\Debug\CassoCli.exe input.a65 -d DEBUG=1 -cl listing.txt

# Assemble and run an assembly source directly
.\x64\Debug\CassoCli.exe run input.a65

# Load and run a pre-assembled binary at a specific address
.\x64\Debug\CassoCli.exe run output.bin --load $8000
```

### Apple II Emulator

The emulator requires Apple II ROM images, which are copyrighted by Apple and not
distributed with this project. A script is included to download them from the
[AppleWin](https://github.com/AppleWin/AppleWin) project:

```powershell
# Download ROM images into the per-machine Machines/<Name>/ folders
.\scripts\FetchRoms.ps1

# Run the emulator (defaults to Apple II+)
.\ARM64\Debug\Casso.exe

# Run with a specific machine config
.\ARM64\Debug\Casso.exe --machine Apple2e
```

ROM images live under `Machines/<MachineName>/` (e.g.,
`Machines/Apple2e/Apple2e.rom`) and shared device boot ROMs live
under `Devices/<Family>/` (e.g., `Devices/DiskII/Disk2.rom`). Both
`Machines/` and `Devices/` are fully runtime-managed: every file
inside is either extracted from binary-embedded resources or
downloaded on first launch (with user consent). Delete either
directory and the next launch rebuilds it from scratch.

Available machine configs are in `Machines/<MachineName>/<MachineName>.json`.

## Assembler Features

| Feature | Syntax / Flag |
|---------|---------------|
| All 56 mnemonics | `LDA`, `STA`, `ADC`, `BNE`, etc. |
| All addressing modes | `#$42`, `$30`, `$1234,X`, `($20),Y`, `A` |
| Labels | `loop: DEX` / `BNE loop` |
| Directives | `.org $8000`, `.byte $FF`, `.word $1234`, `.text "hello"`, `code`/`data`/`bss` |
| Constants | `value = $42`, `carry equ %00000001` (chains and forward refs supported) |
| Conditionals | `if`/`ifdef`/`ifndef`/`else`/`endif` |
| Macros | `name macro` … `endm`, with arguments and `\` line continuation |
| Includes | `include "file.a65"` |
| Comments | `; full line` / `LDA #$42 ; inline` |
| Number formats | `$FF` (hex), `%10101010` (binary), `255` (decimal) |
| Expressions | full operator set: `+ - * / % & \| ^ ~ << >>`, `<label`, `>label`, current-PC `*` |
| Listing output | `-l [file]` (stdout or file), `-c` for cycle counts, `-m` for macro expansion |
| Symbol table | `-t` |
| Output formats | flat binary (default), `-s` (S-record), `-s2` (Intel HEX) |
| Fill control | `-z` for `$00` fill (default `$FF`) |
| Pre-defined symbols | `-d NAME` or `-d NAME=VALUE` |
| Debug info | `-g [file]` |
| Warning control | `--warn`, `--no-warn`, `--fatal-warnings` |
| Verbose / quiet | `-v` / `-q` |
| Flag concatenation | `-tlfile` ≡ `-t -l file` (AS65 style) |

## CPU Emulation Status

All 56 standard 6502 mnemonics are implemented. Validated against [Klaus Dormann's functional test suite](https://github.com/Klaus2m5/6502_65C02_functional_tests) (full pass) and [Tom Harte's SingleStepTests](https://github.com/SingleStepTests/ProcessorTests) (all 151 legal-opcode test sets, 10,000 vectors each).

## Roadmap

### Done

- [x] Pass [Klaus Dormann's 6502 functional test suite](https://github.com/Klaus2m5/6502_65C02_functional_tests) ([#7](https://github.com/relmer/Casso/issues/7))
- [x] Per-opcode validation against [Tom Harte's SingleStepTests](https://github.com/SingleStepTests/ProcessorTests) ([#29](https://github.com/relmer/Casso/issues/29), [#38](https://github.com/relmer/Casso/issues/38))
- [x] Apple //e fidelity — cold boot to BASIC, audit-correct Language Card, 64 KB aux RAM, 80-column text + Double Hi-Res, soft reset vs. power cycle, IRQ/NMI dispatch, RDVBLBAR
- [x] Disk II controller — DOS 3.3 / ProDOS `.dsk` / `.do` / `.po` nibblization + WOZ v1 / v2 with auto-flush on eject
- [x] Disk II mechanical audio — stereo motor hum, head-step clicks, track-0 bump, disk insert / eject sounds, with a runtime View → Options... → Drive Audio toggle. Built on a generic `IDriveAudioSink` / `IDriveAudioSource` / `DriveAudioMixer` abstraction so future drive types (//c internal 5.25, DuoDisk, ProFile, …) plug in without touching the mixer
- [x] Headless test harness for deterministic integration tests (`HeadlessHost`, framebuffer scraper, keyboard injector)
- [x] Performance gate — emulator throughput budget enforced in CI (Release-only)

### High Priority

- [ ] Interactive debugger / monitor — step, breakpoints, register watch, memory dump ([#51](https://github.com/relmer/Casso/issues/51))
- [ ] Undocumented / illegal opcode support ([#52](https://github.com/relmer/Casso/issues/52))

### Medium Priority

- [ ] 65C02 extended instruction support, with assembler `--cpu` flag ([#9](https://github.com/relmer/Casso/issues/9))
- [ ] VS Code extension — syntax highlighting, assemble-on-save, inline diagnostics ([#54](https://github.com/relmer/Casso/issues/54))
- [ ] Example programs — ready-to-assemble demos and tutorials ([#55](https://github.com/relmer/Casso/issues/55))
- [x] Cycle-accurate execution and profiling ([#57](https://github.com/relmer/Casso/issues/57))
- [ ] NES 6502 / Ricoh 2A03 variant ([#47](https://github.com/relmer/Casso/issues/47))
- [ ] Rockwell / WDC 65C02 variants ([#49](https://github.com/relmer/Casso/issues/49), [#50](https://github.com/relmer/Casso/issues/50))

### Low Priority

- [ ] Relocatable object output — o65 format for cc65 toolchain integration ([#58](https://github.com/relmer/Casso/issues/58))

## Why "Casso"?

The 6502 emulator world already has too many projects named [Emu](https://en.wikipedia.org/wiki/Emu), so I decided to be a little different. I picked its larger, flightless, slightly-more-dangerous cousin: the [cassowary](https://en.wikipedia.org/wiki/Cassowary)—Casso to his friends. I want this to be a great emulator and a great assembler, but don't take things too seriously.

I thus present to you our regal namesake—revel in his splendor!

<p align="center">
  <img src="Assets/3a%20Mrs%20Cassowary%20closeup%208167.jpg" alt="Southern Cassowary" width="240" />
</p>

*Cassowary photo by [Mr. Smiley / BunyipCo](https://bunyipco.blogspot.com/2015/04/cassowary-update.html), licensed under [CC BY-NC-SA 3.0](https://creativecommons.org/licenses/by-nc-sa/3.0/).*

## Approved Third-Party Dependencies

Per the [Casso Constitution v1.5.0](.specify/memory/constitution.md), every vendored
third-party source must be MIT or public-domain and listed in the Approved Dependencies
allowlist. The current allowlist:

| Component                                  | Upstream                                                        | Version / SHA                                  | License  | Used by                            |
|--------------------------------------------|------------------------------------------------------------------|------------------------------------------------|----------|------------------------------------|
| **RmlUi**                                  | https://github.com/mikke89/RmlUi                                 | `6.2` / `2230d1a6e8e0848ed87a5761e2a5160b2a175ba4` | MIT      | UI shell — title bar, nav layer, Settings panel, drive widgets |
| **libretro `crt-pi`** (scanlines)          | https://github.com/libretro/glsl-shaders (`crt/shaders/crt-pi.glsl`) | collection `42fa8a98ab19bdaffb53280746a30819eb21f807` | MIT — Davide Berra        | `Casso/Shaders/CRT/scanlines.hlsl` |
| **libretro `bloom`** (Gaussian bloom pass) | https://github.com/libretro/glsl-shaders (`bloom/shaders/bloom.glsl`) | collection `42fa8a98ab19bdaffb53280746a30819eb21f807` | CC0 / PD — Hyllian, hunterk | `Casso/Shaders/CRT/bloom_*.hlsl`   |
| **libretro `ntsc-adaptive`** (color bleed) | https://github.com/libretro/glsl-shaders (`ntsc/shaders/ntsc-adaptive/ntsc-pass1.glsl`) | collection `42fa8a98ab19bdaffb53280746a30819eb21f807` | MIT — Themaister, hunterk  | `Casso/Shaders/CRT/color_bleed.hlsl` |
| **stb_vorbis**                             | https://github.com/nothings/stb                                  | vendored single-header                         | MIT / PD | OGG decode for Disk II audio samples |

`scripts/CheckShaderLicenses.ps1` runs pre-build to enforce that no GPL / copyleft strings
leak into `Casso/Shaders/` outside of designated `// ATTRIBUTION:` comment blocks.

## Acknowledgments

Casso's correctness is validated against two exceptional open-source test suites:

- **[Klaus Dormann's 6502 Functional Test Suite](https://github.com/Klaus2m5/6502_65C02_functional_tests)** — [@Klaus2m5](https://github.com/Klaus2m5)'s exhaustive functional test exercises every documented 6502 behavior: all instructions, addressing modes, flag interactions, BCD arithmetic, and edge cases. Casso passes the full suite.
- **[Tom Harte's SingleStepTests](https://github.com/SingleStepTests/ProcessorTests)** — [@TomHarte](https://github.com/TomHarte)'s per-opcode test vectors validate every legal 6502 opcode against cycle-accurate reference traces. Casso passes all 151 legal-opcode test sets (10,000 vectors each).

Thank you to both authors for making these invaluable resources freely available. They are the gold standard for 6502 emulator validation.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for commit conventions, build instructions, and code style guidelines.

## License

[MIT](LICENSE)
