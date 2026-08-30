# Implementation Plan: Disk Images Shared with a Running Emulator

**Branch**: `028-shared-disk-images` | **Date**: 2026-08-30 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/028-shared-disk-images/spec.md`

## Summary

The emulator reads a disk image at mount and closes the file. It records nothing
about what it read, so it cannot tell that anything changed, and it later writes
its own copy over whatever is there. This adds the missing half: an identity
recorded at mount, a watcher that notices a change, a decision about what to do
about it, and a path for a writing tool to say what it meant.

**Most of the writing-safety story turned out to be already delivered**, which
is why the plan is smaller than the spec's three stories suggest. Both sides
already commit through a temporary and an atomic rename, so a partly written
image cannot be read. What is missing there is two narrow defects, not a locking
layer.

## Technical Context

**Language/Version**: C++ (`/std:c++latest`, MSVC v145 / VS2026)
**Primary Dependencies**: none new. Win32 for the watcher and the message; Dxui
for the banner; all already in the tree.
**Storage**: disk image files on the host filesystem; the preference in the
existing user config store.
**Testing**: `UnitTest` (VSTest), driven through the existing `IDiskFileIo` seam
and a new seam for watching and for messaging, so no test touches a real file or
a real window.
**Target Platform**: Windows desktop (the emulator shell); the CLI is the same
binary family.
**Project Type**: single solution, five projects. Logic lands in `CassoEmuCore`;
`Casso` and `CassoCli` get thin platform shims.
**Performance Goals**: no measurable effect on the emulator's frame rate or
audio (SC-006). The watcher is event-driven and idle when nothing changes.
**Constraints**: the pick-up must land at a point with no disk operation in
flight (FR-014); the CLI must behave identically whether or not an emulator is
running (FR-015).
**Scale/Scope**: two drives per machine, a handful of mounted images, one
emulator in the common case and a small number in the worst.

## Constitution Check

| Principle | How this complies |
|---|---|
| I. Code Quality | New types follow the existing header-comment convention. The two defects fixed in Story 3 are named in comments as defects with their failure modes, per the tree's practice. |
| II. Testing Discipline | Every decision is in `CassoEmuCore` behind seams. Watching, messaging and clock are all injected, so the whole feature is unit-testable without a file, a window or a wait. |
| III. UX Consistency | The banner uses the existing `DxuiInfoBanner`. Diagnostics follow `DiskCommandResult::Failure`'s wording, which every disk refusal already uses. |
| IV. Performance | The watcher is event-driven; no polling loop. The pick-up runs on the existing motor-spindown hook, which already exists for flushing and costs nothing. |
| V. Simplicity | No lock layer, no sidecar files, no daemon. The atomic-rename guarantee already in the tree is relied on rather than duplicated. |
| VI. Thin Exe, Testable Core (NON-NEGOTIABLE) | `IImageWatcher` and `IIntentChannel` are interfaces in core, mirroring `IDiskFileIo`. `Win32ImageWatcher` and `Win32IntentChannel` are shims in the shell. **The first task list violated this** by putting conflict resolution, the backup-failure refusal, report absorption and the watcher-degrade rule in `DiskManager.cpp` and `GlobalUserPrefs.cpp`, both compiled into `Casso.exe`. They now live in `ExternalChangePolicy` and `MountedImageState`; the shell presents what core decided, and the preference file only stores a value. |

**Gate result**: PASS, after a correction. The first pass through this table
recorded Principle VI as compliant while the task list placed seven decisions
inside `Casso.exe`. `/speckit-analyze` caught it. The lesson worth keeping: a
Constitution Check written from the plan's INTENTIONS is not a check -- it has
to be read against the tasks that will actually be written.

## Key Decisions

### The channel is best-effort, and that is what makes it cheap

**Detection and intent are separate concerns.** FR-002 must catch a change from
ANY writer -- a text editor, a second emulator, a script using `disk put` -- so
a watcher is required regardless. The channel carries only the intent, which is
optional metadata riding on a change that will be noticed anyway.

That means a dropped message degrades to the fallback of FR-007, which is
correct behavior rather than a failure. A channel allowed to be lossy needs no
delivery guarantee, no acknowledgement and no cleanup.

**Decision**: a broadcast window message carrying the image path and the intent.
**Rationale**: no files left on disk, so no staleness rule and no cleanup on
crash; no process discovery, since a broadcast reaches every emulator and each
ignores paths it has not mounted; and the tree already routes commands this way.
**Alternatives**: a sidecar file beside the image was attractive while the plan
still needed a lock -- one mechanism for both -- but the lock turned out to be
unnecessary, and a sidecar alone buys nothing while adding staleness and cleanup.
A named pipe is more machinery than lossy metadata warrants and needs instance
discovery.

### The pick-up rides the motor-spindown hook that already exists

`Disk2Controller::SetMotorOffFlushCallback` fires on the CPU thread at the exact
moment the motor spins down -- after an operation completes and about a second
after the last access. Its own comment calls it "a naturally debounced,
race-free point to persist dirty images". It is the same point FR-014 asks for,
and it is already wired to `DiskImageStore::FlushAll`.

**Decision**: apply a pending pick-up from that callback.
**Rationale**: no new thread, no new synchronization, and the debouncing FR-013
wants is partly free. **Alternatives**: applying from the watcher's own thread
would need locking against the CPU thread and could land mid-operation.

### What is NOT being built

- **No lock.** Both sides already commit through a temporary and an atomic
  rename. A reader sees the old file or the new one. FR-024 states that as a
  requirement so abandoning it becomes a regression, and adds nothing.
- **No merge.** A conflict resolves to one surviving image and one backup.
- **No polling.** The watcher is event-driven, and the write-time re-check of
  FR-003 is what makes the guarantee hold if a notification is ever missed.

## Project Structure

### Documentation (this feature)

```text
specs/028-shared-disk-images/
├── spec.md
├── plan.md              # this file
├── research.md          # Phase 0
├── data-model.md        # Phase 1
├── contracts/
│   ├── cli.md           # the intent flag
│   └── channel.md       # the message shape
├── quickstart.md        # Phase 1
└── checklists/requirements.md
```

### Source Code

```text
CassoEmuCore/Devices/Disk/
├── ImageIdentity.h/.cpp        # NEW: what is compared to answer "changed?"
├── IImageWatcher.h             # NEW: seam, notice a file changing
├── MountedImageState.h/.cpp    # NEW: per-mount identity + pending change
├── ExternalChangePolicy.h/.cpp # NEW: intent, fallback, what to do
├── DiskImageStore.h/.cpp       # records identity at mount; re-checks before
│                               # write; unique temp name (FR-025); stamp (FR-026)
└── CommitPlan.cpp              # correct as it stands; may gain a caller

CassoEmuCore/Cli/
├── IIntentChannel.h            # NEW: seam, state an intent
├── ImageArtifactSink.cpp       # states the intent after a successful commit
    (DiskCommandRunner.cpp lives under Devices/Disk/, not here)

CassoCore/
└── CommandLineOptions.h        # NEW field: the stated intent
    CommandLineParser.cpp       # NEW flag row, both dialects + `disk`

Casso/Shell/
├── Win32ImageWatcher.h/.cpp    # NEW: ReadDirectoryChangesW shim
├── Win32IntentChannel.h/.cpp   # NEW: WM_COPYDATA send/receive shim
├── DiskManager.cpp             # wires watcher to store; owns the banner
└── MachineManager.cpp          # motor-spindown callback also applies pick-ups

Casso/Config/
└── GlobalUserPrefs.h/.cpp      # NEW: the fallback answer (FR-007)

UnitTest/
├── ImageIdentityTests.cpp      # NEW
├── ExternalChangePolicyTests.cpp # NEW
├── SharedImageTests.cpp        # NEW: end to end through the seams
├── EmuTests/FakeImageWatcher.h # NEW: drive a change without a file
└── EmuTests/FakeIntentChannel.h# NEW: record an intent without a window
```

**Structure Decision**: single project, matching the tree. Everything that
decides anything goes in `CassoEmuCore` where `UnitTest` links it; `Casso` gets
two shims and the banner; `CassoCli` gets a flag and one call.

## Phasing

The stories are independently shippable and the order is the spec's priority.

- **Phase A (US1)** — identity at mount, watcher seam, pick-up on spindown,
  banner. Delivers the build loop. The fallback answer covers intent until
  Phase C adds the flag.
- **Phase B (US2)** — dirty-image conflict, the timestamped backup, the refusal
  when a backup cannot be written. Delivers the guarantee that nothing is lost.
- **Phase C (US1 completion)** — the CLI flag and the channel, so intent comes
  from the writer rather than the standing answer.
- **Phase D (US3)** — the unique temporary name and the emulator-side stamp.
  Independent of A–C and could go first; it is last because it is the narrowest.

**Phase D is separable enough to land on its own.** Both halves are defects in
today's behavior, not new capability, and neither needs the watcher.

## Complexity Tracking

No constitutional violations. Two notes on where complexity was deliberately
declined:

| Considered | Declined because |
|---|---|
| A lock file coordinating writers | The atomic-rename guarantee already prevents the corruption it would prevent. It would add staleness, cleanup-on-crash and a new failure mode for nothing. |
| An acknowledged, reliable channel | The intent is optional metadata on a change the watcher sees anyway, so loss degrades to the documented fallback. Reliability would cost discovery, retries and a handshake to buy an outcome the fallback already produces. |
