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

## Clarifications

### Session 2026-08-30

- Q: How should the preserved (not-kept) version of a conflicted disk image be named and placed? -> A: Timestamped beside the original, e.g. PROG.20260830-014233.dsk
- Q: How should the emulator treat a burst of changes from a multi-command build? -> A: Coalesce on a quiet period after the last write, then act once
- Q: What should happen when the image is deleted or replaced with something unusable? -> A: Refuse the change and keep running, then offer to save the in-memory disk to a backup, since it may now be the only copy
- Q: How should a pick-up be reported? -> A: A non-modal banner carrying a Restart action, which stays until acted on, absorbs further changes while it stands, and acts on the MOST RECENT contents
- Q: What if the preserved copy cannot be written? -> A: Refuse the action that would discard a version, keep both live, and offer another location

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The build loop shows what was just built (Priority: P1)

A developer has a disk mounted in the emulator and assembles onto it from the
command line. The emulator picks the change up and the guest can run the program
that was just built, without the developer ejecting and re-inserting the disk by
hand.

**Why this priority**: This is the loop the whole disk-writing capability exists
to serve, and today it appears not to work at all. It is also the case that
discards no image FILE, so it can ship on its own — with the one caveat its
phase records: a guest write meeting an external change is held in memory rather
than preserved to disk until Story 2 lands.

**Independent test**: Mount a disk, assemble a changed program onto it from a
second process, and confirm the guest can load the new version without any
manual eject.

**Acceptance Scenarios**:

1. **Given** an assembly that states the change should be taken up in place,
   **When** it writes a mounted image, **Then** the guest sees the new contents
   without anybody being asked anything, and the developer runs the program
   again.
2. **Given** an assembly that states the change should restart the machine,
   **When** it writes a mounted image, **Then** the machine restarts and boots
   the new contents.
3. **Given** a change written by something that stated no intent, **When** it
   reaches a mounted image, **Then** the emulator falls back to the answer the
   user declared, asking if they have declared none.
4. **Given** any of the above, **When** the pick-up happens, **Then** it is
   reported, and it happens at a moment with no disk operation in flight.
5. **Given** a disk mounted and never externally touched, **When** the guest
   runs normally, **Then** nothing about its behavior changes.

### User Story 2 - No version is discarded unless the user chose it (Priority: P1)

A developer has a disk mounted, the guest has written to it, and something
outside changes the same image. Neither side's work is thrown away without the
developer choosing it.

**Why this priority**: This is the case that destroys work, and it destroys it
in both directions — the guest's writes over the external change today, and the
external change over the guest's writes under a naive fix. It is P1 alongside
Story 1 because shipping the reload without this would make the loss MORE likely,
not less. It also covers the case where the FILE goes away and the emulator is
left holding the only copy, which is a loss of the same kind from the other
direction.

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
5. **Given** a mounted disk, **When** its file is deleted or replaced by
   something that cannot be used as that disk, **Then** the change is refused,
   the machine carries on with the contents it holds, and the emulator offers to
   save them — because with the file gone they may be the only copy left.
6. **Given** that offer, **When** the user declines it, **Then** the machine
   keeps running with the disk still mounted, so they can save later.
7. **Given** a conflict the user has been asked about, **When** they eject the
   disk instead of answering, **Then** neither version is discarded.

### User Story 3 - Two writers cannot spoil each other's work (Priority: P2)

Two programs writing the same image cannot corrupt it, and neither can lose the
other's change without anybody noticing.

**Why this priority**: Most of this is already delivered, which is why it ranks
below the two above. Both sides already write to a temporary beside the target
and rename it over, so a reader sees the old file or the new one and never a
mixture. What is left is narrower than "add locking" and is written out below.

**THE CORRUPTION CASE IS ALREADY CLOSED, and the spec says so rather than
asking for it again.** Reading the code: the emulator commits through
`WriteFileAtomically` and the command line through `CommitPlan` and an atomic
replace. Neither can leave a half-written image. A lock adding to that would buy
nothing, and the plan should not add one on the strength of this story.

**Two real gaps remain.** The first is that two EMULATOR instances derive the
same temporary name from one image path -- a fixed suffix -- so they write into
each other's temporary and one commits the other's bytes as its own. The command
line already solved this for itself with a per-invocation tag, and its own
reasoning says why: "the loser's bytes then go into the winner's temporary and
are committed as though they were the winner's". The emulator never got that
treatment. The second is the lost update: whoever renames last wins, and the
loser's change disappears silently. The command line already detects that with
the identity it recorded at read time; the emulator records nothing.

**Independent test**: Have two writers commit the same image and confirm the
result is one whole version, neither mixed nor silently replaced.

**Acceptance Scenarios**:

1. **Given** two emulator instances holding the same image, **When** both flush,
   **Then** neither writes into the other's temporary and the image ends as one
   complete version.
2. **Given** a writer that renames its version over a change it never saw,
   **When** it commits, **Then** it is detected rather than silently winning.
3. **Given** a process that fails or is killed mid-write, **When** anything later
   opens that image, **Then** it is not left permanently unwritable, and any
   temporary left behind does not become somebody else's.

### Edge Cases

- The image is deleted or renamed while mounted, leaving the emulator holding
  the only copy of that disk.
- The image is replaced with a file of a different format, or with something
  that is not a disk image at all.
- The image is replaced with a valid image of a DIFFERENT size or geometry.
- The image becomes unusable and the user declines to save the in-memory copy,
  then the file reappears in a usable state.
- The external change arrives while the guest is mid-write to the same disk.
- A build script writing the image several times in a row, where the machine
  would otherwise restart on the first and be mid-boot when the last arrives.
- A change arriving after the quiet period elapsed but before the emulator has
  finished acting on the previous one.
- Several builds completing while the user is away from the emulator, so a
  report stands unacted-on across all of them.
- The image becoming unusable while a report from an earlier change still
  stands.
- A writer that holds the image far longer than the quiet period.
- The image sits on a network share or a synchronizing folder, where change
  notification and timestamps are less trustworthy.
- Two emulator instances mount the same image and both flush it.
- The user answers the conflict question by ejecting the disk instead.
- The guest has a file OPEN on the disk when the contents are replaced, so its
  cached structure describes bytes that are no longer there. **Accepted and
  unhandled**: invisible from the disk layer, mitigated only by the restart being
  always available. No task addresses it and none can.
- The guest writes to the disk shortly after a pick-up, allocating against the
  structure it cached from the previous contents. **Accepted and unhandled**, for
  the same reason.
- Two conflicts on the same image within the resolution of the backup timestamp.
- The image is write-protected, or the directory holding it is not writable, so
  no backup can be placed beside it.
- The location the user chooses instead is also unwritable.
- The user cancels rather than choosing a location, leaving the conflict
  unresolved and the machine still running.

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
  guarantee hold. This is the mounted-image case of the general writer rule
  stated under "Writing safely"; the two are one mechanism, and splitting them
  across sections is what let the work be scheduled after the phases that need
  it.
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

**THE INTENT TRAVELS WITH THE WRITE, NOT WITH THE DRIVE.** It is clearest at the
moment somebody writes the image, because that is when they know what they
changed: a binary to be run again, or the program the disk boots. So the writer
states it and the emulator carries it out, rather than the emulator holding a
standing guess about a disk it cannot inspect.

That also settles what the intent attaches to. It belongs to the IMAGE that
changed, not to the bay it happens to be mounted in, so a developer with a
bootable disk in one drive and a data disk in another needs no per-drive
configuration: each write carries its own answer.

- **FR-005**: A tool writing a disk image MUST be able to state, as part of the
  invocation that writes it, what the change should do to any emulator running
  that image: take the new contents up in place, or restart the machine.
- **FR-006**: The stated intent MUST be carried per image, so a change to one
  mounted disk does not govern what happens to another.
- **FR-007**: A change that arrives with NO stated intent MUST be handled too. A
  developer editing an image in another program cannot state one, and the
  emulator MUST fall back to an answer the user has declared, defaulting to
  asking.
- **FR-008**: The fallback answer MUST persist across sessions and MUST be
  changeable without restarting the emulator or re-mounting the disk.
- **FR-009**: Where the new contents are taken up in place, a restart MUST
  remain available afterward without the user hunting for it: the swap may turn
  out to have been the wrong call, and the recovery is the action they declined.
- **FR-010**: Every pick-up MUST be reported in a way that does not block the
  machine, and the report MUST carry the restart action. A disk whose contents
  change under a running program is not something to do silently, even when it
  was asked for -- and a report that clears itself takes the restart away with
  it, which is the action the user reaches for once the program misbehaves.
- **FR-011**: A standing report MUST absorb further changes rather than be
  replaced or duplicated by them. A developer may run three builds before
  turning back to the emulator, and three reports about one disk say nothing
  three times.
- **FR-012**: Acting on a standing report MUST use the MOST RECENT contents of
  the image, not those current when it first appeared. The version the developer
  means is the one they just built.
- **FR-013**: Changes arriving close together MUST be treated as one. A disk
  carrying more than one thing is built by more than one command -- assemble a
  loader, assemble a program, place a data file -- and the developer means one
  build. Acting on each in turn restarts the machine repeatedly and lands later
  writes while it is still booting from an earlier one.
- **FR-014**: A pick-up MUST happen at a point where no disk operation is in
  flight, so it cannot land in the middle of a read or a write. The controller
  already reports motor spindown for this purpose. This bounds the damage to
  stale cached structure; it does not eliminate it, and must not be described as
  though it does.
- **FR-015**: An intent stated for an image no emulator has mounted MUST NOT be
  an error. The writer cannot know whether anything is running, and a build
  script must behave the same either way.

##### When the new contents cannot be used

- **FR-016**: Where the image is deleted, or replaced by something that cannot
  be used as the mounted disk -- a different format, a different geometry,
  unreadable bytes -- the emulator MUST refuse the pick-up and carry on with the
  contents it already holds. The machine is running and what it holds is
  known-good; ejecting or halting acts on a guest that was working.
- **FR-017**: That refusal MUST offer to save the in-memory disk to a backup.
  The file that backed it is gone or unusable, so what the emulator holds may be
  the ONLY remaining copy of that disk, whether or not the guest has written to
  it. The offer MUST use the same timestamped naming as any other preserved
  version.
- **FR-018**: Declining that offer MUST leave the machine running and the disk
  mounted, so the user can carry on and save later.

##### What happens to the two versions

- **FR-019**: Where the emulator HAS unsaved guest writes, an external change
  MUST NOT be resolved without asking the user, whatever the pick-up answer says.
  The pick-up answer governs how the guest continues, not whether work may be
  discarded, and no configuration may turn the data-loss question off.
- **FR-020**: Whichever version the user does not keep MUST be preserved in a
  separate image file rather than discarded, and the user MUST be told where it
  is.
- **FR-021**: The preserved version MUST be written beside the original with a
  timestamp in its name, so that repeated conflicts in one session cannot
  overwrite each other and the order they happened in is readable from the
  directory. **Where a timestamp would collide with a backup already there, the
  name MUST be disambiguated rather than overwritten** -- the promise is that
  repeated conflicts accumulate, and a one-second resolution cannot keep it on
  its own.
- **FR-022**: The emulator MUST NOT write an image back over an external change
  it has not resolved.
- **FR-023**: Ejecting the disk with a conflict unresolved MUST NOT discard
  either version. The eject resolves it by keeping what is on disk, and the
  guest's copy is preserved exactly as FR-020 requires. An eject is a plausible
  answer to being asked a question, and it must not become the one path that
  loses work.
- **FR-024**: Where the preserved copy cannot be written -- a read-only
  directory, no space, the volume gone with the image -- the action that would
  discard a version MUST NOT proceed. The emulator MUST keep holding both and
  MUST offer another location. A backup that silently did not happen breaks the
  promise exactly where it matters most.

#### Writing safely

- **FR-025**: A partly written image MUST never be visible to a reader. This is
  already delivered on both sides by writing to a temporary and renaming it over
  the target; it is stated so that a change which abandons that stops being a
  refactor and starts being a regression.
- **FR-026**: Two writers MUST NOT derive the same temporary path from one image.
  The emulator currently derives a fixed name from the image path, so two
  instances write into each other and one commits the other bytes as its own.
- **FR-027**: A writer MUST detect that the image changed under it since it read,
  rather than renaming its version over a change it never saw. The command line
  already does this; the emulator does not.
- **FR-028**: A writer that fails or is killed mid-write MUST NOT leave the image
  permanently unwritable, and a temporary left behind MUST NOT be adopted by
  another writer as its own.
- **FR-029**: The existing guarantee that a failed write leaves an image
  byte-for-byte unchanged MUST continue to hold.

#### Not making things worse

- **FR-030**: A session in which no external change occurs MUST behave exactly
  as it does today, in what it writes and when.
- **FR-031**: Detection MUST NOT impose a cost the user can feel while the
  emulator is running.
- **FR-032**: Where change detection cannot be trusted — a network share, a
  synchronizing folder — the feature MUST degrade to the check before writing
  rather than to silence.

#### Saying so

- **FR-033**: A refusal or a conflict MUST name the image it is about. A user
  with several disks mounted cannot act on a message that does not say which.
- **FR-034**: The conflict question MUST state what is at stake on both sides —
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
  original under a timestamped name, so repeated conflicts accumulate rather
  than overwrite.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can assemble onto a mounted disk and have the guest
  run the new program without ejecting and re-inserting it by hand.
- **SC-002**: No version of an image FILE is discarded without the user having
  chosen to discard it. **Scoped to what the disk layer controls**: a guest that
  allocates against structure it cached from contents now gone can still corrupt
  its own disk, and the Edge Cases mark that accepted and unhandled. A criterion
  written as "no work is ever lost" would be one this feature cannot pass and
  does not claim.
- **SC-003**: A session with no external change produces byte-for-byte the same
  image file it produces today.
- **SC-004**: Concurrent writes never produce an image that is part one writer's
  and part another's, and never let one writer commit another's bytes as its own.
- **SC-005**: Every refusal and every conflict names the image it concerns.
- **SC-006**: The emulator's frame rate and audio are unaffected by the
  detection, measured IN ONE BUILD by comparing a session with the image
  watched against one with the same image mounted and watching disabled -- the
  state an unwatchable directory already produces. **Release**, three runs of five
  minutes: p99 frame time within 2%, and audio underruns per minute within one
  event of the not-watching arm. A zero-tolerance bar on a stochastic metric would
  fail on noise. **No off-switch is being added**, so
  a criterion written as "the same session with it disabled" would describe a
  measurement nobody can perform -- and a cross-build comparison is worse still
  on this hardware, where clock variation between runs swamps the signal.

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
  knows whether they are iterating on a binary or on a bootable system, and
  knows it most clearly at the moment they write the image.
- **A build script must not behave differently depending on whether an emulator
  happens to be running.** Stating an intent is therefore always allowed and
  never an error, and it simply has no effect when nothing has the image
  mounted.
- **The repetition is what makes a prompt unbearable, not the prompt.** Asking
  once and remembering is what keeps the build loop fast, so a remembered answer
  is the mechanism rather than a guessed default.
- **The data-loss question is never configured away.** A declared pick-up answer
  says how the guest continues; it does not grant permission to discard work.
  Those stay separate however the preference is set.
- **Backups are placed beside the image.** A user who has to hunt for the
  recovered version has not really been given it back.

## Dependencies

- **Spec 026 (assembler-to-disk) is a prerequisite for the assembler half, and
  is NOT on master.** Its flat image-target options — `--disk`, `--as`, `--type`,
  `--startup` — and its `ImageArtifactSink` are what an assembly writing into an
  image is made of, and none of them exist here: on this branch `imagePath`
  lives only inside the `disk` subcommand's own nested options, and
  `ImageArtifactSink` does not exist at all. **Stating an intent from an
  assembly therefore waits on 026 landing.**

  Nothing else here waits on it. `disk put` already writes a mounted image
  today, so the emulator-side work and the two writer defects stand alone, and
  the build loop can be demonstrated end to end through `disk put` before 026
  merges.
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
