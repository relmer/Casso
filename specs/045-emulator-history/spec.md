# Feature Specification: Machine History in the Emulator

**Feature Branch**: `045-emulator-history`, to be cut from `035-debugger` (the spec was written there)

**Created**: 2026-10-25

**Status**: Draft

**Input**: Owner decisions of 2026-10-24 and 2026-10-25: make machine history (the captured keyframes, the timeline and rewinding) a feature of the emulator itself, not only of the debugger; fork history instead of discarding it; persist it across launches.

## Context

Spec 035 built reverse execution for the debugger. History is captured whenever the setting is on, which it is by default, whether or not the debugger window is open. But the timeline and the commands are only in the debugger window, and the settings are debugger settings. A user who never opens the debugger has history that is never used.

This feature adds history to the emulator window, makes it non-destructive (resuming in the past forks history rather than discarding the future), defines what a disk image file contains when history has several versions of a disk, and saves history across launches.

## Clarifications

### Session 2026-10-25

- Q: Is history on by default? -> A: Yes; it already is, and stays so.
- Q: What happens to the future when the user resumes in the past? -> A: Resuming is silent, with no prompt, and forks history: the future left behind becomes a branch the user can return to.
- Q: How are branches drawn? -> A: One unified, collapsed timeline: the path from the oldest point to the live end of the active branch, every other branch collapsed to a fork mark where it split. A flyout on a mark lists its branches; a click switches. No lanes.
- Q: Where does switching to a branch land? -> A: At that branch's live end, paused, as a seek does.
- Q: Do branches get names? -> A: No. Fork marks and branch rows are labeled by time only.
- Q: Audio while scrubbing or rewinding? -> A: Silent.
- Q: Does history persist across launches? -> A: Yes, branches and all.
- Q: What does a disk image file contain when branches have different versions of a disk? -> A: The active branch's version. When the active branch changes, the new branch's version of each disk is written to its image file.

## User Scenarios & Testing *(mandatory)*

All stories ship together in one merge. Priorities order the work; they do not mark a releasable subset.

### User Story 1 - Rewind in the emulator window (Priority: P1)

A player makes a mistake in a game, holds the rewind key, watches the game run backward on screen, lets go a few seconds earlier and plays on. No debugger is involved.

**Why this priority**: This is the feature most users would use, and the reason for bringing history out of the debugger.

**Independent Test**: With the debugger never opened, run a program, hold the rewind key for a few seconds, release, then press a key; the machine runs backward with frames on screen, pauses on release, and continues from there.

**Acceptance Scenarios**:

1. **Given** a running machine with history captured and the debugger closed, **When** the user holds the rewind key, **Then** the machine runs backward with each frame drawn on screen and no sound, and stops at the oldest point in history if the key is down that long.
2. **Given** the user is rewinding, **When** the key is released, **Then** the machine pauses at the frame on screen.
3. **Given** the machine is paused in the past, **When** the user presses Run or any key the guest reads, **Then** the machine runs on from that frame with no prompt.
4. **Given** a running machine, **When** the user moves the pointer to the bottom edge of the screen, **Then** the rewind bar appears: a strip of thumbnails covering the history, with the current position marked; **When** the user moves the pointer off the bar, **Then** the bar hides.
5. **Given** the rewind bar is open, **When** the user drags along it, **Then** the frame under the pointer is drawn on the main screen, silently; **When** the user lets go and presses Run, **Then** the machine runs on from that frame.
6. **Given** the history includes a reset, a power cycle, a disk swap or a loaded saved state, **When** the rewind bar is open, **Then** each appears as a mark at its moment.
7. **Given** the emulator window, **When** the user opens its menus, **Then** the history commands are there (go live, step back, rewind, turn history on or off) with their keys.

---

### User Story 2 - Fork history instead of discarding it (Priority: P1)

A player rewinds two minutes, tries a different route, and later wants the first attempt back. Nothing was lost: the first attempt is a branch, opened from a mark on the timeline.

**Why this priority**: A silent resume that discarded the future would make rewinding destructive; forking makes it safe to experiment.

**Independent Test**: Play to a point, rewind, resume and play on; a fork mark appears where the timeline split; open it and switch to the first branch; the machine is exactly where that branch ended.

**Acceptance Scenarios**:

1. **Given** the machine is paused in the past, **When** the user resumes, **Then** history forks at that point with no prompt: the new timeline becomes the active branch, and the future left behind becomes a branch drawn as a fork mark at the fork point.
2. **Given** several branches fork at one point, **When** the timeline is drawn, **Then** one mark is drawn there with a count of its branches.
3. **Given** a fork mark, **When** the pointer rests on it, **Then** a flyout lists one row per branch, each with the branch's last frame, how long it ran past the fork, and when the user left it; **When** the pointer rests on a row, **Then** the main screen previews that branch's last frame.
4. **Given** the flyout is open, **When** the user clicks a row, **Then** that branch becomes active, the machine is paused at its live end in the exact state it had there, the timeline redraws as that branch's path, and the branch left becomes a mark at the same fork point.
5. **Given** the active branch, **When** the user rewinds or scrubs, **Then** movement stays on the active branch's path and never crosses into another branch.
6. **Given** a branch with forks in it, **When** it is not active, **Then** its forks are not drawn; **When** the user switches to it, **Then** its forks appear on its path.

---

### User Story 3 - Disk image files follow the active branch (Priority: P1)

A player whose game saves to disk forks and switches branches. The disk image file on the host contains the active branch's version of the disk, so the file always matches the game being played.

**Why this priority**: A disk image file is the user's real data and exists once, while history has many versions; without a rule, forking could silently overwrite a save, or the file could end up matching neither branch.

**Independent Test**: In a branch, make the guest write to disk; switch to a branch whose disk differs; the image file contains the newly active branch's version; switch back and it contains the first again.

**Acceptance Scenarios**:

1. **Given** branches whose versions of a disk differ, **When** the active branch changes by a switch or by resuming in the past, **Then** each disk's image file is written with the new active branch's version.
2. **Given** the user is behind live without resuming (rewinding or scrubbing), **When** the guest's drive stops, the machine resets or the machine is power cycled in replay, **Then** nothing is written to any image file.
3. **Given** the user is behind live without resuming, **When** a write is forced by an eject, a machine switch or exit, **Then** the image file gets the version at the live end of the active branch, never the version of the moment on screen.
4. **Given** branches with different disks in a drive, **When** the user switches branch, **Then** the drive contains the newly active branch's disk, and the rule above applies to each disk's file separately.

---

### User Story 4 - History persists across launches (Priority: P2)

A user closes Casso and opens it the next day; the timeline goes back into yesterday's session, with its branches.

**Why this priority**: History that disappears at exit cannot help with a mistake noticed after a restart.

**Independent Test**: Record history with a fork, exit, relaunch; the timeline spans the same history with the same fork marks, and switching branches works as it did before the exit.

**Acceptance Scenarios**:

1. **Given** captured history, **When** Casso exits or switches machines, **Then** the history, every branch and the disks in it are saved in a separate file beside the machine's automatic state.
2. **Given** saved history for a machine, **When** Casso launches into that machine, **Then** the timeline spans the saved history, branches included.
3. **Given** saved history from a build whose machine state format differs, or saved history that does not read, **When** Casso launches, **Then** the saved history is discarded and history starts fresh at the launch, with no error message.
4. **Given** the user saves a machine state file, **When** the option to include history is on, **Then** the file contains the history too, and loading it restores the timeline; with the option off, a loaded state starts history fresh, as today.

---

### User Story 5 - One history for the emulator and the debugger (Priority: P2)

A developer rewinds in the emulator window, opens the debugger and finds the same timeline, same position and same branches, with the debugger's marks added.

**Why this priority**: Two histories would disagree; the debugger's reverse features depend on the same recording.

**Independent Test**: Fork history in the emulator window, open the debugger; its timeline has the same path and fork marks, plus its gap and stop marks, and a branch switch in either window appears in both.

**Acceptance Scenarios**:

1. **Given** history captured with the debugger closed, **When** the debugger opens, **Then** its timeline has the same history, position and branches as the emulator window.
2. **Given** the debugger timeline, **When** it is drawn, **Then** it adds the debugger's gap and stop marks to the same strip, fork marks included.
3. **Given** the debugger's stops and reverse continue, **When** used on the active branch, **Then** they work as 035 defines them.
4. **Given** the history settings, **When** the user looks for them, **Then** they are in the general preferences (on or off, memory budget, keyframe interval), not only in the debugger's options.

---

### Edge Cases

- The rewind key down at the oldest point in history: the machine stays paused there.
- History full while the user plays: the oldest keyframes of branches the user has left are dropped first, then the oldest of the active branch's path; a branch with no keyframes left disappears with its mark.
- More branches left behind than the cap: the oldest left branch is dropped whole.
- Resuming at a point that is already a fork mark: the new timeline joins that mark's branches.
- A branch switch while the debugger is stopped at a breakpoint: the switch is a seek; the debugger displays the new position as it does after any seek.
- The pointer near the bottom edge while the guest uses the mouse (an Apple mouse card): the rewind bar opens only while the host pointer is over the bottom band, and the guest never receives a click on the bar.
- The rewind key pressed while a modal dialog is open: ignored, as other emulator keys are.
- A disk image file changed outside Casso while history has versions of it: the existing "Disk modified outside Casso" prompt runs as today; inserting the changed disk is a disk change in the active branch at the live end.
- History saved in an earlier session whose disks no longer match the files at launch: the stretch captured with other disks is drawn past a gap mark, as 035 draws history saved with other disks, and can be viewed.
- A machine switch: the outgoing machine's history is saved with its automatic state; the incoming machine's saved history is loaded.
- History turned off: recording stops, the rewind bar and key are disabled, and the saved history file for the machine is deleted at the next save.

## Requirements *(mandatory)*

### Functional Requirements

**History in the emulator window**

- **FR-001**: History MUST record and replay with the debugger window closed, using the same recording the debugger uses, and MUST be on by default.
- **FR-002**: The emulator window MUST offer a rewind key: while it is down the machine runs backward with each frame drawn, and on release it pauses at the frame on screen. The key MUST be a host key the guest never receives.
- **FR-003**: The emulator window MUST open a rewind bar along the bottom of the screen when the pointer enters a band at the bottom edge, and close it when the pointer moves off the bar; the bar MUST NOT cover the screen otherwise.
- **FR-004**: The rewind bar MUST display thumbnails across the active path, the current position, and marks for resets, power cycles, disk swaps and loaded saved states.
- **FR-005**: Dragging along the rewind bar MUST preview the frame under the pointer on the main screen; releasing and pressing Run, or a key the guest reads, MUST continue from that frame.
- **FR-006**: Audio MUST be silent while rewinding and while scrubbing.
- **FR-007**: The emulator window MUST offer the history commands (go live, step back, rewind, history on or off) in its menus with their keys.
- **FR-008**: The history settings (on or off, memory budget, keyframe interval) MUST be in the general preferences.

**Forking**

- **FR-009**: Resuming from a point in the past MUST NOT prompt and MUST NOT discard history: it forks history at that point, makes the new timeline the active branch, and makes the future left behind a branch.
- **FR-010**: The timeline MUST display one path, from the oldest point in history to the live end of the active branch. Every other branch MUST appear only as a fork mark at the point where it split from that path, with a count when several branches split there.
- **FR-011**: Resting the pointer on a fork mark MUST open a flyout with one row per branch: the branch's last frame, how long it ran past the fork, and when the user left it. Resting on a row MUST preview that last frame on the main screen.
- **FR-012**: Clicking a branch row MUST make that branch active and pause the machine at its live end, in the exact machine state it had there. The timeline MUST then display that branch's path, and the branch left MUST appear as a fork mark at the same point.
- **FR-013**: Rewinding and scrubbing MUST move along the active branch's path only.
- **FR-014**: The forks within a branch MUST appear only while that branch is active.
- **FR-015**: Branches MUST NOT have names; fork marks and branch rows MUST be labeled by time only.
- **FR-016**: All branches MUST share the history budget. When it is full, the oldest keyframes of branches the user has left MUST be dropped before any of the active branch's path; a branch with no keyframes left MUST disappear with its mark.
- **FR-017**: The number of branches left behind MUST be capped (default 8); past the cap, the oldest left branch MUST be dropped whole.

**Disk images**

- **FR-018**: Each disk image file MUST contain the active branch's version of that disk. When the active branch changes, by a switch or by resuming in the past, each disk whose version differs MUST be written to its image file with the new active branch's version.
- **FR-019**: While the user is behind live without resuming, the writes that the drive motor stopping, a reset and a power cycle would make MUST stay deferred, as today, and nothing MUST be written during a replay.
- **FR-020**: A forced write while behind live (eject, machine switch, exit) MUST write the version at the live end of the active branch, never the version of the moment on screen.
- **FR-021**: Switching to a branch that had a different disk in a drive MUST put that branch's disk in the drive; FR-018 MUST apply to each disk separately.

**Persistence**

- **FR-022**: At exit and at a machine switch, history MUST be saved, with every branch and the disks in it, in a separate file beside the machine's automatic state.
- **FR-023**: At launch into a machine, its saved history MUST be loaded, and the timeline MUST span it, branches included.
- **FR-024**: Saved history from a build whose machine state format differs, or saved history that does not read, MUST be discarded silently, and history MUST start fresh.
- **FR-025**: Saving a machine state file MUST offer an option to include history, off by default; a state file with history MUST restore the timeline when loaded.
- **FR-026**: Turning history off MUST stop recording, disable the rewind bar and key, and delete the machine's saved history file at the next save.

**One history**

- **FR-027**: The emulator window and the debugger MUST use one history: the same recording, position and branches.
- **FR-028**: The debugger timeline MUST be the same control as the rewind bar, with the debugger's gap and stop marks added.
- **FR-029**: The debugger's stops and reverse continue MUST work on the active branch as 035 defines them.
- **FR-030**: The cost of capturing history MUST be measured on ordinary software (games and applications at 1x, not stress workloads), and the result written in the plan's research.

### Key Entities

- **History**: everything captured for one machine: keyframes, the inputs between them, the disks in them, and the branches; bounded by a memory budget.
- **Branch**: one line through history from a fork point (or the oldest point) to its live end; exactly one branch is active at a time.
- **Fork point**: a moment where two or more branches split; drawn as a fork mark on the path through it.
- **Active branch**: the branch the machine is on; its path is the one displayed, and the image files contain its version of each disk.
- **Live end**: the newest moment of a branch.
- **Saved history**: history written to a separate file beside a machine's automatic state, or inside a machine state file when that option is on.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001a**: With history on, emulating ordinary software at 1x uses no more than 1% of one core beyond what it uses with history off, on the measured workloads. This is the whole cost of history, most of which recording under 035 already spends: 035 measured the //e at 70.5 times real time without history and 46.6 times while recording (R-040, 2026-10-03), about 1.4% and 2.1% of one core at 1x.
- **SC-001b**: The work in this feature adds no measurable cost per emulated frame beyond 035's recording, within run-to-run noise on the same workloads; branches, rewind bar thumbnails and saved history cost memory or run at exit and at quiet moments, not per frame.
- **SC-002**: Rewinding starts within 100 ms of pressing the rewind key and draws at least 20 frames per second of machine time going backward on a //e.
- **SC-003**: Releasing the rewind key pauses the machine within one frame; resuming runs with no confirmation.
- **SC-004**: After resuming in the past, switching back to the branch left behind restores the machine state at that branch's live end exactly (the same state checksum as when the user left it).
- **SC-005**: After every change of active branch, each disk image file matches the new active branch's version of that disk byte for byte.
- **SC-006**: Exiting while behind live never writes a past moment's disk contents: each image file matches the active branch's live end.
- **SC-007**: History saved at exit is back at the next launch with the same span and fork marks; loading a full history at the default budget adds no more than one second to the launch.
- **SC-008**: The rewind bar appears within 150 ms of the pointer entering the bottom band and hides when the pointer moves off it.

## Assumptions

- The rewind key is a host shortcut, chosen in planning, among keys the guest never receives; it can be rebound where the emulator's keys can be.
- Rewinding runs backward at normal speed by default.
- The bottom band that opens the rewind bar is a few pixels tall and works in windowed and full screen modes.
- The branch cap defaults to 8 branches left behind.
- Saved history uses the history's existing compressed form, so its file is at most about the memory budget (64 MB by default).
- The disk rule writes only disks whose versions differ between the old and new active branch; a disk the guest never wrote is not rewritten.
- The debugger's history UI and commands, stops, reverse continue, and heat map and call stack replays work as 035 defines them, on the active branch.
- The branch is cut from `035-debugger`, since this feature builds on 035's history code, and work may start before 035 merges to master. It takes 035's later commits by merge only, and merges to master after 035 does, in one merge that ships the feature whole.
- The disk image rule (FR-018 to FR-021) touches the disk code that 041-disk-integrity is restructuring (also cut from `035-debugger`); the plan coordinates those changes with 041.
- The order of work is build order only: the ownership move and settings, then the shared timeline control and the emulator window's rewind bar and keys, then forking and the branch marks, then the disk rule, then persistence.

## Out of Scope

- Bookmarks with labels, and branch labels other than times.
- An expanded, multi-lane view of branches (the debugger may add one later).
- Changes to how the debugger's stops and reverse continue work.
