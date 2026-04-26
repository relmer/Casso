# Tasks: Full AS65 Assembler Clone

**Input**: Design documents from `/specs/002-as65-assembler-compat/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/api.md, quickstart.md

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Exact file paths included in descriptions

---

## Phase 1: Setup

**Purpose**: Add new source files to the build, extend types, prepare test infrastructure

- [X] T001 Add ExpressionEvaluator.h/.cpp and OutputFormats.h/.cpp to Casso65Core/Casso65Core.vcxproj
- [X] T002 [P] Add new test .cpp files to UnitTest/UnitTest.vcxproj (ExpressionEvaluatorTests, ConditionalAssemblyTests, MacroTests, ConstantTests, DirectiveTests, IncludeTests, StructTests, CmapTests, OutputFormatTests, DormannIntegrationTests, ConformanceTests)
- [X] T003 [P] Extend AssemblerTypes.h with new types: MacroDefinition, StructDefinition, StructMember, CharacterMap, SymbolKind enum, ConditionalState, FileReader interface in Casso65Core/AssemblerTypes.h
- [X] T004 [P] Download AS65 manual (`as65.man`) and `testcase.a65` from Klaus2m5 repo (text files only — NO executables) to specs/002-as65-assembler-compat/reference/
- [ ] T004a [P] Create conformance test input files (.a65) with hand-computed expected outputs (.expected.bin) per conformance-test-plan.md categories 1–14 in specs/002-as65-assembler-compat/testdata/conformance/ (our own work, committed)
- [ ] T004b [P] Create scripts/RunDormannTest.ps1 — downloads 6502_functional_test.a65 and reference binary from Klaus2m5 bin_files/, assembles with Casso65, compares against reference, deletes all downloaded files on completion
- [ ] T004c [P] Add downloaded temp files to .gitignore

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Expression evaluator and parser rewrite — everything else depends on these

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T005 Create ExpressionEvaluator.h with ExprContext, ExprResult structs and static Evaluate method in Casso65Core/ExpressionEvaluator.h
- [X] T006 Implement recursive descent expression parser with all 13 precedence levels (unary, mul/div/mod, add/sub, shift, comparison, equality, bitAnd, bitXor, bitOr, logAnd, logOr, lo/hi) in Casso65Core/ExpressionEvaluator.cpp
- [X] T007 Implement tokenizer with basic number formats ($hex, %bin, decimal, 'char') in Casso65Core/ExpressionEvaluator.cpp
- [X] T008 Implement `*` and `$` (bare, not followed by hex digit) disambiguation as current-PC vs multiplication/hex-prefix based on context (FR-012) in Casso65Core/ExpressionEvaluator.cpp
- [X] T008a Implement `++` and `--` as unary prefix operators (FR-001a) in Casso65Core/ExpressionEvaluator.cpp
- [X] T008b Implement `[` and `]` as alternative grouping symbols equivalent to `(` and `)` (FR-001b) in Casso65Core/ExpressionEvaluator.cpp
- [X] T009 Implement `lo`/`hi` keyword operators, `!` logical NOT, `&&` logical AND, and `||` logical OR in Casso65Core/ExpressionEvaluator.cpp
- [X] T010 [P] Create expression evaluator unit tests (categories 1.1–1.9 from conformance-test-plan.md, ~35 cases) in UnitTest/ExpressionEvaluatorTests.cpp
- [X] T011 Rewrite Parser::ParseLine to support colon-less labels (column-0 identifier detection), constant definitions (`NAME = EXPR`, `NAME equ EXPR`, `NAME set EXPR`), and AS65 directive recognition in Casso65Core/Parser.cpp
- [X] T012 Replace Parser::ClassifyOperand with expression-based operand evaluation (delegate to ExpressionEvaluator instead of manual label+offset parsing) in Casso65Core/Parser.cpp
- [X] T013 Verify all existing AssemblerTests.cpp and ParserTests.cpp pass unchanged (backward compatibility SC-004)

**Checkpoint**: Expression evaluator and parser foundation ready — user story work can begin

---

## Phase 3: User Story 1 — Full Expression Evaluator (Priority: P1) 🎯 MVP

**Goal**: Replace the limited operand parser with a full expression evaluator so instructions can use complex expressions in operands

**Independent Test**: Assemble `LDA #$FF & $0F` and verify operand byte is `$0F`

- [X] T014 [US1] Integrate ExpressionEvaluator into Assembler pass 2 for resolving instruction operands in Casso65Core/Assembler.cpp
- [X] T015 [US1] Integrate ExpressionEvaluator into directive arguments (.org, .byte, .word value lists) in Casso65Core/Assembler.cpp
- [X] T016 [US1] Add range-checking: truncate to 1 byte for immediate/zp, 2 bytes for absolute, signed byte for branch in Casso65Core/Assembler.cpp
- [X] T017 [US1] Verify backward compatibility: `<label`, `>label`, `label+offset` produce identical output to spec 001

**Checkpoint**: Expression evaluator working in all operand and directive contexts

---

## Phase 4: User Story 2 — Constant Definitions + User Story 3 — Current-PC (Priority: P1)

**Goal**: Support `=`/`equ`/`set` constants and `*` as current-PC operator

**Independent Test**: Assemble `value = $42` / `LDA #value` and verify `$42`; assemble `jmp *` and verify self-targeting JMP

- [X] T018 [US2] Add symbol kind tracking (Label/Equ/Set) to symbol table in Casso65Core/Assembler.cpp
- [X] T019 [US2] Implement `NAME = EXPR` and `NAME set EXPR` (eager evaluation, reassignable) in Casso65Core/Assembler.cpp
- [X] T020 [US2] Implement `NAME equ EXPR` (deferred to pass 2, immutable) in Casso65Core/Assembler.cpp
- [X] T021 [US2] Implement cross-kind redefinition errors (equ→=, label→=, etc.) in Casso65Core/Assembler.cpp
- [X] T022 [US3] Wire `*` and `$` (current-PC) into ExpressionEvaluator via ExprContext.currentPC in Casso65Core/Assembler.cpp
- [X] T022a [US3] Change default origin from $8000 to 0 when no .org is given (FR-034b2) in Casso65Core/Assembler.cpp
- [X] T023 [US3] Verify `jmp *`, `beq *`, `jmp $`, `name = *+1` all produce correct output
- [X] T024 [P] [US2] Create constant definition tests (category 2 from conformance-test-plan.md, ~20 cases) in UnitTest/ConstantTests.cpp

**Checkpoint**: Constants and current-PC working — prerequisite for conditionals and macros

---

## Phase 5: User Story 4 — Conditional Assembly (Priority: P1)

**Goal**: Support `if`/`else`/`endif` directives that control which source lines are assembled

**Independent Test**: Assemble `flag = 1` / `if flag` / `NOP` / `endif` and verify one NOP

- [X] T025 [US4] Implement conditional state stack (assembling/skipping, seenElse, parentAssembling) in Casso65Core/Assembler.cpp
- [X] T026 [US4] Implement `if EXPR` directive processing with expression evaluation in Casso65Core/Assembler.cpp
- [X] T027 [US4] Implement `else` and `endif` directive processing with stack management in Casso65Core/Assembler.cpp
- [X] T028 [US4] Ensure skipped blocks define zero symbols, emit zero bytes, but still track if/else/endif nesting in Casso65Core/Assembler.cpp
- [X] T029 [US4] Support `.if`/`.else`/`.endif` dot-prefixed forms in Casso65Core/Assembler.cpp
- [X] T030 [US4] Add error reporting for unmatched else/endif and unclosed if in Casso65Core/Assembler.cpp
- [X] T031 [P] [US4] Create conditional assembly tests (category 3 from conformance-test-plan.md, ~20 cases) in UnitTest/ConditionalAssemblyTests.cpp

**Checkpoint**: Conditional assembly working — prerequisite for Dormann's config-guarded sections

---

## Phase 6: User Story 5 — Basic Macros (Priority: P1)

**Goal**: Support `NAME macro`/`endm` with positional `\1`–`\9`, `\?` unique labels, and nested macro calls

**Independent Test**: Define `trap macro` / `jmp *` / `endm`, invoke `trap`, verify JMP to self

- [X] T032 [US5] Implement macro definition collection (`NAME macro`/`endm`) in pass 1 in Casso65Core/Assembler.cpp
- [X] T033 [US5] Implement macro invocation detection (name matches macro table) in Casso65Core/Assembler.cpp
- [X] T034 [US5] Implement argument splitting (comma-separated, respecting parentheses) and `\1`–`\9` substitution in Casso65Core/Assembler.cpp
- [X] T035 [US5] Implement `\?` unique suffix replacement with per-expansion 4-digit counter in Casso65Core/Assembler.cpp
- [X] T036 [US5] Implement nested macro expansion with 15-level depth limit in Casso65Core/Assembler.cpp
- [X] T037 [US5] Implement macro-inside-conditional and conditional-inside-macro interactions in Casso65Core/Assembler.cpp
- [X] T038 [US5] Add error reporting: macro name collision with mnemonic, exceeding depth limit in Casso65Core/Assembler.cpp
- [X] T039 [P] [US5] Create macro tests (category 4 from conformance-test-plan.md, ~30 cases including Dormann patterns) in UnitTest/MacroTests.cpp

**Checkpoint**: Basic macros working — Dormann suite macros should expand correctly

---

## Phase 7: User Story 6 + 7 + 8 + 9 — Labels, Directives, Spellings (Priority: P2)

**Goal**: Colon-less labels, `ds`/`dsb`/`.res`, AS65 directive synonyms, `align`, `end`, segments, `ERROR`

**Independent Test**: Assemble Dormann-style source with colon-less labels, `ds`, `org` (no dot), `db`

- [X] T040 [US6] Finalize colon-less label support in Parser (column-0 + not-a-mnemonic/directive/macro → label) in Casso65Core/Parser.cpp
- [X] T041 [US7] Implement `ds`/`dsb`/`.res`/`rmb` storage directives with expression count and optional fill in Casso65Core/Assembler.cpp
- [X] T042 [US8] Add AS65 directive synonym recognition: `org`, `db`/`byt`/`byte`/`fcb`/`fcc`, `dw`/`word`/`fcw`/`fdb`, `dd`, `noopt`/`opt` in Casso65Core/Parser.cpp
- [X] T042a [US8] Implement `fcb` (expressions only) and `fcc` (strings only) distinct behavior per FR-072 in Casso65Core/Assembler.cpp
- [X] T042b [US8] Implement `dd` define-double-word directive (4 bytes, little-endian) per FR-074a in Casso65Core/Assembler.cpp
- [X] T043 [US8] Implement `data`/`bss`/`code` segment switching (update current segment and PC) in Casso65Core/Assembler.cpp
- [X] T043a [US8] Implement three-segment model (code/data/bss with independent PCs; flatten to binary on output) in Casso65Core/Assembler.cpp (FR-077)
- [X] T044 [US8] Implement `end` directive (stop assembly, capture optional entry-point expression, display on stderr) in Casso65Core/Assembler.cpp
- [X] T045 [US8] Implement `align EXPR` (pad to modulus) and `align` (pad to even) in Casso65Core/Assembler.cpp
- [X] T046 [US9] Implement `ERROR` directive (fatal error when in taken block, silent in skipped block) in Casso65Core/Assembler.cpp
- [X] T047 [US7] Implement `db` string escape sequences (`\a`, `\b`, `\n`, `\r`, `\t`, `\\`, `\"`) in Casso65Core/Assembler.cpp
- [X] T048 [P] [US7] Create directive and label tests (categories 5–6 from conformance-test-plan.md, ~35 cases) in UnitTest/DirectiveTests.cpp

**Checkpoint**: All Dormann-required features complete — ready for integration test

---

## Phase 8: User Story 9 — Colon-less Labels (Priority: P1)

**Goal**: Support AS65-style colon-less labels (column-0 identifier that isn't a mnemonic/directive/macro)

**Independent Test**: Assemble Dormann-style source with colon-less labels

- [ ] T040a [US6] Implement colon-less label detection in Parser: column-0 identifier + not-a-mnemonic/directive/macro → label
- [ ] T040b [P] Add colon-less label tests to UnitTest

**Checkpoint**: Colon-less labels working — Dormann source labels recognized

---

## Phase 9: User Story 11 — Advanced Macros (Priority: P2)

**Goal**: `local`, `exitm`, `\0` argument count, named parameters

**Independent Test**: Macro with `local` label invoked twice produces unique labels; `exitm` terminates early

- [ ] T052 [US11] Implement `local` label declarations in macro definitions in Casso65Core/Assembler.cpp
- [ ] T053 [US11] Implement `exitm` early termination during macro expansion in Casso65Core/Assembler.cpp
- [ ] T054 [US11] Implement `\0` argument count substitution in Casso65Core/Assembler.cpp
- [ ] T055 [US11] Implement named parameter mapping (`NAME macro text, value` → `text`/`value` substitution including inside quoted strings) in Casso65Core/Assembler.cpp
- [ ] T056 [P] [US11] Add advanced macro tests (category 4.2–4.6 from conformance-test-plan.md: local, exitm, \0, named params) to UnitTest/MacroTests.cpp

**Checkpoint**: Full AS65 macro feature parity

---

## Phase 10: User Story 12 — File Inclusion (Priority: P2)

**Goal**: `include "filename"` splices another file into assembly

**Independent Test**: Main file includes a file with a label; main file references that label

- [ ] T057 [US12] Implement FileReader interface and default filesystem implementation in Casso65Core/AssemblerTypes.h and Casso65Core/Assembler.cpp
- [ ] T058 [US12] Implement `include "filename"` processing with nesting limit (16 levels) in Casso65Core/Assembler.cpp
- [ ] T059 [US12] Implement file:line error reporting for included files in Casso65Core/Assembler.cpp
- [ ] T060 [P] [US12] Create include tests with mock FileReader (category 8 from conformance-test-plan.md, ~5 cases) in UnitTest/IncludeTests.cpp

**Checkpoint**: Include files working — Dormann `report = 1` mode now possible

---

## Phase 11: User Story 18 — Number Format Extensions (Priority: P2)

**Goal**: `@octal`, `0x`/`0b`, `base#value` number formats

**Independent Test**: `LDA #@17` produces `$0F`; `LDA #16#FF` produces `$FF`

- [ ] T061 [US18] Add `@` octal, `0x` hex, `0b` binary prefixes to tokenizer in Casso65Core/ExpressionEvaluator.cpp
- [ ] T062 [US18] Add `<base>#<value>` arbitrary base format (2–36) to tokenizer in Casso65Core/ExpressionEvaluator.cpp
- [ ] T063 [P] [US18] Add number format tests (category 1.6 from conformance-test-plan.md, ~8 cases) to UnitTest/ExpressionEvaluatorTests.cpp

**Checkpoint**: All AS65 number formats supported

---

## Phase 12: User Story 15 — Output Formats (Priority: P2)

**Goal**: S-record and Intel HEX output writers alongside existing binary

**Independent Test**: Assemble simple program, output as S-record, verify valid S19 structure

- [ ] T064 [US15] Create OutputFormats.h with WriteBinary, WriteSRecord, WriteIntelHex declarations in Casso65Core/OutputFormats.h
- [ ] T065 [US15] Implement WriteSRecord (S0 header, S1 data, S9 start address) in Casso65Core/OutputFormats.cpp
- [ ] T066 [US15] Implement WriteIntelHex (data records, EOF record) in Casso65Core/OutputFormats.cpp
- [ ] T067 [P] [US15] Create output format tests (category 11 from conformance-test-plan.md, ~5 cases) in UnitTest/OutputFormatTests.cpp

**Checkpoint**: All three output formats working

---

## Phase 13: User Story 16 + 17 — Listing Enhancements + CLI Parity (Priority: P2)

**Goal**: Full listing controls and all AS65 CLI flags

**Independent Test**: Assemble with `-c -l -t` and verify listing has cycle counts and symbol table

- [ ] T068 [US16] Implement `list`/`nolist` cumulative enable/disable in Casso65Core/Assembler.cpp
- [ ] T068a [US16] Implement conditional-skip display in listing output (FR-038) in Casso65Core/Assembler.cpp
- [ ] T069 [US16] Implement `page`/`page EXPR` listing page breaks in Casso65Core/Assembler.cpp
- [ ] T070 [US16] Implement `title "string"` listing header in Casso65Core/Assembler.cpp
- [ ] T071 [US16] Add instruction cycle count data to OpcodeTable and `-c` listing display in Casso65Core/OpcodeTable.cpp and Casso65Core/Assembler.cpp
- [ ] T072 [US16] Implement macro expansion `>` prefix in listing output in Casso65Core/Assembler.cpp
- [ ] T072a [US16] Match AS65 listing column layout exactly: address format, byte display width, source text alignment, page header format, symbol table format (FR-037a, SC-011) in Casso65Core/Assembler.cpp
- [ ] T073 [US17] Extend AssemblerOptions with all new fields (cycleCounts, macroExpansion, pageHeight, pageWidth, predefinedSymbols, caseSensitive) in Casso65Core/AssemblerTypes.h
- [ ] T074 [US17] Implement AS65-compatible flag parsing with concatenation rules (`-tlfile` = `-t -lfile`, `-h80t` = `-h80 -t`) (FR-065c) in Casso65/CommandLine.cpp
- [ ] T074a [US17] Implement `-l` / `-l<filename>` dual behavior and `-m` macro expansion flag (FR-065a, FR-065b) in Casso65/CommandLine.cpp
- [ ] T074b [US17] Implement source file auto-extension (`.a65`, `.asm`, `.s`) and output file auto-extension (`.bin`/`.s19`/`.hex`) (FR-065d, FR-063) in Casso65/CommandLine.cpp
- [ ] T074c [US17] Implement `nul` output discard and AS65 exit codes (FR-065e, FR-065g) in Casso65/Casso65.cpp
- [ ] T074d [US17] Implement AS65-mode vs Casso65-mode detection: no subcommand keyword → AS65 mode (FR-065f) in Casso65/CommandLine.cpp
- [ ] T074e [US17] Implement `-n` disable optimizations, `-i` case-insensitive (accept as no-op), `-h<lines>` page height, `-w<width>` column width, `-z` fill with $00 (FR-060, FR-062, FR-064, FR-065, FR-078) in Casso65/CommandLine.cpp
- [ ] T074f [US17] Implement `-v` verbose mode (FR-059a) in Casso65/Casso65.cpp
- [ ] T075 [US17] Wire new CLI flags to AssemblerOptions and output format selection in Casso65/Casso65.cpp
- [ ] T076 [US17] Implement `-d<name>` symbol pre-definition in Casso65Core/Assembler.cpp
- [ ] T076a [US17] Implement default running line counter on stderr; `-q` suppresses it (FR-059) in Casso65/Casso65.cpp
- [ ] T077 [US17] Implement `-t` symbol table generation with asterisk for redefinable labels (FR-057) in Casso65Core/Assembler.cpp
- [ ] T078 [US17] Implement `-g` debug information file output in Casso65Core/Assembler.cpp
- [ ] T079 [US17] Implement `-p` pass 1 listing in Casso65Core/Assembler.cpp

**Checkpoint**: Full CLI parity with AS65

---

## Phase 14: User Story 13 + 14 + 19 — Struct, Cmap, Synonyms (Priority: P3)

**Goal**: Structure definitions, character mapping, instruction synonyms, multi-NOP

**Independent Test**: Define struct, verify offsets; `cmap` and verify remapped db output; `disable` emits SEI

- [ ] T080 [US13] Implement `struct NAME`/`end struct` with db/dw/ds/label/align member definitions in Casso65Core/Assembler.cpp
- [ ] T081 [US13] Implement struct starting offset (`struct NAME, EXPR`) in Casso65Core/Assembler.cpp
- [ ] T082 [P] [US13] Create struct tests in UnitTest/StructTests.cpp
- [ ] T083 [US14] Implement `cmap` directive (identity reset, force value, range mapping) in Casso65Core/Assembler.cpp
- [ ] T084 [US14] Apply character map to db/fcb/fcc string emission in Casso65Core/Assembler.cpp
- [ ] T085 [P] [US14] Create cmap tests in UnitTest/CmapTests.cpp
- [ ] T086 [US19] Add instruction synonyms to OpcodeTable (disable=SEI, enable=CLI, stc=SEC, sti=SEI, std=SED) in Casso65Core/OpcodeTable.cpp
- [ ] T087 [US19] Implement `nop EXPR` multi-NOP emission in Casso65Core/Assembler.cpp
- [ ] T088 [US8] Implement `set` keyword as synonym for `=` in Casso65Core/Parser.cpp

**Checkpoint**: All AS65 features implemented

---

## Phase 15: Verification & Polish — Dormann EXIT GATE + Cross-Cutting

**Purpose**: End-to-end Dormann validation, conformance testing, final polish

- [ ] T004a [P] Create conformance test input files (.a65) with hand-computed expected outputs (.expected.bin) per conformance-test-plan.md categories 1–14 in specs/002-as65-assembler-compat/testdata/conformance/ (our own work, committed)
- [ ] T004b [P] Create scripts/RunDormannTest.ps1 — downloads 6502_functional_test.a65 and reference binary from Klaus2m5 bin_files/, assembles with Casso65, compares against reference, deletes all downloaded files on completion
- [ ] T004c [P] Add downloaded temp files to .gitignore
- [ ] T049 [US10] Attempt assembly of 6502_functional_test.a65 (downloaded on demand) and iteratively fix any remaining issues in Casso65Core/Assembler.cpp
- [ ] T050 [US10] Create Dormann integration test using scripts/RunDormannTest.ps1: download source + reference binary, assemble, compare, delete in UnitTest/DormannIntegrationTests.cpp (NOTE: gated behind TEST_CATEGORY("Integration") — excluded from normal unit test runs per constitution principle II)
- [ ] T051 [US10] Verify assembled binary runs in CPU emulator and reaches success trap — informational only, emulator bugs tracked as separate GitHub issues
- [ ] T089 Verify AS65 testcase.a65 assembles without errors (SC-008) in UnitTest/DormannIntegrationTests.cpp
- [ ] T089a Verify listing output matches AS65 format per as65.man documentation (SC-011) in UnitTest/DormannIntegrationTests.cpp
- [ ] T089b [P] Create ConformanceTests.cpp: data-driven test runner that loops over all testdata/conformance/*.a65 files, assembles each, and compares output against *.expected.bin (FR-079, SC-012) in UnitTest/ConformanceTests.cpp
- [ ] T089c Run full conformance suite — all ~200 test cases pass against hand-computed expected outputs
- [ ] T090 Re-verify Dormann suite assembly after all additions (regression check)
- [ ] T091 Run full test suite: Build + Test Debug (current arch) — all tests pass
- [ ] T092 Run quickstart.md validation steps
- [ ] T093 Code cleanup: verify all new files follow formatting rules (5 blank lines, 80-char comment blocks, Pch.h first)

**Checkpoint**: EXIT GATE — Dormann binary byte-identical to reference + all success criteria (SC-001 through SC-012) verified

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 — BLOCKS all user stories
- **Phase 3 (US1 Expressions)**: Depends on Phase 2
- **Phase 4 (US2+US3 Constants/PC)**: Depends on Phase 3
- **Phase 5 (US4 Conditionals)**: Depends on Phase 4
- **Phase 6 (US5 Macros)**: Depends on Phase 5
- **Phase 7 (US6+7+8+9 Labels/Directives/Segments)**: Depends on Phase 4
- **Phase 8 (Colon-less Labels)**: Depends on Phase 7
- **Phase 9 (US11 Advanced Macros)**: Depends on Phase 6
- **Phase 10 (US12 Include)**: Depends on Phase 2
- **Phase 11 (US18 Number Formats)**: Depends on Phase 2
- **Phase 12 (US15 Output Formats)**: Depends on Phase 2
- **Phase 13 (US16+17 Listing/CLI)**: Depends on Phase 2
- **Phase 14 (US13+14+19 Struct/Cmap/Synonyms)**: Depends on Phase 2
- **Phase 15 (Verification & Polish)**: Depends on ALL previous phases — EXIT GATE

### Parallel Opportunities

After Phase 2 is complete:
- Phases 10, 11, 12, 13, 14 can all be done in parallel (independent features)
- Phases 3→4→5→6→7→8 must be sequential (each builds on the previous)
- Phase 9 can start after Phase 6

### Implementation Strategy

**Core Features**: Phases 1–8 (Setup through Colon-less Labels)
- Delivers: Expression evaluator, constants, current-PC, conditionals, macros, labels, directives
- All Dormann-required assembler features

**Full AS65 Parity**: Phases 9–14
- Adds: Advanced macros, includes, number formats, output formats, listing, CLI, structs, cmap, synonyms
- Can be delivered incrementally after MVP
