---

description: "Task list for 019-assembler-dialects"
---

# Tasks: Merlin Assembler Dialect

**Input**: Design documents from `/specs/019-assembler-dialects/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/)

**Tests**: Test tasks ARE included. Constitution Principle II requires unit tests for all production code, and the spec's own success criteria (SC-001, SC-009) are stated as tests.

**Organization**: Grouped by user story so each is independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story the task belongs to (US1, US2, US3)
- Exact file paths are included in every task

## Path Conventions

Paths are repository-relative and follow the structure in [plan.md](./plan.md):
`CassoCore/` (all new logic), `CassoCli/` (formatting edge only), `UnitTest/`,
`scripts/`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Test scaffolding that later phases fill in.

- [ ] T001 [P] Promote `MockFileReader` out of `UnitTest/IncludeTests.cpp` into `UnitTest/MockFileReader.h`, register in `UnitTest.vcxproj`, and update `UnitTest/IncludeTests.cpp` to include it instead of defining it
- [ ] T002 [P] Create `UnitTest/MerlinCorpus/README.md` documenting the capture procedure, the Merlin-version-per-entry rule, and why the disk image is never committed
- [ ] T003 [P] Create `scripts/CaptureMerlinCorpus.ps1` skeleton with `-Entry` and `-MerlinImage` parameters and usage text

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The dialect seam, diagnostic positions, and the switchable instruction set. Nothing story-specific can begin until these land.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### Seam extraction — must change no behavior

- [ ] T004 Create `CassoCore/Dialect.h` with the `DialectId` enum (`As65`, `Merlin`) and register it in `CassoCore.vcxproj`
- [ ] T005 Create `CassoCore/DialectProfile.h` declaring the abstract seam — data members per [data-model.md](./data-model.md) plus the four virtual hooks — and register it in `CassoCore.vcxproj`
- [ ] T006 Create `CassoCore/As65Dialect.h` / `CassoCore/As65Dialect.cpp` holding today's grammar moved verbatim from `Parser::ParseLine`, and register both in `CassoCore.vcxproj`
- [ ] T007 Create `CassoCore/DialectRegistry.h` / `CassoCore/DialectRegistry.cpp` with the name-to-profile table and a `GetAllDialects()` accessor matching the `DirectiveTable::GetAllSpellings` pattern, and register both in `CassoCore.vcxproj`
- [ ] T008 Route `Parser::ParseLine` through the active profile in `CassoCore/Parser.cpp` and `CassoCore/Parser.h`, moving the file-scope `StripComments` helper into the profile
- [ ] T009 Add `dialect` to `AssemblerOptions` in `CassoCore/AssemblerTypes.h`, defaulting to `DialectId::As65` so every existing caller is unaffected
- [ ] T010 Verify the seam changed nothing: full suite green in `x64\Release` AND `git diff --stat origin/master -- UnitTest/` shows no test modifications. A test edit here means behavior moved with the code — stop and find out what
- [ ] T011 [P] Add `DialectRegistry` sweep tests to `UnitTest/DialectMechanismTests.cpp` asserting every `DialectId` enumerator resolves to a profile, and register the file in `UnitTest.vcxproj`

### Diagnostic positions

- [ ] T012 Add `file` (default empty) and `column` (default 0) to `AssemblyError` in `CassoCore/AssemblerTypes.h`
- [ ] T013 Route `RecordError` and `RecordWarning` through the current `PendingLine`'s `sourceFile` in `CassoCore/AssemblySession.cpp`
- [ ] T014 Make `ReportAssemblyDiagnostics` in `CassoCli/CommandLine.cpp` print the error's own `file` when set, falling back to the input path when empty so AS65 diagnostics are byte-for-byte unchanged
- [ ] T015 [P] Add tests to `UnitTest/MerlinDiagnosticTests.cpp` proving a diagnostic raised inside an included file names that file rather than the top-level input, and register the file in `UnitTest.vcxproj`

### Switchable instruction set

- [ ] T016 Create `CassoCore/InstructionSetProvider.h` / `.cpp` holding both the 6502 and 65C02 `OpcodeTable`s with an active selection, and register both in `CassoCore.vcxproj`
- [ ] T017 Change `AssemblySession`'s `const OpcodeTable & m_opcodeTable` to a re-seatable pointer in `CassoCore/AssemblySession.h`, keeping `Assembler`'s existing single-`Microcode` constructor working
- [ ] T018 Record the active instruction table **per line** during pass 1 and replay it in pass 2 in `CassoCore/AssemblySession.cpp` — never recompute, because conditional assembly can move where the directive is reached
- [ ] T019 Verify SC-004: full suite green, with `UnitTest/AssemblerTests.cpp`, `RegressionTests.cpp`, `IntegrationTests.cpp`, and `OutputFormatTests.cpp` confirming AS65 output bytes are unchanged

**Checkpoint**: Seam in place, diagnostics carry position, both instruction tables held. User story work can begin.

---

## Phase 3: User Story 1 - Assemble existing Merlin source unmodified (Priority: P1) 🎯 MVP

**Goal**: A developer points Casso at unmodified Merlin source and gets the bytes Merlin produces.

**Independent test**: `UnitTest/MerlinCorpusTests.cpp` assembles every corpus entry and compares byte-for-byte against bytes captured from real Merlin 8. Nothing reads a file or invokes another assembler.

### Corpus first — these settle open questions the parser depends on

- [ ] T020 [US1] Define the `CorpusEntry` shape from [data-model.md](./data-model.md) and the comparison harness in `UnitTest/MerlinCorpusTests.cpp`, serving multi-source entries through `UnitTest/MockFileReader.h`, and register the file in `UnitTest.vcxproj`
- [ ] T021 [US1] Implement `scripts/CaptureMerlinCorpus.ps1` to assemble one entry under real Merlin 8 in Casso and emit source, bytes, and Merlin version into `UnitTest/MerlinCorpus/CorpusEntries.h`
- [ ] T022 [P] [US1] Capture irregular-spacing entries — extra spaces, tabs, and mixtures — into `UnitTest/MerlinCorpus/CorpusEntries.h` to settle the field-based line model empirically
- [ ] T023 [P] [US1] Capture mixed-case and long-symbol entries into `UnitTest/MerlinCorpus/CorpusEntries.h` to settle symbol case sensitivity, length limit, and legal character set (research.md CHK008, CHK009)
- [ ] T024 [P] [US1] Capture expression entries covering Merlin's operator set, precedence, and the current-program-counter form into `UnitTest/MerlinCorpus/CorpusEntries.h` (research.md CHK052)
- [ ] T025 [P] [US1] Capture a `XC OFF` entry into `UnitTest/MerlinCorpus/CorpusEntries.h` to settle whether Merlin accepts a reset form (spec Edge Cases)
- [ ] T026 [US1] Cross-check a sample of captured entries against hand-derived expectations from the Merlin manual, and record the answers to all settle-by-capture items in `specs/019-assembler-dialects/research.md`

### The Merlin profile

- [ ] T027 [US1] Create `CassoCore/MerlinDialect.h` / `.cpp` as a `DialectProfile` subclass, register it in `CassoCore/DialectRegistry.cpp` and `CassoCore.vcxproj`
- [ ] T028 [US1] Implement Merlin comment conventions in `CassoCore/MerlinDialect.cpp` — asterisk in column 1 for a whole-line comment, semicolon at any field position (FR-007)
- [ ] T029 [US1] Implement field-based line segmentation in `CassoCore/MerlinDialect.cpp` — whitespace runs separate label, opcode, operand, and comment; tabs are ordinary whitespace with no tab-stop expansion; no field is required at a specific column (FR-008)
- [ ] T030 [US1] Implement label rules and the local-label prefix in `CassoCore/MerlinDialect.cpp`, scoping locals to the enclosing global label (FR-008)
- [ ] T031 [US1] Implement variable symbols in `CassoCore/MerlinDialect.cpp` with reassignment semantics (FR-011)
- [ ] T032 [US1] Add the Merlin directive spelling table to `CassoCore/MerlinDialect.cpp`, reusing existing `Directive` tokens wherever the operation is identical
- [ ] T033 [P] [US1] Add new `Directive` tokens for reversed-order words and raw hexadecimal data in `CassoCore/Directive.h` and `CassoCore/Directive.cpp`, and their pass-1/pass-2 rows in `CassoCore/AssemblySession.cpp` (FR-009)
- [ ] T034 [P] [US1] Create `CassoCore/StringEncoding.h` / `.cpp` implementing high-bit, inverse, flashing, and terminator handling per [contracts/merlin-directives.md](./contracts/merlin-directives.md), and register both in `CassoCore.vcxproj`
- [ ] T035 [US1] Wire the five Merlin string spellings to one `Directive` token carrying a `StringEncodingMode` in `CassoCore/MerlinDialect.cpp`, including delimiter-driven high-bit inference (FR-010)
- [ ] T036 [P] [US1] Implement the loop construct and its terminator in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp` (FR-011)
- [ ] T037 [P] [US1] Implement dummy sections and their terminator in `CassoCore/AssemblySession.cpp` — assign addresses, emit no bytes (FR-012)
- [ ] T038 [US1] Implement Merlin macro definition, positional parameters, and invocation syntax in `CassoCore/MerlinDialect.cpp`, reusing the existing `kMaxMacroDepth` limit (FR-013)
- [ ] T039 [US1] Map Merlin's file-inclusion directives to `Directive::Include` in `CassoCore/MerlinDialect.cpp`, resolving relative to the including source and reusing the existing `kMaxIncludeDepth` limit (FR-014)
- [ ] T040 [US1] Implement the first occurrence of the CPU-selection directive in `CassoCore/MerlinDialect.cpp`, switching `InstructionSetProvider` to the extended table for the remainder of the assembly (FR-003, FR-015)
- [ ] T041 [US1] Implement the object-file directive as naming the output in `CassoCore/MerlinDialect.cpp`, with the command line taking precedence over it (FR-027)
- [ ] T042 [US1] Add diagnostics for unterminated dummy sections, loops, and macros at end of file in `CassoCore/AssemblySession.cpp`, naming the construct and its opening line as the existing unclosed-conditional diagnostic does (research.md CHK041)

### Corpus completion

- [ ] T043 [P] [US1] Capture one entry per string-encoding spelling into `UnitTest/MerlinCorpus/CorpusEntries.h` — five entries, not one, because a high-bit or terminator error still looks plausible
- [ ] T044 [P] [US1] Capture at least one multi-file inclusion entry into `UnitTest/MerlinCorpus/CorpusEntries.h`, served through `UnitTest/MockFileReader.h`
- [ ] T045 [P] [US1] Capture one entry per remaining construct in FR-007 through FR-015 and FR-027 into `UnitTest/MerlinCorpus/CorpusEntries.h`, completing the corpus floor
- [ ] T046 [US1] Verify SC-001: every corpus entry assembles byte-identically via `UnitTest/MerlinCorpusTests.cpp`
- [ ] T047 [P] [US1] Add focused parser tests to `UnitTest/MerlinParserTests.cpp` and directive tests to `UnitTest/MerlinDirectiveTests.cpp`, registering both in `UnitTest.vcxproj`

**Checkpoint**: Unmodified Merlin source assembles to Merlin's bytes. This is the MVP.

---

## Phase 4: User Story 2 - Choose a dialect explicitly (Priority: P1)

**Goal**: A developer states the dialect and gets exactly that dialect's rules, strictly.

**Independent test**: One source file assembled under each dialect selection; constructs valid in one and invalid in the other are accepted and rejected accordingly.

**⚠️ Shared file warning**: `CassoCore/CommandLineParser.cpp` and `UnitTest/CommandLineTests.cpp` are shared with the concurrently developed spec 020. Changes must be purely additive — one row, one enumerator, one arm, one flag parser. Do not restructure the dispatcher; if that seems necessary, stop and raise it.

- [ ] T048 [US2] Add `Subcommand::Merlin` and a `dialect` field to `CommandLineOptions` in `CassoCore/CommandLineOptions.h`
- [ ] T049 [US2] Add one row `{ "merlin", CommandLineOptions::Subcommand::Merlin }` to `s_kSubcommands` and one arm in `Parse` in `CassoCore/CommandLineParser.cpp`, leaving the unrecognized-first-argument AS65 fallback intact
- [ ] T050 [US2] Add `ParseMerlinFlags` to `CassoCore/CommandLineParser.h` and `CassoCore/CommandLineParser.cpp` per [contracts/cli.md](./contracts/cli.md)
- [ ] T051 [US2] Refuse `--cpu` under the merlin subcommand in `CassoCore/CommandLineParser.cpp` with a message naming the in-source CPU directive — refused, never accepted and ignored (FR-026)
- [ ] T052 [US2] Set `AssemblerOptions::dialect` from `CommandLineOptions::dialect` in `CassoCli/CommandLine.cpp`, and set it to AS65 as an inference on the fallback path
- [ ] T053 [US2] Report an inferred dialect on stderr under verbose and in the listing header in `CassoCli/CommandLine.cpp` — never unconditionally on stdout, which carries the listing when no listing file is named (FR-004)
- [ ] T054 [P] [US2] Add merlin grammar tests to a **new** `UnitTest/MerlinCommandLineTests.cpp` rather than editing `UnitTest/CommandLineTests.cpp`, and register it in `UnitTest.vcxproj`
- [ ] T055 [US2] Verify `UnitTest/CommandLineTests.cpp` passes with zero modifications, confirming spec 020's pinned behavior is intact

**Checkpoint**: Dialect selection is explicit, strict, and additive to the shared command-line surface.

---

## Phase 5: User Story 3 - Diagnostics that speak the developer's dialect (Priority: P2)

**Goal**: Failures name constructs in the selected dialect's vocabulary, at the right line and column, and subset-boundary refusals are unmistakable.

**Independent test**: A known error introduced per dialect produces a diagnostic naming the right construct at the right position; every boundary construct produces a named refusal rather than a parse error.

- [ ] T056 [US3] Create `CassoCore/MerlinSubsetBoundary.h` / `.cpp` with one row per refused construct carrying spelling, reason class, explanation, and what widens it, exposed by a `GetAll`-style accessor; register both in `CassoCore.vcxproj` (FR-019)
- [ ] T057 [US3] Refuse relocatable-mode assembly and entry and external symbol declarations in `CassoCore/MerlinDialect.cpp` via the boundary table, naming each construct (FR-016)
- [ ] T058 [US3] Refuse the second occurrence of the CPU-selection directive in `CassoCore/MerlinDialect.cpp` as selecting an unemulated CPU (FR-016)
- [ ] T059 [P] [US3] Refuse the file-type directive in `CassoCore/MerlinDialect.cpp` as owned by `020-disk-file-access` (FR-028)
- [ ] T060 [P] [US3] Refuse the save-object directive in `CassoCore/MerlinDialect.cpp` as multi-output segmentation needing its own decision — the message must NOT describe it as waiting on 020 (FR-029)
- [ ] T061 [US3] Make a boundary refusal distinguishable from a syntax error in `CassoCore/AssemblySession.cpp`, and collect every offender across the whole pass before failing rather than stopping at the first (FR-017, FR-018)
- [ ] T062 [US3] Generate the subset-boundary section of `merlin --help` from the boundary table in `CassoCli/CommandLine.cpp` so help and implementation cannot disagree (FR-019, FR-024)
- [ ] T063 [US3] Populate `column` on every Merlin diagnostic in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp` (FR-021)
- [ ] T064 [US3] Describe constructs in the active dialect's vocabulary, and name which dialect defines a construct rejected as belonging to another, in `CassoCore/AssemblySession.cpp` (FR-020, FR-022)
- [ ] T065 [US3] Explain the column rule when a Merlin label is indented, rather than reporting an unknown symbol, in `CassoCore/MerlinDialect.cpp` (User Story 3 acceptance 1)
- [ ] T066 [P] [US3] Add the hand-authored negative corpus class — boundary refusals and diagnostic expectations, kept distinct from captured entries — to `UnitTest/MerlinCorpus/CorpusEntries.h`
- [ ] T067 [P] [US3] Add `UnitTest/MerlinSubsetBoundaryTests.cpp` sweeping the boundary accessor and asserting every row produces the expected refusal, and register it in `UnitTest.vcxproj`
- [ ] T068 [US3] Verify SC-006 and SC-007: every dialect-specific diagnostic identifies the correct line and column, and every out-of-subset construct is named rather than failing as a parse error

**Checkpoint**: Diagnostics are dialect-native, positioned, and boundary refusals are unmistakable.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [ ] T069 Add a synthetic, test-only third dialect profile to `UnitTest/DialectMechanismTests.cpp` and prove it works end to end — this is what catches a mechanism secretly built for exactly two dialects (SC-009)
- [ ] T070 Verify SC-009: `git diff --stat origin/master -- CassoCore/AssemblySession.cpp CassoCore/ExpressionEvaluator.cpp CassoCore/OpcodeTable.cpp` shows the synthetic profile required no change to the engine, evaluator, or opcode tables
- [ ] T071 [P] Update `CHANGELOG.md` with the merlin subcommand, the dialect mechanism, and the corrected include-file diagnostic attribution
- [ ] T072 [P] Update `README.md` with the new dialect, the updated test count, and the roadmap position relative to `023-ca65-dialect`
- [ ] T073 [P] Document the supported subset and where it ends in the repository docs, deriving the list from `CassoCore/MerlinSubsetBoundary.cpp` (SC-008)
- [ ] T074 Run `scripts/RunDormannTest.ps1` — required for assembler changes
- [ ] T075 Run `scripts/RunHarteTests.ps1 -SkipGenerate` — required for assembler changes
- [ ] T076 Run `scripts/Build.ps1 -RunCodeAnalysis` and `scripts/CheckStyle.ps1`, and confirm x64 Debug and Release are both green
- [ ] T077 Revert `.specify/feature.json` to `specs/020-disk-file-access` before merging, so master does not thrash between two concurrent specs

---

## Dependencies

**Phase order**: Setup → Foundational → US1 → US2 → US3 → Polish.

**Hard blocks**:

- T010 gates everything. If the seam changed behavior, no later phase's evidence is trustworthy.
- T016–T018 block T040 — the CPU-selection directive has nothing to switch without the provider.
- T020–T026 block T027 onward. The corpus settles six open questions, and building the parser first means guessing at answers the capture can supply.
- T056 blocks T057–T062. The boundary table is the single source those all derive from.
- T069 depends on the whole mechanism, so it lands last despite being the criterion 023 gates on.

**Story independence**: US2 and US3 both depend on US1's profile existing, so they are not parallel with it. They are independent of each other and can proceed in either order once US1 lands.

## Parallel Execution Examples

**Setup** — all three are independent files:

```
T001, T002, T003
```

**Corpus capture within US1** — each writes a distinct entry:

```
T022, T023, T024, T025      # the settle-by-capture entries
T043, T044, T045            # the corpus floor completion
```

**Independent implementation within US1**:

```
T033 (new Directive tokens)   +  T034 (StringEncoding)
T036 (loop construct)         +  T037 (dummy sections)
```

**Within US3** — distinct refusals and distinct test files:

```
T059, T060                  # file-type and save-object refusals
T066, T067                  # negative corpus and boundary sweep tests
```

**Polish** — documentation is independent of validation:

```
T071, T072, T073
```

## Implementation Strategy

**MVP is Phase 1 through Phase 3.** At that point unmodified Merlin source
assembles to Merlin's bytes, which the spec calls "the entire feature." US2 and
US3 are refinements on top — valuable, but a developer can already stop porting
their codebase.

**Commit per phase**, per the constitution's commit discipline. Do not accumulate
phases into one commit.

**Validate cheaply on the branch**: compile the touched project and run the
narrowest relevant tests per commit; Release runs the suite in roughly 2 minutes
against Debug's 15. Defer the full gate — both extended suites, code analysis,
and style — to the pre-merge check in Phase 6.

**Merge with `--no-ff`.** CONTRIBUTING forbids squashing.

**One deferral is deliberate and already recorded**: the "unrecognized first
argument falls back to AS65" heuristic stays. Issue #92 wants it gone, but it
breaks the documented `CassoCli input.a65 -o out.bin` form and needs its own
decision plus a CHANGELOG entry. The reasoning is on issue #92; do not remove it
as part of this feature.
