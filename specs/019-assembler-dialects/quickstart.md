# Quickstart: Validating the Merlin Dialect

**Feature**: `019-assembler-dialects` | **Date**: 2026-08-15

How to prove the feature works. Details of *what* is being validated live in
[spec.md](./spec.md) and [contracts/](./contracts/); this is the run guide.

## Prerequisites

- Visual Studio 2026 toolset (v145), x64.
- Build via `Casso.sln`, **not** the `.vcxproj` directly — building the project
  file alone drops the executable somewhere other than the solution output
  directory, and you end up testing a stale binary.
- The style hook, once per clone: `git config core.hooksPath .githooks`.

Nothing below needs a Merlin disk image. Capture is the only step that does, and
it is not part of validation.

## Build and run the suite

```powershell
scripts\RunTests.ps1 -Build
```

`RunTests.ps1` does **not** build unless you pass `-Build`. It refuses to run
against a test assembly older than its sources, because a stale run reports a
confident pass against code that is not on disk — and a new test file that never
compiled in is simply absent from the count rather than failing.

Baseline before this feature: **2,961 tests Debug, 2,958 Release**. The
three-test delta between configurations is unexplained and tracked separately as
issue #113 — it is not something this feature introduced or is expected to fix.

Treat those numbers as soft. The Dormann integration tests fetch their source on
demand from inside the test DLL, so when that data is unreachable the suite gets
faster by **doing less work while still reporting a pass** — the same shape as a
run against a stale assembly. Do not re-baseline against a figure measured in
that state, and expect the counts to move when the fix lands elsewhere. A count
that changed for that reason is not a regression here.

Debug takes roughly 15 minutes and Release roughly 2, so **Release is the one to
use while iterating**. For the edit-test loop, `-Filter` narrows the run further:

```powershell
scripts\RunTests.ps1 -Build -Configuration Release -Filter Merlin
```

A bare word is promoted to a fully-qualified-name substring match, so `-Filter
Merlin` catches every Merlin test class. Anything containing filter grammar is
passed to vstest verbatim, so the full expression syntax stays available:

```powershell
scripts\RunTests.ps1 -Filter "FullyQualifiedName~Merlin&Name!~Slow"
```

A filtered run prints a loud banner because it is **not** a suite run — reading a
green partial result as a suite pass is the same class of mistake the staleness
guard exists to prevent. Every phase criterion below that uses `-Filter` still
needs a full-suite run before the phase is considered done.

## Phase exit criteria, as commands

Each phase in [plan.md](./plan.md) has a criterion that is checkable rather than
asserted.

### Phase A — seam extraction changed no behavior

```powershell
scripts\RunTests.ps1 -Build
git diff --stat origin/master -- UnitTest/
```

The suite passes **and** the second command shows no *existing* test file
modified. Phase A moves the AS65 grammar behind the seam without altering it, so
an edit to a test that already existed means behavior moved with it. That is the
signal to stop and find out what changed. Run this before adding any new test
file — new files are expected later in the phase and do not violate the gate.

This is also why the AS65 directive spelling table stays in `DirectiveTable`
rather than moving into the profile: `UnitTest/DirectiveTokenTests.cpp` sweeps
`GetAllSpellings()`, and moving the table would force an edit to exactly the kind
of test this gate protects.

### Phase B — diagnostics name the right file

```powershell
scripts\RunTests.ps1 -Build -Configuration Release -Filter MerlinDiagnosticTests
```

A diagnostic raised inside an included file names that file, not the top-level
input. Existing AS65 diagnostics are unchanged, which the untouched suite proves.

### Phase C — AS65 output is byte-identical

```powershell
scripts\RunTests.ps1 -Build -Configuration Release
```

This is SC-004. Holding both instruction tables must not change a single byte the
existing corpus produces.

### Phase E — the Merlin corpus matches captured bytes

```powershell
scripts\RunTests.ps1 -Build -Configuration Release -Filter MerlinCorpusTests
```

This is SC-001. Every entry meeting the corpus floor assembles to bytes identical
to those captured from real Merlin 8. Nothing here reads a file or invokes another
assembler — sources are compiled-in literals, and multi-file entries are served
by an injected mock reader.

### Phase F — every boundary construct is refused by name

```powershell
scripts\RunTests.ps1 -Build -Configuration Release -Filter MerlinSubsetBoundaryTests
```

The test sweeps the boundary table's accessor rather than a hand-picked sample, so
a row added to the table is covered without anyone editing a test.

### Phase H — a third dialect needs no engine change

```powershell
scripts\RunTests.ps1 -Build -Configuration Release -Filter DialectMechanismTests
git show --stat HEAD -- CassoCore/AssemblySession.cpp CassoCore/ExpressionEvaluator.cpp CassoCore/OpcodeTable.cpp
```

This is SC-009, and the two commands are equally important. The synthetic
test-only profile must pass, **and** the commit that added it must show no change
to the engine, the evaluator, or the opcode tables. A mechanism secretly
hard-coded for two dialects passes every Merlin test and fails exactly here, which
is the cheap way to catch it before 023 finds it the expensive way.

Note the diff is scoped to **the adding commit**, not to `origin/master`. Earlier
phases of this same feature legitimately modify `AssemblySession.cpp` — new
directive rows, dummy sections, per-line instruction tables — so a diff against
master is never empty and would prove nothing. The claim SC-009 makes is about
what *adding a dialect* costs, which is a property of that one commit.

## Before merging to master

Assembler changes require both extended suites, not just the unit tests:

```powershell
scripts\RunDormannTest.ps1
scripts\RunHarteTests.ps1 -SkipGenerate
scripts\Build.ps1 -RunCodeAnalysis
scripts\CheckStyle.ps1
```

`CheckStyle.ps1` inspects commit messages as well as added lines. If it rejects a
push, rephrase — do not reach for `--no-verify`, which switches off every rule
rather than the one that fired.

ARM64 is build-only; no device is available to run tests on, so x64 Debug and
Release green is the bar.

## Capturing a corpus entry

Not part of validation. Offline, one-time per entry, and needs a Merlin 8 disk
image you supply yourself — the image is commercial software and is never
committed, on the same grounds as the gitignored DOS 3.3 master.

```powershell
scripts\CaptureMerlinCorpus.ps1 -Entry <name> -MerlinImage <path>
```

The script assembles the entry under real Merlin 8 running in Casso and writes the
resulting bytes into the generated corpus header, recording the exact Merlin
version alongside them. Only source authored here and the bytes it produced are
committed.

Sanity-check a sample of entries against hand-derived expectations from the Merlin
manual. Agreement confirms the emulation ran Merlin correctly on the day of
capture; disagreement means either a corpus error or an emulator bug, and both are
worth finding.

Bytes come back off the disk with `scripts/ExtractDos33File.ps1`:

```powershell
scripts\ExtractDos33File.ps1 -Image <path>                  # list the catalog
scripts\ExtractDos33File.ps1 -Image <path> -Name OBJ.OUT    # extract one file
```

It works only because the Merlin disk is a flat DOS-order image; it is capture
tooling, not a product feature, and does not stand in for
`020-disk-file-access`'s `disk get`. Delete it when that lands.

Source goes in by typing or pasting into Merlin's editor — and **paste is
verified, not trusted**. Issue #110 reports the guest paste path garbling input,
so save the source back to the disk from within Merlin, extract it, and compare
against what you intended. A clean round trip is the proof; without it, a garbled
paste becomes a captured expectation and the corpus quietly encodes a lie.

Batch constructs into a few composite source files rather than one per construct:
assemble once with the listing on, save the object, extract, split by known
offsets.
