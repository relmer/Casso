# Feature Specification: Nibble Disk Images

**Feature Branch**: `027-nibble-images`

**Created**: 2026-08-29

**Status**: Draft

**Input**: User description: "Mount and write back `.nib` nibble disk images, split out of spec 022's User Story 2."

## Overview

Casso mounts four image types: `.dsk`, `.do`, `.po` and `.woz`. A nibble image --
`.nib`, or its less common sibling `.nb2` -- is not among them, and a folder of
them is currently a folder Casso will not open.

A nibble image is the raw GCR byte stream as read off the drive, one fixed-length
block per track, with no sector structure and no header. It is how protected disks
were archived before WOZ existed, and a considerable amount of software still
circulates in that form.

**The reason to do this is compatibility, not fidelity.** WOZ dominates nibble
images for preservation and this feature does not change that. A nibble image
records whole bytes only, so the self-sync byte information is not in the file --
and self-sync patterns are exactly what copy protection inspects. What this feature
delivers is that the images people already have will open, boot where the format
preserved enough to boot, and survive being written to. It does not deliver a
second preservation format, and the documentation must not imply that it does.

**Writing is the hard part, not reading.** Casso's mount path is a write-back
path: a mounted image is flushed on eject, on power cycle and on reset. So an
image that can be mounted will eventually be written, and by then its tracks are
live bit streams the guest has altered. A loader on its own is not a deliverable
here.

Three facts shape the work, and a reasonable reading of the existing code gets all
three wrong.

**A nibble image is written back from bytes, not from sectors.** Casso already
converts bit streams to sector images and refuses to do so when a track only
partly decodes, precisely so a damaged disk is never written over as though it
were clean. A nibble image does not take that path at all: its file format IS the
byte stream, so writing it back means re-deriving nibble bytes from the bit stream
and nothing more. Tracks that do not decode to standard sectors -- much of the
point of the format -- therefore cost nothing on the emulator's flush path. They
matter only where a command genuinely needs sectors, which is the console's
file-level commands.

**A track's bit length is fixed when the image is mounted, and the guest cannot
change it.** Guest writes flip bits in place and wrap at the end of the track, the
way a real drive writes onto a circle of fixed physical length. What the guest can
change is how many *bytes* that fixed number of bits yields, because a self-sync
byte occupies ten bit cells and still yields one byte. So a rewritten track derives
*fewer* bytes than the block holds, never more. Under-fill is the normal case after
any guest write; overflow cannot happen. The block is a fixed size, so the shortfall
has to be filled with something, and what it is filled with is a decision this spec
makes rather than an implementation detail.

**The file filter follows the loader without being told.** The interface no longer
keeps its own list of extensions; it asks the loader what it can mount. Once the
loader claims nibble images, drag-and-drop, the disk picker and the folder scan all
offer them. Restoring a second list would recreate the defect that removing it
fixed.

## Clarifications

### Session 2026-08-29

- Q: Given that 6,656 vs 6,384 bytes per track is a convention files in the wild
  break, how should a nibble image be identified? → A: The file's length decides
  the track size, and both extensions are offered. `.nib` and `.nb2` are each
  accepted at either total length.
- Q: A rewritten track derives fewer nibble bytes than the fixed-size block holds.
  What fills the remainder? → A: Pad with `$FF` self-sync bytes.
- Q: Should `disk create --type nib` and `disk init` on a nibble image exist? → A:
  Yes, full parity with the other container types.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Open a nibble image (Priority: P1)

A user drops a `.nib` file onto a drive, or picks one from the disk dialog, or
names one on the command line. It mounts and the machine boots from it.

**Why this priority**: It is the whole point of the feature, and it is the state
change a user can see. Everything else here protects it.

**Independent Test**: Mount a nibble image of a known disk and confirm it boots and
catalogs identically to the same disk as `.dsk` and as `.woz`.

**Acceptance Scenarios**:

1. **Given** a nibble image of a standard 16-sector disk, **When** the user mounts
   it, **Then** it boots and catalogs correctly.
2. **Given** a nibble image of a copy-protected disk, **When** the user mounts it,
   **Then** it boots to the extent the format preserved the protection.
3. **Given** a 223,440-byte file named `.nib`, or a 232,960-byte file named `.nb2`,
   **When** the user mounts it, **Then** it opens correctly at the track size its
   length implies, without the extension being trusted over the bytes.
4. **Given** a nibble image in a folder, **When** the user opens the disk picker or
   drags the file onto a drive widget, **Then** the file is offered and accepted by
   the same rule the loader uses, with no separate list of extensions deciding.
5. **Given** a file named `.nib` or `.nb2` that is not a usable nibble image,
   **When** the user mounts it, **Then** it is refused with a message naming what
   is wrong with that particular file, not a generic failure.
6. **Given** a mounted nibble image that the guest never wrote to, **When** it is
   ejected, **Then** the file on disk is byte-for-byte what it was.

---

### User Story 2 - Keep what the guest wrote (Priority: P1)

A user boots a nibble image, the software saves to it, and the change is still
there next time.

**Why this priority**: Not optional and not separable in the way a normal P2 would
be. The mount path is the write-back path, so shipping User Story 1 without this
ships a feature that silently discards writes or damages images on eject. It is a
second story rather than part of the first because it is independently testable and
carries the bulk of the risk.

**Independent Test**: Mount a nibble image, write to it from the guest, eject,
remount, and confirm the written data reads back and every untouched track is
unchanged.

**Acceptance Scenarios**:

1. **Given** a mounted nibble image the guest has written to, **When** it is
   flushed, **Then** the file is written back in nibble form and the written data
   reads back on remount.
2. **Given** a mounted nibble image the guest has written to on one track, **When**
   it is flushed, **Then** every other track in the file is unchanged.
3. **Given** a mounted nibble image the guest has not written to, **When** a flush
   is triggered by eject, power cycle or reset, **Then** no write to the file
   occurs at all.
4. **Given** a rewritten track whose derived bytes do not fill the fixed block,
   **When** it is written back, **Then** the remainder is filled with self-sync
   bytes and the track reads back as a valid stream on the next mount.
5. **Given** a rewritten track, **When** it is written back, **Then** the padding
   falls in gap space and interrupts no address or data field.
6. **Given** a disk written, ejected, remounted and written again, **When** the
   cycle is repeated, **Then** the volume's contents stay readable and the disk
   does not degrade with each pass.
7. **Given** a mounted nibble image whose backing file cannot be written, **When**
   a flush is attempted, **Then** the user is told the writes could not be saved
   and what became of them, rather than the loss passing unreported.

---

### User Story 3 - Work with a nibble image from the console (Priority: P2)

A user runs the `disk` commands against a nibble image: creates one, formats it,
lists its catalog, gets and puts files, reads and writes sectors.

**Why this priority**: The console commands are the reason a user reaches for
Casso rather than an emulator, and a format the interface mounts but the tool
refuses is an inconsistency users will find immediately. It is P2 because mounting
is the capability people are missing today and this is a second surface on top of
it.

**Independent Test**: Run each `disk` command against a nibble image of a DOS 3.3
disk and against the same disk as `.dsk`, and confirm the results agree.

**Acceptance Scenarios**:

1. **Given** a nibble image of a DOS 3.3 or ProDOS volume, **When** the user runs
   `list`, `get`, `put`, `delete`, `boot`, `sectorread` or `sectorwrite`, **Then**
   each behaves as it does on the equivalent `.dsk`.
2. **Given** a name with a nibble extension, **When** the user runs `create`,
   **Then** a new nibble image is written, formatted as asked, and is immediately
   usable -- mountable, bootable if requested, and accepted by the other commands.
3. **Given** an existing nibble image, **When** the user runs `init`, **Then** it
   is reformatted in place and its container is unchanged.
4. **Given** a nibble image whose tracks do not all decode to standard sectors,
   **When** the user runs a command that needs sectors, **Then** it is refused with
   a reason naming the surface, and no partial write occurs.

---

### User Story 4 - Know what a nibble image can and cannot do (Priority: P3)

A user reading the documentation can tell, before trying, that nibble images will
open their collection and that this is not the format to preserve a disk into.

**Why this priority**: The claim that nibble images could be dropped onto a drive
was in the documentation once, was false, and was removed. Making it true again
without also being honest about what the format loses would restore the same
problem in a subtler form.

**Independent Test**: Read the format documentation and the drag-and-drop
documentation and confirm each supported format states what can be read, written
and modified, with the self-sync limitation stated for nibble images.

**Acceptance Scenarios**:

1. **Given** the documentation, **When** a user looks up nibble images, **Then** it
   states that they mount, boot and write back, and that self-sync information is
   not carried by the format so protection that depends on it will not survive.
2. **Given** the documentation, **When** a user is deciding what to archive a disk
   into, **Then** it points at WOZ rather than at a nibble image.

---

### Edge Cases

- **A file whose length is neither of the two accepted totals.** It is not a nibble
  image, whatever its name, and MUST be refused saying so with its actual size --
  the same treatment a `.dsk` of the wrong size gets today.
- **A file of an accepted length whose content is not a GCR byte stream.** A
  renamed or corrupt file can be exactly the right size. The refusal MUST
  distinguish "the length is wrong" from "the length is right and the contents are
  not nibbles".
- **Bytes with the high bit clear.** A real drive can never present one, but they
  appear in nibble images, usually in gaps. They MUST NOT cause a refusal; the
  image loads and the drive reads whatever the stream contains.
- **A track holding no byte with the high bit set at all.** Deriving nibbles from a
  bit stream looks for that bit, so a track of zeros has no first nibble. The
  write-back MUST terminate on such a track rather than searching forever, and MUST
  produce a defined result for it.
- **A rewritten track that derives far fewer bytes than the block holds.** A track
  the guest reformatted with generous self-sync gaps can fall well short. The
  padding MUST still be placed where it interrupts nothing, however large it is.
- **Where the derivation begins.** The stored stream's first bit is an arbitrary
  rotational position and may sit inside a field. The write-back MUST choose an
  origin such that the padded tail lands in gap space, rather than beginning at bit
  zero and splitting whatever field happens to be there.
- **Half and quarter tracks.** The format has no track map and no way to express
  them. An image that needs them is an image this format could not have preserved,
  and nothing about mounting it should pretend otherwise.
- **Write protection.** A nibble image carries no write-protect flag of its own, so
  only the host file's state and the user's own setting can protect it. The
  interface MUST attribute the protection correctly rather than implying the image
  asked for it.

## Requirements *(mandatory)*

### Functional Requirements

#### Mounting

- **FR-001**: The system MUST mount nibble images and boot from them.
- **FR-002**: The system MUST accept nibble images wherever disk images are chosen
  -- drag-and-drop, the disk picker, the folder scan and the command line -- and
  MUST do so by asking the loader which extensions it handles, never from a
  separate list of extensions maintained alongside it.
- **FR-003**: The system MUST offer both the `.nib` and `.nb2` extensions, and MUST
  determine a nibble image's track size from the file's own length rather than from
  its extension: 232,960 bytes is 35 tracks of 6,656, and 223,440 bytes is 35
  tracks of 6,384. Either length MUST be accepted under either extension, because
  images of one track size circulate under the other's name.
- **FR-004**: The system MUST refuse a file it cannot read as a nibble image with a
  reason specific to that file, distinguishing at minimum a wrong length from
  contents that are not a nibble stream, and MUST report the length it found and
  the lengths it accepts.
- **FR-005**: Mounting a nibble image MUST NOT assert or report a coding error for
  any content the file may contain. A malformed image is user input and gets a
  verdict.

#### Writing back

- **FR-006**: The system MUST write a mounted nibble image back to its own format
  when it is flushed, deriving the nibble bytes from the live bit stream as a drive
  would read them.
- **FR-007**: A flush MUST leave every track the guest did not write byte-identical
  to what the file held.
- **FR-008**: A mounted nibble image the guest has not written to MUST NOT be
  rewritten at all.
- **FR-009**: The write-back MUST NOT require a track to decode as standard
  sectors, and MUST NOT be refused because a track does not.
- **FR-010**: When a rewritten track's derived bytes do not fill the fixed-size
  block, the system MUST pad the remainder with `$FF` self-sync bytes. Every byte
  the file carries therefore has its high bit set, so the padding reads back as an
  ordinary gap rather than as a stretch the drive cannot assemble a nibble from.
- **FR-011**: The write-back MUST place its padding where it interrupts no address
  field and no data field, choosing the point at which it begins deriving the track
  accordingly rather than starting at a fixed offset.
- **FR-012**: A flush that could not persist the guest's writes MUST report the
  loss to the user, naming the image and what became of the writes.
- **FR-013**: Repeated write, eject and remount cycles MUST NOT progressively
  degrade an image: a volume written and reopened many times MUST stay readable.

#### Console commands

- **FR-014**: All nine `disk` commands MUST accept nibble images. `list`, `get`,
  `put`, `delete`, `boot`, `sectorread` and `sectorwrite` MUST behave as they do on
  the equivalent sector image; `create` MUST write a new nibble image; and `init`
  MUST reformat an existing one in place, leaving its container unchanged.
- **FR-015**: `create` MUST accept the nibble container by name in its type option
  alongside the existing types, and MUST continue to refuse an unrecognized type
  naming the ones that exist.
- **FR-016**: A `disk` command that needs standard sectors and meets a nibble image
  whose tracks do not supply them MUST refuse, name the surface as the reason, and
  write nothing.

#### Consistency and documentation

- **FR-017**: Adding nibble support MUST NOT change behavior for the four formats
  supported today, verified against the existing test corpus.
- **FR-018**: The documentation MUST state, for nibble images, what can be read,
  written and modified, and MUST state that self-sync information is not carried by
  the format so protection depending on it does not survive.
- **FR-019**: The documentation MUST point a user archiving a disk at WOZ rather
  than at a nibble image.

### Key Entities

- **Nibble Image**: A headerless file of 35 fixed-length per-track blocks holding
  the GCR byte stream a drive would read, with no sector structure, no track map,
  and no metadata of any kind. Its length is the only thing that identifies it, and
  the only thing that says how long a track is.
- **Nibble Track**: One track's block within that file -- a fixed number of bytes,
  every one of which the drive presents as read. Fixed length is the property that
  constrains writing back, since the number of bytes a live track yields is not
  fixed.
- **Derived Nibble Stream**: The bytes recovered from a live track bit stream by the
  same rule the drive's shift register uses. This is what a write-back produces,
  where the format's loss of self-sync information happens, and what the padding
  extends to the block's fixed size.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user with a folder of nibble images can open them in Casso without
  converting anything first, including images whose extension disagrees with their
  track size.
- **SC-002**: A nibble image of a standard disk boots and catalogs to the same
  result as the same disk in every other format Casso supports.
- **SC-003**: Mounting a nibble image and ejecting it without writing to it leaves
  the file byte-for-byte identical, for every image tested.
- **SC-004**: A guest write to a nibble image survives an eject and remount, and
  every track the guest did not touch is byte-identical afterwards.
- **SC-005**: A volume that is written, ejected and remounted repeatedly stays
  readable across every cycle, with no accumulating loss.
- **SC-006**: No nibble image can be written back in a way that loses data without
  the user being told.
- **SC-007**: Every nibble image Casso refuses produces a message identifying what
  is wrong with that file, with no unexplained failures.
- **SC-008**: A user can determine from the documentation alone what Casso will do
  with a nibble image, including what the format cannot preserve, without trial and
  error.
- **SC-009**: The four formats supported today behave exactly as they did, verified
  by the existing test suite passing unchanged.

## Assumptions

- Nibble images hold 35 fixed-length tracks and nothing else. There is no header,
  no metadata, no track map and no write-protect flag, so nothing about the file can
  be honored beyond its bytes.
- The two track sizes in circulation are 6,656 and 6,384 bytes, giving total lengths
  of 232,960 and 223,440 bytes. By convention the first is `.nib` and the second
  `.nb2`, but that convention is not reliably followed in the wild, which is why
  FR-003 makes the length authoritative. Neither total is likely to collide with
  another format.
- A track's bit length is fixed when the image is mounted and guest writes cannot
  change it -- they write in place and wrap. This is why the write-back faces
  under-fill and never overflow, and it is a property of the existing track model
  rather than something this feature introduces.
- The existing bit-stream track model, the mount and flush lifecycle, the mount
  refusal reporting, and the console's `disk` commands all exist and are reused.
  This feature adds a container to them; it does not reimplement any of them.
- The defect where a partly-decoded track was written back as a clean save is
  already fixed, and the nibble write-back does not pass through that code at all.
  This feature therefore neither inherits nor re-exposes it. Where the console's
  file-level commands do decode sectors, they meet the existing refusal, which is
  the correct behavior and is left alone.
- A guest write to a track beyond the 35 the format carries goes nowhere, exactly
  as it does for the 35-track sector formats today. That behavior is shared with
  `.dsk`, `.do` and `.po`, is not introduced here, and is out of scope for this
  feature.
- Copy protection that inspects self-sync patterns will not work from a nibble
  image, because the information is not in the file. This is a property of the
  format and not a defect to be fixed later.
- Half-track and quarter-track formatting cannot be represented and is out of
  scope for this container.
- 2MG containers wrapping nibble data remain part of spec 022 and are not delivered
  here, even though they carry the same track data. The container's header is 022's
  subject.
- Delivering this feature satisfies spec 022's User Story 2 and its FR-003, which
  are marked delivered here rather than remaining duplicated, and satisfies spec
  007's FR-022 and SC-004, which have never been satisfiable.
