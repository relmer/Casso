# Quickstart: Validating Shared Disk Images

**Feature**: `028-shared-disk-images` | **Date**: 2026-08-30

How to prove this works, end to end. Scenarios map to the spec's user stories.
Flag and message details are in [contracts/](contracts/) rather than repeated.

**Every scenario here needs two processes.** That is the whole subject, and it
is also why the unit suite cannot cover it alone: the seams let `UnitTest` drive
every decision, but only a real emulator beside a real `CassoCli` proves the
wiring.

## Prerequisites

```bash
scripts\Build.ps1
```

```bash
scripts\RunTests.ps1 -Build
```

Have a disk with something runnable on it, and a source that rebuilds it.

**Most commands below are written with the assembler, and the assembler cannot
target an image until spec 026 lands.** Until then, substitute `disk put`, which
targets a mounted image today and exercises exactly the same machinery -- the
watcher, the policy, the coalescing and the channel do not care which tool wrote
the file. Where a scenario turns on assembler behavior specifically, it says so.

## Scenario 1 — The build loop (User Story 1, P1)

Mount a disk, leave the machine at a prompt, and write onto it. Buildable
today:

```bash
CassoCli disk put work.dsk prog.bin --as PROG --on-change reload
```

Or, once spec 026 has landed, from an assembly directly:

```bash
CassoCli as65 prog.a65 --disk work.dsk --as PROG --on-change reload
```

**Expected**: within a second or two the emulator picks the change up and shows
a banner saying so, carrying a Restart action. `BRUN PROG` in the guest runs the
version just built, with no eject and no re-insert.

**The eject-and-reinsert is the control.** Before this feature, the only way to
see the new bytes was to eject and re-insert by hand. If that is still required,
nothing here is working, however good the banner looks.

Then the other intent:

```bash
CassoCli as65 boot.a65 --disk work.dsk --as PROG.SYSTEM --type SYS --startup --on-change restart
```

**Expected**: the machine restarts and boots what was just built.

Then no intent at all — edit `work.dsk` with any other tool, or:

```bash
CassoCli disk put work.dsk notes.txt --as NOTES
```

**Expected**: the emulator asks -- "work.dsk in Drive 1 was modified externally"
-- offering `Accept the changes` and `Ignore the changes`, and saying why a
reboot may be needed. There is no setting that suppresses this, deliberately
(FR-007). This is the path every writer that is not `CassoCli` takes, so it must
not be an afterthought.

## Scenario 2 — Nothing is lost (User Story 2, P1)

Have the guest write to the disk (save a file from Applesoft), THEN rebuild onto
it from outside.

**Expected**: a conflict question, and nothing written until it is answered.
Take the external version.

**Expected**: the guest's writes are preserved in a timestamped image beside the
original, and the user is told the path. Confirm the backup actually contains
the guest's file — a backup nobody reads back is a backup nobody has tested.

Repeat and keep the guest's version instead.

**Expected**: the external change is preserved the same way, and the disk keeps
what the guest wrote.

Then the case that must NOT prompt: guest writes, no external change, eject.

**Expected**: written directly, no backup, no question. Byte for byte what
today's build produces (SC-003).

### The refusal

Make the directory read-only and force a conflict.

**Expected**: the discarding action does not proceed, both versions stay live,
and another location is offered. **Not** a report followed by the loss anyway —
that is the one outcome FR-024 exists to prevent.

## Scenario 3 — Two writers (User Story 3, P2)

Run two emulator instances on the same image and make both flush.

**Expected**: one whole version, neither mixed nor silently substituted. Before
the fix both derive `<path>.casso-tmp` and one commits the other's bytes as its
own, so this scenario FAILS on today's build — which is what makes it worth
running.

Then have the emulator commit over a change it never saw.

**Expected**: detected rather than silently winning.

## Scenario 4 — The unusable image

With a disk mounted, delete it. Or copy a `.woz` over a mounted `.dsk`.

**Expected**: the change is refused, the machine carries on with what it holds,
and the emulator offers to save that copy — because with the file gone it may be
the only copy of the disk left. Decline, and the machine keeps running with the
disk still mounted.

## Scenario 5 — The burst (FR-013)

Run a multi-command build against one mounted disk:

```bash
CassoCli as65 loader.a65 --disk game.dsk --as LOADER --on-change restart
```
```bash
CassoCli as65 main.a65 --disk game.dsk --as MAIN --on-change restart
```
```bash
CassoCli disk put game.dsk levels.dat --as LEVELS --on-change restart
```

**Expected**: the machine restarts ONCE, after the last write, booting a disk
that has all three on it. Not three restarts, and above all not a restart that
begins while the second command is still writing.

Then walk away: run three builds without touching the emulator.

**Expected**: one banner, not three, and acting on it uses the newest contents.

## Scenario 6 — Nothing regressed

```bash
CassoCli as65 prog.a65 -oprog.bin
```

**Expected**: byte for byte the object produced before this feature. No image is
involved and nothing here should touch that path.

Run a whole session with no external change at all.

**Expected**: identical to today in what is written and when, and no measurable
effect on frame rate or audio (SC-006). Measure it rather than assuming it —
a watcher that wakes on every write in a directory is easy to build by accident.

## What a passing run does NOT prove

The guest's cached disk structure is invisible from here. A pick-up that looks
clean can still corrupt the disk on the guest's NEXT write, because DOS 3.3
holds the VTOC and ProDOS a volume control block from contents that are gone.
That is why a restart is always offered and why the spec refuses to describe any
pick-up as safe. **Do not add a scenario claiming to have verified that it is.**
