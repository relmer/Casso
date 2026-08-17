# Quickstart: Disk File Access for the Build Loop

**Feature**: `specs/020-disk-file-access`

How to validate the feature end to end. Contracts live in `contracts/`; entity
detail in `data-model.md`. This file is the run guide, not the implementation.

## Prerequisites

- `scripts/Build.ps1` succeeds for x64 Debug and Release.
- `%LOCALAPPDATA%\Casso\Disks\DOS 3.3 System Master.dsk` — **required**, not
  optional. Every test that needs it **fails** when it is absent rather than
  skipping: a test that cannot reach its data must not pass, or a machine
  missing the asset reports a confident green suite over guest-visible criteria
  nothing verified. This deliberately reverses the skip-if-missing pattern
  earlier disk specs used; that pattern is why the Dormann suite ran green while
  doing no work.

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
   `$6000`, boot it, and confirm `CATALOG` lists **`B 004 PROG`** and
   `BLOAD PROG` places the bytes at `$6000`. The sector count is four, not two:
   DOS stores the load address and length *inside* the file, so 512 payload
   bytes occupy 516 stored bytes — three data sectors — and the track/sector
   list is the fourth. `B 002` is what a payload of 252 bytes or fewer produces.
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
5. `list` a disk with a **partially** decodable track — some sectors decode, then
   a failure: unrecovered sectors identified as unrecovered, never as zero bytes,
   and the write path refuses that track.
6. `list` a disk with a **wholly unformatted** track: reported as blank, **not**
   as damage, and the write path accepts it. This is the case the existing
   `Denibblize_UnformattedTrack_ZeroFillsThatTrackAndKeepsOthers` test pins, and
   it must keep passing — collapsing it into case 5 would make blank disks
   unwritable.
7. `list` a disk with a track carrying an **out-of-range** sector number, and one
   with **duplicate** sector numbers: both report incomplete coverage and both
   refuse writes. Neither goes through a decode failure, so a fix aimed only at
   the abandoned-track path would leave both reporting a clean disk.

Cases 5-7 are the three distinct ways a logical sector ends up zeroed. Each gets
its own test beside the existing unformatted one, because the defect survived
precisely by hiding behind a passing test whose name and comment covered only the
benign case:

```text
Denibblize_PartiallyDecodableTrack_ReportsDataLossAndDoesNotZeroTail
Denibblize_OutOfRangeSectorNumber_ReportsIncompleteCoverage
Denibblize_DuplicateSectorNumbers_ReportsIncompleteCoverage
```

All three are built the way the existing test builds its wiped track: nibblize a
valid image, then patch one address field's 4-and-4 encoded sector value.

Run 1-3 against `.dsk`, `.do`, `.po`, and `.woz` of the same content — SC-004
requires byte-exact extraction across every mountable format.

#### Making the damaged images these cases need

None of them exist as fixtures, and none should — a deliberately broken disk is
a constructed shape, and the real volumes are read-only evidence. Build them
from a copy at the point of use.

A broken catalog chain, from the host, needs no code. A DOS-logical sector sits
at `(track * 16 + sector) * 256`, and the catalog's forward pointer is the two
bytes at `+1` and `+2`. Merlin's catalog runs T17 S15 down to S1, so pointing
one hop out of range truncates it mid-walk:

```powershell
$b = [IO.File]::ReadAllBytes('<merlin.dsk>')
$b[(17*16 + 13)*256 + 1] = 40          # a track this volume does not have
[IO.File]::WriteAllBytes('broken.dsk', $b)
```

`CassoCli disk list broken.dsk` then delivers 25 stdout lines, one stderr
complaint naming the listing as incomplete, and **exit 1** — which is scenario 4
end to end.

Track damage cannot be done this way, because a sector image has no bit stream
to corrupt. It needs `NibblizeDsk` → patch a track's bits → `WozLoader::Serialize`,
so it lives in the test assembly; see `CrossFormatExtractionTests.cpp` for the
address-field patching helpers and `NibblizationTests.cpp` for the decoder-level
cases.

**There is no real `.woz` in this repository that serves as a clean read
fixture.** All eleven under `Apple2/Demos/` were tried. Nine are copy-protected
and correctly refused as carrying no filesystem this tool recognizes. `The Print
Shop Color side A.woz` reports 307 undecodable sectors and exits 1, which is
right. Side B is the only one that lists — as DOS 3.3, volume 0, with a catalog
of one type-`I` entry and rows with no visible name, at exit 0. Treat that as a
detection false positive on out-of-scope material, not as a fixture; a
`.woz` built from a real `.dsk` in the test is the clean path.

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

## Binary-output check (manual) — the one no test can reach

**Why it is manual, at every level.** This platform opens standard output in
text mode, so the runtime rewrites every `$0A` byte as `$0D $0A` on the way out.
That translation happens in the runtime *below* the file seam, so the in-memory
substitute the unit tests use does not perform it — those tests pass whether or
not the edge sets binary mode, and a green run there is evidence of nothing.

The narrower assertion — "the edge put its handle in binary mode" — is **also**
unavailable, and that is worth stating because it looks like a way out.
`.github/copilot-instructions.md:426` forbids inspection of real processes,
environment variables or console handles, and querying or changing standard
output's translation mode is both an inspection of a console handle and a
mutation of real process state. It would be a bad trade even if permitted:
CppUnitTest runs every test in one process, so a test flipping the mode would be
mutating state the harness and every other test share.

So there is no automated home for this check at any level, and the assertion
lives here. The fixture that exposes it is chosen rather than constructed:

```powershell
# MAKE DUMP's payload is 589 bytes containing 29 line-feed bytes and NO
# pre-existing CR/LF pair, so any corruption is purely additive.
CassoCli.exe disk get <merlin.dsk> "MAKE DUMP" 2>$null > out.bin
(Get-Item out.bin).Length      # MUST be 589
```

**Run it in PowerShell 7 or `cmd`, not Windows PowerShell 5.1.** Both were
measured. PowerShell 7 and `cmd` pass a native command's output through to a
file as bytes and report 589. Windows PowerShell 5.1 decodes and re-encodes it
as text and reports **1174** — which is neither of the two legitimate answers,
so it would read as a defect in the code rather than in the shell. Redirect
stderr to `$null` as shown, or the load-address line lands in the file too.

| Result | Meaning |
|---|---|
| **589** | Correct. The edge put its handle in binary mode. |
| **618** | 589 + 29. Standard output was left in text mode; every line feed was expanded. |
| **1174** | You are in Windows PowerShell 5.1. Re-run under `pwsh` or `cmd`. |
| anything else | Something other than text mode is wrong — the arithmetic only produces the first two. |

Compare the bytes against `UnitTest/Fixtures/Merlin/MAKE DUMP` from offset 4;
they must be identical. A length check alone passes under any corruption that
happens to preserve size.

**Last run**: 589 bytes, `cmd` and PowerShell 7, byte-identical to the oracle.

## Interrupted-write check (manual)

Crash safety is hard to unit-test and worth one manual pass. Start a `put`,
kill the process during the commit, then confirm the original image is intact and
bootable and that no temporary file remains.
