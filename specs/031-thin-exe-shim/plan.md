# Implementation Plan: Thin Executable, Testable Core — `Casso.exe`

**Branch**: `031-thin-exe-shim` | **Date**: 2026-09-09 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/031-thin-exe-shim/spec.md`

## Summary

`Casso.exe` holds 93,927 lines across 189 files in violation of Principle VI.
This plan moves all of it into `CassoEmuCore`, leaving an executable project
that contains a resource script, a resource header, and one comment-only
translation unit — the shape `TCDir` already ships. `CassoCli.exe`'s surviving
`main` goes the same way at the end.

The work runs as nine slices on one long-lived branch. A tenth thing happens
first: the constitution is amended, because Principle VI as ratified still
grants a GUI executable the entry point, the window, its message pump and the
device objects, all of which FR-003 and FR-004 overrule.

Two decisions shape everything after. The `CassoCore` / `CassoEmuCore` split is
retained, because `CassoCore` has no upward include today and merging the
libraries would preserve every dependency while making its direction
unenforceable. And a `Machines/<Family>/<Model>/` hierarchy is swept across the
whole tree first, so the 93,927 relocated lines land in their final home instead
of being moved twice.

Phase 0 turned up something the specification had wrong, and it changes the
work: the exe is not untested. Thirty-eight of its `.cpp` files are compiled a
second time into `UnitTest.dll` through `<ClCompile Include="..\Casso\...">`
entries. That is the workaround the missing library boundary forced, and
retiring it is a per-slice obligation and a second progress metric.

## Technical Context

**Language/Version**: C++, `stdcpplatest`, MSVC v145+

**Primary Dependencies**: Windows SDK, C++ STL, Direct3D 11 / Direct2D /
DirectWrite / DirectComposition / WIC via `Dxui`

**Storage**: JSON preferences and machine definitions on disk, behind the
`IFileSystem` seam

**Testing**: Microsoft C++ Unit Test Framework, `UnitTest` project

**Target Platform**: Windows 10/11, x64 and ARM64

**Project Type**: Desktop application plus console tool over shared static
libraries

**Performance Goals**: unchanged. This is a relocation; FR-007 forbids
user-visible change, and that includes how the emulator feels.

**Constraints**: behavior-preserving except for defects the extraction exposes;
Test Isolation forbids real files, registry, network, processes or system APIs
in tests; each slice leaves the branch building and green in Debug and Release
on x64, with ARM64 compiling.

**Scale/Scope**: 93,927 lines out of `Casso/`; roughly 200 further files
relocated by the machine sweep; 38 dual-compile entries retired; two executable
projects reduced to zero functions.

## Constitution Check

*GATE: evaluated before Phase 0 and re-evaluated after Phase 1.*

### Principle VI — Thin Executable, Testable Core (NON-NEGOTIABLE)

**Status: CONDITIONAL PASS. The condition is an amendment this plan schedules
as its first commit.**

This is the principle the feature exists to satisfy, and recording a bare PASS
here would be exactly the failure FR-013 describes. The honest position:

Principle VI's **What Actually Stays** currently reads, "For a GUI application
it is the entry point, the `HWND` and its message pump, and the graphics/audio
device objects." FR-003 and FR-004 overrule all four. As ratified, the
constitution would license every exemption this feature exists to remove, and
any later slice could be argued against it.

The specification resolves this in FR-005a: Principle VI is amended before the
first slice is implemented, so that an executable is granted no code at all.
The amendment is the first commit on the branch. `TCDir` is the evidence —
`TCDir/Main.cpp` is five lines of comment, its `wmain` is at
`TCDirCore/TCDir.cpp:226`, and every configuration names
`wmainCRTStartup` as its entry point symbol.

**What this plan verifies for Principle VI, per FR-013:**

- Receiving library: `CassoEmuCore` for everything leaving `Casso/`. It already
  holds `CassoCli.exe`'s entire program in `Cli/`; the graphical program becomes
  its sibling.
- Staying in the exe: nothing. Not the window, not the pump, not the device
  objects, not the entry point.
- Placement reasoning: UT-reachability only. No platform API is named as a
  justification anywhere in this plan, per FR-005.

### Principle II — Testing Discipline

**Status: PASS, and the feature's main product.**

FR-009 requires tests in the same slice as the move; FR-010 binds them to Test
Isolation. Story 7's render tests need a WARP device and a readback path that do
not exist yet, built before the render chain is extracted; WARP needs no display
and no window, so isolation holds.

One tension to record rather than gloss: FR-008a's golden images are files. They
are test fixtures, which the constitution explicitly distinguishes from
dependencies, and the tree already reads fixture data in the Harte vectors. They
are read through the same seam as the rest of the suite.

### Principle I — Code Quality, and Principle V — Simplicity

**Status: PASS with one recorded deviation.**

Relocation preserves formatting, EHM patterns and function comments; moved
functions carry their `////` banners, and `CheckStyle.ps1 -Mode Tree` runs at
every slice boundary because relocation is precisely what orphans a banner.

**Deviation**: Principle V limits changes to files explicitly required. User
Story 0 touches roughly two hundred files this extraction would not otherwise
have opened. Justified in the specification's Assumptions: applying the
hierarchy only to relocated files leaves `Devices/` split between two
conventions with no rule a reader could infer, and the sweep has to happen at
some point regardless, at which time it would cost strictly more.

### Principle III — User Experience Consistency

**Status: PASS.** No user-visible change is intended (FR-007). Command-line
behavior is untouched; the switch-table refactor discussed during clarification
is a separate feature and is not in this plan.

### Principle IV — Performance

**Status: PASS.** No hot path changes shape. Static libraries discard
unreferenced members, so moving code between them does not grow either binary.

### Technology Constraints

**Status: PASS.** No new third-party dependency. WARP is part of the Windows
SDK.

### Post-Phase 1 re-evaluation

Unchanged from the above. Phase 1 introduced no new constitutional question. The
R2 finding strengthens the Principle II position rather than weakening it: the
exe's logic is currently reachable only by compiling it twice, which is the
symptom the principle predicts.

## Project Structure

### Documentation (this feature)

```text
specs/031-thin-exe-shim/
├── plan.md                          # This file
├── spec.md
├── research.md                      # Phase 0
├── data-model.md                    # Phase 1
├── quickstart.md                    # Phase 1
├── contracts/
│   ├── executable-project.md        # What an exe may contain, and the linker mechanism
│   └── machine-layout.md            # The Machines/<Family>/<Model> rule
├── checklists/
│   └── requirements.md
└── tasks.md                         # Phase 2, from /speckit-tasks
```

### Source Code (repository root)

```text
CassoCore/                  toolchain: assembler, dialects, instruction set,
                            command line, shared plumbing. Receives nothing.
                            Keeps its one-way boundary.

CassoEmuCore/
├── Core/                   machine-generic framework (unchanged by this work)
├── Devices/                machine-neutral devices and chips
├── Machines/               NEW - Story 0
│   └── Apple2/
│       ├── Common/
│       ├── Apple2/
│       ├── Apple2Plus/
│       ├── Apple2e/
│       ├── Apple2eEnhanced/
│       └── Apple2c/
├── Video/  Audio/  Render/  Capture/
├── Cli/                    CassoCli.exe's whole program (existing)
├── Config/                 NEW - Story 1, from Casso/Config/
├── Shell/                  NEW - Stories 3, 4, 6
├── Ui/                     NEW - Story 5
│   ├── Chrome/  Dialogs/  Scene/  Settings/  Debug/
├── Print/                  NEW - Story 4
├── Shaders/                NEW - Story 7, with Shaders.targets
├── Seams/                  NEW - Win32DiskFileIo, Win32IntentChannel, out of Cli/
└── Gui/                    NEW - Story 8, holds wWinMain

Dxui/                       out of scope; already a tested library

Casso/                      -> resource script, resource header,
                               comment-only Main.cpp
CassoCli/                   -> the same three shapes
MeshCreator/                untouched; build-time tool

UnitTest/                   gains tests per slice; loses 38 dual-compile
                            entries, the ..\Casso include path, and the
                            ProjectReference to Casso.vcxproj
```

**Structure Decision**: Two libraries, retained. `CassoCore` is the
machine-independent toolchain and receives nothing from this work;
`CassoEmuCore` is the emulated machine and everything the running application is
built from, and receives all 93,927 lines. The split stands because it is the
only architectural boundary in the tree a build can enforce, and because
`CassoCore`'s zero upward includes prove it currently holds. Within
`CassoEmuCore`, the GUI program becomes a set of sibling directories to `Cli/`,
which already demonstrates that one executable's entire program fits in a
subdirectory. Machine-specific code moves under `Machines/<Family>/<Model>/`
with a per-family `Common/`, swept across the whole tree before any extraction
begins.

## Slice sequence

| # | Story | Content | Depends on |
|---|---|---|---|
| — | — | Amend Principle VI (FR-005a) | — |
| 0 | 0 | Machine hierarchy swept across the tree | amendment |
| 1 | 1 | `Casso/Config/` with its `IFileSystem` seam | 0 |
| 2 | 2 | The already-pure strays | 0 |
| 3 | 3 | Layout, pacing, input mapping out of `EmulatorShell` | 1, 2 |
| 4 | 4 | `Casso/Shell/` managers, `Casso/Print/` | 1, 2 |
| 5 | 5 | `Casso/Ui/` state logic | 1-4 |
| 6 | 6 | `EmulatorShell` entire, window and pump included | 3, 5 |
| 7 | 7 | Render chain, shaders, audio mixing, WARP harness | 5 |
| 8 | 8 | Both executables become linker targets | all |

Slice 0 is fixed in place; the rest may be reordered if one becomes cheap, so
long as each leaves the branch building and green.

## Delivery

One long-lived branch. Each slice is committed and pushed when it completes, and
more often where the work divides naturally. No slice merges to master
individually. The branch merges once, after the whole specification is
implemented and tested, and only when the owner asks for that merge and
approves it (FR-006).

Master is merged **into** this branch at every slice boundary at least. Four
sweeping renames have landed on master since August 2026; they merge cleanly and
then fail to compile, so the textual conflict count understates the work every
time.

## Complexity Tracking

| Deviation | Why it is necessary | What was rejected |
|---|---|---|
| User Story 0 touches ~200 files outside the extraction's scope | A hierarchy applied to half a directory teaches no rule, and the sweep costs strictly more if deferred | Applying the hierarchy only to relocated files |
| Golden images compared pixel-exact, brittle across WARP revisions | Owner's decision; tolerances are too weak an assertion for the code the slice exists to cover | Per-channel epsilon; computed structural expectations |
| A constitution amendment precedes the feature | Principle VI as ratified licenses the exemptions this feature removes | Running as a documented deviation; amending last |
