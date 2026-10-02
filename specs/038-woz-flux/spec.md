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
21-34) and 19 flux tracks (half tracks 1.5 through 20.5). Its copy protection
writes parts of each flux track with cells about 3.7 µs long and parts with
cells about 4.1 µs long, and its loader times its reads to tell them apart.
Casso ignores the FLUX chunk today, so those 19 tracks read as unformatted and
the boot code searches forever. Converting flux to bit cells at load would
round every cell to the same length and erase exactly what the loader checks,
so flux tracks have to be played back by time.

## Scope

Applies to every machine with a Disk II controller (Apple ][, ][+, //e and
//c) and to 5.25" WOZ 2.1 images. 3.5" flux tracks are out of scope.

A disk inspector that draws flux timing (in Casso Explorer and in Casso,
modeled on AppleEm's Disk Inspector) is a separate future spec. This spec only
has to leave the flux timing available for it.

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
   nominal cell timing, and the rest keeps its original timing.
3. **Given** a rewritten flux track grew larger, **When** Casso saves, **Then**
   the TRKS entry and INFO's largest-flux-track field reflect its new size.
4. **Given** a saved flux image, **When** it is opened in Casso again, **Then**
   it boots as it did before the save.

---

### User Story 3 - Clear message for a flux image Casso cannot use (Priority: P3)

A user inserts a WOZ image whose FLUX chunk or flux track data is damaged.
Casso reports the problem when the disk is mounted instead of letting the boot
hang with no explanation.

**Why this priority**: The reporter's minimum expectation was "either the disk
boots or an error". Damaged flux data should never look like a blank track.

**Independent Test**: Mount images with a truncated flux track, a FLUX entry
pointing past the end of the file, and a flux-block field of zero alongside a
FLUX chunk, and confirm each produces a mount diagnosis.

**Acceptance Scenarios**:

1. **Given** a WOZ image whose FLUX chunk refers to track data outside the
   file, **When** it is mounted, **Then** Casso reports a malformed WOZ image
   and says the flux data is damaged.
2. **Given** a damaged flux track, **When** the mount is reported, **Then** the
   message follows Casso's error message format (a short label, then complete
   sentences).

### Edge Cases

- A flux track with no transitions at all: the drive reads random noise, as it
  does on an unformatted bit track.
- A gap between transitions longer than the drive's read circuit tolerates:
  the drive produces random bits for the gap, as it does for long runs of zero
  bits on a bit track.
- A gap encoded across several 255 bytes, longer than one cell by a large
  margin: it decodes to one long gap, not several short ones.
- An image whose INFO version is below 3 but which still has a FLUX chunk:
  the FLUX chunk is honored.
- Write-protected flux disk: writes are blocked and the image is never
  re-encoded.
- Quarter tracks a flux half track bleeds into: the drive picks up the nearest
  mapped track the same way it does for bit tracks.
- Bit-only WOZ, NIB, DSK, DO and PO images behave exactly as before.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Casso MUST read the FLUX chunk, INFO's flux-block field and
  INFO's largest-flux-track field from WOZ 2.1 images.
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
  stretch is stored at nominal cell timing and the rest of the track keeps its
  recorded timing.
- **FR-007**: Saving an image MUST write unwritten flux tracks back
  byte-for-byte, and MUST update the TRKS entries and INFO's flux fields for
  any rewritten flux track.
- **FR-008**: Damaged flux data (references outside the file, truncated
  tracks, inconsistent INFO fields) MUST produce a mount diagnosis with a
  specific message, never a silent unformatted track.
- **FR-009**: Bit-track playback and every non-flux image format MUST behave
  exactly as before.
- **FR-010**: The recorded flux timing of each track MUST remain available to
  other components, so a future disk inspector can show cell timing without
  re-reading the file.
- **FR-011**: Playing flux tracks MUST NOT measurably slow emulation compared
  with playing bit tracks.

### Key Entities

- **Flux track**: one revolution of a quarter track as a sequence of times
  between flux transitions, in 125 ns ticks, plus the record of which
  stretches the drive has rewritten.
- **FLUX map**: 160 entries mapping quarter tracks to flux tracks in TRKS,
  laid out like TMAP.
- **Track slot**: what the head reads at a quarter track; either a bit track
  or a flux track.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `00_Bandits.woz` boots to its title screen on an Apple //e
  Enhanced.
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
- **SC-006**: Each damaged-flux case in User Story 3 produces a mount
  diagnosis; none hangs silently.

## Assumptions

- The flux format and map rules follow the WOZ 2.1 reference
  (https://applesaucefdc.com/woz/reference2/). The reference says nothing
  about writing to flux tracks, so FR-006 and FR-007 are Casso's own choice.
- AppleEm (mikedaley/web-a2e, MIT license) is the reference for the timing
  model; its commit 74d710b plays flux by time and confirms *Bandits* needs it.
  Casso converts written flux tracks to flux, not to bits as AppleEm does.
- *Bandits* is the only flux image on hand. The reporter will be asked for
  permission to check it in as a test fixture; until then it is tested
  locally, and made-up flux tracks cover the unit tests.
- Flux data is kept in memory at about its size in the file (about 38 KB per
  track), not expanded to one bit per read-circuit step.
- The disk inspector is out of scope and gets its own spec.
