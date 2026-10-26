# Implementation Plan: Machine History in the Emulator

**Branch**: `045-emulator-history` | **Date**: 2026-10-26 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/045-emulator-history/spec.md`

## Summary

History (the keyframes, the input journal, rewinding and the timeline) moves out of the debugger into a new shell component, `ShellHistory`, so the emulator window can use it with the debugger closed. The emulator window gains a rewind key (F2, while it is down), a rewind bar that opens over the bottom of the screen, a History menu, and a pulsing replay indicator. Run in the past replays the recorded future; the first change made behind live forks history silently, and the future left behind becomes a branch, drawn as a fork mark with a flyout. Recording continues at every speed. The keyframe store becomes a tree of segments over one arena, presenting the active path to the existing controller as one sequence (research R-004). Every save of the machine includes its history, in a new section of the state file (R-010). The disk rule, under which each image file contains the active branch's version, is built last, on 041-disk-integrity's restructured disk store, after 041 is on master (R-011).

The first research item measured history's cost on ordinary software: 0.03% to 0.19% of one core at 1x, far under SC-001a's 1% (R-001).

## Technical Context

**Language/Version**: C++ (`/std:c++latest`, MSVC v145)

**Primary Dependencies**: Windows SDK and the C++ STL; Dxui (the tree's Direct2D and DirectWrite UI library); the Windows Compression API (XPRESS in the keyframe store, MSZIP in state files). No new dependency.

**Storage**: the machine state file (`.cassostate`, `MachineStateFile`), format version 4 with an optional history section (contracts/saved-history-format.md); the user preferences JSON (`GlobalUserPrefs`).

**Testing**: Microsoft C++ Unit Test Framework in `UnitTest` (no real files, as the constitution requires); `ScenarioTests` for real DOS 3.3 boots, the cost measurement, persistence timing and, after 041, the disk rule over real guest writes.

**Target Platform**: Windows 10 and 11, x64 and ARM64 (ARM64 build-only; no device).

**Project Type**: desktop application over core static libraries (`CassoEmuCore`, `Dxui`); the executable projects contain no code.

**Performance Goals**: history at no more than 1% of one core at 1x on ordinary software (SC-001a, measured: under 0.2%); no measurable cost per frame added by this feature (SC-001b); rewinding starts within 100 ms and draws every frame at normal speed, 60 per second (SC-002); a full 64 MB history loads in no more than 1 s (SC-007); the rewind bar opens within 150 ms (SC-008).

**Constraints**: steady recording allocates nothing on the machine thread (035's rule; the extent allocator allocates nothing in steady recording); all branches share one budget (FR-016); the guest never receives the rewind key or a click on the bar; no 045 change to the disk store before 041 is on master.

**Scale/Scope**: up to 65,536 keyframes at the default budget, as today; at most 8 branches left behind; history files up to about the budget plus the machine state.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Gate | Status |
|---|---|---|
| I. Code Quality | EHM in every fallible function, single exit, declarations at the top, the tree's spacing and alignment rules | Pass, with no exception |
| II. Testing Discipline | Every new path driven by `UnitTest` with no system state: the store tree, the journal moves, fork and switch determinism, the strip's marks and flyout, key routing, the off confirmation, the history section's parse and discard; real files only in `ScenarioTests` | Pass |
| III. User Experience Consistency | Sentence-case labels, the existing command table for the menu, no new prompts (forks are silent), the confirmation for turning history off | Pass |
| IV. Performance | Measured cost first (R-001); no per-frame cost added by this feature (SC-001b, re-measured at the end); no allocation in steady recording | Pass |
| V. Simplicity | Branches confined to the store and the journal; the controller and replayer use a linear interface, as today; one strip control for both windows | Pass, with one structural change justified under Complexity Tracking |
| VI. Thin Executable, Testable Core | Everything in `CassoEmuCore` and `Dxui`; `Casso.rc` gains only accelerator rows for Ctrl+F11 and Ctrl+F2 | Pass |

**Post-design re-check (Phase 1)**: Pass. The data model and contracts add no code to an executable, every behavior in the contracts has a listed unit or scenario test, and the extent allocator is the only change that adds a mechanism.

## Project Structure

### Documentation (this feature)

```text
specs/045-emulator-history/
├── spec.md
├── plan.md                                  # this file
├── research.md                              # R-001 to R-014
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── saved-history-format.md
│   ├── timeline-strip.md
│   └── emulator-history-commands.md
├── checklists/requirements.md
└── tasks.md                                 # /speckit-tasks
```

### Source code (repository root)

```text
CassoEmuCore/
├── Shell/Components/
│   ├── ShellHistory.h/.cpp                  # new: history's owner in the shell (R-003)
│   └── ShellChrome.h/.cpp                   # the rewind bar overlay and the replay indicator
├── Shell/
│   ├── ShellDebugger.h/.cpp, ShellDebuggerReverse.cpp   # history members and functions move out
│   ├── EmulatorShell*.cpp, MachineManager.cpp           # start, stop, save and load through ShellHistory
│   ├── EmulatorShellState.cpp                           # state load: replace history, or a marked change
│   ├── MachineStateFile.h/.cpp                          # format version 4, the history section
│   ├── SavedHistory.h/.cpp                              # new: the section's writer and reader
│   ├── Input/ShellKeyRouting.h/.cpp                     # ShellKeyOwner::Rewind
│   └── Window/EmulatorWindowInput.cpp                   # F2 press and release, the bar's pointer band
├── Debugger/Reverse/
│   ├── KeyframeStore.h/.cpp                 # segments, the extent allocator, eviction order
│   ├── ArenaExtents.h/.cpp                  # new: the free-extent list
│   ├── InputJournal.h/.cpp                  # moving records at a fork and a switch
│   ├── ReverseController.h/.cpp             # fork in place of the cut, live-end keyframe, switch, no Maximum pause
│   ├── ReverseHost.h/.cpp                   # Fork, SwitchBranch, the rewind loop
│   ├── DivergenceGate.h/.cpp                # fork in place of the question
│   └── HistoryThumbnails.h/.cpp             # marks, fork rows, PathChanged
├── Debugger/HeatHistory.cpp, CallStackHistory.cpp       # follow PathChanged
├── Shell/DebugEventLogs.cpp                             # follows PathChanged
├── Config/GlobalUserPrefs.h/.cpp, DebuggerSettings.h    # HistorySettings, the old keys read
├── Ui/Settings/GeneralPage*.h/.cpp                      # the History group
├── Ui/Debugger/HistoryOptionsPage.h/.cpp                # the same settings
├── Ui/Chrome/EmulatorCommands.cpp, resource.h           # the History menu
└── Devices/Disk/DiskImageStore.h/.cpp                   # after 041 only: branch versions, pruning (R-011)
Dxui/Widgets/
├── IDxuiImageStripSource.h                  # marks and fork rows
└── DxuiImageStrip.h/.cpp                    # painting and hit-testing marks, the flyout
Casso/Casso.rc                               # accelerator rows only
UnitTest/EmuTests/, UnitTest/Dxui/           # the unit tests listed in each contract
ScenarioTests/
├── HistoryCostTests.cpp                     # R-001's measurement
├── SavedHistoryScenarioTests.cpp            # new: persistence over a real boot, load timing
└── BranchDiskScenarioTests.cpp              # new, after 041: the disk rule over real DOS 3.3 writes
```

**Structure Decision**: the existing layout. The feature's new types go beside the history code they extend (`Debugger/Reverse/`), the shell component beside the others (`Shell/Components/`), and the strip's additions in `Dxui`.

## Build Order

The phases order the work; the feature merges to master whole, after 035, in one merge.

1. **Setup**: commit `HistoryCostTests`; merge `origin/035-debugger` for its latest commits; investigate R-002's growth rate.
2. **Ownership and settings** (Story 5's foundation): `ShellHistory` takes history out of `ShellDebugger`; `HistorySettings` and the General page group; recording at Maximum speed (R-007).
3. **The emulator window** (Story 1): the strip's marks, the rewind bar overlay, F2 and the rewind loop with its SC-002 measurement, the History menu and keys, the replay indicator, the off confirmation.
4. **Forking** (Story 2, Story 5): the extent allocator and the segment tree; journal moves; fork in place of every cut; the live-end keyframe; branch switches; eviction order and the cap; the fork marks and flyout; the debugger's consumers following `PathChanged`.
5. **Persistence** (Story 4): format version 4 and the history section; save at exit, at a switch and from File > Save state; load at launch and from a state file; discard rules; the resume position; load and save timing.
6. **The disk rule** (Story 3): once 041 is on master, merge master into 045, then branch disk versions, the writes at a fork and a switch, forced writes at the live end, pruning by referenced media, and the scenario test.
7. **Polish**: SC-001b re-measured; `docs/` and help for the new keys; CHANGELOG and README entries for the owner's approval, last.

## Coordination with 041-disk-integrity

041 merges to master before 045 (owner decision, 2026-10-26). Until master with 041 is merged into 045, no 045 task edits `DiskImageStore`, `MountedImageState`, `CommitPlan` or `Disk2Controller`, so 041's merge has no 045 conflict there. Phases 2 to 5 depend on the store only through its existing public interface (`SetFlushHold`, `SetReplaying`, `SeatMedia`, `GetMediaId`, `SetMediaRetention`). Phase 6 builds on what 041 brings (R-011): the split `FlushEntry` and `DurableCommit`, `FlushMoment::Background`, the single owning thread, and FR-017's reset that writes only through the store, which FR-019 depends on. If 041 is not on master when Phase 5 ends, Phase 6 waits, and the feature does not merge without it.

## Decisions for the Owner's Review

- **Keys** (R-014): Step back on Ctrl+F11 and Go live on Ctrl+F2, so no modifier the guest can read is pressed.
- **History from before a disk changed outside Casso** (R-010): viewable and replayable, and a change made there is ignored with a notice, so no fork writes an old version over the changed file.
- **Exit behind live** (R-010): the next launch returns to the position on screen, paused.
- **R-002**: the disk-read growth rate is investigated in Phase 1, and its fix goes to 035 if the cause is a device's state.

## Complexity Tracking

| Change | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| The arena managed by a free-extent list, in place of a ring with a head and a tail | Branches left behind are dropped before the active path (FR-016), which frees space in the middle of the arena | The ring cannot reuse space until its head passes it, so the required eviction order would free nothing; copying branches out of the arena exceeds the budget (R-004) |
