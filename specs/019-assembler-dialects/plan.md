# Implementation Plan: Merlin Assembler Dialect

**Branch**: `019-assembler-dialects` | **Date**: 2026-08-15 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/019-assembler-dialects/spec.md`

## Summary

Casso's assembler has exactly one front end, hard-coded in `Parser::ParseLine`.
This feature extracts that front end into a **dialect profile** seam and adds
**Merlin** as a second profile, leaving the two-pass engine, expression
evaluator, and opcode tables shared and unchanged.

Three things make this more than a parser swap. Merlin selects its CPU from
source rather than the command line, so the assembler must hold both instruction
tables and switch between them mid-assembly. Merlin's file inclusion makes
multi-file assembly normal, which exposes an existing defect where every
diagnostic is attributed to the top-level input. And Merlin's relocatable path
must be refused by name rather than failing as an unknown directive, which means
the subset boundary is a table rather than scattered checks.

Correctness is measured against bytes captured from real Merlin 8, compiled into
the test project. Nothing at test time reads a disk or invokes another assembler.

## Technical Context

**Language/Version**: C++ (`stdcpplatest`, MSVC v145 / VS 2026)

**Primary Dependencies**: None added. Windows SDK and STL only, per the
constitution's dependency baseline.

**Storage**: N/A. Corpus fixtures are compiled-in string literals and byte
arrays, not files.

**Testing**: Microsoft C++ Unit Test Framework, in the existing `UnitTest`
project. Extended suites `scripts/RunDormannTest.ps1` and
`scripts/RunHarteTests.ps1 -SkipGenerate` are required before merge, because this
is an assembler change.

**Target Platform**: Windows 10/11, x64 and ARM64. ARM64 is build-only here — no
device is available to run tests on, so x64 Debug and Release green is the bar.

**Project Type**: Compiler front end inside a static library (`CassoCore`),
consumed by a console tool (`CassoCli`), a GUI application (`Casso`), and the
test project.

**Performance Goals**: No specific target. The constitution's bar — a typical
6502 program under 10K lines with no noticeable delay — carries, and dialect
dispatch is a per-line virtual call against work already dominated by expression
evaluation.

**Constraints**: Output bytes for every existing AS65 source must be unchanged
(SC-004). Existing AS65 *acceptance* must be unchanged (FR-005 as clarified).
`CommandLineParser` is shared with the concurrently developed spec 020, so changes
to it must be additive: one table row, one enumerator, one arm, one flag parser.

**Scale/Scope**: One new dialect profile plus the mechanism, sized for N profiles.
Roughly 20 Merlin directives in scope, 5 refused at the subset boundary. Baseline
test suite is 2,961 Debug / 2,958 Release.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment | Verdict |
|---|---|---|
| I. Code Quality | New code follows EHM, single-exit, declarations-at-top, and the file/class layout rules. Each dialect profile is a class with methods and therefore gets its own header/source pair. No anonymous namespaces; tables are file-scope `static constexpr` because they span 3+ lines. | PASS |
| II. Testing Discipline | Every new class is in `CassoCore` and reachable from `UnitTest`. Corpus fixtures are compiled-in; the multi-file case uses an injected mock reader. No test reads a file, and FR-019 was clarified specifically to avoid a doc-reading test. | PASS |
| III. User Experience Consistency | `merlin` is a bare-word subcommand matching `run`. The dialect is reported on the diagnostic stream and in listing headers, never unconditionally on stdout, so piped listings are unaffected. Existing invocations are untouched — the AS65 fallback stays. | PASS |
| IV. Performance | One virtual call per source line. No allocation added to the per-line path beyond what `ParseLine` already does. | PASS |
| V. Simplicity | The seam is justified by FR-001 and by 023 depending on it; it is not speculative. The profile is mostly data specifically to avoid an elaborate class hierarchy. | PASS |
| VI. Thin Executable, Testable Core | Everything lands in `CassoCore`. `CassoCli` gains **only I/O edges**: printing a diagnostic using the error's own file, printing a string core generated, and returning a code core computed. Every decision behind those — what to report, when to report it, which exit code applies, and the text of generated help — lives in core where `UnitTest` reaches it. | PASS |

**Post-design re-check**: PASS. No violation surfaced during Phase 1; the
Complexity Tracking table below is empty.

One deliberate deviation from the `/speckit-plan` workflow is recorded rather
than silently taken: the step that rewrites `CLAUDE.md`'s active-spec block was
**not** performed. That block currently names spec 020, which another session is
developing concurrently, and flipping it here would put the two specs in conflict
on a shared project file.

`.specify/feature.json` is handled by exactly one mechanism, not two: it is
repointed to 019 as session-local state and **reverted to `020-disk-file-access`
by task T077 before the merge**, so master never thrashes between the two
concurrent specs. Staging is by explicit path throughout the feature rather than
`git add -A`, so the file cannot be swept into a commit by accident in the
meantime.

## Project Structure

### Documentation (this feature)

```text
specs/019-assembler-dialects/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── cli.md               # merlin subcommand grammar
│   ├── dialect-profile.md   # the seam every profile implements
│   └── merlin-directives.md # supported vocabulary and the subset boundary
├── checklists/
│   └── requirements.md  # requirements quality audit
└── tasks.md             # Phase 2 output (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
CassoCore/
├── Dialect.h                    # DialectId enum + registry row type
├── DialectRegistry.h/.cpp       # name -> profile table, GetAllDialects()
├── DialectProfile.h             # abstract seam: identity, CPU source, ParseLine
├── As65Dialect.h/.cpp           # today's grammar, extracted unchanged
├── MerlinDialect.h/.cpp         # the new profile
├── MerlinSubsetBoundary.h/.cpp  # refusal table + GetAll accessor + generated
│                                # help text (in core, so a test can reach it)
├── DialectReporting.h/.cpp      # decides what dialect/CPU line to emit and when
├── AssemblerExitCode.h/.cpp     # maps an assembly result to 0 / 1 / 2
├── StringEncoding.h/.cpp        # high-bit / inverse / flashing / terminator
├── InstructionSetProvider.h/.cpp# both opcode tables, switchable mid-assembly
├── Parser.h/.cpp                # delegates line parsing to the active profile
├── Directive.h/.cpp             # shared token enum; spelling tables move to profiles
├── AssemblerTypes.h             # AssemblyError gains file/column; options gain dialect
├── AssemblySession.h/.cpp       # per-line active table; diagnostics carry file/column
├── CommandLineOptions.h         # Subcommand::Merlin, dialect field
└── CommandLineParser.h/.cpp     # one row, one arm, ParseMerlinFlags

CassoCli/
└── CommandLine.cpp              # I/O edges only: print diagnostics using the
                                 # error's own file, print core-generated help,
                                 # return the core-computed exit code

UnitTest/
├── MockFileReader.h             # promoted out of IncludeTests.cpp
├── DialectMechanismTests.cpp    # includes the synthetic third profile (SC-009)
├── MerlinParserTests.cpp
├── MerlinDirectiveTests.cpp
├── MerlinSubsetBoundaryTests.cpp
├── MerlinDiagnosticTests.cpp
├── MerlinCorpusTests.cpp
└── MerlinCorpus/
    ├── CorpusEntries.h          # generated: source + expected bytes + version
    └── README.md                # capture procedure

scripts/
└── CaptureMerlinCorpus.ps1      # regenerates one entry against real Merlin 8
```

**Structure Decision**: Everything behavioral lands in `CassoCore`, which both the
executables and `UnitTest` link — the arrangement Principle VI requires. `CassoCli`
receives a single formatting change. One class per header/source pair, per the
project's file-layout rule; the abstract `DialectProfile` is header-only because
it declares no out-of-line behavior beyond its pure virtuals.

## Implementation Phases

Ordered so that each phase leaves the tree compilable and the suite green, and so
that the phases that *settle open questions* come before the phases that depend on
the answers.

### Phase A — Seam extraction (no behavior change)

Introduce `DialectProfile`, `DialectRegistry`, and `As65Dialect`; move today's
`Parser::ParseLine` grammar into the AS65 profile untouched; route the parser
through the active profile. Add `dialect` to `AssemblerOptions` and default it to
AS65.

**The AS65 directive spelling table does not move.** `DirectiveTable` keeps its
existing global table and `GetAllSpellings()` accessor, and the AS65 profile
delegates to them; Merlin brings its own table alongside. Moving the table into
the profile would change `UnitTest/DirectiveTokenTests.cpp:70`, which sweeps that
accessor — and this phase's whole value is that no existing test changes. A later
migration is possible; it is not this feature's business.

**Exit criterion**: the full suite passes with **no existing test file modified**.
Adding new test files is expected and does not violate the gate. This phase is
provably behavior-preserving or it is wrong, which is why it is first and alone.

### Phase B — Diagnostics carry position

Add `file` and `column` to `AssemblyError` with defaults, route `RecordError` and
`RecordWarning` through the current `PendingLine`, and teach
`ReportAssemblyDiagnostics` to prefer the error's own file. Fixes the
include-attribution defect (FR-025).

**Exit criterion**: existing tests unchanged and passing; a new test proves an
error inside an included file names that file.

### Phase C — Instruction-set provider

Hold both opcode tables, record the active one per line in pass 1, replay it in
pass 2. No dialect uses the switch yet.

**Exit criterion**: AS65 output byte-identical across the existing corpus
(SC-004).

### Phase D — Corpus capture and harness

Promote `MockFileReader`, write the capture script and procedure, and land the
first corpus entries — starting with the ones that **settle open questions**:
irregular spacing, symbol case and length, and expression operators and
precedence. Entries land before the parser that must satisfy them.

**Exit criterion**: corpus harness runs, entries are compiled in, and the six
"settle by capture" items in research.md have answers recorded.

### Phase E — Merlin profile

The dialect itself: comments, labels and local labels, data and hex directives,
the string-encoding family, variables and the loop construct, dummy sections,
macros, file inclusion, and the CPU-selection directive. Driven by the corpus.

**Exit criterion**: the corpus floor is met and byte-identical (SC-001).

### Phase F — Subset boundary

The refusal table, its accessor, generated help text, and the refusals themselves
including the second CPU-selection directive, the file-type directive, and the
save-object directive.

**Exit criterion**: every table row produces a named refusal, distinguishable from
a syntax error, with all offenders reported in one pass (FR-016 through FR-019).

### Phase G — Command line and reporting

The `merlin` subcommand row, arm, and flag parser; `--cpu` refusal; dialect
reporting on stderr under verbose and in listing headers.

**Exit criterion**: `UnitTest/CommandLineTests.cpp` passes unchanged, and new
tests cover the added grammar.

### Phase H — Mechanism proof and polish

The synthetic third profile proving SC-009, then CHANGELOG, README, and help
text.

**Exit criterion**: the synthetic profile compiles and passes without touching the
engine, evaluator, or opcode tables — the claim 023 gates on.

**Why this is last, and must not be pulled forward.** Run the synthetic-profile
test against a seam shaped by AS65 alone and it passes trivially, because the
synthetic profile gets written to fit the seam that exists. All that proves is
that the seam supports profiles shaped like AS65 — which is not the claim. The
test carries weight only once **two genuinely different real profiles** have
shaped the seam, and Merlin is the one that exerts real pressure: field-based
lines, semicolons that mean different things inside and outside the operand,
quoted operands containing spaces, and a first-character conditional. If the seam
survives all of that with the engine unchanged, a third profile passing means
something.

**Expect the seam to grow during Phase E, and do not read that as a failure.**
Adding a virtual to `DialectProfile` is *extending the seam*. SC-009 forbids
modifying the **engine** — the two-pass driver, the expression evaluator, the
opcode tables — which is a different thing entirely. A later reader who conflates
them will either fight a legitimate addition or weaken the criterion to
accommodate it; both are worse than saying this plainly here.

Two consequences to price in. Each virtual added for Merlin also lands on
`As65Dialect`, and that churn is the honest cost of not shipping speculative
hooks — the cheaper side of the trade, but not free. And the T010-style "no
existing test modified" evidence will **not** hold across that churn, nor should
it be expected to; T010's gate was scoped to the extraction, where behavior
preservation was the entire claim.

## Risks

| Risk | Mitigation |
|---|---|
| Phase A silently changes AS65 behavior while "just moving code" | Phase A is isolated and its exit criterion is a fully unchanged test suite. Any test edit in Phase A is a signal to stop. |
| Merlin does not boot cleanly under Casso, blocking capture | Capture is offline and one-time. A sample of entries is cross-checked against hand-derived expectations from the manual, so a disagreement identifies whether the corpus or the emulator is at fault. |
| Pass 1 and pass 2 disagree about the active instruction table | The table is recorded per line in pass 1 and replayed, never recomputed. The failure this prevents is specific: a CPU-selection directive inside a conditional block whose taken-ness differs between passes would leave pass 2 sizing instructions against a different table than pass 1 bound them with. That surfaces as a corpus byte mismatch far from its cause, which is among the hardest defects here to trace — so the rule is a design constraint, not an optimization. Called out in research.md D3 and as task T018. |
| Concurrent spec 020 conflicts in `CommandLineParser` | Changes are additive by construction: one row, one enumerator, one arm, one new flag parser. No dispatcher restructuring. |
| Concurrent spec 020 conflicts in `CassoCli/CommandLine.cpp` | **Expected, and caused by adjacency rather than disagreement.** Both features add a usage function near `PrintUsageRun` and both touch the exit-code path in `DoAs65` / `DoRun`. The exit-code vocabulary is already agreed (0 clean, 1 warned, 2 no output, 3+ scoped per subcommand), so that half is coordination done — do not re-derive it. The help-text insertion point is a textual conflict that takes seconds to resolve. `ReportAssemblyDiagnostics` is this feature's alone. Splitting the file mid-flight would cost two rebases to avoid conflicts cheaper than the rebases. |
| Merlin's expression semantics differ from the shared evaluator | The corpus floor requires evaluator entries, and Phase D lands them before Phase E depends on them. If they diverge, the evaluator gains a dialect-scoped operator table rather than the profile forking it. |

## Complexity Tracking

> Fill ONLY if Constitution Check has violations that must be justified.

No violations. Table intentionally empty.
