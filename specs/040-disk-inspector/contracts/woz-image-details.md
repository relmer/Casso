# Contract: What Casso keeps from a WOZ file, and how it loads damaged records

**Covers**: FR-050 to FR-054, FR-071, SC-010, SC-013. **Decisions**: research
R8, R9, R17.

## Kept on load

`WozMetadata` gains `WozInfo` and `WozFileLayout` (data-model.md, section 1).
They are filled by `WozLoader::Load`, copied wherever `WozMetadata` is copied
(including `DiskImageStore.cpp:1501`), and never re-read from the file for the
Image tab.

`WozLoader::ReadInfo` is the only INFO parser. `WozLoader::Describe` and
`WozCompatibility::ReadRequirements` call it; neither reads INFO bytes of its
own. `ReadRequirements` keeps its signature.

## Loading records a map entry points at

| Record | Loads as | Image writable | Shown as |
|---|---|---|---|
| All fields zero | Empty slot | Yes | "Nothing recorded" (TMAP) or an empty flux track (FLUX); image file problem "map entry points at an empty record" |
| Count > 0, start block 0 or block count 0 | Damaged, `RecordLocationMissing` | No | Damaged with the reason; the Image tab shows the start block and count it claims |
| Start block 1 or 2 | Damaged, `RecordInHeader` | No | Damaged with the reason |
| Count larger than its blocks (bit or flux) | Damaged, `CountExceedsBlocks` | No | Damaged with the reason |
| TMAP or FLUX entry 160 to 254 | Quarter track damaged, `MapEntryOutOfRange` | No | Damaged with the reason on that quarter track |
| Entry 255 | Unmapped | Yes | "Nothing recorded" |

Every quarter track that maps to a damaged record shows the reason, not only
one of them. Nothing in the file is overwritten when it loads.

041's `ReserveBlankTracks` does nothing on an image with any damage, so every
map entry of a damaged image stays as loaded, whatever its `DamageReason`.

## Saving (WOZ)

- TMAP is written from the stored map for every quarter track whose record
  still exists, so the standard layout, shared records and a bit record that
  FLUX overrides survive a save.
- Records no map refers to keep their bytes.
- Tracks keep their kind; untouched records are written as they were.
- A WOZ 1 is saved as WOZ 2, as today.

## Image file problems (FR-051)

Reported in Findings and the Image tab: checksum mismatch; INFO largest track
smaller than the largest record; a count larger than its blocks hold; a META
value outside the WOZ lists (`language`, `requires_ram`, `requires_machine`);
an `image_date` not in RFC 3339; duplicate or out-of-order chunks; data past
the last chunk; an unreferenced record; a map entry pointing at an all-zero
record; a NIB track with no sync. Only the checksum mismatch, a record that
cannot be read and an out-of-range map entry are called damaged.
