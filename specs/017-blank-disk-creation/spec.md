# Feature Specification: Blank Disk Creation & Mounting

**Feature Branch**: `017-blank-disk-creation`

**Created**: 2026-07-08

**Status**: Draft

**Input**: User description: "There's a glaring omission — no way to create a blank disk of a given format and then insert that into the drive."

## Overview

Casso can insert existing disk images into a drive, but there is no way to
**create a new, empty disk** from within the app. This blocks every workflow
that needs a fresh place to write: saving a BASIC program, `INIT`-ing a disk,
capturing a game's save state, or letting an application (e.g. The Print Shop)
store its configuration to a data disk. Users must currently fabricate a blank
image with an external tool and then insert it — a gap that makes the emulator
feel read-only.

This feature adds a first-class "create a blank disk" action: the user picks a
format and (optionally) a filesystem, names the file, and the new disk is
created on the host and mounted into a chosen drive, ready to use.

## Clarifications

### Session 2026-07-08

- Q: Default image format for a new writable disk? → A: WOZ — its writes
  round-trip reliably; `.dsk` writes are currently broken (tracked separately
  as a defect) so WOZ is the safe default for a disk the user intends to write.
- Q: Should new disks be pre-formatted or blank? → A: Offer both, defaulting to
  pre-formatted (DOS 3.3) so the disk is immediately usable; an "unformatted"
  option is available for users who want to `INIT`/format from the guest.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Create a ready-to-use disk and save to it (Priority: P1)

A user is running the //e and wants to save their work (a BASIC program, a
document, an app's settings). They invoke a "New Blank Disk" action, accept the
defaults, and the app creates a fresh, pre-formatted disk and mounts it in a
drive. They immediately `SAVE`/store from the guest and it succeeds.

**Why this priority**: This is the whole point of the feature — turning the
emulator from read-only into something a user can actually save to. Delivered
alone, it unblocks every "save my work" workflow. Choosing WOZ + a standard
filesystem by default means the saved data reliably round-trips.

**Independent Test**: From a running machine, create a new disk with defaults,
then from the guest write a file and read it back (e.g. DOS 3.3
`SAVE TEST` → `LOAD TEST` → `LIST`). The write completes without error and the
data survives a re-read (and a re-mount).

**Acceptance Scenarios**:

1. **Given** a running //e with a free drive, **When** the user creates a new
   blank disk with default options and mounts it, **Then** the drive shows the
   new disk and the guest can write and re-read a file with no I/O error.
2. **Given** a newly created disk, **When** the machine is closed and the disk
   re-mounted later, **Then** previously written files are still present.
3. **Given** the target drive already has a disk, **When** the user creates and
   mounts a new disk into that drive, **Then** the user is warned/confirmed
   before the existing disk is replaced.

---

### User Story 2 - Choose format and filesystem (Priority: P2)

A user creating a disk wants control over the image format (WOZ / DSK / PO) and
what's on it (DOS 3.3, ProDOS, or unformatted), because the target software
expects a particular combination.

**Why this priority**: Different guest software needs different disks (a DOS 3.3
game-save vs. a ProDOS data disk). Without a choice, the feature only serves the
default case. It builds directly on US1's create-and-mount flow.

**Independent Test**: Create a disk for each supported combination and confirm
the guest recognizes it — e.g. a ProDOS-formatted disk `CAT`s clean under
ProDOS; a DOS 3.3 disk `CATALOG`s clean under DOS 3.3; an unformatted disk is
rejected until the guest `INIT`s/formats it.

**Acceptance Scenarios**:

1. **Given** the create dialog, **When** the user selects ProDOS + WOZ, **Then**
   the resulting disk mounts and is a valid, empty ProDOS volume.
2. **Given** the create dialog, **When** the user selects "unformatted", **Then**
   the disk mounts as blank media and the guest can `INIT`/format it.
3. **Given** an incompatible combination (e.g. a format that cannot represent
   the chosen filesystem ordering), **When** the user selects it, **Then** the
   option is disabled or the user is told why it is unavailable.

---

### User Story 3 - Name and locate the new disk (Priority: P3)

A user wants the new disk saved to a sensible place with a clear name, and to be
able to choose a different folder/name.

**Why this priority**: Quality-of-life. The disk has to live somewhere on the
host; a good default plus an override keeps files organized without blocking the
core create-and-mount flow.

**Independent Test**: Create a disk accepting the default name/location and
confirm the file appears there with the correct extension; create another
choosing a custom folder/name and confirm it lands there and mounts.

**Acceptance Scenarios**:

1. **Given** the create action, **When** the user accepts defaults, **Then** the
   disk is written to a predictable default folder with a unique, non-colliding
   name and the chosen format's extension.
2. **Given** a name that already exists in the target folder, **When** the user
   confirms, **Then** the app avoids silent overwrite (unique-name or explicit
   confirm).

---

### Edge Cases

- Target drive already occupied → confirm before replacing the mounted disk.
- Target host folder is read-only / disk full → surface a clear error, create
  nothing, leave any existing mount untouched.
- Filename collision in the target folder → auto-unique or explicit overwrite
  confirmation; never silently clobber.
- Format/filesystem mismatch (e.g. ProDOS ordering requested for a DOS-order
  `.dsk`) → prevent the invalid combination up front.
- Creating while a machine has no Disk ][ controller (or no free drive) → the
  action is unavailable or clearly explains why.
- A created disk must itself be writable by the guest — see Dependencies re:
  the `.dsk` write-round-trip defect; WOZ is the reliable default until that is
  fixed.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The app MUST provide a user-invocable action to create a new,
  empty disk image (menu command and/or drive-widget control), available when a
  machine with a Disk ][ controller is running.
- **FR-002**: The user MUST be able to choose the image **format** for the new
  disk. v1 supports WOZ, DSK (DOS-order), and PO (ProDOS-order).
- **FR-003**: The user MUST be able to choose the initial **filesystem/content**:
  DOS 3.3, ProDOS, or unformatted (raw/blank media).
- **FR-004**: The app MUST default to a **writable-reliable** configuration
  (WOZ, DOS 3.3, pre-formatted) so accepting defaults yields a disk the guest
  can immediately and correctly save to.
- **FR-005**: A pre-formatted disk MUST mount as a valid, empty volume of the
  chosen filesystem (the guest lists it clean with no files); an unformatted
  disk MUST mount as blank media the guest can `INIT`/format.
- **FR-006**: The app MUST write the new disk to a host file and MUST let the
  user pick the destination (folder + name), with a sensible default folder and
  a unique default name carrying the format's correct extension.
- **FR-007**: The app MUST NOT silently overwrite an existing host file — it
  MUST auto-generate a unique name or require explicit confirmation.
- **FR-008**: After creation the app MUST mount the new disk into a user-chosen
  drive (default: an empty drive, or drive 1), reusing the existing mount path
  so the drive widget, MRU, and persistence behave identically to inserting any
  other image.
- **FR-009**: If the chosen drive already holds a disk, the app MUST confirm
  before replacing it.
- **FR-010**: The app MUST prevent invalid format/filesystem combinations (e.g.
  a filesystem ordering the chosen format cannot represent), disabling or
  explaining unavailable options rather than producing a broken disk.
- **FR-011**: On any failure (I/O error, permission, full disk), the app MUST
  create nothing partial, leave existing mounts untouched, and report a clear
  error.
- **FR-012**: New disks MUST NOT be marked write-protected by default (the user
  intends to write to them); existing write-protect controls still apply
  afterward.
- **FR-013**: The blank-image construction (track/sector layout, filesystem
  skeleton for DOS 3.3 / ProDOS, format encoding) MUST be pure and unit-testable
  — no window, file, or registry dependency in the generation logic — with only
  the host file write and mount as the thin shell edge (per the core/shell
  doctrine: logic in core for UT coverage).

### Key Entities *(include if feature involves data)*

- **Blank Disk Template**: The in-memory description of a disk to create —
  format, filesystem/content, size (140K 5.25"), volume name/number. Produced by
  pure logic; serialized to the chosen on-disk format.
- **Disk Format**: WOZ (bit-stream, order-agnostic, writes round-trip), DSK
  (DOS 3.3 sector order), PO (ProDOS sector order). Governs encoding + which
  filesystems are representable.
- **Filesystem Skeleton**: The empty on-disk structures for a formatted volume —
  DOS 3.3 (VTOC + catalog track) or ProDOS (volume bitmap + volume directory) —
  or none for unformatted.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: From a running machine, a user can create a writable disk and
  successfully `SAVE` a file from the guest — with no external tools — in under
  30 seconds and a handful of clicks.
- **SC-002**: A file written to a newly created (default WOZ) disk reliably
  round-trips: it re-reads correctly in the same session and after re-mounting
  the saved image (0% data-loss on the happy path).
- **SC-003**: Every supported format×filesystem combination that the UI offers
  produces a disk the corresponding guest OS recognizes as a valid empty volume
  (or blank media for "unformatted"); no offered combination yields an
  unreadable disk.
- **SC-004**: Creation never corrupts or overwrites unintended data: no silent
  host-file overwrite, and a failed creation leaves the prior drive state and
  host filesystem unchanged, 100% of the time.

## Assumptions

- Target machines are Apple II family with a Disk ][ controller (5.25" 140K
  media). 3.5"/800K, hard-disk, and larger volumes are out of scope for v1.
- v1 supported formats are WOZ, DSK, and PO; NIB and 2MG creation are out of
  scope (NIB/2MG may still be mountable via existing insert). Format **conversion**
  of existing images is out of scope.
- Default new-disk configuration is **WOZ + DOS 3.3 + pre-formatted**, chosen
  because WOZ writes round-trip reliably today.
- A custom dxui create dialog is desirable but the initial surface may reuse
  existing UI patterns (menu + standard save dialog + a small options prompt);
  final UI treatment is a design detail for planning.
- Pre-formatting produces a standard empty volume (default volume name/number);
  advanced volume metadata is not user-configurable in v1.

## Dependencies

- **Writable target correctness**: A created disk is only useful if the guest
  can write to it. WOZ writes round-trip today; **`.dsk` writes are currently
  broken** (DOS `SAVE` → I/O ERROR; tracked as a separate high-priority defect,
  GitHub issue #89). This feature therefore defaults to WOZ and does not depend
  on that fix; creating writable `.dsk`/`.po` disks becomes fully reliable once
  #89 is resolved. Creating a blank WOZ also gives that fix a clean test bed.
- Reuses the existing disk **mount** path (drive widget, MRU, per-machine
  persistence) rather than introducing a parallel insertion mechanism.
