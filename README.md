# My6502

[![CI](https://github.com/relmer/My6502/actions/workflows/ci.yml/badge.svg)](https://github.com/relmer/My6502/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/github/license/relmer/My6502)](LICENSE)

## About

My6502 is a 6502 CPU emulator and assembler written in C++. It emulates the MOS Technology 6502 microprocessor — the chip behind the Apple II, Commodore 64, NES, and Atari 2600.

The project includes:

- **CPU emulator** — fetch-decode-execute cycle with register/flag management, stack operations, and all 14 addressing modes
- **Two-pass assembler** — converts 6502 assembly source to machine code, with labels, directives, expressions, listing output, and error reporting
- **CLI tool** — `assemble` and `run` subcommands for standalone use
- **170+ unit tests** — comprehensive coverage of instruction encoding, addressing modes, arithmetic, branching, and assembler features

## Project Structure

```
My6502.sln
├── My6502Core/     Static library — CPU emulator, assembler, parser, opcode table
├── My6502/         Console application — CLI with assemble/run subcommands
└── UnitTest/       Test DLL — Microsoft Native CppUnitTest (170+ tests)
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
# Assemble a source file to binary
.\x64\Debug\My6502.exe assemble input.asm -o output.bin

# Assemble with listing and symbol table
.\x64\Debug\My6502.exe assemble input.asm -o output.bin -l labels.txt -a

# Assemble and run directly
.\x64\Debug\My6502.exe run input.asm

# Load and run a pre-assembled binary
.\x64\Debug\My6502.exe run output.bin --load $8000
```

## Assembler Features

| Feature | Syntax |
|---------|--------|
| All 56 mnemonics | `LDA`, `STA`, `ADC`, `BNE`, etc. |
| All addressing modes | `#$42`, `$30`, `$1234,X`, `($20),Y`, `A` |
| Labels | `loop: DEX` / `BNE loop` |
| Directives | `.org $8000`, `.byte $FF`, `.word $1234`, `.text "hello"` |
| Comments | `; full line` / `LDA #$42 ; inline` |
| Number formats | `$FF` (hex), `%10101010` (binary), `255` (decimal) |
| Expressions | `label+2`, `<label` (low byte), `>label` (high byte) |
| Listing output | `-a` flag shows address, bytes, and source per line |
| Warning control | `--warn`, `--no-warn`, `--fatal-warnings` |
| Verbose mode | `-v` for detailed assembly progress |
| EEPROM output | `--fill $FF` (default) for EEPROM-ready flat binary |

## CPU Emulation Status

All 56 standard 6502 mnemonics are implemented.

### Working

- Load/Store (LDA, LDX, LDY, STA, STX, STY)
- Arithmetic (ADC, SBC)
- Logic (AND, ORA, EOR)
- Shifts/Rotates (ASL, LSR, ROL, ROR) — *see [#1](https://github.com/relmer/My6502/issues/1) for dispatch bug*
- Compare (CMP, CPX, CPY) — *see [#4](https://github.com/relmer/My6502/issues/4) for carry flag edge case*
- Branch (BEQ, BNE, BCC, BCS, BMI, BPL, BVC, BVS)
- Jump (JMP, JSR)
- Increment/Decrement (INC, DEC, INX, DEX, INY, DEY)
- BIT test — *see [#3](https://github.com/relmer/My6502/issues/3) for V/N flag bug*
- BRK (software interrupt)
- Stack operations (PHA, PLA, PHP, PLP)
- Register transfers (TAX, TXA, TAY, TYA, TXS, TSX)
- Flag manipulation (CLC, SEC, CLI, SEI, CLV, CLD, SED)
- Return instructions (RTS, RTI)
- NOP

## Known Issues

| Issue | Description |
|-------|-------------|
| [#1](https://github.com/relmer/My6502/issues/1) | ShiftLeft/ShiftRight dispatch calls RotateLeft/RotateRight |
| [#3](https://github.com/relmer/My6502/issues/3) | BIT instruction V/N flags read from AND result instead of operand |
| [#4](https://github.com/relmer/My6502/issues/4) | Compare carry flag incorrect for boundary values |

## Roadmap

- [ ] Fix all known CPU bugs ([#1](https://github.com/relmer/My6502/issues/1)–[#5](https://github.com/relmer/My6502/issues/5))
- [ ] Run [Klaus Dormann's 6502 functional test suite](https://github.com/Klaus2m5/6502_65C02_functional_tests) for comprehensive validation
- [ ] Binary file loader (`LoadBinary(filename, address)`)
- [ ] 65C02 extended instruction support
- [ ] Decimal mode (BCD) for ADC/SBC

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for commit conventions, build instructions, and code style guidelines.

## License

[MIT](LICENSE)