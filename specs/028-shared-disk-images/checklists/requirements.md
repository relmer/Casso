# Specification Quality Checklist: Disk Images Shared with a Running Emulator

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-08-30
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

### Where the line was drawn on implementation detail

The Overview names `boot.dsk`, a byte count and the fact that the emulator holds
no OS handle. That is EVIDENCE, not design: it is the measurement that shows the
problem is real and states what was actually run, so a reader can reproduce it
rather than take the problem on trust. The requirements themselves name no file,
no class and no mechanism.

The owner's desired shape — a lock held during the write, a mount-time timestamp,
a change notification — is deliberately NOT written into the requirements as
those mechanisms. FR-001 asks for "enough to decide whether it has changed",
FR-002 for prompt detection, FR-010 for non-interleaving. Those are the outcomes
the owner's design achieves, and stating them as outcomes leaves the plan free to
confirm the design or find that the platform makes a different one better, while
still being testable.

### The one judgment call worth re-reading

FR-005 picks up an external change WITHOUT asking when the guest has no unsaved
writes, and FR-007 always asks when it does. The owner's sketch put a prompt on
the external change generally, offering reboot or remount.

The narrowing is deliberate and is argued in the Assumptions: the build loop is
the case this feature exists to serve, it runs many times an hour, and a dialog
on each pass would make the feature worse than the bug. Nothing is at risk in
that case, because the emulator writes back only when dirty.

Reboot is not offered as an option anywhere. It discards guest state the user may
care about, and re-reading the disk achieves what the build loop wants without
it. If a title needs a reboot to notice a changed disk, the user can reboot; the
tool should not do it for them.

### Deliberately unresolved, for `/speckit-clarify` or `/speckit-plan`

- What a backup image is CALLED, and what happens when a second conflict would
  overwrite the first session's backup. FR-008 requires the backup and names no
  scheme.
- Whether the pick-up in FR-005 can happen mid-execution or must wait for a
  quiet moment. A disk changing under a running program has consequences the
  plan should weigh against the cost of waiting.
- Whether FR-010's hold is taken by the emulator for its whole mount or only
  around each write. The spec requires only that writers not interleave; the
  owner's sketch takes it around the write, which also keeps `disk put` working
  against a mounted image.
