# Feature Specification: Disk Manager and Disk Inspection

**Feature Branch**: `021-disk-manager`

**Created**: 2026-08-15

**Status**: Draft

**Input**: User description: "A graphical disk manager built into Casso covering the rest of AppleCommander's functionality, with matching command-line access for everything — including operating on the disk that is currently in the drive while the machine is running."

## Overview

With disk file access in place, Casso can put files on disks and take them off
from the command line. This feature gives that capability a face, and completes
the verb set.

The point is not merely a nicer-looking AppleCommander. Every existing Apple II
disk tool is a separate program pointed at a file: it opens an image, edits it,
and closes it, and the emulator knows nothing about any of it. Casso *is* the
emulator. Its disk manager can operate on the disk that is in the drive right now,
while the machine runs — drop a file in, then type `CATALOG` on the guest and
watch it appear. No other tool in the ecosystem can do that, because no other tool
has a running Apple II behind it.

That capability comes with real constraints, which this specification treats as
first-class requirements rather than caveats: a Disk II has no disk-change line,
so the guest operating system cannot know the media changed, and its cached
filesystem state can write back over host-side edits.

Everything the graphical manager can do MUST also be reachable from the command
line, so the feature serves interactive users and build scripts equally.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Browse and manage a disk image in the UI (Priority: P1)

A user opens a disk image inside Casso and sees its contents in a sortable list —
name, type, size, load address, lock state — then adds files from the host,
extracts files to the host, and deletes files, without leaving the emulator.

**Why this priority**: This is the feature's visible surface and the reason to
build it. It makes disk contents legible to users who will never type a command,
and it is independently valuable against image files alone, before any interaction
with a running machine.

**Independent Test**: Open a disk image with known contents in the manager, verify
every file is listed correctly, then add, extract, and delete files and confirm
each result against the image on disk.

**Acceptance Scenarios**:

1. **Given** a disk image with several files, **When** the user opens it in the
   manager, **Then** every file is listed with its name, type, size, and lock
   state, along with the volume name and free space.
2. **Given** an open disk image, **When** the user sorts by any column, **Then** the
   list reorders and the choice persists while the image stays open.
3. **Given** an open disk image, **When** the user drags a file from the host onto
   the list, **Then** the file is placed on the disk and appears in the listing.
4. **Given** a selected file, **When** the user extracts it, **Then** the host file
   produced matches the bytes on the disk.
5. **Given** a selected file, **When** the user deletes it, **Then** it disappears
   from the listing and its space is returned to the volume's free count.
6. **Given** a write-protected image, **When** the user attempts any modification,
   **Then** the action is unavailable and the reason is shown.
7. **Given** a ProDOS image with subdirectories, **When** the user opens it, **Then**
   the directory structure is navigable.

---

### User Story 2 - Edit the disk that is in the drive, live (Priority: P2)

A user modifies the disk currently mounted in a Casso drive while the machine is
running, then uses it from the guest immediately — no eject, no reboot, no
re-mount.

**Why this priority**: This is the differentiator, and the reason the manager
belongs inside Casso rather than beside it. It is P2 rather than P1 because it
depends on Story 1 existing first, and because it carries safety requirements that
must be got right rather than got quickly.

**Independent Test**: With a machine booted and idle, add a file to the mounted
disk through the manager, then run a catalog command on the guest and confirm the
file is listed and loadable.

**Acceptance Scenarios**:

1. **Given** a booted machine sitting idle at a prompt with a disk mounted, **When**
   the user adds a file through the manager and then catalogs the disk from the
   guest, **Then** the new file is listed and loads correctly.
2. **Given** a mounted disk being modified through the manager, **When** the drive
   is actively running, **Then** the modification is deferred or refused rather
   than applied underneath the running drive.
3. **Given** a mounted disk whose guest operating system may hold cached filesystem
   state, **When** the user makes a change that could conflict with that cache,
   **Then** the user is warned and offered a way to force the guest to re-read the
   medium.
4. **Given** a disk mounted in a drive, **When** the user modifies it through the
   manager, **Then** the change is applied to the live medium and survives the
   drive's next flush rather than being overwritten by it.
5. **Given** a modification applied to a mounted disk, **When** the emulator later
   writes the image back to its file, **Then** the file contains both the guest's
   writes and the manager's changes.

---

### User Story 3 - Complete the file-management verb set (Priority: P3)

A user renames files, locks and unlocks them, creates and removes directories on
ProDOS volumes, renames the volume itself, and extracts every file at once — from
the UI or the command line.

**Why this priority**: These are the operations that turn a viewer into a manager.
Each is small on its own, and none blocks the build loop, so they follow the core
surface rather than gating it.

**Independent Test**: Perform each operation on a test image and verify the result
by reading the image back and by booting it and inspecting from the guest.

**Acceptance Scenarios**:

1. **Given** a file on a disk, **When** the user renames it, **Then** the new name
   appears in the guest's catalog and the contents are unchanged.
2. **Given** an unlocked file, **When** the user locks it, **Then** the guest shows
   it as locked and refuses to overwrite or delete it.
3. **Given** a locked file, **When** the user attempts to delete it, **Then** the
   action is refused until it is unlocked.
4. **Given** a ProDOS volume, **When** the user creates a subdirectory, **Then** the
   guest can change into it and store files there.
5. **Given** a disk image, **When** the user extracts all files to a host folder,
   **Then** every file is written with its type and load address preserved in a
   recoverable form.
6. **Given** a volume, **When** the user renames it, **Then** the guest reports the
   new volume name.

---

### User Story 4 - Inspect a disk below the file level (Priority: P4)

A user examines raw sectors and blocks as hex, sees which areas of the disk are
allocated, disassembles a boot sector or a binary file as 6502 code, and compares
two images to find what differs.

**Why this priority**: This is where Casso has an advantage nothing else in the
category can match — it already contains a validated 6502 instruction set, so
disassembly is a small addition rather than a new subsystem. It is P4 because it
serves diagnosis and curiosity rather than the primary workflow.

**Independent Test**: Dump a known sector and compare against its expected bytes;
disassemble a known boot sector and compare against a reference listing; compare
two images differing in one sector and confirm only that sector is reported.

**Acceptance Scenarios**:

1. **Given** any mountable image, **When** the user views a track and sector,
   **Then** its bytes are shown as hex with printable characters alongside.
2. **Given** any volume, **When** the user views the allocation map, **Then**
   allocated and free areas are distinguishable, and selecting a file highlights
   the areas it occupies.
3. **Given** a bootable disk, **When** the user disassembles its boot sector,
   **Then** the output is valid 6502 assembly with addresses and operands resolved.
4. **Given** two disk images, **When** the user compares them, **Then** the
   differing areas are reported at file level where both are readable volumes and
   at sector level otherwise.
5. **Given** a bit-stream image, **When** the user inspects a track, **Then** the
   track's encoding is reported, including whether it holds standard sectors.

---

### User Story 5 - Drive the same operations from scripts (Priority: P4)

A build script or a shell user performs any of the above from the command line,
and can consume listings as structured data rather than parsing formatted text.

**Why this priority**: Parity between the interface and the command line is a
project principle, not an optional extra. Structured output is what makes disk
contents usable in a pipeline. It ranks with inspection because the essential
scripting verbs already arrived with the build-loop feature.

**Independent Test**: Perform each operation from the command line against a test
image and confirm the result matches what the graphical manager produces; parse a
structured listing with a standard tool and confirm every field is present.

**Acceptance Scenarios**:

1. **Given** any operation available in the manager, **When** the user invokes its
   command-line form, **Then** the effect on the image is identical.
2. **Given** a disk image, **When** the user requests a structured listing, **Then**
   the output parses cleanly with standard tooling and carries every field the
   graphical listing shows.
3. **Given** a disk image, **When** the user lists it with a name pattern, **Then**
   only matching files are reported.
4. **Given** several disk images, **When** the user searches them for a file by
   name, **Then** every image containing a match is reported.

---

### Edge Cases

- What happens when the manager modifies a disk while the emulated drive motor is
  running? The medium is being read bit by bit; a modification underneath it can
  produce a torn read. Modification MUST be deferred until the drive is idle or
  refused with the reason.
- What happens when the guest operating system has cached filesystem state — a
  volume table of contents or a volume bitmap — and then writes it back after the
  manager changed the medium? The guest's stale map can allocate over new files.
  The system MUST warn where this is possible and MUST offer a way to force the
  guest to re-read.
- What happens when the manager edits an image file that is simultaneously mounted
  in a drive? The emulator's in-memory copy is authoritative and will be written
  back over the file. The manager MUST route edits to the mounted medium rather
  than to the backing file whenever the image is mounted.
- What happens when a modification is requested from the interface while the
  machine is running? Emulation state is owned by the machine's own execution
  context; modifications MUST be handed to it rather than applied directly, and the
  interface MUST reflect the result only after it has been applied.
- What happens when the user deletes a file that the guest currently has open? The
  guest cannot be told. The system MUST warn, and MUST NOT pretend the operation is
  safe.
- What happens when two images being compared use different formats or different
  filesystems? The comparison MUST report what could and could not be compared
  rather than failing.
- What happens when a disassembly encounters bytes that are not code? The output
  MUST remain well-formed and MUST mark undecodable bytes as data rather than
  inventing instructions.
- What happens when a disk image is very large or a listing very long? The
  interface MUST remain responsive and MUST NOT stall the emulated machine.

## Requirements *(mandatory)*

### Functional Requirements

#### Disk manager interface

- **FR-001**: The system MUST provide an in-application disk manager that lists a
  volume's files with name, type, size, load address where applicable, and lock
  state, plus the volume name and free space.
- **FR-002**: The listing MUST be sortable by any displayed column and MUST support
  filtering by name.
- **FR-003**: The manager MUST navigate ProDOS directory hierarchies.
- **FR-004**: The manager MUST add files from the host, including by drag-and-drop.
- **FR-005**: The manager MUST extract selected files to the host.
- **FR-006**: The manager MUST delete selected files.
- **FR-007**: The manager MUST present the reason any operation is unavailable
  rather than silently disabling it.
- **FR-008**: The manager MUST follow the application's active theme and honor the
  same keyboard navigation conventions as the rest of the interface.

#### Live operation against a mounted disk

- **FR-009**: The manager MUST operate on the disk currently mounted in a drive,
  applying changes to the live medium rather than to the backing file.
- **FR-010**: Changes applied to a mounted medium MUST persist through the
  emulator's normal write-back, alongside any writes the guest made.
- **FR-011**: Modifications to a mounted medium MUST be applied through the
  machine's own execution context, never concurrently with it.
- **FR-012**: The system MUST NOT modify a mounted medium while its drive is
  active; it MUST defer or refuse, and MUST say which.
- **FR-013**: The system MUST warn when a modification may conflict with
  filesystem state the guest has cached, and the warning MUST reflect the actual
  risk for the volume's filesystem.
- **FR-014**: The system MUST offer an action that forces the guest to re-read the
  medium after a modification.
- **FR-015**: The interface MUST reflect the state of the medium after a
  modification has actually been applied, never before.

#### File management

- **FR-016**: The system MUST rename a file.
- **FR-017**: The system MUST lock and unlock a file, and MUST honor the lock when
  deleting or overwriting.
- **FR-018**: The system MUST create and remove ProDOS subdirectories.
- **FR-019**: The system MUST rename a volume where the filesystem supports it.
- **FR-020**: The system MUST extract all of a volume's files in one operation,
  preserving each file's type and load address in a recoverable form.

#### Inspection

- **FR-021**: The system MUST display any track and sector, or any block, as
  hexadecimal with a printable-character rendering alongside.
- **FR-022**: The system MUST display a volume's allocation map, distinguishing
  allocated from free areas.
- **FR-023**: Selecting a file MUST identify the areas of the medium it occupies.
- **FR-024**: The system MUST disassemble a selected region — a boot sector, a
  block, or a binary file — as 6502 or 65C02 code with addresses and operands
  resolved, marking undecodable bytes as data.
- **FR-025**: The system MUST compare two images and report their differences at
  file level where both are readable volumes, and at sector level otherwise.
- **FR-026**: The system MUST report a track's encoding for bit-stream images,
  including whether it contains standard sectors.

#### Command-line parity and scripting

- **FR-027**: Every operation available in the manager MUST have a command-line
  equivalent with identical effect.
- **FR-028**: The system MUST emit volume listings in at least one structured,
  machine-parseable format in addition to human-readable text.
- **FR-029**: The system MUST support name-pattern matching when listing or
  selecting files.
- **FR-030**: The system MUST search a set of images for files matching a name and
  report which images contain matches.
- **FR-031**: Every command-line operation MUST report success or failure through a
  distinct exit status, with diagnostics on the error stream.

### Key Entities

- **Volume View**: What the manager presents — a volume's identity, capacity, free
  space, and the file entries within the current directory.
- **File Selection**: One or more chosen entries and the operations legal for them,
  given the volume's write state and each entry's lock state.
- **Mount Binding**: The association between an open volume view and a drive, which
  determines whether edits target a live medium or a file and which safety rules
  apply.
- **Medium Region**: An addressable area for inspection — a track and sector, a
  block, or a span of a file — and its rendering as hex, as allocation state, or as
  disassembled code.
- **Comparison Result**: What differs between two images, expressed at file level
  or at sector level, plus what could not be compared.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can add a file to a disk, see it in the guest's catalog, and
  load it, without ejecting the disk or restarting the machine.
- **SC-002**: A user unfamiliar with Apple II disk tools can add a file to a disk
  image on their first attempt using only the interface, with no documentation.
- **SC-003**: Every operation available in the manager is reachable from the
  command line, verified by an inventory check rather than by sampling.
- **SC-004**: No sequence of manager operations against a mounted disk can leave
  the volume in a state the guest cannot read, verified across the documented
  hazard scenarios.
- **SC-005**: Modifying a mounted disk never disturbs the running machine's timing
  perceptibly — no audible break in emulated audio and no dropped frames.
- **SC-006**: Listing a full volume, and rendering any inspection view, completes
  fast enough to feel immediate.
- **SC-007**: Disassembly of a known boot sector matches a reference listing
  instruction for instruction.
- **SC-008**: Structured listing output parses with standard tooling and carries
  every field shown in the interface.

## Assumptions

- File-level read and write for DOS 3.3 and ProDOS already exists from the
  build-loop feature; this feature consumes it rather than reimplementing it.
- The interface is built from the application's existing widget set and theming;
  no new user-interface technology is introduced.
- The disassembler is new but small, because a validated instruction table already
  exists. It is expected to be reused by the interactive monitor (GitHub issue #51)
  and the debugger panel (GitHub issue #59), and should be designed for that reuse.
- Media larger than 140 KB and filesystems beyond DOS 3.3 and ProDOS are out of
  scope; they arrive with the disk-format feature.
- Live editing is supported only when the drive is idle. This mirrors physical
  reality — a Disk II has no disk-change line — and is a documented behavior rather
  than a limitation to engineer around.
- The guest operating system's caching behavior differs by filesystem, and warnings
  are expected to differ accordingly rather than being uniform.
- Comparison and search operate on images the system can already mount; adding
  formats is not part of this feature.
