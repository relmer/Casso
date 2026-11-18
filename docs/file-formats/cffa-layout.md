# CFFA card layout

A CFFA card (CompactFlash for the Apple II, by Rich Dreher) divides its
storage into volumes at fixed places. There is no partition map. Casso's
`CffaLayout` finds the volumes of a card or a card image. This document is
the specification that code is written from.

Sources of the facts: CFFA's manuals at dreher.net (version 1.0, and 2.0,
whose firmware maps up to 13 drives), and CiderPress II's CFFA notes
(`DiskArc/Multi/CFFA-notes.md`).

## Layout

Blocks are 512 bytes.

- A 32 MB slot is 65,536 blocks. A ProDOS volume is at most 65,535 blocks, so
  the last block of a slot with a full-size ProDOS volume is unused.
- With the card's jumper in 4-drive mode, slots 1 to 4 start at blocks 0,
  65,536, 131,072 and 196,608.
- In 4-drive mode, Dave Lyons' GS/OS driver for firmware 1.x adds up to two
  more volumes: the 5th starts right after the 4th and is up to 1 GB
  (2,097,152 blocks); the 6th starts 1 GB after the start of the 5th and is
  also up to 1 GB.
- In 8-drive mode, slots 5 to 8 are more 32 MB slots after the first four.
  CFFA 2.0's firmware continues them to 13 slots.
- Volumes are ProDOS or HFS.
- The last volume can be shorter than its slot when the storage ends inside
  it. Storage past the last volume isn't used.

## Recognizing a card

A store of 65,536 blocks or fewer is a single volume, not a card.

A slot is recognized when its first blocks have a ProDOS volume directory or
an HFS master directory block whose block count fits in the slot.

1. Read the first four 32 MB slots, each clipped to the end of the store.
   With none recognized, the store isn't a card.
2. If the store ends inside the first four slots, those are the card.
3. Otherwise read what is after them both ways: as up to two 1 GB volumes,
   and as up to nine more 32 MB slots, each clipped to the end of the store.
   The reading with more recognized volumes is the layout. When both have
   the same number, the 1 GB reading is the layout, because 8-drive mode
   with nothing past slot 5 lists only empty slots.
4. Slots after the last recognized one are left out of the list. Empty
   slots before it are listed, with an unknown volume kind.

A card that was reformatted in the other mode without being erased can be
read the old way, because its old volumes are recognized.
