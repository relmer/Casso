# UnitTest/Fixtures/Disks: real Apple II volumes

**These files are third-party material under a license that is not Casso's, and
they contain runnable third-party software.** Read the license section before
copying them anywhere or reusing this directory as a pattern.

## What this is

Three unmodified Apple II disk images and the catalog listing for each. They
exist so the filesystem layer can be tested against volumes that real software
actually shipped on, rather than against volumes this repository built to suit
its own reader.

The distinction matters more than it sounds. A synthetic image is written by the
same understanding of the format that the code under test uses, so the two agree
by construction, including where both are wrong. These disks were written in
1984 and 1985 by software that had never heard of Casso, and they carry the
irregularities that come with that: entries for deleted files, decorative
zero-length catalog rows, locked files, missing timestamps, near-exhausted free
space, and a volume whose sector order disagrees with its filesystem.

Synthetic fixtures remain the right default for structural cases where a
specific shape needs to be constructed on purpose. See the parent README.

## License

Merlin Pro is **CC BY-NC-ND 3.0**, https://creativecommons.org/licenses/by-nc-nd/3.0/

- **Author**: Glen Bredon
- **Publisher**: Roger Wagner Publishing, 1984–1985
- **Source**: https://archive.org/details/MerlinProMacroAssembler

The license permits verbatim redistribution for non-commercial purposes with
attribution, which is what this directory does and all it does. Four points,
because this directory goes further than any other fixture here:

1. **These files are not MIT.** Casso is MIT; this directory is not. The
   distinction is per-file and stops at this directory.
2. **They contain executable software**, not just data, `MERLIN.SYSTEM`,
   `PRODOS`, `BASIC.SYSTEM`, `ASM.1`, `ASM.2` and the assembler itself are all
   present and bootable. That is a larger step than shipping source text, and it
   was taken deliberately rather than by extending an earlier precedent.
3. **They are unmodified and must stay that way.** The ND term forbids
   distributing altered copies, and a fixture that has been written to is no
   longer evidence of anything. Tests MUST treat these as read-only and operate
   on an in-memory copy. `IFixtureProvider` opens read-only and never writes
   back, which is the mechanism that enforces this.
4. **Non-commercial.** Anyone redistributing Casso commercially needs to remove
   this directory and the tests that depend on it.

Every file is byte-identical to what `scripts/FetchMerlin.ps1` downloads, and
that script pins the same hashes listed below. The provenance chain from
archive.org to this directory is re-runnable at any time rather than asserted.

## Inventory

| File | Bytes | Contents | SHA-256 |
|---|---:|---|---|
| `Merlin-proDos2.23.dsk` | 143360 | DOS 3.3 volume, 35×16, volume 254 | `CB7FD9522A3B90792ACBB00D6C811323DC046DC2920FC05A640858BFE611F0E6` |
| `Merlin-proProdos2.33-a.dsk` | 143360 | ProDOS volume `/MERLIN`, 280 blocks | `90DDC687D78373B034B0576BADEE5F46EF6C0AD74E232ACDF04FD171D418B9DC` |
| `Merlin-proProdos2.33-b.dsk` | 143360 | ProDOS volume `/APPLESOFT`, 280 blocks | `48DD8471C63D1FDF4C10DE78546C976FC411CD3A94E152B913B8AFCF50797FFE` |
| `Merlin-proDos2.23Catalog.txt` | 2445 | Vendor catalog listing for the above | `2BE9A70FB401052D831AEEFDFC68C475B923744D41BDCB0F7EC50F5E3927AAD1` |
| `Merlin-proProdos2.33-aCatalog.txt` | 5399 | Vendor catalog listing, all directories | `9E775537136370314744918A99E3BDD6F7EDCAC0ADEDEAF9629680DFB342C5C2` |
| `Merlin-proProdos2.33-bCatalog.txt` | 1006 | Vendor catalog listing | `5E3524D44CD80C8E9A30C373B643FB78953502BC16E3342F2A239B55709BD2D3` |

The catalog listings are the vendor's own record of what each disk holds. They
are the expected-results reference for enumeration tests: a directory listing
Casso produces should account for exactly these entries.

They are close enough to compare line by line, not merely entry by entry.
Skipping the four header lines of `Merlin-proDos2.23Catalog.txt`, a `CassoCli
disk list` of that image matches **66 of its 67 lines exactly**, including the
backspace-drawn heading rows described below; the one difference is the
reference's trailing `]` prompt against our free-space summary. Free space is
37 sectors of 560 on the DOS 3.3 disk, 21 blocks of 280 on `/MERLIN`, and 8 of
280 on `/APPLESOFT`.

## All three images are in DOS sector order

Including the two holding ProDOS volumes. This is the single most useful
property of the set, and it is not a quirk; it is how these images were
captured and how a great many `.dsk` files in the wild are.

It means a reader cannot infer the filesystem from the sector order, or the
sector order from the filesystem. The two are independent, and both have to be
determined. A ProDOS volume in DOS order puts block 2 at track 0 sector 11
rather than at byte offset 1024.

## The DOS 3.3 image defeats a naive ProDOS probe

`Merlin-proDos2.23.dsk` is a DOS 3.3 disk. Probe it as though it were
ProDOS-ordered and read the storage-type nibble at offset 1024, where a ProDOS
volume directory header would sit, and the high nibble is `$F`, exactly what a
volume header looks like. The "volume name" that follows decodes to garbage,
because those bytes are 6502 boot code that happens to land that way.

A format detector that checks one field misidentifies this disk. That makes it a
negative test worth keeping: format detection must corroborate across more than
a single nibble, and this image proves the point without anyone having to
construct a hostile case.

## A binary's load address lives inside the file on DOS 3.3 and outside it on ProDOS

The same asymmetry, measured on both filesystems here. It is the shape that
produces a four-byte offset bug on one filesystem only, and a reader unified
across the two acquires it.

```
DOS 3.3   LABELS      00 80 D8 03 | B0 B2 B0 B0 C9 4E ...
                      ^^^^^^^^^^^   load $8000, length 984, then the payload
ProDOS    PARMS                     3C ...
                      no header at all: load $8000 comes from aux_type,
                      length 44 from EOF, both in the directory entry
```

So a DOS 3.3 binary's stored size is its payload plus four, and its declared
length is checkable against that. A ProDOS binary's stored size *is* its EOF and
there is nothing to cross-check, apply DOS-style stripping to one and it comes
back four bytes short with no other symptom.

Verified on this set: `PARMS` on `/MERLIN` is type `$06`, one block, EOF 44,
aux `$8000`, first stored byte `$3C`.

## The DOS 3.3 disk's decorative catalog entries are drawn with backspaces

Twenty of its sixty-three entries occupy no sectors and exist to draw section
headings in `CATALOG`. All twenty carry a track/sector pointer of `$7F/$7F`, a
sentinel never meant to be followed, and a sector count of zero.

Their *names* are the part worth knowing about. The first is:

```
C1 88 88 88 88 88 88 88 88 CD C5 D2 CC C9 CE A0 D0 D2 CF A0 AD A0 C4 CF D3 A0 B3 AE B3 A0
 A  <-- eight backspaces -->  M  E  R  L  I  N     P  R  O     -     D  O  S     3  .  3
```

`$C1` is a high-ASCII `A`; `$88` is **backspace**. DOS prints ` *T 000 ` and then
the thirty name bytes straight to the screen, so the eight backspaces walk the
cursor back over the `A`, the sector count, the type letter and the lock flag,
and the heading lands at column zero. It is a display trick encoded in a filename.

Three consequences for anything reading this disk:

1. **Zero-sector entries are not broken chains.** The discriminator is the
   entry's own sector count, not the `$7F/$7F` pointer: zero sectors means the
   entry declares it occupies nothing, so there is nothing to reach and nothing
   lost. A reader that follows the pointer reports a shipped disk as damaged, on
   twenty of sixty-three entries.
2. **Do not validate catalog names as printable text.** Eight of these thirty
   bytes are below `$20` once the high bit is stripped. A printable-only check
   looks reasonable, and rejects twenty entries a vendor shipped. This was tried
   and reverted; do not try it again without new evidence.
3. **Render them.** DOS renders them, the vendor's own captured listing shows
   them, and hiding them makes a listing disagree with the machine's. Anyone who
   wants them gone is asking for a filter, which is a different request.

## ProDOS text on these volumes is high-bit, and mixed

Contrary to the widely repeated claim that ProDOS `TXT` is plain seven-bit ASCII
with `$0D`. Measured here:

| File | Volume | Finding |
|---|---|---|
| `SENDMSG.S` | `/MERLIN/LIB` | 149 of 149 bytes high-bit, 26 high spaces, 15 `$8D` terminators |
| `APPLESOFT.S` | `/APPLESOFT` | predominantly high-bit with `$8D`, and mixed |
| `PI.NAMES.S` | `/APPLESOFT` | 223 of 256 high, with 33 plain `$20` spaces among them |

The conclusion is narrower than either claim: **the `TXT` type does not imply a
convention; the producer does.** So a decoder must strip bit 7 and may never
assert it, on *either* filesystem. The `UnitTest/Fixtures/Merlin` README documents
the same rule for DOS 3.3 with the field-separator-versus-comment-space detail.

## What these volumes cannot exercise

Stated so neither reads later as a coverage gap. Both need a constructed
fixture, which is the legitimate use of one, these volumes simply cannot reach
the shape:

| Shape | Why not |
|---|---|
| ProDOS **tree** storage | A tree needs more than 256 data blocks. These are 280-block volumes and the largest file on any of them is `WHATSIT.A.Q` at 60 blocks, so no tree exists here and none could. |
| **Random-access** text | Every `TXT` file across both ProDOS volumes has an auxiliary type of 0, meaning sequential. None sets a record length, so the form where the auxiliary type *is* the record size and unwritten records are sparse holes has no real sample. |

The second is the more dangerous of the two, because a reader that copied
`aux_type` into the load address regardless of file type would report `$0000` for
every text file here and be indistinguishable from a correct one.

## What each volume exercises

| | |
|---|---|
| `Merlin-proDos2.23` | DOS 3.3: 63 catalog entries chained across multiple catalog sectors, locked files, decorative zero-sector entries, both `T` and `B` file types, track/sector lists, and files large enough to need more than one. |
| `2.33-a` (`/MERLIN`) | ProDOS with **subdirectories**: `SOURCEROR`, `LIB`, `SOURCE`, `PI`, `UTIL`: so directory traversal is exercised rather than assumed. `SYS`, `BIN`, `DIR` and `TXT` types; `<NO DATE>` timestamps; per-file load addresses and record lengths. |
| `2.33-b` (`/APPLESOFT`) | ProDOS storage types beyond seedling: `WHATSIT.A.Q` is 29798 bytes across 60 blocks, so it is a sapling with an index block. Also `BAS` and `SYS` types, and a nearly full volume, 8 blocks free of 280, for free-space edges. |

`2.33-a` also carries its own copy of `LABELS`/`LABELS.S`, at different sizes
from the DOS 3.3 disk's. That is a later build, not a discrepancy: source and
object correspond **within** a disk, never across disks. The `UnitTest/Fixtures/Merlin`
oracle pairs are all drawn from the single DOS 3.3 image for that reason.

## Rules

- Access through `IFixtureProvider::OpenFixture()` with a path relative to
  `UnitTest/Fixtures/`, e.g. `Disks/Merlin-proProdos2.33-a.dsk`.
- **Never write to a fixture**, including through a mounted drive. Copy the
  bytes and mutate the copy.
- Never modify these files to make a test pass. See license point 3.
- Re-verify or re-obtain with `scripts/FetchMerlin.ps1`.

## See also

- `scripts/FetchMerlin.ps1`: obtains and hash-verifies these files
- `UnitTest/Fixtures/Merlin/`: source/object oracle pairs from the DOS 3.3 image
- `specs/020-disk-file-access/spec.md`: the filesystem layer these validate
