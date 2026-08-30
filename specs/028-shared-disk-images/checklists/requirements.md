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

### The judgment call, and a correction to it

FR-005 picks up an external change without BLOCKING when the guest has no
unsaved writes, and FR-008 always asks when it does. The build loop is the case
this feature exists to serve, it runs many times an hour, and a modal dialog on
each pass would make the feature worse than the bug.

**A first draft of this spec went further and said reboot should not be offered
at all**, on the grounds that it discards guest state and re-reading achieves
what the loop wants. That was wrong, and the owner caught it.

Replacing a mounted disk's contents IS a disk swap. Real machines allow it and
users did it constantly, but at a prompt with no files open, and that is
precisely the state the emulator cannot verify. The guest caches disk structure
in its own RAM: DOS 3.3 holds the VTOC, ProDOS a volume control block and an
open file's index blocks. Swap underneath and the guest's next WRITE allocates
against a map belonging to a disk that is gone.

The "no unsaved guest writes" gate does not cover this. It says only that the
guest has not written YET; a guest that has merely READ still holds cached
structure, and it is the write AFTER the pick-up that corrupts.

So FR-011 now requires reboot to be offered wherever a change is reported, and
FR-006 requires the report itself to carry it. Reboot is the only action that
makes the guest's cached structure match the bits on the disk, and the spec must
not imply the emulator has established anything safer. FR-007 adds that a
pick-up waits for a moment with no operation in flight, which the controller
already reports at motor spindown; that bounds the damage without eliminating
it, and is described as doing exactly that much.

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
