# DOS MASTER volumes

DOS MASTER, by Glen Bredon, puts DOS 3.3 volumes inside a ProDOS volume, most
often several on an 800K disk. Casso's `DosMasterLayout` finds them. This
document is the specification that code is written from.

Source of the facts: CiderPress II's notes on hybrid disks
(`DiskArc/FS/Hybrid-notes.md`).

## Layout

- DOS MASTER takes one run of contiguous blocks, usually at the end of the
  ProDOS volume, and marks them in use in the volume bitmap without a file.
  ProDOS then never allocates those blocks to a file.
- The run is divided into DOS volumes of one size:

  | Volume | Tracks | Sectors a track | Blocks |
  |---|---|---|---|
  | 140K | 35 | 16 | 280 |
  | 160K | 40 | 16 | 320 |
  | 200K | 50 | 16 | 400 |
  | 400K | 50 | 32 | 800 |

- Each volume's sectors are in track and sector order on its blocks: track
  t, sector s is at byte (t × sectors a track + s) × 256 of the volume.
- When the DOS volumes fill the whole disk, the disk's first 7 blocks (the
  ProDOS boot blocks and volume directory) are left to ProDOS, inside the
  first DOS volume, so the disk is a ProDOS volume with no free space.
  An install option sets aside 28K more (56 blocks) for PRODOS and DOS.3.3,
  so the disk can boot into DOS 3.3. The run in the bitmap then starts at
  block 7 or block 63, and the DOS volumes start at block 0.

## Recognizing the volumes

1. The store is a ProDOS volume.
2. The run is the last stretch of blocks marked in use in the volume bitmap,
   counted back from the end of the volume. Only the directory and bitmap
   blocks are read for it.
3. A run that starts at block 7 or block 63 and ends at the last block of
   the volume is extended to block 0.
4. The volume sizes are checked smallest first. A size fits when it divides
   the run exactly and every volume of that size in the run is a DOS 3.3
   volume of its tracks and sectors. The first size that fits is the layout.
5. The layout is confirmed by the ProDOS volume's integrity report: no file,
   directory or volume structure has a block of the run from where the
   bitmap marks it, block 7 or 63 for a run extended to block 0. This reads
   the whole volume, so it comes last.
