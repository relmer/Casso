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
path, including failures, and assert that the drive and the store agree
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
   program rewrites that file, **Then** Casso notices, exactly as for an
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
   **Then** it reads track 39's data, and the end-stop sound plays at track 39.
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
- A save fails, then the user quits: the existing rescue question still runs.
- The image watcher reports a change for a path while that path's bay is being
  remounted.
- A drive is ejected while its debug panel is open.
- A recovery copy itself cannot be written (no room): the user is told once,
  and the disk keeps its unsaved writes.
- The head is stepped past track 39: it stops there.

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
  touching the old one; if the new image does not load, the old disk MUST stay
  mounted, attached to the drive, and unretired.
- **FR-010**: A mount, re-insert, eject, reset, power cycle or machine switch MUST
  NOT discard unsaved writes when the save before it fails; the disk and its
  writes MUST be kept and the user told truthfully.
- **FR-011**: Replacing a drive's disk MUST stop watching the old disk's folder
  when no other mounted disk uses it.
- **FR-012**: A reload of an externally changed file MUST commit or discard any
  open flux write against the image it was made on, never the reloaded one.
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
  and MUST keep the replaced file's attributes and permissions.
- **FR-019**: After the `master` merge, the update installer's final save MUST
  run with the emulation thread stopped, never concurrently with it.

**Drive emulation**

- **FR-020**: The Disk II head MUST travel to quarter track 159 (track 39), and
  the end-stop sound MUST follow the new limit.
- **FR-021**: A guest write over a quarter track with no stored data MUST create
  a full-capacity track there, map it, and save it into the image.

**Documentation**

- **FR-022**: Comments and documents that state ownership, atomicity, flush
  triggers or write paths the code does not have MUST be corrected, including
  `DiskImageStore.h`, `Disk2Controller.h`, `docs/disk-write-integrity.md` and
  `ARCHITECTURE.md` §2, §5 and §8.

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
- **SC-002**: A stress run of 10,000 mount, eject, swap and reload operations
  against a concurrent status reader completes with no sanitizer report.
- **SC-003**: No code outside the emulation thread touches a disk image, checked
  by an ownership assertion on every disk store entry point in Debug builds,
  with the full suite passing.
- **SC-004**: The full unit and scenario suites pass in Debug and Release, with
  the test count equal to the 035 baseline plus the tests this spec adds
  (baseline 9,525 Debug, 9,519 Release).
- **SC-005**: Idle CPU cost of the drive status publish is under 1% of a frame.

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
- Defects found in 035's own new code during this work (reverse execution's
  replay flag surviving `Stop`, the pause flag with no acknowledgement) are
  reported to the 035 session, not fixed here.
