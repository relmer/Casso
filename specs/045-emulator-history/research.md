# Research: Machine History in the Emulator

**Feature**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

Every entry was checked against the code on `045-emulator-history` at `476d7707e` (035 at `29a8bce48`), and against `origin/041-disk-integrity` at `986aea9b6`.

## R-001: What history costs on ordinary software at 1x

**Status**: measured 2026-10-25 (FR-030, SC-001a).

**Method**: `ScenarioTests/HistoryCostTests.cpp`. Two //e Enhanced machines boot the DOS 3.3 master the same way and run the same program, one with history on at the defaults and one with it off, in alternating ten-second rounds, six rounds each, after a ten-second warm-up. The difference in host time per emulated second is the cost; divided by a second, it is a share of one core at 1x. Release x64, run with `start /wait /b /high /affinity 10`, so the packing work on the thread pool runs on the same core and is counted. Across three runs the costs differ by a few hundredths of a percent.

| Workload | History off | History on | Cost at 1x |
|---|---|---|---|
| DOS 3.3 prompt waiting for a key | 0.85% of a core (117x real time) | 0.89% (112x) | 0.03% to 0.05% |
| Applesoft lo-res game loop (a ball and a keyboard poll) | 0.71% (141x) | 0.75% (133x) | 0.04% |
| Applesoft hi-res plotting | 0.66% (151x) | 0.71% (141x) | 0.05% |
| Applesoft loop running `VERIFY HELLO` (the disk turns on every pass) | 1.43% (70x) | 1.61% (62x) | 0.18% to 0.19% |

035's test (`UnitTest/EmuTests/ReverseReplayTests.cpp`, `RecordingCostIsLogged`, the reverse rig's guest loop with a disk mounted), five pinned runs on this build: 70.9x real time with history off, 67.6x with it on, 4.9% slower.

**Decision**: SC-001a is met by a wide margin: the whole cost of history on these workloads is under 0.2% of one core at 1x, against a ceiling of 1%.

**The 035 figures are out of date.** R-040's 46.6x was measured on 2026-10-03 with the per-frame checkpoint ring, which "As built 2026-10-03: snapshots only" removed the same day. With keyframes alone the rate is 67.6x.

**Rationale**: the per-instruction hook is one compare of the cycle count against the next due cycle (`HistoryRecorder`), and a keyframe every ten frames costs about 350 us of checksum, XOR and packing, most of it on the thread pool.

**Alternatives considered**: a 1x run of the application, comparing the process's CPU time with history on and off. Rejected: at 1x the CPU thread sleeps between frames, so a cheaper path only sleeps longer, and the rendering and audio threads add noise larger than the difference (`reference-measure-per-instruction-perf`). The comparison also requires changing the user preferences file between runs, which other instances on this machine read.

## R-002: History grows fast while a disk turns

**Status**: measured 2026-10-25; cause found and fixed 2026-10-26 (T003, T004).

During the `VERIFY` loop, history grew by 36.6 MB in 60 emulated seconds, about 600 KB per emulated second: at the 64 MB default, under two minutes of history. The other workloads grew by 0.6 MB to 1.5 MB in the same time. 035 measured 24 KB to 70 KB per second on its write and hi-res workloads (R-040, "Measured 2026-10-03" and "Measured again 2026-10-13"), so a turning disk costs about ten times the busiest of those.

**Why it matters now**: under FR-008b, recording continues at Maximum speed. The owner's reasoning, that a long run at Maximum is most often a small loop that adds little history, is right per emulated second, and so is the cost of a disk load: a boot or a game load run at Maximum takes the same emulated seconds as at 1x and adds the same history. At 600 KB per emulated second, a long load can push out older history quickly.

**Likely cause, to confirm**: each keyframe packs to about 100 KB, the size of a whole snapshot, so either the group restarts on every keyframe or the differences are large. `KeyframeStore` starts a new group when a snapshot's size differs from its group's whole snapshot; a Disk II or nibble engine state whose serialized size depends on the motor or the read position would cause it.

**Cause**: the Disk II address-mark watcher (`Disk2AddressMarkWatcher`, section `D2AM` in the Disk II's `DSK2`) saved only the nibbles of each data field read or written so far, so its saved size followed the head's progress through a field, from 82 to 421 bytes. A keyframe whose size differs from its group's whole snapshot starts a new group, so while a disk turned, 101 of 121 keyframes in 20 emulated seconds were whole snapshots of about 118 KB each.

**Fix**: section version 4 saves each field's whole buffer, the nibbles so far then zeros, so the size is constant; versions 3 and older load as before the change. In the same 20 seconds, 121 keyframes are taken, 1 of them whole, in 657 KB. Over 60 seconds, history grows by 1.1 MB to 1.3 MB during the `VERIFY` loop, against 36.6 MB before: about 20 KB per emulated second. `SavedState_isOneSizeWhateverTheFieldsProgress` covers it, and fails without the fix (88 bytes saved idle against 282 with fields in progress).

**Decision**: the fix is a 035 device's state, so it is a candidate for 035 too; it is in 045 now, since history depends on it.

## R-003: Where history lives

**Decision**: a new shell component, `ShellHistory` (`CassoEmuCore/Shell/Components/ShellHistory.h/.cpp`), constructed by `EmulatorShell` beside the other components. These `ShellDebugger` members move into it: `ReverseHost` and the history settings, `DivergenceGate`, `HeldInputWatch` and the input deferred behind live, `DeferredHistoryEdits`, the replay caption, the seek state (`m_seekCyclePosted` and the rest), `ReplayControl`, and the history renderer and thumbnails. The functions that work on them move out of `ShellDebuggerReverse.cpp` with them. `ShellDebugger` is given a `ShellHistory &`. The heat map, call stack and event log replays, the debugger window and session, and reverse continue's stop test remain in `ShellDebugger`.

**Rationale**: FR-001 and FR-027. The members work with the debugger window closed already (recording runs whenever the setting is on), but the emulator window can access them only through the debugger, and the settings are in the debugger's options. A component is the shell's existing unit for this (`ShellDisks`, `ShellTapeDeck`, `ShellInput`), and the move is mechanical: `ShellDebuggerReverse.cpp` is a separate file of these functions today.

**Alternatives considered**: history stays in `ShellDebugger`, with accessors for the emulator window. Rejected: the rewind bar, the key and the settings would depend on the debugger object, which the spec moves history away from, and every new history function would go into a class documented as the debugger.

## R-004: Branches in the keyframe store

**Decision**: `KeyframeStore` becomes a tree of segments over one arena. Its index-based interface presents the active path as one sequence, so `ReverseController`, `Replayer` and the step, seek and reverse-continue code work on the active path with no branch logic.

- **Segment**: a run of keyframes and the journal records between them, with its parent segment and the fork position where it begins. The root segment begins where history begins. The active path is the chain of segments from the root to the active leaf.
- **Index**: index *i* of the store is the *i*th keyframe along the active path. `GetCount`, `GetInfo`, `Restore`, `TryFindAtOrBefore`, `TryFindByPosition` and `CopyPacked` resolve through the chain.
- **Positions**: a branch's positions continue from its fork position, so two branches number the positions after a fork alike. A position is unique within a path, which is all the controller requires; a pair of segment and position is unique in the tree.
- **A fork at position P**: the segment containing P is split at P. The keyframes and journal records after P go to a new child segment, the branch left behind, and the active leaf becomes a second, empty child, which recording fills. Nothing is copied: entries move between segments, and the arena bytes stay at their offsets.
- **Groups across a fork**: a difference keyframe after P is stored against a whole snapshot at or before P, which both children share. A whole snapshot is dropped only when no segment has a difference against it.
- **The arena**: allocated once, as today, but managed as a list of free extents (ordered by offset, first fit, adjacent extents merged) in place of a head and a tail. Dropping a branch's keyframes frees extents anywhere in the arena, which the ring discipline cannot reuse until its head passes them. The free list is reserved with the table, so steady recording allocates nothing.
- **Eviction (FR-016, FR-017)**: when room is short, the oldest group of the oldest branch left behind is dropped first, branch by branch, oldest first; a segment with no keyframes left is removed with its mark. With more than 8 branches left behind, the oldest is dropped whole. The active path's oldest group is dropped, as today, only when no branch is left.
- **The live end of every branch is a keyframe**: when the machine moves from live into history, the controller captures a keyframe at the live end. A switch loads it, so SC-004's exact state takes no replay, and the flyout draws the branch's last frame from it.

**Rationale**: the controller is position arithmetic over one linear sequence, tested against determinism (`ReverseReplayTests`, `ReverseHostTests` and their neighbors). Presenting the active path as that sequence confines branches to the store and the journal, and every existing test remains meaningful.

**Alternatives considered**:

- **Copy the branch left behind out of the arena** at a fork, into a heap copy or a second store, and copy it back at a switch. Rejected: either the memory exceeds the budget by the size of the branches (the arena is allocated at the full budget), or the arena shrinks and grows around copies; and a switch copies up to the whole budget.
- **One store per branch.** Rejected for the same budget reason, and because each store's newest-group invariant and work buffers multiply by the branch count.
- **Branch-tagged entries in the existing ring.** Rejected: the ring cannot reuse the space of a branch dropped from its middle, so the eviction order FR-016 requires would free nothing.

## R-005: The input journal across branches

**Decision**: `InputJournal` contains the records of the active path, with the absolute indices it has today. At a fork, the records after the fork's journal cursor move into the branch left behind, in place of `Truncate`, which drops them. At a switch, the records after the common ancestor's cursor move out to their segment, and the target branch's records move in. Keyframes store journal indices, and both branches number their records from the cursor at the fork, so every index is valid along its path.

**Rationale**: records are small (a few scalars, and a payload string set only for a mount), and a frame adds a few, so moving them at a fork or a switch costs microseconds; the replayer and every listener read the journal by index, as today.

**Alternatives considered**: per-segment journals read through the chain, as the store is. Rejected: the replayer walks records by index on every instruction boundary, and a chain lookup there costs per instruction.

## R-006: What forks history, and what replays

**Decision**:

- **Run in the past replays**, in both windows, as 035's "Running forward from history" defines: `ReverseController::ReplayHere` with the debug hook attached, sound muted, and the disk files untouched. At the live end the machine is live.
- **A change behind live forks, silently.** Every path that cuts history today forks instead: `ReverseHost::Diverge` becomes `Fork`, and `ReverseController::OnMachineChanged` splits the segment at the position (R-004) where it now calls `DropAfterPosition`. The triggers are the ones 035 prompts for today, less those that replace the machine: a command the input journal takes (a disk mounted, ejected or write-protected, a reset, a power cycle, a paste), the answer to a changed disk file (`IDM_DISK_RESOLVE_CHANGE`, which forks at the position on screen, as an eject does), a debugger restart, a state file loaded without history, and a debugger edit. A machine switch or a configuration change (`IDM_FILE_OPEN`) does not fork: the outgoing machine's history is saved at the position on screen (R-010), so that case is removed from `DivergenceGate::IsStateChangingCommand`.
- **Input the guest reads forks at the read.** `DivergenceGate` defers a key, a paste, the game port and the mouse behind live until the replayed guest reads a deferred line (`HeldInputWatch`). Where it prompts today (`StopForHeldInputRead`, `OnConfirmInputDiverge`), it forks there instead, and the deferred input goes in live. The question text (`DivergenceGate::kpszQuestion`) and the prompts (`AskToDiverge`, `OnConfirmDiverge`) are removed.

**Rationale**: the owner's decisions of 2026-10-26. The deferred-input mechanism finds the exact read a key would change, so the fork point is the first instruction the change affects.

## R-007: Recording at Maximum speed

**Decision**: recording continues at every speed (FR-008b). `ReverseHost::OnFrame` and `ShellHistory::RecordFrame` take no speed, so nothing pauses recording at Maximum. The controller's pause stays as `SetRecordingPaused`, since gaps remain (R-010) and the gap tests use it to make them. `SpeedChooser` stays: `CpuManager` and its callers read it beyond history.

**Cost**: at Maximum, history grows per emulated second as at 1x; 035's test runs 4.9% slower with history on than off (R-001). R-002 is the case to watch.

## R-008: The rewind key and drawing backward

**Decision**:

- **F2** is routed in `ShellKeyRouting` to a new owner, `ShellKeyOwner::Rewind`, so the guest never receives it, and its character is swallowed. A press starts rewinding and its release stops it; a repeat while the key is down is ignored. `EmulatorShell::OnKeyUp` intercepts the F2 release, which today goes to the guest unconditionally. While a modal dialog is open, the window receives no keys, so F2 does nothing then (spec edge case).
- **Each frame while F2 is down**, the CPU thread runs `ReverseController::StepBackCycles (kFrameCycles)`, which loads the keyframe at or before the target and replays at most one keyframe interval, then renders and presents the frame. At the oldest point the machine pauses there.
- **Cost per frame**: a stretch is 10 frames (170,300 cycles); at the measured 62x to 141x real time, replaying it takes about 1.2 ms to 2.7 ms, under a 16.7 ms frame. A task measures it on R-001's workloads against SC-002. If a workload misses, the fallback is to replay a stretch once and save its frames (10 framebuffers, about 8.6 MB) for the frames that follow, as Mesen 2 does.
- **Measured** (2026-10-27, Release, `RewindCostTests`: a //e from the DOS 3.3 master, 30 seconds of history, then 300 frames stepped back): the Applesoft lo-res game loop averaged 0.99 ms a frame, its first step 1.33 ms and its worst 2.71 ms; the VERIFY loop averaged 1.70 ms, first 2.50 ms, worst 4.46 ms. No frame came near 16.7 ms, so the fallback was not built.
- **Pacing**: frames are paced at the video rate whatever the speed (`CpuManager::SetPaceForced`), so a rewind at Maximum speed runs at 60 frames a second too.
- **Sound**: a replay mutes the sinks already (`MachineHost::SetOutputMuted`), as FR-006 requires.

**Rationale**: `StepBackCycles` and the per-frame replay exist and are tested; drawing backward is a frame-paced loop around them.

**Alternatives considered**: Backspace, the usual emulator key. Rejected: Apple II software reads it as the left arrow (spec clarification, 2026-10-25).

## R-009: The rewind bar, the marks and the fork flyout

**Decision**:

- **One control.** `DxuiImageStrip` is the strip of both bars (FR-028). The debugger timeline is a toolbar with a dock, as today. The emulator window gains an overlay: a `DxuiToolbar` with one strip entry, drawn over the bottom of the viewport, so the picture is not resized.
- **Opening**: the overlay opens when the pointer enters the bottom 8 DIPs of the viewport, scaled for the monitor's DPI, and closes when the pointer moves off it. While the pointer is captured (paddle mode), the overlay does not open. The overlay takes the clicks over it, so the guest mouse never receives one.
- **Marks.** `IDxuiImageStripSource` gains `GetMarks`, a list of marks with an offset in cells, a kind, a count and a tip; `DxuiImageStrip` paints them and hit-tests them. The kinds: gap, reset, power cycle, disk mount, eject or swap, write-protect change, loaded state, and fork. The source builds them from the journal records on the active path (`InputKind::Reset`, `PowerCycle`, `DiskMount`, `DiskEject`, `DriveWriteProtect`, `ImageWriteProtect`), from `KeyframeInfo::hasGapBefore`, from a new `InputKind::StateLoad`, and from the fork positions of the segment tree on the active path.
- **The fork flyout**: resting the pointer on a fork mark opens a popup from the strip's popup host, with one row per branch: the picture at its live end, how long it ran past the fork, and when the user left it. Resting on a row draws that picture over the viewport. A click posts the switch to the CPU thread.
- **Two sources.** Each strip has a separate `HistoryThumbnails` over the same store, because a source lays out its cells for one strip's size; each draws only while its strip is visible, as today. Each has a separate `ScratchMachineRenderer`, since a renderer draws for one worker thread at a time.
- **After a switch**, the store notifies its listeners that the path changed after the fork position (a new `KeyframeDrop::PathChanged`), and `HistoryThumbnails` drops its pictures from there (`ForgetPictures`).

**Rationale**: FR-003, FR-004, FR-010 to FR-015 and FR-028, on the control the debugger uses today, which supplies thumbnails, previews, scrubbing and the playhead.

## R-010: Saved history

**Decision**:

- **Where**: `MachineStateFile` version 4 adds an optional history section after the payload. Every save writes it while history is on: the automatic state at exit and at a machine switch, and File > Save state (FR-022, FR-025). It is not packed with the payload's MSZIP, because the keyframes are packed already.
- **Its layout**: a size and hash for the section, then the machine state format fingerprint, the keyframe settings, the segment tree with its fork positions and the times the user left each branch, every keyframe's `KeyframeInfo`, the arena's used extents end to end, the journal records of each segment, the media table, and the machine's position when it was saved. [contracts/saved-history-format.md](contracts/saved-history-format.md) has the byte layout.
- **Discarding (FR-024)**: a section whose size or hash fails, whose fingerprint differs from the running build's, or that does not parse is ignored; the machine state loads and history starts fresh, with no message. The fingerprint is a checksum over the section tags and versions a `MachineHost::SaveState` of the running machine writes, so a build whose state format differs is caught without reading a keyframe.
- **Media**: keyframes refer to disks by media id. The section lists, for each id a keyframe refers to, the image path, format and identity (`ImageIdentity`), and the image as in the newest keyframe that refers to it. Loading recreates those disks as retained media with the saved ids, and the id counter starts past them, so no packed keyframe is rewritten.
- **Disks that no longer match** (spec edge case): a disk whose file identity differs at launch is mounted from its file, and the history before the launch is marked with a gap at the launch's keyframe. The stretch before the gap can be viewed and replayed; a change made there is ignored with a notice, so no fork can write a disk version from before the file changed outside Casso.
- **The Uthernet II fence**: 035 drops history and turns recording off while the Uthernet II has a socket or connection open (`ShellDebugger::FenceHistoryForNetwork`, `1fb25e6fd`), since a replay cannot reopen host connections. 045 moves the fence into `ShellHistory` as built: it displays no confirmation and does not change the setting, and the next save writes the history in memory then, which is none while the card is in use.
- **Exit behind live (FR-020)**: the machine seeks to the live end of the active branch, the disks are written there, the state is saved there with the history and the position on screen, and the next launch seeks back to that position, paused.
- **Speed**: SC-007 allows 1 s for loading a full history. A load reads the file, checks the hash, lays the extents into the arena and unpacks the newest whole snapshot, and nothing else. A task measures load and save at the 64 MB default.

**Alternatives considered**: a separate history file beside the automatic state. Rejected after the owner's 2026-10-25 answer: saves include history, File > Save state included, and one format serves both.

## R-011: The disk rule and 041

**Decision**: the disk rule (FR-018 to FR-021) is built last, after 041-disk-integrity is on master and master is merged into 045 (owner decision, 2026-10-26). Until then no 045 task edits `DiskImageStore.h/.cpp`, `MountedImageState`, `CommitPlan` or `Disk2Controller`.

**What 041 changes underneath** (`origin/041-disk-integrity` at `986aea9b6`):

- `FlushEntry` is split into conflict, report and commit steps (`22c5921e8`). Phase 4 routes every commit through `DurableCommit` (T059) and classifies save failures by moment, adding `FlushMoment::Background` for the flushes nobody asked for (T063). The branch writes go through that commit.
- The store and its drives have one owning thread, the CPU thread (`1fb27f396`), which is where history runs, so the branch writes run without marshaling.
- FR-017, "a reset writes only through the store": today the Disk II controller writes its dirty disks itself on a reset, outside `SetFlushHold` (`ReverseController.h`'s comment), so a reset replayed behind live writes the files. FR-019 depends on 041's fix.

**What 045 adds on top**:

- **Branch disk versions**: when a branch is left behind, its segment lists each disk that left a bay after the fork on that branch, with its image as it left, packed with `SnapshotCompressor` and counted against the history budget; the disks in bays at its live end are in its live-end keyframe.
- **A switch or a fork** compares, for every disk touched after the fork on either path, the old and new active versions by checksum, and writes each that differs through the store's commit (FR-018, FR-021, SC-005). A disk the guest never wrote is not written.
- **Forced writes behind live** that do not fork (a machine switch, exit) write the live end's version (FR-020, SC-006): the machine seeks to the live end first, as R-010 does at exit.
- **Retained media** are pruned by the set of media ids any segment's keyframes refer to, in place of `PruneRetainedMedia (oldestPosition)`, since a disk ejected on a branch left behind must stay retained while that branch exists.

**Rationale**: the rule touches the flush, commit and retention paths 041 is restructuring. Building on 041's store avoids writing the rule twice and spares 041's merge any 045 conflicts.

## R-012: Settings

**Decision**: the history settings (on or off, budget, keyframe interval) move from `DebuggerSettings` to a `HistorySettings` member of `GlobalUserPrefs` (`CassoEmuCore/Config/HistorySettings.h`), under the JSON keys they have today, `reverseRecording`, `reverseBudgetMb` and `reverseIntervalFrames`: several builds share one preferences file, and with new keys a build that reads the old ones would go on recording after another turned history off. The debugger's `HistoryOptionsPage` is the History page of the Settings sheet as well (FR-008), so both edit the same settings through one page, which gains a keyframe interval slider; a new interval applies from the next start of history, since a keyframe schedule changed partway through history would mix two.

**Turning history off (FR-026)**: either page first displays a confirmation dialog whose text specifies that all of the machine's history, in memory and saved, will be deleted. On confirmation, recording stops, the store is cleared, and the next save writes no history section. After a cancel, the setting is on.

## R-013: The replay indicator

**Decision**: a chrome overlay, drawn over the top of the viewport below `ShellChrome::ComputeTopOverlayEdgePx`, with the text "Replaying" and the host's time of the position, in the format of the timeline's labels (`DebuggerStatusText::FormatWallClock`), its opacity pulsing on the chrome's animation tick. It is drawn while the machine is behind live and running, and removed when the machine is live, paused or forked. A capture paint hides it with the other application overlays (`SetStandInOverlaysHidden`). The caption's replay note remains.

## R-014: Menus and keys in the emulator window

**Decision** (approved with the plan, 2026-10-26): a History menu in the emulator window's main menu, with Rewind (F2, while it is down), Step back (Ctrl+F11, beside the window's Step on F11), Go live (Ctrl+F2), and History on (a check item, with FR-026's confirmation when cleared). The key strings come from the command table, as for the menu's other items (`EmulatorCommands.cpp`).

**Why Ctrl**: a modifier's press goes to the guest while the machine is live, before the command moves it into history. Alt is Open Apple and the //e's Shift is a readable line (`InputLine::Shift`), but Apple II software cannot read the Control key alone, so Ctrl+F11 and Ctrl+F2 change nothing in the guest. The debugger's keys (Alt+F11 and the Ctrl+R chords) are not changed, since the debugger window sends no keys to the guest.
