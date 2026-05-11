# Casso

[![CI](https://github.com/relmer/Casso/actions/workflows/ci.yml/badge.svg?branch=master&event=push)](https://github.com/relmer/Casso/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/github/license/relmer/Casso)](LICENSE)

## About

![Casso Apple II Emulator](Assets/Apple%20%5D%5B%20GR%20Color%20Bands.png)

Casso is a 6502 CPU emulator and assembler written in C++. It emulates the MOS Technology 6502 microprocessor — the chip behind the Apple II, Commodore 64, NES, and Atari 2600.

The project includes:

- **Apple II platform emulator** — GUI-based Apple II, II+, and //e emulator with D3D11 rendering, WASAPI audio, data-driven machine configs, keyboard input, Disk II controller (DOS 3.3 / ProDOS / WOZ images), 80-column text + Double Hi-Res, auxiliary RAM, audit-correct Language Card state machine, and cycle-accurate IRQ/NMI infrastructure
- **A real, full-featured AS65-compatible assembler** — Casso's assembler is a from-scratch reimplementation of Frank A. Kingswood's AS65, intended to be a drop-in replacement. It supports the complete AS65 syntax: macros, conditional assembly (`if`/`ifdef`/`ifndef`/`else`/`endif`), the full expression evaluator (arithmetic, bitwise, logical, shift, `<`/`>` byte selectors, current-PC `*`), `equ`/`=` constants, `include`, three-segment model (`code`/`data`/`bss`), AS65-style listing output, and AS65 command-line flags (`-l`, `-t`, `-s`, `-s2`, `-z`, `-c`, `-w`, `-d`, `-g`, …) including flag concatenation (`-tlfile`).
- **CLI tool** — runs as an AS65-style assembler by default, or with the `run` subcommand to load and execute a binary or assembly source
- **Headless test harness** — `HeadlessHost` drives the emulator with no Win32 window, enabling deterministic integration tests for cold boot, disk boot, video framebuffer hashing, and reset semantics
- **1023 unit tests** — comprehensive coverage of CPU instruction encoding, addressing modes, arithmetic, branching, assembler features, audio pipeline, //e MMU + Language Card, video timing, Disk II nibble engine, WOZ + nibblized image formats, 80-col + DHR video, reset semantics, perf budget, and backwards-compat for ][/][+ machines

## Project Structure

```
Casso.sln
├── CassoCore/     Static library — CPU emulator, assembler, parser, opcode table
├── CassoEmuCore/  Static library — Apple II devices, video modes, audio generator
├── Casso/         Win32 application — Apple II platform emulator (D3D11, WASAPI)
├── CassoCli/      Console application — AS65-compatible assembler CLI with `run` subcommand
└── UnitTest/      Test DLL — Microsoft Native CppUnitTest (1023 tests)
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
# Download ROM images to the ROMs/ directory
.\scripts\FetchRoms.ps1

# Run the emulator (defaults to Apple II+)
.\ARM64\Debug\Casso.exe

# Run with a specific machine config
.\ARM64\Debug\Casso.exe --machine Machines\apple2e.json
```

The `ROMs/` directory is gitignored. Available machine configs are in `Machines/`.

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

All 56 standard 6502 mnemonics are implemented.

## Roadmap

### Done

- [x] Pass [Klaus Dormann's 6502 functional test suite](https://github.com/Klaus2m5/6502_65C02_functional_tests) ([#7](https://github.com/relmer/Casso/issues/7))
- [x] Per-opcode validation against [Tom Harte's SingleStepTests](https://github.com/SingleStepTests/ProcessorTests) ([#29](https://github.com/relmer/Casso/issues/29), [#38](https://github.com/relmer/Casso/issues/38))
- [x] Apple //e fidelity — cold boot to BASIC, audit-correct Language Card, 64 KB aux RAM, 80-column text + Double Hi-Res, soft reset vs. power cycle, IRQ/NMI dispatch, RDVBLBAR (spec 004)
- [x] Disk II controller — DOS 3.3 / ProDOS `.dsk` / `.do` / `.po` nibblization + WOZ v1 / v2 with auto-flush on eject (spec 004)
- [x] Headless test harness for deterministic integration tests (`HeadlessHost`, framebuffer scraper, keyboard injector)
- [x] Performance gate — emulator throughput budget enforced in CI (Release-only)

### High Priority

- [ ] Interactive debugger / monitor — step, breakpoints, register watch, memory dump ([#51](https://github.com/relmer/Casso/issues/51))
- [ ] Undocumented / illegal opcode support ([#52](https://github.com/relmer/Casso/issues/52))

### Medium Priority

- [ ] 65C02 extended instruction support, with assembler `--cpu` flag ([#9](https://github.com/relmer/Casso/issues/9))
- [ ] VS Code extension — syntax highlighting, assemble-on-save, inline diagnostics ([#54](https://github.com/relmer/Casso/issues/54))
- [ ] Example programs — ready-to-assemble demos and tutorials ([#55](https://github.com/relmer/Casso/issues/55))
- [ ] Memory-mapped I/O hooks — prerequisite for platform emulation ([#56](https://github.com/relmer/Casso/issues/56))
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

## Acknowledgments

Casso's correctness is validated against two exceptional open-source test suites:

- **[Klaus Dormann's 6502 Functional Test Suite](https://github.com/Klaus2m5/6502_65C02_functional_tests)** — [@Klaus2m5](https://github.com/Klaus2m5)'s exhaustive functional test exercises every documented 6502 behavior: all instructions, addressing modes, flag interactions, BCD arithmetic, and edge cases. Casso passes the full suite.
- **[Tom Harte's SingleStepTests](https://github.com/SingleStepTests/ProcessorTests)** — [@TomHarte](https://github.com/TomHarte)'s per-opcode test vectors validate every legal 6502 opcode against cycle-accurate reference traces. Casso passes all 151 legal-opcode test sets (10,000 vectors each).

Thank you to both authors for making these invaluable resources freely available. They are the gold standard for 6502 emulator validation.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for commit conventions, build instructions, and code style guidelines.

## License

[MIT](LICENSE)
