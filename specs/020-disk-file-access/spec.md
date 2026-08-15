# Feature Specification: Disk File Access for the Build Loop

**Feature Branch**: `020-disk-file-access`

**Created**: 2026-08-15

**Status**: Draft

**Input**: User description: "Developer-focused features so a 6502 developer can edit source on Windows, assemble it, get the result onto an Apple II disk image, and boot it — without leaving Casso or hand-assembling on a 128K machine."

## Overview

Casso today assembles 6502 source and boots disk images, but nothing connects the
two. A developer who assembles a program has no way to put it on a disk, and no
way to read a file back off one. The gap is closed by every other Apple II
toolchain with a second and third tool (AppleCommander for file placement, c2d
for bootable images), each with its own conventions and runtime.

This feature makes the loop **edit → assemble → place on disk → boot** a single
toolchain, so a developer types one command and watches their program run.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Assemble to a loadable binary (Priority: P1)

A developer assembles a source file and gets a binary that an Apple II can
actually load: just the assembled bytes, optionally carrying the load address and
length the target operating system expects.

Today the only binary output is a full 64 KB memory image padded with fill bytes.
That shape is correct for ROM burning and reference comparison, but a developer
who wants to load a 2 KB routine at `$6000` has to slice 64 KB down by hand.

**Why this priority**: Every other story depends on having a loadable artifact.
Without it there is nothing to place on a disk. It is also the smallest change in
the feature and unblocks developers immediately, even before disk access exists —
they can pair it with an external disk tool the same day.

**Independent Test**: Assemble a source file with the new output selection and
confirm the file contains exactly the assembled bytes (and, when requested, the
4-byte address/length prefix), with no padding.

**Acceptance Scenarios**:

1. **Given** a source file that assembles to 512 bytes starting at `$6000`,
   **When** the developer requests raw binary output, **Then** the output file is
   exactly 512 bytes long and its first byte is the byte assembled at `$6000`.
2. **Given** the same source, **When** the developer requests binary output with a
   DOS 3.3 header, **Then** the output file is 516 bytes: a 2-byte load address of
   `$6000`, a 2-byte length of 512, then the assembled bytes.
3. **Given** the same source, **When** the developer requests the existing full-image
   output, **Then** the output is byte-for-byte identical to what Casso produces
   today.
4. **Given** source that assembles to a non-contiguous layout, **When** raw output
   is requested, **Then** the assembler reports what span was written so the
   developer can tell whether gaps were filled.

---

### User Story 2 - Put a file onto a disk image (Priority: P1)

A developer places an assembled binary onto a disk image, naming it and telling
the disk what kind of file it is and where it loads. The disk is then mounted in
Casso and the program is loaded from the guest.

**Why this priority**: This is the missing step the entire feature exists to
provide. Paired with Story 1 it completes the minimum viable loop: assemble,
place, boot, run.

**Independent Test**: Put a known binary onto a freshly created disk image, mount
that image in Casso, and confirm the guest's catalog lists the file and that
loading it produces the expected bytes in memory.

**Acceptance Scenarios**:

1. **Given** a formatted DOS 3.3 disk image and a 512-byte binary, **When** the
   developer places it as a binary file named `PROG` loading at `$6000`, **Then**
   booting the image and running `CATALOG` lists `B 002 PROG`, and `BLOAD PROG`
   places the bytes at `$6000`.
2. **Given** a formatted ProDOS disk image, **When** the developer places the same
   binary, **Then** the guest catalog lists it as type `BIN` with an auxiliary
   type of `$6000`.
3. **Given** a disk image that already contains a file of the same name, **When**
   the developer places a new file under that name, **Then** the existing file is
   replaced and the freed space is returned to the volume.
4. **Given** a disk image with insufficient free space, **When** the developer
   attempts to place a file, **Then** the operation fails with a message naming
   the shortfall and the image is left byte-for-byte unchanged.
5. **Given** a write-protected image, **When** the developer attempts to place a
   file, **Then** the operation is refused and the image is unchanged.

---

### User Story 3 - Read a disk image's contents (Priority: P2)

A developer lists what is on a disk image and extracts a file from it to the host
— to recover source stored on a disk, to inspect what a program actually wrote, or
to confirm that a placement worked.

**Why this priority**: Listing is how a developer verifies Story 2 without booting
the emulator, which makes the loop debuggable. Extraction answers the recurring
request to get source code *out* of a disk image and edit it on a modern machine.

**Independent Test**: List a disk image with known contents and confirm every file
is reported with the correct name, type, size, and lock state; extract one and
compare it byte-for-byte against the original.

**Acceptance Scenarios**:

1. **Given** a disk image containing several files, **When** the developer lists
   it, **Then** each file's name, type, size, and lock state is shown, along with
   the volume's free space.
2. **Given** a disk image containing a binary file, **When** the developer extracts
   it, **Then** the extracted bytes match what was originally placed, and the load
   address is reported.
3. **Given** a disk image containing a text file, **When** the developer extracts
   it, **Then** the high-bit character encoding is converted to host text and line
   endings are normalized.
4. **Given** a disk image whose catalog is damaged, **When** the developer lists it,
   **Then** the readable entries are reported and the damage is described rather
   than the operation failing outright.

---

### User Story 4 - Make the disk boot the program (Priority: P2)

A developer configures a bootable disk so that inserting it and powering on runs
their program, with no typing at the guest prompt.

Casso can already create bootable DOS 3.3 and ProDOS disks, but they always boot
to a stock greeting. There is no way to say "boot into *my* program."

**Why this priority**: It removes the last manual step from the loop. Without it
every iteration ends with the developer typing `BRUN PROG` by hand — tolerable
once, tedious fifty times a day. It is P2 rather than P1 because the loop is
already usable without it.

**Independent Test**: Create a bootable disk, place a program, set it as the boot
program, then boot the image in Casso and confirm the program runs without input.

**Acceptance Scenarios**:

1. **Given** a bootable DOS 3.3 image containing a binary file `PROG`, **When** the
   developer sets `PROG` as the boot program and boots the image, **Then** the
   program runs automatically after DOS loads.
2. **Given** a bootable ProDOS image, **When** the developer sets a system program
   as the boot program and boots the image, **Then** that program runs after
   ProDOS loads.
3. **Given** an image whose boot program names a file that is not present, **When**
   the developer sets it, **Then** the operation is refused with a message naming
   the missing file.

---

### User Story 5 - Boot straight into a program with no operating system (Priority: P3)

A developer produces a disk image that boots directly into a binary with no DOS or
ProDOS present at all — the program owns the machine from power-on and gets the
memory the operating system would otherwise occupy.

**Why this priority**: For demos, test harnesses, and "does this even run," waiting
for DOS to load is pure overhead, and the memory DOS occupies is often exactly
what the program needs. It is P3 because it serves a narrower audience than the
filesystem stories and is independently valuable — it does not depend on any of
them.

**Independent Test**: Produce a no-operating-system boot image from a binary, boot
it in Casso, and confirm the program begins executing without any filesystem
present on the disk.

**Acceptance Scenarios**:

1. **Given** a binary that loads at `$0800`, **When** the developer produces a
   direct-boot image from it and boots that image, **Then** the program is running
   within a noticeably shorter time than an equivalent DOS 3.3 boot.
2. **Given** a binary larger than the space a direct-boot image can carry, **When**
   the developer attempts to produce one, **Then** the operation is refused with a
   message stating the available capacity.
3. **Given** a direct-boot image, **When** the developer asks the program to be
   entered at an address other than its load address, **Then** execution begins at
   the named address.

---

### User Story 6 - Put BASIC source on a disk as a runnable program (Priority: P3)

A developer writes Applesoft BASIC as ordinary text on the host and places it on a
disk as a program the guest can `RUN` directly.

**Why this priority**: Hand-producing tokenized Applesoft is impractical, so
without this a developer who wants a BASIC loader or test harness must type it at
the guest prompt and save it there — which breaks the "edit on the host" premise.
It is P3 because assembly, not BASIC, is the primary audience.

**Independent Test**: Place a known BASIC listing on a disk, boot it, `LIST` the
program in the guest, and confirm the listing matches the source text.

**Acceptance Scenarios**:

1. **Given** a text file containing a numbered Applesoft listing, **When** the
   developer places it as a BASIC program, **Then** booting the disk and running
   `LIST` reproduces the original listing.
2. **Given** a listing containing a syntax error or an out-of-order line number,
   **When** the developer places it, **Then** the operation is refused with the
   offending line quoted and its line number reported.

---

### Edge Cases

- What happens when the target image is currently mounted in a running copy of
  Casso? The emulator holds the disk in memory and will write its own copy back
  when the drive flushes, silently discarding host-side edits. The tool MUST
  detect that the file is in use where the platform allows it and refuse, and MUST
  document the hazard where it cannot.
- What happens when a file name is not legal on the target filesystem — too long,
  lowercase, or containing characters the catalog cannot store? The name MUST be
  reported as rejected rather than silently truncated or transliterated.
- What happens when a placement fails partway through — out of space discovered
  after some sectors are written? The image MUST be left exactly as it was; a
  partially written file is worse than a failed operation.
- What happens when the volume's free-space map disagrees with what the catalog
  actually references? The inconsistency MUST be reported rather than compounded
  by allocating from a map that is already wrong.
- What happens when a file is placed on a bit-stream image that carries data the
  sector layer cannot represent (a copy-protected disk)? The operation MUST be
  refused with the reason, rather than rewriting the track and destroying the
  protection.
- What happens when a DOS 3.3 file is locked? Placement over it MUST be refused
  until it is unlocked, matching how the guest behaves.
- What happens when the assembled output is empty (source assembled no bytes)?
  Raw output MUST produce an empty file and say so, rather than producing a header
  claiming zero length with no data.

## Requirements *(mandatory)*

### Functional Requirements

#### Assembler output

- **FR-001**: The assembler MUST be able to emit only the assembled bytes, with no
  padding before the start address or after the end address.
- **FR-002**: The assembler MUST be able to emit the assembled bytes prefixed with
  a 2-byte load address and 2-byte length, in the layout DOS 3.3 binary files use.
- **FR-003**: The existing full-64 KB image, S-record, and Intel HEX outputs MUST
  remain available and byte-for-byte unchanged.
- **FR-004**: The assembler MUST report the start address, end address, and byte
  count of what it wrote, so a build script can place the result without
  re-deriving them.

#### Volume access

- **FR-005**: The system MUST read the file catalog of a DOS 3.3 volume, reporting
  each file's name, type, size in sectors, and lock state, plus the volume's free
  space.
- **FR-006**: The system MUST read the file catalog of a ProDOS volume, reporting
  each file's name, type, auxiliary type, size, and creation/modification stamps
  where present, plus the volume's free space.
- **FR-007**: The system MUST write a file into a DOS 3.3 volume, allocating from
  the volume's free-space map, creating the catalog entry and sector list, and
  leaving the map consistent with what was allocated.
- **FR-008**: The system MUST write a file into a ProDOS volume, allocating from
  the volume bitmap and growing the file's block structure as its size requires.
- **FR-009**: The system MUST extract a file's contents from either volume type.
- **FR-010**: The system MUST delete a file from either volume type, returning its
  space to the volume.
- **FR-011**: Writing a file whose name already exists MUST replace the existing
  file, not create a duplicate entry or fail.
- **FR-012**: Every write operation MUST be all-or-nothing: on any failure the
  image MUST be left byte-for-byte as it was.
- **FR-013**: The system MUST refuse to write to a volume that is write-protected,
  and MUST refuse to overwrite a file that is locked.
- **FR-014**: Volume access MUST work on every disk image format Casso can already
  mount, including bit-stream images, not only sector-order images.
- **FR-015**: The system MUST refuse to write to an image whose track data cannot
  be losslessly represented as standard sectors, reporting why.

#### File types and encoding

- **FR-016**: The system MUST accept a file type and, where the filesystem stores
  one, a load address when placing a file.
- **FR-017**: The system MUST convert host text to the target's character encoding
  and line-ending convention when placing a text file, and reverse the conversion
  when extracting one.
- **FR-018**: The system MUST convert an Applesoft BASIC listing in host text into
  the tokenized on-disk form when placing it as a BASIC program, and reverse the
  conversion when extracting one.
- **FR-019**: Placing a BASIC listing that cannot be tokenized MUST be refused with
  the offending line number and text reported.

#### Boot configuration

- **FR-020**: The system MUST set which program a bootable volume runs after its
  operating system loads.
- **FR-021**: Setting a boot program that is not present on the volume MUST be
  refused.
- **FR-022**: The system MUST be able to produce a disk image that boots directly
  into a supplied binary with no operating system present.
- **FR-023**: A direct-boot image MUST reject a payload that exceeds the memory the
  boot path can load, reporting the available capacity.
- **FR-024**: A direct-boot image MUST support entering the payload at an address
  other than its load address.

#### Interface

- **FR-025**: Every capability above MUST be reachable from the command-line tool
  using subcommand-style invocation consistent with the existing tool.
- **FR-026**: Every operation MUST report success or failure through a distinct
  exit status so a build script can branch on the outcome.
- **FR-027**: Failure messages MUST name the image, the file, and the reason, and
  MUST go to the error stream so they do not contaminate piped output.
- **FR-028**: Every capability MUST be documented in the tool's help output.
- **FR-029**: The system MUST detect, where the host platform allows it, that a
  target image is currently in use by a running emulator and refuse to modify it.

### Key Entities

- **Volume**: A formatted filesystem on a disk image — DOS 3.3 or ProDOS. Knows its
  total and free capacity, its name or number, and the files it contains.
- **File Entry**: One catalog record — name, type, size, lock state, and (per
  filesystem) load address, auxiliary type, and timestamps.
- **File Payload**: The bytes of one file, plus the metadata needed to place it
  correctly (its type and, where applicable, its load address).
- **Boot Configuration**: What a bootable volume runs after its operating system
  loads, or — for a direct-boot image — the payload, its load address, and its
  entry point.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can go from an edited source file to their program
  running in the emulator with a single command invocation per step and no
  third-party tool.
- **SC-002**: A developer new to Casso can complete that loop for the first time
  using only the tool's own help output, without consulting external documentation.
- **SC-003**: Files placed by Casso are read correctly by the guest operating
  system in 100% of cases for the supported file types — verified by listing and
  loading each type from a booted machine.
- **SC-004**: Files extracted by Casso match the bytes originally placed exactly,
  for every supported file type, across every image format Casso can mount.
- **SC-005**: No failed operation ever leaves a disk image in a state the guest
  operating system cannot read — verified by attempting every documented failure
  mode and booting the image afterwards.
- **SC-006**: A full iteration — assemble, place, launch, program running — takes
  under 10 seconds on a typical development machine for a program of a few
  kilobytes.
- **SC-007**: A direct-boot image reaches the developer's code measurably faster
  than the equivalent operating-system boot of the same program.

## Assumptions

- The primary audience is a developer assembling 6502 code on Windows for a
  35-track, 140 KB 5.25-inch disk. Larger media and additional filesystems are out
  of scope here and are covered by a later feature.
- DOS 3.3 and ProDOS are the only filesystems in scope. Pascal, CP/M, and RDOS
  volumes are not addressed.
- The command-line tool is the only interface in this feature. A graphical disk
  manager is a separate feature and does not gate this one.
- Read-side support for damaged or unusual volumes is best-effort: the goal is a
  useful report, not recovery.
- Casso's existing blank-disk creation covers producing the formatted, optionally
  bootable images this feature writes into; no new formatting capability is
  required beyond setting the boot program.
- The tool operates on image files on disk. Modifying an image while it is mounted
  in a running emulator is explicitly not supported by this feature; doing so
  safely is a concern of the graphical disk manager, which can coordinate with the
  running machine.
- File placement targets standard-format volumes. Copy-protected bit-stream images
  are readable but not writable, and this is expected rather than a defect.
