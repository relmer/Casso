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

Type or paste into Merlin's editor.

**Do not be tempted to write the file directly instead.** Merlin source on this
disk is stored as a **type-B (BINARY) file with load address `$0901`**, in
high-bit ASCII with CR line terminators — not as a type-T text file. Confirmed on
every `.S` file on the distribution disk. A text-typed file will not open in
Merlin's editor, so a write path that produced one would fail in a way that looks
like a Merlin problem rather than a tooling problem.

That is the argument for the editor route: Merlin produces its own format
natively, and pasting avoids having to reproduce it. Anyone building the write
half later needs the format above, and needs to know a plain text file is not it.

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

This is the same standard the extraction script was held to: it was validated
against the DOS 3.3 System Master, where `FID` extracts at its canonical load
address of `$0803` — verification against an external fact rather than against
itself.

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
