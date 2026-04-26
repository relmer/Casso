# Implementation Plan: Full AS65 Assembler Clone

**Branch**: `002-as65-assembler-compat` | **Date**: 2026-04-25 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/002-as65-assembler-compat/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Extend the existing two-pass `Assembler` class (from spec 001) into a full AS65-compatible assembler. The core additions are: a recursive-descent expression evaluator with full C-style operator support (including `!`, `&&`, `||`, `++`, `--`, `[]`), `=`/`equ`/`set` constant definitions (with `=`/`set` reassignable), `*` and `$` as current-PC operators, `if`/`else`/`endif` conditional assembly, `NAME macro`/`endm` macro definitions with positional/named parameters and `\?`/`local`/`exitm`, colon-less labels, `include` file splicing, `struct`/`end struct` layout definitions, `cmap` character mapping, `ds`/`align` storage/alignment directives, all AS65 directive and instruction synonyms (`dd`, `std`, `fcb`/`fcc`/`fcw`/`fdb`/`rmb`), three output formats (binary/S-record/Intel HEX), full AS65 CLI flag parity (including flag concatenation, auto-extension, `nul` discard), and a default origin of 0. The exit criterion is byte-identical assembly of the Klaus Dormann 6502 functional test suite (compared against pre-built reference binaries). The assembler engine MUST be CPU-independent to support future architecture extensions (65C02, 65816, etc.).

## Technical Context

**Language/Version**: C++ (stdcpplatest, MSVC v145 / Visual Studio 2026)  
**Primary Dependencies**: STL only (no third-party libraries)  
**Storage**: N/A (in-memory processing; CLI does file I/O for input/output)  
**Testing**: Microsoft Native CppUnitTest (CppUnitTestFramework)  
**Target Platform**: Windows 10/11, x64 and ARM64  
**Project Type**: Library (Casso65Core) + CLI (Casso65) + Test DLL (UnitTest)  
**Performance Goals**: Assemble the Dormann suite (~6100 lines, heavy macro expansion) in under 1 second  
**Constraints**: Max 64 KB output image (6502 address space), no external dependencies  
**Scale/Scope**: Full AS65 v1.42 feature set (6502 mode), ~96 functional requirements, 19 user stories

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### I. Code Quality — PASS
- Formatting: Will follow existing Casso65 style (spaces, column alignment, 80-char comment delimiters)
- EHM macros: The assembler core is pure (string → result struct); EHM patterns used in CLI file I/O layer
- Smart pointers: Used for macro storage and include file handles
- Precompiled headers: All new `.cpp` files will include `"Pch.h"` first
- Function size: Expression evaluator uses recursive descent with small per-precedence-level functions; macro expansion and conditional assembly factored into separate methods

### II. Testing Discipline — PASS
- All assembler logic is pure (string in → bytes/errors/symbols out); no system state dependency
- Tests in `UnitTest/` project using CppUnitTestFramework
- `include` directive tests use in-memory file resolution (injectable file reader), not real filesystem
- ~200 conformance tests with hand-computed expected outputs (no external assembler binary executed)
- Dormann integration test downloads source + pre-built reference binary on demand (data files only, no executables), compares, deletes
- Existing spec 001 tests remain unchanged (backward compatibility SC-004)
- No GPL code or proprietary executables committed to the repo

### III. User Experience Consistency — PASS
- Extends existing `assemble`/`run` CLI with new AS65-compatible flags (`-c`, `-d`, `-g`, `-h`, `-i`, `-l`, `-m`, `-n`, `-o`, `-p`, `-q`, `-s`, `-s2`, `-t`, `-v`, `-w`, `-z`)
- Error messages follow existing format (file:line: message)
- `--help` updated with all new flags
- Existing CLI behavior preserved (backward compatible)

### IV. Performance Requirements — PASS
- Dormann suite with macro expansion generates ~30K effective lines; two passes over this is well within 1s budget
- Expression evaluator is recursive descent with no backtracking
- Character map is a 256-byte lookup table (O(1) per character)

### V. Simplicity & Maintainability — PASS
- New expression evaluator is a single new file (~600 lines); recursive descent is the simplest correct approach
- Macro/conditional/include processing added to existing `Assembler` class as private methods
- No new projects added — all code fits in existing Casso65Core/Casso65/UnitTest
- Struct/cmap are self-contained features with no coupling to the main assembler loop beyond directive dispatch
- **CPU extensibility**: The assembler engine (expressions, macros, conditionals, directives, segments, listing, CLI) is fully CPU-independent. The instruction set is injected via `OpcodeTable` at construction. Adding 65C02 or other architectures requires only a new instruction table and any new addressing mode patterns — no changes to core assembler logic.

### Gate Result: **PASS** — No violations. Proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/002-as65-assembler-compat/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── conformance-test-plan.md  # Detailed test plan (~200 cases)
├── contracts/           # Phase 1 output
├── checklists/          # Spec quality checklist
│   └── requirements.md
├── reference/           # AS65 manual (text only, no executables)
├── testdata/            # Conformance test inputs + expected outputs
│   └── conformance/     # .a65 inputs + .expected.bin (our work, committed)
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```
```

### Source Code (repository root)

```text
Casso65Core/
├── Assembler.h/.cpp         # Extended: conditionals, macros, includes, segments, struct, cmap
├── ExpressionEvaluator.h/.cpp  # NEW: recursive descent expression parser
├── Parser.h/.cpp            # Extended: colon-less labels, AS65 directives, constant syntax
├── AssemblerTypes.h         # Extended: MacroDefinition, StructDefinition, CharacterMap types
├── OutputFormats.h/.cpp     # NEW: S-record and Intel HEX writers
├── OpcodeTable.h/.cpp       # Extended: instruction synonyms, cycle count data
├── Pch.h/.cpp               # Unchanged
└── (existing files)         # Unchanged

Casso65/
├── CommandLine.h/.cpp       # Extended: AS65-compatible flags
└── Casso65.cpp              # Extended: new flags dispatched

UnitTest/
├── ExpressionEvaluatorTests.cpp  # NEW
├── ConditionalAssemblyTests.cpp  # NEW
├── MacroTests.cpp                # NEW
├── ConstantTests.cpp             # NEW
├── DirectiveTests.cpp            # NEW
├── IncludeTests.cpp              # NEW
├── StructTests.cpp               # NEW
├── CmapTests.cpp                 # NEW
├── OutputFormatTests.cpp         # NEW
├── ConformanceTests.cpp          # NEW: data-driven test runner for ~200 conformance cases
├── DormannIntegrationTests.cpp   # NEW
├── AssemblerTests.cpp            # Existing — must still pass
├── ParserTests.cpp               # Existing — must still pass
└── (existing files)              # Unchanged
```

**Structure Decision**: No new projects. All assembler enhancements go into Casso65Core. New test files per feature area in UnitTest/ plus a data-driven ConformanceTests runner. Two new source files in Casso65Core (`ExpressionEvaluator`, `OutputFormats`). CLI extensions in existing Casso65 project. No external executables downloaded or run.

## Complexity Tracking

> No constitution violations — no entries needed.

## Constitution Re-Check (Post-Design)

### I. Code Quality — PASS
- Expression evaluator: ~10 small functions (one per precedence level), each under 30 lines
- Macro expansion: factored into `ExpandMacro()` helper with clear single purpose
- Conditional assembly: stack-based state machine, ~40 lines total
- Segment handling: three parallel PCs, switch on directive
- `ExpressionEvaluator` and `OutputFormats` are new files with `"Pch.h"` first include

### II. Testing Discipline — PASS
- All new features have dedicated test files
- ~200 conformance tests with hand-computed expected outputs (no external assembler binary)
- `include` tests use injectable `FileReader` — no real filesystem access
- Dormann integration test downloads source + pre-built reference binary (data only), compares, deletes
- No GPL code, proprietary executables, or untrusted binaries in the repo
- Existing tests unchanged — backward compatibility verified

### III. User Experience Consistency — PASS
- CLI supports both AS65-style (`-l -m -s2`) and Casso65-style (`assemble`, `run`) invocation
- Error messages include file:line for included files
- `--help` documents all new flags

### IV. Performance Requirements — PASS
- Expression evaluator: O(n) per expression, no backtracking
- Macro expansion: O(body × invocations), bounded by 15-level nesting limit
- Character map: O(1) per character
- Segment model: O(1) per directive switch

### V. Simplicity & Maintainability — PASS
- 2 new source files, 11 new test files — all in existing projects
- No new projects, no new dependencies
- Each feature (macros, conditionals, structs, cmap) is a self-contained method/block
- Function size budget honored: recursive descent functions ~20 lines each

### Post-Design Gate Result: **PASS** — Design is compliant. Proceed to Phase 2 (tasks).
