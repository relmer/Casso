# Feature Specification: Apple II Cassette Tape Support

**Feature Branch**: `037-cassette-tape`

**Created**: 2026-10-01

**Status**: Draft

**Input**: GH #160, "Support for Apple II cassette tape interface"
(https://github.com/relmer/Casso/issues/160). A user asked to load cassette
tape images for the early Apple II software that shipped on tape, before
disks took over around 1982.

**GH ref**: GH #160 -- used in the CHANGELOG entry and in the commit and merge
subjects for this feature.

## Scope

Applies to the Apple ][, ][+ and //e. The //c has no cassette port and gets no
tape support. The C64 Datasette is out of scope and belongs to a future C64
spec.

## Clarifications

### Session 2026-10-02

- Q: Should an MP3 tape image load directly, or must the user convert it to WAV first? → A: Load MP3 directly, decoded through Windows Media Foundation.
- Q: Should Casso remember the inserted tape and its position across restarts? → A: Reinsert the last tape on launch, rewound to position 0.
- Q: Where does a recording go when record is armed? → A: Like a real deck, it overwrites the inserted tape from the current position; a new blank tape gives a fresh recording.
- Q: Should fast tape loading reuse the existing Maximum speed mode or use a separate turbo? → A: A temporary Maximum override; the speed menu keeps showing the user's setting, which is restored afterward.
- Q: Are +/-3% speed drift and a 16 KB load in under 10 s with fast loading the right targets? → A: Yes, as written in SC-003 and SC-004.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Load a program from a tape recording (Priority: P1)

A user has a recording of an Apple II cassette (a `.wav` from an archive, or
one they made from a real tape). They insert it into Casso's tape deck, type
the usual load command in the emulated machine (Monitor `addr.addrR`, or
Applesoft/Integer BASIC `LOAD`), press play, and the program arrives in memory
exactly as it would on real hardware.

**Why this priority**: This is the request in GH #160. Without playback there
is no tape feature.

**Independent Test**: Generate a `.wav` from a known binary, load it through
the real ROM on each supported model, and compare memory byte-for-byte with
the source binary.

**Acceptance Scenarios**:

1. **Given** a ][+ with a tape `.wav` of a known binary inserted, **When** the
   user enters `800.9FFR` in the Monitor and plays the tape, **Then** memory
   `$0800-$09FF` matches the source binary byte-for-byte and the Monitor
   reports no checksum error.
2. **Given** a //e at the Applesoft prompt with a tape of an Applesoft program
   inserted, **When** the user types `LOAD` and plays the tape, **Then** the
   program loads and `LIST` shows it intact.
3. **Given** a game that uses its own tape loader instead of the ROM READ
   routine, **When** the tape plays, **Then** the game's loader decodes it the
   same way it would on hardware, because Casso supplies only the signal.
4. **Given** a real-world recording with a DC offset, uneven level, mild speed
   drift and background noise, **When** it is loaded through the ROM, **Then**
   the load succeeds.

---

### User Story 2 - Fast loading during tape I/O (Priority: P2)

A real tape load takes minutes. By default, while a tape is playing and the
guest is actively reading it, Casso runs as fast as the host allows, so a load
finishes in seconds. A user who wants the authentic experience can turn this
off and hear the tape load at real speed.

**Why this priority**: Playback works without it, but multi-minute loads make
the feature unpleasant for anything beyond a demo.

**Independent Test**: Load the same tape with the preference on and off;
compare host wall-clock time and confirm identical memory results.

**Acceptance Scenarios**:

1. **Given** the default preference and a playing tape, **When** the guest
   starts reading the tape, **Then** emulation runs at Maximum speed, audio is muted,
   and frames are skipped.
2. **Given** A fast tape load, **When** the tape ends, the user stops it,
   or the guest makes no tape access for about 100 ms of emulated time,
   **Then** emulation returns to normal speed with audio and video restored.
3. **Given** a //e with no tape playing, **When** software polls the same
   address for another input (such as a game controller button), **Then**
   emulation stays at normal speed.
4. **Given** the real-time preference, **When** a tape loads, **Then** it takes
   its real duration and the tape signal is audible.

---

### User Story 3 - Save a program to tape (Priority: P3)

A user saves from the emulated machine (Monitor `addr.addrW`, or BASIC `SAVE`)
with record armed on the tape deck. Casso writes the output as a `.wav` that
loads back in Casso and in other emulators and tools.

**Why this priority**: Less common than loading, but completes the device and
gives the round-trip test.

**Independent Test**: Save a memory range to tape, load it back into a cleared
machine, and compare memory.

**Acceptance Scenarios**:

1. **Given** record armed and a new tape file chosen, **When** the user runs
   `800.9FFW` in the Monitor, **Then** a `.wav` is written that, played back
   through `800.9FFR` on a reset machine, restores the same bytes.
2. **Given** a recorded `.wav`, **When** it is opened in another Apple II tape
   tool, **Then** that tool decodes the same data.

---

### User Story 4 - Tape-deck controls (Priority: P2)

A user controls the tape from a tape-deck widget: insert or eject a recording,
play, stop, record, rewind, and see the current position and progress. The
widget looks and behaves like the existing drive widgets and sits in the
device toolbar.

**Why this priority**: Playback and recording are unusable without a way to
drive them. It ships with User Story 1.

**Independent Test**: Insert a tape, play part of it, stop, rewind, and
confirm the position display tracks each step; confirm the widget is absent
on the //c.

**Acceptance Scenarios**:

1. **Given** a ][+, **When** the user inserts a `.wav` from the tape deck,
   **Then** the widget shows the file and its length with position 0.
2. **Given** a playing tape, **When** the user presses stop and then rewind,
   **Then** the position holds while stopped and returns to 0 on rewind.
3. **Given** a //c, **Then** no tape deck is offered.

---

### Edge Cases

- A tape file that is not a readable audio file: insertion fails with a clear
  message and the deck stays empty.
- Stereo, 8-bit, 16-bit, 24-bit, and float recordings at common sample rates
  (8 kHz to 96 kHz) all load; stereo uses one channel or a mix.
- Inverted polarity: the ROM reads by transition timing, so it loads anyway.
- The tape reaches its end mid-load: playback stops; the guest sees what real
  hardware would (a stalled read or checksum error), never a crash.
- The user ejects or stops the tape during a fast load: speed returns to
  normal at once.
- The user resets or changes machine model while a tape plays: the tape stops
  and stays inserted.
- The user saves with record not armed: the output is discarded, as with a
  disconnected recorder.
- The user arms record on an archive tape partway through: the recording
  replaces the tape from that point, exactly as a real deck would.
- Emulation is paused: the tape position does not advance.
- Long silence or leader between programs on one tape: plays through normally.

## Requirements *(mandatory)*

### Functional Requirements

**Playback**

- **FR-001**: Casso MUST accept `.wav` and `.mp3` tape recordings, and SHOULD
  accept `.aif`/`.aiff`. MP3 is decoded through Windows Media Foundation.
- **FR-002**: Casso MUST convert the recording to a sequence of signal level
  transitions and present the current level to the guest on the cassette
  input ($C060 bit 7) on the ][, ][+ and //e.
- **FR-003**: Tape position MUST advance with emulated CPU time, never host
  wall-clock time, so loads are identical at any emulation speed.
- **FR-004**: The guest's own code MUST do all decoding. The load path MUST
  contain no knowledge of the Apple tape byte format, so ROM READ and custom
  loaders both work.
- **FR-005**: Transition detection MUST tolerate DC offset, uneven level, mild
  speed drift and noise in real recordings.

**Recording**

- **FR-006**: While record is armed, Casso MUST capture every cassette output
  toggle ($C020) with its emulated-cycle timestamp.
- **FR-007**: Recording MUST overwrite the inserted tape from the current
  position, extending it if the recording runs past the end, as a real deck
  does. On stop or eject Casso MUST write the result as a `.wav` that loads
  back in Casso and in other Apple II tape tools.
- **FR-007a**: The tape deck MUST offer a new blank tape, which creates an
  empty `.wav` file chosen by the user, so a fresh recording never touches an
  existing tape. Record MUST be unavailable for a tape Casso cannot write
  (MP3 or AIFF, or a read-only file), like a tape with its tab broken out.

**Fast loading**

- **FR-008**: By default, Casso MUST run at Maximum speed while a tape is playing (or
  record is armed) AND the guest is actively accessing the tape.
- **FR-009**: Maximum-speed override MUST end on end of tape, user stop or eject, or no
  tape access for about 100 ms of emulated time.
- **FR-010**: Polling the cassette input address alone MUST NOT start the
  Maximum-speed override, because on some models that address also reads
  another input (for example //e PB3).
- **FR-011**: During the override, audio MUST be muted or skipped and frames MUST
  be skipped.
- **FR-012**: A preference MUST let the user choose real-time loading instead,
  with authentic speed and audible tape sound.

**User interface**

- **FR-013**: A tape-deck control MUST offer insert, new blank tape, eject,
  play, stop, record, and rewind, and MUST show the tape's position and progress.
- **FR-014**: The tape-deck control MUST follow the conventions of the existing
  drive widgets and device toolbar.
- **FR-015**: The tape deck MUST NOT appear on machine models without a
  cassette port.
- **FR-016**: Casso MUST remember the inserted tape across sessions and
  reinsert it on launch, rewound to position 0 and stopped. A remembered tape
  that no longer exists leaves the deck empty.

### Explicitly rejected

- **ROM-trap fast-load** -- intercepting READ at $FEFD and injecting
  pre-decoded bytes. It breaks custom loaders, needs a second decoder that can
  disagree with the ROM, and fast loading gives near-instant loads
  without it.
- **TZX, TAP, T64** and other non-Apple tape formats.

### Possible follow-on

- A decode-only diagnostic that lists what a recording contains. It would be
  a separate tool and never a load path.

### Key Entities

- **Tape image**: an inserted recording -- source file, sample rate, length,
  and its derived sequence of level transitions.
- **Tape deck**: the transport state -- empty, stopped, playing, or
  recording -- and the current position in emulated time.
- **Recording capture**: the timestamped output toggles gathered while record
  is armed, and the file they will be written to.
- **Fast-load preference**: Maximum-speed override (default) or real-time.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every test tape generated from a known binary loads through the
  real ROM on the ][, ][+ and //e with memory matching byte-for-byte, via both
  Monitor `R` and BASIC `LOAD`.
- **SC-002**: A save-then-load round trip restores identical memory in 100% of
  test cases.
- **SC-003**: Test tapes with added noise, DC offset, level changes and speed
  drift of at least +/-3% load successfully.
- **SC-004**: With the default preference, a 16 KB load finishes in under 10
  seconds of host time on the development machine (real time: about 2
  minutes).
- **SC-005**: Maximum-speed override ends within 100 ms of emulated time of the last
  tape access, and never starts when no tape is playing or armed.
- **SC-006**: At least one real tape from the Internet Archive Apple II
  cassette collections loads and runs.

## Testing Approach

- Generate `.wav` files from known binaries with Egan Ford's c2t; load them
  through the real ROM (Monitor `addr.addrR` and Applesoft `LOAD`) and verify
  memory byte-for-byte.
- Record-then-playback round trip.
- Robustness cases: noise, DC offset, level change, speed drift.
- Override start and stop conditions, including the //e shared-address case.
- Manual validation with real tapes from the Internet Archive Apple II
  cassette collections (mostly WAV and MP3) and Apple-1 ACI
  archives, which use the same signal scheme.

## Assumptions

- One tape deck per machine, matching the hardware.
- The inserted tape is remembered across sessions like disk paths and is
  reinserted on launch rewound to position 0; the position is not saved.
- Recording overwrites the inserted tape from the current position, like a
  real deck; a fresh recording starts from a new blank tape.
- Fast loading is a temporary override to Casso's existing Maximum speed
  mode. The speed menu keeps the user's setting, which is restored when the
  override ends.
