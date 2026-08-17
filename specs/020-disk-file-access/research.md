# Phase 0 Research: Disk File Access for the Build Loop

**Feature**: `specs/020-disk-file-access` | **Date**: 2026-08-15

Findings that shape the design. Empirical results were measured against the
cached DOS 3.3 System Master and the code in the tree, not recalled — each such
item says how it was verified.

---

## R-001 — The substrate is asymmetric between the two filesystems

**Decision**: Plan DOS 3.3 and ProDOS as separate work streams with different
shapes, not as one parameterized effort.

**Finding** (verified by reading the headers and grepping the tree):

| | ProDOS | DOS 3.3 |
|---|---|---|
| Reader | `ProDosReader::ExtractFile` — seedling / sapling / tree | **None. No `Dos33Reader` exists.** |
| Writer | `ProDosFileWriter::WriteFile` — name, type, aux, bytes; seedling + sapling; `AllocateBlock` against the volume bitmap | `Dos33FileWriter::WriteHello (buffer)` — zero parameters, one hardcoded file |
| Delete | None | None |
| Paths | Volume directory only | Flat by nature |
| Tests | `UnitTest/EmuTests/ProDosVolumeTests.cpp` | Only via `BlankDiskBuilderTests` |

Both classes are declared inside `ProDosSkeleton.h` / `Dos33Skeleton.h` rather
than in files named for themselves, which is why a survey by filename misses
them.

**Rationale**: The two are not at the same starting point, but the effort is
closer to balanced than the table suggests, because DOS 3.3's structures are
much simpler — flat catalog, track/sector list, no tree, no subdirectories.
Building its reader from nothing is small; ProDOS's remaining work is fiddlier
per line.

**Consequence for sequencing**: US3 (extraction) is P1 and needs **both**
readers. The DOS 3.3 reader is on the critical path from the first story, and
nothing exists to build it on.

**Alternatives considered**: A single generic volume abstraction with two
backends, written together. Rejected — the two filesystems share almost no
structure below the API surface, and the shared layer would be an interface with
two independent implementations behind it, which is what the contract already
describes without forcing simultaneous development.

---

## R-002 — The sector decoder discards what it cannot read (pre-existing defect)

**Decision**: Extend denibblization to report per-track, per-sector decode
results, and land it before any write path consumes its output.

**Finding** (verified by reading `NibblizationLayer.cpp:753-785` and
`DiskImage.cpp:429-435`): `Denibblize` walks each track calling
`DecodeOneSector`, and on the first failure executes `break`, then returns
`S_OK`. Undecoded sectors remain zeros in the output buffer. Because the loop
breaks rather than continues, **one undecodable sector zeroes every later sector
in scan order on that track**; sectors decoded before it survive.

`DiskImage::Serialize` routes `Dsk`/`Do`/`Po` straight into `Denibblize`, and
that is the emulator's flush path. So this is **live, not latent**: a guest that
leaves a track partially written can already lose the remainder of that track on
eject, with no error surfaced anywhere.

**Rationale**: This feature would build a write path on top of a decoder that
silently returns zeros for data it could not read — the exact shape of a
data-loss bug. FR-018 is the fix.

**A test already pins this behavior, and it is right about the case it tests.**
`UnitTest/EmuTests/NibblizationTests.cpp:308` deliberately asserts the zero-fill,
reasoning that "missing sectors read back as zeros … is intentional for sector
images (a blank disk is all zeros), not silent corruption of a valid track."

That reasoning is correct for the case it exercises and wrong as a general claim,
and the gap between them is exactly the defect:

- **Wholly unformatted track** (no address fields anywhere): zeros are correct — a
  blank disk really is all zeros. The test wipes an entire track, so this is the
  case it covers.
- **Partially decodable track** (some sectors decode, then a failure): `break`
  zeroes the tail. That *is* the "silent corruption of a valid track" the comment
  claims it is not.

**Three consequences:**

1. **The existing test does not block the fix.** It wipes the whole track, so
   under continue-and-resync it still yields zeros and still passes. No existing
   test breaks.
2. **The comment must be narrowed as part of the fix.** Left standing it reads as
   license for the behavior, and a later reader will cite it to revert the fix.
3. **The report must carry the shape of the failure, not just its presence.** It
   MUST distinguish "no address fields found → unformatted, benign, writable"
   from "coverage incomplete → data loss, refuse the write". A report that only
   says "not all sixteen decoded" collapses the two, and its consumer then either
   rejects blank disks or accepts damaged ones — the latter being this defect
   again, wearing a report.

**The `break` is not the only zero-fill path.** Two more leave logical sectors
zeroed with no decode failure at all, so a fix aimed at `break` alone leaves both
live:

- **Out-of-range sector number** — `continue` at NibblizationLayer.cpp:771 skips a
  sector whose address field claims, say, sector 200. The loop is bounded at
  sixteen iterations, so that iteration is consumed and one logical slot is never
  filled. Nothing failed; nothing was reported.
- **Duplicate sector numbers** — two physical sectors both claiming sector 5 both
  `memcpy` to the same offset. The second overwrites the first and some other
  logical slot goes unclaimed. Again no failure.

Both are corruption modes a damaged or deliberately-formatted track produces.

**Therefore `Complete` is a coverage property, not a loop property.** "Ran sixteen
times without failing" does not mean intact. The sound definition:

```text
Complete     = all 16 logical sectors written EXACTLY ONCE
Partial      = address fields present, coverage incomplete or duplicated
Unformatted  = no address fields found at all
```

Implemented as a 16-bit coverage mask per track, checked at end of track, with a
second write to an already-covered slot counting as a violation. Sixteen bits and
one comparison — cheaper than the decode work already happening. It **subsumes**
the `break` case rather than sitting beside it (a track cut short simply has
incomplete coverage), so one property decides the outcome instead of three
conditions that must each be remembered, and any future mechanism that loses a
sector is caught without anyone having anticipated it.

**Three corrections from implementing it**, each caught by a test rather than by
review:

1. **The "exactly once" test is no longer redundant.** It was specified as
   insurance against a change to the sixteen-iteration bound; the implementation
   made that change (scanning one revolution instead), so duplicates can now
   coexist with full coverage and the weaker "mask is full" test would be wrong.
2. **The revolution bound must be tested where the address field was found, not
   where the cursor sits.** The gap trailing the last sector leaves the cursor
   short of a full revolution, so a bound on the cursor admits one more attempt
   that wraps onto an already-recovered header — reporting a duplicate the disk
   does not have. Damage invented by the scan is as bad as damage missed by it.
3. **Coverage is indexed by output-buffer slot, not by the number the address
   field carried.** Every consumer holds the flat buffer; the interleave between
   the two belongs to the track layer.

**Also found while implementing**: the decoder never verified the address-field
checksum. An unverified header yields a plausible but wrong sector number, and
the payload is then filed under the wrong logical sector — silent misplacement
rather than a reported failure. Now validated, with a mismatch resynchronizing
onto the next header.

**Call-site census** (verified by grep): exactly **one** production caller,
`DiskImage::Serialize` at `DiskImage.cpp:434`. Everything else is tests — twelve
sites across `NibblizationTests` (9) and `BlankDiskBuilderTests` (3).

**Overload decision**: keep both signatures, but the existing three-argument form
**forwards to the reporting form and fails on data loss** rather than bypassing
it. Preserving it as a reportless passthrough would leave the defect reachable in
the one place that matters — the flush path — while looking like compatibility.
Forwarding-and-failing keeps all twelve test sites compiling, keeps the
unformatted-track test passing, fixes `Serialize` whether or not anyone migrates
it, and leaves no overload that silently loses data for someone to pick later.

**Action**: The pre-existing emulator-side exposure is a defect in its own right
and should be tracked separately rather than folded silently into this feature.
See spec § Dependencies and Known Defects.

**Alternatives considered**: Treating any decode failure as a whole-operation
error. Rejected — US3 explicitly wants a useful partial report from a damaged
disk, and it would also make blank tracks fail, which is wrong.

---

## R-003 — DOS 3.3 boot greeting is a 30-byte field inside the DOS image

**Decision**: Setting the DOS 3.3 startup program patches the greeting filename
in place, in the DOS image on tracks 0-2. No catalog change, no chaining file.

**Finding** (verified empirically against the cached DOS 3.3 System Master by
scanning tracks 0-2 for high-ASCII `HELLO` = `C8 C5 CC CC CF`):

- Exactly two occurrences on the whole disk: **T01 S09 `+$75`** (the DOS image)
  and T17 S15 `+$0E` (the catalog entry's name field).
- The T01 S09 field holds the name in **high ASCII, padded with `$A0`**, 30 bytes
  wide — the same width as a catalog name field.
- Caveat worth recording: the bytes *after* the field are also `$A0`, so the
  field's end is not self-evident from the data. 30 is taken from the catalog
  name width, which the DOS code that reads it shares.

**Bonus finding**: the master's catalog entry 0 has type byte **`$82`** — the
`$80` lock bit over `$02` (Applesoft). The stock HELLO is locked, so the most
obvious test disk already exercises FR-014's locked-file refusal.

**Rationale**: Verified beats recalled for an on-disk offset, and this one is
cheap to verify.

**Alternatives considered**: Writing a new greeting file that chains to the
target. Rejected — it costs a catalog entry and sectors, changes what `CATALOG`
shows, and is not how DOS itself does it.

---

## R-004 — ProDOS startup program is directory order, not a stored name

**Decision**: Setting the ProDOS startup program reorders the volume directory
so the desired `.SYSTEM` file is the first one the boot code finds.

**Rationale**: The ProDOS boot block loads `PRODOS`, which launches the first
file of type `$FF` (SYS) in volume-directory order. There is no "startup program"
field to patch — the mechanism *is* the ordering. This is a genuinely different
mechanism from DOS 3.3's, not a second spelling of one, and the two must not be
unified behind a single "write the boot name" helper.

**Consequence**: The ProDOS arm needs directory-entry reordering, which is
adjacent to the delete work (both rewrite directory entries in place) and should
be built with it.

---

## R-005 — Volume integrity is one pass with four consumers

**Decision**: Build the reference map once as a first-class pass over a volume,
not three or four times inside its callers.

**Consumers**:

1. **Delete** (FR-011) — what may be freed is what this file uniquely owns.
2. **Listing** (US3) — the damage report is this pass's output.
3. **Allocation** (Edge Cases) — whether the free map may be trusted.
4. **Pre-commit check** (FR-039) — run over the *computed result* before writing
   a byte, so a write verifies its own output rather than assuming it.

**Rationale**: The fourth consumer is what changes the feature's character. R-002
describes a defect that shipped and survived precisely because nothing on the
write path ever inspected its own output. A pre-commit self-check is the
structural answer to that class, and it costs one call to a pass being built
anyway.

**Termination** (FR-038): the pass walks every file's chain on volumes selected
for being damaged, and a corrupted next-pointer can form a cycle. Traversal is
bounded by a visited set plus a ceiling derived from the volume's own capacity
(560 sectors / 280 blocks), and a chain hitting the bound is reported
unfollowable rather than followed.

**Alternatives considered**: Checking only on delete, where the danger is most
obvious. Rejected — that leaves the write path unverified, which is the case
that actually shipped a bug.

---

## R-006 — Bit-stream writes re-encode only changed tracks

**Decision**: Denibblize to a flat buffer, edit, then re-encode **only** the
tracks whose 4,096 bytes changed, writing into `DiskImage::GetTrackBitsForWrite`
and leaving every other track's packed bits untouched. Serialize as normal.

**Feasibility** (verified by reading `DiskImage.h`): the class already stores
per-track bit streams (`m_trackBits`), per-track dirty flags, per-track bit
counts, and an explicit quarter-track map. Nothing new is needed to hold
untouched tracks verbatim.

**Refusal signals**, cheapest first (FR-019):

1. **Quarter-track map** — free and definitive. Sector images map every
   quarter-track to `qt / 4`; WOZ images install an explicit map from the TMAP,
   so any position resolving elsewhere means data at half/quarter positions that
   a sector-level rewrite cannot represent. Whole-image refusal, zero decoding.
2. **Image metadata** — flags recording that the image was captured with
   cross-track synchronization or with drive-level fake bits preserved. Whole
   image, two bytes of parsing, evaluated before any track is touched.
3. **Per-track sector decode** — the primary test. Sixteen distinct valid
   standard sectors, or the track is not writable. This is R-002's report.
4. **Track bit length** — advisory only; materially off-nominal lengths are a
   hint, not a verdict.

**Posture**: fail-safe. Prove standard-ness; never enumerate protection schemes.

**Scope note**: most copy-protected disks never reach the track check at all,
because they carry no readable DOS 3.3 or ProDOS volume and the filesystem layer
refuses them first. The track check is the backstop for a mostly-standard disk
with one or two protected tracks — precisely where a silent failure does the most
damage, which is why it earns its keep despite rarely firing.

---

## R-007 — Atomic commit at the shell edge

**Decision**: Core produces complete image bytes or fails, changing nothing (the
shape `BlankDiskBuilder::Build` already uses). The shell writes those bytes to a
uniquely named temporary file alongside the target, then atomically replaces the
original.

**Rationale**: Two independent arguments, and the second is the stronger one.
Crash safety is the obvious one. The better one is **non-destructive replace**:
replace is delete + write, and done in place a failure between the two frees the
old file and never lands the new one, losing the file outright — worse than a
refused write. Computing the whole result first makes that impossible by
construction. This project has already shipped two disk-write corruption defects,
so insurance on this specific path is not hypothetical.

**Details to get right**:

- **Uniqueness and cleanup** — two concurrent invocations on one image must not
  collide, and a hard kill must not litter. The project dislikes stray files and
  forbids gitignoring them, so a leftover temp is a real if minor violation.
- **Write-protect composition** — spec 017 uses the host read-only attribute as
  the write-protect mechanism for sector formats. Replacing a read-only file
  fails with access denied, which is the *correct* outcome under FR-014, but the
  message must be intelligible rather than a raw platform code.
- **Documented asymmetry** — the emulator's own flush path writes
  non-atomically. After this feature, command-line writes are crash-safe and
  emulator flushes are not. Deliberate: one is a one-shot on a file the user may
  have no other copy of, the other happens continuously. Recorded in the spec's
  Assumptions so it is not rediscovered as an oversight.

**Alternatives considered**: In-memory rollback with a plain overwrite (covers
every documented failure mode but not interruption); a `.bak` copy (manual
recovery, litters the tree).

**MEASURED AFTERWARDS, AND ONE HALF OF "must not litter" IS NOT MET.** The
interrupted-write pass in `quickstart.md` §US2 stops the process from inside the
commit at a chosen instant, which is the only way this instant has ever been
reached — every attempt to kill the process from outside landed after the
replace. The image survives both stages byte-identical and still boots. The
temporary does **not** get cleaned up, and that is two separate facts:

- A hard stop cannot run cleanup. Expected, and not the problem.
- **No later run reclaims it either.** `CommitPlan::TemporaryPathFor` stamps
  each invocation's own tag into the name, so an orphan sits at a name no future
  invocation will ever choose, examine, or sweep. Recovery is unaffected — the
  next `put` succeeds and produces the right bytes — but the file stays until a
  person deletes it.

So read the decision above as satisfied for uniqueness and for crash safety, and
**not satisfied for littering after a hard abort**. The "real if minor
violation" this section names is still open, deliberately rather than by
oversight: the fix is a sweep of stale siblings at commit time, which is a
behavior change to the shipping commit path and needs a rule for telling an
orphan from another live invocation's temporary — the very case the invocation
tag exists to keep apart. That is its own piece of work with its own tests, and
whoever picks it up should start here rather than rediscovering the measurement.

---

## R-008 — In-use detection is out of scope, and the requirement said otherwise

**Decision**: Do not attempt to detect that an image is mounted in a running
emulator. Document the hazard; probe only for what the platform can observe;
re-verify the file between read and commit.

**Finding** (verified by reading `DiskImageStore.cpp:185` and
`DiskImage.cpp:594`): both open the image with a scoped `ifstream`, read it, and
close. **The emulator holds no handle on the image while it is mounted.** An
exclusive-open probe therefore succeeds even with the image mounted, so the
obvious mechanism cannot work.

**Rationale**: The spec's own Assumptions already declared a mounted image out of
scope, while the pre-renumber FR-029 (now FR-035 and FR-036) required detecting exactly that — an internal
contradiction. The fix is the requirement, not the mechanism. Spec 021's disk
manager can coordinate properly because it runs *inside* the process that knows
what is mounted; a separate CLI process fundamentally cannot without inventing a
protocol this feature deliberately does not introduce.

**Retained, because they are cheap and honest**:

- Best-effort exclusive-open probe — catches *another* tool holding the file (an
  editor, a sync client, another disk utility). Never catches Casso; the
  documentation must not imply it does.
- Size + modification-time re-verify immediately before commit — closes the
  window where another writer landed between read and commit. Cannot detect a
  write landing *after* the commit, and the documentation says so.

**Alternatives considered**: A sidecar lock file with a PID (adds a cross-process
protocol for an out-of-scope hazard; PIDs recycle, so stale locks false-positive);
holding an exclusive handle for the mount's lifetime (degrades the emulator to
serve the CLI, and refactors code spec 021 will build on while 021 is unwritten —
if 021 wants a held handle, that is 021's call with the whole picture in view).

---

## R-009 — CLI surface is additive by construction

**Decision**: One row in the subcommand table, one arm in `Parse`, one options
struct extension. Do not reshape the dispatcher.

**Finding** (verified by reading `CommandLineParser.cpp:14-17, 942-1008`): the
table `s_kSubcommands` currently holds a single row (`run`), `LookUpSubcommand`
resolves against it, and an unrecognized first argument falls through to AS65
mode. Adding `disk` is one table row plus one `if` arm alongside the existing
`Run` arm.

**Rationale**: Spec 019 is being developed concurrently in another worktree and
its only overlap with this feature is exactly these files. They were moved into
the core library and made table-driven so both features could extend them
additively. `UnitTest/CommandLineTests.cpp` pins current behavior; breaking those
tests breaks the other feature.

**Verb vocabulary** (FR-030): descriptive canonical verbs `list`, `put`, `get`,
`delete`, `boot`, with terse aliases `ls` and `rm`. Help displays the descriptive
form. `put` / `get` are unambiguous because they are named from the *disk's*
perspective, which also happens to match the mnemonics of the tool developers are
migrating from — worth saying in the help text. `cat` is excluded: it collides
with the established meaning of printing a file's contents, which this tool does
under `get`.

**Exit statuses** (FR-031, FR-032): `0` clean, `1` succeeded with complaints, `2`
produced no output — matching what `as65` and `run` already return
(`CommandLine.cpp:1218`, `1159`, `986`). Values of 3 and above stay
subcommand-scoped and documented in that subcommand's help; requiring global
uniqueness above 2 would couple subcommands that are otherwise independent, which
is the property that let 019 and 020 proceed in parallel at all.

---

## R-011 — Both text conventions settled by measurement (CLOSED)

**Decision**: The high-bit convention stays a **parameter** of the text codec --
not because the answer is unknown, but because measurement showed there is no
single answer to know. Both filesystems carry high-bit text in practice; the
ProDOS type does not constrain its producer, so a decoder must tolerate either
and the encoder picks one as policy.

### DOS 3.3 — settled: high-bit-set ASCII, `$8D` terminators

Measured across whole files on a Merlin disk carrying eight type-T files:

| File | Bytes | High-bit | `$A0` spaces | `$20` spaces |
|---|---|---|---|---|
| `T.MACRO LIBRARY` | 1,616 | 1,578 | 236 | 37 |
| `T.SENDMSG` | 150 | 150 | 26 | 0 |
| `PI.NAMES` | 40 | 39 | 0 | 1 |

Terminators in `T.MACRO LIBRARY`'s first 256 bytes: `$8D` × 28, `$0D` × 0.
`T.SENDMSG` is the clean specimen — 100% high-bit, every space `$A0`, no
low-ASCII at all. **Spaces are high too**, not merely letters.

### The decoder consequence, which matters more than the convention

**Real files are mixed, so the decoder must strip bit 7 if present and tolerate
it absent — never validate it.** `T.MACRO LIBRARY` carries 37 legitimate `$20`
spaces inside otherwise high-bit content, on a banner comment line padded with
low ASCII. A decoder asserting bit 7 would reject a genuine vendor file.

The same tolerance applies to terminators: comparing the *raw* byte to `$8D`
would miss a plain `$0D` line ending in the same file and silently run two lines
together. The comparison therefore happens **after** stripping.

This was nearly gotten backwards from a 48-byte sample, which landed on that
low-ASCII banner line and suggested spaces were plain. The whole-file counts say
the opposite. Same shape as the catalog-names trap below: a sample adjacent to
the truth is not the truth.

### ProDOS — SETTLED by measurement, and the common assertion is wrong

Real ProDOS `TXT` files arrived with the Merlin fixture disks. Measured:

| File | Bytes | High-bit | `$A0` | `$20` | `$8D` | `$0D` |
|---|---:|---:|---:|---:|---:|---:|
| `/MERLIN/LIB/SENDMSG.S` | 149 | **149** | 26 | 0 | 15 | 0 |
| `/MERLIN/SOURCE/PI.NAMES.S` (first block) | 256 | 223 | 3 | 33 | 10 | 0 |

**ProDOS text here is high-bit-set ASCII with `$8D` terminators — the same
convention as DOS 3.3, not the plain seven-bit ASCII with `$0D` that is widely
asserted.** `SENDMSG.S` is 149 of 149 bytes high with every space at `$A0`.

And it is **mixed** in exactly the way DOS 3.3 files are: `PI.NAMES.S` carries 33
plain `$20` spaces among 223 high-bit bytes. So the tolerance the decoder already
has is required on this filesystem too, not merely on the other one.

### The conclusion is subtler than "ProDOS is high ASCII"

Every sample here was written by Merlin, an Apple II assembler that uses the
high-bit convention natively. That is not a reason to discount the evidence —
these are genuine ProDOS `TXT` files that a reader must handle — but it is a
reason not to over-generalize from it. A `TXT` file written by a Pascal or
AppleWorks-era tool may well be plain ASCII.

So the durable finding is: **the ProDOS `TXT` type does not imply a character
convention. The producer decides, and both occur.** That is more useful than
either single answer, and it settles the design questions:

- **Decoding must tolerate both**, which it does. This is now a requirement
  supported by measurement rather than a defensive choice.
- **Encoding is a policy decision, not a discovered fact.** Casso writes for
  Apple II developers whose other tools are Apple II tools, so high ASCII is the
  better default on both filesystems — chosen deliberately, and recorded as a
  choice rather than as a finding.

### Being wrong is cheaper than it first appeared — reads cannot be corrupted

Tolerating mixed bytes has a consequence worth stating on its own: **decoding is
independent of the convention entirely.** Once the high bit is ignored,
high-ASCII and plain-ASCII text differ in nothing, so the parameter only affects
the *encode* side.

That changes the cost of guessing wrong from "we mangle files in both
directions" to "we write the wrong convention, and reading it back still works."
Much smaller, and it is why shipping with this question open is defensible
rather than merely tolerable. A wrong guess produces ProDOS text files a
period-correct tool might render oddly; it cannot make Casso unable to read its
own output, and it cannot damage an existing file on read.

**Why the parameter is right regardless**: a parameter costs one argument, makes
the answer a data change instead of a rewrite, and lets round-trip identity be
tested for both conventions today.

**A trap worth naming**: the verified fact about catalog *names* is not evidence
about file *contents*. Generalizing from the adjacent case is exactly how a
comment in the nibblization tests came to license a data-loss defect.

---

## R-012 — Runtime validation pass over quickstart §US3 (T022)

Run against the shipped `CassoCli.exe`, not through the seam. Every scenario in
quickstart §US3 was exercised against real material; what follows is what it
found rather than a restatement of what passed.

**Confirmed working.** `list` on a DOS 3.3 `.dsk` reproduces the machine's own
`CATALOG` output on 66 of 67 lines — the one difference is the reference's
trailing `]` prompt against our free-space summary. `list` on a `.do` correctly
identifies a ProDOS volume in DOS sector order. `get` of a binary reports its
load address on stderr and delivers 589 bytes through a real shell redirect
rather than 618, which is the manual check the binary-mode call exists for.
A damaged catalog delivers 25 stdout lines, one stderr complaint naming the
listing as incomplete, and exit 1.

**Fixed: `--text` was parsed and then ignored.** The flag reached
`CommandLineOptions` and nothing consulted it, so `get --text` returned the
stored bytes and reported success. Wired into `DiskCommandRunner::ApplyEncoding`
and verified end to end: `T.SENDMSG` extracts as 149 high-bit bytes verbatim and
as 149 plain bytes with 15 line feeds under `--text`.

**Fixed: `--basic` now refuses instead of silently doing nothing.** There is no
Applesoft tokenizer yet. A parsed-then-dropped flag is worse than an absent one,
because the caller reads "converted to a listing" in the help and cannot tell
tokenized output from a file that needed no conversion.

**Observed, deliberately NOT fixed: copy-protected disks whose VTOC satisfies
detection.** `The Print Shop Color side B.woz` lists as DOS 3.3 with exit 0, a
volume number of 0, and a catalog of one type-I entry plus rows with no visible
name. Side A of the same title reports 307 undecodable sectors and exits 1,
which is correct. Six other protected `.woz` titles are refused outright.

A name-plausibility check was written for this and **reverted**, because it
rejected 20 of the 63 entries on a disk Merlin shipped.

The reason is worth stating exactly, because the first guess at it was wrong.
Those heading rows are not inverse video. Their names are literally
`C1 88 88 88 88 88 88 88 88` followed by the heading text — a high-ASCII `A`,
then eight **backspace** characters. DOS prints ` *T 000 ` and then the thirty
name bytes straight to the screen, so the backspaces walk the cursor back over
the sector count, the type letter and the lock flag, and the heading lands at
column zero. Eight of the thirty bytes are therefore below `$20` once the high
bit is stripped, which is byte-for-byte indistinguishable from arbitrary data.

So name bytes cannot separate the two cases, and tightening on them rejects real
vendor material to catch a disk that is out of scope anyway. Recorded here, and
in `UnitTest/Fixtures/Disks/README.md` on `master` where 021 and 022 will look,
so the same idea is not tried again without new evidence.

---

## R-013 — A stepper-sensitive gate has to run against a WOZ

**Finding**: a sector image cannot fail a head-positioning test, so a gate that
means to check head positioning must not use one.

**Half and quarter tracks work.** `WozLoader` reads the WOZ **TMAP** chunk and
calls `DiskImage::SetQuarterTrackSlot` for every one of the 160 quarter-track
phases, so a WOZ carries a real per-quarter-track map: distinct quarter-tracks
address distinct flux, an unmapped phase resolves to nothing, and copy-protected
disks that format between tracks are read correctly. The `qt / 4` mapping in
`DiskImage::InitWholeTrackMap` is only the synthetic default installed for
**sector images**, which is the only thing it could be — a `.dsk` or `.po` file
is 35 whole tracks of decoded sectors and physically cannot carry half-track
data. **This is a property of the sector formats, not a defect.**

**What it costs is test sensitivity.** Against a sector image, a head left at
quarter-track 6 instead of 4 resolves to the neighboring whole track and reads it
perfectly; a real drive at that position would be off-track and read nothing. So
every stepper defect that shows up as "the head ends somewhere near, but not on,
the intended track" is invisible to the guest when the guest is booting a `.dsk`.

Measured, not reasoned: a mutation to the stepper survived a direct-boot
guest-visible gate running on a sector image and was caught only by a structural
assertion elsewhere. The guest booted anyway.

**Rule for whoever writes the next guest test**: if the property under test is
where the head ends up — seek, recalibration, phase ordering, anything that
walks the stepper — point the gate at a WOZ. A sector image is the right fixture
for filesystem and payload properties and the wrong one for mechanism.

This belongs in `UnitTest/Fixtures/Disks/README.md`, which is where the on-disk
format findings live; it is recorded here because that file is on `master` and
`master` was frozen while this branch was open. Move it there when the branch
lands.

---

## R-010 — Deferred, with rationale

Not researched here because none changes the architecture, and each is better
answered against real code than in advance.

- **Applesoft tokenizer coverage** (US6, P3) — the token table is well
  documented; the open question is the boundary (strings containing token
  spellings, `DATA` payloads, `REM` text), which is a test-design question best
  settled against a real listing.
- **Direct-boot loader capacity** (US5, P3) — how many sectors a custom boot
  path can pull before handing off, which sets FR-027's reported capacity.

  **ANSWERED IN PHASE 6: 183 sectors, 46,848 bytes, and it is a MEMORY limit
  rather than a media one.** The question as posed assumed the disk would run
  out first; it does not, and by a wide margin. The loader re-enters the Disk II
  boot ROM's own read routine, which is what DOS 3.3's boot0 does — read out of
  the stock master at track 0 sector 0, where it builds a `JMP ($3E)` to
  `$Cs5C`. That routine terminates by comparing against the byte at **$0800**
  and returns to **$0801**, both absolute addresses inside the ROM, so page $08
  belongs to the loader for as long as anything is being read. With $0300-$03FF
  taken by the ROM's decode table and secondary buffer, and $C000 up not being
  memory, a payload occupies **$0900 to $BFFF**. Capacity is therefore
  `$C000 - loadAddress` bytes, and 183 sectors is its value at the bottom of
  that window — against 544 sectors of media still free past the loader's own
  track. The relationship is a `static_assert` rather than a runtime check,
  because a check that can never fire is indistinguishable from one that works.
- **File-type spelling on the command line** — whether types are accepted as
  names (`BIN`, `TXT`), numbers, or both.

---

## Open risks

| Risk | Impact | Mitigation |
|---|---|---|
| DOS 3.3 reader does not exist and US3 is P1 | Extraction slips | Sequence it first in the US3 phase; the structures are simple and `Dos33Skeleton`'s constants already describe the geometry |
| Denibblization defect is on the emulator's live flush path | Data loss today, independent of this feature | Fix (FR-018) lands in the foundational phase, before any write path; track the emulator exposure as its own defect |
| Delete is required by replace, which is P1 | Cannot ship US2 without delete + free-space return on both filesystems | Treat delete as foundational, not Tier 2 |
| Concurrent edits to the command-line files by spec 019 | Merge conflict on shared files | Keep the change to one table row and one arm; run `CommandLineTests` before every push |
| ProDOS tree growth is the fiddliest remaining piece | Large files fail or corrupt | Property-test block accounting against the integrity pass rather than by inspection |
