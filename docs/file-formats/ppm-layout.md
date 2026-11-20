# Pascal ProFile Manager volumes

Apple's Pascal ProFile Manager (PPM) puts up to 31 UCSD Pascal volumes in a
region of a ProDOS volume, for Apple Pascal 1.2 and later on a ProFile hard
disk. Casso's `PpmLayout` finds them. This document is the specification
that code is written from.

Sources of the facts: ProDOS 8 Technical Note #25, "Non-Standard Storage
Types", the *Pascal ProFile Manager Manual*, and CiderPress II's PPM notes
(`DiskArc/Multi/PPM-notes.md`).

## Layout

All numbers are little-endian. Blocks are 512 bytes.

The region is the file `PASCAL.AREA` in the ProDOS root directory, file
type PAS ($EF), storage type $4. No other file has storage type $4. Its key
pointer is the region's first block and its blocks used is the region's
size.

A root directory block has a 4-byte header (the previous and next block of
the directory) and 13 entries of 39 bytes. An entry's first byte is its
storage type in the high 4 bits and its name length in the low 4; the name
follows. The key pointer is at +$11 of the entry and the blocks used at
+$13. The directory starts at block 2.

The first two blocks of the region, taken as one 1K map:

```
+$000 / 2: the region's size in blocks
+$002 / 2: the number of volumes, 1 to 31
+$004 / 4: $03 "PPM", the signature
+$008 / 8: volume 0's info, not used
+$010 / 8: volume 1's info
  ...
+$0F8 / 8: volume 31's info
+$100 /16: volume 0's description, not used
+$110 /16: volume 1's description: a length byte and ASCII text
  ...
+$2F0 /16: volume 31's description
+$300 / 8: volume 0's name, not used
+$308 / 8: volume 1's name, a copy of the name on the volume
  ...
```

A volume's info:

```
+$00 / 2: the volume's first block, counted from the start of the ProDOS
          volume, not of the region
+$02 / 2: the volume's length in blocks
+$04 / 1: the default unit
+$05 / 1: write protection, in the high bit
+$06 / 2: the old driver's address
```

Volumes are in ascending order of first block. A deleted volume isn't in
the map.

## Recognizing the volumes

1. The store is a ProDOS volume of at most 65,535 blocks.
2. The root directory has a `PASCAL.AREA` entry of storage type $4 whose
   region is at least the 2 map blocks and inside the ProDOS volume.
3. The map has the signature and a volume count of 1 to 31.
4. Each volume from 1 to the count is listed, with its description as its
   name, unless it ends past the ProDOS volume, its description's length
   is 16 or more, or it starts before the end of the volume listed before
   it. The volume kind is detected from the volume's blocks.
