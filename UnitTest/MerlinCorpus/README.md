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

### 3. Verify the round trip — do not skip this

**Paste is not trusted.** Issue #110 reports the guest paste path garbling
input. The response is to verify the channel, not to avoid it:

1. Save the source to the disk from within Merlin.
2. Extract it back: `scripts/ExtractDos33File.ps1 -Image <image> -Name <name>`
3. Compare against what you intended, byte for byte.

A clean round trip proves the paste. Without it, a garbled paste becomes a
captured expectation, and the corpus quietly encodes a lie — which is worse than
having no entry at all, because it launders a mistake into evidence.

### 4. Assemble and capture

Assemble under Merlin with the listing on, save the object, and extract it with
the same script. The script reports the load address and length from the DOS
binary header and strips them, leaving the payload.

### 5. Record the entry

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

## The two entry classes

**Captured** entries carry bytes from real Merlin and a version stamp.

**Negative, hand-authored** entries carry expected *diagnostics* rather than
bytes — subset-boundary refusals and error positions. Merlin produces no bytes
for source it rejects, so these expectations cannot come from capture and must
never claim a version stamp.

Keeping the classes distinct is what makes it always clear where a given
expectation came from.
