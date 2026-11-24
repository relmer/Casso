# Research: WOZ Format Stamp

## R1. Where the stamp is stored

- **Decision**: a WOZ chunk with the id `CSSO`, placed after every chunk Casso writes today, payload UTF-8 JSON.
- **Rationale**: the WOZ reference requires readers to skip unknown chunk ids, so Applesauce and other emulators load a stamped file. `WozLoader` already reads unknown chunks into `WozMetadata::passThrough` and writes them back in `Serialize`, so the chunk survives saves by every Casso build, including builds before this feature.
- **Alternatives considered**: META keys (flat tab-separated text meant for catalog fields; no room for per-track lists, and other tools display META); a sidecar file (separates from the image on copy); a binary chunk (not readable by a person, and every other tool would require a custom parser).

## R2. JSON support

- **Decision**: use the tree's `JsonValue`, `JsonParser` and `JsonWriter` (`CassoEmuCore/Core`).
- **Rationale**: no new dependency; the parser already reports errors without throwing, and `JsonWriter` writes compact or pretty text.
- **Alternatives considered**: a separate serializer for this one format (a second JSON implementation).

## R3. Which analysis produces the entries

- **Decision**: `DiskAnalyzer::Analyze` over a `DiskCopy` of the image, with the decode settings in effect (standard for a capture), then a new `FormatStampBuilder` that reads the `DiskAnalysis` and each `TrackAnalysis`.
- **Rationale**: FR-013 requires one analysis for capture and inspector. `TrackAnalysis` already contains each field's `prologueFound` and `epilogueFound`, sector and volume numbers, and checksum results, which covers FR-004. `DiskAnalyzer::Reanalyze` reanalyzes a subset of records, as the write path (FR-021) requires.
- **Alternatives considered**: a separate capture-side analysis in `CassoLinkCapture` (a second definition of "the marks a track uses").

## R4. Recording marks that detection does not try

`TrackAnalyzer` finds only the marks its `FieldMarks` contains. For a capture, the standard settings would miss a track whose prolog no list contains, so the stamp would record it as Nonstandard with no marks.

- **Decision**: add a mark discovery step to the builder. For a track classified Nonstandard or with fewer good sectors than fields, scan the framed nibbles for three-nibble sequences that are followed by a valid 4-and-4 address body whose checksum matches (an address prolog candidate), and for sequences followed by a 6-and-2 or 5-and-3 body whose checksum matches (a data prolog candidate). Analyze the track again with the candidates as custom marks, and use them if more sectors decode with good checksums.
- **Rationale**: a checksum-verified address body after a candidate is strong evidence; a person finds the marks in the inspector the same way today. It produces exact marks for the Ultima program disk's per-track variants without any built-in list.
- **Alternatives considered**: record only what the built-in lists find (makes the stamp worthless for exactly the disks it is for); record wildcard marks (FR edge case requires the nibbles found).

## R5. How the host decoder uses the stamp

The 16-sector decoder (`TrackSectorDecoder`) compares against constants; the 13-sector decoder (`ThirteenSectorCodec`) uses `FieldLocator` with `FieldMarks`.

- **Decision**: add a `StampedTrackMarks` value (address prologs, data prologs, epilogs, and whether checksums are checked) that both decoders accept as an optional argument. `Denibblizer` and `ThirteenSectorCodec::Denibblize` decode each track with detection first. If every sector verified, that result is final. Otherwise, when the stamp has an entry for the track, they decode it again with the stamped marks and use, per sector, the better outcome: `Verified`, then `Recovered`, then `Lost`.
- **Rationale**: this ordering satisfies FR-016 by construction: the result for each sector is at least as good as detection's, and a stamp cannot replace a verified sector. Standard tracks pay nothing. FR-014's "stamped marks first" is met in effect, since the stamped result is used wherever it is better, and the order in which the two decodes run is invisible to callers.
- **Alternatives considered**: stamp first, detection on failure (a stamp whose marks produce a valid but different sector, for example a decoy field, could replace detection's data and violate FR-016); stamp only (violates FR-016 for stale stamps).

## R6. Where the image carries the parsed stamp

- **Decision**: `WozMetadata` gains a parsed `std::optional<FormatStamp>` beside `passThrough`, plus the original chunk bytes. `WozLoader::Load` parses the first `CSSO` chunk; a parse failure, a newer major version or a size over 1 MB results in an empty optional, and the bytes stay in `passThrough` as any unknown chunk's do.
- **Rationale**: every host reader already has the `DiskImage`, and through it `GetWozMetadata()`, so no signature outside the decoders changes.

## R7. Keeping the stamp accurate on save

- **Decision**: `WozLoader::Serialize` calls a `FormatStampRefresher` before writing chunks. When no track is dirty, the chunk is written from its original bytes (FR-024). When tracks are dirty and the stamp parsed, the refresher reanalyzes the dirty tracks, replaces their entries, clears their capture facts, sets `changedAfterCapture`, and writes the stamp again. When tracks are dirty and the stamp did not parse, the chunk is removed (FR-023).
- **Rationale**: `DiskImage` has per-track dirty bits until the flush clears them, so `Serialize` is the one point with access to both the bits and the set of changed tracks. File commits (put, replace) go through the same flush.
- **Alternatives considered**: invalidate at write time in the drive (runs analysis on the emulation thread for every sector write).

## R8. Capture facts

- **Decision**: `CassoLinkCapture::BuildWoz` passes a `CaptureFacts` value built from its `Result` (unstable, weak, replaced, streams, between-track reads, revolution cycles, synchronized) to the builder, then attaches the stamp to the image before `Serialize`.
- **Rationale**: `Result` already contains every fact FR-006 lists. A WOZ rebuilt from saved reads (`ParseReads` then `BuildWoz`) goes through the same path (FR-009).

## R9. Version policy

- **Decision**: `"format": "casso-woz-stamp"`, `"version": "1.0"`. A reader accepts any 1.x and ignores unknown fields; a 2.x stamp is treated as unreadable. Minor versions add optional fields only.
- **Rationale**: lets later releases add facts (protection notes, 3.5" tracks) without breaking 1.0 readers.

## R10. Command line

- **Decision**: a `disk stamp <image>` subcommand in the existing disk command set, printing a table by default and the stored JSON with `--json`. Exit 0 with a stamp, 2 for a WOZ without one, 1 for an error.
- **Rationale**: fits the `DiskCommandRunner` family (`DiskInspectCommand` is the nearest neighbor) and doesn't require a new parser.
