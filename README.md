# Casso65

[![CI](https://github.com/relmer/Casso65/actions/workflows/ci.yml/badge.svg?branch=master&event=push)](https://github.com/relmer/Casso65/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/github/license/relmer/Casso65)](LICENSE)

## About

Casso65 is a 6502 CPU emulator and assembler written in C++. It emulates the MOS Technology 6502 microprocessor — the chip behind the Apple II, Commodore 64, NES, and Atari 2600.

The project includes:

- **A real, full-featured AS65-compatible assembler** — Casso65's assembler is a from-scratch reimplementation of Frank A. Kingswood's AS65, intended to be a drop-in replacement. It supports the complete AS65 syntax: macros, conditional assembly (`if`/`ifdef`/`ifndef`/`else`/`endif`), the full expression evaluator (arithmetic, bitwise, logical, shift, `<`/`>` byte selectors, current-PC `*`), `equ`/`=` constants, `include`, three-segment model (`code`/`data`/`bss`), AS65-style listing output, and AS65 command-line flags (`-l`, `-t`, `-s`, `-s2`, `-z`, `-c`, `-w`, `-d`, `-g`, …) including flag concatenation (`-tlfile`).
- **CPU emulator** — fetch-decode-execute cycle with register/flag management, stack operations, decimal mode (BCD) for ADC/SBC, and all 14 addressing modes
- **CLI tool** — runs as an AS65-style assembler by default, or with the `run` subcommand to load and execute a binary or assembly source
- **400+ unit tests** — comprehensive coverage of instruction encoding, addressing modes, arithmetic, branching, and assembler features

## Project Structure

```
Casso65.sln
├── Casso65Core/     Static library — CPU emulator, assembler, parser, opcode table
├── Casso65/         Console application — AS65-compatible assembler CLI with `run` subcommand
└── UnitTest/       Test DLL — Microsoft Native CppUnitTest (400+ tests)
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
.\x64\Debug\Casso65.exe input.a65 -o output.bin

# Assemble with a listing file and a symbol table
.\x64\Debug\Casso65.exe input.a65 -o output.bin -l listing.txt -t

# Output Motorola S-record (.s19) or Intel HEX (.hex)
.\x64\Debug\Casso65.exe input.a65 -s   -o output.s19
.\x64\Debug\Casso65.exe input.a65 -s2  -o output.hex

# Pre-define a symbol on the command line, generate cycle counts in the listing
.\x64\Debug\Casso65.exe input.a65 -d DEBUG=1 -cl listing.txt

# Assemble and run an assembly source directly
.\x64\Debug\Casso65.exe run input.a65

# Load and run a pre-assembled binary at a specific address
.\x64\Debug\Casso65.exe run output.bin --load $8000
```

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

### Working

- Load/Store (LDA, LDX, LDY, STA, STX, STY)
- Arithmetic (ADC, SBC), including decimal mode (BCD) when the D flag is set
- Logic (AND, ORA, EOR)
- Shifts/Rotates (ASL, LSR, ROL, ROR)
- Compare (CMP, CPX, CPY)
- Branch (BEQ, BNE, BCC, BCS, BMI, BPL, BVC, BVS)
- Jump (JMP, JSR)
- Increment/Decrement (INC, DEC, INX, DEX, INY, DEY)
- BIT test
- BRK (software interrupt)
- Stack operations (PHA, PLA, PHP, PLP)
- Register transfers (TAX, TXA, TAY, TYA, TXS, TSX)
- Flag manipulation (CLC, SEC, CLI, SEI, CLV, CLD, SED)
- Return instructions (RTS, RTI)
- NOP

## Roadmap

- [ ] Pass [Klaus Dormann's 6502 functional test suite](https://github.com/Klaus2m5/6502_65C02_functional_tests) (tracked in [#7](https://github.com/relmer/Casso65/issues/7) / [#28](https://github.com/relmer/Casso65/issues/28))
- [ ] Per-opcode validation against [Tom Harte's SingleStepTests](https://github.com/SingleStepTests/ProcessorTests) ([#29](https://github.com/relmer/Casso65/issues/29), [#38](https://github.com/relmer/Casso65/issues/38))
- [ ] 65C02 extended instruction support, with assembler `--cpu` flag ([#9](https://github.com/relmer/Casso65/issues/9))

## Why "Casso65"?

The 6502 emulator world already has [Emu](https://en.wikipedia.org/wiki/Emu) — so we picked its larger, flightless, slightly-more-dangerous cousin: the [cassowary](https://en.wikipedia.org/wiki/Cassowary). "Casso" + "65" (for 6502) = **Casso65**. We want this to be a great emulator and a great assembler — but we don't take ourselves too seriously.

<p align="center">
  <img src="Assets/3a%20Mrs%20Cassowary%20closeup%208167.jpg" alt="Southern Cassowary" width="360" />
</p>

*Cassowary photo by [Mr. Smiley / BunyipCo](https://bunyipco.blogspot.com/2015/04/cassowary-update.html), licensed under [CC BY-NC-SA 3.0](https://creativecommons.org/licenses/by-nc-sa/3.0/) ([license text](Assets/CC-BY-NC-SA-3.0.txt)).*

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for commit conventions, build instructions, and code style guidelines.

## License

[MIT](LICENSE)