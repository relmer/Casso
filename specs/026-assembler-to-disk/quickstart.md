# Quickstart: Validating Assembler-to-Disk Output

**Feature**: `026-assembler-to-disk` | **Date**: 2026-08-29

How to prove this feature works, end to end. Scenarios map to the spec's user
stories; details of flags and directives are in [contracts/](contracts/) rather
than repeated here.

## Prerequisites

- A build of `CassoCli.exe`. Build through `Casso.sln`, not the `.vcxproj` — the
  latter drops the exe in a different directory and you end up testing a stale
  binary from the solution directory.
- The unit suite runs with `scripts\RunTests.ps1 -Build`. It does **not** build
  without `-Build`, and its staleness guard refuses a run against a test
  assembly older than the newest source that compiles into it.

```bash
scripts\Build.ps1
```

```bash
scripts\RunTests.ps1 -Build
```

Filtered runs during the edit-test loop:

```bash
scripts\RunTests.ps1 -Build -Filter AssemblerToDisk
```

A filtered run prints a banner saying it is not the suite. Never report a
filtered pass as a suite pass.

## Scenario 1 — Assemble onto a disk (User Story 1, P1)

The feature in one action, and the correctness win on its own.

```bash
CassoCli disk create prog.dsk --format prodos --volume WORK
```

Assemble a source whose origin is `$6000` straight onto it:

```bash
CassoCli as65 prog.a65 --disk prog.dsk --as PROG
```

Then read the volume back:

```bash
CassoCli disk list prog.dsk
```

**Expected**: `PROG` present, typed `BIN`, and its aux type `$6000` — taken from
the source's `ORG`, with no `--load` anywhere on the command line. That last
point is the whole of SC-002: there was no opportunity to state a conflicting
address.

Confirm the other artifacts stayed on the host (FR-004):

```bash
CassoCli as65 prog.a65 --disk prog.dsk --as PROG -lprog.lst -g
```

**Expected**: `prog.lst` and the debug file are host files. `disk list` shows
only `PROG` on the volume.

## Scenario 2 — A failed assembly changes nothing (FR-014, SC-005)

The guarantee that makes the rest safe. Record the image's bytes, assemble a
source with a deliberate error, and compare.

**Expected**: the assembly fails, exit status 2, and the image is byte-for-byte
identical. Not "mostly unchanged" — compare the whole file.

Repeat with the image open in a running emulator.

**Expected**: refused, naming the image, and unchanged. `DiskImageSession`
already words this one.

## Scenario 3 — Merlin source that names its own output (User Story 2, P2)

A source carrying `DSK PROG` and `TYP $06`, assembled with no naming flags:

```bash
CassoCli merlin PROG.S --disk prog.dsk
```

**Expected**: `PROG` on the volume, typed binary, from the source alone.

Then confirm the flag wins (FR-007):

```bash
CassoCli merlin PROG.S --disk prog.dsk --as OTHER
```

**Expected**: `OTHER`, not `PROG`. The directive supplied a default and the
command line overrode it.

Then the refusal that must not become an approximation (FR-010) — a source with
`TYP $FF` against a **DOS 3.3** volume:

**Expected**: refused, naming both the type and the filesystem, image unchanged.
Not filed under some nearby DOS 3.3 type.

## Scenario 4 — Several files from one source (User Story 3, P3)

A source with two `ORG`/`SAV` sequences:

```bash
CassoCli merlin TWO.S --disk prog.dsk
```

**Expected**: both files on the volume, each with its own load address, and
**the second holding only the bytes assembled after the first save**. That last
clause is the one that discriminates: a cumulative implementation puts the first
file's bytes inside the second and otherwise looks correct.

The same source with no image target:

```bash
CassoCli merlin TWO.S
```

**Expected**: two host files. `SAV` is not refused for want of a disk (FR-020).

Then break the source after the first `SAV`:

**Expected**: neither file exists, on the volume or on the host. The image is
byte-for-byte unchanged.

## Scenario 5 — A disk that boots what was assembled (User Story 4, P3)

```bash
CassoCli disk create boot.dsk --format prodos --volume WORK --bootable
```

```bash
CassoCli as65 prog.a65 --disk boot.dsk --as PROG.SYSTEM --type SYS --startup
```

**Expected**: the volume records `PROG.SYSTEM` as its startup program. Boot the
image in the emulator and it runs — this scenario is only really validated by
booting it:

```bash
Casso.exe --disk1 boot.dsk
```

Then the refusals: `--startup` with no `--disk` (FR-023), and `--startup` naming
a file the volume's operating system would not actually run (FR-022).

**Expected**: refused. The DOS 3.3 case is the interesting one — a booting
DOS 3.3 `RUN`s its greeting rather than `BRUN`ing it, so a binary named as the
greeting leaves the disk booting and the program silently never running. The
same rule that catches this for `disk boot` must catch it here, because it is
the *same rule*, shared and not copied.

## Scenario 6 — Nothing regressed (FR-016, SC-006)

```bash
CassoCli as65 prog.a65 -oprog.bin
```

**Expected**: byte-for-byte the same object as before this feature. Assembling
without an image target is untouched.

## Pre-merge gates

This feature changes assembler output paths, so both extended suites run before
merging to `master`:

```bash
scripts\RunDormannTest.ps1
```

```bash
scripts\RunHarteTests.ps1 -SkipGenerate
```

The checked-in 200-vector Harte depth is correct here. Full depth is for CPU and
instruction-set changes, and this feature touches neither. The runner prints
which depth it ran; take that from its output rather than assuming.

Also required before merge: `scripts\Build.ps1 -RunCodeAnalysis` with zero
warnings, and the full suite green in **Debug** — Release runs a different set
and verifies no assertion behavior, so it is not a substitute for the gate.

## A note on writing these tests

The all-or-nothing guarantee is structural: `IVolume` computes a complete buffer
or none. That means a test for Scenario 2 or the failure half of Scenario 4 can
pass without the feature being correct.

Mutate the implementation and confirm the test notices — commit after each save
point instead of once at the end, and the test must go red. A test that stays
green under that mutation is measuring nothing, and would be indistinguishable
in the output from one that works.
