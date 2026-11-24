# Feature Specification: WOZ Format Stamp

**Feature Branch**: `052-woz-format-stamp`

**Created**: 2026-11-23

**Status**: Draft

**Input**: User description: "Record each track's disk format facts inside the WOZ files Casso writes, so later reads of the image use the exact marks instead of guessing. Must ship in the initial public release so every image captured from then on has it."

## Overview

A WOZ image contains a disk's raw bits, but not what those bits mean. Every later
read of the image has to work out again which address and data marks each track
uses, how its sectors are numbered and whether its checksums are standard.
Casso does that today with a fixed list of known variants (D4 AA 96 for Ultima
I, sector numbers $11 to $20 for Ultima V, sixteen prolog variants for the 1981
Ultima program disk). Each new protection scheme adds to that list, and every
Casso matches every variant on the list on every disk.

The format stamp is a record, inside the WOZ file, of each track's format as
Casso found it when the image was captured or inspected. A later read uses the
stamped format for that track first and checks it against the sector checksums.
The bits remain the authority: a wrong or stale stamp makes a read slower,
never wrong.

The stamp has to be present from the first public release. An image captured
without one can be stamped later, but the facts known only during a capture
(which reads disagreed, which sectors came from another read, how the tracks
lined up) are lost if they are not written down at that moment.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A captured image records its format (Priority: P1)

The owner captures a copy-protected disk over CassoLink. The resulting WOZ file
contains, for every track, the encoding, the address and data marks found, the
sector numbers, the volume, the checksum behavior, how many sectors decoded
cleanly, and the capture facts (weak and unstable positions, replaced sectors,
half and quarter track streams, revolution timing, whether the tracks were
placed relative to track 0).

**Why this priority**: capture is the one moment some of these facts exist.
Everything else in this feature can be added to an image later; this cannot.

**Independent Test**: capture a synthetic disk through the capture pipeline
with tracks in several formats, then read the stamp out of the resulting bytes
and compare it with the formats the synthetic tracks were built with.

**Acceptance Scenarios**:

1. **Given** a capture of a standard DOS 3.3 disk, **When** the WOZ is written, **Then** its stamp lists 35 tracks as 16-sector with standard marks, 16 good sectors each and the volume read from the address fields.
2. **Given** a capture of a disk whose tracks use D4 AA 96 address marks, **When** the WOZ is written, **Then** each such track's stamp lists D4 AA 96 as its address prolog.
3. **Given** a capture of a disk where each track uses a different prolog variant, **When** the WOZ is written, **Then** each track's entry lists that track's variant, not a disk-wide one.
4. **Given** a track that uses more than one address prolog, **When** the WOZ is written, **Then** that track's entry lists every prolog found.
5. **Given** a capture in which some positions were weak, unstable or had sectors replaced from another read, **When** the WOZ is written, **Then** the stamp lists those positions and sectors.
6. **Given** a capture of a disk with a half track stream, **When** the WOZ is written, **Then** the stamp lists the half track position and its format.

---

### User Story 2 - Reads of a stamped image use the stamp (Priority: P1)

The owner opens a stamped image of a protected disk in Casso Explorer, lists
its catalog, copies a file off it or opens it in a saved game viewer. CassoCli
`disk` commands read the same image. Each track is decoded with its stamped
marks first. When the stamp does not match the bits (it is stale, or another
tool changed the file), the read falls back to detection and succeeds
wherever detection would have.

**Why this priority**: this is the reason to record the stamp. Without a
consumer, the stamp is just text.

**Independent Test**: build an image in memory whose tracks use marks that
are NOT on the built-in detection list, stamp it with those marks, and read a
file from it. Then corrupt the stamp and confirm the read falls back.

**Acceptance Scenarios**:

1. **Given** a stamped image whose tracks use marks outside the built-in list, **When** a file is read from it, **Then** the read succeeds using the stamped marks.
2. **Given** the same image with no stamp, **When** the file is read, **Then** the read fails as it does today, with the same message.
3. **Given** a stamp whose marks do not match a track's bits, **When** that track is read, **Then** the read falls back to detection, and the result is identical to reading the image with no stamp.
4. **Given** a stamp whose JSON cannot be parsed, or whose version is newer than Casso understands, **When** the image is opened, **Then** the image opens and reads exactly as an unstamped image does, and the file contains the stamp byte for byte.
5. **Given** a stamped standard disk, **When** a file is read, **Then** the result is identical to reading the same image without a stamp.

---

### User Story 3 - Stamp an existing image from the disk inspector (Priority: P2)

The owner has WOZ images from Applesauce, from another archive, or from Casso
captures made before this feature. In the disk inspector, a command analyzes
every track and writes a stamp into the image, or refreshes the stamp that is
already there. If the owner has set custom decode settings for some tracks, the
stamp lists the marks those settings found.

**Why this priority**: it brings the benefit of User Story 2 to every image
already in the owner's collection.

**Independent Test**: open an unstamped image in the inspector, run the
command, save, reopen and confirm the stamp is present and matches the
inspector's analysis of each track.

**Acceptance Scenarios**:

1. **Given** an unstamped WOZ open in the inspector, **When** the owner runs the stamp command, **Then** the image gains a stamp whose source is the inspector, and every other byte of the file outside the stamp chunk and the header CRC is identical to the original.
2. **Given** a stamped image, **When** the owner runs the command again, **Then** the per-track entries are replaced with the current analysis and the stamp contains the capture facts from the original stamp.
3. **Given** an image open read-only or write-protected, **When** the owner runs the command, **Then** the command is unavailable, with the reason in its tooltip.
4. **Given** a .dsk, .po or .nib image, **Then** the command is not offered.

---

### User Story 4 - The inspector uses the stamp (Priority: P2)

The owner opens a stamped image of a protected disk in the disk inspector.
Tracks whose format matches the stamp show their class (16-sector,
13-sector) and their sectors as good, instead of Nonstandard, without the owner
adding custom marks in Decode settings. Decode settings shows the stamped marks
as the starting point, and the owner can change them.

**Why this priority**: the inspector already has the concept of custom marks;
seeding them from the stamp removes manual setup for every protected image.

**Independent Test**: open a stamped image whose tracks use D4 AA 96 and
confirm the track list shows those tracks as 13-sector or 16-sector, not
Nonstandard, and that Decode settings lists the stamped marks.

**Acceptance Scenarios**:

1. **Given** a stamped image with nonstandard marks, **When** it opens in the inspector, **Then** each stamped track is analyzed with its stamped marks and its class reflects them.
2. **Given** the same image, **When** the owner opens Decode settings, **Then** the stamped marks appear as ranges labeled as coming from the stamp.
3. **Given** the owner turns the stamped marks off in Decode settings, **Then** the tracks are analyzed with the standard marks, and show as Nonstandard as they do today.
4. **Given** the image details view, **Then** it shows whether the image has a stamp, its source, creator and date, and the capture facts it contains.

---

### User Story 5 - Writes keep the stamp accurate (Priority: P1)

The guest writes to a stamped disk, or the owner copies a file onto it, replaces
a file or formats it from Casso Explorer or CassoCli. When the image is saved,
each track that changed has its stamp entry analyzed again from its new bits.
No stamp entry is ever about bits that are no longer on the track.

**Why this priority**: a stamp that drifts from the bits is worse than no stamp.
The read fallback (User Story 2) prevents a stale stamp from corrupting a read,
but the stamp would mislead anyone who reads it.

**Independent Test**: stamp an image in memory, write a sector on one track
through the emulated drive, save, and confirm that only that track's entry
changed and that it matches the new analysis.

**Acceptance Scenarios**:

1. **Given** a stamped image, **When** the guest writes one sector and the image is saved, **Then** that track's entry is analyzed again and the other entries are identical to the original.
2. **Given** a stamped image, **When** a file is put or replaced, **Then** every track the write touched is analyzed again before the file is saved.
3. **Given** a stamped image, **When** the guest reformats a track in a different format, **Then** that track's entry lists the new format.
4. **Given** a rewritten track, **Then** the stamp's capture facts for that track (weak, replaced, unstable) are removed, since they are about bits that are gone, and the track is marked as changed after capture.
5. **Given** a stamped image whose stamp version is newer than Casso understands, **When** Casso writes a track and saves, **Then** the unknown stamp is removed from the file, since Casso cannot update it, and the save succeeds.

---

### User Story 6 - Print the stamp from the command line (Priority: P3)

The owner, or a script, runs a CassoCli command on a WOZ file and gets its
stamp, either as a readable table or as the raw JSON for another tool to
consume.

**Why this priority**: useful for scripts and for checking captures, but every
other story works without it.

**Independent Test**: run the command on a stamped image built by the tests and
compare the output with a known table and JSON.

**Acceptance Scenarios**:

1. **Given** a stamped WOZ, **When** the owner runs the command, **Then** it prints one row per track with its encoding, marks, sector count and volume, followed by the disk-level facts.
2. **Given** the JSON option, **Then** it prints the stamp exactly as stored.
3. **Given** an unstamped WOZ, **Then** it prints that the image has no stamp and exits with a distinct status.
4. **Given** a file that is not a WOZ, **Then** it prints an error and exits with a failure status.

---

### Edge Cases

- A track with no fields at all (unformatted or blank): its entry lists it as unformatted, with no marks.
- A track that has both 13-sector and 16-sector fields: its entry lists both encodings and both sets of marks.
- Address marks with an "any" nibble in custom decode settings: the stamp lists the nibbles actually found, never the wildcard.
- A disk with more than one volume number across tracks: each track entry lists the volume found on that track.
- Sector numbers outside 0 to 15 (Ultima V's $11 to $20): the entry lists the sector numbers found.
- An oversized stamp chunk: the stamp is bounded so a damaged or hostile file cannot make Casso allocate without limit; an oversized stamp is ignored as unreadable.
- Two stamp chunks in one file: Casso uses the first and writes the rest back byte for byte.
- A 3.5" WOZ: the stamp is not written or read for 3.5" disks in this feature.
- A flux track (WOZ 2.1): its entry is analyzed from the flux, as the inspector does today.
- A WOZ version 1 image: stamping it in the inspector saves it as WOZ 2, as any other write does.
- An image whose file changes on disk while open: the stamp is reread with the rest of the file, by the existing external-change handling.

## Requirements *(mandatory)*

### Functional Requirements

**Format**

- **FR-001**: The stamp MUST be stored in a WOZ chunk with the id `CSSO`, so readers that do not recognize it skip it, as the WOZ reference requires.
- **FR-002**: The chunk payload MUST be UTF-8 JSON with a top-level format version number. A reader MUST ignore a stamp whose major version is newer than the one it understands and MUST accept fields it does not recognize within a version it does understand.
- **FR-003**: The format MUST be documented in the repository with a schema description and an example, so that other tools can read and write it without reading Casso's source.
- **FR-004**: Each track entry MUST record: its quarter-track position; its encoding (16-sector, 13-sector, both, unformatted or nonstandard); the address prolog, address epilog, data prolog and data epilog nibbles actually found, as a list when several were found; the sector numbers found; the volume numbers found; the address and data checksum behavior (standard, not checked or failing); the number of sectors found and the number that decoded with good checksums; whether every sector found decoded with good checksums; and whether the track changed after it was captured.
- **FR-005**: The stamp MUST record at disk level: the format version; the program and version that wrote it; the date and time it was written (UTC); its source (capture or inspector); and the decode settings that produced the analysis, when they were not standard.
- **FR-006**: A stamp written by a capture MUST also record: weak positions; unstable positions; sectors replaced from another read, as position and sector; which positions between whole tracks were read and which have a separate stream; the drive's revolution time when it was measured; and whether the tracks were placed relative to track 0.
- **FR-007**: The format MUST reserve a list of protection notes, at disk level and per track, each with a kind and its parameters, for facts such as track alignment, nibble count and weak bit regions. The list MAY be empty. Readers MUST write back note kinds they do not recognize.
- **FR-008**: The stamp MUST NOT exceed 1 MB. A larger chunk MUST be treated as unreadable.

**Producers**

- **FR-009**: Every CassoLink capture of a 5.25" disk MUST write a stamp into the WOZ it produces, including a WOZ rebuilt from saved reads.
- **FR-010**: The disk inspector MUST offer a command that stamps the open WOZ image, or refreshes its stamp, from its current analysis.
- **FR-011**: Refreshing a stamp MUST replace the per-track format entries and MUST retain the capture facts of tracks that have not changed since capture.
- **FR-012**: Stamping or refreshing MUST change only the stamp chunk and the header CRC. Every other byte of the file MUST be identical to the original, except where the existing WOZ 1 to WOZ 2 save already changes it.
- **FR-013**: The capture and the inspector MUST use the same track analysis, so the same bits always produce the same entry.

**Consumers**

- **FR-014**: Every host-side read of a WOZ image (Casso Explorer, CassoCli disk commands, the saved game viewers, the file system readers) MUST decode each track with the marks its stamp entry lists before using detection.
- **FR-015**: A sector decoded with stamped marks MUST pass its address and data checksums to be accepted, unless the stamp lists that track's checksums as not checked.
- **FR-016**: When a track's stamped marks do not produce the sectors that detection would produce, the read MUST fall back to detection for that track. A read of a stamped image MUST never return less data, or different data, than a read of the same image without the stamp.
- **FR-017**: The emulated drive MUST NOT use the stamp. The guest reads the raw bits, as it does today.
- **FR-018**: The disk inspector MUST seed its decode settings from the stamp when it opens a stamped image, and MUST label those ranges as coming from the stamp. The owner MUST be able to turn them off.
- **FR-019**: The image details view MUST show whether the image has a stamp and, when it does, its source, creator, date and capture facts.
- **FR-020**: CassoCli MUST provide a command that prints a WOZ file's stamp as a table, or as the stored JSON with an option, and that exits with a distinct status for an image with no stamp.

**Writes**

- **FR-021**: When Casso saves a stamped WOZ, every track whose bits changed since the stamp was written MUST have its entry analyzed again from its new bits, and its capture facts MUST be removed.
- **FR-022**: A track entry analyzed again after a write MUST be marked as changed after capture.
- **FR-023**: When Casso saves a stamped WOZ whose stamp it cannot parse, or whose version it does not understand, and any track changed, the stamp MUST be removed from the file.
- **FR-024**: Saving a stamped WOZ whose tracks did not change MUST write the stamp back byte for byte.

**Scope**

- **FR-025**: The stamp MUST be written and read only for 5.25" WOZ images. Other formats (.dsk, .do, .po, .nib, .2mg, .hdv) and 3.5" WOZ images MUST open, read and save exactly as they do today.
- **FR-026**: The built-in detection list MUST stay in place as the fallback for unstamped images. Narrowing it is out of scope.

### Key Entities

- **Format stamp**: the disk-level record in one WOZ file: version, writer, date, source, decode settings, capture facts, protection notes and track entries.
- **Track entry**: the format of one quarter-track position as analyzed: encoding, marks found, sector and volume numbers, checksum behavior, sector counts, whether it was verified and whether it changed after capture.
- **Capture facts**: facts about the reads (weak, unstable, replaced, streams, timing, synchronization) that are not recoverable from the bits alone.
- **Protection note**: a typed, open-ended fact about a copy protection, at disk or track level.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every WOZ produced by a capture from the first public release on contains a stamp.
- **SC-002**: A file can be read from a stamped image whose tracks use marks not on the built-in detection list, where the same image without the stamp cannot be read.
- **SC-003**: For every image in the scenario test set, a read with the stamp returns exactly the same files and bytes as a read without it.
- **SC-004**: For every image in the scenario test set, a stamp with deliberately wrong marks on every track returns exactly the same files and bytes as a read with no stamp.
- **SC-005**: Opening, reading and saving an image with an unreadable or future-version stamp succeeds in every case.
- **SC-006**: A program written from the documented format alone can list each track's marks from a stamped image without reading Casso's source.
- **SC-007**: Stamping an image in the inspector doesn't take longer than the inspector's whole-disk analysis plus one save.
- **SC-008**: After any write and save, re-analyzing every track of the saved image produces the entries in its stamp.

## Assumptions

- The owner's first public release is close, so this feature merges into 033-casso-explorer, which contains the capture and inspector code it changes.
- WozLoader writes unknown chunks back on save, so a stamp in a file survives saves by this and older Casso builds; older builds do not update it, which FR-016 makes safe.
- The stamp is JSON so that it is readable by a person and parseable by any tool; the size limit in FR-008 is far above the size of a 35-track stamp.
- Casso does not depend on a third-party JSON library; the plan specifies how the stamp is written and parsed within the constitution's dependency rules.
- Applesauce and other tools will skip the chunk, as the WOZ reference requires for unknown chunk ids.
- 3.5" disks and narrowing the built-in detection list are left for later features.
