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
  track or a sector to a file is in this spec, and in which forms. FR-077:
  how the inspector window in Casso gets a light theme, so that the parts
  specific to Casso can be checked in light as well as dark; Casso's own
  themes are all dark, and 033 adds Light and Dark to Explorer only. The four
  unchecked items fail on these two alone: export has no acceptance scenario
  until FR-058 is answered, and the light check of the Casso-only parts in
  SC-008 depends on FR-077's answer. Copy to the clipboard (FR-057)
  is decided and does not depend on either answer.
- The review issues that made other requirements ambiguous or contradictory
  are fixed in this revision: the quarter-track and sharing findings that
  fired on every ordinary disk (FR-048, SC-013), noise against random-bit
  regions and the reference turn (FR-009, FR-010, FR-008, SC-002), the
  disk's format rule (FR-018), the order of classification (FR-016), the
  nominal track length (Assumptions), field pairing (FR-013), the first
  sector selected (FR-039), the histogram's scope (FR-045), the hosts' common
  and specific parts (FR-001), the whole-disk analysis cases (FR-068), the
  pass and fail tests for small features, drags and legibility (FR-023,
  FR-025, FR-026, FR-035), and the measurement methods in SC-004, SC-005 and
  SC-008. The 033 merge order is stated in Scope under Delivery.
- The spec uses Disk II and WOZ terms (nibbles, sync, prologues and
  epilogues, address and data fields, 4-and-4, 6-and-2 and 5-and-3, quarter
  tracks, flux ticks, cell timing, TMAP, FLUX, TRKS, INFO, META, WRIT). These
  describe the disks and the file format the feature inspects, which are its
  subject, not implementation choices. "Written for non-technical
  stakeholders" is checked on that basis: the readers are Apple II disk
  users, and the spec holds no class, file, thread or rendering detail.
  Requirements on what Casso keeps and makes available to the inspector
  (FR-052, FR-053, FR-062, FR-067) state the data the inspector needs, not
  how Casso stores it.
- Menu labels, tooltips and empty-state text are given in full because they
  are user-visible behavior; all are in sentence case.
- Figures taken from AppleEm (600× platter zoom, the 0.3 s Follow head delay,
  the ±5% default timing range, the 48-nibble data search window, and the
  980×660 window with its 640×460 minimum) are recorded as choices in
  Assumptions or the requirements. Planning may tune the search window, the
  ±2% track-length finding and the unformatted threshold.
- Scope lists every AppleEm feature left out or changed, with its reason.
  The comparison was repeated after the window layout (FR-007), the saved
  open state (FR-079), the data-search stop rule (FR-013), the empty-track
  text (FR-040), the "Nothing recorded" format (FR-018) and the read marks
  (FR-047, FR-031) were added; the 10-cell noise rule is listed as changed.
  Scope also lists the additions from other tools and the features deferred
  to a future spec. Delivery is one merge; priorities give build order only
  (analyzer, views, Casso host, then the Explorer hosts after 033 merges
  master and 040 is rebased onto 033), and 040 merges to master only after
  033 is on master.
