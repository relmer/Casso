# Merlin Corpus — Capture Procedure

The corpus is the correctness evidence for the Merlin dialect. Every captured
entry pairs source authored here with the bytes **real Merlin 8 produced from
it**, so a test can assert byte-identity without running another assembler.

This document is the procedure. It is written to be followed by someone who was
not here when the entries were captured, because when a corpus test fails later,
this is what decides whether the assembler is wrong or the expectation is.

## What is committed, and what is not

Committed: source authored in this project, the bytes Merlin produced from it,
and the Merlin version that produced them.

**Never committed: the Merlin disk image.** It is commercial software, on the
same grounds that keep `Disks/Apple/dos33-master.dsk` out of the repository.
`/DevDisks/` is gitignored for this reason. A developer regenerating the corpus
supplies their own copy, the way machine ROMs already work.

The bytes themselves are this project's output, not Merlin's — they are what our
source compiles to, captured through a reference implementation.

## Prerequisites

- A Merlin 8 disk image at `DevDisks/Merlin8-v2.47.do`, relative to the
  repository root. Any flat DOS-order image works; the version is recorded per
  entry because edge semantics differ across revisions.
- A Casso build, since Merlin runs under our own emulation.

Nothing here is needed to *run* the corpus tests. Capture is offline and
one-time per entry; the tests read compiled-in literals.

## Why this is not `020-disk-file-access`

`scripts/ExtractDos33File.ps1` reads files off the disk. It exists only because
the Merlin disk happens to be a flat DOS-order image, where sector *S* of track
*T* sits at `((T * 16) + S) * 256` with no nibble decoding — small enough to be
worth doing directly rather than waiting.

It is **throwaway capture tooling and not a product feature**. Spec 020's
`disk get` is tested C++ covering every mountable format including WOZ; this is
forty lines of PowerShell that would fall over on the first nibble image. When
020's extraction lands, delete this script and capture through it instead.

## Procedure

### 1. Write the source

Batch aggressively. Put **many constructs in one composite source file** rather
than one file per construct: assemble once, save the object once, extract once,
and split by known offsets. A handful of composites covers the whole
FR-007..FR-015 floor; one file per construct multiplies the slowest step by
twenty.

### 2. Get it onto the disk

Type or paste into Merlin's editor. To do that **without taking the keyboard
away from whoever is using the machine**, use `scripts/SendCassoKeys.ps1`, which
posts key messages to the emulator window rather than synthesizing focused
input:

```powershell
scripts\SendCassoKeys.ps1 -ProcessId <casso pid> -Text " LDA #`$41" -Return
```

Focused input (`SendInput`, `SendKeys`) requires the emulator window to be
foreground and additionally needs a click into the display panel before keys are
delivered at all. Posted messages need neither.

Merlin's menu also offers `R :Read text file`, so a **type-T text file** can be
read in as source — `L :Load source` is the one that requires Merlin's own
type-B format. Either route still needs a way to write to the disk, which is why
the editor remains the practical path today.

Merlin source on this disk is stored as a **type-B (BINARY) file with load
address `$0901`**, in high-bit ASCII with CR line terminators — confirmed on
every `.S` file on the distribution disk, and corroborated by Merlin's own
`Source: A$0901` display on the boot screen.

An earlier version of this document claimed a text-typed file "will not open in
Merlin's editor". **That is wrong**, and running Merlin disproved it: the menu
carries both `L :Load source`, which wants the type-B format, and `R :Read text
file`, which reads a type-T file. Either is a viable target for a future write
path; the type-B route just avoids a conversion step.

The editor remains the practical route today because writing *any* file to the
disk needs DOS 3.3 write support that the capture tooling does not have — not
because a text file would be rejected.

### 3. Read the source back — and commit *that* copy

Save the source to the disk from within Merlin, then read it back:

```powershell
scripts\CaptureMerlinCorpus.ps1 -Entry myentry -SourceName MYENTRY.S `
    -Expected corpus\myentry.s -Canonical corpus\myentry.s -Verify
```

**The disk copy is canonical.** It is what Merlin actually assembled, so it is
the only text guaranteed to correspond to the bytes captured from it. Commit it,
not the text you pasted. The entry is then self-consistent by construction.

That matters because Merlin's editor may not store what you typed byte-for-byte.
It is column-oriented over a high-bit, CR-terminated format, and normalizing
whitespace or column positions on save would be entirely ordinary. **Whether it
does is a settle-by-capture question — find out on entry one**, before the
harness shape depends on the answer.

The comparison stays, and stays loud, but a mismatch is **information rather than
a failed capture**. Judge which it is:

- **Editor normalization** — expected and benign. The committed disk copy is
  correct regardless.
- **A garbled paste** (issue #110) — the entry is still self-consistent, but it
  now tests a construct you did not intend.

The check is deliberately not fatal. Making routine normalization fail every
capture invites loosening the comparison, and a loosened comparison is no guard
at all.

#### The gap this leaves — automation does not close it

If the paste garbled *and* Merlin assembled the garbled source, the entry is
self-consistent: the bytes genuinely came from the source beside them. It simply
tests something other than what you meant.

The `discriminates` flag catches the worst version — a garble that destroys the
Merlin construct makes the entry stop failing under AS65, and the harness reports
it. It does **not** catch a garble that merely changes an operand.

So: **read the first few entries yourself.** Nothing downstream will tell you.

This guard has a **demonstrated near-miss**, not a theoretical one. The first
version of `scripts/SendCassoKeys.ps1` corrupted every shifted character —
`:` arrived as `;`, `"` as `'` — and it surfaced only because the garbled text
happened to be a BASIC syntax error, so the guest complained. Had the first thing
typed been valid under either spelling, subtly wrong source would have been
entered, assembled faithfully, and its bytes captured. The entry would have been
perfectly self-consistent and tested the wrong thing, and **none of the five
automated axes would have caught it**.

### 4. Delete the target object file — required, not advice

**Before every assembly**, delete the object file you are about to produce, from
within DOS:

```
DELETE MYENTRY.OBJ
```

Confirm it is gone before assembling:

```powershell
scripts\CaptureMerlinCorpus.ps1 -Entry myentry -ObjectName MYENTRY.OBJ -ConfirmAbsent
```

This is the only freshness check available, because **DOS 3.3 catalogs carry no
timestamps** — there is no equivalent of the staleness guard that protects the
test suite.

Two ways capture otherwise produces bytes that did not come from the source
beside them:

- **The assembly errored and you captured anyway.** Merlin reports errors on
  screen, and nothing downstream of the screen knows.
- **The object file is stale.** Assemble entry A, save, edit for entry B,
  assemble, hit an error before saving — and A's object is still on the disk.
  Capture it and you have recorded A's bytes as B's expectation. It is
  self-consistent, plausible, wrong, and **it will never fail**: the assembler
  faithfully reproduces A's bytes from A's constructs, and the test compares them
  to A's bytes.

Deleting first converts "did Merlin succeed?" from something you have to watch
for into something the extraction step asserts. Absence afterwards proves nothing
wrote it; presence proves *this* assembly did.

### 5. Assemble and capture

Assemble under Merlin with the listing on, save the object, and extract it with
the same script. The script reports the load address and length from the DOS
binary header and strips them, leaving the payload.

If extraction reports the file is missing, **the assembly failed** — go back and
read Merlin's errors. Do not capture. An entry with empty expected bytes is an
error in the harness, and deleting first is what makes that guard reachable
rather than theoretical.

### 6. Record the entry

Add source, bytes, and **the exact Merlin version** to `CorpusEntries.h`.

## Cross-checking the oracle

Capture depends on Casso running Merlin correctly — but only at capture time, and
that dependency is discharged rather than assumed. Hand-derive the expected bytes
for a sample of entries from the Merlin manual and compare.

Agreement confirms the emulation ran Merlin correctly on the day of capture.
Disagreement means either a corpus error or an emulator bug, and both are worth
finding.

This is the same standard the extraction script was held to. It was validated
twice against external facts rather than against itself:

- On the DOS 3.3 System Master, `FID` extracts at its canonical load address of
  `$0803`.
- On the Merlin disk, its catalog walk matches **Merlin's own `C :Catalog`
  output** file for file — names, type letters, sector counts, and lock flags.
  Two independent readers of the same bytes agreeing is a stronger check than
  either one looking self-consistent.

Merlin 8 v2.47 is confirmed to boot and run correctly under Casso: menus,
catalog, and disk access all work. One cosmetic defect is known and does not
affect assembly — the title banner renders MouseText glyphs where flashing text
belongs (issue #117).

## Does the entry actually test Merlin?

Most of what Merlin source contains is **shared** with AS65: labels, origin,
literals, and the whole expression evaluator. An entry built only from those
assembles identically whether the Merlin profile works or is never consulted. A
corpus can be large, entirely green, and prove nothing about the dialect.

So every entry records whether it **discriminates**. An entry whose purpose is a
Merlin-specific construct must be verified to **fail under AS65** as well as
matching under Merlin. If it passes under both, one of two things is true and
both are defects: it is not exercising the construct it claims to, or the profile
is not being consulted.

Shared-construct entries leave the flag clear. They are legitimate — engine
regression cover, which SC-004 already leans on — and marking them says so rather
than leaving a reader to wonder.

This is the same technique as verifying a new test fails without its fix. A green
result proves nothing on its own; what proves something is knowing the specific
way it would have gone red.

## The two entry classes

**Captured** entries carry bytes from real Merlin and a version stamp.

**Negative, hand-authored** entries carry expected *diagnostics* rather than
bytes — subset-boundary refusals and error positions. Merlin produces no bytes
for source it rejects, so these expectations cannot come from capture and must
never claim a version stamp.

Keeping the classes distinct is what makes it always clear where a given
expectation came from.
