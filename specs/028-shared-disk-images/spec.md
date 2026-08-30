# Feature Specification: Disk Images Shared with a Running Emulator

**Feature Branch**: `028-shared-disk-images`

**Created**: 2026-08-30

**Status**: Draft

**Input**: User description: "Allow CassoCli to modify disks mounted by Casso, and have Casso deal properly with that when it happens."

## Overview

`CassoCli` writes into disk images. `Casso` mounts them. Today the two do not
know about each other, and the result is a build loop that silently does the
wrong thing.

**Measured, not supposed.** With `Casso.exe --disk1 boot.dsk` running,
`CassoCli as65 boot.a65 --disk boot.dsk --as OTHER.SYSTEM` returns 0 and changes
27 bytes of `boot.dsk`. An exclusive open of that same file from a third process
also succeeds. The emulator holds no operating-system handle on a mounted image:
it reads the bytes in at mount and closes the file.

Two things follow, and only the second loses work:

1. **The running guest does not see the change.** It is holding its own copy, so
   an edit, assemble, run loop appears to do nothing at all until the disk is
   ejected and re-inserted. Nothing is lost; the user is simply told nothing and
   concludes the tool is broken.
2. **A guest write can destroy the change.** When the guest has written to the
   disk, the emulator later serializes its own copy over the file, and whatever
   was assembled in the meantime is gone.

The second needs BOTH a guest write and an external change: the emulator writes
back only when its image is dirty, so a session that only reads from a disk
cannot clobber it. That narrows the loss case considerably and it does not make
it acceptable — a build loop that assembles onto a disk the guest is writing to
is a normal thing to do.

**This is coordination, not an assembler feature.** It sits between the two
programs and belongs to neither. The disk manager (spec 021) will need the same
guarantees the moment it edits a mounted disk live, and should build on what
this delivers rather than inventing a second answer.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The build loop shows what was just built (Priority: P1)

A developer has a disk mounted in the emulator and assembles onto it from the
command line. The emulator picks the change up and the guest can run the program
that was just built, without the developer ejecting and re-inserting the disk by
hand.

**Why this priority**: This is the loop the whole disk-writing capability exists
to serve, and today it appears not to work at all. It is also the case that
loses nothing, so it can ship on its own and be worth having.

**Independent test**: Mount a disk, assemble a changed program onto it from a
second process, and confirm the guest can load the new version without any
manual eject.

**Acceptance Scenarios**:

1. **Given** a developer who has declared that changes should be taken up in
   place, **When** another process writes the image, **Then** the guest sees the
   new contents without being asked anything, and the developer runs the program
   again.
2. **Given** a developer who has declared that changes should restart the
   machine, **When** another process writes the image, **Then** the machine
   restarts and boots the new contents.
3. **Given** a developer who has declared nothing, **When** another process
   writes the image, **Then** they are asked which of the two should happen, and
   their answer is remembered.
4. **Given** any of the above, **When** the pick-up happens, **Then** it is
   reported, and it happens at a moment with no disk operation in flight.
5. **Given** a disk mounted and never externally touched, **When** the guest
   runs normally, **Then** nothing about its behavior changes.

### User Story 2 - A guest's work is never silently overwritten (Priority: P1)

A developer has a disk mounted, the guest has written to it, and something
outside changes the same image. Neither side's work is thrown away without the
developer choosing it.

**Why this priority**: This is the case that destroys work, and it destroys it
in both directions — the guest's writes over the external change today, and the
external change over the guest's writes under a naive fix. It is P1 alongside
Story 1 because shipping the reload without this would make the loss MORE likely,
not less.

**Independent test**: Mount a disk, have the guest write to it, change the image
externally, and confirm both versions survive somewhere and the user is asked
what to do.

**Acceptance Scenarios**:

1. **Given** a mounted disk the guest has written to, **When** the image changes
   externally, **Then** the user is asked what to do, and nothing is written
   until they answer.
2. **Given** that question, **When** the user takes the external version,
   **Then** the guest's unsaved writes are preserved in a separate backup image
   rather than discarded, and the user is told where it is.
3. **Given** that question, **When** the user keeps the guest's version,
   **Then** the external change is preserved in a separate backup image rather
   than overwritten.
4. **Given** a mounted disk the guest has written to, **When** the emulator
   writes back and the image has NOT changed externally, **Then** it writes
   directly as it does today, with no backup and no prompt.

### User Story 3 - Two writers cannot interleave (Priority: P2)

Two programs writing the same image at the same time cannot produce a file that
is half one and half the other.

**Why this priority**: The window is small and the consequence is a corrupt
image rather than a lost file, so it ranks below the two above. It is worth
closing because a corrupt image is much harder to diagnose than a missing file.

**Independent test**: Have one writer hold the image and confirm the other
refuses, waits, or otherwise does not write into the same bytes.

**Acceptance Scenarios**:

1. **Given** a process writing an image, **When** another tries to write the
   same image, **Then** the second does not interleave with the first.
2. **Given** a process that fails or is killed mid-write, **When** anything
   later opens that image, **Then** it is not left permanently unwritable.

### Edge Cases

- The image is deleted or renamed while mounted.
- The image is replaced with a file of a different format, or with something
  that is not a disk image at all.
- The image is replaced with a valid image of a DIFFERENT size or geometry.
- The external change arrives while the guest is mid-write to the same disk.
- Several changes arrive in quick succession, as a build script writing twice
  in a second would produce.
- The image sits on a network share or a synchronizing folder, where change
  notification and timestamps are less trustworthy.
- Two emulator instances mount the same image.
- The user answers the conflict question by ejecting the disk instead.
- The guest has a file OPEN on the disk when the contents are replaced, so its
  cached structure describes bytes that are no longer there.
- The guest writes to the disk shortly after a pick-up, allocating against the
  structure it cached from the previous contents.
- A backup would overwrite a backup from an earlier conflict in the same session.
- The image is write-protected, or the directory holding it is not writable, so
  no backup can be placed beside it.

## Requirements *(mandatory)*

### Functional Requirements

#### Noticing a change

- **FR-001**: The emulator MUST record, at mount, enough about the image file to
  decide later whether it has changed underneath — at minimum its
  last-modified time and its size.
- **FR-002**: The emulator MUST detect an external change to a mounted image
  without the user asking it to, rather than only at the moment it happens to
  write.
- **FR-003**: The emulator MUST re-check immediately before writing an image
  back, whatever it has or has not been told in the meantime. Notification is an
  optimization for promptness; the check before writing is what makes the
  guarantee hold.
- **FR-004**: A change the emulator itself made MUST NOT be reported as an
  external change.

#### Acting on a change

**REPLACING A MOUNTED DISK'S CONTENTS IS A DISK SWAP, and it inherits every
hazard of a real one.** Users swapped floppies constantly and the machines
expect it, but they did it at a prompt with no files open, and that is exactly
the state the emulator cannot verify. The guest caches disk structure in its own
RAM: DOS 3.3 holds the VTOC, ProDOS holds a volume control block and an open
file's index blocks. Swap underneath that and the guest's next WRITE allocates
against a map belonging to the disk that is no longer there. ProDOS defends
itself somewhat by comparing the volume name when it reaches the directory;
DOS 3.3 does not.

**TWO DIFFERENT QUESTIONS ARE TANGLED HERE and the spec keeps them apart.**

1. *What happens to the two versions*, when the guest has written and something
   else has changed the file. This is a data-loss question, it arises only when
   the image is dirty, and its answer is the conflict resolution below.
2. *How the guest should take up the new contents* — carry on against the new
   bits, or restart the machine. This is a correctness-of-guest-state question,
   it arises on EVERY pick-up including the clean ones, and its answer cannot be
   computed by the emulator at all.

The second is the one the user is equipped to answer and the tool is not. A
developer iterating on a binary they will `BRUN` again wants the contents
swapped and nothing else; a developer working on a bootable program, or one
whose guest holds state that a changed disk invalidates, wants the machine
restarted. Nothing in the image or the emulator distinguishes those; only the
person driving does. So the tool asks once, remembers the answer, and gets out
of the way.

##### How the guest takes up the new contents

- **FR-005**: The user MUST be able to declare, in advance, what an external
  change should do to a running machine: take up the new contents in place,
  restart the machine, or ask each time.
- **FR-006**: The default MUST be to ask. A tool that silently restarts a
  machine, or silently swaps a disk under a program that cannot survive it, has
  chosen for the user on the question the user is uniquely placed to answer.
- **FR-007**: The declared answer MUST persist across sessions, so an
  established build loop is configured once rather than confirmed hourly. It is
  the repetition that makes a prompt unbearable, not the prompt.
- **FR-008**: The user MUST be able to change the answer without restarting the
  emulator or re-mounting the disk, since which loop they are in changes during
  a session.
- **FR-009**: Where the answer is to take up the new contents in place, a
  restart MUST remain available afterward without the user hunting for it: the
  swap may turn out to have been the wrong call, and the recovery is the same
  action they declined.
- **FR-010**: Every pick-up MUST be reported, whichever answer governs it. A
  disk whose contents change under a running program is not something to do
  silently even when the user asked for it.
- **FR-011**: A pick-up MUST happen at a point where no disk operation is in
  flight, so it cannot land in the middle of a read or a write. The controller
  already reports motor spindown for this purpose. This bounds the damage to
  stale cached structure; it does not eliminate it, and must not be described as
  though it does.

##### What happens to the two versions

- **FR-012**: Where the emulator HAS unsaved guest writes, an external change
  MUST NOT be resolved without asking the user, whatever the pick-up answer says.
  The pick-up answer governs how the guest continues, not whether work may be
  discarded, and no configuration may turn the data-loss question off.
- **FR-013**: Whichever version the user does not keep MUST be preserved in a
  separate image file rather than discarded, and the user MUST be told where it
  is.
- **FR-014**: The emulator MUST NOT write an image back over an external change
  it has not resolved.

#### Writing safely

- **FR-015**: A program writing a disk image MUST hold it for the duration of
  the write, so that a second writer cannot interleave with it.
- **FR-016**: A writer that cannot obtain the image MUST report that plainly,
  naming the image, rather than failing obscurely or waiting forever.
- **FR-017**: A writer that fails or is killed mid-write MUST NOT leave the
  image permanently unwritable by anything else.
- **FR-018**: The existing guarantee that a failed write leaves an image
  byte-for-byte unchanged MUST continue to hold.

#### Not making things worse

- **FR-019**: A session in which no external change occurs MUST behave exactly
  as it does today, in what it writes and when.
- **FR-020**: Detection MUST NOT impose a cost the user can feel while the
  emulator is running.
- **FR-021**: Where change detection cannot be trusted — a network share, a
  synchronizing folder — the feature MUST degrade to the check before writing
  rather than to silence.

#### Saying so

- **FR-022**: A refusal or a conflict MUST name the image it is about. A user
  with several disks mounted cannot act on a message that does not say which.
- **FR-023**: The conflict question MUST state what is at stake on both sides —
  that the guest has written, and that something else has changed the file —
  rather than asking the user to choose between two unlabeled options.

### Key Entities

- **Mounted image record**: what the emulator holds for a mounted disk. Gains
  the identity it recorded at mount, and the state of any unresolved external
  change.
- **Image identity**: what is compared to answer "has this file changed since I
  read it". The command-line side already records one at read time and
  re-verifies it at commit; the emulator records none.
- **Conflict**: an external change to an image the guest has also written to.
  Holds both versions until the user resolves it, and resolves to exactly one
  surviving image plus one backup.
- **Backup image**: the version the user did not keep, written beside the
  original under a name that says what it is.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can assemble onto a mounted disk and have the guest
  run the new program without ejecting and re-inserting it by hand.
- **SC-002**: No sequence of guest writes and external changes results in work
  being lost without the user having chosen to lose it.
- **SC-003**: A session with no external change produces byte-for-byte the same
  image file it produces today.
- **SC-004**: Concurrent writes never produce an image that is part one writer's
  and part another's.
- **SC-005**: Every refusal and every conflict names the image it concerns.
- **SC-006**: The emulator's frame rate and audio are unaffected by the
  detection, measured against the same session with it disabled.

## Assumptions

- **The common case is one emulator and one command-line tool on one machine.**
  Two emulator instances sharing an image is an edge case to be handled sanely,
  not a workflow to be optimized.
- **The command-line side already refuses to clobber.** It records the file's
  identity when it reads and re-verifies before it commits, so the work here is
  mostly on the emulator side, which records nothing.
- **The emulator writes back only when its image is dirty.** A session that only
  reads cannot destroy an external change, which is why Story 1 can ship without
  Story 2 and still be safe.
- **A swap cannot be made unconditionally safe, so the choice belongs to the
  user.** Whether a pick-up is harmless depends on guest RAM the disk layer
  cannot see -- open files, a cached VTOC, a ProDOS volume control block. The
  emulator cannot compute the answer and must not pretend to; the developer
  knows whether they are iterating on a binary or on a bootable system.
- **The repetition is what makes a prompt unbearable, not the prompt.** Asking
  once and remembering is what keeps the build loop fast, so a remembered answer
  is the mechanism rather than a guessed default.
- **The data-loss question is never configured away.** A declared pick-up answer
  says how the guest continues; it does not grant permission to discard work.
  Those stay separate however the preference is set.
- **Backups are placed beside the image.** A user who has to hunt for the
  recovered version has not really been given it back.

## Dependencies

- Nothing outside the repository.
- Independent of spec 021 (disk manager) and spec 022 / 027 (image formats),
  though 021 will need these guarantees when it edits a mounted disk live and
  should build on them rather than growing its own.

## Out of Scope

- Letting the guest and an external tool write the same image *simultaneously*
  and merging the results. The two versions are reconciled by choosing one and
  keeping the other, not by merging.
- Live editing of a mounted disk from within Casso's own UI, which is spec 021.
- Sharing an image between two machines, or over a network filesystem, as a
  supported workflow. Such a location must degrade safely, not work well.
- Any change to what the assembler or the `disk` subcommand writes.
