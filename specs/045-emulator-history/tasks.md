# Tasks: Machine History in the Emulator

**Input**: Design documents from `specs/045-emulator-history/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: required. The constitution (Principle II) requires a test for every public function and significant path, and `UnitTest` cases touch no real file; real files and real guests go in `ScenarioTests`. Every new test is seen to fail first: revert the fix, or stub the new function, confirm the assertion message, restore (`.github/copilot-instructions.md`, "Verify a new test fails without the fix").

**Organization**: phases follow the plan's build order, not the stories' priorities. Story 3 (P1) is built last because its disk rule waits for 041-disk-integrity to be on master (research R-011). The feature merges to master whole, after 035, in one merge.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on an unfinished task)
- **[Story]**: US1 to US5, as in spec.md

## Rules for every task

- Read `.github/copilot-instructions.md` before the first code change of a session; follow its spacing, alignment, EHM and include rules.
- Run `pwsh -File C:/Users/relmer/.claude/scripts/Check-Prose.ps1` on every new comment block, commit message, CHANGELOG and README line.
- Run `scripts/CheckStyle.ps1 -Mode Staged` before committing a new file, and `scripts/CheckStyle.ps1 -Mode Tree` before every merge.
- Commit after each phase at least; subjects only, `type(scope): Imperative ...`.
- Never edit `CassoEmuCore/Devices/Disk/DiskImageStore.h/.cpp`, `MountedImageState`, `CommitPlan` or `Disk2Controller` before T070 (R-011).
- Never write "watchpoint"; the debugger has breakpoints and watches.
- 045 has no GitHub issue, so commits have no `Refs` line.

---

## Phase 1: Setup

**Purpose**: the measurement baseline and the latest 035.

- [X] T001 Add `ScenarioTests/HistoryCostTests.cpp` to `ScenarioTests/ScenarioTests.vcxproj` and record R-001's measurement in `specs/045-emulator-history/research.md` (done 2026-10-25, `b6f6b41dd`)
- [X] T002 Merge `origin/035-debugger` into `045-emulator-history` (merge, never rebase), resolve conflicts, build Debug and Release, run `scripts/RunTests.ps1 -Build`, and push; record the merged 035 commit here (done: `a7c88fa4d` merged at `27938b4dc`, Release suite passed)
- [X] T003 R-002: in a scratch test beside `ScenarioTests/HistoryCostTests.cpp`, run the `VERIFY HELLO` loop for 60 emulated seconds and log, per keyframe, `isWhole`, `stateBytes` and `storedBytes`; then diff two consecutive states section by section (`StateWriter` tags) to find which section changes size or content; record the cause and the decision in research.md R-002
- [X] T004 R-002 follow-up: if T003 finds a defect in history code (`CassoEmuCore/Debugger/Reverse/KeyframeStore.cpp`), fix it with a regression test in `UnitTest/EmuTests/KeyframeStoreTests.cpp`; if it is in a device's state, write the finding up for 035 in `specs/045-emulator-history/research.md` R-002 and tell the owner; then rerun `HistoryCostTests` and update R-001's table

---

## Phase 2: Foundational (ownership and settings)

**Purpose**: history as a shell component with general settings, before any story builds on it (R-003, R-007, R-012).

**⚠️ CRITICAL**: no story phase begins until this phase passes the full Debug suite.

- [X] T005 Create `CassoEmuCore/Shell/Components/ShellHistory.h/.cpp` and add them to `CassoEmuCore/CassoEmuCore.vcxproj` and its filters: a class constructed by `EmulatorShell` with the `MachineHost &`, `CpuManager &` and an `IHistoryHost` seam (new, in `ShellHistory.h`) for posting commands and dialogs; no members moved yet (done: `ShellHistory` takes the shell's existing `IDebuggerHost`, which already covers what it posts and displays, in place of a new seam; the debugger side is the new `IHistoryClient`, in `CassoEmuCore/Shell/IHistoryClient.h`, which `ShellDebugger` implements)
- [X] T006 Move from `CassoEmuCore/Shell/ShellDebugger.h` into `ShellHistory`: `m_reverseHost`, `m_isReverseOn`, `m_reverseBudgetMb`, `m_reverseIntervalFrames`, `m_lastReverseOutcome`, `m_lastReversePosition`, `m_historyRenderer`, `m_historyThumbnails`, `m_divergenceGate`, `m_heldInputWatch`, `m_isResumeOwedAfterHeldRead`, `m_pasteLengthBeforeHold`, `m_isJoyportSyncOwed`, `m_deferredEdits`, the replay caption and its mutex, `m_isReplayingHistory`, `m_replayStartedAt`, `m_replayControl`, the `m_seekCycle*` atomics and `m_isNetworkFenced` with `FenceHistoryForNetwork` (035's Uthernet II fence, moved as 035 built it: no confirmation, the setting untouched); move the functions in `CassoEmuCore/Shell/ShellDebuggerReverse.cpp` that use only these into `ShellHistory.cpp`, and pass a `ShellHistory &` to `ShellDebugger`'s constructor for the rest (heat map, call stack and event log replays, reverse continue's stop test) (done)
- [X] T007 Repoint every caller of the moved functions to `ShellHistory`: `CassoEmuCore/Shell/EmulatorShellCpuThread.cpp` (start and stop recording, `RecordHistoryFrame`, `ServiceHistoryThumbnails`), `EmulatorShellState.cpp`, `MachineManager.cpp`, `Window/EmulatorWindow.cpp`, `Window/EmulatorWindowInput.cpp`, `CpuCommandDispatcher.cpp`, `WindowCommandManager.cpp` and the debugger window's `IDebuggerWindowHost` members (`GetHistoryThumbnails`, `SeekHistoryCycle`, `IsHistorySeekBusy`, `IsReplayingHistory`, `GetReplayProgress`, `StopReplay`); no behavior change; full Debug suite passes (done)
- [X] T008 [P] Move the existing reverse tests that construct `ShellDebugger` only for history onto `ShellHistory`, in `UnitTest/EmuTests/` (find them with `rg -l "ShellDebugger" UnitTest/EmuTests` among those calling `RunReverseCommand`, `HoldInputBehindLive` or `GetHistoryStatus`); add `UnitTest/EmuTests/ShellHistoryTests.cpp` with recording started and stopped with the debugger window never created (done: the shell-level tests use the shell, so only their calls moved; `UnitTest/EmuTests/ShellHistoryTests.cpp` added)
- [X] T009 Add `HistorySettings` (`isRecording` default `true`, `budgetMb` default 64, `intervalFrames` default 10) to `CassoEmuCore/Config/GlobalUserPrefs.h`, move `kMinReverseBudgetMb`, `kMaxReverseBudgetMb` and `kMaxReverseIntervalFrames` with them, and remove the three fields from `CassoEmuCore/Config/DebuggerSettings.h`; in `GlobalUserPrefs.cpp` write `historyRecording`, `historyBudgetMb` and `historyIntervalFrames`, and read the old `reverseRecording`, `reverseBudgetMb` and `reverseIntervalFrames` keys only when the new ones are absent (done, with one change: the JSON keys stay `reverseRecording`, `reverseBudgetMb` and `reverseIntervalFrames`, since several builds share one preferences file and new keys would set them apart; `HistorySettings` is in `CassoEmuCore/Config/HistorySettings.h`)
- [X] T010 [P] Settings tests in `UnitTest/EmuTests/GlobalUserPrefsTests.cpp`: defaults; a round trip; old keys alone load; new keys take precedence over old; out-of-range values clamp to the moved constants (done: `HistorySettings_DefaultsRoundTripAndClamp`)
- [X] T011 Repoint `ShellHistory`, `ShellDebugger::GetReverseOptions` / `SetReverseOptions` and `CassoEmuCore/Ui/Debugger/HistoryOptionsPage.h/.cpp` to `GlobalUserPrefs::history` (done)
- [X] T012 Add a History group to Settings > General in `CassoEmuCore/Ui/Settings/GeneralPage.h/.cpp` and `GeneralPageModel.h/.cpp`: History on (check), memory budget and keyframe interval, with the debugger page's ranges; sentence-case labels; model tests in `UnitTest/EmuTests/GeneralPageModelTests.cpp` (done differently: the debugger's `HistoryOptionsPage` is the History page of the Settings sheet as well, committed on OK, and gains a keyframe interval slider that applies from the next start of history; tests in `UnitTest/DebuggerTests/DebuggerOptionsSheetTests.cpp`)
- [X] T013 R-007: remove the pause at Maximum speed: `ReverseController::SetUserMaximumSpeed`, `m_isPaused` and `m_pauseStart` in `CassoEmuCore/Debugger/Reverse/ReverseController.h/.cpp`, and the `isUserMaximumSpeed` argument of `ReverseHost::OnFrame` and `ShellHistory::RecordHistoryFrame`; keep `hasGapBefore`, `MarkNewestGap` and `ReverseOutcome::AtHistoryGap`; check with `rg SpeedChooser` whether anything but history reads `SpeedChooser`, and remove it only if nothing does (done: recording no longer pauses at Maximum; the controller's pause stays, renamed `SetRecordingPaused`, since gaps remain and the tests use it to make them)
- [X] T014 [P] Replace the Maximum-speed gap tests in `UnitTest/EmuTests/ReverseHostTests.cpp` (and any other `rg -l SetUserMaximumSpeed UnitTest`) with `RecordingContinuesAtMaximumSpeed`: keyframes fall due at Maximum and no gap is marked (done)
- [X] T015 Full Debug suite and `scripts/RunTests.ps1 -Build -Configuration Release`; commit and push the phase (done 2026-10-26: Release 11,091 passed, Debug 11,097 passed, 3 skipped in each)

**Checkpoint**: history lives in `ShellHistory`, its settings are general, and recording runs at every speed.

---

## Phase 3: User Story 1 - Rewind in the emulator window (Priority: P1)

**Goal**: the rewind key, the rewind bar with marks, the History menu and the replay indicator, with the debugger closed.

**Independent Test**: quickstart.md Story 1.

### Tests for User Story 1

- [X] T016 [P] [US1] `UnitTest/Dxui/DxuiImageStripMarkTests.cpp` per contracts/timeline-strip.md: marks at offsets paint and hit-test, a fork mark with a count above 1, marks closer than a glyph width merge with a combined tip, a source with no marks paints none Done: `DxuiImageStripMarkTests`; the fork-count case is a mark with a count, since a fork is Phase 4.
- [X] T017 [P] [US1] `UnitTest/EmuTests/HistoryMarkTests.cpp`: `HistoryThumbnails` builds marks from a synthetic journal (`Reset`, `PowerCycle`, `DiskMount`, `DiskEject`, `DriveWriteProtect`, `ImageWriteProtect`, `StateLoad`) and from `hasGapBefore`, each at its cycle's offset Done: `HistoryMarkTests`, over `HistoryMarks`, which reads only the records added since the last call and starts over after a cut (`InputJournal::GetCutCount`).
- [X] T018 [P] [US1] `UnitTest/EmuTests/ShellKeyRoutingTests.cpp`: F2 is `ShellKeyOwner::Rewind` in every routing state, its character is swallowed, and the guest receives neither the press nor the release Done: in `ShellKeyRoutingTests` (`F2IsTheRewindKeysInEveryState`).
- [X] T019 [P] [US1] `UnitTest/EmuTests/RewindLoopTests.cpp` over a test machine with history: each frame steps back by `kFrameCycles`; sound is muted throughout; the loop stops at the oldest point paused there; a release stops it on the frame on screen, and the next frame after the release does not step back (SC-003); a repeat is ignored; a scrub through `HistoryTimelineScrub` is muted as well (FR-006) Done: `RewindLoopTests` in `ShellHistoryTests.cpp`. Sound is not checked: a rewind frame runs no slices, and the replay drops its speaker clicks.
- [X] T020 [P] [US1] `UnitTest/EmuTests/RewindBarBandTests.cpp`: the opening rule as a pure function of pointer, viewport, DPI and capture state: opens in the bottom 8 DIPs scaled for the DPI, not above them, not while the pointer is captured, not with history off Done: `RewindBarBandTests`, with a curved-screen case for the desk scene.

### Implementation for User Story 1

- [X] T021 [US1] Add `DxuiStripMarkKind`, `DxuiStripMark`, `DxuiStripForkRow` and the `GetMarks`, `GetForkRows`, `OnForkRowHovered` and `OnForkRowClicked` defaults to `Dxui/Widgets/IDxuiImageStripSource.h`, exactly as contracts/timeline-strip.md lists them Done with a change: `DxuiStripMark` has a `glyph` and `hasRows` in place of a kind, since Dxui has no reference to history's kinds; the fork rows wait for Phase 4.
- [X] T022 [US1] Paint and hit-test marks in `Dxui/Widgets/DxuiImageStrip.h/.cpp`: each mark's `glyph` (set by the source; `HistoryThumbnails` takes them from `CassoEmuCore/Core/UnicodeSymbols.h`, which Dxui cannot include), the count beside a fork mark, merged marks with a combined tip; T016 passes Done.
- [X] T023 [US1] Add `InputKind::StateLoad` to `CassoEmuCore/Debugger/Reverse/InputJournal.h`, and build marks in `HistoryThumbnails::Service` (`CassoEmuCore/Debugger/Reverse/HistoryThumbnails.h/.cpp`) from the journal and keyframe gaps, published under the lock with the cells; implement `GetMarks`; T017 passes Done: the marks are built in `ShellHistory::PublishMarks`, from the journal on `MachineHost`, and given to both timelines.
- [X] T024 [US1] Add a second `HistoryThumbnails` and `ScratchMachineRenderer` to `ShellHistory` for the rewind bar (R-009), attached to the same store; the debugger window uses the first Done.
- [X] T025 [US1] Build the rewind bar overlay in `CassoEmuCore/Shell/Components/ShellChrome.h/.cpp`: a `DxuiToolbar` with one `DxuiImageStrip` entry over the bottom of the viewport, sourced from T024's thumbnails, with the strip's popup host and label room as `DebuggerWindow::ConfigureTimeline` sets them; the opening rule from T020 factored into a static function the test calls; the guest mouse never receives a click over it; `SetVisible` on its thumbnails follows the overlay Done as `CassoEmuCore/Ui/Chrome/RewindBar.h/.cpp`, owned by `ShellChrome`. On the desk scene the glass is found by the scene's hit test, since the composition's glass rectangle does not bound the drawn tube.
- [X] T026 [US1] Route the pointer band in `CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp`: mouse moves over the bottom band open the bar (SC-008, within 150 ms), moves off it close it, and the bar's drags scrub through `HistoryTimelineScrub` as the debugger timeline does Done. A drag pauses a running machine, which is paused after the release too (FR-005); the debugger's timeline runs it on.
- [X] T027 [US1] Add `ShellKeyOwner::Rewind` for `VK_F2` in `CassoEmuCore/Shell/Input/ShellKeyRouting.h/.cpp`; dispatch its press in `EmulatorShell::OnKeyDown` and intercept its release in `EmulatorShell::OnKeyUp` (`CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp`) to start and stop rewinding; T018 passes Done.
- [X] T028 [US1] Build the rewind loop in `ShellHistory` and `CassoEmuCore/Debugger/Reverse/ReverseHost.h/.cpp`: posted start and stop commands; each CPU-thread frame while rewinding runs `ReverseController::StepBackCycles (kFrameCycles)`, then renders and presents; stops at the oldest point; T019 passes Done in `ShellHistory::RunRewindFrame` and `ReverseCommand::RewindFrame`, paced by `CpuManager::SetPaceForced`.
- [X] T029 [US1] Measure the rewind loop for SC-002 in `ScenarioTests/HistoryCostTests.cpp` (a new logged test per R-001 workload: time from the first step to its frame, and the worst frame over 5 s of rewinding, Release, pinned); if any frame exceeds 16.7 ms, build R-008's fallback (replay a stretch once, save its frames) in `ReverseHost`; record the numbers in research.md R-008 Done: worst frame 4.46 ms, so no fallback (research R-008).
- [X] T030 [US1] A key press while paused behind live starts the replay so the guest reads it (spec US1 scenario 3), in `ShellHistory`'s input path beside `HoldInputBehindLive` Done: `ShellHistory::RunForHeldInput`.
- [X] T031 [US1] Add the History menu to `CassoEmuCore/Ui/Chrome/EmulatorCommands.cpp` and `CassoEmuCore/resource.h`: Rewind (F2, while it is down), Step back (Ctrl+F11, `ReverseCommand::StepBack`), Go live (Ctrl+F2, `ReverseCommand::GoLive`), History on (check item); new ids above the highest id in every origin branch's `resource.h` and `.rc`; accelerator rows for Ctrl+F11 and Ctrl+F2 in `Casso/Casso.rc`; enabling per contracts/emulator-history-commands.md, factored into a static function of `ShellHistory` over plain values (history on, behind live, at the oldest point) that `CassoEmuCore/Shell/Window/EmulatorWindow.cpp` calls; tests of every row in `UnitTest/EmuTests/ShellHistoryTests.cpp` Done: ids 40410 to 40413; Rewind from the menu steps back one frame.
- [X] T032 [US1] Build the replay indicator in `CassoEmuCore/Shell/Components/ShellChrome.h/.cpp` (R-013): "Replaying" and the host time of the position (`DebuggerStatusText::FormatWallClock`) below `ComputeTopOverlayEdgePx`, its opacity pulsing on the chrome's animation tick (`WantsAnimationTick` true while drawn), drawn only while behind live and running, hidden by `SetStandInOverlaysHidden`; a test of the visibility rule in `UnitTest/EmuTests/ReplayIndicatorTests.cpp` Done as `CassoEmuCore/Ui/Chrome/ReplayIndicator.h/.cpp`.
- [X] T033 [US1] Turning history off (FR-026) in `ShellHistory`: one function the menu, Settings > General and `HistoryOptionsPage` all call, which displays a confirmation dialog through `IHistoryHost` with the text "Turning history off deletes all of this machine's history, in memory and saved." and Turn off / Cancel buttons; on confirmation stop recording, clear the store, disable the bar and the key; tests in `UnitTest/EmuTests/ShellHistoryTests.cpp` for confirm and cancel; run Check-Prose on the dialog text Done in `ShellHistory::SaveOptions`, which every caller uses. Deleting saved history is Phase 6's.
- [X] T034 [US1] Run quickstart.md Story 1 on screen (minimized launch with `--title`, posted input, captures), fix what fails, commit and push Done 2026-10-27 with the workstation locked: the bar opens over the glass, F2 rewinds past a CATALOG, and Run replays it with the indicator pulsing.

**Checkpoint**: Story 1 works with the debugger never opened.

---

## Phase 4: User Story 2 - Fork history instead of discarding it (Priority: P1)

**Goal**: replay from the past, silent forks, branches with marks and a flyout, switches to a branch's live end (R-004 to R-006).

**Independent Test**: quickstart.md Story 2.

### Tests for User Story 2

- [X] T035 [P] [US2] `UnitTest/EmuTests/ArenaExtentsTests.cpp`: first fit, merging of adjacent free extents, freeing in the middle, a full arena reports no room, reserved once with no later allocation Done.
- [X] T036 [P] [US2] `UnitTest/EmuTests/KeyframeStoreBranchTests.cpp`: a fork at P splits the segment with no bytes copied; the index interface lists the active path in order; differences after P restore against the shared whole snapshot; a switch changes the active path; `PathChanged` is sent with the fork position Done in `KeyframeStoreBranchTests`.
- [X] T037 [P] [US2] Eviction tests in `UnitTest/EmuTests/KeyframeStoreBranchTests.cpp`: the oldest groups of branches left behind are dropped first, oldest branch first (FR-016); an emptied segment is removed; a ninth branch left behind drops the oldest whole (FR-017); the active path's oldest group is dropped only when no branch is left; a whole snapshot with a difference against it in any segment stays Done in `KeyframeStoreBranchTests`; a branch trimmed to its live end begins with a gap from the fork.
- [X] T038 [P] [US2] `UnitTest/EmuTests/InputJournalBranchTests.cpp`: records after a fork move to the branch left behind, not dropped; a switch moves the active path's records out and the target's in; indices stay absolute and valid along each path Done in `InputJournalBranchTests`.
- [X] T039 [P] [US2] `UnitTest/EmuTests/ReverseForkTests.cpp` over `ReverseSessionRig`: Run behind live replays and lands live at the end; a key the guest reads forks at the read; each change in contracts/emulator-history-commands.md's "Behind live" table forks with no prompt; a debugger edit behind live forks through `GuardHistoryEdit` with no prompt; a seek, a step back and a rewind across a fork position stay on the active branch (FR-013); a change before a disk-mismatch gap is ignored with a notice; a machine switch does not fork; switching back to the branch left behind lands at its live end with the same `ComputeChecksum` it had (SC-004), paused Partly done in `ReverseForkTests` (a fork and a switch back, exactly at the live end; a seek into a switched branch; moves on the active path), `ShellHistoryTests` (a debugger edit forks), `DivergenceGateTests` (a machine switch forks nothing) and on screen. The disk-mismatch gap waits for Phase 6's saved history.
- [X] T040 [P] [US2] Flyout tests in `UnitTest/Dxui/DxuiImageStripMarkTests.cpp` and `UnitTest/EmuTests/HistoryMarkTests.cpp`: resting on a fork mark opens the flyout with one row per branch (last frame, time past the fork, time left); a hover routes `OnForkRowHovered`; a click routes `OnForkRowClicked` and closes it; forks within a branch appear only while it is active (FR-014); labels are times only (FR-015) Done in `DxuiImageStripMarkTests` and `HistoryMarkTests`.

### Implementation for User Story 2

- [X] T041 [US2] Create `CassoEmuCore/Debugger/Reverse/ArenaExtents.h/.cpp` (a free-extent list ordered by offset, first fit, adjacent extents merged, reserved once), add it to the project and filters; T035 passes Done; there is no filters file.
- [X] T042 [US2] Replace `KeyframeStore`'s ring placement (`Place`, `TryFindRoom`, `m_arenaEnd`) with `ArenaExtents` in `CassoEmuCore/Debugger/Reverse/KeyframeStore.h/.cpp`, with no segments yet; the existing suites for `KeyframeStore`, `IdleHistoryRate` and reverse execution pass Done.
- [X] T043 [US2] Add `KeyframeStore::Segment` per data-model.md (`id`, `parent`, `forkPosition`, `entries`, `records`, `firstJournalIndex`, `leftAtWallTime`, `liveEnd`), the active path, and index resolution through the chain for `GetCount`, `GetInfo`, `GetEntry`, `Restore`, `RestoreAtPosition`, `TryFindAtOrBefore`, `TryFindByPosition`, `CopyPacked` and `GetSide`; add `Fork (uint64_t position)` and `SwitchTo (uint32_t leaf)`, `KeyframeDrop::PathChanged` with its position, and `KeyframeInfo::isLiveEnd`; T036 passes Done with a change: the active path stays in the table, and each branch left behind is a segment of entries and records with a parent and a fork position, as the refinement of R-004 has it. Branches are in `KeyframeStoreBranches.cpp`.
- [X] T044 [US2] Eviction order and the 8-branch cap in `KeyframeStore::TryDropOldestGroup` and `DropOldestGroups`, with a whole snapshot dropped only when no segment has a difference against it; T037 passes Done: only branches with none forked from them are trimmed, oldest first.
- [X] T045 [US2] Journal moves in `CassoEmuCore/Debugger/Reverse/InputJournal.h/.cpp`: `MoveAfter (size_t index, std::vector<InputRecord> & outRecords)` and `Append (const std::vector<InputRecord> & records)`, used by the store's fork and switch; T038 passes Done.
- [X] T046 [US2] In `ReverseController::LeaveLive` (`CassoEmuCore/Debugger/Reverse/ReverseController.cpp`), capture a keyframe at the live end with `isLiveEnd` set, before the machine moves Done: `ReverseController::CaptureLiveEnd`.
- [X] T047 [US2] Replace the cut with a fork: `ReverseController::OnMachineChanged` calls `KeyframeStore::Fork` where it calls `DropAfterPosition`, and the journal's `Truncate` there becomes the move; `ReverseHost::Diverge` becomes `Fork`; `IDM_DEBUG_DIVERGE` posts it Done.
- [X] T048 [US2] Add `ReverseHost::SwitchBranch (uint32_t leaf)` and `ReverseController::SwitchBranch`: the store's `SwitchTo`, then load the leaf's live-end keyframe; the machine is paused there and live; the branch left gets `leftAtWallTime`; posted as a new `IDM_DEBUG_REVERSE` word `switch <leaf>` in `CassoEmuCore/Shell/CpuCommandDispatcher.cpp` (format and parse functions with tests in `UnitTest/EmuTests/CpuCommandDispatcherTests.cpp`); a switch while the debugger is stopped is reported as a seek Done, with the payload words in `ReverseCommandPayloadTests`.
- [X] T049 [US2] Silent forks in `CassoEmuCore/Debugger/Reverse/DivergenceGate.h/.cpp` and `ShellHistory`: `Judge` returns `Proceed` for a change behind live, which the shell forks before running; the deferred-input read forks where `StopForHeldInputRead` and `OnConfirmInputDiverge` prompt today; make `GuardHistoryEdit` and `DeferredHistoryEdits` fork and run the edit with no prompt; remove `kpszQuestion`, `kpszTitle`, `AskToDiverge`, `OnConfirmDiverge` and the window's handlers in `CassoEmuCore/Shell/Window/EmulatorWindow.cpp`; remove `IDM_FILE_OPEN` from `IsStateChangingCommand`; T039 passes Done: the gate's verdict is `Fork`; `DeferredHistoryEdits` is no longer used, since an edit forks at once.
- [X] T050 [US2] Fork marks and rows in `HistoryThumbnails`: fork positions on the active path with counts, `GetForkRows` with each branch's live-end picture drawn by the worker as a preview, time past the fork and time left, and `OnForkRowClicked` calling the switch function set by `ShellHistory`; on `PathChanged`, `ForgetPictures` from the position and rebuild the marks Done, in `HistoryThumbnailMarks.cpp`.
- [X] T051 [US2] The flyout in `Dxui/Widgets/DxuiImageStrip.h/.cpp` per contracts/timeline-strip.md (a popup of the strip's popup host, no focus or capture, closes when the pointer moves off it and the mark); T040 passes Done as `Dxui/Widgets/DxuiStripFlyout.h/.cpp`.
- [X] T052 [US2] Row hover preview: `ShellHistory` passes the hovered row's picture to `ShellChrome`, which draws it over the viewport until the hover ends; tests in `UnitTest/EmuTests/ShellHistoryTests.cpp` that a hover sets the preview picture, a hover of row 0 clears it, and a switch clears it Done as `CassoEmuCore/Ui/Chrome/BranchPreview.h/.cpp`; checked on screen, not by a unit test.
- [X] T053 [US2] Run quickstart.md Story 2 on screen, fix what fails, commit and push Done 2026-10-27 with the workstation locked: a key typed in the past forked at the read, the fork mark opened its flyout, a hovered row drew the branch over the screen, and a click switched to it.

**Checkpoint**: forks are silent and nothing is lost; branch switches land exactly.

---

## Phase 5: User Story 5 - One history for the emulator and the debugger (Priority: P2)

**Goal**: the debugger's views follow forks and switches on the one shared history.

**Independent Test**: quickstart.md Story 5.

- [X] T054 [P] [US5] `CassoEmuCore/Debugger/HeatHistory.cpp`: on `KeyframeDrop::PathChanged`, drop the heat after the position and rebuild from the side bytes of the new path; test in `UnitTest/EmuTests/HeatHistoryTests.cpp` Done: the anchor stays when the fork's drops moved it to a keyframe before the fork, so the totals at the branch's live end are a straight run's.
- [X] T055 [P] [US5] `CassoEmuCore/Debugger/CallStackHistory.cpp`: on `PathChanged`, drop the call record copies after the position and start a rebuild, as after a move; test in `UnitTest/EmuTests/CallStackHistoryTests.cpp` Done; the existing handling passed the new test with no change.
- [X] T056 [P] [US5] `CassoEmuCore/Shell/DebugEventLogs.cpp`: on `PathChanged`, `CutBack` every cache to the position and rebuild the newer stretches from history; test in `UnitTest/EmuTests/DebugEventLogsTests.cpp` Done.
- [X] T057 [US5] The debugger timeline draws the same marks and flyout as the rewind bar, with no mark for a breakpoint stop (FR-028), through `CassoEmuCore/Ui/Debugger/DebuggerWindowTimeline.cpp`; reverse continue and the step backs work on the active branch (FR-029), checked by the existing reverse continue tests plus one after a switch in `UnitTest/EmuTests/ReverseForkTests.cpp` Done: the debugger's strip uses `ShellHistory::m_thumbnails`, to which `PublishMarks` sends the same marks and branches as to the bar's, and the row labeler and the hover and switch functions are set on both. Marks come from the input journal and forks only, so a breakpoint stop has none.
- [X] T058 [US5] Run quickstart.md Story 5 on screen, fix what fails, commit and push Done 2026-10-27 with the workstation locked: a key typed after F2 forked at the read, and the debugger opened afterward has the fork mark on its timeline. The flyout and a switch from the debugger's strip were not driven, since a posted move draws a WM_MOUSELEAVE while the real pointer is elsewhere; they run through the same strip, source and `ShellHistory::SwitchBranch` as the bar's, which were checked in T053. The switch, reverse continue and the views that follow it are covered by the T054 to T057 tests.

---

## Phase 6: User Story 4 - History persists across launches (Priority: P2)

**Goal**: every save includes history; launches and state files restore it (R-010).

**Independent Test**: quickstart.md Story 4.

### Tests for User Story 4

- [X] T059 [P] [US4] `UnitTest/EmuTests/SavedHistoryTests.cpp`, from synthetic bytes, per contracts/saved-history-format.md "Reading": round trip with forks (segments, keyframe checksums, journal records, fork marks, the resume position); each discard row (no section, bad tag, size, hash, fingerprint, an unparsable section) loads the state with fresh history and reports no error; a file in format version 3 loads with no history; an arena larger than the budget drops branches left behind first Done in `SavedHistoryTests`, with `ParseFailsOnADamagedSection` and `ParseFailsOnAnUnknownInputKind` for the discard rows and `AStateWithDamagedHistoryLoadsWithFreshHistory` in `StateLoadHistoryTests` for the load. A file in format version 3 is in `MachineStateFileTests`. T067's scenario exposed a Phase 4 bug: a switch back to a branch forked at the oldest keyframe failed, since the journal entries from the fork's journal index to that keyframe's had been dropped; `InputJournal::Truncate` now cuts back past them.
- [X] T060 [P] [US4] `UnitTest/EmuTests/StateLoadHistoryTests.cpp`: a state file with history replaces the current history; one without becomes an `InputKind::StateLoad` marked point in the current history, and forks when behind live (FR-025); with history off a save writes no section and a load ignores one Done in `StateLoadHistoryTests`, on a shell rig moved from `ShellHistoryTests` into `ShellHistoryRig.h`; a state saved behind live resumes where it was saved.

### Implementation for User Story 4

- [X] T061 [US4] Create `CassoEmuCore/Shell/SavedHistory.h/.cpp`: `Build (const KeyframeStore &, const InputJournal &, media table, resume position, std::vector<Byte> & out)` and `Parse (const std::vector<Byte> &, ...)` per the contract's sections `HFMT`, `HSET`, `HSEG`, `HKEY`, `HARN`, `HJRN`, `HMED`, `HPOS`; the fingerprint is a checksum over the tags and versions of a `MachineHost::SaveState` of the running machine; add `KeyframeStore` functions to export its used extents and segment tree and to rebuild from them; T059's parse rows pass Done with changes: the first segment is the active path and the branches follow, so `HSEG` has no active leaf; `HMED` lists each bay's medium id, since the payload has the disks; the fingerprint is `MachineHost::ComputeStateFingerprint`, over each state part's top-level sections. The store's side is `KeyframeStore::Export` and `Import` in `KeyframeStoreSaved.cpp`.
- [X] T062 [US4] `CassoEmuCore/Shell/MachineStateFile.h/.cpp`: format version 4 with the optional history section after the payload, with a tag, size and hash for the section, stored unpacked; `MachineStateContents` gains the section's bytes; files in format version 3 load Done; `AVersionFourStateHasItsHistorySectionAfterThePayload` in `MachineStateFileTests`.
- [ ] T063 [US4] Media in saved history: a `DiskImage` id restored from the section and the id counter started past the highest saved one (`CassoEmuCore/Devices/Disk/DiskImage.h/.cpp`, only the id functions, since 041 also changes `DiskImage`), and the saved media recreated as retained media through the store's existing public mount and retention functions; a disk whose `ImageIdentity` differs at load mounts from its file and the launch's keyframe gets a gap. If the store's public functions cannot recreate a retained disk with a given id, stop: move that part to Phase 7, and until then saved history whose keyframes refer to a disk no longer in a bay starts fresh. Tests in `UnitTest/EmuTests/SavedHistoryTests.cpp`: saved ids restored, the counter past them, a mismatched identity produces the gap Partly done: `DiskImage::SetImageId` sets a bay's medium to its saved id and moves the counter past it. The store has no public function to recreate a retained disk with a given id, so, as this task specifies, that part moves to Phase 7, and until then no history is saved while a disk that left a bay is retained. A mismatched identity cannot arise yet: the payload has the disks' bytes, and no image file is read at a load.
- [X] T064 [US4] Saving: `ShellHistory` supplies the section to every save while history is on (the automatic state in `CassoEmuCore/Shell/EmulatorShellState.cpp` and `MachineManager.cpp`, File > Save state); behind live the machine first seeks to the active branch's live end and `HPOS` records the position on screen; the writer waits for keyframes in flight Done: `ShellHistory::BuildSavedHistory` and `ReturnFromSave`; at exit the section is built in `OnCpuThreadStop`, before recording stops, and the stop then writes the live end's disks.
- [X] T065 [US4] Loading: at launch and on File > Load state, a valid section rebuilds the store and the machine then seeks to `HPOS`, paused; a file without one becomes a `StateLoad` change (forking behind live) in `EmulatorShell::LoadMachineStateBytes`; history off ignores the section; T060 passes Done: `ShellHistory::StartFromSaved`, through the pending section `StartRecording` takes; a state with no history goes through `PrepareStateLoad` and `FinishStateLoad`, which raises the history cycles to those before the load (`Cpu6502::AdvanceHistoryCycleCount`), so the history before it stays.
- [ ] T066 [US4] History before a disk-mismatch gap (R-010): replay and seeks work there; a change made there is ignored with a notice through `ShellChrome::ShowNotice`; test in `UnitTest/EmuTests/StateLoadHistoryTests.cpp` Moves to Phase 7: no gap arises while the payload has the disks' bytes.
- [X] T067 [US4] `ScenarioTests/SavedHistoryScenarioTests.cpp`: boot the DOS 3.3 master, record with a fork, save to bytes, load into a fresh machine, switch branches; and fill a 64 MB history and log the save and load times (SC-007 allows 1 s for the load); add it to `ScenarioTests/ScenarioTests.vcxproj`; record the times in research.md R-010 Done; times in R-010.
- [X] T068 [US4] Run quickstart.md Story 4 on screen, fix what fails, commit and push Done 2026-10-27 with the workstation locked: a key typed after F2 forked history, File > Exit wrote the automatic state with a history section, and the next launch's debugger timeline began at the first run's start with the fork mark. A switch after the load, the discard rows and the File > Save state round trip are covered by the T059, T060 and T067 tests; the flyout could not be driven by posted input.

**Checkpoint**: everything but the disk rule is built. Run the full Debug and Release suites and the scenario suite.

---

## Phase 7: User Story 3 - Disk image files follow the active branch (Priority: P1)

**Goal**: each image file contains the active branch's version of its disk (R-011).

**Independent Test**: quickstart.md Story 3.

**Blocked**: until 041-disk-integrity is merged to master. If it is not there when Phase 6 ends, stop and tell the owner; the feature does not merge without this phase.

- [ ] T069 [US3] Confirm 041 is on master (`git log origin/master --oneline --grep 041` and its merge commit), run `scripts/CheckStyle.ps1 -Mode Tree`, merge `origin/master` into `045-emulator-history` (renames merge cleanly and then fail to compile; fix every call site the compiler reports), build both configurations, run the full suites, and push
- [ ] T070 [US3] Reread `specs/041-disk-integrity/plan.md`, `research.md` and the shipped `DiskImageStore.h` on master; update research.md R-011 with the actual commit, flush and retention functions this phase uses, and the FR-017 reset behavior
- [ ] T071 [P] [US3] `UnitTest/EmuTests/BranchDiskTests.cpp`, through the store's flush sink (no real file): after a fork, and after a switch, each disk image matches the new active branch's version byte for byte (SC-005); a disk the guest never wrote is not written; a branch with a different disk in a drive puts that disk in the drive (FR-021); behind live without a fork, the motor stopping, a reset and a power cycle in replay write nothing (FR-019); a machine switch and exit behind live write the live end's version (FR-020, SC-006); an eject behind live forks and writes the version on screen
- [ ] T072 [US3] `BranchDiskVersion` per data-model.md in `KeyframeStore::Segment`: when a branch is left behind, each disk that left a bay after the fork on it, packed with `SnapshotCompressor` and counted against the budget; `HBDV` added to `SavedHistory`
- [ ] T073 [US3] The writes at a fork and a switch in `ShellHistory`: compare old and new active versions by checksum for every disk touched after the fork on either path, and write each that differs through the store's commit path as 041 left it
- [ ] T074 [US3] Forced writes behind live without a fork (machine switch, exit) seek to the live end before the flush, in `ShellHistory` and `MachineManager.cpp`
- [ ] T075 [US3] Prune retained media by the set of media ids any segment's keyframes refer to, replacing `PruneRetainedMedia (oldestPosition)` in `CassoEmuCore/Devices/Disk/DiskImageStore.h/.cpp` and its callers; T071 passes
- [ ] T076 [US3] `ScenarioTests/BranchDiskScenarioTests.cpp`: on a scratch copy of the DOS 3.3 master, `SAVE T1` on one branch and `SAVE T2` on another; after each switch the file matches the active branch byte for byte, and DOS 3.3 catalogs the expected file; add it to the project
- [ ] T077 [US3] Run quickstart.md Story 3 on screen with a scratch disk, fix what fails, commit and push

---

## Phase 8: Polish and Cross-Cutting Concerns

- [ ] T078 Re-measure SC-001a and SC-001b with `HistoryCostTests` (Release, pinned) against T001's figures, and SC-002, SC-007 and SC-008 on the final build; update research.md
- [X] T079 [P] Document the History menu, F2, Ctrl+F11, Ctrl+F2, the rewind bar, forks and saved history in `docs/` beside the debugger's history documentation, and in the key map (`IDM_HELP_KEYMAP`) Done in `docs/History.md`, linked from the debugger's timeline section, and an "F2, held" row in the keyboard map.
- [X] T080 [P] Remove dead code the move left: `rg -n "AskToDiverge|kpszQuestion|SetUserMaximumSpeed|reverseRecording" CassoEmuCore UnitTest` returns only the old JSON keys' read Done: the deferred debugger edits, unused since an edit behind live forks at once, are removed; the old JSON key is read and written by `GlobalUserPrefs`, as R-012 settles.
- [ ] T081 Full gate: `scripts/RunTests.ps1 -Build` in Debug and Release, `scripts/RunTests.ps1 -Build -Scenario`, `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`, ARM64 Debug and Release builds (build only; there is no ARM64 device), any AddressSanitizer scenario run master requires after 041, and `scripts/CheckStyle.ps1 -Mode Tree`; report any suite that could not run, and why
- [X] T083 Remove the keyframe interval setting (spec Clarifications 2026-10-27, FR-008): the History page's interval slider, `HistorySettings::intervalFrames`, `ReverseOptions::intervalFrames` and the `reverseIntervalFrames` read and write; history starts at `KeyframeSettings::kDefaultFrames` Done; `GlobalUserPrefsTests` checks the old key is kept for older builds.
- [X] T084 History > Rewind 10 seconds and History > Forward 10 seconds (FR-007, FR-007a): `ReverseCommand::SkipBack` and `SkipForward`, `ReverseController::kSkipCycles`, `IDM_HISTORY_FORWARD` Done; `ReverseHostTests::TheSkipsStopAtTheEndsOfHistory` and the menu rules in `ShellHistoryTests`.
- [X] T085 Draw a fork mark as a horizontal branch (FR-010): `DxuiStripMarkArt`, `Dxui/Widgets/DxuiBranchArt.h/.cpp` and `DxuiImageStrip::PaintBranchArt` Done; `DxuiImageStripMarkTests` covers the art's pixels and the badge.
- [X] T086 Defer the "Disk modified outside Casso" report and question until the machine is live (FR-021a): `Devices/Disk/DeferredChangePrompts.h/.cpp`, `ShellDisks::OnChangeReport`, `OnChangeQuestion` and `ShowDeferredChanges`, called from `WM_APP_GAMEPORT_FLUSH` once live Done; `DeferredChangePromptsTests`.
- [X] T087 List the active branch first in a fork mark's flyout, then the branches by when they were left, and place a row's preview beside the bar's flyout (FR-011): `HistoryThumbnails::kActiveBranchId`, `GetForkRows`, `OnForkRowClicked`, `BranchPreview::FitBeside`, `RewindBar::TryGetFlyoutRect` and `ShellChrome::SyncBranchPreview` Done; `HistoryMarkTests` and `BranchPreviewTests`. Checked on screen 2026-10-28 (T088).
- [X] T088 Check T085 and T087 on screen and fix what fails: a fork made by a reset or power cycle was hidden under that change's mark (`DxuiImageStrip::JoinMarks` now draws a shared badge as the mark with rows); DrawFramebuffer rewrote one bitmap for every row, so each flyout row but the last was flushed under the card (`DxuiTextRenderer` now cycles through bitmap slots); the hover color was not premultiplied; and the bar closed when the pointer went into the flyout (`RewindBar` now closes once the flyout does). Done; `DxuiImageStripMarkTests::AResetJoiningItsForkIsDrawnAsTheFork`.
- [ ] T089 Redraw the fork mark's branch art: at strip size it reads as a "not" sign rather than a branch (seen on screen 2026-10-28, `branch-preview-onscreen.png`). The owner picks the new art from a rendered sheet.
- [ ] T082 Draft the CHANGELOG `[Unreleased]` entries (one terse line each, under Added and Changed) and the README feature line, run Check-Prose, and give them to the owner for approval before the master merge

---

## Dependencies & Execution Order

### Phase dependencies

- **Phase 1** first. T002 before any code task.
- **Phase 2** blocks every story.
- **Phase 3 (US1)** after Phase 2.
- **Phase 4 (US2)** after Phase 3: the fork marks and flyout extend T021 to T025.
- **Phase 5 (US5)** after Phase 4: it follows `PathChanged`.
- **Phase 6 (US4)** after Phase 4: the format saves the segment tree.
- **Phase 7 (US3)** after Phase 6 and after 041 is on master.
- **Phase 8** last.

### Within each story

Tests first, seen to fail; then the code in the listed order; then the on-screen check.

### Parallel opportunities

- T008 and T010 alongside T011 to T013.
- T016 to T020 together; T035 to T040 together; T054 to T056 together; T059 and T060 together.
- T079 and T080 together.

## Parallel Example: User Story 2

```text
T035 ArenaExtentsTests      T036 KeyframeStoreBranchTests      T038 InputJournalBranchTests
T039 ReverseForkTests       T040 flyout tests
```

## Implementation Strategy

No MVP: the feature merges to master whole, after 035, in one merge. The phases order the work so each builds on a tested base: the move and settings, the emulator window, forking, the debugger's views, persistence, then the disk rule on 041's store. Push after each phase.

## Notes

- 041's merge to master is the one outside dependency; check its state at the start of Phase 6.
- If the design turns out wrong during a task, record the finding in research.md and ask the owner before building around it.
