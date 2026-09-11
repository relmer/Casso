# Measurements: Thin Executable, Testable Core

**Feature**: 031-thin-exe-shim | **Requirements**: FR-012, SC-001, SC-002, SC-003a, SC-007

Every slice records its before and after here, so progress is observable rather
than asserted and a later reader can check it. Produced by
`scripts/MeasureExtraction.ps1 -Markdown`.

## Method

`.cpp` and `.h` lines under each project directory, excluding build output
(`x64/`, `ARM64/`, `Debug/`, `Release/`) and vendored `External/`. Functions are
counted in the executable projects only, where the expected end state is zero
and any non-zero result is read by a person.

Two counting notes, recorded so the figures are not mistaken for drift:

- The line counts land within three lines of the specification's Context table
  (`Casso` 93,928 against 93,927; `CassoCore` 33,410 against 33,409;
  `CassoEmuCore` 69,247 against 69,244). The difference is whether a file
  without a trailing newline contributes a final line. `CassoCli` and `Dxui`
  match exactly, and every file count matches exactly.
- The first run of the counter reported 4,124 functions in `Casso`, because it
  counted every `if` and `for` whose brace sat on the next line. The counter now
  skips keywords that take parentheses. Recorded because a measurement that is
  wrong in the direction of looking impressive is worth flagging.

## Branch point (2026-09-09)

Commit `60a86e83`, before any code moved.

| Project | Files | Lines | Functions |
|---|---:|---:|---:|
| `Casso` (exe) | 189 | 93,928 | 1,235 |
| `CassoCli` (exe) | 2 | 57 | 1 |
| `CassoCore` | 78 | 33,410 | n/a |
| `CassoEmuCore` | 291 | 69,247 | n/a |
| `Dxui` | 130 | 45,523 | n/a |

Dual-compiled exe sources in `UnitTest.vcxproj`: **38**

`Casso.vcxproj` holds 86 `ClCompile` and 101 `ClInclude` entries, one
`ResourceCompile` (`Casso.rc`), and the `Shaders.targets` import at line 473.
Neither executable project sets `EntryPointSymbol`.

### Targets

| Measure | Branch point | Target |
|---|---:|---:|
| `Casso` lines of code | 93,928 | 0 |
| `Casso` functions | 1,235 | 0 |
| `CassoCli` functions | 1 | 0 |
| Dual-compiled sources | 38 | 0 |
| `UnitTest` -> `Casso.vcxproj` reference | present | removed |

## Per-slice record

Each slice appends a row on completion.

| Slice | Story | `Casso` lines | `Casso` functions | Dual-compiled | Receiver lines |
|---|---|---:|---:|---:|---:|
| — | baseline | 93,928 | 1,235 | 38 | 69,247 |
| 0 | machine hierarchy | 93,928 | 1,235 | 38 | 69,247 |
| 1 | Config + tests | 86,128 | 1,187 | 31 | 77,054 |
| 9 | machine definitions | 86,128 | 1,187 | 31 | 77,638 |
| 2 | strays (partial) | 84,151 | 1,150 | 28 | 78,270 |
| 4-7 | the whole program moves | 944 | 4 | 1 | 160,410 |
| 8 | linker targets | **5** | **0** | **0** | 161,273 |

### Target status

| Measure | Branch point | Target | Now |
|---|---:|---:|---:|
| `Casso` lines of code | 93,928 | 0 | **0** (5 comment lines) |
| `Casso` functions | 1,235 | 0 | **0** |
| `CassoCli` functions | 1 | 0 | **0** |
| Dual-compiled sources | 38 | 0 | **0** |
| `UnitTest` -> `Casso.vcxproj` | present | removed | **removed** |

SC-001, SC-002, SC-002a and SC-003a are met. Each executable project holds one
comment-only translation unit and its resource script, and names the CRT
startup symbol so the linker recovers the entry point from the library.

### Slice 2 notes

Two of the nine strays did not move. `InputDebugDialogState` includes
`Widgets/DxuiListView.h`, and `AssetBootstrap` (3,631 lines) reaches into eight
Dxui headers including dialogs and panels. `CassoEmuCore` does not reference
`Dxui` and will not until slice 5 moves `Casso/Ui/`, so both wait for it rather
than forcing the reference early for two files.

### Slice 1 notes

The coverage obligation (FR-009) was already largely met, by dual compilation
rather than by linking. `CrtResolverTests`, `GlobalUserPrefsTests`,
`MonitorCatalogTests`, `WindowPlacementProfileTests`, `MachineInputPrefsTests`
and `UserConfigStoreTests` all existed and all now link the library instead of
compiling a second copy. Only two of the story's acceptance scenarios were
genuinely unasserted, and only those two tests were added; padding the rest
with restatements of existing coverage would make FR-009 look satisfied without
making anything safer.

### Slice 0 notes

Every figure is unchanged, which is the result FR-005e asks for: the sweep
relocated 100 files and altered no line of code. What changed is where the
files are, and that is not a number this table can show. The countable part of
the slice is the relocation itself:

| Destination | Files |
|---|---:|
| `Machines/Apple2/Common/` | 88 |
| `Machines/Apple2/Apple2e/` | 6 |
| `Machines/Apple2/Apple2c/` | 2 |
| `Seams/` | 5 |

**Three of the five Apple II models need no code of their own.**
`Machines/Apple2/` has directories for `Apple2e` and `Apple2c` only;
`Apple2`, `Apple2Plus` and `Apple2eEnhanced` are fully described by their
definitions under `Resources/Machines/` plus the family's shared code. Empty
directories are not created for them, since git does not track a directory and
an empty one would assert a distinction that does not exist. The contract's
check therefore reads in one direction only: every model directory that exists
matches a definition, not every definition has a directory.

## Final measurement (T104)

Taken at the end of the extraction, for closing issue #85.

| Project | Files | Lines | Functions |
|---|---:|---:|---:|
| `Casso` (exe) | 1 | 5 | **0** |
| `CassoCli` (exe) | 1 | 5 | **0** |
| `CassoCore` | 78 | 33,410 | n/a |
| `CassoEmuCore` | 481 | 161,273 | n/a |
| `Dxui` | 130 | 45,523 | n/a |

Dual-compiled exe sources in `UnitTest.vcxproj`: **0**

The five lines in each executable are the comment-only translation unit. Each
project also holds its resource script and names its CRT startup symbol.

### Verification (T102, T102a-c, T103, T107)

| Check | Result |
|---|---|
| `CassoCore` upward includes | none; and it references no other project |
| Platform API named as a placement reason | none in any commit on the branch |
| New logic added outside core while in flight | none; every new file landed in `CassoEmuCore` or `UnitTest` |
| Unrelated feature work folded in | none; all 28 commits are 031 |
| ARM64 Debug | builds |
| x64 Debug | builds, 4,989 tests pass |
| x64 Release | builds, 4,987 tests pass |
| `CheckStyle -Mode Tree` | clean over 1,357 files |
| Both executables run | //e launches to its desk scene; `CassoCli --version` answers |

## What placement did not buy

Issue #85 asked for the code to live where tests can reach it, and it now does.
That is necessary and it is not sufficient: a file linked into `UnitTest` is
reachable, but a function that can only be called through a live
`EmulatorShell` is still not drivable.

`MachineManager::SwitchMachine` is the clearest case. Its teardown reads
`m_shell.m_disk2DebugPanel`, `m_shell.m_inputDebugPanel` and the whole
`m_refs` observer struct, so asserting that a debug panel re-attaches to the
new controller after a switch -- the shipped fix User Story 4 wanted guarded --
means constructing a shell, which means a window. The code is in core; its
shape has not changed.

So the extraction's second half is real work that remains: splitting the
lifecycle, the layout math, the pacing gate and the input mapping out of the
shell into functions that take their inputs. User Stories 3 and 6 describe
exactly that, and their acceptance scenarios are the measure of it. This
document records that the line-count goal was met without it, so a later reader
does not mistake one for the other.

What DID gain coverage, because it was already shaped for it: the machine
definitions, the hardware-merge refusals, `PerfStats`, the Config edge cases,
and the CRT chain through a WARP device. What was already covered and now
reaches its subject by linking rather than by dual compilation: the disk and
input debug projections, the track/sector predicate, settings pages, desk-scene
layout, chrome widgets and the reset/power-cycle semantics.

## Deferred, with reasons

- **The disk-layer split (Phase 13).** All four files have a reusable core, but
  the split's correct shape depends on what a 1541 or an 810 actually needs
  from a track buffer and a mount coordinator, and neither exists to check
  against. Guessing now would bake a second set of assumptions in beside the
  Apple II ones it removes.
- **T114, removing the ignored keys from the machine JSON.** The values are
  already overridden in code, so the hole is closed; what remains is a file
  that declares fields the program ignores. Removing them needs a
  `$cassoMachineVersion` bump, a `MachineConfigUpgrade` path, and
  `SettingsPanelState` reading its hardware tree from the definitions.
- **The checked-in golden images (FR-008a).** Blessing a golden means looking
  at it and deciding it is right.

