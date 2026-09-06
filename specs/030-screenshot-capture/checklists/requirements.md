# Specification Quality Checklist: Screenshot capture modes, file output, and metadata

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-09-05
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

- The design was settled in discussion before the spec was written, so no
  [NEEDS CLARIFICATION] markers were needed and `/speckit-clarify` can be skipped.
- Two deliberate judgment calls were recorded as assumptions rather than raised as
  clarifications, because a reasonable default existed for each: `scene` mode under a
  theme that draws no desk scene captures the viewport anyway rather than silently
  switching modes, and a minimized window refuses `scene` / `crt` rather than writing a
  black or stale image.
- Implementation-shaped decisions settled in the same discussion -- how the rendered
  image is recovered, which existing components are generalized, and why a one-shot
  render was rejected in favor of reading the live picture -- were deliberately kept out
  of the spec and belong in `plan.md`.
- FR-020 no longer restates the metadata table. It references
  `contracts/screenshot-metadata.md`, which is the single authority: a published contract
  where entries may be added later but never renamed or repurposed, so two copies of it
  were the obvious place for drift.
- `/speckit-analyze` was run on 2026-09-05 after tasks generation and raised 13 findings
  (0 critical, 1 high, 6 medium, 6 low). All 13 were resolved in spec.md, plan.md,
  data-model.md and tasks.md. The high finding was a self-contradiction in the
  `CaptureSource` table over whether `Scene` reads the picture target or the back buffer;
  it is now stated as three mutually exclusive, exhaustive conditions with the reasoning
  spelled out.
