# Quickstart: Disk File Access for the Build Loop

**Feature**: `specs/020-disk-file-access`

How to validate the feature end to end. Contracts live in `contracts/`; entity
detail in `data-model.md`. This file is the run guide, not the implementation.

## Prerequisites

- `scripts/Build.ps1` succeeds for x64 Debug and Release.
- `%LOCALAPPDATA%\Casso\Disks\DOS 3.3 System Master.dsk` present for the
  boot-level gates. Every test that needs it **skips gracefully** when it is
  absent — the sanctioned exception carried over from the existing suite.

## Unit validation

```powershell
# The filesystem layer, fast loop
scripts\RunTests.ps1 -Build -Filter Volume

# The track layer's decode report
scripts\RunTests.ps1 -Build -Filter Nibbliz

# The command-line surface -- MUST stay green; spec 019 shares these files
scripts\RunTests.ps1 -Build -Filter CommandLine
```

A filtered run prints a banner saying it is not the suite. It never counts as a
suite pass.

Full gate before merging:

```powershell
scripts\RunTests.ps1 -Build -Configuration Debug     # ~15 min
scripts\RunTests.ps1 -Build -Configuration Release   # ~2 min
scripts\Build.ps1 -RunCodeAnalysis
scripts\CheckStyle.ps1
```

Debug and Release run **different test sets** — assertion-behavior tests verify
nothing in Release — so Release is not a substitute for the Debug gate.

## The loop this feature exists to close

```powershell
# 1. Assemble to a loadable binary (already shipped)
CassoCli.exe prog.a65 -o prog.bin --dos-bin

# 2. Put it on a disk
CassoCli.exe disk put mydisk.dsk prog.bin --as PROG --type B --addr `$6000

# 3. Confirm it landed, without booting anything
CassoCli.exe disk list mydisk.dsk

# 4. Make the disk run it on boot
CassoCli.exe disk boot mydisk.dsk PROG

# 5. Boot it
Casso.exe --disk1 mydisk.dsk
```

SC-001 is met when every step is one invocation with no third-party tool.
SC-006 is met when steps 1-5 complete in under 10 seconds for a few-kilobyte
program.

## Acceptance walkthroughs

### US2 — placement (P1)

1. Create a formatted DOS 3.3 image, `put` a 512-byte binary as `PROG` at
   `$6000`, boot it, and confirm `CATALOG` lists `B 002 PROG` and `BLOAD PROG`
   places the bytes at `$6000`.
2. Same payload onto a ProDOS image; the guest catalog shows type `BIN` with
   auxiliary type `$6000`.
3. `put` over an existing name: the old file is replaced, its space returned, and
   there is exactly one catalog entry.
4. `put` onto a nearly-full image: refused, the shortfall named, and the image is
   **byte-for-byte unchanged** — compare hashes before and after.
5. `put` onto a write-protected image: refused with an intelligible message, not
   a raw error code.
6. `put` over the stock master's `HELLO` (type `$82`, locked): refused until
   unlocked.

### US3 — reading (P1, and chronologically first for a migrating developer)

1. `list` a disk with known contents: every file's name, type, size, and lock
   state, plus free space.
2. `get` a binary: bytes match what was placed, load address reported.
3. `get` a text file: high-bit encoding converted, line endings normalized.
4. `list` a disk with a damaged catalog: readable entries on stdout, damage on
   stderr, **exit status 1**.
5. `list` a disk with an undecodable track: unrecovered sectors identified as
   unrecovered, never as zero bytes.

Run 1-3 against `.dsk`, `.do`, `.po`, and `.woz` of the same content — SC-004
requires byte-exact extraction across every mountable format.

### US4 — boot configuration (P2)

1. DOS 3.3: `disk boot` a binary, boot the image, confirm it runs with no typing.
   Verify the patch landed at **T01 S09 `+$75`**, 30 bytes, high ASCII,
   `$A0`-padded.
2. ProDOS: the chosen `SYS` file becomes the first the boot path finds.
3. Naming a file that is not present is refused.

### US5 / US6 (P3)

Direct-boot image reaches the payload measurably faster than the equivalent OS
boot (SC-007); an oversized payload is refused with the capacity stated. A BASIC
listing placed as a program `LIST`s back identically; an untokenizable line is
refused with its number and text quoted.

## Safety gates — the ones that matter most

These verify the properties that make the feature trustworthy rather than merely
working.

**SC-005 — nothing partial survives a failure.** For every documented failure
mode, hash the image before and after and boot it afterwards. Byte-identical,
still bootable, and no temporary file left beside it.

**SC-008 — no write destroys unreadable data.** Against an image with a track
that does not decode to standard sectors, the write is refused and the image is
byte-for-byte unchanged.

**SC-009 — every commit was checked.** Feed the write path a deliberately
corrupted computed result — a free map disagreeing with the catalog, a sector
claimed twice — and confirm it is refused rather than written.

**SC-010 — damaged volumes terminate.** Build volumes with cyclic and
self-referential chains; confirm every operation returns rather than hanging.
This is a real hang risk, not a theoretical one: the integrity pass walks every
chain, and it runs by design on volumes chosen for being damaged.

## Interrupted-write check (manual)

Crash safety is hard to unit-test and worth one manual pass. Start a `put`,
kill the process during the commit, then confirm the original image is intact and
bootable and that no temporary file remains.
