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

`RunTests.ps1` takes no filter argument; it runs the whole assembly. Debug takes
roughly 15 minutes and Release roughly 2, so **Release is the one to use while
iterating**. Where a phase criterion below names a specific test class, run it
directly instead:

```powershell
. scripts\VSTools.ps1
& (Get-VS2026VSTestPath) x64\Release\UnitTest.dll /TestCaseFilter:"FullyQualifiedName~MerlinCorpusTests"
```

Build first — that path is the assembly `RunTests.ps1 -Build` produces, and
invoking vstest directly skips the staleness guard that would otherwise catch a
stale binary for you.

## Phase exit criteria, as commands

Each phase in [plan.md](./plan.md) has a criterion that is checkable rather than
asserted.

### Phase A — seam extraction changed no behavior

```powershell
scripts\RunTests.ps1 -Build
git diff --stat origin/master -- UnitTest/
```

The suite passes **and** the second command shows no test modifications. Phase A
moves the AS65 grammar behind the seam without altering it, so any test edit in
this phase means behavior moved with it. That is the signal to stop and find out
what changed.

### Phase B — diagnostics name the right file

```powershell
& (Get-VS2026VSTestPath) x64\Release\UnitTest.dll /TestCaseFilter:"FullyQualifiedName~MerlinDiagnosticTests"
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
& (Get-VS2026VSTestPath) x64\Release\UnitTest.dll /TestCaseFilter:"FullyQualifiedName~MerlinCorpusTests"
```

This is SC-001. Every entry meeting the corpus floor assembles to bytes identical
to those captured from real Merlin 8. Nothing here reads a file or invokes another
assembler — sources are compiled-in literals, and multi-file entries are served
by an injected mock reader.

### Phase F — every boundary construct is refused by name

```powershell
& (Get-VS2026VSTestPath) x64\Release\UnitTest.dll /TestCaseFilter:"FullyQualifiedName~MerlinSubsetBoundaryTests"
```

The test sweeps the boundary table's accessor rather than a hand-picked sample, so
a row added to the table is covered without anyone editing a test.

### Phase H — a third dialect needs no engine change

```powershell
& (Get-VS2026VSTestPath) x64\Release\UnitTest.dll /TestCaseFilter:"FullyQualifiedName~DialectMechanismTests"
git diff --stat origin/master -- CassoCore/AssemblySession.cpp CassoCore/ExpressionEvaluator.cpp CassoCore/OpcodeTable.cpp
```

This is SC-009, and the two commands are equally important. The synthetic
test-only profile must pass, **and** adding it must not have touched the engine,
the evaluator, or the opcode tables. A mechanism secretly hard-coded for two
dialects passes every Merlin test and fails exactly here, which is the cheap way
to catch it before 023 finds it the expensive way.

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

The two disk steps — source onto the Merlin disk, bytes back off it — are
`020-disk-file-access` capabilities. Until that lands, an external disk tool covers
them. Convenience only; nothing functional depends on it.
