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
  Story 8, FR-083 to FR-095), sector editing (User Story 9, FR-096 to FR-109)
  and comparison (User Story 10, FR-117 to FR-123) join the first release,
  with SC-014 to SC-022. Four preservation stories (User Stories 11 to 14:
  custom translate tables, bit slip, protection identification and weak
  bits) are specified for a later release of the same feature, with their own
  merge; their requirements (FR-124 to FR-140), success criteria (SC-023 to
  SC-027), entities, edge cases and assumptions are marked "later release".
  Disk breakpoints go to the 035 debugger spec; the out-of-scope list points
  there, and Assumptions lists the parts the two specs share. Viewing still
  changes nothing, and a disk changes only through an applied, undone or
  redone sector edit.
- The review of that expansion was applied in the next revision. Three
  requirements and two success criteria were added: FR-063 (the inspector
  reads copies of track records, never the drive's live record), FR-084 (how
  a DOS 3.3 or ProDOS volume is found, its size, and which quarter track and
  field supply each sector), FR-106 (an Explorer edit of an image a running
  Casso has in a drive), SC-020 (no guest write, applied edit or pending edit
  is lost without notice) and SC-027 (opening a large A2R capture). The main
  changes: comparison compares each whole track once where both disks use
  the standard layouts, gives quarter tracks with nothing recorded their own
  verdict, decodes B with the window's decode settings, and gains "Ignore
  volume numbers" (FR-117 to FR-121); an edit reaches the drive only at a
  safe point with the machine held paused until it is saved or rolled back,
  and a failed save restores the tracks and keeps the edits pending (FR-103,
  FR-104); redo has the same guards as undo, and both obey write protection
  (FR-107); pending edits follow their fields after a guest write or reload
  (FR-108); Explorer's damaged images count as write-protected (FR-102);
  sector images always recompute the checksum (FR-098); Casso's NIB and NB2
  save writes an edited field in place (FR-104); the map's roles, precedence
  and chain rules are defined (FR-086, FR-089, FR-092, FR-093); and story
  priorities are distinct for User Stories 7 to 14, in build order.
- The owner's decisions on the questions that revision left open were applied
  in the next revision, and every requirement and success criterion was
  renumbered by script, with every cross-reference checked afterward:
  1. An edit Explorer saves to an image a running Casso holds is a change made
     outside Casso, sent with the reload intent and handled exactly as
     Explorer's other writes to a mounted image are (033's FR-024), through
     Casso's external-change handling, which spec 041 reworks (FR-106). The
     confirmation note and the description of two outcomes are gone; Casso's
     inspector shows which file the drive holds afterward, Explorer's
     inspector shows Casso's reply when it is anything other than a reload
     (FR-105), and Assumptions assigns any inconsistency in that handling to
     Casso's external-change handling (FR-001, User Story 9, Edge Cases,
     SC-020, Assumptions).
  2. The shared sector writer joins the first release (FR-110 to FR-116,
     SC-017, User Story 9 scenarios 16 to 24). The inspector's editor, every
     sector, block and file write of the `disk` command, and Explorer's writes
     use it. It accepts the standard WOZ layouts, keeps protecting tracks that
     do not decode completely and images with records of their own between
     whole tracks, never formats a track, finds whole track N through the map
     as the `disk` command's and Explorer's reads now also do, never changes a
     field's nibble count or length (they are fixed for 256 bytes of 6-and-2
     or 5-and-3 data), and saves durably. The out-of-scope entry that left
     those writers unchanged is gone; GH #TBD tracks the immediate fix of the
     standard-layout rejection and the volume-254 rebuild, and Scope and
     Assumptions say that 040 moves the writers onto the shared writer and
     fixes the reads.
  3. Direct sector writes keep each flux cell's recorded time, a fix to the
     shipped behavior of spec 038's FR-013, while guest writes stay at the
     drive's own timing (FR-113).
  4. Apple Pascal and CP/M file maps join the first release, with read-only
     readers sufficient for the map (FR-083, FR-084, FR-085, FR-086 to
     FR-089, FR-093, FR-094, User Story 8 scenarios 13 to 16, SC-014,
     SC-015).
  5. The later release gains User Story 15 (documented custom track formats,
     one decoder each, starting with RW18; FR-141 to FR-144, SC-028) and User
     Story 16 (comparing quarter tracks of one disk; FR-145 to FR-147,
     SC-029). Protection identification and A2R captures move to P12 and P13,
     so each is built after the analysis it reports on, except weak bits,
     which User Story 14 adds to the Protection category.
  6. Every story heading follows the Spec Kit template.
  7. 040 follows spec 041: the head limit is the head stop of Casso's emulated
     Disk II, the same in both hosts (FR-004); the empty tracks Casso keeps
     for the guest to format show as "Nothing recorded" and are not listed in
     the Image tab, and a track the guest writes there shows as written and
     added (FR-005, FR-050, FR-071, User Story 6 scenario 9); saves are
     durable (FR-116); and the inspector never reads the drive's disk itself,
     but reads every track record and image detail from a copy (FR-063), while
     Assumptions record that its head state, write-protect causes and counts
     come from 041's drive status (FR-062, FR-065, FR-068). Class names and
     threads appear only in Assumptions.
- Choices made in that revision, none of them a [NEEDS CLARIFICATION] marker:
  the shared writer also covers `delete`, `boot` and the assemblers' `--disk`
  output, which also change sectors of an image (FR-110); a `disk` command or
  Explorer write to a track with no standard address field fails instead of
  formatting the track (FR-115); a CP/M volume is looked for only when no
  other file system is found, and a Pascal volume of other than 280 blocks is
  not mapped (FR-085); and sectors of custom track formats are not editable
  (FR-144).
- The review of that revision was applied in this one, with no requirement or
  success criterion added or removed, so no renumbering was needed; every
  cross-reference was checked by script afterward. The quarter-track finding
  now exempts every layout the shared writer accepts, including Casso's own
  and a track the guest formats where the file held none (FR-048, FR-118,
  SC-013). "Recompute the checksum" turned off keeps the stored checksum
  nibble, and whether the sector then reads as good or bad depends on the new
  bytes, as the running XOR of 6-and-2 and 5-and-3 data gives (FR-098, User
  Story 9 scenario 3, SC-016). A flux cell's recorded time is defined
  (FR-111), and the `disk` command's and Explorer's reads find each track
  through the map as the writer does (FR-111, User Story 9 scenario 19,
  SC-017, Scope, Assumptions). File comparison covers Pascal and CP/M files
  and a disk with two volumes (FR-120, User Story 10, SC-021), and the Sector
  data header, "Go to" and sector pairs give the CP/M sector (FR-040, FR-055,
  FR-120). CP/M blocks 128 to 139 wrap to tracks 0 to 2 (FR-085). RW18
  findings for a failed address check value and for a valid nibble with the
  wrong value are defined (FR-144, User Story 15 scenarios 2 to 4, SC-028).
  Also corrected: the custom formats' place in classification and the disk's
  format (FR-016, FR-018, FR-141), the head limit in Explorer (FR-004), copies
  of image details (FR-063), the Image tab's added records (FR-050), the scope
  of the shared writer (FR-110, FR-113), the two changed-file states in Casso
  (FR-102, FR-104), the Sector data tab's controls (FR-007), and SC-017's
  commands, formats and failure steps.
- Choices made in this revision, none of them a [NEEDS CLARIFICATION] marker:
  a map entry that points at a track record with a zero start block or a zero
  block count is an image file problem, not damage, so the image stays
  writable, its quarter tracks show what the drive reads there, and the guest
  can format the track (FR-051, FR-053, Assumptions); 4-and-4 sectors are not
  editable (FR-096, FR-112, FR-124); "Save edited copy..." over an existing
  file the user confirms replacing uses the replacing commit (FR-116,
  Assumptions); the file comparison compares A's volume shown in the File map
  tab with B's volume of the same file system (FR-120); and a disk with RW18
  tracks gives RW18 in its format, after the standard kinds or alone (FR-018).
- "Edge cases are identified" was rechecked after the failure paths of editing
  were added to Edge Cases: a failed save, a save to a preserved copy, a pause
  while the drive writes or the head is inside an edited field, a guest write
  during the apply wait, redo after a guest write, quitting or changing the
  machine with pending edits, and an Explorer edit of an image Casso holds. It
  was rechecked again in the owner's revision for the head limit, the empty
  tracks Casso keeps for the guest to format, writes of the `disk` command and
  Explorer to standard-layout, non-254-volume, incompletely decoded and
  unformatted tracks, a save that fails partway, Pascal and CP/M volumes of
  other sizes, empty CP/M directories, the CP/M system entry and Pascal .BAD
  files, and in the later release RW18 track number mismatches, mixed standard
  and RW18 disks, and quarter tracks that share a record. In this revision it
  gained a map entry that points at a track record with a zero start block or
  a zero block count.
- Every value left to planning has a starting value in Assumptions: the data
  search window (48 nibbles), the track-length finding (±2%), the
  unformatted threshold (more than half a track), the flux timing tolerance
  for comparison (±1% of a cell), the nibble alignment limit (64 nibbles),
  and, in the later release, the bit-slip minimum run (6 nibbles), the
  "Solve table" limit (two swapped pairs), the "Trace boot" cycle limit (5 s
  of emulated time), the revolution alignment tolerance (one cell), the
  off-grid limit for weak bits (25% of a cell) and the share of matching
  nibbles that marks a quarter track as likely a read of its neighbor (75%).
- The Background's description of the Disk ][ debug window was corrected
  twice: it logs only the fields the guest reads with standard marks, which
  are address fields with a good checksum and data fields whose epilogue the
  guest reads, not every field that passes the head.
- The spec uses Disk II and WOZ terms (nibbles, sync, prologues and
  epilogues, address and data fields, 4-and-4, 6-and-2 and 5-and-3, quarter
  tracks, flux ticks, cell timing, TMAP, FLUX, TRKS, INFO, META, WRIT), DOS
  3.3, ProDOS, Apple Pascal and CP/M terms (VTOC, catalog, track/sector list,
  index and master index blocks, volume bitmap, Pascal directory, CP/M
  directory entries, extents, user numbers and allocation blocks), RW18 terms
  (pages, ID nibble) and capture terms (A2R, revolutions, latch framing,
  translate tables, weak bits). These describe the disks, file systems and
  file formats the feature inspects, which are its subject, not
  implementation choices. "Written for non-technical stakeholders" is checked
  on that basis: the readers are Apple II disk users, and the requirements
  hold no class, file, thread or rendering detail. Assumptions give the class
  names and threads of spec 041 and of today's sector writers where planning
  needs them, and nowhere else. Requirements on what Casso keeps and makes
  available to the inspector (FR-052, FR-053, FR-062, FR-063, FR-068, and the
  disk as inserted in FR-117) state the data the inspector needs, not how
  Casso stores it. The Assumptions on today's sector writers, on spec 041 and
  on the controller's one event listener describe existing or planned Casso
  behavior the feature has to work with, not choices this spec makes.
- Menu labels, tooltips and empty-state text are given in full because they
  are user-visible behavior; all are in sentence case.
- Figures taken from AppleEm (600× platter zoom, the 0.3 s Follow head delay,
  the ±5% default timing range, the 48-nibble data search window, and the
  980×660 window with its 640×460 minimum) are recorded as choices in
  Assumptions or the requirements.
- Scope lists every AppleEm feature left out or changed, with its reason. The
  comparison was repeated after the window layout (FR-007), the saved open
  state (FR-080), the data-search stop rule (FR-013), the empty-track text
  (FR-040), the "Nothing recorded" format (FR-018) and the read marks (FR-047,
  FR-031) were added; the 10-cell noise rule is listed as changed. Scope also
  lists the additions from other tools, which of them are in the later
  release, the two later-release features of the spec's own, the shared sector
  writer and its effect on the `disk` command and Explorer, and what stays out
  of scope. Delivery is two releases, each one merge: the first holds User
  Stories 1 to 10, with the shared sector writer and the Pascal and CP/M
  readers, and the later one User Stories 11 to 16. Priorities give build
  order only (analyzer, views, Casso host, then the map with its Pascal and
  CP/M readers, editing with the shared sector writer, and comparison, then
  the Explorer hosts and the parts that need 033, after master is merged into
  033 and 040 is rebased onto 033, and in the later release the preservation
  stories), and 040 merges to master only after 033 is on master. Delivery and
  Assumptions also give how 040 and spec 041 adapt to each other: whichever
  merges second adapts to the other.
