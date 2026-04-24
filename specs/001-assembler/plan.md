# Implementation Plan: 6502 Assembler

**Branch**: `001-assembler` | **Date**: 2026-04-23 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-assembler/spec.md`

## Summary

A two-pass 6502 assembler implemented as an instance-based `Assembler` class in the My6502Core static library. The assembler converts assembly language source text into machine code bytes by performing a reverse lookup on the existing 256-entry `Microcode` instruction table (mnemonic + addressing mode → opcode byte). Pass 1 determines label addresses; Pass 2 emits final bytes with resolved references. The assembler produces a flat memory image, symbol table, error list, warning list, and listing data. A CLI layer in My6502 provides `assemble` and `run` subcommands. Test integration methods on `TestCpu` enable assemble-and-run testing.

## Technical Context

**Language/Version**: C++ (stdcpplatest, MSVC v145 / Visual Studio 2026)
**Primary Dependencies**: STL only (no third-party libraries)
**Storage**: N/A (in-memory processing; CLI does file I/O for input/output)
**Testing**: Microsoft Native CppUnitTest (CppUnitTestFramework)
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Library (My6502Core) + CLI (My6502) + Test DLL (UnitTest)
**Performance Goals**: Assemble typical programs (< 10K lines) in under 100ms
**Constraints**: Max 64 KB output image (6502 address space), no external dependencies
**Scale/Scope**: 56 standard mnemonics, 13 addressing modes, ~150 valid opcode combinations

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### I. Code Quality — PASS
- Formatting: Will follow existing My6502 style (spaces, column alignment)
- EHM macros: Use EHM patterns (CHR, CBR, BAIL_OUT_IF, etc.) for HRESULT-returning functions; avoid early returns, explicit gotos, and deeply nested code
- Smart pointers: Will use where appropriate for owned resources
- Precompiled headers: All `.cpp` files will include `"Pch.h"` first

### II. Testing Discipline — PASS
- All assembler logic is pure (string in → bytes/errors out); no system state
- Tests in `UnitTest/` project using CppUnitTestFramework
- Test isolation: Assembler takes a string, returns a result struct — fully deterministic
- TestCpu integration: extends existing `TestCpu` class with `Assemble()` / `RunUntil()`

### III. User Experience Consistency — PASS (adapted)
- My6502 currently has no CLI; the `assemble`/`run` subcommand design is greenfield
- Errors to stderr, usage on `--help` / no args
- No backward compatibility concern (no existing CLI behavior)

### IV. Performance Requirements — PASS
- Assembler is not performance-critical at expected scale (< 10K lines)
- Will avoid unnecessary allocations; single-pass string scan per pass

### V. Simplicity & Maintainability — PASS
- Single `Assembler` class with clear two-pass architecture
- Reverse opcode lookup built once from existing `instructionSet[256]`
- Functions under 50 lines; parsing helpers factored out
- No new projects added — all code fits in existing My6502Core/My6502/UnitTest

### Gate Result: **PASS** — No violations. Proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/001-assembler/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   └── assembler-api.md
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
My6502Core/                    # Static library — all assembler logic here
├── Assembler.h                # Assembler class declaration
├── Assembler.cpp              # Two-pass assembler implementation
├── AssemblerTypes.h           # AssemblyResult, AssemblyError, AssemblyLine structs
├── Parser.h                   # Line parser (tokenizer, operand parser)
├── Parser.cpp                 # Parsing implementation
├── OpcodeTable.h              # Reverse lookup table (mnemonic+mode → opcode)
├── OpcodeTable.cpp            # Built from instructionSet[256]
├── Pch.h                      # (existing) — add <vector>, <unordered_map>, <sstream>
├── Cpu.h / Cpu.cpp            # (existing, unchanged)
├── Microcode.h / Microcode.cpp # (existing, unchanged — read-only reference)
└── ...                        # (existing files unchanged)

My6502/                        # Console application
├── My6502.cpp                 # (modified) — add CLI argument parsing, subcommands
├── CommandLine.h              # CLI argument parsing
├── CommandLine.cpp            # assemble/run subcommand dispatch
└── ...

UnitTest/                      # Test DLL
├── TestHelpers.h              # (modified) — add Assemble(), RunUntil(), LabelAddress()
├── AssemblerTests.cpp         # Core assembler unit tests
├── ParserTests.cpp            # Line parser unit tests
├── OpcodeTableTests.cpp       # Reverse lookup table tests
├── IntegrationTests.cpp       # Assemble-and-run integration tests
└── ...                        # (existing test files unchanged)
```

**Structure Decision**: All new code goes into the existing three-project structure. The assembler core (Assembler, Parser, OpcodeTable) lives in My6502Core as a static library. CLI layer lives in My6502. Tests in UnitTest. No new projects needed.

## Constitution Re-Check (Post Phase 1 Design)

### I. Code Quality — PASS
- New files follow existing style (Pch.h first, quoted includes, column alignment)
- EHM macros: Use EHM patterns for HRESULT-returning functions; BAIL_OUT_IF for success tests; avoid early returns and deeply nested code

### II. Testing Discipline — PASS
- Assembler is pure: string in, result struct out — fully deterministic, no system state
- Parser is pure: string in, parsed tokens out
- OpcodeTable is pure: built from in-memory Microcode array
- TestCpu integration: reads/writes only in-memory CPU memory array
- No file I/O, registry, network, or system API access in any testable code
- CLI file I/O is untested at unit level (tested manually / integration)

### III. User Experience Consistency — PASS
- CLI uses standard `--flag` syntax, errors to stderr
- Listing format follows conventional 6502 assembler style

### IV. Performance Requirements — PASS
- No performance-sensitive paths at expected scale

### V. Simplicity & Maintainability — PASS
- 3 new core files (Assembler, Parser, OpcodeTable) with single responsibilities
- No new projects; fits existing 3-project structure
- Functions kept under 50 lines; parsing helpers factored out
- Instance-based Assembler with stateless Assemble() calls

### Gate Result: **PASS** — Design confirmed. Ready for Phase 2 task generation.

## Complexity Tracking

No constitution violations — table intentionally left empty.
