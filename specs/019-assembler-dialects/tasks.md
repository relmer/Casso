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

- [x] T001 [P] Promote `MockFileReader` out of `UnitTest/IncludeTests.cpp` into `UnitTest/MockFileReader.h`, register in `UnitTest.vcxproj`, and update `UnitTest/IncludeTests.cpp` to include it instead of defining it
- [x] T002a Create `scripts/ExtractDos33File.ps1` — catalog walk and file extraction from a flat DOS-order image, stripping the DOS BIN header. **Throwaway capture tooling, not a product feature**: it exists only because the Merlin disk happens to be flat DOS-order, and it does not duplicate `020-disk-file-access`'s `disk get`, which is tested C++ spanning every mountable format including WOZ. Delete it if 020's extraction lands first. *(Validated against the DOS 3.3 System Master: FID extracts at load `$0803`, CHAIN at `$0208` / 453 bytes with the stripped payload matching the raw sectors after the 4-byte header.)*
- [x] T002 [P] Create `UnitTest/MerlinCorpus/README.md` documenting the capture procedure end to end: source goes in by typing or pasting into Merlin's editor, and bytes come back out via `scripts/ExtractDos33File.ps1`. Record the Merlin-version-per-entry rule. *(Two premises here are obsolete and the README must be corrected, not just extended: the developer no longer supplies their own image — `UnitTest/Fixtures/Disks/Merlin-proDos2.23.dsk` is committed — and "the disk image is never committed" is now false. Capture is for **adding** a fixture; the five vendor oracles need none of it.)*
- [x] T002b [P] Create `scripts/FetchMerlin.ps1` — retrieve the Merlin Pro volumes from the archive item and verify each against a pinned SHA-256. Recorded after the fact: the script was written during capture and had no task, which is how the provenance chain came to be re-runnable without being planned. It stays useful now that the volumes are committed, because it is what makes "these bytes are the archive's bytes" checkable rather than asserted
- [x] T002c [P] Create `scripts/ExtractMerlinFixtures.ps1` — lift the vendor source and object files off the hash-pinned image into `UnitTest/Fixtures/Merlin/`. Also recorded after the fact. This is the **only** sanctioned way to add a Merlin fixture; nothing in the test suite runs it, and the fixtures it produces must never be edited afterward
- [x] T003 [P] Create `scripts/CaptureMerlinCorpus.ps1` skeleton with `-Entry` and `-MerlinImage` parameters and usage text, calling `ExtractDos33File.ps1` for the read-back half

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The dialect seam, diagnostic positions, and the switchable instruction set. Nothing story-specific can begin until these land.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### Seam extraction — must change no behavior

- [x] T004 Create `CassoCore/Dialect.h` with the `DialectId` enum and register it in `CassoCore.vcxproj`. *(Ships with `As65` only. `Merlin` was initially listed and the registry sweep failed on its first run, correctly: an enumerator with no profile resolves to the wrong profile while looking like support that exists. The enumerator lands with the profile in T027.)*
- [x] T005 Create `CassoCore/DialectProfile.h` declaring the abstract seam — identity, CPU-selection source, and `ParseLine` — and register it in `CassoCore.vcxproj`. *(Built with **one** virtual, not the four originally sketched: extraction showed line parsing is the honest boundary and the other three are internal to a profile that needs them. Virtuals get added when a dialect proves it needs one — see [data-model.md](./data-model.md).)*
- [x] T006 Create `CassoCore/As65Dialect.h` / `CassoCore/As65Dialect.cpp` holding today's grammar moved verbatim from `Parser::ParseLine`, and register both in `CassoCore.vcxproj`. **The AS65 directive spelling table does not move**: `DirectiveTable` keeps its global table and `GetAllSpellings()` accessor, and the profile delegates to them. Moving it would change `UnitTest/DirectiveTokenTests.cpp:70`, which sweeps that accessor — and T010 forbids exactly that
- [x] T007 Create `CassoCore/DialectRegistry.h` / `CassoCore/DialectRegistry.cpp` with the name-to-profile table and a `GetAllDialects()` accessor matching the `DirectiveTable::GetAllSpellings` pattern, and register both in `CassoCore.vcxproj`
- [x] T008 Route `Parser::ParseLine` through the active profile in `CassoCore/Parser.cpp` and `CassoCore/Parser.h`, moving the file-scope `StripComments` helper into the profile
- [x] T009 Add `dialect` to `AssemblerOptions` in `CassoCore/AssemblerTypes.h`, defaulting to `DialectId::As65` so every existing caller is unaffected
- [x] T010 Verify the seam changed nothing, **before any new test file is added**: full suite green in `x64\Release` AND `git diff --stat origin/master -- UnitTest/` shows no *existing* test file modified. Adding new files is expected later and does not violate this gate; editing one that already existed does, and means behavior moved with the code — stop and find out what
- [x] T011 [P] Add `DialectRegistry` sweep tests to `UnitTest/DialectMechanismTests.cpp` asserting every `DialectId` enumerator resolves to a profile, and register the file in `UnitTest.vcxproj`. Runs **after** T010, since it adds a file under `UnitTest/`

### Diagnostic positions

- [x] T012 Add `file` (default empty) and `column` (default 0) to `AssemblyError` in `CassoCore/AssemblerTypes.h`
- [x] T013 Route `RecordError` and `RecordWarning` through the current `PendingLine`'s `sourceFile` in `CassoCore/AssemblySession.cpp`. **Populate the position where the error is CREATED, not where it is reported** — that is the whole difficulty. Extending the error record is trivial; include attribution is only correct if the originating file is captured at the point of failure, since by reporting time the only file in hand is the top-level input. The value must also survive the trip out of core to reach the reporting site in the executable
- [x] T014 Make `ReportAssemblyDiagnostics` in `CassoCli/CommandLine.cpp` print the error's own `file` when set, falling back to the input path when empty so AS65 diagnostics are byte-for-byte unchanged
- [x] T015 [P] Add tests to `UnitTest/MerlinDiagnosticTests.cpp` proving a diagnostic raised inside an included file names that file rather than the top-level input, and register the file in `UnitTest.vcxproj`

### Switchable instruction set

- [x] T016 Create `CassoCore/InstructionSetProvider.h` / `.cpp` holding both the 6502 and 65C02 `OpcodeTable`s with an active selection, and register both in `CassoCore.vcxproj`
- [x] T017 Change `AssemblySession`'s `const OpcodeTable & m_opcodeTable` to a re-seatable pointer in `CassoCore/AssemblySession.h`, keeping `Assembler`'s existing single-`Microcode` constructor working
- [x] T018 Record the active instruction table **per line** during pass 1 and replay it in pass 2 in `CassoCore/AssemblySession.cpp` — never recompute, because conditional assembly can move where the directive is reached
- [x] T019 Verify SC-004: full suite green, with `UnitTest/AssemblerTests.cpp`, `RegressionTests.cpp`, `IntegrationTests.cpp`, and `OutputFormatTests.cpp` confirming AS65 output bytes are unchanged

**Checkpoint**: Seam in place, diagnostics carry position, both instruction tables held. User story work can begin.

---

## Phase 3: User Story 1 - Assemble existing Merlin source unmodified (Priority: P1) 🎯 MVP

**Goal**: A developer points Casso at unmodified Merlin source and gets the bytes Merlin produces.

**Independent test**: `UnitTest/MerlinCorpusTests.cpp` assembles every corpus entry and compares byte-for-byte against bytes captured from real Merlin Pro. Nothing reads a file or invokes another assembler.

### Corpus first — these settle open questions the parser depends on

**⚠️ The five vendor oracles no longer need capturing.** They are committed under
`UnitTest/Fixtures/Merlin/` and read through `IFixtureProvider::OpenFixture`, the
project's only sanctioned path to fixture bytes — `UnitTest/Fixtures/README.md`
states the Constitution II contract: no `std::ifstream` of host paths from test
code, and anything outside `UnitTest/Fixtures/` is a violation. **No test may read
`DevDisks/`.** Capture tooling exists only to *add* a fixture, via
`scripts/ExtractMerlinFixtures.ps1` against the hash-pinned disk.

Fixture format: raw DOS 3.3 bytes — skip the 4-byte BIN header, take the length
from bytes 2–3, mask bit 7, translate `$8D` to newline, compare objects from
offset 4. Do **not** assert bit 7 is set; `DCI` clears it on its terminator.
Never edit a fixture to make a test pass.

That reorders this phase: the byte-comparison work (T045-series) is unblocked
**now** and no longer waits on capture, while only the settle-by-capture entries
(T022–T025f), which need source authored here, still want the editor.

- [x] T020 [US1] Define the `CorpusEntry` shape from [data-model.md](./data-model.md) and the comparison harness in `UnitTest/MerlinCorpusTests.cpp`, serving multi-source entries through `UnitTest/MockFileReader.h`, and register the file in `UnitTest.vcxproj`. *(Comparison logic and its ten self-tests are done. The entry source changes: bytes now come from `IFixtureProvider::OpenFixture`, not from generated literals — see T020e.)*
- [ ] T020e [US1] Read corpus bytes through `IFixtureProvider::OpenFixture` (e.g. `OpenFixture("Merlin/LABELS.S")`) and add a fixture-decoding helper covering the DOS 3.3 BIN convention once rather than per entry: skip the 4-byte header, read the length from bytes 2–3, mask bit 7 for source text, translate `$8D` to newline, compare objects from offset 4. It must **not** assert bit 7 is set — `DCI` clears it on the terminating character, which is exactly the encoding this corpus exists to pin
- [ ] T020d [US1] Assert a **non-zero entry count** in the corpus sweep, and assert the count against the corpus floor once the floor is met. This is the half of the absent-corpus guard T020a could not land, since counting needs the entry table T020e introduces; it is not a duplicate of it. Lands with the first real entry rather than now, because asserting it against an empty corpus would leave a permanently red test in the suite — which masks other failures and is its own version of a signal nobody reads
- [x] T020a [US1] Make the corpus harness **fail when the corpus is absent**, not pass. A loop over an empty entry table reports success while covering nothing, which is the same failure shape as a stale test assembly and as an integration test whose data cannot be reached — success reported, coverage absent. Make an entry with empty expected bytes an error rather than a trivially satisfied comparison, and make two empty vectors comparing equal an error too — that is the worst case, since a naive comparison calls it a match. *(Done, and **only** that half. The entry-**count** assertions this task originally also claimed cannot exist yet: there is no entry table to count until T020e supplies one, so they are T020d's and the checkbox here covers the empty-expectation guards alone.)*
- [ ] T020b [US1] *(Flag and rationale are in place on `CorpusEntry`; the assertion lands with the first real entry, since it needs an assembly to run.)* Add a `discriminates` flag to `CorpusEntry` and have the harness in `UnitTest/MerlinCorpusTests.cpp` assert every entry carrying it **fails under the AS65 profile** as well as matching under Merlin. This closes the second vacuity shape: labels, origin, literals, and the evaluator are shared, so an entry built from those alone is green whether the Merlin profile works or is never consulted. An entry that passes under both dialects while claiming a Merlin construct is a defect either way — it is not exercising what it claims, or the profile is not being consulted. Shared-construct entries leave the flag clear and stay legitimate engine regression cover
- [ ] T020c [US1] Set `discriminates` on every settle-by-capture and Merlin-construct entry as it is captured (T022–T025f, T043–T045), so the classification is recorded with the entry rather than reconstructed later
- [ ] T021 [US1] Implement `scripts/CaptureMerlinCorpus.ps1` to assemble one entry under real Merlin Pro in Casso and emit source, bytes, and Merlin version as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), reading bytes back with `scripts/ExtractDos33File.ps1`
- [ ] T021c [US1] Make **delete-before-assemble** a required step of capture: delete the target object from within DOS before every assembly and confirm its absence with `scripts/CaptureMerlinCorpus.ps1 -ConfirmAbsent`. DOS 3.3 catalogs carry no timestamps, so there is no equivalent of the test suite's staleness guard — this is the only freshness check available. Without it, an assembly that errors before saving leaves the *previous* entry's object on the disk, and capturing it records one entry's bytes as another's expectation: self-consistent, plausible, wrong, and it will never fail, because the assembler faithfully reproduces the first entry's bytes from the first entry's constructs. Absence after assembly proves nothing wrote it; presence proves *this* assembly did
- [ ] T021a [US1] Read the source back off the disk and **commit that copy**, not the text that was pasted. The disk copy is what Merlin assembled, so it is the only text guaranteed to correspond to the captured bytes, and the entry becomes self-consistent by construction. This matters because Merlin's editor may normalize whitespace or column positions on save — it is column-oriented over a high-bit, CR-terminated format — which would otherwise fail every entry's verification and invite loosening the comparison until it guarded nothing. Keep the comparison and keep it loud, but treat a mismatch as information about the editor rather than a failed capture (issue #110)
- [ ] T021d [US1] **Settle on entry one**: does Merlin's editor store pasted source byte-for-byte, or normalize it? Record the answer in `specs/019-assembler-dialects/research.md`. *(Partially settled: the editor demonstrably tabs fields to fixed display columns, so the screen is not what was typed. Whether STORED bytes are normalized is still open — stalled on how to exit the editor's Add mode, since `ESC` in both message forms, a bare `RETURN`, and `Ctrl-C` are all appended as source lines rather than exiting. Needs the manual's editor command list.)* **The fixtures pivot demoted this.** It was written when every entry came through the editor, so an editor that silently normalized would have invalidated the whole corpus. The five vendor oracles are committed and never passed through the editor, so this now gates only the first **entry authored here** — T022 onward — and nothing in the byte-comparison path.
- [ ] T021f [US1] Add the disk **work-copy** discipline to `scripts/CaptureMerlinCorpus.ps1` so it refuses to operate on the pristine image: capture writes source, writes objects, and deletes targets, all on irreplaceable commercial software the developer supplied and this repository cannot regenerate. The procedure now mandates a copy; the tooling should enforce it rather than rely on remembering
- [ ] T021e [US1] Record in `UnitTest/MerlinCorpus/README.md` the residual gap automation cannot close: if the paste garbled *and* Merlin assembled the garbled source, the entry is self-consistent and simply tests a construct nobody intended. The `discriminates` flag catches the worst version — a garble that destroys the Merlin construct stops the entry failing under AS65 — but not one that merely changes an operand. Read the first few entries by eye; nothing downstream will report it. **This guard has a demonstrated near-miss, not a theoretical one**: the first version of `SendCassoKeys.ps1` corrupted every shifted character, and it surfaced only because the garbled text happened to be a BASIC syntax error. Had the first thing typed been valid either way, subtly wrong source would have been assembled faithfully and its bytes captured — self-consistent, wrong, and caught by none of the five automated axes
- [ ] T021b [US1] Batch constructs into a few **composite** source files rather than one file per construct: assemble once with the listing on, save the object, extract, and split by known offsets. A handful of composites covers the FR-007..FR-015 floor at a fraction of the typing
- [ ] T022 [P] [US1] Capture irregular-spacing entries — extra spaces, tabs, and mixtures — as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) to settle the field-based line model empirically
- [ ] T023 [P] [US1] Capture mixed-case and long-symbol entries as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) to settle symbol case sensitivity, length limit, and legal character set (research.md CHK008, CHK009)
- [ ] T024 [P] [US1] Capture expression entries covering Merlin's operator set, precedence, and the current-program-counter form as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) (research.md CHK052)
- [ ] T025 [P] [US1] Capture a `XC OFF` entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) to settle whether Merlin accepts a reset form (spec Edge Cases)
- [ ] T025a [P] [US1] Capture the comment-field experiment as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file): one line whose fourth field begins with an ordinary word and would be a syntax error if parsed as anything but a comment. Acceptance confirms comment-by-position; an error proves `;` is required. Keep the entry either way, to pin the answer against regression
- [ ] T025b [P] [US1] Capture quoted-string entries with **leading, embedded, and trailing spaces inside the quotes** as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file). A whitespace-delimited operand scanner breaks on these, and the disk's own `PI.START.S` is full of them — the spaces are payload bytes, so getting this wrong both truncates the operand and silently changes emitted data
- [ ] T025c [P] [US1] Capture a macro fall-through entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), modeled on the vendor library's `ADDX MAC` / `TXA` / `ADDA MAC` with one shared `<<<`, to settle whether an unterminated `MAC` falls into the next. Also settles that the unterminated-macro diagnostic must not fire on legitimate vendor source
- [ ] T025d [P] [US1] Capture a macro-local label entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file): expand **two macros that each define the same bare label in a single assembly**, the case the vendor library carefully never creates. Both outcomes are informative — a duplicate-symbol error means Merlin does not scope macro bodies, the vendor library carries an undocumented constraint that those macros are mutually exclusive, and our duplicate-symbol diagnostic is right to fire; a clean assembly means Merlin scopes macro-local labels and we must too, or every multi-use macro breaks. Either way it changes the symbol table
- [ ] T025e [P] [US1] Capture a `>>>` macro-invocation entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file). The vendor library invokes macros by bare name only, so the disk can never report whether an explicit invocation prefix is also accepted — and a user's source may well contain one. First instance of the general rule that absence from the disk is not absence from the language
- [ ] T025f [P] [US1] Capture the vendor library's five-deep nested first-character conditional (`MOVD`) as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) as a stress entry. This is how Merlin macros dispatch on addressing mode, so any macro library of consequence exercises it
- [ ] T026 [US1] Cross-check a sample of captured entries against hand-derived expectations from the Merlin manual, and record the answers to all settle-by-capture items in `specs/019-assembler-dialects/research.md`. **If T025 shows a CPU-target reset form exists**, amend FR-015 and the `InstructionSetProvider` state transition in `data-model.md` — both currently describe a one-way `base → extended` change — and add the implementing task before T040 rather than discovering the conflict during it

### The Merlin profile

- [ ] T027 [US1] Create `CassoCore/MerlinDialect.h` / `.cpp` as a `DialectProfile` subclass, register it in `CassoCore/DialectRegistry.cpp` and `CassoCore.vcxproj`, and **add the `DialectId::Merlin` enumerator in the same change**. The enumerator is added *with* its profile, never ahead of it — a placeholder resolves to the wrong profile while looking like support that exists, and `DialectMechanismTests` fails until the registry answers for it
- [ ] T028 [US1] Implement Merlin comment conventions in `CassoCore/MerlinDialect.cpp` — asterisk in column 1 for a whole-line comment; a semicolon **beginning the field after the operand** introduces a trailing comment. **Not "a semicolon anywhere"**: inside the operand field a semicolon is data, and the disk's own macro library depends on it (`ADD SUMSTR;DEFLEN;PL`). Whether the introducer is even required, or whether a fourth field is a comment regardless of what starts it, is one of the settle-by-capture questions — T025a answers it and this task implements the answer (FR-007)
- [ ] T029 [US1] Implement field-based line segmentation in `CassoCore/MerlinDialect.cpp` — whitespace runs separate label, opcode, operand, and comment; tabs are ordinary whitespace with no tab-stop expansion; no field is required at a specific column. **The scanner must respect quoting**: whitespace ends the operand only outside a quoted string, or `ASC "HELLO WORLD"` splits into an operand and a bogus comment. **And a `;` inside the operand field is data**, not a comment — it is Merlin's macro-argument separator (FR-007, FR-008)
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

- [ ] T043 [P] [US1] Capture one entry per string-encoding spelling as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file) — five entries, not one, because a high-bit or terminator error still looks plausible
- [ ] T044 [P] [US1] Capture at least one multi-file inclusion entry as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), served through `UnitTest/MockFileReader.h`
- [ ] T045 [P] [US1] Capture one entry per remaining construct in FR-007 through FR-015 and FR-027 as a hand-authored entry in `UnitTest/MerlinCorpusTests.cpp` (vendor oracles live in `UnitTest/Fixtures/Merlin/`; entries authored here stay in the test file), completing the corpus floor
- [ ] T045a [P] [US1] Validate against the **five** real positive oracles on the Merlin Pro 2.23 disk — source and shipped object both present, absolute mode — not "~40 files". Measured: `LABELS.S`→`LABELS` (984 @ `$8000`, 105× `DCI` plus one `ERR`), `KEYMAC.S`→`KEYMAC` (674 @ `$9000`), `PRINTFILER.S`→`PRINTFILER` (286 @ `$02A0`), `MAKE DUMP.S`→`MAKE DUMP` (589 @ `$9000`), and `CLOCK.S`→**both** `CLOCK.24` and `CLOCK.12` (365 @ `$0240` each). Five sources, six objects. Vendor source and objects **are committed**, under `UnitTest/Fixtures/Merlin/`, and read through `IFixtureProvider::OpenFixture` — the earlier "used, not committed" instruction is superseded
- [ ] T045d [P] [US1] Prioritize `CLOCK.S`: one source producing two different objects through `DO HOURS-12` / `ELSE` / `FIN`, so a single capture yields conditional-assembly coverage **and** two independent byte-identical checks. The highest-value single entry on the disk
- [ ] T045e [P] [US1] Use `Merlin/PI.ADD.S` and `Merlin/PI.START.S` as **negative** subset-boundary specimens only, never positive comparisons — they ship no objects, and the APPLE PI group is the linker demo whose own header says "This is just a test source for the linker". `PI.ADD.S` is the export-only shape (`REL` + **6** `ENT`, no `EXT`); `PI.START.S` is the no-workaround shape (`REL` + 3× `EXT` + 1 `ENT`). Between them they exercise **both** refusal messages, which is why exactly these two are committed: `PI.MAIN.S` and `PI.DIV.S` also import and are redundant with `PI.START.S`, and `PI.LOOK.S` is redundant with `PI.ADD.S`
- [ ] T045f [P] [US1] Use the type-T macro libraries (`T.MACRO LIBRARY`, `T.SENDMSG`, `T.PRDEC`, `T.OUTPUT`, `T.FPMACROS`, `T.ROCKWELL MACROS`, `T.PI.MACS`, `PI.NAMES`) as the `PUT`/`USE` inclusion corpus. They are the only type-T files on the disk — every `.S` source is type B loading at `$0901` — so they also settle the DOS 3.3 text convention from real vendor files. `RWTS DEMO.S` ships **no** object, so it is a parse/assemble case only, never a comparison
- [ ] T045c [P] [US1] Record the vendor-source validation *result* — that re-assembling a vendor file reproduces its shipped object byte-for-byte — as evidence in `research.md` rather than as a committed corpus entry, which is licensing-safe and is the part that actually carries information
- [ ] T045b [P] [US1] Walk the **manual's directive list** and add a corpus entry for every construct the disk does not exercise. The disk demonstrates idiom but cannot report what the vocabulary holds that this vendor never used; absence from the disk is not evidence of absence from the language (spec Corpus Floor)
- [ ] T046 [US1] Verify SC-001: every corpus entry assembles byte-identically via `UnitTest/MerlinCorpusTests.cpp`
- [ ] T046a [US1] Verify SC-002 in `UnitTest/MerlinCorpusTests.cpp`: the five vendor sources assemble **exactly as committed**, with no edit to any line. The corpus already proves the bytes match; what this adds is the claim that the *input* was not touched to get there — assert each entry's source is the fixture bytes as `IFixtureProvider::OpenFixture` returns them, not a transcribed or tidied copy. Without it, SC-002 is only inspected, and "unmodified" is precisely the property a passing corpus can be made to fake
- [ ] T046b [US1] Verify SC-003 in `UnitTest/MerlinSubsetTests.cpp`: assemble every committed vendor source and assert that **each rejection maps to a row in `CassoCore/MerlinSubsetBoundary.cpp`**. SC-003 defines a rejection with no boundary row as a defect, so this is a sweep over rejections rather than a fixed list of expected errors — a new unexplained rejection fails the test by construction. Runs against `PI.ADD.S` and `PI.START.S` too, where rejections are the expected outcome and must still be table-backed
- [ ] T047 [P] [US1] Add focused parser tests to `UnitTest/MerlinParserTests.cpp` and directive tests to `UnitTest/MerlinDirectiveTests.cpp`, registering both in `UnitTest.vcxproj`

**Checkpoint**: Unmodified Merlin source assembles to Merlin's bytes. This is the MVP.

---

## Phase 4: User Story 2 - Choose a dialect explicitly (Priority: P1)

**Goal**: A developer states the dialect and gets exactly that dialect's rules, strictly.

**Independent test**: One source file assembled under each dialect selection; constructs valid in one and invalid in the other are accepted and rejected accordingly.

**⚠️ Shared file warning**: `CassoCore/CommandLineParser.cpp` and `UnitTest/CommandLineTests.cpp` are shared with the concurrently developed spec 020. Everything **added** here is additive — two rows, one enumerator, one arm, one flag parser. Do not restructure the dispatcher; if that seems necessary, stop and raise it.

The **one** sanctioned exception is T049b/T049c, removing the fallback heuristic: a decision taken explicitly, not a restructuring reached for. It is the only place this feature edits shared behavior rather than extending it, and it is why the 020 session will need to rebase. Keep it confined to the fallback cases — a wider edit here is not covered by that decision.

- [ ] T048 [US2] Add `Subcommand::Merlin` and a `dialect` field to `CommandLineOptions` in `CassoCore/CommandLineOptions.h`
- [ ] T049 [US2] Add one row `{ "merlin", CommandLineOptions::Subcommand::Merlin }` to `s_kSubcommands` and one arm in `Parse` in `CassoCore/CommandLineParser.cpp`
- [ ] T049a [US2] Add one row `{ "as65", CommandLineOptions::Subcommand::As65 }` to `s_kSubcommands` in `CassoCore/CommandLineParser.cpp`, giving AS65 the **explicit** selector FR-001 requires. `Subcommand::As65` already exists and is already the arm the fallback lands on, so this is one table row and **no new arm** — smaller than T049. Two things it must settle rather than discover later: an explicit `as65 <source>` must produce options identical to the fallback's for the same source, pinned by a test; and `CassoCli as65` with no operand stops being a request to assemble a file *named* `as65` (resolved through `s_kpszSourceExtensions`) and becomes a usage error. That second one is a real change to an existing invocation, however unlikely the filename — name it in `CHANGELOG.md` rather than letting it surface as a bug report. This is the **prerequisite** for T049b, which removes the fallback heuristic (issue #92): the heuristic cannot be retired while it is the only route to AS65, so the selector must land first and this ordering is not negotiable
- [ ] T049b [US2] **Remove the unrecognized-first-argument AS65 fallback** from `Parse` in `CassoCore/CommandLineParser.cpp` (issue #92). Runs strictly after T049a — the heuristic cannot be retired while it is the only route to AS65. The error for an unrecognized first argument MUST name the replacement (`did you mean: CassoCli as65 <source>`) rather than print usage: a script that has invoked Casso this way for years needs the fix in the message, not a bisect. Reverses the earlier deferral recorded in [research.md](./research.md) D5 and in the CLI contract's guaranteed-unchanged list; both now say so
- [ ] T049c [US2] Update `UnitTest/CommandLineTests.cpp` for the removal — the tests that pin the fallback now pin the error and its suggestion instead. **This is the one place this feature stops being additive to shared code.** Spec 020 is developing against this file concurrently and will need to rebase; that cost belongs to the decision in T049b, not to whoever hits the merge conflict. Keep every unrelated case in the file untouched so the conflict surface is exactly the fallback cases and no wider
- [ ] T050 [US2] Add `ParseMerlinFlags` to `CassoCore/CommandLineParser.h` and `CassoCore/CommandLineParser.cpp` per [contracts/cli.md](./contracts/cli.md)
- [ ] T051 [US2] Refuse `--cpu` **when the active profile's `cpuSource` is in-source**, driven by profile data rather than by a merlin-specific branch, in `CassoCore/CommandLineParser.cpp`; the message names the directive supplied by the profile. A hard-coded merlin arm here would put a per-dialect branch in the shared mechanism, which is what `contracts/dialect-profile.md` guarantee 3 forbids and what SC-009 exists to catch (FR-026)
- [ ] T052 [US2] Set `AssemblerOptions::dialect` from `CommandLineOptions::dialect` in `CassoCli/CommandLine.cpp`, carrying **provenance** and not just the dialect. With the fallback gone the command line always states a dialect, but `AssemblerOptions` still defaults to AS65 for callers that set none — FR-006 makes the assembler reachable from entry points that are not the CLI — and the reporting table in [contracts/cli.md](./contracts/cli.md) keys off exactly that distinction: stated is reported nowhere, defaulted is reported under `-v` or in the listing header
- [ ] T053 [US2] Create `CassoCore/DialectReporting.h` / `.cpp` deciding **what** dialect-and-CPU line to emit and **when**, per the reporting table in [contracts/cli.md](./contracts/cli.md), and register both in `CassoCore.vcxproj`. `CassoCli/CommandLine.cpp` only prints what it returns — never unconditionally on stdout, which carries the listing when no listing file is named. The decision lives in core so `UnitTest` can exercise it (FR-004, SC-005)
- [ ] T053a [P] [US2] Report the **CPU target** alongside the dialect through the same path — including when it was left at the dialect's default, so "no directive was seen" is not misread as "the flag was ignored" — in `CassoCore/DialectReporting.cpp` (SC-005)
- [ ] T053c [US2] Verify SC-005 in `UnitTest/DialectReportingTests.cpp`: walk **every row** of the reporting table in [contracts/cli.md](./contracts/cli.md) and assert `DialectReporting` produces what that row says — including the two negative rows, that nothing is emitted when a selection was stated, and that nothing reaches stdout in any case. The negatives are the half worth having: an implementation that reports unconditionally satisfies "the developer can determine it" while breaking the piped-listing guarantee FR-004 spends most of its words on
- [ ] T053b [P] [US2] Register the `as65` and `merlin` subcommands and their flag tables in the tool's usage and help output via `CassoCli/CommandLine.h` and `CassoCli/CommandLine.cpp`, deriving the flag list from core so help cannot drift from the parser, with a test (FR-024, US2 acceptance 4)
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
- [ ] T057a [US3] Make the relocatable refusal **actionable** in `CassoCore/MerlinSubsetBoundary.cpp`: a module with relocatable mode and entry symbols but no external symbols exports without importing, so the message states it assembles on its own once relocatable mode is removed and an origin supplied. A module declaring any external symbol gets the other message — no workaround, resolving cross-module references needs the linker in issue #112 — because offering the first fix there sends the developer down a path that cannot work. The vendor's own sample is the export-only case, so this is the likely first encounter (FR-031)
- [ ] T058 [US3] Refuse the second occurrence of the CPU-selection directive in `CassoCore/MerlinDialect.cpp` as selecting an unemulated CPU (FR-016)
- [ ] T059 [P] [US3] Refuse the file-type directive in `CassoCore/MerlinDialect.cpp` as owned by `020-disk-file-access` (FR-028)
- [ ] T060 [P] [US3] Refuse the save-object directive in `CassoCore/MerlinDialect.cpp` as multi-output segmentation needing its own decision — the message must NOT describe it as waiting on 020 (FR-029)
- [ ] T061 [US3] Make a boundary refusal distinguishable from a syntax error in `CassoCore/AssemblySession.cpp`, and collect every offender across the whole pass before failing rather than stopping at the first (FR-017, FR-018)
- [ ] T062 [US3] Generate the subset-boundary help text **from the boundary table inside `CassoCore/MerlinSubsetBoundary.cpp`**, returning a string that `CassoCli/CommandLine.cpp` merely prints. Generation in the executable would be unreachable from `UnitTest`, so FR-019's "cannot disagree by construction" would gain no test — and Principle VI is non-negotiable (FR-019, FR-024)
- [ ] T062a [P] [US3] Add a test to `UnitTest/MerlinSubsetBoundaryTests.cpp` asserting the generated help text names every row the accessor returns, so a row added to the table without help coverage fails the build rather than shipping
- [ ] T063 [US3] Populate `column` on every Merlin diagnostic in `CassoCore/MerlinDialect.cpp` and `CassoCore/AssemblySession.cpp` (FR-021)
- [ ] T064 [US3] Describe constructs in the active dialect's vocabulary, and name which dialect defines a construct rejected as belonging to another, in `CassoCore/AssemblySession.cpp` (FR-020, FR-022)
- [ ] T065 [US3] Explain the column rule when a Merlin label is indented, rather than reporting an unknown symbol, in `CassoCore/MerlinDialect.cpp` (User Story 3 acceptance 1)
- [ ] T066 [P] [US3] Add the hand-authored negative corpus class — boundary refusals and diagnostic expectations, kept distinct from captured entries — to `UnitTest/MerlinCorpusTests.cpp` as hand-authored entries, including an entry where a macro is invoked with another dialect's argument syntax and must be **rejected rather than partially expanded** (spec Edge Cases)
- [ ] T067 [P] [US3] Add `UnitTest/MerlinSubsetBoundaryTests.cpp` sweeping the boundary accessor and asserting every row produces the expected refusal, and register it in `UnitTest.vcxproj`
- [ ] T068 [US3] Verify SC-006 and SC-007: every dialect-specific diagnostic identifies the correct line and column, and every out-of-subset construct is named rather than failing as a parse error

**Checkpoint**: Diagnostics are dialect-native, positioned, and boundary refusals are unmistakable.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [ ] T069 Add a synthetic, test-only third dialect profile to `UnitTest/DialectMechanismTests.cpp` and prove it works end to end — this is what catches a mechanism secretly built for exactly two dialects (SC-009). **Do not pull this forward.** Run against a seam shaped by AS65 alone it passes trivially, because the synthetic profile gets written to fit whatever seam exists; it only carries weight once Merlin has pressed on the seam with its field model, operand-internal semicolons, quoted operands, and first-character conditional
- [ ] T070 Verify SC-009 against **T069's commit alone**, not against `origin/master`: `git show --stat HEAD -- CassoCore/AssemblySession.cpp CassoCore/ExpressionEvaluator.cpp CassoCore/OpcodeTable.cpp` must be empty. Diffing against master cannot work — T013, T018, T033, T036, T037, T042, T061, T063, and T064 all modify `AssemblySession.cpp` earlier in this same feature, so that diff is never empty and the criterion 023 gates on would go unverified. The claim is that *adding a dialect* touches none of the three, which is a property of the adding commit
- [ ] T071a [P] Add a **breaking changes** entry to `CHANGELOG.md`, as its own heading rather than inside the feature announcement: `CassoCli input.a65 -o out.bin` no longer works and becomes `CassoCli as65 input.a65 -o out.bin` (T049b), and a bare `CassoCli as65` stops resolving `as65` as a source filename (T049a). A reader scanning for what will break must not have to find it inside a paragraph about dialect support. State the replacement invocation literally, so the entry is copy-pasteable into a build script
- [ ] T071 [P] Update `CHANGELOG.md` with the merlin subcommand, the dialect mechanism, and the corrected include-file diagnostic attribution
- [ ] T072 [P] Update `README.md` with the new dialect, the updated test count, and the roadmap position relative to `023-ca65-dialect`
- [ ] T073 [P] Document the supported subset and where it ends in the repository docs, deriving the list from `CassoCore/MerlinSubsetBoundary.cpp` (SC-008)
- [ ] T074 Run `scripts/RunDormannTest.ps1` — required for assembler changes
- [ ] T075 Run `scripts/RunHarteTests.ps1 -SkipGenerate` — required for assembler changes
- [ ] T076 Run `scripts/Build.ps1 -RunCodeAnalysis` and `scripts/CheckStyle.ps1`, and confirm x64 Debug and Release are both green
- [ ] T077a Update `CLAUDE.md`'s spec inventory **at merge time**, not before: move 019 from "drafted but NOT started" to shipped, and strike the sequencing note that says 023 must wait on it — 023's gate is satisfied once this lands. Deliberately deferred rather than done during the feature, because `CLAUDE.md` names spec 020 as active and the concurrent session owns that block; editing it early would put the two specs in conflict on a shared file. Leave the active-spec pointer alone entirely — that is 020's to change
- [ ] T077 Revert `.specify/feature.json` to `specs/020-disk-file-access` before merging, so master does not thrash between two concurrent specs. This is the **only** mechanism for that file — plan.md states the same, and staging by explicit path throughout the feature is what keeps it from being committed accidentally in the meantime

---

## Dependencies

**Phase order**: Setup → Foundational → US1 → US2 → US3 → Polish.

**Hard blocks**:

- T010 gates everything. If the seam changed behavior, no later phase's evidence is trustworthy.
- T016–T018 block T040 — the CPU-selection directive has nothing to switch without the provider.
- T020 and T020e block T027 onward. The harness and the fixture read path are what make any parser claim checkable; without them the parser has no oracle.
- The **settle-by-capture** entries block only the tasks whose semantics they settle, not the phase: T025a → T028, T022 and T025b → T029, T023 → T030, T025c/T025d/T025e → T038, T025 → T040, T025f → the conditional handling in T038's neighborhood, T024 → the expression work T026 records. Each is a specific question with a specific dependent, and pretending the whole block gates the whole phase would idle work the committed fixtures already unblock.
- The five vendor oracles block nothing — they are committed. T045a and the rest of the T045-series can run as soon as T020e lands, which is why they no longer sit behind capture.
- T056 blocks T057–T062a. The boundary table is the single source those all derive from, including its generated help text.
- T033 blocks T035–T040. All the new `Directive` tokens land in one commit so the exhaustiveness-checked `switch` breaks once rather than at every directive task.
- T005 blocks T051. The `--cpu` refusal reads the profile's `cpuSource`, so the field must exist on the seam before the parser can consult it.
- T049a blocks T049b, and the order is not negotiable. Removing the fallback before the explicit `as65` selector exists would leave AS65 unreachable — a window in which the tool cannot assemble AS65 at all, however briefly. T049c lands with T049b in the same commit, since the tests and the behavior they pin cannot disagree even transiently.
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
