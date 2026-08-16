# UnitTest/Fixtures/Disks — real Apple II volumes

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
by construction — including where both are wrong. These disks were written in
1984 and 1985 by software that had never heard of Casso, and they carry the
irregularities that come with that: entries for deleted files, decorative
zero-length catalog rows, locked files, missing timestamps, near-exhausted free
space, and a volume whose sector order disagrees with its filesystem.

Synthetic fixtures remain the right default for structural cases where a
specific shape needs to be constructed on purpose. See the parent README.

## License

Merlin Pro is **CC BY-NC-ND 3.0** — https://creativecommons.org/licenses/by-nc-nd/3.0/

- **Author**: Glen Bredon
- **Publisher**: Roger Wagner Publishing, 1984–1985
- **Source**: https://archive.org/details/MerlinProMacroAssembler

The license permits verbatim redistribution for non-commercial purposes with
attribution, which is what this directory does and all it does. Four points,
because this directory goes further than any other fixture here:

1. **These files are not MIT.** Casso is MIT; this directory is not. The
   distinction is per-file and stops at this directory.
2. **They contain executable software**, not just data — `MERLIN.SYSTEM`,
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

## All three images are in DOS sector order

Including the two holding ProDOS volumes. This is the single most useful
property of the set, and it is not a quirk — it is how these images were
captured and how a great many `.dsk` files in the wild are.

It means a reader cannot infer the filesystem from the sector order, or the
sector order from the filesystem. The two are independent, and both have to be
determined. A ProDOS volume in DOS order puts block 2 at track 0 sector 11
rather than at byte offset 1024.

## The DOS 3.3 image defeats a naive ProDOS probe

`Merlin-proDos2.23.dsk` is a DOS 3.3 disk. Probe it as though it were
ProDOS-ordered and read the storage-type nibble at offset 1024 — where a ProDOS
volume directory header would sit — and the high nibble is `$F`, exactly what a
volume header looks like. The "volume name" that follows decodes to garbage,
because those bytes are 6502 boot code that happens to land that way.

A format detector that checks one field misidentifies this disk. That makes it a
negative test worth keeping: format detection must corroborate across more than
a single nibble, and this image proves the point without anyone having to
construct a hostile case.

## What each volume exercises

| | |
|---|---|
| `Merlin-proDos2.23` | DOS 3.3: 63 catalog entries chained across multiple catalog sectors, locked files, decorative zero-sector entries, both `T` and `B` file types, track/sector lists, and files large enough to need more than one. |
| `2.33-a` (`/MERLIN`) | ProDOS with **subdirectories** — `SOURCEROR`, `LIB`, `SOURCE`, `PI`, `UTIL` — so directory traversal is exercised rather than assumed. `SYS`, `BIN`, `DIR` and `TXT` types; `<NO DATE>` timestamps; per-file load addresses and record lengths. |
| `2.33-b` (`/APPLESOFT`) | ProDOS storage types beyond seedling: `WHATSIT.A.Q` is 29798 bytes across 60 blocks, so it is a sapling with an index block. Also `BAS` and `SYS` types, and a nearly full volume — 8 blocks free of 280 — for free-space edges. |

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

- `scripts/FetchMerlin.ps1` — obtains and hash-verifies these files
- `UnitTest/Fixtures/Merlin/` — source/object oracle pairs from the DOS 3.3 image
- `specs/020-disk-file-access/spec.md` — the filesystem layer these validate
