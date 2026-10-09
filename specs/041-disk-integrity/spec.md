# Feature Specification: Disk integrity

**Feature Branch**: `041-disk-integrity`

**Created**: 2026-10-08

**Status**: Draft

**Input**: User description: "Disk integrity: make the disk subsystem single-owner and fix the confirmed disk data-loss and crash defects"

## Merge order *(binding)*

This branch is built on `035-debugger` (cut at `811a6f727`), because 035 holds
about 5,100 unmerged lines in 27 of the files this work changes. It takes 035's
later commits by merge, and nothing flows from this branch into 035.

**It does not merge to `master` until 035 has.** After 035 reaches `master`,
this branch merges `master` (which then holds 035's own resolution of its
conflicts with `master`), re-runs every gate, and only then merges. It never
merges `master` on its own before that, because doing so would re-resolve
035-versus-`master` conflicts that the 035 session will resolve differently.

## Background

A code audit of the disk subsystem found the defects this spec fixes. Each was
confirmed by at least two of three independent reviewers on `master`, then
re-checked against 035's code. Four were already fixed by 035 and are out of
scope: the Restart change answer leaving both drives empty, the two debugger
step defects, and Eject leaving the Salvage command enabled. One, the update
install flushing on the wrong thread, exists only on `master` and is handled
after the `master` merge (FR-019).

The underlying fault is ownership. The disk store, the disk images and the
Disk II drive are changed by the emulation thread, while the window thread,
the folder watcher and the settings code read and sometimes change the same
state with nothing coordinating them. Most crash and wrong-label defects follow
from that. The data-loss defects are independent logic errors in the write path.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - What the guest saves stays saved (Priority: P1)

A user runs software that writes to a disk image, in any supported format, over
a session of any length, and the image file on the host ends up holding exactly
what the guest wrote, through every combination of saves, ejects, re-inserts,
resets, write-protect changes and failures along the way.

**Why this priority**: Silent loss of the user's work is the worst failure an
emulator with writable disks can have, and several of the paths that cause it
are ordinary: two saves in one session on a `.nib`, a write-protect toggle, a
reset after a failed save.

**Independent Test**: Run the write-path regression suite: each scenario writes
through the emulated drive, triggers the flush event in question, and compares
the image file byte-for-byte with the expected result.

**Acceptance Scenarios**:

1. **Given** a writable `.nib` or `.nb2` image, **When** the guest saves twice
   with the motor stopping in between, **Then** the file holds both saves.
2. **Given** a disk whose last save failed (locked file, full volume), **When**
   the user re-inserts it, resets, power-cycles, switches machine or ejects,
   **Then** the unsaved writes are kept and the message about them is true.
3. **Given** a disk with unsaved writes, **When** the user write-protects the
   drive from Settings, **Then** the writes are saved before protection applies.
4. **Given** a disk image that has just been saved, **When** the host loses
   power immediately afterward, **Then** the file holds either the previous or
   the new contents in full, never a truncated or zero-filled image.
5. **Given** a sector image whose guest data no longer decodes, **When** the
   motor stops repeatedly, **Then** one recovery copy is kept up to date and the
   user is told once, not once per stop.
6. **Given** a reset after a re-insert whose file could not be read, **When** the
   reset runs, **Then** no write bypasses the checks every other save goes
   through.
7. **Given** an image saved over an existing file, **When** the save completes,
   **Then** the file keeps its attributes and permissions.

---

### User Story 2 - Disk operations never crash or corrupt memory (Priority: P1)

A user inserts, ejects, swaps and salvages disks, and has build tools rewrite
mounted images, while the machine runs, and the emulator never crashes, never
shows a drive in a state it is not in, and never reads a disk that has been
freed.

**Why this priority**: The audit found a single-thread use-after-free on an
ordinary action (inserting a damaged image into an occupied drive), and many
paths where the window reads disk state while the emulation thread replaces
it. These crash rarely and unreproducibly, which is the hardest kind to report.

**Independent Test**: Unit tests drive each mount, eject, reload and salvage
path, including failures, and assert that the drive and the store match
afterward; a stress test changes disks on the emulation thread while another
thread reads the published status, under the address sanitizer.

**Acceptance Scenarios**:

1. **Given** a drive holding a disk, **When** the user inserts a file that reads
   but does not load, **Then** the original disk stays mounted and in use, and
   the user is told the new one could not be used.
2. **Given** a running machine, **When** disks are mounted, ejected, swapped or
   reloaded, **Then** every on-screen indication (drive labels, lamps, head
   position, tooltips, menu state, the desk scene) reflects either the state
   before or the state after the change, never a mixture or freed memory.
3. **Given** a damaged disk, **When** the user salvages it and inserts the copy,
   **Then** the assessment, the write and the insert all happen without racing
   the running machine, and the window stays responsive.
4. **Given** a build tool rewriting a mounted image, **When** the change is
   picked up, **Then** a pending flux write from before the change is never
   spliced into the reloaded image.
5. **Given** the Disk II debug panel being opened or closed, **When** the
   machine is running or switching, **Then** no device is left pointing at a
   panel that has gone.

---

### User Story 3 - Drive status and external changes are accurate (Priority: P2)

A user who mounts disks from the command line, by drag and drop or through the
picker sees the right name, protection and activity for each drive, and changes
made to those files by other programs are noticed.

**Why this priority**: Wrong or missing status misleads without losing data;
missed external changes break the build-and-run loop the command line exists
for.

**Independent Test**: Unit tests over the status publisher and the watcher
registration, plus a scenario that mounts by relative path and rewrites the file.

**Acceptance Scenarios**:

1. **Given** Casso started with a relative `--disk1` path, **When** another
   program rewrites that file, **Then** Casso detects the change, exactly as for an
   absolute path.
2. **Given** a drive whose disk is swapped for one in another folder, **When**
   the swap completes, **Then** the old folder is no longer watched.
3. **Given** layout code sizing the window, **When** the machine is being
   switched at that moment, **Then** layout reads a consistent description of
   the machine.

---

### User Story 4 - Forty-track disks and blank tracks work (Priority: P2)

A user mounts a 40-track image, or a third-party WOZ with blank tracks, and the
guest can read every track the image has and format the blank ones.

**Why this priority**: Real Disk II drives reach track 39 and the WOZ format
maps 160 quarter tracks; software that uses tracks 35 to 39 currently fails, and
formatting a third-party blank WOZ stops with an I/O error.

**Independent Test**: Scenario tests seek to track 39 and read it, and format a
WOZ whose tracks are unmapped, then read back the result from the saved file.

**Acceptance Scenarios**:

1. **Given** a 40-track WOZ, **When** the guest steps the head to track 39,
   **Then** it reads track 39's data; the head stops at quarter track 158, the
   last half-track position of track 39, and the end-stop sound plays on the
   next outward step.
2. **Given** a WOZ with unmapped tracks, **When** the guest formats the disk,
   **Then** every track is written and the saved file maps them.

---

### User Story 5 - Settings are saved consistently (Priority: P3)

A user changes settings while disks mount and the machine switches, and every
change is saved, with no torn or lost preference file.

**Why this priority**: The settings store is read and written from both
threads; the result is rare corruption of the user's preferences.

**Independent Test**: A stress test saves preferences from two threads at once
and checks the file parses and holds both changes.

**Acceptance Scenarios**:

1. **Given** a disk mount saving its path while the user changes a setting,
   **When** both save at once, **Then** the preference file holds both changes.

### Edge Cases

- A mount fails while reverse execution is retaining media: the outgoing disk
  must not be retired, so history does not record an empty drive the live run
  never had.
- The flush hold is set (the machine is behind live in its history): a mount
  that fails writes nothing, as a mount that did not happen should.
- A save fails, then the user quits: the rescue question runs, for a disk whose
  file cannot be written as well as for one changed by another program, and
  when no rescue happens the user is told the changes cannot be kept.
- A disk command reaches the emulation thread while the machine is behind live
  in its history (it was posted before a queued step back ran): the disk is not
  changed and the user is told.
- The image watcher reports a change for a path while that path's bay is being
  remounted.
- A drive is ejected while its debug panel is open.
- A recovery copy itself cannot be written (no room): the user is told once,
  and the disk keeps its unsaved writes.
- The head is stepped past track 39: it stops there.
- An update is installed while the machine is behind live, by any path (an
  MSIX update installed at once, a zip update installed now, or an update
  left until Casso closes): the machine is returned to the live end and the
  disks are saved as they are there; if the machine cannot be returned there,
  they are saved where it stands, and the user is told unless Casso is
  already closing (FR-019).
- A WOZ track record claims data at a location it misstates, or more data
  than its blocks hold: the record is damage, the image is write-protected,
  and the guest cannot format that track (FR-021).

## Requirements *(mandatory)*

### Functional Requirements

**Ownership**

- **FR-001**: The emulation thread MUST be the only thread that reads or changes
  the disk store's bays, the disk images it owns, and the Disk II controller and
  nibble engine state.
- **FR-002**: Every other thread MUST read drive status (mounted path, format,
  write protection and its causes, damage, salvage availability, motor, head
  position, activity) only from a copy the emulation thread publishes, and the
  copy MUST be consistent within itself.
- **FR-003**: The published status MUST be refreshed while the machine is paused,
  so a mount or eject made while paused shows at once.
- **FR-004**: Changes requested from the window (user write protection, salvage,
  attaching debug panels) MUST run on the emulation thread.
- **FR-005**: Notice of an external change to a file, from the folder watcher or
  from the command-line tools, MUST be recorded without touching bay state, and
  matched to bays on the emulation thread.
- **FR-006**: Window layout MUST read machine facts (drive controller, case
  switches, built-in drive, slots, external drive) from a consistent copy taken
  under the machine lifetime lock.
- **FR-007**: The settings store and the window's global preferences MUST be safe
  to read and save from more than one thread.
- **FR-008**: The drive widget's event queue MUST be safe to fill on one thread
  and drain on another.

**Mount, eject and reload**

- **FR-009**: A mount into an occupied drive MUST load the new image before
  retiring the old one; if the new image does not load, the old disk MUST stay
  mounted, attached to the drive, and unretired. When the new file is the old
  disk's own file (a re-insert, a reset or power-cycle remount, the
  remount of a machine switch), the old disk MUST be saved first and the file
  read after the save, so the image loaded is the one just written; when the
  file is unchanged since that save, the mounted disk stays as it is.
- **FR-010**: A mount, re-insert, eject, reset, power cycle or machine switch MUST
  NOT discard unsaved writes when the save before it fails; the disk and its
  writes MUST be kept and the user told truthfully.
- **FR-011**: Replacing a drive's disk MUST stop watching the old disk's folder
  when no other mounted disk uses it.
- **FR-012**: A reload of an externally changed file MUST commit any open flux
  write to the image it was made on, so the write counts toward the conflict
  and is kept with the guest's version, and MUST never splice it into the
  reloaded image.
- **FR-013**: Command-line disk paths MUST be made absolute before mounting.

**Write path**

- **FR-014**: After a `.nib` or `.nb2` image is saved, later saves in the same
  session MUST build every track from the guest's current data.
- **FR-015**: Applying user write protection to a disk with unsaved writes MUST
  save them first; a failed save MUST leave the disk writable with its writes.
- **FR-016**: Repeated failed saves of the same session MUST update one recovery
  copy and report once, until the session is saved or the disk leaves the drive.
- **FR-017**: A reset MUST NOT write any disk through a path that skips the
  store's checks (identity, preserved copy, write protection, recovery).
- **FR-018**: A completed save MUST be durable against power loss once reported,
  and MUST keep the replaced file's attributes and permissions. This covers the
  emulator's saves, the `casso disk` commands, `casso debug --write-disks` and
  Cassque.
- **FR-019**: After the `master` merge, the update installer's final save MUST
  run with the emulation thread stopped, never concurrently with it. For every
  update install path (the MSIX deploy, from the update dialog or from a
  pending bundle; a zip update installed now, which closes Casso the ordinary
  way; and an update left until Casso closes), when the machine is behind live
  in its history, it MUST be returned to the live end first and the disks
  saved as they are there (owner decision, 2026-10-08; owner confirmed
  2026-10-09 for all three paths); when it cannot be returned there, the disks
  MUST be saved where the machine stands and the user told, except on the
  path that runs while Casso is already closing. A quit with no update pending
  is unchanged: it saves the disks where the machine stands.

**Drive emulation**

- **FR-020**: The Disk II head MUST travel to quarter track 158, the last
  half-track position of track 39 (the stepper moves in half tracks, so an odd
  stop would strand the head between detents), and the end-stop sound MUST
  follow the new limit.
- **FR-021**: On a writable WOZ image, a guest write over a whole-track position
  with no stored data MUST create a full-capacity bit track there, map it, and
  save it into the image. A bit track record whose bit count is zero has no
  stored data, whatever its start block and block count (owner confirmed
  2026-10-09 for a start block of 3 or more); an empty flux record stays an
  empty flux track, which the guest can already write. A record with a count
  above zero that claims data at a location it misstates (a zero start block
  or block count, or a start block below 3, inside the file's header), or
  more data than its blocks hold, and a map entry from 160 to 254, are damage:
  the image is write-protected, and no track is reserved or created anywhere
  on an image with any damage. That record rule is spec 040's (its FR-053),
  and this work takes 040's implementation of it. Sector and nibble images,
  whose geometry is fixed, are unchanged.

**Documentation**

- **FR-022**: Comments and documents that state ownership, atomicity, flush
  triggers or write paths the code does not have MUST be corrected, including
  `DiskImageStore.h`, `Disk2Controller.h`, `docs/disk-write-integrity.md` and
  the "Threading model", "Devices and the per-instruction tick" and "Disks"
  sections of `ARCHITECTURE.md`. The "Disks" section (§8) reached `master` in
  merge `1390af1a5` and is not on this branch, which is built on 035, so the
  `ARCHITECTURE.md` changes are made after the `master` merge brings it.

### Key Entities

- **Bay**: one drive position in a slot; holds at most one mounted image, its
  path, format, identity, watch state and pending external change.
- **Disk image**: the in-memory disk: tracks, quarter-track map, dirty state,
  write-protect causes, damage.
- **Published drive status**: the emulation thread's copy of each bay and each
  slot 6 drive's activity, read by every other thread.
- **Recovery copy**: the lossless WOZ written beside an image whose guest data
  cannot be saved in its own format.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every confirmed defect in scope has a regression test that fails
  before its fix and passes after.
- **SC-002**: The drive-status stress test passes in the Debug, Release and
  AddressSanitizer builds. It performs 10,000 mount, eject, swap and reload
  operations on a thread holding the disk store's ownership while another
  thread reads the published status throughout, and passes only when every
  status read equals a state the owning thread published, each published state
  is read at least once, the published status equals the live bay after every
  operation, and the AddressSanitizer build ends with no report. Every
  committed test passes in the AddressSanitizer build. A local change, never
  committed, that makes the test read the live bays instead of the published
  copy fails in the AddressSanitizer build; the commit that adds the test
  records that result.
- **SC-003**: No code outside the emulation thread touches the disk store, its
  images or the Disk II drive. In Debug builds this is shown by an ownership
  assertion on every guarded entry point, a sweep that calls each one from the
  wrong thread and checks that it asserts, a stand-in test for each window-thread
  reader that moved, and a run of the app through mounting, ejecting, the
  menus, the Disk II debug panel, a machine switch and quit with no ownership
  assertion.
- **SC-004**: The full unit and scenario suites pass in Debug and Release, with
  the unit test count equal to the baseline plus the tests this spec adds
  (those taken from spec 040 included), less the real-file tests it moves
  from the unit suite to the scenario suite, 65 at the inventory of
  2026-10-09 (tasks.md T057), so that no unit test creates, writes, renames,
  deletes or watches a real file or folder (plan.md lists the read-only cases the owner
  left in the unit suite on 2026-10-09). The
  baseline is the test count of the 035 (later `master`) commit most recently
  merged into this branch, measured at that merge and recorded in tasks.md; it
  started at 9,525 Debug and 9,519 Release (035 at `811a6f727`).
- **SC-005**: Idle CPU cost of the drive status publish is under 1% of a frame.
- **SC-006**: A reset, power cycle, re-insert or machine switch taken while the
  guest has unsaved writes leaves those writes in both the file and the
  mounted disk.

## Assumptions

- 035's debugger, reverse execution and flush hold land on `master` in their
  current form; this work follows their model (emulation-thread publish,
  snapshot read) and is re-checked against them at each 035 merge.
- The debugger view snapshot is the established pattern for publishing
  emulation-thread state, and the drive status follows it.
- Raising the head stop to track 39 matches real drives closely enough for the
  software that uses those tracks; no copy protection known to rely on a stop at
  track 35 is in the test corpus.
- A track created by a guest write gets the full track capacity a blank disk
  made by Casso already uses, 51,200 bits.
- Defects found in 035's own code during this work are reported to the 035
  session at the start of the work (tasks.md T006), not fixed here, except
  where 041's own change removes one, as noted. Six were reported on
  2026-10-08, with the owner's approval, as defects a to f, and each item
  below gives its letter. 035 fixed a, b, d, e and f the same day on
  `origin/035-debugger` (tip `d3c15b55c`), each commit citing "041 audit
  defect <letter>", merged in 035 at `710531848` (a, d, f) and `21306089a`
  (b, e); for c it took only a comment (`fe0427676`). tasks.md T008 brings
  all of it, and tasks.md "What 035 brings" gives the commits. Items 7 to 10
  were not among the six; 035 fixed item 10 as well. Paths are under
  `CassoEmuCore/`.
  1. Defect a, fixed by 035 in `c913c9247` and `65499c194`.
     `ReverseController::Stop` cleared the flush hold but not the replay
     flag, which `ReplayHere` leaves set after a forward step from the past
     (`Debugger/Reverse/ReverseController.cpp:137-144`, `:1885`, `:1913`), so
     quit then ran `FlushAllForShutdown`, which returns at `FlushEntry`'s
     replay check (`Devices/Disk/DiskImageStore.cpp:1090`), and saved nothing
     (threading-035.md §3). `Stop` now clears the flag and unmutes the
     printer. T008 runs 035's `QuittingAfterAStepFromThePastWritesTheDisk`
     and `SwitchingMachinesAfterAStepFromThePastSavesAndRemountsTheDisk` on
     the merged code, and this branch does not merge until T137 confirms them
     on the merged `master`; if they fail, the owner decides rather than the
     branch merging. 041's own design does not lean on the fix elsewhere:
     R2's decline tests whether the writes are in a file, so a disk whose save
     the flag skipped stays in its bay either way, and the update installer's
     save clears the flag itself when it saves behind live (FR-019).
  2. Defect b, fixed by 035 in `3698a1a0a`, `f793616db` and `e8290e93f`.
     `TogglePaused` and `SetPaused` had no acknowledgement, so a paused
     emulation thread could run the rest of its frame after the window moved
     on (`Shell/CpuManager.cpp:215-256`; threading-035.md §1). 035 now stores
     and wakes under the pause mutex, stops the machine on the next
     instruction boundary, and parks the thread, with `IsParked` and
     `TryWaitUntilParked` as the acknowledgement. 041 builds on that code
     after T008: the update installer's deploy hold (`HoldForDeploy`,
     `IsHeldForDeploy`) is a mechanism of its own, with its own names and
     acknowledgement, because 035's parked machine still runs posted commands
     and the deploy has to hold those too; and the drive status is published
     on every service pass whether or not the thread has parked.
  3. Defect e, fixed by 035 in `7e6c7bf8d` and `446df598c`.
     `MachineStateFile::Apply` replaced the disks in the bays before
     `LoadStateOverMountedMedia` (`Shell/MachineStateFile.cpp:232-236`), so a
     state load that failed after that point left the bays changed
     (research.md R4 Risks). `Apply` now saves the machine and turns media
     retention on before it touches the bays, and a failed load rolls back
     and emits a bay change for each bay it put back, through two new public
     store members, `IsRetainingMedia` and `ReportSeatedMedia`, which 041's
     ownership check covers (tasks.md T013). 041's own save check comes ahead
     of all of it: a state load saves every disk before `Apply` touches a bay
     (tasks.md T101), and R4's `LoadState` compatibility rule keeps the
     reserved WOZ slots from failing such a load.
  4. Defect f, fixed by 035 in `a7f8e937a`, `0528cba72` and `7de76eeba`.
     `MountExternallyModifiedDisk`'s move-assign
     (`Devices/Disk/DiskImageStore.cpp:3525`) kept no retained medium, so
     history could not step back across an external reload (research.md R2,
     Risk 7). An external reload now keeps the outgoing disk for history. 041
     edits the same function (tasks.md T026, T065, T084) and re-applies those
     edits to 035's version at the merge.
  5. Defect c, fixed on this branch. A replayed reset wrote the user's file:
     `Replayer::ApplyInput` calls `MachineHost::SoftReset`, which reached
     `Disk2Controller::SoftReset` and `DiskImage::Flush`, and neither checked
     the replay flag or the hold (`Debugger/Reverse/Replayer.cpp:493-496`,
     `Machines/Apple2/Common/Disk2Controller.cpp:931-948` before the fix,
     `Devices/Disk/DiskImage.cpp:981-1009`). On a heat-rebuild scratch machine
     the same call wrote the relative path `heat-rebuild-s6d1` from a pool
     thread (`Shell/ScratchHeatReplayer.cpp:563`, `DiskImage.cpp:803`). 041
     fixed it in `e32b2b68c` (FR-017; tasks.md T066, T067; audit.md
     softreset-second-write-path). On 2026-10-08 the 035 session said it
     would cherry-pick that commit, but its tip `d3c15b55c` holds no
     cherry-pick: `710531848` leaves the reset flush to 041, and `fe0427676`
     rewrites the `ReverseController.h` class comment to give the reset as
     the one exception to the flushes held behind live until 041's fix
     merges. So 035 reaches `master` with the controller's write, unless it
     takes `e32b2b68c` later, and 041's merge removes it; T008 corrects
     `fe0427676`'s paragraph once the two are merged here, and T130 corrects
     the `DiskImageStore::SoftReset` banner and `DiskImageStore.h:34` unless
     the 035 commit for those comments arrives first.
  6. Defect d, fixed by 035 in `5ee85434f`. `IDM_MACHINE_RESET` journals
     `Reset`, then remounts the disks, and each remount's
     `NotifyMediaChanged` captured a boundary keyframe before `SoftReset`
     ran, at a journal index past the `Reset` record
     (`Shell/CpuCommandDispatcher.cpp:62-71`,
     `Debugger/Reverse/ReverseController.cpp:283-296`,
     `Debugger/Reverse/Replayer.cpp:175-179`), so a replay across a reset with
     a disk mounted started from a machine state that never existed. A disk
     change while live now becomes a boundary keyframe before the next
     instruction or reverse command, as `OnMachineEdited` defers through
     `m_isEditPending` (`ReverseController.cpp:313-333`). 041's FR-009 also
     keeps the mounted disk when its file is unchanged, so an ordinary reset
     changes no medium at all.
  7. The divergence gate's check runs on the posting thread, from
     `HostInputGate::IsHeld` (`Shell/EmulatorShellReverse.cpp:303-333`,
     `Shell/EmulatorShell.h:521`), which is set only when a reverse command runs
     on the emulation thread. An insert, eject or write-protect change posted
     while a step back is still queued passes the gate and runs behind live.
     041's command handlers change nothing behind live; the gate is 035's. Not
     among the six defects reported on 2026-10-08.
  8. A `DriveWriteProtect` record is journaled before dispatch with the
     requested value (`Shell/EmulatorShellCpuThread.cpp:350-355`), so a
     protection that FR-015 declines would replay as applied. 041 records the
     real outcome through a boundary keyframe. Whether such records should be
     journaled after dispatch instead is a question for the 035 session; it
     was not part of the 2026-10-08 report.
  9. Behind live, every Settings apply, even a color change, raises the
     divergence question, because `IDM_DISK_WRITEPROTECT1/2` is state-changing
     and is sent for both drives on every apply
     (`Ui/Settings/SettingsPanelState.cpp:1054-1057`; audit.md
     settings-wp-drops-dirty, 035 impact 7). Not among the six defects
     reported on 2026-10-08.
  10. The comment at `Debugger/Reverse/ReverseController.h:88-90` described
      flushes as held "while recording"; the code holds them only while the
      machine is behind live (threading-035.md §3). Not among the six defects
      reported on 2026-10-08; 035 fixed it in `c414dd193`, merged at
      `710531848`.
