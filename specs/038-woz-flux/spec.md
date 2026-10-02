# Feature Specification: WOZ Flux Track Support

**Feature Branch**: `038-woz-flux`

**Created**: 2026-10-02

**Status**: Draft

**Input**: GH #159, "WOZ disk images with type FLUX don't boot or give error"
(https://github.com/relmer/Casso/issues/159). A user reported that
`00_Bandits.woz` hangs at boot in Casso 1.29.0 on an Apple //e Enhanced.
Virtual ][ reports that the FLUX type is not supported; AppleEm (web-a2e)
boots it.

**GH ref**: GH #159 -- used in the CHANGELOG entry and in the commit and merge
subjects for this feature.

## Background

A WOZ 2.1 image can store a track two ways. A bit track is a loop of 4 µs bit
cells, each either a flux transition (1) or none (0), so every cell is the same
length. A flux track is a timeline: each byte is the time since the previous
transition, in 125 ns ticks, so it keeps how fast or slow each stretch of the
track was written. Both kinds can sit in one file. The TMAP chunk maps quarter
tracks to bit tracks, the FLUX chunk maps quarter tracks to flux tracks, and
both kinds of track data live in the TRKS chunk.

The reported disk, Sirius's *Bandits*, holds 15 bit tracks (track 0 and tracks
21-34) and 19 flux tracks (half tracks 1.5 through 19.5). Its copy protection
writes parts of each flux track with cells about 3.7 µs long and parts with
cells about 4.1 µs long, and its loader times its reads to tell them apart.
Casso ignores the FLUX chunk today, so those 19 tracks read as unformatted and
the boot code searches forever. Converting flux to bit cells at load would
round every cell to the same length and erase exactly what the loader checks,
so flux tracks have to be played back by time.

## Scope

Applies to every machine with a Disk II controller (Apple ][, ][+, //e and
//c) and to 5.25" WOZ 2.1 images. 3.5" flux tracks are out of scope. Mounting
images with damaged tracks (User Story 3) also covers bit tracks in WOZ 1 and
WOZ 2 images. Flux support extends to the sector-level tools (the `disk`
command, Casso Explorer and salvage), not only the drive.

A disk inspector that draws flux timing (in Casso Explorer and in Casso,
modeled on AppleEm's Disk Inspector) is a separate future spec. This spec only
has to leave the flux timing available for it.

## Clarifications

### Session 2026-10-02

- Q: How should automated tests check that *Bandits* boots, given the disk
  can't be checked in yet? → A: *Bandits* is used only for local testing while
  this spec is in development. No test that depends on it merges to master;
  the merged tests use made-up flux images only.
- Q: When a WOZ image has damaged flux data, should Casso refuse to mount it,
  or mount it and warn? → A: Mount it read-only, as 1.17.0 does for a
  checksum-damaged disk; damaged tracks read as unformatted and the report on
  insert lists them. This applies to damaged bit tracks too, which today
  refuse the whole mount. A FLUX or TMAP map that cannot be trusted at all
  still refuses the mount.
- Q: When must Casso honor a FLUX chunk? → A: Whenever the chunk is at least
  160 bytes, whatever INFO's version and flux fields say. Casso finds the
  chunk by walking the chunk list, so the INFO fields are not needed to read
  it. Only a FLUX chunk shorter than 160 bytes refuses the mount. Casso still
  writes the INFO flux fields correctly on save.
- Q: When Casso writes to a flux track, how long should each written cell be?
  → A: 31.29 ticks, the controller's own cell of 4 CPU cycles, not the
  format's nominal 32 ticks (4 µs). This is the timing an emulated Disk II
  produces, so a stretch Casso wrote reads back exactly.
- Q: What do the sector-level tools (the `disk` command, Casso Explorer,
  salvage) do with a flux disk? → A: They read and write it. A flux track is
  decoded to bits at the controller's timing to find its sectors. A sector
  write replaces only that sector's data field, spliced into the flux at the
  same 31.29-tick cell the drive writes; the rest of the track keeps its
  recorded timing.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Boot a disk with flux tracks (Priority: P1)

A user inserts a WOZ image that contains flux tracks, such as *Bandits*, and
boots it. The disk boots and the program runs as it does on real hardware,
including copy protection that checks how long bit cells are.

**Why this priority**: This is the reported bug. Without it, any disk with
flux tracks is unusable.

**Independent Test**: Boot `00_Bandits.woz` on an Apple //e Enhanced and reach
the title screen; separately, load a made-up image whose flux tracks mix fast
and slow cells and confirm the drive reads the cell lengths the image holds.

**Acceptance Scenarios**:

1. **Given** a WOZ image with flux tracks inserted in drive 1, **When** the
   machine boots, **Then** the disk boots past every flux track to the
   program's title screen.
2. **Given** a quarter track mapped in the FLUX chunk, **When** the head is on
   it, **Then** the drive reads flux transitions at the times the image
   records, not at fixed cell boundaries.
3. **Given** a quarter track mapped in both TMAP and FLUX in a badly made file,
   **When** the head is on it, **Then** the flux track is used.
4. **Given** the head steps between a bit track and a flux track, **When** it
   arrives, **Then** it reads from the same angular position round the disk.

---

### User Story 2 - Keep flux tracks intact on save (Priority: P2)

A user boots a flux disk whose program writes to the disk (a high score, a
saved game). When Casso writes the image back, the flux tracks are still flux
tracks, and the parts nobody wrote still hold their original timing.

**Why this priority**: Without it, the first save either drops the flux
tracks or leaves a stale FLUX chunk that no longer matches the track, and the
disk stops booting.

**Independent Test**: Load an image with flux tracks, write to one flux track
through the drive, save, reload, and confirm the unwritten flux data is
byte-for-byte identical and the written stretch reads back as written.

**Acceptance Scenarios**:

1. **Given** a flux image that was never written to, **When** Casso saves it,
   **Then** the FLUX chunk and every flux track are byte-for-byte unchanged.
2. **Given** the drive wrote part of a flux track, **When** Casso saves, **Then**
   the track stays in the FLUX map, the rewritten stretch is stored as flux at
   the controller's cell timing (31.29 ticks), and the rest keeps its original
   timing.
3. **Given** a rewritten flux track grew larger, **When** Casso saves, **Then**
   the TRKS entry and INFO's largest-flux-track field reflect its new size.
4. **Given** a saved flux image, **When** it is opened in Casso again, **Then**
   it boots as it did before the save.

---

### User Story 3 - Open a WOZ image with damaged tracks (Priority: P3)

A user inserts a WOZ image in which some tracks, flux or bit, are damaged.
Casso mounts it read-only, as it already does for a disk whose checksum does
not match, and reports which tracks are damaged when the disk is inserted. The
undamaged tracks read normally, so a program that never reads a damaged track
still runs, and one that does hang is no longer a mystery.

**Why this priority**: The reporter's minimum expectation was "either the disk
boots or an error". Damaged track data should never look like a blank track
with no explanation, and a damaged preservation dump should still open.

**Independent Test**: Mount images with a truncated flux track, a FLUX entry
pointing past the end of the file, a TRKS bit-track entry pointing past the end
of the file, and a FLUX chunk too short for its map. Confirm the track-level
cases mount read-only with a report that lists the damaged tracks, and the
short FLUX chunk is refused.

**Acceptance Scenarios**:

1. **Given** a WOZ image whose FLUX or TMAP entry refers to track data outside
   the file, or whose track data is truncated, **When** it is mounted, **Then**
   it mounts read-only, the damaged tracks read as unformatted, and the report
   on insert lists them.
2. **Given** such a disk, **When** the user opens salvage, **Then** salvage
   works as it does for a checksum-damaged disk, and the report says a
   salvaged copy keeps sector data but not flux timing or copy protection.
3. **Given** a FLUX chunk too short for its map, **When** it is mounted,
   **Then** Casso refuses the mount and reports a malformed WOZ image.
4. **Given** any of these reports, **When** it is shown, **Then** it follows
   Casso's error message format (a short label, then complete sentences).

### Edge Cases

- A flux track with no transitions at all: the drive reads random noise, as it
  does on an unformatted bit track.
- A gap between transitions longer than the drive's read circuit tolerates:
  the drive produces random bits for the gap, as it does for long runs of zero
  bits on a bit track.
- A gap encoded across several 255 bytes, longer than one cell by a large
  margin: it decodes to one long gap, not several short ones.
- An image whose INFO version is below 3, or whose INFO flux fields are zero
  or wrong, but which has a FLUX chunk of at least 160 bytes: the FLUX chunk
  is honored.
- Write-protected flux disk: writes are blocked and the image is never
  re-encoded.
- Quarter tracks a flux half track bleeds into: the drive picks up the nearest
  mapped track the same way it does for bit tracks.
- A disk is flushed while the drive is in the middle of a write to a flux
  track: the write so far is spliced in before the image is saved.
- Bit-only WOZ, NIB, DSK, DO and PO images behave exactly as before.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Casso MUST read the FLUX chunk from WOZ images, finding it by
  the chunk walk. INFO's version and flux fields MUST NOT decide whether it is
  read.
- **FR-002**: A quarter track mapped in FLUX MUST be played as flux; FLUX
  takes precedence over TMAP for the same quarter track.
- **FR-003**: The drive MUST deliver each flux transition on the read-circuit
  step where its recorded time falls, using the drive's real clock (a nominal
  cell is 31.29 flux ticks, not 32), so cell lengths that differ from nominal
  are preserved.
- **FR-004**: Moving the head between bit tracks and flux tracks MUST keep the
  head's angular position.
- **FR-005**: Long gaps without transitions on a flux track MUST produce the
  same random-bit behavior as long zero runs on a bit track.
- **FR-006**: Writing to a flux track MUST keep it a flux track; the written
  stretch is stored at the controller's cell timing (31.29 ticks) and the rest
  of the track keeps its recorded timing.
- **FR-007**: Saving an image MUST write unwritten flux tracks back
  byte-for-byte, and MUST update the TRKS entries and INFO's flux fields for
  any rewritten flux track.
- **FR-008**: A WOZ image with damaged tracks, flux or bit (a track entry
  referring to data outside the file, or truncated track data), MUST mount
  read-only with those tracks unformatted, and the report on insert MUST list
  the damaged tracks. A FLUX chunk shorter than 160 bytes MUST refuse the
  mount with a malformed-WOZ diagnosis. Damage MUST never appear as a silent
  unformatted track.
- **FR-009**: Bit-track playback and every non-flux image format MUST behave
  exactly as before.
- **FR-010**: The recorded flux timing of each track MUST remain available to
  other components, so a future disk inspector can show cell timing without
  re-reading the file.
- **FR-011**: Playing flux tracks MUST NOT measurably slow emulation compared
  with playing bit tracks.
- **FR-012**: Salvage MUST work on a disk with damaged tracks as it does on a
  checksum-damaged disk, MUST read sectors on flux tracks, and MUST say that
  the salvaged copy does not keep flux timing or copy protection when the
  source has flux tracks.
- **FR-013**: Sector-level tools (the `disk` command, Casso Explorer, and
  anything else that reads or writes sectors rather than playing the drive)
  MUST read sectors on flux tracks, and MUST write a sector to a flux track by
  replacing only its data field at the controller's cell timing, leaving the
  rest of the track's flux unchanged.

### Key Entities

- **Flux track**: one revolution of a quarter track as a sequence of times
  between flux transitions, in 125 ns ticks. A write is spliced into the
  sequence when it ends, so no separate record of written stretches is kept.
- **FLUX map**: 160 entries mapping quarter tracks to flux tracks in TRKS,
  laid out like TMAP.
- **Track slot**: what the head reads at a quarter track; either a bit track
  or a flux track.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `00_Bandits.woz` boots to its title screen on an Apple //e
  Enhanced, checked locally during development; no merged test depends on
  the image.
- **SC-002**: A made-up flux track mixing 3.7 µs and 4.1 µs cells reads back
  with each stretch's cell length within one read-circuit step of what the
  image holds.
- **SC-003**: Loading and saving an unwritten flux image produces a file
  whose FLUX chunk and flux track data are byte-for-byte identical to the
  original.
- **SC-004**: Every copy-protected bit-track WOZ disk in the test set that
  boots before this change still boots after it, and the full unit and
  scenario suites pass.
- **SC-005**: Emulation speed at maximum speed on a flux disk is within 2% of
  the same machine on a bit-only disk.
- **SC-006**: Each damaged-track case in User Story 3 mounts read-only with a
  report listing the damaged tracks, each unusable-map case is refused with a
  diagnosis, and none hangs silently.

## Assumptions

- The flux format and map rules follow the WOZ 2.1 reference
  (https://applesaucefdc.com/woz/reference2/). The reference says nothing
  about writing to flux tracks, so FR-006 and FR-007 are Casso's own choice.
- AppleEm (mikedaley/web-a2e, MIT license) is the reference for the timing
  model; its commit 74d710b plays flux by time and confirms *Bandits* needs it.
  Casso converts written flux tracks to flux, not to bits as AppleEm does.
- *Bandits* is the only flux image on hand. It is used for local testing
  during this spec's development only and is never checked in; made-up flux
  tracks cover every unit and scenario test that merges to master.
- Flux data is kept in memory at about its size in the file (about 38 KB per
  track), not expanded to one bit per read-circuit step.
- The disk inspector is out of scope and gets its own spec.
