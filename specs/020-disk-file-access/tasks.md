# Tasks: Disk File Access for the Build Loop

**Input**: Design documents from `/specs/020-disk-file-access/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: INCLUDED — Constitution Testing Discipline is non-negotiable; every
pure component ships its unit suite in the same phase. Guest-visible gates reuse
the real-CPU DOS-boot harness with graceful SKIP-if-missing.

**Organization**: Phases follow plan.md's **dependency order**, not story order.
User Story 1 is **DELIVERED** (shipped `--raw` / `--dos-bin`) and has no tasks.
US3 (read) precedes US2 (write) because extraction is what a migrating developer
needs first and because a write path must not be built before the decode report
exists. Constitution commit discipline: commit + push after each completed phase.

## Format: `[ID] [P?] [Story] Description`

---

## Phase 1: Setup

- [ ] T001 Add EHM-conformant stubs for every new file and wire them into `CassoEmuCore/CassoEmuCore.vcxproj(.filters)`, `CassoCore/CassoCore.vcxproj(.filters)`, `CassoCli/CassoCli.vcxproj(.filters)`, `UnitTest/UnitTest.vcxproj(.filters)`: `CassoEmuCore/Devices/Disk/{IVolume.h, VolumeTypes.h, Dos33Volume.h/.cpp, ProDosVolume.h/.cpp, VolumeIntegrityReport.h/.cpp, TrackWritability.h/.cpp}`, `CassoCore/{AppleTextCodec.h/.cpp, ApplesoftTokenizer.h/.cpp}`, `CassoCli/FileCommit.h/.cpp`, `UnitTest/EmuTests/{Dos33VolumeTests.cpp, VolumeIntegrityTests.cpp}` — x64 Debug compiles clean

---

## Phase 2: Foundational (blocking prerequisites — plan.md Phase A)

**Nothing in any later phase may consume denibblized output until this phase is
complete.** T003–T005 are the fix for a defect that is live on the emulator's
flush path today; building a write path on top of the current decoder would ship
it a second time.

- [ ] T002 [P] Define `SectorDecodeReport` and `TrackDecodeOutcome` (`Complete` / `Unformatted` / `Partial`) with per-track 16-bit `coverage`, `duplicated`, `hasDataLoss`, `unrecoveredCount` in `CassoEmuCore/Devices/Disk/VolumeTypes.h` per data-model.md
- [ ] T003 Rewrite the per-track decode loop in `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp` (currently lines 762–780): continue past a failed sector and resynchronize on the next address prologue instead of `break`; maintain the coverage mask; classify the track by **coverage** (`Complete` iff all sixteen logical sectors filled, each exactly once), using address-fields-found to separate `Unformatted` from `Partial`. Add the four-argument `Denibblize` overload carrying `SectorDecodeReport`
- [ ] T004 Rewire the existing three-argument `Denibblize` in `CassoEmuCore/Devices/Disk/NibblizationLayer.h/.cpp` to **forward to the reporting form and fail when the report shows data loss**, succeeding only when every track is `Complete` or `Unformatted`. It must not remain a reportless passthrough — `DiskImage::Serialize` (`DiskImage.cpp:434`) is the sole production caller and the one place the defect matters. Verify all twelve existing call sites still compile unchanged
- [ ] T005 In `UnitTest/EmuTests/NibblizationTests.cpp`: narrow the comment on `Denibblize_UnformattedTrack_ZeroFillsThatTrackAndKeepsOthers` (line 308) so it claims correctness **only** for the wholly-unformatted case — as written it generalizes to "missing sectors read back as zeros … not silent corruption", which is the standing license that let the defect survive. Add the three siblings that pin what it did not cover: `Denibblize_PartiallyDecodableTrack_ReportsDataLossAndDoesNotZeroTail`, `Denibblize_OutOfRangeSectorNumber_ReportsIncompleteCoverage`, `Denibblize_DuplicateSectorNumbers_ReportsIncompleteCoverage` — each built by nibblizing a valid image then patching one address field's 4-and-4 sector value. The existing test must keep passing unchanged
- [ ] T006 [P] Implement `TrackWritability` in `CassoEmuCore/Devices/Disk/TrackWritability.h/.cpp`: whole-image refusal first (quarter-track map resolving any position off `qt / 4` via `DiskImage::ResolveQuarterTrack`; image metadata declaring timing-sensitive capture), then per-track writable iff `Complete` or `Unformatted`. Positive proof only — never protection-scheme recognition. Tests in `UnitTest/EmuTests/NibblizationTests.cpp`
- [ ] T007 [P] Implement `NibblizationLayer::RenibblizeTracks` (re-encode only the listed tracks into `DiskImage::GetTrackBitsForWrite`, leaving every other track's packed bits byte-identical) in `CassoEmuCore/Devices/Disk/NibblizationLayer.cpp`; test that an unrelated track's bits are bit-for-bit unchanged after a write elsewhere
- [ ] T008 [P] Define `FilePath` (path-based from the outset per FR-009), `FileEntry`, `FilePayload`, and `VolumeListing` in `CassoEmuCore/Devices/Disk/VolumeTypes.h` per data-model.md — fields a filesystem does not store are absent, not zero, so "no load address" is distinguishable from "loads at $0000"
- [ ] T009 Define the `IVolume` seam in `CassoEmuCore/Devices/Disk/IVolume.h` per contracts/volume-api.md: `Enumerate`, `Read`, `Write`, `Delete`, `BuildIntegrityReport`, `SetStartupProgram`. Every mutating call takes the current sector buffer and yields a **new** one — nothing mutates in place
- [ ] T010 Implement `VolumeIntegrityReport` in `CassoEmuCore/Devices/Disk/VolumeIntegrityReport.h/.cpp`: `claimedBy`, `crossLinked`, `allocatedButUnclaimed`, `claimedButFree`, `unfollowableChains`, `catalogFullyParsed`, `isClean`. **Traversal MUST terminate on any input** (FR-038) — visited set plus a ceiling derived from volume capacity; a chain hitting the bound is recorded unfollowable, never followed. Tests in `UnitTest/EmuTests/VolumeIntegrityTests.cpp` including cyclic and self-referential chains (SC-010)

**Checkpoint**: the decoder no longer loses data silently, and the integrity pass
exists for all four of its consumers.

---

## Phase 3: User Story 3 — Read a disk image's contents (P1)

**Goal**: list a volume and extract files from it, across every mountable format.

**Independent test**: quickstart §US3 — list a known disk and confirm name, type,
size, and lock state for every file plus free space; extract one and compare
byte-for-byte; repeat against `.dsk`, `.do`, `.po`, and `.woz` of the same content.

**Why first**: a migrating developer's source is on Apple II disks and cannot be
edited until it comes off. Nothing else in the feature is reachable for them
until this exists.

- [ ] T011 [US3] Implement `Dos33Volume` read side in `CassoEmuCore/Devices/Disk/Dos33Volume.h/.cpp` — **no DOS 3.3 reader exists today**: walk the VTOC (T17 S0) and catalog chain (T17 S15→S1), decode each entry's name (30 bytes high ASCII), type byte with the `$80` lock bit masked off, sector count, and track/sector list; `Enumerate` and `Read`. Reuse `Dos33Skeleton`'s geometry constants rather than restating offsets. Tests in `UnitTest/EmuTests/Dos33VolumeTests.cpp`
- [ ] T012 [P] [US3] Implement `Dos33Volume::BuildIntegrityReport` (walk every catalog entry's T/S chain into the claim map; compare against the VTOC free bitmap) in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`; damaged-volume cases in `UnitTest/EmuTests/VolumeIntegrityTests.cpp`
- [ ] T013 [US3] Implement `ProDosVolume` read side in `CassoEmuCore/Devices/Disk/ProDosVolume.h/.cpp`: wrap the existing `ProDosReader::ExtractFile` behind `IVolume`, add `Enumerate` over the volume directory, and add **path-based traversal** so a file inside a subdirectory is reachable (FR-009). Where traversal is not yet supported, refuse a multi-component path with a clear reason — never silently truncate. Extend `UnitTest/EmuTests/ProDosVolumeTests.cpp`
- [ ] T014 [P] [US3] Implement `ProDosVolume::BuildIntegrityReport` (walk seedling / sapling / tree block chains into the claim map; compare against the volume bitmap) in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`
- [ ] T015 [P] [US3] Implement `AppleTextCodec` in `CassoCore/AppleTextCodec.h/.cpp`: high-ASCII ↔ host text with line-ending normalization both directions (FR-021); round-trip tests in `UnitTest/`
- [ ] T016 [US3] Extend the command-line surface **additively**: add `Disk` to `CommandLineOptions::Subcommand`, a `DiskVerb` enum and operand fields in `CassoCore/CommandLineOptions.h`; **one row** `{ "disk", …Subcommand::Disk }` in `s_kSubcommands` and **one arm** calling a new `ParseDiskOptions` in `CassoCore/CommandLineParser.cpp`. Do not reshape the dispatcher. Accept `ls`→`list` and `rm`→`delete` aliases. **`UnitTest/CommandLineTests.cpp` must stay green — spec 019 is being developed concurrently against these files**; add disk-grammar cases beside the existing ones
- [ ] T017 [US3] Implement `DoDisk` list/get in `CassoCli/CommandLine.cpp`: read the image, denibblize with the report, dispatch to the volume, print the listing or extracted bytes to **stdout**
- [ ] T018 [US3] Implement the exit-status and stream contract in `CassoCli/CommandLine.cpp`: `0` clean, `1` succeeded-with-complaints (damage on stderr, usable listing still on stdout), `2` produced no output — matching what `as65` and `run` already return. Failure messages name image, file, and reason, on stderr (FR-031, FR-033). Covers spec US3 acceptance 4
- [ ] T019 [US3] Cross-format extraction gate (SC-004): the same content as `.dsk`, `.do`, `.po`, and `.woz`; every file extracts byte-identically from each. Plus the damaged-volume cases from quickstart §US3.5–7 — partial track reports unrecovered sectors as unrecovered (never zeros), wholly unformatted track reports blank not damage
- [ ] T020 [US3] Runtime validation pass over quickstart §US3; fix what it finds

**Checkpoint**: US3 is complete and independently shippable — a developer can get
their source off an Apple II disk, which is the first thing they need.

---

## Phase 4: User Story 2 — Put a file onto a disk image (P1)

**Goal**: place a binary on a disk, replacing any file of the same name, with
every failure leaving the image byte-for-byte unchanged.

**Independent test**: quickstart §US2 — place a 512-byte binary as `PROG` at
`$6000`, boot, confirm `CATALOG` lists `B 002 PROG` and `BLOAD PROG` lands the
bytes; then run every failure mode and confirm the image hash is unchanged.

**Note**: delete lands here, not later. Replace is delete + write, and neither
filesystem can delete today.

- [ ] T021 [US2] Generalize DOS 3.3 writing into `Dos33Volume::Write` in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: allocate from the VTOC free bitmap, build the track/sector list, write data sectors, create the catalog entry, leave the bitmap consistent. `Dos33FileWriter::WriteHello` is a zero-parameter hardcoded emitter — treat it as a worked example of the structures, not a base to extend. Tests in `UnitTest/EmuTests/Dos33VolumeTests.cpp`
- [ ] T022 [US2] Implement `Dos33Volume::Delete` with free-space return in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: free **only** sectors the integrity report shows this file uniquely owns, report the rest as leaked, and remain available for a file whose T/S chain is damaged so a bad file cannot strand the volume (FR-011). Warn distinctly when `catalogFullyParsed` is false (FR-040)
- [ ] T023 [US2] Add **tree growth** to the ProDOS writer (master index block of index blocks) in `CassoEmuCore/Devices/Disk/ProDosSkeleton.cpp` / `ProDosVolume.cpp` — the existing writer handles seedling and sapling only; extend `ProDosVolume::Write` to grow storage type as size requires (FR-008). Block-accounting asserted against `BuildIntegrityReport`, not by inspection
- [ ] T024 [US2] Implement `ProDosVolume::Delete` with volume-bitmap free-space return in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`, under the same unique-ownership rule as T022
- [ ] T025 [US2] Implement replace in both volumes (FR-012): compute the complete post-replacement buffer as a **whole** — never a delete applied to the target followed by a write, which would free the old file and lose it outright if the write then failed. Test that a write failing after the delete step leaves the original file intact
- [ ] T026 [US2] Wire the pre-commit self-check (FR-039): every `Write` and `Delete` runs `BuildIntegrityReport` over its **computed result** and refuses to return a result that fails it — sector claimed twice, free map disagreeing with the catalog, chain broken by the edit. Feed the path a deliberately corrupted result and confirm refusal (SC-009)
- [ ] T027 [US2] Implement the bit-stream write path in `CassoEmuCore/Devices/Disk/`: diff the pre- and post-edit sector buffers, call `RenibblizeTracks` for **only** the changed tracks, and refuse via `TrackWritability` when the write needs a track that is not writable (FR-016, FR-017). Test that writing to a WOZ leaves untouched tracks bit-identical, and that a `Partial` track refuses the write with the image unchanged (SC-008)
- [ ] T028 [US2] Implement `FileCommit` in `CassoCli/FileCommit.h/.cpp`: write to a **uniquely named** temporary file beside the target, flush, then `ReplaceFileW` onto the original; remove the temporary on any failure. Names must not collide between concurrent invocations, and a hard kill must leave no stray file (FR-013)
- [ ] T029 [US2] Add the staleness re-verify and best-effort probe in `CassoCli/CommandLine.cpp`: record size and modification time at read, re-verify immediately before commit and refuse if either changed (FR-036); best-effort exclusive-open probe refusing when **another** holder has the file open, with help text that does not imply it detects Casso (FR-035)
- [ ] T030 [US2] Add `put` and `delete` verbs to `ParseDiskOptions` and `DoDisk` (`--as`, `--type`, `--addr`, `--text`/`--basic`/`--raw`), with write-protect and locked-file refusals reported in intelligible terms rather than raw platform codes (FR-014)
- [ ] T031 [US2] Guest-visible gate (SC-003): real-CPU tests — place a binary on a DOS 3.3 image, boot, `CATALOG` shows `B 002 PROG`, `BLOAD PROG` lands bytes at `$6000`; same payload on ProDOS shows type `BIN` aux `$6000`. Attempt to overwrite the stock master's locked `HELLO` (type `$82`) and confirm refusal
- [ ] T032 [US2] Failure-mode gate (SC-005): for **every** documented failure — volume full, locked file, write-protected image, illegal name, unwritable track, stale target — hash the image before and after, confirm byte-identical, confirm still bootable, and confirm no temporary file remains
- [ ] T033 [US2] Runtime validation pass over quickstart §US2 plus the manual interrupted-write check (kill mid-commit; original intact and bootable, no temp left); fix what it finds

**Checkpoint**: the minimum viable loop is closed — assemble, place, boot, run.

---

## Phase 5: User Story 4 — Make the disk boot the program (P2)

**Goal**: boot straight into the developer's program with no typing.

**Independent test**: quickstart §US4 — set a boot program, boot the image, the
program runs unattended.

- [ ] T034 [US4] Implement `Dos33Volume::SetStartupProgram` in `CassoEmuCore/Devices/Disk/Dos33Volume.cpp`: patch the greeting filename **in place at T01 S09 `+$75`**, 30 bytes, high ASCII, `$A0`-padded (verified against the stock master in research R-003). No catalog change, no chaining file
- [ ] T035 [US4] Implement `ProDosVolume::SetStartupProgram` in `CassoEmuCore/Devices/Disk/ProDosVolume.cpp`: reorder the volume directory so the chosen `SYS` file is the first the boot path finds. **Deliberately not shared with T034** — the two mechanisms differ in kind, and a unified "write the boot name" helper would be wrong for both
- [ ] T036 [US4] Add the `boot` verb to `ParseDiskOptions` and `DoDisk`; refuse a program not present on the volume, naming the missing file (FR-024)
- [ ] T037 [US4] Boot gate: real-CPU tests — a DOS 3.3 image with a set boot program runs it unattended after DOS loads; a ProDOS image launches the chosen system program

---

## Phase 6: User Story 5 — Boot with no operating system (P3)

**Goal**: an image that boots directly into a binary, with no DOS or ProDOS.

**Independent test**: quickstart §US5 — the payload runs measurably sooner than
the equivalent OS boot.

- [ ] T038 [US5] Implement direct-boot image generation in `CassoEmuCore/Devices/Disk/`: a boot-sector loader that pulls the payload's sectors and jumps to it (FR-025)
- [ ] T039 [US5] Refuse a payload exceeding what the boot path can load, reporting the available capacity (FR-026); support an entry address different from the load address (FR-027)
- [ ] T040 [US5] Gate (SC-007): real-CPU test — direct-boot image reaches the payload measurably faster than the equivalent DOS 3.3 boot of the same program

---

## Phase 7: User Story 6 — BASIC source as a runnable program (P3)

**Goal**: place an Applesoft listing written as host text as a program the guest
can `RUN`.

**Independent test**: quickstart §US6 — place a listing, boot, `LIST` reproduces
the source.

- [ ] T041 [US6] Implement Applesoft tokenization in `CassoCore/ApplesoftTokenizer.h/.cpp`: host-text listing → tokenized on-disk form with line-link fixups (FR-022). Settle the coverage boundary against a real listing — token spellings inside strings, `DATA` payloads, `REM` text
- [ ] T042 [US6] Implement detokenization (the reverse direction, FR-022) in `CassoCore/ApplesoftTokenizer.cpp`; round-trip tests
- [ ] T043 [US6] Refuse an untokenizable listing with the offending line number and text quoted (FR-023); wire `--basic` through `put` and `get`
- [ ] T044 [US6] Gate: place a known listing, boot, `LIST` in the guest, confirm it matches the source

---

## Phase 8: Polish & Cross-Cutting

- [ ] T045 Help output (FR-034, SC-002): every capability documented, with a **worked example of the whole loop** — assemble, put, boot — not just a flag list, since SC-002 requires a newcomer to complete the loop from help alone. Say that `put`/`get` are named from the disk's perspective
- [ ] T046 Update `CHANGELOG.md` and `README.md` (user-visible feature, test-count change); document the deliberate asymmetry that command-line writes are crash-safe while emulator flushes are not, and that in-use detection is out of scope
- [ ] T047 Pre-merge gates: `scripts/RunTests.ps1 -Build` for x64 Debug **and** Release (different test sets — Release is not a substitute), `scripts/Build.ps1 -RunCodeAnalysis` clean, `scripts/CheckStyle.ps1` clean; merge to master with `--no-ff`
- [ ] T048 File the pre-existing denibblization defect as its own GitHub issue — the emulator-side flush exposure described in research R-002 predates this feature and should not be folded silently into it

---

## Dependencies

```text
Phase 1 (Setup)
    ↓
Phase 2 (Foundational) ──── blocks EVERYTHING; T003–T005 are a data-loss fix
    ↓
Phase 3 (US3, P1 read) ──── independently shippable
    ↓
Phase 4 (US2, P1 write) ─── needs US3's readers + delete; closes the MVP loop
    ↓
    ├── Phase 5 (US4, P2)
    ├── Phase 6 (US5, P3) ── independent of 4/5; needs only Phase 2
    └── Phase 7 (US6, P3)
    ↓
Phase 8 (Polish)
```

**Within Phase 2**: T002 ∥ T006 ∥ T007 ∥ T008 are parallel; T003 → T004 → T005 is
a chain; T009 needs T008; T010 needs T008.

**Within Phase 3**: T011 ∥ T013 ∥ T015 (different files); T012 needs T011; T014
needs T013; T016 → T017 → T018.

**Within Phase 4**: T021 → T022 and T023 → T024 are two independent chains that
can run in parallel; T025 needs both; T026 needs T025; T028 ∥ T029.

## Implementation Strategy

**MVP = Phases 1–4.** US1 is already shipped; US3 then US2 closes the loop the
feature exists to provide. Stop there and the feature is genuinely useful:
extract source off old disks, assemble on the host, place the result back,
boot it.

Phases 5–7 are refinements, each independently valuable, none gating the
migration. Phase 6 (US5) depends only on Phase 2 and can be pulled forward if
direct-boot turns out to matter more than boot configuration.

**The one ordering that is not negotiable** is Phase 2 before any write path.
The decoder currently returns success over data it silently zeroed; building a
write path on top of that ships the same defect from a second direction.
