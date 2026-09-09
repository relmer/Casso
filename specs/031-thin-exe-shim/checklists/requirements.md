# Specification Quality Checklist: Thin Executable, Testable Core — `Casso.exe`

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-09-09
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

Three items warrant a note, since this is an internal engineering specification
rather than an end-user feature and two of the checklist items land differently
here than they would on a normal feature.

- **"No implementation details" and "written for non-technical stakeholders"**
  are satisfied in substance rather than literally. The specification names
  projects, directories and modules because those are the *subject* of the work,
  not the means of doing it: a specification about where code lives cannot avoid
  saying where code lives. What it deliberately does not do is prescribe class
  designs, seam signatures, or the order of operations inside a slice, all of
  which are left to `/speckit-plan`. The audience is a maintainer, which is the
  correct stakeholder for a Principle VI compliance effort.

- **The line counts are measured, not estimated.** Every figure in the Context
  section was counted on this branch at branch point, excluding build output and
  vendored `External/`. SC-001's floor is derived from the measured size of the
  code that must stay behind, so the target is checkable rather than aspirational.

- **The first draft of this specification broke the principle it specifies.** It
  measured the exe by counting which files mentioned a window handle, a graphics
  type or an audio interface, and it enumerated those same APIs as the
  "irreducible edge" that stays behind. That is the platform-boundary question
  Principle VI forbids, reintroduced inside the document meant to enforce it,
  and it was wrong on the facts as well: a graphics device on a software adapter
  is drivable by a test, so the renderer and post-process chain are core work.
  The count is gone, FR-003 is stated in terms of what a test could drive,
  FR-004 bans naming a platform API as a justification anywhere, SC-001's target
  dropped from 6,000 lines to 1,500 once the conceded floor went with it, and
  User Story 7 exists to extract the code the first draft would have exempted.
  Recorded here rather than fixed silently, because the same mistake is what
  constitution 1.10.0 was written to correct.

- **The second draft still conceded too much, and `TCDir` settled it.** Having
  removed the API-token reasoning, the draft still granted the exe a list of
  things that stay — the pump, the devices, the dialogs — and merely justified
  the list differently. The owner's rule is that the exe holds one function at
  most, and `TCDir` in the same source tree goes further: its exe project holds
  *zero* code, `wmain` lives in `TCDirCore`, and the linker pulls it back out
  because the project names `wmainCRTStartup` as its entry point symbol. That is
  now FR-003 and FR-003a, and it removes the last surface an exemption argument
  could attach to. User Story 8 exists to reach it, and SC-002 is a count of
  functions in the exe, expected zero — a check that can actually fail.

- **The slice ordering is a default and says so.** FR-005 binds each slice to
  being independently shippable; the P1 → P3 ordering is risk-and-dependency
  reasoning that planning may revise. This is recorded in Assumptions so a later
  reader does not mistake the ordering for a commitment.
