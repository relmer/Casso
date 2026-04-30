# Changelog

All notable changes to Casso65 are documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/).
Versioned entries use `MAJOR.MINOR.BUILD` from [Version.h](Casso65Core/Version.h).
Entries before versioning was introduced use dates only.

## [Unreleased]

### Fixed
- **Bright green screen (CPU not executing ROM)** — `Cpu::StepOne` and `Cpu::Run`
  fetched opcodes via `memory[PC]` directly, bypassing `EmuCpu::ReadByte` override
  that routes through the MemoryBus; changed to call virtual `ReadByte(PC)` so the
  CPU correctly reads ROM and I/O through the bus
- **Debug console close kills emulator** — closing the debug console window (Ctrl+D)
  sent CTRL_CLOSE_EVENT which terminated the entire process; added a console control
  handler that intercepts the event and calls FreeConsole instead
- ROM search path bug: ROM files are now searched independently across all search paths,
  fixing failures when `machines/` and `roms/` are at different base directories
- **Black screen in Casso65Emu** — D3D11 shaders were never compiled, so the textured
  quad draw call was skipped and only the black clear color was displayed; implemented
  runtime shader compilation via D3DCompile

### Added
- `PathResolver` class in Casso65EmuCore — testable search-path logic for locating
  machine configs and ROM files
- `MemoryRegion::resolvedPath` — fully resolved ROM path stored after config loading
- Unit tests for `PathResolver` (BuildSearchPaths, FindFile) and `MachineConfigLoader`
  (Load, ROM resolution, error cases)

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
- Project renamed from **My6502** to **Casso65**

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
- Extracted `Casso65Core` static library from monolithic project
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
