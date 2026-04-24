# Tasks: 6502 Assembler

**Input**: Design documents from `/specs/001-assembler/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/assembler-api.md, quickstart.md

**Tests**: TDD approach — tests are written FIRST and must FAIL before implementation begins.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing. US5 (Comments/Whitespace) is absorbed into the foundational Parser and US1 phase since comment handling is inherent to parsing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Exact file paths included in descriptions

## Path Conventions

- **Core library**: `My6502Core/` (static library — all assembler logic)
- **CLI application**: `My6502/` (console app — subcommand dispatch, file I/O)
- **Tests**: `UnitTest/` (CppUnitTestFramework DLL)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization — add new files, update PCH, configure project files

- [X] T001 Update My6502Core/Pch.h to add STL includes needed by assembler (`<vector>`, `<unordered_map>`, `<sstream>`, `<string>`, `<algorithm>`, `<cctype>`)
- [X] T002 [P] Create My6502Core/AssemblerTypes.h with data structs: `AssemblyError` (lineNumber, message), `AssemblyLine` (lineNumber, address, bytes, sourceText, hasAddress), `AssemblyResult` (success, bytes, startAddress, endAddress, symbols, errors, warnings, listing), `AssemblerOptions` (fillByte=0xFF, generateListing=false, warningMode), `WarningMode` enum (Warn, NoWarn, FatalWarnings), `OpcodeEntry` (opcode, operandSize)
- [X] T003 [P] Create stub header and source files for assembler core: My6502Core/Assembler.h, My6502Core/Assembler.cpp, My6502Core/Parser.h, My6502Core/Parser.cpp, My6502Core/OpcodeTable.h, My6502Core/OpcodeTable.cpp — each `.cpp` must include `"Pch.h"` first
- [X] T004 [P] Create stub test files with `"Pch.h"` includes and empty test classes: UnitTest/OpcodeTableTests.cpp, UnitTest/ParserTests.cpp, UnitTest/AssemblerTests.cpp, UnitTest/IntegrationTests.cpp
- [X] T005 Update My6502Core.vcxproj to include new source files (AssemblerTypes.h, Assembler.h/cpp, Parser.h/cpp, OpcodeTable.h/cpp)
- [X] T006 Update UnitTest.vcxproj to include new test files (OpcodeTableTests.cpp, ParserTests.cpp, AssemblerTests.cpp, IntegrationTests.cpp)
- [X] T007 Build all projects (My6502Core, My6502, UnitTest) and verify compilation succeeds with stubs

---

## Phase 2: Foundational (OpcodeTable + Parser Basics)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented — reverse opcode lookup and line parsing

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### Tests (write first — must FAIL)

- [X] T008 [P] Write OpcodeTable unit tests in UnitTest/OpcodeTableTests.cpp: construction from `instructionSet[256]`, `Lookup("LDA", Immediate)` returns opcode `0xA9` with operandSize 1, `Lookup("STA", ZeroPage)` returns `0x85`, `IsMnemonic("LDA")` returns true, `IsMnemonic("XYZ")` returns false, `HasMode("LDA", Immediate)` returns true, `HasMode("LDA", Implied)` returns false, verify all 56 standard mnemonics have at least one entry
- [X] T009 [P] Write basic Parser unit tests in UnitTest/ParserTests.cpp: `SplitLines()` splits on `\n`, `ParseLine()` extracts label from `"loop: LDA #$42"`, extracts mnemonic (case-insensitive: `"lda"` → `"LDA"`), extracts operand string, strips full-line comments (`"; comment"`), strips inline comments (`"LDA #$42 ; load"`), handles blank lines, handles lines with only whitespace

### Implementation

- [X] T010 Implement OpcodeTable class in My6502Core/OpcodeTable.h and My6502Core/OpcodeTable.cpp: constructor iterates `instructionSet[0..255]`, skips illegal entries, builds `unordered_map<string, unordered_map<AddressingMode, OpcodeEntry>>` keyed by uppercase mnemonic; implement `Lookup()`, `IsMnemonic()`, `HasMode()`
- [X] T011 Implement basic Parser in My6502Core/Parser.h and My6502Core/Parser.cpp: `SplitLines()` to break source text into lines, `ParseLine()` to extract label (before `:`), mnemonic (uppercase), operand (remainder), strip comments (everything after `;` outside quotes), skip blank/whitespace-only lines

**Checkpoint**: OpcodeTable and Parser basics ready — user story implementation can now begin

---

## Phase 3: User Story 1 — Assemble Simple Instructions (P1) + User Story 5 — Comments/Whitespace (P3) 🎯 MVP

**Goal**: Convert basic 6502 instructions (all addressing modes) into correct opcode bytes. Comments and whitespace are handled gracefully.

**Independent Test**: Assemble a handful of instructions, compare emitted bytes against known-good opcodes, verify output length.

### Tests (write first — must FAIL) ⚠️

- [X] T012 [P] [US1] Write instruction encoding tests in UnitTest/AssemblerTests.cpp: `LDA #$42` → bytes `{0xA9, 0x42}`, `STA $10` → `{0x85, 0x10}`, `JMP $1234` → `{0x4C, 0x34, 0x12}`, `ROL A` → `{0x2A}`, `NOP` → `{0xEA}`, `LDA $10,X` → `{0xB5, 0x10}`, `LDA $1234,X` → `{0xBD, 0x34, 0x12}`, `LDA $1234,Y` → `{0xB9, 0x34, 0x12}`, `STA $10,X` → `{0x95, 0x10}`, `STX $10,Y` → `{0x96, 0x10}`, `LDA ($10,X)` → `{0xA1, 0x10}`, `LDA ($10),Y` → `{0xB1, 0x10}`, `JMP ($1234)` → `{0x6C, 0x34, 0x12}`
- [X] T013 [P] [US1] [US5] Write comment and whitespace handling tests in UnitTest/AssemblerTests.cpp: full-line comment `"; this is a comment"` produces zero bytes, inline comment `"LDA #$42 ; load value"` assembles correctly to `{0xA9, 0x42}`, blank lines interspersed produce same output as without blanks, varied indentation and spacing `"  LDA   #$42  "` assembles correctly

### Implementation

- [X] T014 [US1] Implement operand addressing mode classifier in My6502Core/Parser.cpp: detect `#value` (Immediate), `A` (Accumulator), `$ZZ` (ZeroPage), `$HHHH` (Absolute), `$ZZ,X` (ZeroPageX), `$ZZ,Y` (ZeroPageY), `$HHHH,X` (AbsoluteX), `$HHHH,Y` (AbsoluteY), `($ZZ,X)` (ZeroPageXIndirect), `($ZZ),Y` (ZeroPageIndirectY), `($HHHH)` (JumpIndirect), no operand (SingleByteNoOperand) — include hex value parsing (`$FF` → 0xFF). Note: `Relative` and `JumpAbsolute` modes are inferred from the mnemonic, not syntax — branches (BEQ, BNE, etc.) use Relative, JMP absolute uses JumpAbsolute. The parser must perform mnemonic-aware disambiguation: `JMP $1234` → JumpAbsolute (not Absolute), branch targets → Relative (not ZeroPage/Absolute).
- [X] T015 [US1] Implement two-pass Assembler::Assemble() skeleton in My6502Core/Assembler.h and My6502Core/Assembler.cpp: constructor takes `const Microcode instructionSet[256]` and `AssemblerOptions`, builds `OpcodeTable`; Pass 1 parses lines and advances PC by instruction size; Pass 2 emits opcode + operand bytes into flat `vector<Byte>` image; returns `AssemblyResult` with bytes, startAddress, endAddress
- [X] T016 [US1] Implement zero-page preference in My6502Core/Assembler.cpp: when operand value fits in one byte (0x00–0xFF) and mnemonic supports both ZeroPage and Absolute modes, emit the shorter ZeroPage encoding (FR-016)

**Checkpoint**: Basic instructions assemble correctly. Comments and whitespace handled. This is a usable MVP — can assemble simple instruction sequences.

---

## Phase 4: User Story 2 — Labels and Branching (P1)

**Goal**: Support named labels and branch/jump instructions that reference labels. Two-pass resolution: Pass 1 records label addresses, Pass 2 resolves all references.

**Independent Test**: Assemble code with forward and backward label references, verify branch offsets and jump targets resolve correctly.

### Tests (write first — must FAIL) ⚠️

- [X] T017 [P] [US2] Write label resolution tests in UnitTest/AssemblerTests.cpp: forward reference `"BEQ target\nNOP\ntarget: NOP"` produces correct branch offset, backward reference `"loop: INX\nBNE loop"` produces correct negative offset, `"JMP label"` with `"label: NOP"` produces correct absolute address in little-endian, `"JSR sub"` with `"sub: RTS"` produces correct absolute address, label appears in `result.symbols` map with correct address
- [X] T018 [P] [US2] Write label error tests in UnitTest/AssemblerTests.cpp: duplicate label `"dup: NOP\ndup: NOP"` reports error with line number, undefined label `"BEQ nowhere"` reports error, label named `"LDA"` (collides with mnemonic) reports error, label named `"A"` or `"X"` or `"Y"` (collides with register name) reports error

### Implementation

- [X] T019 [US2] Implement Pass 1 symbol table building in My6502Core/Assembler.cpp: on label definition, record name → current PC in `unordered_map<string, Word>`, detect duplicate labels (error with line number); implement label name validation in My6502Core/Parser.cpp: must start with letter or underscore, alphanumeric + underscore only, not a reserved mnemonic (checked via `OpcodeTable::IsMnemonic()`), not a register name (A, X, Y, S)
- [X] T020 [US2] Implement Pass 2 label resolution in My6502Core/Assembler.cpp: when operand is a label name (not a numeric literal), look up address in symbol table; for absolute addressing emit little-endian address; for undefined labels record error and emit placeholder zeros
- [X] T021 [US2] Implement relative branch offset calculation in My6502Core/Assembler.cpp: for branch mnemonics (BEQ, BNE, BCC, BCS, BMI, BPL, BVC, BVS), compute signed offset from (PC after branch instruction) to target address, validate range -128 to +127, report error if out of range

**Checkpoint**: Labels, forward/backward references, and branch offsets all resolve correctly. Symbol table populated.

---

## Phase 5: User Story 6 — Assemble-and-Run Integration in Tests (P1)

**Goal**: TestCpu convenience methods to assemble source text directly into emulator memory, execute it, and verify CPU state — making unit tests dramatically more readable.

**Independent Test**: Write a test that assembles a short program, runs it to a label, and asserts register values.

### Tests (write first — must FAIL) ⚠️

- [X] T022 [P] [US6] Write `TestCpu::Assemble()` tests in UnitTest/IntegrationTests.cpp: assemble `"LDA #$42\nSTA $10"` at default address `0x8000`, verify memory at `0x8000` contains `{0xA9, 0x42, 0x85, 0x10}`, verify PC is set to `0x8000`; assemble with explicit `startAddress = 0xC000`, verify bytes at `0xC000`; assemble with errors returns `success == false` and memory is unchanged
- [X] T023 [P] [US6] Write `TestCpu::RunUntil()` tests in UnitTest/IntegrationTests.cpp: assemble `"LDA #$42\nSTA $10\ndone: BRK"`, run until `LabelAddress("done")`, verify `RegA() == 0x42` and `Peek(0x10) == 0x42`; test cycle-limit timeout (run with maxCycles=1, verify does not reach target); test illegal opcode stop (jump to uninitialized memory `$FF`)
- [X] T024 [P] [US6] Write `TestCpu::LabelAddress()` test in UnitTest/IntegrationTests.cpp: assemble program with labels `"start: NOP\nend: BRK"`, verify `LabelAddress(result, "start")` returns `0x8000` and `LabelAddress(result, "end")` returns `0x8001`
- [X] T024a [P] [US6] Write BRK software interrupt test in UnitTest/IntegrationTests.cpp: set up IRQ vector at $FFFE/$FFFF pointing to a handler address, execute BRK, verify PC loaded from IRQ vector, status and PC+2 pushed to stack, B flag set in pushed status (FR-021d)

### Implementation

- [X] T025 [US6] Add `Assemble()` and `LabelAddress()` to TestCpu in UnitTest/TestHelpers.h: `Assemble(const char* source, Word startAddress = 0x8000)` creates an `Assembler` with the CPU's instruction set, assembles source, on success writes bytes to `memory[startAddress..]` and sets PC to `startAddress`, returns `AssemblyResult`; `LabelAddress(const AssemblyResult& result, const char* name)` looks up name in `result.symbols`
- [X] T026 [US6] Add `RunUntil()` to TestCpu in UnitTest/TestHelpers.h: `RunUntil(Word targetAddress, uint32_t maxCycles = 0)` loops calling `Step()`, stops when `PC == targetAddress`, or illegal opcode encountered, or `cycles >= maxCycles` (if maxCycles > 0); expose stop reason for test assertions

**Checkpoint**: Test developer can write assemble-and-run tests in ~5 lines. All subsequent story tests can use this pattern.

---

## Phase 6: User Story 3 — Directives for Data and Origin (P2)

**Goal**: Support `.org` (set origin address), `.byte` (raw bytes), `.word` (16-bit little-endian), `.text` (ASCII string) directives.

**Independent Test**: Assemble programs using each directive, verify output bytes at expected offsets.

### Tests (write first — must FAIL) ⚠️

- [X] T027 [P] [US3] Write `.org` directive tests in UnitTest/AssemblerTests.cpp: `.org $C000` followed by `NOP` → NOP at address `$C000` (startAddress == 0xC000), `.org` backward from current PC reports error
- [X] T028 [P] [US3] Write `.byte` directive tests in UnitTest/AssemblerTests.cpp: `.byte $FF,$00,$42` emits three bytes `{0xFF, 0x00, 0x42}` in order
- [X] T029 [P] [US3] Write `.word` directive tests in UnitTest/AssemblerTests.cpp: `.word $1234,$ABCD` emits four bytes `{0x34, 0x12, 0xCD, 0xAB}` (little-endian)
- [X] T030 [P] [US3] Write `.text` directive tests in UnitTest/AssemblerTests.cpp: `.text "Hello"` emits `{0x48, 0x65, 0x6C, 0x6C, 0x6F}`, `.text ""` emits zero bytes
- [X] T031 [P] [US3] Write label-before-data test in UnitTest/AssemblerTests.cpp: label before `.byte` data resolves to the address where data begins

### Implementation

- [X] T032 [US3] Implement directive parsing in My6502Core/Parser.cpp: detect `.org`, `.byte`, `.word`, `.text` as directive keywords, parse comma-separated value lists, parse quoted strings
- [X] T033 [US3] Implement `.org` handling in My6502Core/Assembler.cpp: set PC to specified address, validate address is not backward from current output position (error if so), adjust flat image start/end tracking
- [X] T034 [US3] Implement `.byte`, `.word`, `.text` data emission in My6502Core/Assembler.cpp: `.byte` emits each value as a single byte, `.word` emits each value as two bytes little-endian, `.text` emits each character as its ASCII byte value

**Checkpoint**: All directives work. Can set code origin, embed data tables and strings.

---

## Phase 7: User Story 4 — Number Formats and Simple Expressions (P2)

**Goal**: Support hexadecimal (`$FF`), binary (`%10101010`), and decimal (`255`) literals, plus label expressions (`<label`, `>label`, `label+offset`).

**Independent Test**: Assemble instructions using each format and expression, verify emitted operand bytes.

### Tests (write first — must FAIL) ⚠️

- [X] T035 [P] [US4] Write number format tests in UnitTest/ParserTests.cpp: `$FF` → 255, `$ff` → 255 (case-insensitive hex), `%10101010` → 170, `%00001111` → 15, `255` → 255, `0` → 0
- [X] T036 [P] [US4] Write expression tests in UnitTest/AssemblerTests.cpp: label `data` at `$1234`, `LDA #<data` → operand `0x34` (low byte), `LDA #>data` → operand `0x12` (high byte), `LDA table+3` with `table` at `$2000` → operand address `$2003`

### Implementation

- [X] T037 [US4] Implement binary (`%10101010`) and decimal (`255`) number parsing in My6502Core/Parser.cpp alongside existing hex parsing
- [X] T038 [US4] Implement `<label` (low byte) and `>label` (high byte) operators and `label+offset` expression evaluation in My6502Core/Assembler.cpp during Pass 2 operand resolution

**Checkpoint**: All number formats and label expressions resolve correctly.

---

## Phase 8: User Story 7 — Error Reporting (P2)

**Goal**: Collect all errors with line numbers rather than stopping at the first error. Cover all error conditions: invalid mnemonics, bad syntax, undefined labels, duplicates, out-of-range values/branches.

**Independent Test**: Feed intentionally malformed assembly, verify each error has correct line number and meaningful message.

### Tests (write first — must FAIL) ⚠️

- [X] T039 [P] [US7] Write comprehensive error tests in UnitTest/AssemblerTests.cpp: invalid mnemonic `"XYZ"` → error with line number, missing operand `"LDA"` → error, value out of range `"LDA #$1FF"` → error, branch out of range (target > 127 bytes away) → error, source with 3 errors on different lines → all 3 collected with correct line numbers
- [X] T040 [P] [US7] Write Pass 1 error recovery test in UnitTest/AssemblerTests.cpp: parse error on line 2, label defined on line 4, label address is close to correct (best-effort PC estimation kept PC advancing)

### Implementation

- [X] T041 [US7] Harden error collection in My6502Core/Assembler.cpp: add `AssemblyError` for each error condition (invalid mnemonic, missing operand, undefined label, duplicate label, operand out of range, branch out of range, `.org` backward), continue assembly after each error, set `result.success = false` if any errors
- [X] T042 [US7] Implement best-effort PC estimation in My6502Core/Assembler.cpp: when a line fails to parse in Pass 1, estimate instruction size (1 byte for unknown implied, 2 bytes for unknown with operand, 3 bytes for unknown with long operand) and advance PC to minimize cascading label offset errors

**Checkpoint**: All error conditions detected. Multiple errors collected per assembly with correct line numbers.

---

## Phase 9: User Story 9 — Listing File Output (P1)

**Goal**: Generate listing data showing address, hex bytes, and original source text per line — critical for debugging assembly programs.

**Independent Test**: Assemble with listing enabled, verify listing entries have correct address, bytes, and source for each line type.

### Tests (write first — must FAIL) ⚠️

- [X] T043 [P] [US9] Write listing output tests in UnitTest/AssemblerTests.cpp: instruction line `"LDA #$42"` has `hasAddress=true`, `address=0x8000`, `bytes={0xA9,0x42}`, `sourceText="LDA #$42"`; comment-only line has `hasAddress=false`, `sourceText="; comment"`; `.byte` line shows data bytes; label-only line shows address; `.org` line shows new address; listing is empty when `generateListing == false`

### Implementation

- [X] T044 [US9] Implement `AssemblyLine` population during Pass 2 in My6502Core/Assembler.cpp: for each source line, record lineNumber, address, emitted bytes, original sourceText, and hasAddress flag; only populate when `options.generateListing == true`
- [X] T045 [US9] Implement listing format helper in My6502Core/Assembler.h or My6502Core/Assembler.cpp: format listing lines as `"$XXXX  XX XX XX  source text"` with address column (4-digit hex), bytes column (up to 3 hex bytes), and source column — used by CLI for `-a` output

**Checkpoint**: Listing data correctly represents all line types (instructions, directives, labels, comments).

---

## Phase 10: User Story 8 — Command-Line Interface (P1)

**Goal**: CLI with `assemble` and `run` subcommands modeled after ACME assembler. Primary non-test entry point.

**Independent Test**: Run executable with various argument combinations, check exit codes, output files, stderr messages.

> **Note**: CommandLine lives in My6502/ project (not My6502Core), so unit tests cannot directly test it. Acceptance is validated through integration testing (running the executable) and the CLI-vs-API equivalence test (T057e). The assembler core is already fully unit-tested in earlier phases.

### Implementation

- [ ] T046 [US8] Create My6502/CommandLine.h with `CommandLineOptions` struct (subcommand, inputFile, outputFile, symbolFile, fillByte, loadAddress, stopAddress, entryAddress, maxCycles, useResetVector, verbose, generateListing, warningMode, showVersion, showHelp) and `ParseCommandLine(int argc, char* argv[])` declaration
- [ ] T047 [US8] Implement CLI argument parsing in My6502/CommandLine.cpp: parse `assemble <input> -o <output> [-l <symbols>] [-a] [--fill <byte>] [-v] [--warn|--no-warn|--fatal-warnings]`, parse `run <input> [--load <addr>] [--entry <addr>] [--reset-vector] [--stop <addr>] [--max-cycles <n>] [-v]`, parse `--help` and `--version`, validate required arguments
- [ ] T048 [US8] Implement `DoAssemble()` in My6502/CommandLine.cpp: read input `.asm` file, create `Assembler` with options, call `Assemble()`, write binary output to `-o` file, optionally write symbol table to `-l` file, optionally print listing to stdout (`-a`), print errors to stderr, return appropriate exit code (0=success, 1=assembly errors, 2=I/O error)
- [ ] T049 [US8] Implement `DoRun()` in My6502/CommandLine.cpp: if `.asm` file → assemble then execute; if `.bin` file → load at `--load` address; determine entry point (default=lowest assembled address or `$8000`, `--entry` override, `--reset-vector` for `$FFFC/$FFFD`); execute with stop conditions (`--stop`, illegal opcode, `--max-cycles`); print execution summary; exit code 3 for illegal opcode stop
- [ ] T050 [US8] Update My6502/My6502.cpp: replace current `main()` with subcommand dispatch — call `ParseCommandLine()`, handle `--help` (print usage summary), `--version` (print version), dispatch to `DoAssemble()` or `DoRun()`, return exit code
- [ ] T051 [US8] Update My6502.vcxproj to include CommandLine.h and CommandLine.cpp

**Checkpoint**: CLI fully functional with `assemble` and `run` subcommands. Exit codes, stderr errors, and all flags working.

---

## Phase 11: User Story 10 — Verbose Output and Warning Control (P2)

**Goal**: Verbose mode for detailed assembly progress. Warning modes: show (default), suppress, or treat as errors.

**Independent Test**: Assemble with different flags, verify expected messages appear or are suppressed.

### Tests (write first — must FAIL) ⚠️

- [ ] T052 [P] [US10] Write warning mode tests in UnitTest/AssemblerTests.cpp: with `WarningMode::Warn` — warning is recorded in `result.warnings` but `result.success` remains true; with `WarningMode::FatalWarnings` — warning is promoted to `result.errors` and `result.success` is false; with `WarningMode::NoWarn` — warning is suppressed (not recorded). Warning conditions to test: unused label (defined but never referenced), redundant `.org` to same address, label name differing from mnemonic only by case (e.g., `lda:`) (FR-033a)

### Implementation

- [ ] T053 [US10] Implement warning mode handling in My6502Core/Assembler.cpp: when recording a warning, check `options.warningMode` — `Warn` adds to `result.warnings`, `FatalWarnings` adds to `result.errors` and sets `success=false`, `NoWarn` discards silently
- [ ] T054 [US10] Implement verbose output in My6502/CommandLine.cpp: when `-v` flag is set, print to stderr: pass number start/end, symbol resolution details (label → address), byte count summary, timing information

**Checkpoint**: Warning modes work correctly in assembler core. Verbose output available in CLI.

---

## Phase 12: Polish & Cross-Cutting Concerns

**Purpose**: Validation, comprehensive coverage, and final quality checks

- [ ] T055 [P] Run quickstart.md validation — assemble and execute the example program from quickstart.md, verify expected output
- [ ] T056 [P] Write comprehensive opcode coverage test in UnitTest/OpcodeTableTests.cpp — verify all 56 mnemonics × valid addressing mode combinations produce correct opcodes (SC-001)
- [ ] T057 [P] Write `WriteBytes()` equivalence test in UnitTest/IntegrationTests.cpp — assemble a program, also `WriteBytes()` the same raw bytes, verify both produce identical CPU execution results (SC-005)
- [ ] T057a [P] Write Assembler instance reuse test in UnitTest/AssemblerTests.cpp — create one Assembler, call `Assemble()` twice with different source, verify results are independent and correct (FR-041)
- [ ] T057b [P] Write case-sensitive labels test in UnitTest/AssemblerTests.cpp — define both `foo:` and `FOO:` at different addresses, verify they resolve to different addresses; reference both, verify correct resolution (FR-023)
- [ ] T057c [P] Write 100-label stress test in UnitTest/AssemblerTests.cpp — generate a program with ~100 labels and cross-references, verify all resolve correctly (SC-002)
- [ ] T057d [P] Write empty source text test in UnitTest/AssemblerTests.cpp — `Assemble("")` returns success with zero bytes and no errors (edge case)
- [ ] T057e [P] Write CLI-vs-API equivalence test — assemble same source via CLI and via `Assembler::Assemble()`, compare output byte-for-byte (SC-007; may be manual/integration if not automatable in UnitTest)
- [ ] T058 Run full test suite and verify zero test failures
- [ ] T059 Build Release configuration for both x64 and ARM64, verify no errors or warnings

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — BLOCKS all user stories
- **US1+US5 (Phase 3)**: Depends on Phase 2 — first user story, MVP core
- **US2 (Phase 4)**: Depends on Phase 3 (needs instruction encoding for label tests)
- **US6 (Phase 5)**: Depends on Phase 4 (needs labels for `RunUntil`/`LabelAddress`)
- **US3 (Phase 6)**: Depends on Phase 4 (needs label + PC management from two-pass)
- **US4 (Phase 7)**: Depends on Phase 3 (extends number parsing)
- **US7 (Phase 8)**: Depends on Phase 4 (needs assembler structure for error paths)
- **US9 (Phase 9)**: Depends on Phase 4 (needs assembled output to generate listing)
- **US8 (Phase 10)**: Depends on Phases 3–9 (needs full assembler core)
- **US10 (Phase 11)**: Depends on Phase 10 (extends CLI)
- **Polish (Phase 12)**: Depends on all user stories

### User Story Dependencies

```
Phase 2 (Foundational)
  │
  ├──► Phase 3: US1+US5 (Simple Instructions + Comments)  🎯 MVP
  │      │
  │      ├──► Phase 4: US2 (Labels & Branching)
  │      │      │
  │      │      ├──► Phase 5: US6 (TestCpu Integration)
  │      │      ├──► Phase 6: US3 (Directives)
  │      │      ├──► Phase 8: US7 (Error Reporting)
  │      │      └──► Phase 9: US9 (Listing Output)
  │      │
  │      └──► Phase 7: US4 (Number Formats)
  │
  └──► [All above] ──► Phase 10: US8 (CLI)
                            │
                            └──► Phase 11: US10 (Verbose/Warnings)
```

### Within Each User Story (TDD Order)

1. **Test tasks** run FIRST — they must FAIL before implementation begins
2. Test tasks marked [P] can be written in parallel (different test methods/classes)
3. Implementation tasks follow sequentially
4. All tests must PASS after implementation
5. Build must succeed before moving to next phase

### Parallel Opportunities

- **Phase 1**: T002, T003, T004 can run in parallel (independent files)
- **Phase 2**: T008, T009 can run in parallel (independent test files)
- **Phase 3**: T012, T013 can run in parallel (independent test methods)
- **Phase 4**: T017, T018 can run in parallel (independent test methods)
- **Phase 5**: T022, T023, T024 can run in parallel (independent test methods)
- **Phase 6**: T027–T031 can all run in parallel (independent test methods)
- **Phase 7**: T035, T036 can run in parallel (different test files)
- **Phase 8**: T039, T040 can run in parallel (independent test methods)
- **Phases 6–9** can run in parallel once Phase 4 is complete (different files, no cross-dependencies)

---

## Parallel Example: User Story 1 (Phase 3)

```text
# Write both test groups simultaneously:
T012: instruction encoding tests in UnitTest/AssemblerTests.cpp
T013: comment/whitespace tests in UnitTest/AssemblerTests.cpp

# Then implement sequentially:
T014: operand addressing mode classifier in My6502Core/Parser.cpp
T015: Assembler skeleton in My6502Core/Assembler.h + My6502Core/Assembler.cpp
T016: zero-page preference in My6502Core/Assembler.cpp
```

---

## Implementation Strategy

### MVP First (Phase 3 = User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (OpcodeTable + Parser)
3. Complete Phase 3: US1+US5 (Simple Instructions)
4. **STOP and VALIDATE**: Assemble basic instructions, verify bytes match expected opcodes
5. This alone is useful — can convert mnemonics to machine code

### Incremental Delivery

1. Setup + Foundational → Infrastructure ready
2. US1+US5 → Basic assembler works (MVP!)
3. US2 → Labels and branching → Real programs possible
4. US6 → TestCpu integration → Tests become readable
5. US3 → Directives → Data tables, origin control
6. US4 → Number formats → Full expression support
7. US7 → Error reporting → Production-quality diagnostics
8. US9 → Listing output → Debugging support
9. US8 → CLI → Standalone tool
10. US10 → Verbose/warnings → Polish
11. Each increment is independently testable and valuable

---

## Notes

- Every `.cpp` file MUST include `"Pch.h"` as its first `#include`
- All system/STL headers go in `Pch.h` only — project files use quoted includes (`"header.h"`)
- `Byte` = `unsigned char`, `SByte` = `signed char`, `Word` = `unsigned short` (defined in `Pch.h`)
- Tests use `TestCpu` subclass from `TestHelpers.h` to access protected `Cpu` members
- Use `TestCpu::InitForTest()` for clean state — never rely on `Cpu::Reset()`
- Assembler is instance-based: config at construction, stateless `Assemble()` calls
- OpcodeTable is built from existing `instructionSet[256]` — no hardcoded opcode values
- Labels are case-sensitive; mnemonics and register names are case-insensitive
- Fill byte default is `$FF` (EEPROM erased state / illegal opcode → natural execution stop)
- Commit after each completed phase; all tests must pass before committing
