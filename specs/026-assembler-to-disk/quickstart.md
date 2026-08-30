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

**Expected**: the assembly fails, **exit status 3**, and the image is
byte-for-byte identical. Not "mostly unchanged" — compare the whole file.

This said exit 2, and 2 is the wrong number: it is the file-open failure. A
source that will not assemble is 3, which is what the tool returns and what the
help page documents. Walked and confirmed: 0 of 143,360 bytes differ.

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
`TYP $FF` against a **DOS 3.3** volume. That needs a DOS 3.3 disk, which every
other scenario here does not use:

```bash
CassoCli disk create dos.dsk --format dos33 --volume 254
```

```bash
CassoCli merlin SYSPROG.S --disk dos.dsk
```

**Expected**: refused, naming both the type and the filesystem, image unchanged.
Not filed under some nearby DOS 3.3 type. Run the same source against the ProDOS
disk to confirm it is accepted there — a refusal that fires on both volumes is
refusing for the wrong reason.

**This is the step that found the feature's one shipped defect.** The refusal
happened and said nothing: exit 2, image untouched, not one word printed. The
sink carries its diagnostics rather than printing them, which is what lets a
test read them, and nothing on the calling side read them back — so every
refusal on this path was silent, including no image, a full volume, a locked
file and an illegal name. The unit tests could not see it, because what they
assert is that the sink PRODUCES the text. Only running the tool did.

**On ProDOS, a `SYS` file lists with `aux=$0000` and that is correct.** ProDOS
puts the load address in the auxiliary type for `BIN` only; a `SYS` file loads
at `$2000` by definition and its aux field means nothing. A `BIN` output at the
same origin lists `aux=$2000`, which is the comparison that settles it. Do not
"fix" this.

Then `TYP` with no disk at all:

```bash
CassoCli merlin SYSPROG.S
```

**Expected**: refused, naming `--disk` (FR-041) — the same answer `--type`
without `--disk` gets. A host file has no filesystem type, so unlike `DSK` and
`SAV` there is no host meaning for `TYP` to fall back to.

## Scenario 4 — Several files from one source (User Story 3, P3)

**The expected values below are not invented.** They were measured by running
these exact sources through Merlin Pro 2.23 under Casso and reading the objects
back off the disk. See [research.md](research.md) finding 2a. Assert against
these numbers, not against a plausible-looking result.

### 4a — Two saves with an origin between them

```
 ORG $300
 LDA #$11
 RTS
 SAV SPAN1A
 ORG $6000
 LDA #$22
 RTS
 SAV SPAN1B
```

| File | Load | Length | Bytes |
|---|---|---|---|
| `SPAN1A` | `$0300` | `$0003` | `A9 11 60` |
| `SPAN1B` | `$6000` | `$0003` | `A9 22 60` |

**Two clauses discriminate.** `SPAN1B` must be **3 bytes, not 6** — a cumulative
implementation puts the first file's bytes inside the second and otherwise looks
correct. And it must load at **`$6000`, not `$0303`** — taking the stated origin
rather than continuing from the previous save.

### 4b — Two `DSK`s and no `SAV` at all

```
 DSK SPAN2A
 ORG $300
 LDA #$11
 RTS
 DSK SPAN2B
 LDA #$22
 RTS
```

| File | Load | Length | Bytes |
|---|---|---|---|
| `SPAN2A` | `$0300` | `$0003` | `A9 11 60` |
| `SPAN2B` | `$0303` | `$0003` | `A9 22 60` |

Two files with no `SAV` in the source. `$0303` is the previous end plus one,
which is the run-on rule — no origin intervened here, unlike 4a.

### 4c — Bytes after the last save are dropped

```
 ORG $300
 LDA #$11
 RTS
 SAV SPAN3A
 LDA #$22
 RTS
```

**Expected**: `SPAN3A` only, `$0300`, 3 bytes. The trailing two instructions are
assembled and counted in the byte total but reach **no file** (FR-045), and the
assembly **warns** that bytes were assembled and not saved.

A second file here is a failure, not a bonus — Merlin writes one, and SC-003
promises the files a period assembler would have produced. A missing warning is
equally a failure: dropping bytes silently is the thing the warning exists to
prevent.

### 4d — Two saves under one name

```
 ORG $300
 LDA #$11
 SAV SAME
 LDA #$22
 SAV SAME
```

**Expected**: `Errors: 0`, one file, holding `A9 22` — the second save overwrote
the first. Merlin reports nothing about it.

**This was specified as a refusal and measurement said otherwise.** Refusing
leaves no file where Merlin leaves one, which SC-003 does not allow. Casso warns
and lets the later output stand; the warning is the half that is ours.

### 4e — A save under an output-file directive

```
 DSK OUTER
 ORG $300
 LDA #$11
 RTS
 SAV INNER
 LDA #$22
 RTS
```

**Expected**: refused, at the `SAV` line, naming `DSK`. Merlin answers
`Bad "SAV" in line: 6` and writes no second file.

**This was specified the other way round too.** The code carried a rule for
combining them — the directive staying in effect past a save and governing the
next span — reasoned out because no vendor source mixes the two. One streams the
following code to disk as it is assembled and the other writes the buffer held
in memory: different mechanisms, not two spellings of one.

### 4f — A listing per output

```bash
CassoCli merlin TWO.S -l
```

**Expected**: `SPAN1A`, `SPAN1A.lst`, `SPAN1B`, `SPAN1B.lst`. Each listing holds
its own output's lines plus the equates above the first output, which appear in
both. The line carrying `SAV SPAN1A` belongs to `SPAN1A.lst`, not to `SPAN1B`'s.

`-lfoo.lst` is refused, naming the rule. `as65 -l` is untouched and still writes
to standard output.

**No period behavior is being matched here.** Merlin sent its listing to a screen
or a printer and could not write one to disk, so there is nothing to be faithful
to; FR-028 says as much.

### 4g — Host files and failure

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

**Expected**: byte-for-byte the same object as before this feature, with no
allowance. The bytes are the part of this that does not bend.

Then the listing, which does get one narrow allowance:

```bash
CassoCli as65 prog.a65 -oprog.bin -lprog.lst
```

**Expected**: `prog.lst` identical line for line to before. as65's `-l` is
untouched, so nothing about this invocation changes at all.

```bash
CassoCli merlin PROG.S
```

**Expected**: the listing text is the same lines as before, but under Merlin it
now lands in a file rather than on standard output, and a multi-output source
divides it across files. That division and destination is the whole allowance —
no line of listing text differs.

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
