# Specification Quality Checklist: Per-field CRT user overrides

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-09-03
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

Validation ran over three iterations. What changed:

**Iteration 1.** Implementation detail had leaked into six places. Type and
member names (`CrtOverrides`, `userOverride`, `crtByMode`), a JSON key name, a
file path and a function name were all named in requirements. Rewritten to state
the behavior instead. FR-013 is the one requirement that still constrains
structure, and deliberately so: it says the rules live in one testable place
without naming that place, because the duplication it forbids has already
shipped two defects and a spec that cannot forbid it is not describing the
problem being solved.

**Iteration 2.** Three success criteria were not measurable as written.
"Migration is lossless" became SC-003, which names what is compared and against
what. "Old builds still work" became SC-005. "Resolution is testable" became
SC-008 plus SC-009, separating the coverage claim from the per-frame cost claim,
which are verified differently.

**Iteration 3.** Two edge cases were added that the reviews had surfaced but the
spec had not carried: a hand-edited settings file whose override section is
present but not an object, and drawing a frame during a machine switch. Both are
reachable and both change required behavior, so neither belongs only in the
plan.

**No [NEEDS CLARIFICATION] markers were needed.** Every question this feature
raised was already settled by the owner before the spec was written: per-field
rather than per-group granularity, the monitor and mode pair as the storage key,
dropping the legacy settings block at upgrade, fanning an upgraded adjustment
onto both monitors in the old catalog, freezing shipped monitor identifiers, and
leaving the settings file version number alone. Those decisions are recorded in
the Assumptions section where they constrain scope, and their reasoning belongs
in research.md rather than here.

**One dependency is outside this feature's control** and is recorded as an
assumption rather than a requirement: a settings save must not overwrite the
global section with defaults. That fix is owned by another work item. If it does
not land, the upgrade this feature performs can be silently reverted, so
planning must sequence around it.
