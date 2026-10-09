# Specification Quality Checklist: Disk Inspector

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-10-08
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [ ] No [NEEDS CLARIFICATION] markers remain
- [ ] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [ ] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [ ] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- Two [NEEDS CLARIFICATION] markers remain. FR-058: whether exporting a
  track or a sector to a file is in this spec, and in which forms. FR-078:
  how the inspector window in Casso gets a light theme, so that the parts
  specific to Casso can be checked in light as well as dark; Casso's own
  themes are all dark, and 033 adds Light and Dark to Explorer only. The four
  unchecked items fail on these two alone: export has no acceptance scenario
  until FR-058 is settled, and the light check of the Casso-only parts in
  SC-008 depends on how FR-078 is settled. Copy to the clipboard (FR-057) is
  decided and does not depend on either.
- An earlier revision fixed the review issues that made the first
  requirements ambiguous or contradictory: the quarter-track and sharing
  findings that fired on every ordinary disk (FR-048, SC-013), noise against
  random-bit regions and the reference turn (FR-009, FR-010, FR-008, SC-002),
  the disk's format rule (FR-018), the order of classification (FR-016), the
  nominal track length (Assumptions), field pairing (FR-013), the first
  sector selected (FR-039), the histogram's scope (FR-045), the hosts' common
  and specific parts (FR-001), the whole-disk analysis cases (FR-069), the
  pass and fail tests for small features, drags and legibility (FR-023,
  FR-025, FR-026, FR-035), and the measurement methods in SC-004, SC-005 and
  SC-008. The 033 merge order is stated in Scope under Delivery.
- On 2026-10-08 the owner expanded the spec. A file and sector map (User
  Story 8, FR-083 to FR-094), sector editing (User Story 9, FR-095 to FR-108)
  and comparison (User Story 10, FR-109 to FR-115) join the first release,
  with SC-014 to SC-021. Four preservation stories (User Stories 11 to 14:
  custom translate tables, bit slip, protection identification and weak
  bits) are specified for a later release of the same feature, with their own
  merge; their requirements (FR-116 to FR-132), success criteria (SC-022 to
  SC-026), entities, edge cases and assumptions are marked "later release".
  Disk breakpoints go to the 035 debugger spec; the out-of-scope list points
  there, and Assumptions lists the parts the two specs share. Viewing still
  changes nothing, and a disk changes only through an applied, undone or
  redone sector edit.
- The review of that expansion is applied in this revision. Three
  requirements and two success criteria were added, and every requirement
  and success criterion after them was renumbered: FR-063 (the inspector
  reads copies of track records, never the drive's live record), FR-084 (how
  a DOS 3.3 or ProDOS volume is found, its size, and which quarter track and
  field supply each sector), FR-105 (an Explorer edit of an image a running
  Casso has in a drive), SC-019 (no guest write, applied edit or pending edit
  is lost without notice) and SC-026 (opening a large A2R capture). The main
  changes: comparison compares each whole track once where both disks use
  the standard layouts, gives quarter tracks with nothing recorded their own
  verdict, decodes B with the window's decode settings, and gains "Ignore
  volume numbers" (FR-109 to FR-113); an edit reaches the drive only at a
  safe point with the machine held paused until it is saved or rolled back,
  and a failed save restores the tracks and keeps the edits pending (FR-102,
  FR-103); redo has the same guards as undo, and both obey write protection
  (FR-106); pending edits follow their fields after a guest write or reload
  (FR-107); Explorer's damaged images count as write-protected (FR-101);
  sector images always recompute the checksum (FR-097); Casso's NIB and NB2
  save writes an edited field in place (FR-103); Explorer edits use the
  in-place change and never the existing sector writes, which stay out of
  scope (Scope, FR-104); the map's roles, precedence and chain rules are
  defined (FR-085, FR-088, FR-091, FR-092); and story priorities are now
  distinct for User Stories 7 to 14, in build order.
- "Edge cases are identified" was rechecked after the failure paths of
  editing were added to Edge Cases: a failed save, a save to a preserved
  copy, a pause while the drive writes or the head is inside an edited field,
  a guest write during the apply wait, redo after a guest write, quitting or
  changing the machine with pending edits, and an Explorer edit of an image
  Casso holds with guest writes not yet saved.
- Every value left to planning has a starting value in Assumptions: the data
  search window (48 nibbles), the track-length finding (±2%), the
  unformatted threshold (more than half a track), the flux timing tolerance
  for comparison (±1% of a cell), the nibble alignment limit (64 nibbles),
  and, in the later release, the bit-slip minimum run (6 nibbles), the
  "Solve table" limit (two swapped pairs), the "Trace boot" cycle limit (5 s
  of emulated time), the revolution alignment tolerance (one cell) and the
  off-grid limit for weak bits (25% of a cell).
- The Background's description of the Disk ][ debug window was corrected
  twice: it logs only the fields the guest reads with standard marks, which
  are address fields with a good checksum and data fields whose epilogue the
  guest reads, not every field that passes the head.
- The spec uses Disk II and WOZ terms (nibbles, sync, prologues and
  epilogues, address and data fields, 4-and-4, 6-and-2 and 5-and-3, quarter
  tracks, flux ticks, cell timing, TMAP, FLUX, TRKS, INFO, META, WRIT), DOS
  3.3 and ProDOS terms (VTOC, catalog, track/sector list, index and master
  index blocks, volume bitmap) and capture terms (A2R, revolutions, latch
  framing, translate tables, weak bits). These describe the disks, file
  systems and file formats the feature inspects, which are its subject, not
  implementation choices. "Written for non-technical stakeholders" is checked
  on that basis: the readers are Apple II disk users, and the spec holds no
  class, file, thread or rendering detail. Requirements on what Casso keeps
  and makes available to the inspector (FR-052, FR-053, FR-062, FR-063,
  FR-068, and the disk as inserted in FR-109) state the data the inspector
  needs, not how Casso stores it. FR-105's reload intent and the Assumptions
  on the existing sector writes, the channel between Explorer and Casso and
  the controller's one event listener describe existing Casso behavior the
  feature has to work with, not choices this spec makes.
- Menu labels, tooltips and empty-state text are given in full because they
  are user-visible behavior; all are in sentence case.
- Figures taken from AppleEm (600× platter zoom, the 0.3 s Follow head delay,
  the ±5% default timing range, the 48-nibble data search window, and the
  980×660 window with its 640×460 minimum) are recorded as choices in
  Assumptions or the requirements.
- Scope lists every AppleEm feature left out or changed, with its reason.
  The comparison was repeated after the window layout (FR-007), the saved
  open state (FR-080), the data-search stop rule (FR-013), the empty-track
  text (FR-040), the "Nothing recorded" format (FR-018) and the read marks
  (FR-047, FR-031) were added; the 10-cell noise rule is listed as changed.
  Scope also lists the additions from other tools, which of them are in the
  later release, and what stays out of scope. Delivery is two releases, each
  one merge: the first holds User Stories 1 to 10 and the later one User
  Stories 11 to 14. Priorities give build order only (analyzer, views, Casso
  host, then the map, editing and comparison, then the Explorer hosts and the
  parts of the map, editing and comparison that need 033, after 033 merges
  master and 040 is rebased onto 033, and in the later release the
  preservation stories), and 040 merges to master only after 033 is on
  master.
- Choices made in this revision that the owner may want to revisit, none of
  them a [NEEDS CLARIFICATION] marker: with guest writes not yet saved, an
  Explorer edit of an image Casso holds leaves the drive on the edited file
  or on the preserved copy depending on which of Casso's checks finds the
  change first (FR-105), rather than changing Casso's rules so the drive
  always ends on the edited file; the existing sector writes of the `disk`
  command and Explorer stay as they are, with their defects left to a
  separate GH issue that does not exist yet, and spec 038's FR-013 is not
  amended; comparing two quarter tracks of one disk, and decoding 18-sector
  layouts, are out of scope.
