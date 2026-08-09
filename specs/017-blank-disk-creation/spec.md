# Feature Specification: Blank Disk Creation & Mounting

**Feature Branch**: `017-blank-disk-creation`

**Created**: 2026-07-08

**Updated**: 2026-08-08 (entry point, create-dialog navigation, write-protect toggle)

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

This feature adds a first-class "create a blank disk" action, discovered where
users already go to get a disk into a drive: the insert-disk picker carries a
**`<Create new disk...>`** entry pinned as its first row. Selecting it opens a
create dialog the user can navigate like a real file-save dialog — browse
folders, see existing files, choose the location and name — plus the image
type choices (format and initial contents). The new disk is created on the
host and mounted into the chosen drive, ready to use.

The feature also closes an adjacent gap: there is no way to change a disk's
**write protection** from within the app. Mounted images whose format supports
it get an on/off write-protect toggle.

## Clarifications

### Session 2026-07-08

- Q: Default image format for a new writable disk? → A: WOZ — its writes
  round-trip reliably; `.dsk` writes are currently broken (tracked separately
  as a defect) so WOZ is the safe default for a disk the user intends to write.
  *(2026-08-08 update: the `.dsk` write defect — GH #89 — was fixed 2026-07-09
  and `.dsk`/`.po` writes now round-trip. WOZ remains the default: it is
  order-agnostic and represents any filesystem, so the default stays the most
  robust choice rather than a workaround.)*
- Q: Should new disks be pre-formatted or blank? → A: Offer both, defaulting to
  pre-formatted (DOS 3.3) so the disk is immediately usable; an "unformatted"
  option is available for users who want to `INIT`/format from the guest.

### Session 2026-08-08

- Q: Where does the user invoke "create a new disk"? → A: In the existing
  insert-disk picker, as a `<Create new disk...>` row pinned as the FIRST item
  regardless of the list's sort order. Selecting it opens the create dialog.
- Q: What is the create dialog? → A: An in-app themed dialog that is navigable
  like a real file-save dialog — the user can browse folders, see existing
  files, and choose the destination folder and file name inside the dialog —
  alongside the image-type controls (format WOZ/DSK/PO; contents DOS 3.3 /
  ProDOS / unformatted).
- Q: Additional scope? → A: The user needs a way to toggle write protection
  on/off for a mounted image, for image formats that support it (WOZ carries an
  in-image write-protect flag; formats without one use the host file's
  read-only attribute).
- Q: Should a pre-formatted disk be data-only or bootable? → A: Both — the
  create dialog gets a **bootable** toggle, and the contents choice is a
  listbox naming the OS (DOS 3.3, ProDOS by version, unformatted). Bootable
  writes the chosen OS onto the disk, sourced through the existing
  consent-based asset bootstrap (the same mechanism that fetches ROMs); the
  OS is never bundled in the binary. Default remains data-only.
- Q: Write-protect toggle on DSK/PO (no in-image flag)? → A: All formats —
  WOZ flips its in-image flag; DSK/PO set/clear the host file's read-only
  attribute.
- Q: Default destination folder? → A: `Documents\Casso Disks` on first use
  (created on demand, mirroring `Pictures\Casso Prints`); thereafter the
  dialog defaults to the last folder a disk was created in (persisted).
- Q: Creating over a file currently mounted in a drive? → A: Refuse with a
  clear "that image is mounted in Drive N" error; the user must eject first.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Create a ready-to-use disk and save to it (Priority: P1)

A user is running the //e and wants to save their work (a BASIC program, a
document, an app's settings). They click the drive to insert a disk, choose the
`<Create new disk...>` row at the top of the picker, accept the defaults in the
create dialog, and the app creates a fresh, pre-formatted disk and mounts it in
the drive. They immediately `SAVE`/store from the guest and it succeeds.

**Why this priority**: This is the whole point of the feature — turning the
emulator from read-only into something a user can actually save to. Delivered
alone, it unblocks every "save my work" workflow. Choosing WOZ + a standard
filesystem by default means the saved data reliably round-trips.

**Independent Test**: From a running machine, create a new disk with defaults,
then from the guest write a file and read it back (e.g. DOS 3.3
`SAVE TEST` → `LOAD TEST` → `LIST`). The write completes without error and the
data survives a re-read (and a re-mount).

**Acceptance Scenarios**:

1. **Given** a running //e with a free drive, **When** the user opens the
   insert picker, **Then** `<Create new disk...>` is the first row regardless
   of how the rest of the list is sorted.
2. **Given** the picker's `<Create new disk...>` row, **When** the user selects
   it and accepts the create dialog's defaults, **Then** the drive shows the
   new disk and the guest can write and re-read a file with no I/O error.
3. **Given** a newly created disk, **When** the machine is closed and the disk
   re-mounted later, **Then** previously written files are still present.
4. **Given** the target drive already has a disk, **When** the user creates and
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
4. **Given** the bootable toggle enabled with DOS 3.3 (or a ProDOS version)
   selected and its OS payload available, **When** the disk is created and
   booted, **Then** the emulator boots it to the chosen OS's prompt.
5. **Given** the bootable toggle with the OS payload unavailable (no consent
   or offline), **When** the user views the option, **Then** it is disabled
   with an explanation and data-only creation still works.

---

### User Story 3 - Name and locate the new disk in-dialog (Priority: P3)

A user wants the new disk saved to a sensible place with a clear name, chosen
without leaving the create dialog — navigating it the way they would a real
file-save dialog.

**Why this priority**: Quality-of-life. The disk has to live somewhere on the
host; a good default plus in-dialog navigation keeps files organized without
blocking the core create-and-mount flow.

**Independent Test**: Create a disk accepting the default name/location and
confirm the file appears there with the correct extension; create another by
navigating to a different folder and typing a new name inside the dialog, and
confirm it lands there and mounts.

**Acceptance Scenarios**:

1. **Given** the create action, **When** the user accepts defaults, **Then** the
   disk is written to a predictable default folder with a unique, non-colliding
   name and the chosen format's extension.
2. **Given** the create dialog, **When** the user browses into a different
   folder, **Then** the dialog shows that folder's existing files (so name
   collisions are visible before confirming) and creates the disk there.
3. **Given** a name that already exists in the target folder, **When** the user
   confirms, **Then** the app avoids silent overwrite (unique-name or explicit
   confirm).

---

### User Story 4 - Toggle write protection on a mounted image (Priority: P2)

A user wants to protect a disk from accidental writes (a master disk, a golden
save), or un-protect one they previously locked, from within the app.

**Why this priority**: Write protection is half of a real disk workflow — the
notch tab on a physical 5.25" floppy. Casso already *honors* write protection;
this story makes it *controllable*. It is user-requested scope for this
feature.

**Independent Test**: Mount a WOZ disk, toggle write protection on, attempt a
guest `SAVE` (expect the guest's write-protect error), toggle it off, and
confirm the `SAVE` now succeeds. Repeat for a `.dsk` (host read-only attribute)
and confirm the same behavior.

**Acceptance Scenarios**:

1. **Given** a mounted WOZ image, **When** the user toggles write protection
   on, **Then** the image's own write-protect flag is set, the guest sees the
   disk as protected, and the state survives remount (it lives in the image).
2. **Given** a mounted `.dsk`/`.po` image (no in-image flag), **When** the user
   toggles write protection on, **Then** the host file's read-only attribute is
   set and the guest sees the disk as protected.
3. **Given** a write-protected mounted image, **When** the user toggles
   protection off, **Then** the flag/attribute is cleared and the guest can
   write again.
4. **Given** any mounted image, **When** its write-protect state changes,
   **Then** the drive's existing write-protect indication reflects the new
   state immediately.

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
- Bootable requested but the OS payload is unavailable (no consent, offline) →
  the toggle is disabled with an explanation; data-only creation still works.
- Destination path is an image currently mounted in a drive → refused with an
  error naming the drive; the prior mount and file are untouched.
- Write-protect toggle fails (host permission denies the attribute change, or
  the image file is missing) → report the error and leave the effective state
  unchanged and truthfully indicated.
- Write-protect toggled while the guest is mid-write → the change applies at a
  safe boundary; an in-flight sector write is never torn.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The insert-disk picker MUST offer a `<Create new disk...>` entry
  pinned as its FIRST row regardless of the list's sort order, available when a
  machine with a Disk ][ controller is running; selecting it opens the create
  dialog targeting the drive the picker was opened for.
- **FR-002**: The user MUST be able to choose the image **format** for the new
  disk. v1 supports WOZ, DSK (DOS-order), and PO (ProDOS-order).
- **FR-003**: The user MUST be able to choose the initial **contents** from a
  list naming the OS: DOS 3.3, ProDOS (by version), or unformatted (raw/blank
  media).
- **FR-004**: The app MUST default to **WOZ, DOS 3.3, pre-formatted,
  non-bootable (data-only)** so accepting defaults yields a disk the guest can
  immediately and correctly save to, with no download dependency.
- **FR-005**: A pre-formatted disk MUST mount as a valid, empty volume of the
  chosen filesystem (the guest lists it clean with no files); an unformatted
  disk MUST mount as blank media the guest can `INIT`/format.
- **FR-006**: The create dialog MUST let the user choose the destination
  **inside the dialog**, navigating like a real file-save dialog: browse
  folders (including up/into), see the existing files of the current folder,
  and edit the file name — with a unique default name carrying the chosen
  format's correct extension. The default folder is `Documents\Casso Disks`
  (created on demand) on first use, and thereafter the last folder a disk was
  created in (persisted across sessions).
- **FR-007**: The app MUST NOT silently overwrite an existing host file — it
  MUST auto-generate a unique name or require explicit confirmation.
- **FR-008**: After creation the app MUST mount the new disk into the target
  drive, reusing the existing mount path so the drive widget, MRU, and
  persistence behave identically to inserting any other image.
- **FR-009**: If the target drive already holds a disk, the app MUST confirm
  before replacing it.
- **FR-010**: The app MUST prevent invalid format/filesystem combinations (e.g.
  a filesystem ordering the chosen format cannot represent), disabling or
  explaining unavailable options rather than producing a broken disk.
- **FR-011**: On any failure (I/O error, permission, full disk), the app MUST
  create nothing partial, leave existing mounts untouched, and report a clear
  error.
- **FR-012**: New disks MUST NOT be write-protected by default (the user
  intends to write to them).
- **FR-013**: The blank-image construction (track/sector layout, filesystem
  skeleton for DOS 3.3 / ProDOS, format encoding) MUST be pure and unit-testable
  — no window, file, or registry dependency in the generation logic — with only
  the host file write and mount as the thin shell edge (per the core/shell
  doctrine: logic in core for UT coverage).
- **FR-014**: The app MUST provide a user-invocable toggle for a mounted
  image's write protection. For formats with an in-image write-protect flag
  (WOZ), the toggle MUST set/clear that flag so the state travels with the
  image; for formats without one (DSK/PO), the toggle MUST set/clear the host
  file's read-only attribute.
- **FR-015**: The effective write-protect state MUST be reflected by the
  drive's existing write-protect indication immediately after a toggle, and
  the guest MUST observe the new state (writes blocked / allowed) from the
  next write attempt at a safe I/O boundary.
- **FR-016**: If a write-protect toggle cannot be applied (permissions, missing
  file), the app MUST report the failure and continue to indicate the true
  current state.
- **FR-017**: The create dialog MUST offer a **bootable** toggle for formatted
  contents. When enabled, the chosen OS (DOS 3.3 or the selected ProDOS
  version) is written onto the new disk so it boots in the emulator. The OS
  payload MUST be sourced through the existing consent-based asset bootstrap
  (as ROMs are) — never bundled with the app; when the payload is not
  available (no consent, offline, not yet downloaded), the toggle MUST be
  unavailable with a clear explanation, and data-only creation still works.
- **FR-018**: The create dialog MUST refuse a destination path that is
  currently mounted in any drive, naming the drive in the error; the user must
  eject first. (Overwriting a live mount's backing file is never allowed.)

### Key Entities *(include if feature involves data)*

- **Blank Disk Template** *(implemented as `BlankDiskSpec`)*: The in-memory
  description of a disk to create — format, contents (OS + version or
  unformatted), bootable flag, size (140K 5.25"), volume name/number. Produced
  by pure logic; serialized to the chosen on-disk format.
- **Boot Payload**: The OS content a bootable disk carries (DOS 3.3 system
  tracks; ProDOS system files for the selected version). Obtained via the
  consent-based asset bootstrap, cached like other downloaded assets, never
  bundled with the app.
- **Disk Format**: WOZ (bit-stream, order-agnostic, in-image write-protect
  flag), DSK (DOS 3.3 sector order), PO (ProDOS sector order). Governs
  encoding, which filesystems are representable, and where write protection
  lives.
- **Filesystem Skeleton**: The empty on-disk structures for a formatted volume —
  DOS 3.3 (VTOC + catalog track) or ProDOS (volume bitmap + volume directory) —
  or none for unformatted.
- **Write-Protect State**: A mounted image's effective protection — sourced
  from the WOZ INFO flag or the host file's read-only attribute — surfaced by
  the drive's indication and controlled by the new toggle.

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
- **SC-005**: Toggling write protection takes effect on the very next guest
  write attempt, in both directions, for every mounted format the UI offers
  the toggle on — and the indicated state never disagrees with the enforced
  state.
- **SC-006**: A disk created with the bootable option boots the chosen OS in
  the emulator to its prompt, for every OS the listbox offers.

## Assumptions

- Target machines are Apple II family with a Disk ][ controller (5.25" 140K
  media). 3.5"/800K, hard-disk, and larger volumes are out of scope for v1.
- v1 supported formats are WOZ, DSK, and PO; NIB and 2MG creation are out of
  scope (NIB/2MG may still be mountable via existing insert). Format **conversion**
  of existing images is out of scope.
- Default new-disk configuration is **WOZ + DOS 3.3 + pre-formatted**. WOZ is
  order-agnostic and represents any filesystem; with GH #89 fixed, DSK/PO are
  also reliably writable, so the choice is about robustness, not workaround.
- The create dialog is the app's own themed (Dxui) dialog. "Navigable like a
  real file-save dialog" is a functional requirement (FR-006); the exact
  widget composition (folder list, breadcrumb vs. up-button, etc.) is a design
  detail for planning.
- Pre-formatting produces a standard empty volume (default volume name/number);
  advanced volume metadata is not user-configurable in v1.
- Which ProDOS versions the contents listbox offers is a planning detail
  driven by what the asset bootstrap's catalog can source; DOS 3.3 is the
  baseline. Bootable defaults OFF so the default create has no download
  dependency.
- The write-protect toggle's surface (menu row, drive-widget affordance, or
  picker integration) is a design detail for planning; the requirement is only
  that a mounted image's protection is user-controllable and truthfully
  indicated. Casso already *reads* and *honors* write protection and surfaces
  per-drive state; this feature adds the control.

## Dependencies

- **Writable target correctness**: RESOLVED — GH #89 (`.dsk` write round-trip)
  was fixed 2026-07-09 and #88 closed 2026-07-10; all v1 formats are reliably
  writable. WOZ remains the default for robustness.
- Reuses the existing disk **mount** path (drive widget, MRU, per-machine
  persistence) rather than introducing a parallel insertion mechanism.
- Reuses the existing per-drive **write-protect indication**; the toggle drives
  the state that indication already displays.
