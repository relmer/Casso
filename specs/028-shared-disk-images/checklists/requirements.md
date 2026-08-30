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

**A second revision went further still, and it is the shape the spec now has.**
Offering a reboot on every notice was better than hiding it, but it still had
the tool deciding the default and the user re-answering hourly. The owner's
resolution is to let the user declare the policy: take changes up in place,
restart the machine, or ask — remembered, and changeable mid-session.

That works because the question was never computable. The emulator cannot see
whether a swap is safe, but the developer knows which loop they are in: a binary
they will `BRUN` again wants the contents swapped and nothing else; a bootable
program, or a guest holding state a changed disk invalidates, wants a restart.

Writing it that way also separated two questions the earlier drafts had tangled:

- **How the guest takes up the new contents** (FR-005 to FR-011) — arises on
  every pick-up, including clean ones, and is what the policy governs.
- **What happens to the two versions** (FR-012 to FR-014) — arises only when the
  guest has written AND the file changed, and is a data-loss question. FR-012
  states explicitly that no setting turns it off, so a policy of "take changes up
  in place" can never be read as permission to discard the guest's work.

FR-011 keeps the motor-spindown requirement from the previous revision: a pick-up
waits for a point with no operation in flight, which bounds the damage without
eliminating it, and is described as doing exactly that much.

### Deliberately unresolved, for `/speckit-clarify` or `/speckit-plan`

- What a backup image is CALLED, and what happens when a second conflict would
  overwrite the first session's backup. FR-008 requires the backup and names no
  scheme.
- Whether the pick-up in FR-005 can happen mid-execution or must wait for a
  quiet moment. A disk changing under a running program has consequences the
  plan should weigh against the cost of waiting.
- Whether FR-015's hold is taken by the emulator for its whole mount or only
  around each write. The spec requires only that writers not interleave; the
  owner's sketch takes it around the write, which also keeps `disk put` working
  against a mounted image.
- **Whether the pick-up policy is one setting or one per drive.** A developer
  may hold a bootable disk in one drive and a data disk in another with
  genuinely different needs. The spec says "the user declares" without saying
  how many answers there are, because per-drive is a real want and a global
  setting is a much smaller thing to build.
- **Whether `CassoCli` can state the intent per invocation** rather than the
  emulator holding a standing answer — something like a flag saying "I just
  replaced the boot program, restart it". That is strictly more expressive,
  since intent is clearest at the moment of writing, but it needs a channel from
  the command line to a running emulator that does not exist today. If the
  writer-hold of FR-015 turns out to be a file beside the image, that file is an
  obvious place to carry the hint, and the plan should say whether it does.
