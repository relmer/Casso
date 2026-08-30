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

The requirements are grouped, and the groups are the shape of the feature:

| Group | Requirements | What it settles |
|---|---|---|
| Noticing a change | FR-001 – FR-004 | That an external change is seen at all |
| How the guest takes up the new contents | FR-005 – FR-015 | Stated intent, the fallback, coalescing, reporting |
| When the new contents cannot be used | FR-016 – FR-018 | Deleted, corrupt, wrong geometry |
| What happens to the two versions | FR-019 – FR-023 | The data-loss question and the preserved copy |
| Writing safely | FR-024 – FR-027 | Two writers cannot interleave |
| Not making things worse | FR-028 – FR-030 | No regression, no felt cost |
| Saying so | FR-031 – FR-032 | Every message names its image |

### Where the line was drawn on implementation detail

The Overview names `boot.dsk`, a byte count and the fact that the emulator holds
no OS handle. That is EVIDENCE, not design: the measurement showing the problem
is real, stated so a reader can reproduce it rather than take it on trust. The
requirements themselves name no file, no class and no mechanism.

The owner's desired shape — a lock held during the write, a mount-time timestamp,
a change notification — is deliberately NOT written into the requirements as
those mechanisms. FR-001 asks for "enough to decide whether it has changed",
FR-002 for prompt detection, FR-024 for non-interleaving. Those are the outcomes
the owner's design achieves, and stating them as outcomes leaves the plan free to
confirm that design or find the platform makes a different one better, while
still being testable.

### Two corrections the owner made, recorded so they are not re-opened

**The first draft said reboot should not be offered at all**, on the grounds that
it discards guest state and re-reading is what the build loop wants. That was
wrong.

Replacing a mounted disk's contents IS a disk swap. Real machines allow it and
users did it constantly, but at a prompt with no files open — precisely the state
the emulator cannot verify. The guest caches disk structure in its own RAM:
DOS 3.3 holds the VTOC, ProDOS a volume control block and an open file's index
blocks. Swap underneath and the guest's next WRITE allocates against a map
belonging to a disk that is gone. A "no unsaved guest writes" gate does not cover
it: that says only the guest has not written YET, and it is the write AFTER the
pick-up that corrupts.

**The second correction replaced a standing emulator preference with intent
stated by the writer.** The question was never computable by the emulator, but it
is obvious to whoever is writing the image — they know whether they replaced a
binary to be run again or the program the disk boots. So the writing tool states
it (FR-005) and it attaches to the IMAGE rather than the drive (FR-006), which is
why there is no per-drive setting to design. The emulator keeps only a fallback
(FR-007), for changes made by something that cannot state an intent.

That separation also untangled two questions the early drafts had confused: how
the guest takes up new contents (FR-005 – FR-015) arises on every pick-up, while
what happens to the two versions (FR-019 – FR-023) arises only when the guest has
written AND the file changed. FR-019 states that no setting turns the second off,
so an intent of "take changes up in place" can never be read as permission to
discard the guest's work.

### Clarification session 2026-08-30

Five questions, all answered, all integrated:

1. **Backup naming** — timestamped beside the original (FR-021). A timestamp
   cannot collide across repeated conflicts in one session.
2. **Change bursts** — coalesce on a quiet period (FR-013). Grounded in what a
   build actually does: a disk carrying more than one thing is built by more than
   one command, and acting per command restarts the machine while the script is
   still writing.
3. **Unusable new contents** — refuse and keep running, then offer to save the
   in-memory disk (FR-016 – FR-018). The owner sharpened this: if the file is
   gone, what the emulator holds may be the only copy left, dirty or not.
4. **How a pick-up is reported** — a non-modal banner carrying Restart
   (FR-010 – FR-012). The owner added that it must absorb further changes while
   it stands and act on the MOST RECENT contents: a developer may run three
   builds before turning back to the emulator.
5. **Backup cannot be written** — refuse the discarding action, keep both live,
   offer another location (FR-023). A backup that silently did not happen breaks
   the promise exactly where it matters most.

### Left for `/speckit-plan`

- **The channel from a writing tool to a running emulator does not exist today.**
  FR-005 requires the intent to be stated and FR-024 requires a writer to hold the
  image; if the hold turns out to be a file beside the image, that file is the
  obvious place to carry the intent. One mechanism serving both would be
  considerably smaller than two, and the plan should say whether it does.
- **How long the quiet period of FR-013 is**, and whether it is fixed or derived
  from how long writers actually hold the image.
- **Whether the hold of FR-024 is taken for the whole mount or around each
  write.** The spec requires only that writers not interleave. It must be around
  each write in practice, or nothing could ever write a mounted image, but the
  spec deliberately does not encode that.
