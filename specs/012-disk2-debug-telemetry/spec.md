# Feature Specification: Disk II Debug Telemetry — Head-Resolution & Read-Stall Diagnostics

**Feature Branch**: `012-disk2-debug-telemetry`
**Created**: 2026-05-30
**Status**: Draft (deferred — do not implement until coordinated with spec 011)
**Input**: User description: "What data should we have added to the Disk II debug window to have caught the half-track truncation bug earlier? Capture four enhancements — (1) flag commanded-vs-resolved head-position discrepancies as errors with a new severity column, (2) surface the TMAP/resolved-track lookup and actively diagnose dropped half-track data rather than leaving it to the user, (3) a read-stall event when the loader spins with no address mark, (4) a raw nibble peek under the head — and defer them to a spec to avoid colliding with the other CLI instance currently editing the debug dialog."

## Background & Motivation *(non-normative)*

Choplifter (#69) hangs on "track 12". A multi-session investigation eventually
traced this to a **whole-track-only disk pipeline** colliding with Choplifter's
**half-track-formatted outer tracks** (its WOZ TMAP routes the real outer-track
data — TRKS 13, 15, 17, … — to the `.5` quarter-track positions, e.g. qt50 =
track 12.50 → TRKS 13). Two truncation points silently discarded that data:
`WozLoader` dropped every non-whole-track TMAP entry, and `Disk2Controller`
quantized the head with `m_quarterTrack / 4`.

Crucially, the Disk II Debug window (spec 006) **already recorded the raw
signal** — `HeadStep` events carry quarter-track `prevQt`/`newQt`, so the loader
stepping to qt50 was right there in the log. What the window never showed was
the **resolution result**: that qt48 and qt50 both served the *same* bitstream
(TRKS 12), when the TMAP says qt50 should serve TRKS 13. Nothing contrasted the
*commanded* head position against the *resolved* track. This spec adds the
telemetry that would have turned a multi-session hunt into a glance.

This feature is a **diagnostic-fidelity** add-on to spec 006; it does not change
emulation behavior. It is most useful *after* the quarter-track resolution fix
(tracked separately under #67); several requirements below describe behavior in
both the pre-fix (truncating) and post-fix (quarter-track-aware) worlds.

## Coordination constraint *(normative)*

The Disk II Debug dialog surface (`Casso/Disk2DebugDialog.*`,
`Disk2DebugDialogState.*`, `Disk2EventDisplay.*`) is being actively modified by a
separate effort (spec 011, native-dialogs-completion). To avoid merge conflicts:

- **FR-C1**: This feature MUST NOT be implemented until spec 011's dialog edits
  have landed on `master`. Until then this document is a captured backlog item.
- **FR-C2**: New event types and payloads (the emulation-core side:
  `Disk2Event.h`, `Disk2Controller`, `Disk2NibbleEngine`, `WozLoader`,
  `DiskImage`, the `IDisk2EventSink` interface) MAY be designed and even
  implemented independently of the dialog, since they live in CassoEmuCore and
  do not touch the Win32 surface. The dialog-side rendering (new column, icons,
  formatters, filters) MUST be sequenced after spec 011.

## Clarifications

### Session 2026-05-30

- Q: Should a commanded-vs-resolved head-position discrepancy be surfaced as an
  error? → A: Yes. Introduce a new **Severity** column (Info / Warning / Error)
  with a matching status icon. A discrepancy between the commanded quarter-track
  and the track the engine actually serves is an **Error**-severity row.
- Q: For the "unformatted / dropped half-track data" case, should the window just
  show an FF/unformatted flag, or actually diagnose the data loss? → A: Actively
  diagnose it. The tooling MUST detect — without relying on the user to
  eyeball — when a mounted WOZ carries distinct data at quarter-track positions
  the current pipeline cannot address, and emit an explicit diagnostic naming the
  affected tracks. Bonus over a bare FF indicator.
- Q: Read-stall event and raw nibble peek? → A: Both approved as specified.
- Q: The boot-recalibrate audio ratchet (Disk2AudioSource) deliberately
  suppresses the audio onset on some head bumps (the silent slot of its
  `[thunk, pause, click, click]` pattern), so the window shows a bare
  `Head bump` row with no accompanying audio-decision event. Should the
  window distinguish a bump that voiced a sound from one whose audio was
  intentionally suppressed? → A: Yes. A bump-without-audio currently reads
  like a dropped/missing event. The window MUST identify suppressed audio
  events explicitly (Info severity) so an intentionally silent ratchet slot
  is visibly distinct from an audio failure.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Resolved-track telemetry exposes a head-resolution bug at a glance (Priority: P1)

A developer is investigating why a WOZ image hangs partway through loading. They
open **View → Disk II Debug…**, mount the image, and cold-boot. As the loader
steps the head, each `HeadStep` row now shows not just the commanded quarter-
track but the **resolved** mapping the engine used: `qt → TMAP[qt] → TRKS idx
(bitCount)`. When the loader steps to a half-track whose data the pipeline cannot
reach, the row resolves to the *wrong* TRKS record (or an unformatted slot), and
the contrast between commanded and resolved position is immediately visible. With
the discrepancy-detection of User Story 2, that row is additionally flagged
Error-severity.

**Why this priority**: This is the single datum whose absence cost multiple
sessions of investigation. It is the feature's core reason to exist.

**Independent Test**: Mount a half-track-formatted WOZ (e.g. Choplifter), boot,
and confirm that as the head reaches the half-track region the debug log shows
distinct commanded and resolved track values for the `.5` positions. Confirm a
standard DOS 3.3 disk shows commanded == resolved for every step.

**Acceptance Scenarios**:

1. **Given** a mounted WOZ and the debug window open, **When** the head steps to
   quarter-track `qt`, **Then** the resulting `HeadStep` (or a new
   `HeadResolve`) row MUST display the commanded position (`qtN`, rendered as a
   fractional track e.g. `12.50`), the TMAP entry for `qt` (TRKS index or
   `FF`/unformatted), and the bit length of the served bitstream.
2. **Given** a standard whole-track disk, **When** the head settles on any track,
   **Then** the commanded and resolved track MUST be equal for every step (no
   spurious discrepancy rows).
3. **Given** a half-track-formatted disk on the pre-fix (truncating) pipeline,
   **When** the head steps to a `.5` position carrying distinct data, **Then**
   the resolved TRKS index MUST differ from what the TMAP assigns to that
   quarter-track (exposing the truncation).

---

### User Story 2 — Severity column flags commanded-vs-resolved discrepancies as errors (Priority: P1)

The developer wants problems to *announce themselves*, not require reading every
row. A new **Severity** column (leftmost or adjacent to Event) classifies each
row Info / Warning / Error with a matching icon. Routine events are Info. A
head-resolution discrepancy — commanded quarter-track resolves to a track the
TMAP did not assign to it, or to an unformatted slot when the loader expected
data — is an **Error**. The developer can filter to Error+Warning only and
instantly see the failing seek.

**Why this priority**: Turns the resolved-track datum (US1) from "available if
you look" into "impossible to miss." Pairs with US1 as the MVP.

**Independent Test**: Boot a half-track WOZ on the truncating pipeline and verify
an Error-severity row appears at the first `.5`-position seek; filter to
Error-only and confirm the failing seeks are isolated. Boot a standard disk and
verify zero Error/Warning rows.

**Acceptance Scenarios**:

1. **Given** any controller/audio event with no anomaly, **When** it is logged,
   **Then** its Severity MUST be Info with the Info icon.
2. **Given** a head step whose commanded quarter-track resolves to a TRKS record
   the TMAP did not assign to that quarter-track, **When** it is logged, **Then**
   its Severity MUST be Error with the Error icon and a Detail string naming both
   the commanded and resolved positions.
3. **Given** the Severity filter set to "Error and Warning only", **When** the
   projection rebuilds, **Then** only rows at those severities are shown
   (consistent with spec 006's projection-not-drop filter model).
4. **Given** the Severity column, **When** the user sorts/auto-sizes columns,
   **Then** it behaves like other spec-006 columns (FR-026/FR-027 semantics).

---

### User Story 3 — Half-track data-loss is diagnosed at mount, not left to the user (Priority: P2)

When a WOZ is mounted, the tooling scans the TMAP and determines whether the
image carries **distinct** data at quarter-track positions the current pipeline
cannot address (i.e. distinct TRKS records at `qt % 4 != 0` while head resolution
is whole-track-only). If so, it emits a single explicit diagnostic at mount time
naming the affected fractional tracks (e.g. "WOZ uses half-track formatting;
data at tracks 12.50, 13.50, 14.50, … is not addressable by the current
whole-track head pipeline — N quarter-tracks affected"). The developer learns the
*cause* without manually decoding the TMAP. After the quarter-track resolution
fix lands, the same scan instead emits an Info row confirming half-track data is
present and addressable.

**Why this priority**: Converts a latent silent data-loss condition into an
explicit, named diagnostic. High value, but depends on US1's TMAP plumbing.

**Independent Test**: Mount Choplifter on the truncating pipeline and verify a
single Warning/Error mount-time diagnostic listing the affected half-tracks.
Mount a standard DOS 3.3 disk and verify no such diagnostic. After the
quarter-track fix, mount Choplifter and verify the diagnostic downgrades to Info
("half-track data present and addressable").

**Acceptance Scenarios**:

1. **Given** a WOZ whose TMAP assigns distinct TRKS records to `qt % 4 != 0`
   positions, **When** it is mounted on a whole-track-only pipeline, **Then** a
   single mount-time diagnostic row MUST be emitted at Warning or Error severity,
   listing the affected fractional tracks and the count of unreachable
   quarter-tracks.
2. **Given** a WOZ with no distinct fractional-track data (standard format),
   **When** it is mounted, **Then** NO half-track diagnostic MUST be emitted.
3. **Given** the quarter-track resolution fix is in place, **When** a half-track
   WOZ is mounted, **Then** the diagnostic MUST be Info severity and state the
   data is addressable.
4. **Given** the diagnostic is emitted, **When** the user reads it, **Then** it
   MUST be a single consolidated row (not one row per affected quarter-track).

---

### User Story 4 — Read-stall event makes an invisible spin visible (Priority: P2)

The hang's actual signature is a read loop spinning at a head position finding no
matching address mark. Today `AddrMark` fires only on **success**, so the spin is
invisible — the log just shows address marks tapering off. This feature adds a
`ReadStall` event emitted when the passive nibble watcher observes the head dwell
at a position for ≥ K disk revolutions with zero address marks decoded while the
read latch is being actively polled. The row names the position and the
revolution count, pointing straight at the failing seek.

**Why this priority**: Directly surfaces the failure mode. Independent of the
resolution telemetry, but lower priority than the resolved-track contrast that
explains *why* the stall happens.

**Independent Test**: Boot a disk that stalls (half-track WOZ on the truncating
pipeline) and verify a `ReadStall` row appears at the stalling track within a
bounded number of revolutions. Boot a healthy disk and verify no `ReadStall`
rows fire during a normal boot.

**Acceptance Scenarios**:

1. **Given** the read latch is being polled (`$C08C` reads) at a fixed head
   position, **When** ≥ K full revolutions elapse with zero address marks
   decoded, **Then** exactly one `ReadStall` row MUST be emitted (Warning
   severity) naming the position and revolution count.
2. **Given** a `ReadStall` was emitted, **When** the loader subsequently decodes
   an address mark at that position or steps the head, **Then** the stall state
   MUST reset so a later stall at the same position can re-fire.
3. **Given** a normal boot with steady address-mark cadence, **When** the boot
   completes, **Then** NO `ReadStall` rows MUST be emitted.

---

### User Story 5 — Raw nibble peek under the head (Priority: P3)

The developer wants to see the actual nibbles currently flowing under the head at
a chosen position, to compare against an expected prologue. The window offers a
read-only "nibble peek" that samples a short window of nibbles the engine is
currently returning at the head's position (non-perturbing — it does not consume
or advance the real read cursor). Comparing the peeked nibbles after a `.5`-seek
against the expected outer-track prologue shows a wrong-track read directly. The
existing `trackFilterRawQt` flag in `FilterState` suggests this raw view was
already anticipated.

**Why this priority**: Powerful but the most niche and the most implementation-
heavy (non-perturbing sampling). Lowest priority of the five.

**Independent Test**: Seek to a known track on a standard disk and verify the
nibble peek shows the expected `D5 AA 96` prologue cadence; verify peeking does
not alter the live read stream (boot continues unaffected).

**Acceptance Scenarios**:

1. **Given** a mounted disk and a settled head, **When** the user requests a
   nibble peek, **Then** the window MUST display a short sample of the nibbles
   currently under the head at the current position.
2. **Given** a peek is taken, **When** the live emulation continues, **Then** the
   peek MUST NOT consume, advance, or otherwise perturb the real read cursor or
   the LSS state (non-perturbing sampling).

---

### Edge Cases

- WOZ with a TMAP that maps a quarter-track to a TRKS record whose `bitCount` is
  zero or out of range → resolved-track display MUST show the slot as
  unformatted/empty rather than crash, and (if the loader expected data) flag a
  discrepancy.
- Multiple distinct fractional-track regions on one disk (e.g. tracks 12.5–34.5)
  → the mount-time half-track diagnostic MUST consolidate into one row with a
  compact range/list, not flood the log.
- `.dsk` / `.nib` images (no TMAP) → resolved-track display MUST degrade
  gracefully (commanded == resolved, no TMAP column content), and the half-track
  diagnostic MUST NOT fire.
- Quarter-track fix landed but a WOZ legitimately leaves a quarter-track
  unformatted by design (protection expects no data there) → resolving to an
  unformatted slot MUST be Info/expected, not an Error, when the loader is not
  actively waiting for a prologue there. (Discrepancy = commanded resolves to a
  *different assigned* track, not merely an unformatted one.)
- Read-stall K threshold tuning so brief inter-sector gaps and self-sync regions
  do not trip a false stall.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The emulation core MUST expose, per head-position change, the
  commanded quarter-track, the TMAP entry for that quarter-track (TRKS index or
  unformatted marker), and the bit length of the served bitstream, via the
  `IDisk2EventSink` surface (new payload or new event type).
- **FR-002**: The debug window MUST render, for each head-resolution event, the
  commanded fractional track, the resolved TRKS index, and the served bit length.
- **FR-003**: The system MUST classify every logged row with a Severity of Info,
  Warning, or Error, and the window MUST render a new Severity column with a
  matching status icon per level.
- **FR-004**: The system MUST classify a head-resolution event as Error severity
  when the commanded quarter-track resolves to a TRKS record the TMAP did not
  assign to that quarter-track (or to an unformatted slot while the loader is
  awaiting a prologue there).
- **FR-005**: The Severity column MUST participate in the spec-006 filter model
  (projection, not drop), with at least an "Error/Warning only" filter option.
- **FR-006**: On disk mount, the system MUST scan the TMAP and detect distinct
  data at quarter-track positions the current head pipeline cannot address; when
  found, it MUST emit exactly one consolidated diagnostic row naming the affected
  fractional tracks and the count of unreachable quarter-tracks.
- **FR-007**: The mount-time half-track diagnostic MUST be Warning/Error severity
  while the pipeline is whole-track-only, and Info severity once quarter-track
  resolution is in place and the data is addressable.
- **FR-008**: The system MUST emit a `ReadStall` event (Warning severity) when
  the read latch is actively polled at a fixed head position for ≥ K disk
  revolutions with zero address marks decoded; the stall state MUST reset on a
  successful address-mark decode or a head step so it can re-fire later.
- **FR-009**: The `ReadStall` revolution threshold K MUST be a named constant
  tuned to avoid false positives across self-sync gaps and normal inter-sector
  spacing. [NEEDS CLARIFICATION: exact K — proposed default 2–3 revolutions.]
- **FR-010**: The window MUST provide a non-perturbing "nibble peek" that samples
  a short window of the nibbles currently under the head without consuming or
  advancing the real read cursor or mutating LSS state.
- **FR-011**: All new event types MUST keep `Disk2Event` within its documented
  size bound (currently ≤ 32 bytes; see `Disk2Event.h` static_assert) or the
  bound MUST be explicitly and documented-ly relaxed with a ring-footprint
  benchmark.
- **FR-012**: For non-WOZ images (no TMAP), resolved-track display MUST degrade
  to commanded == resolved with empty TMAP content, and the half-track diagnostic
  MUST NOT fire.
- **FR-013**: The system MUST distinguish a head-movement event that voiced a
  sound from one whose audio was intentionally suppressed. The boot-recalibrate
  ratchet in `Disk2AudioSource::OnHeadBump` cycles rapid consecutive bumps
  through a `[thunk, silent, click, click]` pattern, deliberately emitting no
  audio-decision event on the silent slot. Today such a bump renders as a bare
  `Head bump` row with no adjacent `Audio …` row, which is indistinguishable
  from a missing/dropped audio event. The window MUST surface an explicit
  Info-severity indicator (e.g. an `Audio suppressed (ratchet)` annotation or a
  voiced/silent marker on the bump row) so an intentionally silent ratchet slot
  is visibly distinct from an audio failure. This requires the audio source to
  report the suppression decision (a new audio-event-sink signal or equivalent
  payload), not just omit the event.

### Key Entities

- **Head-resolution record**: commanded quarter-track, fractional track label,
  TMAP entry (TRKS index or unformatted), served bit length, discrepancy flag.
- **Severity**: enum { Info, Warning, Error } with associated icon; a property of
  every display row.
- **Half-track diagnostic**: one-shot, per-mount consolidated record listing the
  fractional tracks carrying unreachable distinct data and the affected count.
- **ReadStall record**: head position, revolution count at stall detection.
- **Nibble peek**: ephemeral, non-perturbing sample of nibbles at the current
  head position (not a ring event; a pull-style query).
- **Suppressed-audio marker**: per-bump indication that the audio source
  intentionally voiced no sound on a ratchet silent slot, distinguishing a
  deliberately silent bump from an audio failure (FR-013).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Given a half-track-formatted WOZ on the truncating pipeline, a
  developer can identify the failing fractional-track seek from the debug window
  in under 60 seconds, without external tools or TMAP decoding.
- **SC-002**: Booting a standard DOS 3.3 disk produces zero Error- and
  zero Warning-severity rows for an entire successful boot (no false positives).
- **SC-003**: The mount-time half-track diagnostic correctly fires for Choplifter
  and stays silent for at least one standard-format reference disk.
- **SC-004**: A `ReadStall` row appears within K+1 revolutions of a genuine read
  stall and never appears during a clean boot.
- **SC-005**: Enabling the nibble peek during an active boot does not alter the
  boot outcome or timing (non-perturbing, verified by identical boot behavior
  with peek on vs off).

## Assumptions

- This feature is a diagnostic add-on to spec 006 and changes no emulation
  behavior; it only observes and reports.
- The quarter-track resolution fix (under #67) is a separate, prerequisite-ish
  work item; this spec describes telemetry behavior in both the pre-fix
  (truncating) and post-fix (quarter-track-aware) worlds and is most valuable
  once that fix lands.
- The Disk II Debug dialog surface is owned by spec 011 in the near term; the
  dialog-side portions of this feature are sequenced after spec 011 reaches
  `master` (see Coordination constraint). The CassoEmuCore-side event plumbing
  may proceed independently.
- Single Disk II controller, consistent with spec 006's v1 scope.
- WOZ images are not committed to the repo; tests use synthetic mini-WOZ fixtures
  or on-demand downloads, per repository security rules.
