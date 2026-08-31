# Feature Specification: Additional Disk Image Formats and Filesystems

**Feature Branch**: `022-disk-image-formats`

**Created**: 2026-08-15

**Status**: Draft

**Input**: User description: "The remaining disk image formats and filesystems that AppleCommander and the wider Apple II ecosystem support, so software people actually download opens in Casso without conversion."

## Overview

Casso mounts four image types today: `.dsk`, `.do`, `.po`, and `.woz`. The Apple II
software that exists in the world does not confine itself to those. Archives ship
as `.2mg`, as `.nib`, wrapped in ShrinkIt or Binary II, gzip-compressed, or as
DiskCopy images. A user who downloads a disk and finds Casso will not open it
learns that Casso is the problem, not the file.

This feature widens what Casso accepts, and widens what it can read once a volume
is open. It is deliberately last of the disk-related features: it multiplies the
reach of capabilities that must exist first, and adds nothing on its own.

Scope is governed by one rule: **support a format only where something can be done
with it.** A large hard-disk volume format is not useful until an emulated device
exists that could mount one, so those parts of this feature are gated on the
corresponding hardware work rather than shipped as dead readers.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Open a 2MG image (Priority: P1)

A user opens a `.2mg` image and Casso mounts it correctly, honoring the metadata
the container carries rather than guessing.

**Why this priority**: 2MG is the most common modern container, and it is the only
one that carries the facts Casso currently has to infer, sector ordering, volume
number, and write-protect state. Supporting it removes a class of
silently-wrong-guess bugs as well as adding reach.

**Independent Test**: Mount 2MG images wrapping DOS-ordered, ProDOS-ordered, and
nibble data, and confirm each boots and catalogs identically to the equivalent bare
image.

**Acceptance Scenarios**:

1. **Given** a 2MG image wrapping DOS-ordered sectors, **When** the user mounts it,
   **Then** it behaves identically to the same data as a `.do` image.
2. **Given** a 2MG image wrapping ProDOS-ordered sectors, **When** the user mounts
   it, **Then** it behaves identically to the same data as a `.po` image.
3. **Given** a 2MG image whose header marks it write-protected, **When** the user
   mounts it, **Then** the disk is write-protected and the interface states that
   the image's own flag is the cause.
4. **Given** a mounted 2MG image, **When** the guest writes to it and the image is
   flushed, **Then** the file is written back as a valid 2MG with its header intact.
5. **Given** a 2MG image with a malformed or unrecognized header, **When** the user
   mounts it, **Then** it is refused with a message naming the problem.

---

### User Story 2 - Open a nibble image (Priority: P2) -- DELIVERED in 027

**Delivered by `specs/027-nibble-images`**, which was split out of this story the
way 023 was split out of 019. `.nib` and `.nb2` mount, boot and write back; all
nine `disk` commands take them. The acceptance scenarios below stand as written,
including the second one's wording about protection surviving only as far as the
format preserved it, which turned out to be exactly right. FR-003 below is
likewise delivered there and is not this feature's to do again.


A user opens a `.nib` image and it mounts and boots.

**Why this priority**: Nibble images predate WOZ as the way protected disks were
preserved, and a good deal of archived software is still in that form. It is P2
because WOZ already covers the modern preservation case, so this is reach rather
than capability.

**Independent Test**: Mount a nibble image of a known disk and confirm it boots and
catalogs identically to the same disk in another format.

**Acceptance Scenarios**:

1. **Given** a nibble image of a standard disk, **When** the user mounts it, **Then**
   it boots and catalogs correctly.
2. **Given** a nibble image of a copy-protected disk, **When** the user mounts it,
   **Then** it boots to the extent the format preserves the protection.
3. **Given** a mounted nibble image the guest has written to, **When** it is
   flushed, **Then** the file is written back in nibble form without corrupting
   unwritten tracks.

---

### User Story 3 - Open a compressed or archived disk (Priority: P3)

A user opens a disk that arrives compressed or inside an archive (gzip, zip,
ShrinkIt, or Binary II) and Casso extracts the image and mounts it, without the
user unpacking anything first.

**Why this priority**: This is how Apple II software is actually distributed, so it
removes the most common friction between finding software and running it. It is P3
because the workaround, unpack it yourself, is obvious and cheap, unlike the
earlier stories where no workaround exists.

**Independent Test**: Open a disk image in each supported wrapper and confirm it
mounts identically to the unwrapped image.

**Acceptance Scenarios**:

1. **Given** a gzip-compressed disk image, **When** the user opens it, **Then** it
   mounts identically to the uncompressed image.
2. **Given** an archive containing exactly one disk image, **When** the user opens
   it, **Then** that image is mounted.
3. **Given** an archive containing several disk images, **When** the user opens it,
   **Then** the user is asked which to mount.
4. **Given** an archive containing no disk image, **When** the user opens it,
   **Then** it is refused with a message saying so.
5. **Given** a disk opened from a read-only wrapper, **When** the guest writes to
   it, **Then** the behavior (write-protected, or written back to a separate file)
   is stated to the user rather than silently chosen.

---

### User Story 4 - Read volumes in additional filesystems (Priority: P4)

A user opens a disk formatted with Pascal, CP/M, or another period filesystem and
can see and extract its contents.

**Why this priority**: These volumes exist in archives and are currently opaque.
Read access makes them useful for recovery and curiosity. It is P4 because the
audience is much smaller than for DOS 3.3 and ProDOS, and because write support is
explicitly not proposed, reading is the whole value.

**Independent Test**: Open a volume in each supported filesystem and confirm its
catalog is listed correctly and extracted files match reference contents.

**Acceptance Scenarios**:

1. **Given** a Pascal volume, **When** the user lists it, **Then** its files are
   reported with name, type, and size.
2. **Given** a Pascal volume, **When** the user extracts a file, **Then** the bytes
   match the reference.
3. **Given** a volume in a filesystem that is readable but not writable, **When**
   the user attempts to modify it, **Then** the action is unavailable and the
   reason is stated.
4. **Given** a volume in no recognized filesystem, **When** the user opens it,
   **Then** it is reported as unformatted or unrecognized, and sector-level
   inspection remains available.

---

### User Story 5 - Work with larger media (Priority: P5)

A user mounts an 800 KB 3.5-inch volume or a ProDOS hard-disk volume and works
with it as with any other disk.

**Why this priority**: Larger media are widely used, but nothing in Casso can
currently mount one; there is no emulated 3.5-inch drive and no mass-storage
device. Shipping a reader with no device to read it into would be dead code, so
this story is deliberately gated on that hardware existing.

**Independent Test**: With a suitable emulated device present, mount a large volume
and confirm the guest can catalog and load from it.

**Acceptance Scenarios**:

1. **Given** an emulated device that accepts 800 KB media, **When** the user mounts
   an 800 KB ProDOS volume, **Then** the guest catalogs and loads from it.
2. **Given** an emulated mass-storage device, **When** the user mounts a ProDOS
   hard-disk volume, **Then** the guest catalogs and loads from it.
3. **Given** a large volume and no device that can accept it, **When** the user
   attempts to mount it, **Then** the refusal explains that the machine has no
   suitable drive.
4. **Given** a large ProDOS volume, **When** the user manages files on it, **Then**
   directory hierarchies and free space are handled correctly at that capacity.

---

### Edge Cases

- What happens when a file's extension disagrees with its actual content; a
  ProDOS-ordered image named `.do`? Content MUST take precedence over extension
  where the format can be identified from the data, and the discrepancy MUST be
  reported.
- What happens when a container's declared geometry disagrees with its actual size?
  The image MUST be refused with the discrepancy named, rather than mounted with a
  guess.
- What happens when a compressed image expands to an implausible size? Expansion
  MUST be bounded and refused past a stated limit, rather than exhausting memory.
- What happens when an archive is corrupt partway through? The failure MUST be
  reported against the archive, not surfaced as a corrupt disk.
- What happens when the guest writes to a disk that came from a wrapper Casso can
  read but not produce? The system MUST decide and state the outcome up front, 
  either write-protect the disk or write back to a separate file, rather than
  discovering it at flush time.
- What happens to an image format that preserves flux or protection details that
  sector-level operations cannot represent? File-level modification MUST be
  refused, consistent with the existing rule for such media.
- What happens when a nibble image contains tracks that do not decode to standard
  sectors? They MUST be preserved as-is through a write-back rather than being
  normalized away.

## Requirements *(mandatory)*

### Functional Requirements

#### Container formats

- **FR-001**: The system MUST mount 2MG images, honoring the container's declared
  sector ordering, volume number, and write-protect flag.
- **FR-002**: The system MUST write 2MG images back preserving their header.
- **FR-003**: The system MUST mount nibble images and write them back without
  altering tracks the guest did not write. **DELIVERED in `specs/027-nibble-images`.**
- **FR-004**: The system MUST identify an image's format from its content where
  possible, and MUST report any disagreement between content and file extension.
- **FR-005**: The system MUST refuse an image whose declared geometry is
  inconsistent with its content, naming the inconsistency.

#### Wrappers and archives

- **FR-006**: The system MUST open disk images that are gzip-compressed.
- **FR-007**: The system MUST open disk images contained in archives, including the
  archive formats commonly used to distribute Apple II software.
- **FR-008**: When an archive contains more than one disk image, the system MUST
  let the user choose which to open.
- **FR-009**: When an archive contains no disk image, the system MUST say so.
- **FR-010**: The system MUST bound decompression and refuse inputs that expand
  beyond a stated limit.
- **FR-011**: For any image opened from a wrapper the system cannot write back, the
  write behavior MUST be determined and communicated at mount time.

#### Filesystems

- **FR-012**: The system MUST read Pascal volumes, listing and extracting their
  files.
- **FR-013**: The system MUST report a volume whose filesystem it does not
  recognize as unformatted or unrecognized, while keeping sector-level inspection
  available.
- **FR-014**: For any filesystem the system reads but cannot write, modification
  MUST be unavailable with the reason stated.

#### Larger media

- **FR-015**: The system MUST support volume capacities beyond 140 KB, including
  800 KB media and ProDOS hard-disk volumes, wherever an emulated device exists
  that can mount them.
- **FR-016**: The system MUST refuse to mount media the current machine has no
  device for, explaining why.
- **FR-017**: File management on large volumes MUST handle their directory
  hierarchies and free-space accounting correctly at full capacity.

#### Consistency

- **FR-018**: Every newly supported format MUST work with all existing disk
  capabilities (mounting, booting, file access, inspection, and management) or
  the exceptions MUST be stated.
- **FR-019**: Newly supported formats MUST be selectable wherever images are
  chosen, including file dialogs and drag-and-drop.
- **FR-020**: Documentation MUST list every supported format and, for each, whether
  it can be read, written, and modified at file level.

### Key Entities

- **Image Container**: The file-level wrapper around disk data (bare sectors, a
  header-bearing container, a nibble stream, or a compressed or archived form)
  together with any metadata it carries.
- **Media Geometry**: A volume's capacity, track and sector or block organization,
  and the emulated device classes that can accept it.
- **Filesystem Reader**: The ability to interpret a volume's catalog for a given
  filesystem, and whether that ability extends to writing.
- **Format Capability Matrix**: For every supported format, which operations are
  available, mount, boot, read files, write files, inspect.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can open the disk images distributed by the major Apple II
  software archives without converting or unpacking anything first.
- **SC-002**: Every format Casso claims to support is verified against real-world
  images from those archives, not only synthetic test data.
- **SC-003**: No supported format can be mounted in a way that silently corrupts it
  on write-back, verified by round-tripping every format and comparing.
- **SC-004**: A user can determine what Casso can do with a given format from the
  documentation alone, without trial and error.
- **SC-005**: An image that cannot be opened always produces a message identifying
  the reason, with no unexplained failures.
- **SC-006**: Adding format support changes no existing behavior for the four
  formats supported today, verified by the existing test corpus.

## Assumptions

- File-level access, the disk manager, and inspection all exist already; this
  feature widens what they apply to and does not reimplement them.
- Large-media support is gated on emulated mass-storage and 3.5-inch drive hardware
  (GitHub issues #101 and #93). Without those, the reader has nothing to mount
  into, and that part of this feature waits.
- Write support is expected for the container formats and not expected for the
  additional filesystems; reading Pascal and similar volumes is the whole value
  there.
- Archive and compression handling favors reading. Producing archives is not a goal.
- Any decompression or archive handling must satisfy the project's dependency
  rules: source-vendored, permissively licensed, no package manager and no binary
  downloads. If no candidate meets that bar for a given format, that format is
  deferred rather than granted an exception.
- Casso's documentation currently claims nibble images can be dragged onto a drive,
  which the mounting code does not support. That discrepancy is resolved by this
  feature delivering the capability.
