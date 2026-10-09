# Specification Quality Checklist: Disk integrity

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-10-08
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- This is a defect batch in an emulator, so the spec uses the user-visible
  terms the owner uses (image formats, `--disk1`, quarter tracks, the address
  sanitizer as the race oracle) and, in FR-022, lists the documents to correct. Those
  are the subject matter, not design choices; how each fix is built is left to
  the plan.
- The three decisions that would otherwise need clarification were settled by
  the owner on 2026-10-08: fix every confirmed defect in one branch, raise the
  head stop to track 39, and create a track on a write to a blank one. The
  merge order against 035 was settled the same day.
- Scope: the 25 defects still present on 035's code, plus FR-019 after the
  `master` merge. Four defects 035 already fixed are out of scope (Background).
- Three more decisions were settled by the owner on 2026-10-08, after
  planning: an update installed while the machine is behind live saves the
  disks at the live end (FR-019); real-file tests go to the scenario suite,
  with no constitution exception (SC-004); and the WOZ track-record rule is
  shared with spec 040 (FR-021).
- Three more were settled by the owner on 2026-10-09: the live-end save
  covers every update install path, the MSIX deploy, a zip update installed
  now and an update left until Casso closes (FR-019); every unit test that
  touches a real file moves to the scenario suite, not only the disk seam's
  (SC-004); and a WOZ record with a zero bit count and a start block of 3 or
  more is empty, not damage (FR-021), as spec 040's loader has it.
