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

Tasks added after generation sit in their **execution** position rather than at
the end, so the file still reads in the order the work happens. They carry either
the next free number (T078, T079) or a letter suffix where they belong beside an
existing task (T033a, T053a, T062a). Existing IDs are never renumbered, because
the phase notes and the dependency graph reference them.

## Path Conventions

Paths are repository-relative and follow the structure in [plan.md](./plan.md):
`CassoCore/` (all new logic), `CassoCli/` (formatting edge only), `UnitTest/`,
`scripts/`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Test scaffolding that later phases fill in.

- [ ] T001 [P] Promote `MockFileReader` out of `UnitTest/IncludeTests.cpp` into `UnitTest/MockFileReader.h`, register in `UnitTest.vcxproj`, and update `UnitTest/IncludeTests.cpp` to include it instead of defining it
- [x] T002a Create `scripts/ExtractDos33File.ps1` — catalog walk and file extraction from a flat DOS-order image, stripping the DOS BIN header. **Throwaway capture tooling, not a product feature**: it exists only because the Merlin disk happens to be flat DOS-order, and it does not duplicate `020-disk-file-access`'s `disk get`, which is tested C++ spanning every mountable format including WOZ. Delete it if 020's extraction lands first. *(Validated against the DOS 3.3 System Master: FID extracts at load `$0803`, CHAIN at `$0208` / 453 bytes with the stripped payload matching the raw sectors after the 4-byte header.)*
- [ ] T002 [P] Create `UnitTest/MerlinCorpus/README.md` documenting the capture procedure end to end: the developer supplies their own Merlin 8 image, source goes in by typing or pasting into Merlin's editor, and bytes come back out via `scripts/ExtractDos33File.ps1`. Record the Merlin-version-per-entry rule and why the disk image is never committed
- [ ] T003 [P] Create `scripts/CaptureMerlinCorpus.ps1` skeleton with `-Entry` and `-MerlinImage` parameters and usage text, calling `ExtractDos33File.ps1` for the read-back half

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The dialect seam, diagnostic positions, and the switchable instruction set. Nothing story-specific can begin until these land.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### Seam extraction — must change no behavior

- [ ] T004 Create `CassoCore/Dialect.h` with the `DialectId` enum (`As65`, `Merlin`) and register it in `CassoCore.vcxproj`
- [ ] T005 Create `CassoCore/DialectProfile.h` declaring the abstract seam — data members per [data-model.md](./data-model.md) plus the four virtual hooks — and register it in `CassoCore.vcxproj`
- [ ] T006 Create `CassoCore/As65Dialect.h` / `CassoCore/As65Dialect.cpp` holding today's grammar moved verbatim from `Parser::ParseLine`, and register both in `CassoCore.vcxproj`. **The AS65 directive spelling table does not move**: `DirectiveTable` keeps its global table and `GetAllSpellings()` accessor, and the profile delegates to them. Moving it would change `UnitTest/DirectiveTokenTests.cpp:70`, which sweeps that accessor — and T010 forbids exactly that
- [ ] T007 Create `CassoCore/DialectRegistry.h` / `CassoCore/DialectRegistry.cpp` with the name-to-profile table and a `GetAllDialects()` accessor matching the `DirectiveTable::GetAllSpellings` pattern, and register both in `CassoCore.vcxproj`
- [ ] T008 Route `Parser::ParseLine` through the active profile in `CassoCore/Parser.cpp` and `CassoCore/Parser.h`, moving the file-scope `StripComments` helper into the profile
- [ ] T009 Add `dialect` to `AssemblerOptions` in `CassoCore/AssemblerTypes.h`, defaulting to `DialectId::As65` so every existing caller is unaffected
- [ ] T010 Verify the seam changed nothing, **before any new test file is added**: full suite green in `x64\Release` AND `git diff --stat origin/master -- UnitTest/` shows no *existing* test file modified. Adding new files is expected later and does not violate this gate; editing one that already existed does, and means behavior moved with the code — stop and find out what
- [ ] T011 [P] Add `DialectRegistry` sweep tests to `UnitTest/DialectMechanismTests.cpp` asserting every `DialectId` enumerator resolves to a profile, and register the file in `UnitTest.vcxproj`. Runs **after** T010, since it adds a file under `UnitTest/`

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
- [ ] T021 [US1] Implement `scripts/CaptureMerlinCorpus.ps1` to assemble one entry under real Merlin 8 in Casso and emit source, bytes, and Merlin version into `UnitTest/MerlinCorpus/CorpusEntries.h`, reading bytes back with `scripts/ExtractDos33File.ps1`
- [ ] T021a [US1] Verify the source round trip before trusting any captured bytes: save the pasted source to the Merlin disk from within Merlin, extract it back with `ExtractDos33File.ps1`, and compare against what was intended. Issue #110 reports the guest paste path garbling input, so paste is **suspect until proven clean per entry** — the answer is to verify the loop, not to avoid pasting
- [ ] T021b [US1] Batch constructs into a few **composite** source files rather than one file per construct: assemble once with the listing on, save the object, extract, and split by known offsets. A handful of composites covers the FR-007..FR-015 floor at a fraction of the typing
- [ ] T022 [P] [US1] Capture irregular-spacing entries — extra spaces, tabs, and mixtures — into `UnitTest/MerlinCorpus/CorpusEntries.h` to settle the field-based line model empirically
- [ ] T023 [P] [US1] Capture mixed-case and long-symbol entries into `UnitTest/MerlinCorpus/CorpusEntries.h` to settle symbol case sensitivity, length limit, and legal character set (research.md CHK008, CHK009)
- [ ] T024 [P] [US1] Capture expression entries covering Merlin's operator set, precedence, and the current-program-counter form into `UnitTest/MerlinCorpus/CorpusEntries.h` (research.md CHK052)
- [ ] T025 [P] [US1] Capture a `XC OFF` entry into `UnitTest/MerlinCorpus/CorpusEntries.h` to settle whether Merlin accepts a reset form (spec Edge Cases)
- [ ] T026 [US1] Cross-check a sample of captured entries against hand-derived expectations from the Merlin manual, and record the answers to all settle-by-capture items in `specs/019-assembler-dialects/research.md`. **If T025 shows a CPU-target reset form exists**, amend FR-015 and the `InstructionSetProvider` state transition in `data-model.md` — both currently describe a one-way `base → extended` change — and add the implementing task before T040 rather than discovering the conflict during it

### The Merlin profile

- [ ] T027 [US1] Create `CassoCore/MerlinDialect.h` / `.cpp` as a `DialectProfile` subclass, register it in `CassoCore/DialectRegistry.cpp` and `CassoCore.vcxproj`
- [ ] T028 [US1] Implement Merlin comment conventions in `CassoCore/MerlinDialect.cpp` — asterisk in column 1 for a whole-line comment, semicolon at any field position (FR-007)
- [ ] T029 [US1] Implement field-based line segmentation in `CassoCore/MerlinDialect.cpp` — whitespace runs separate label, opcode, operand, and comment; tabs are ordinary whitespace with no tab-stop expansion; no field is required at a specific column (FR-008)
- [ ] T030 [US1] Implement label rules and the local-label prefix in `CassoCore/MerlinDialect.cpp`, scoping locals to the enclosing global label (FR-008)
- [ ] T031 [US1] Implement variable symbols in `CassoCore/MerlinDialect.cpp` with reassignment semantics (FR-011)
- [ ] T032 [US1] Add the Merlin directive spelling table to `CassoCore/MerlinDialect.cpp`, reusing existing `Directive` tokens wherever the operation is identical
- [ ] T033 [P] [US1] Add **all** new `Directive` tokens this feature introduces in `CassoCore/Directive.h` and `CassoCore/Directive.cpp`, with their pass-1/pass-2 rows in `CassoCore/AssemblySession.cpp`: reversed-order words, raw hexadecimal data, the loop construct and its terminator, the dummy section and its terminator, CPU selection, and the single encoded-string token. Adding them in one task keeps the exhaustiveness-checked `switch` compiling once rather than breaking at each of T035–T040 (research.md D2, FR-009)
- [ ] T033a [US1] Resolve directive spellings that collide with an instruction mnemonic by the active dialect's rule in `CassoCore/MerlinDialect.cpp`, using the `DirectiveTable::FromAmbiguousSpelling` precedent so resolution never depends on which table is consulted first (spec Edge Cases)
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
- [ ] T051 [US2] Refuse `--cpu` **when the active profile's `cpuSource` is in-source**, driven by profile data rather than by a merlin-specific branch, in `CassoCore/CommandLineParser.cpp`; the message names the directive supplied by the profile. A hard-coded merlin arm here would put a per-dialect branch in the shared mechanism, which is what `contracts/dialect-profile.md` guarantee 3 forbids and what SC-009 exists to catch (FR-026)
- [ ] T052 [US2] Set `AssemblerOptions::dialect` from `CommandLineOptions::dialect` in `CassoCli/CommandLine.cpp`, and set it to AS65 as an inference on the fallback path
- [ ] T053 [US2] Create `CassoCore/DialectReporting.h` / `.cpp` deciding **what** dialect-and-CPU line to emit and **when**, per the reporting table in [contracts/cli.md](./contracts/cli.md), and register both in `CassoCore.vcxproj`. `CassoCli/CommandLine.cpp` only prints what it returns — never unconditionally on stdout, which carries the listing when no listing file is named. The decision lives in core so `UnitTest` can exercise it (FR-004, SC-005)
- [ ] T053a [P] [US2] Report the **CPU target** alongside the dialect through the same path — including when it was left at the dialect's default, so "no directive was seen" is not misread as "the flag was ignored" — in `CassoCore/DialectReporting.cpp` (SC-005)
- [ ] T053b [P] [US2] Register the `merlin` subcommand and its flag table in the tool's usage and help output via `CassoCli/CommandLine.h` and `CassoCli/CommandLine.cpp`, deriving the flag list from core so help cannot drift from the parser, with a test (FR-024, US2 acceptance 4)
- [ ] T054 [P] [US2] Add merlin grammar tests to a **new** `UnitTest/MerlinCommandLineTests.cpp` rather than editing `UnitTest/CommandLineTests.cpp`, and register it in `UnitTest.vcxproj`
- [ ] T055 [US2] Verify `UnitTest/CommandLineTests.cpp` passes with zero modifications, confirming spec 020's pinned behavior is intact
- [ ] T078 [US2] Create `CassoCore/AssemblerExitCode.h` / `.cpp` mapping an `AssemblyResult` to the shared vocabulary — 0 clean, 1 succeeded with complaints, 2 no output — and register both in `CassoCore.vcxproj`; `CassoCli/CommandLine.cpp` returns what it computes. A subset-boundary refusal maps to 2 and is distinguished by its message, not by a distinct code. In core so the mapping is unit-testable rather than reachable only by running the exe (FR-030)
- [ ] T079 [P] [US2] Add a cross-dialect strictness test to `UnitTest/MerlinCommandLineTests.cpp`: a Merlin-only construct assembled under AS65 is rejected naming the construct and the active dialect, and an AS65-only construct under Merlin likewise. This is US2's independent test and FR-005's direct evidence — no other task exercises the accept/reject matrix

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
- [ ] T062 [US3] Generate the subset-boundary help text **from the boundary table inside `CassoCore/MerlinSubsetBoundary.cpp`**, returning a string that `CassoCli/CommandLine.cpp` merely prints. Generation in the executable would be unreachable from `UnitTest`, so FR-019's "cannot disagree by construction" would gain no test — and Principle VI is non-negotiable (FR-019, FR-024)
- [ ] T062a [P] [US3] Add a test to `UnitTest/MerlinSubsetBoundaryTests.cpp` asserting the generated help text names every row the accessor returns, so a row added to the table without help coverage fails the build rather than shipping
- [ ] T063 [US3] Populate `column` on every Merlin diagnostic in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp` (FR-021)
- [ ] T064 [US3] Describe constructs in the active dialect's vocabulary, and name which dialect defines a construct rejected as belonging to another, in `CassoCore/AssemblySession.cpp` (FR-020, FR-022)
- [ ] T065 [US3] Explain the column rule when a Merlin label is indented, rather than reporting an unknown symbol, in `CassoCore/MerlinDialect.cpp` (User Story 3 acceptance 1)
- [ ] T066 [P] [US3] Add the hand-authored negative corpus class — boundary refusals and diagnostic expectations, kept distinct from captured entries — to `UnitTest/MerlinCorpus/CorpusEntries.h`, including an entry where a macro is invoked with another dialect's argument syntax and must be **rejected rather than partially expanded** (spec Edge Cases)
- [ ] T067 [P] [US3] Add `UnitTest/MerlinSubsetBoundaryTests.cpp` sweeping the boundary accessor and asserting every row produces the expected refusal, and register it in `UnitTest.vcxproj`
- [ ] T068 [US3] Verify SC-006 and SC-007: every dialect-specific diagnostic identifies the correct line and column, and every out-of-subset construct is named rather than failing as a parse error

**Checkpoint**: Diagnostics are dialect-native, positioned, and boundary refusals are unmistakable.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [ ] T069 Add a synthetic, test-only third dialect profile to `UnitTest/DialectMechanismTests.cpp` and prove it works end to end — this is what catches a mechanism secretly built for exactly two dialects (SC-009)
- [ ] T070 Verify SC-009 against **T069's commit alone**, not against `origin/master`: `git show --stat HEAD -- CassoCore/AssemblySession.cpp CassoCore/ExpressionEvaluator.cpp CassoCore/OpcodeTable.cpp` must be empty. Diffing against master cannot work — T013, T018, T033, T036, T037, T042, T061, T063, and T064 all modify `AssemblySession.cpp` earlier in this same feature, so that diff is never empty and the criterion 023 gates on would go unverified. The claim is that *adding a dialect* touches none of the three, which is a property of the adding commit
- [ ] T071 [P] Update `CHANGELOG.md` with the merlin subcommand, the dialect mechanism, and the corrected include-file diagnostic attribution
- [ ] T072 [P] Update `README.md` with the new dialect, the updated test count, and the roadmap position relative to `023-ca65-dialect`
- [ ] T073 [P] Document the supported subset and where it ends in the repository docs, deriving the list from `CassoCore/MerlinSubsetBoundary.cpp` (SC-008)
- [ ] T074 Run `scripts/RunDormannTest.ps1` — required for assembler changes
- [ ] T075 Run `scripts/RunHarteTests.ps1 -SkipGenerate` — required for assembler changes
- [ ] T076 Run `scripts/Build.ps1 -RunCodeAnalysis` and `scripts/CheckStyle.ps1`, and confirm x64 Debug and Release are both green
- [ ] T077 Revert `.specify/feature.json` to `specs/020-disk-file-access` before merging, so master does not thrash between two concurrent specs. This is the **only** mechanism for that file — plan.md states the same, and staging by explicit path throughout the feature is what keeps it from being committed accidentally in the meantime

---

## Dependencies

**Phase order**: Setup → Foundational → US1 → US2 → US3 → Polish.

**Hard blocks**:

- T010 gates everything. If the seam changed behavior, no later phase's evidence is trustworthy.
- T016–T018 block T040 — the CPU-selection directive has nothing to switch without the provider.
- T020–T026 block T027 onward. The corpus settles six open questions, and building the parser first means guessing at answers the capture can supply.
- T056 blocks T057–T062a. The boundary table is the single source those all derive from, including its generated help text.
- T033 blocks T035–T040. All the new `Directive` tokens land in one commit so the exhaustiveness-checked `switch` breaks once rather than at every directive task.
- T005 blocks T051. The `--cpu` refusal reads the profile's `cpuSource`, so the field must exist on the seam before the parser can consult it.
- T069 depends on the whole mechanism, so it lands last despite being the criterion 023 gates on.
- T070 must be evaluated against T069's own commit. Diffing against `origin/master` cannot work, because earlier tasks in this feature legitimately modify `AssemblySession.cpp`.

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
