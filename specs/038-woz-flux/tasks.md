# Tasks: WOZ Flux Track Support

**Input**: Design documents from `specs/038-woz-flux/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required. The constitution's Principle II applies, and the scenario
suite is mandatory because the drive and the loader change. Every new test is
shown to fail with its implementation stubbed or reverted before it counts.

**Format**: `[ID] [P?] [Story] Description`. `[P]` means the task can run in
parallel (different files, no dependency on an unfinished task).

## Phase 1: Setup

- [X] T001 Run `scripts/FetchRoms.ps1 -Fixtures`, then build x64 Debug through `Casso.sln` and run `scripts/RunTests.ps1` to record a green baseline (total test count) before any change
- [X] T002 Copy `00_Bandits.woz` from the investigation scratchpad (`C:\Users\relmer\AppData\Local\Temp\claude\C--Users-relmer-source-repos-relmer-Casso-worktrees-issue-159-woz-flux\55ca770d-0acf-46e8-a8e7-3242a682d185\scratchpad\00_Bandits.woz`) into this session's scratchpad. It stays outside the repo and is never committed (clarification 1)

---

## Phase 2: Foundational (blocks every story)

- [X] T003 [P] Create `FluxTrack` in `CassoEmuCore/Devices/Disk/FluxTrack.h` and `.cpp`, per data-model.md. Members: `m_bytes` (raw TRKS bytes), `m_totalTicks` (`uint64_t`), and `m_transitionCount`. A factory validates the source: "the byte count is no larger than block count × 512, and the blocks lie inside the file" and "the last byte is not 255". It reports a failure as a `DamageReason` (`OutsideFile`, `CountExceedsBlocks`, `TruncatedRun`), not as a load failure. "A zero byte count is a valid track with no transitions." Add a cursor type {byte index, absolute tick}, plus `FindTransitionAtOrAfter (tick)`, `AdvanceCursor (cursor)` (summing 255 runs and wrapping at `m_totalTicks`), `GetBytes()` and `GetTotalTicks()`. Add both files to `CassoEmuCore.vcxproj` and its `.filters`
- [X] T004 [P] Write `UnitTest/EmuTests/FluxTrackTests.cpp` and add it to `UnitTest.vcxproj`. It covers total ticks, a 255 run decoding as one gap (255, 255, 10 → 520), wrap at the end of the revolution, seek to a tick inside a gap, zero transitions, and each validation failure (a trailing 255, a count larger than the blocks hold, blocks outside the file)
- [X] T005 Extend `DiskImage` in `CassoEmuCore/Devices/Disk/DiskImage.h` and `.cpp`:
  - add free enums `TrackKind { Bits, Flux }` and `DamageReason { OutsideFile, CountExceedsBlocks, TruncatedRun, V1RecordPastTrks }`;
  - add `m_slotKind` and `m_fluxTracks`, indexed by slot (TRKS index);
  - add `GetTrackKind (slot)` and `GetFluxTrack (slot)` (const and for-write);
  - make `EnsureTrackSlots` size the new vectors and `IsTrackDirty` cover flux slots.

  Bit-slot behavior does not change
- [X] T006 Add `WozLoader::BuildSyntheticV21` in `CassoEmuCore/Machines/Apple2/Common/WozLoader.h` and `.cpp`, next to `BuildSyntheticV2`. It builds a WOZ 2.1 image from a list of bit tracks and flux tracks, each with its quarter tracks. It lays the file out the way Applesauce does: INFO version 3, FLUX on the first block boundary after TRKS, and INFO bytes +46 and +48 set
- [X] T007 Add the bits-to-flux converter in `UnitTest/EmuTests/FluxTestImages.h` and `.cpp`. It turns a bit stream into flux bytes with a chosen cell length for each stretch, in ticks, carrying the fraction (3.7 µs = 29.6 ticks, 4.1 µs = 32.8 ticks, nominal = 1408/45). Add it to `UnitTest.vcxproj`. Also add it to `ScenarioTests.vcxproj` as `..\UnitTest\EmuTests\FluxTestImages.cpp`, the pattern that project already uses for `GuestSession.cpp` and the other helpers

**Checkpoint**: flux tracks can be built in memory and in synthetic files. The engine and loader don't touch them yet.

---

## Phase 3: User Story 1 - Boot a disk with flux tracks (P1)

**Goal**: quarter tracks mapped in FLUX play by time, and *Bandits* boots.

**Independent test**: a synthetic image that mixes 3.7 µs and 4.1 µs stretches reads each stretch's cell length within one LSS step (SC-002), and *Bandits* reaches its title screen on an Apple //e Enhanced (SC-001, by hand).

### Loader

- [X] T008 [US1] In `WozLoader::Load` (`CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp`), recognize the FLUX chunk in the chunk walk instead of passing it through. "Only a FLUX chunk shorter than 160 bytes refuses the mount as `MalformedWoz`." INFO's version and flux fields are not consulted (R8). After TMAP, parse every TRKS index FLUX references into a `FluxTrack`. Bit Count holds the byte count. Then apply FLUX map entries over TMAP, so FLUX wins (FR-002). Add named constants for INFO offsets +46 (FLUX block) and +48 (largest flux track)
- [X] T009 [P] [US1] Add tests to `UnitTest/EmuTests/WozFluxLoaderTests.cpp` (new file) covering:
  - FLUX parsed into flux slots;
  - FLUX precedence over TMAP for the same quarter track;
  - a FLUX chunk used at INFO version 2 and with INFO +46/+48 zero;
  - a FLUX chunk under 160 bytes refused as `MalformedWoz`;
  - neighboring quarter tracks that point at the same flux track all resolve to it (the bleed edge case);
  - a bit-only image's slots and map unchanged (FR-009).

### Engine

- [X] T010 [US1] Cache the resolved slot in `Disk2NibbleEngine` (`CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h` and `.cpp`). Add `m_slot` and `m_isFluxSlot`, refreshed in `SetCurrentTrack`, `SetDiskImage` and `Reset`, so `StepLss` stops calling `ResolveQuarterTrack` on every clock (R10). Run the existing `Disk2NibbleEngine` tests plus `-Filter Disk` to show no behavior change
- [X] T011 [US1] Add the flux time base to `Disk2NibbleEngine`. Use `m_fluxNow` in 1/45-tick units, advanced by named constants (176 per LSS clock; 1408 per nominal cell, derived in a comment from 14.31818 MHz = 315/22 MHz, CPU = 45/44 MHz, 2 LSS clocks per CPU cycle, 8 MHz flux ticks; R1). Keep `m_fluxCursor`, which holds the due time of the next transition. When `m_isFluxSlot` is set, `StepLss` hands the sequencer a pulse on whichever clock the due time falls, then advances the cursor. When it is clear, the bit path is exactly as before, sampling on `kLssReadClock` (R2)
- [X] T012 [US1] Add weak bits on flux tracks to `Disk2NibbleEngine`. Track `m_fluxLastPulse`. Once the gap since the last real transition exceeds four nominal cells (4 × 1408 units), inject `NextWeakBit()` pulses at nominal cell cadence until the next real transition is due (R5, FR-005). This includes the zero-transition track
- [X] T013 [US1] Add head-angle conversion to `Disk2NibbleEngine::SetCurrentTrack` (R4, FR-004). Bit to flux and flux to flux: fraction = position / length, set `m_fluxNow` = fraction × total ticks × 45, then re-seek the cursor with `FindTransitionAtOrAfter`. Flux to bit: `m_bitPos` = fraction × bit count. Bit to bit keeps `m_bitPos %= newBits` unchanged
- [X] T014 [P] [US1] Write `UnitTest/EmuTests/Disk2NibbleEngineFluxTests.cpp` and add it to `UnitTest.vcxproj`. It covers:
  - a nominal flux track of a known nibble pattern reads the same nibbles as the equivalent bit track;
  - a track with 3.7 µs and 4.1 µs stretches gives pulse spacings within one LSS step of 29.6 and 32.8 ticks (SC-002);
  - no drift after 1,000 revolutions (the cursor tick equals the integer expectation);
  - a long gap produces weak bits that vary between revolutions;
  - a zero-transition track reads noise;
  - stepping bit to flux and back mid-revolution keeps the angular fraction within one cell;
  - a bit-only disk's nibble stream is unchanged from before (FR-009).
- [X] T015 [US1] Check *Bandits* by hand (SC-001). Build x64 Release and launch Casso minimized with `--title <worktree name>` and `--disk1 <scratchpad>\00_Bandits.woz` on an Apple //e Enhanced. Boot to the title screen, capture a screenshot, then restore `disk1Path` in UserPrefs. If it hangs, find the failing track with the engine's trace before going further

**Checkpoint**: US1 complete. Flux disks boot and the bit path is unchanged.

---

## Phase 4: User Story 2 - Keep flux tracks intact on save (P2)

**Goal**: drive writes and sector-level writes keep flux tracks as flux, saving round-trips, and the sector-level tools read and write flux disks.

**Independent test**: write one flux track through the drive and one sector through the `disk` path, then flush and reload. Unwritten flux bytes are identical, and both written stretches read back as written (SC-003, FR-013).

### Drive writes and saving

- [X] T016 [US2] Merge `origin/master` into `038-woz-flux` before building writes (plan Risks; CLAUDE.md rename hazard). Rebuild, rerun the unit suite, and fix any stale call sites
- [X] T017 [US2] Add `FluxTrack::SpliceWrite (startTick, bits)` in `CassoEmuCore/Devices/Disk/FluxTrack.cpp` (R6):
  - copy the original flux up to the start tick;
  - write each 1 bit as a transition, with cells of 1408/45 ticks (31.29, clarification 4) rounded to whole ticks with the fraction carried;
  - resume the original stream at the burst's end tick;
  - split each straddling gap so `m_totalTicks` is unchanged;
  - handle a burst that wraps past the end of the revolution;
  - recompute the transition count.
- [X] T018 [P] [US2] Add splice tests to `UnitTest/EmuTests/FluxTrackTests.cpp`: total ticks preserved, bytes outside the burst identical, a burst across the wrap, a burst of all zeros (one long gap), and the written bits reading back as written through a cursor
- [X] T019 [US2] Add the write burst to `Disk2NibbleEngine`. While writing on a flux slot, append each written bit to `m_writeBurst` and record the start tick. End the burst on write-mode off, a step, motor off, `SetDiskImage` and `Reset`, and on each end call `SpliceWrite` and `MarkTrackDirty`. Add a public `CommitPendingWrite()` that ends any open burst. A write-protected disk never buffers or splices (edge case)
- [X] T020 [US2] Commit an open burst before a flush (R6, "Flush mid-burst"). The engine registers itself on the `DiskImage` as `IPendingWriteOwner` while a burst is open; `DiskImage::Flush`, `DiskImage::Eject` and `DiskImageStore::FlushEntry` call `DiskImage::CommitPendingWrite()` first (research R6). Test in `UnitTest/EmuTests/Disk2NibbleEngineFluxTests.cpp`: flush in the middle of a flux write saves the partial write
- [X] T021 [US2] Extend `WozLoader::Serialize` (`CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp`) per `contracts/woz-file.md`:
  - a flux slot's TRKS record holds its byte count, and its bytes are copied verbatim;
  - TMAP is 0xFF where FLUX claims the quarter track;
  - the FLUX chunk is rebuilt from the map with TRKS indices and placed on the first block boundary after TRKS;
  - INFO version is raised to 3 when FLUX is written, and +44, +46 and +48 are set;
  - a source FLUX pass-through chunk is dropped (also in `WozMetadata.h` handling);
  - META and other pass-through chunks follow FLUX.
- [X] T022 [US2] Confirm that flush skips a clean image, so an unwritten flux image is never rewritten (R7 point 4). Find the check in `DiskImageStore::FlushEntry` and add a test if none covers it
- [X] T023 [P] [US2] Add round-trip tests to `UnitTest/EmuTests/WozLoaderTests.cpp`. Each serializes then reloads, and checks:
  - an unwritten synthetic 2.1 image gives the same slots, kinds, map and flux bytes, with byte-identical FLUX entries (SC-003);
  - a written flux track stays in FLUX and its stretch reads back as written;
  - a grown track updates TRKS and INFO +48 (US2 scenario 3);
  - INFO +46 equals the FLUX chunk's block;
  - a bit-only image serializes byte-identically to the output before this change.

### Sector-level tools (FR-012, FR-013)

- [X] T024 [US2] Create `FluxBitView` in `CassoEmuCore/Devices/Disk/FluxBitView.h` and `.cpp` (R11, data-model.md):
  - `m_bits` holds packed bits, MSB first, in the `DiskImage` layout;
  - it also holds `m_bitCount` and `m_bitStartTick`;
  - each transition decodes as "`round (gap / cell) - 1` zero bits, then a 1", with the cell at 1408/45 ticks;
  - "Long gaps decode as zeros, never as random bits";
  - `GetTickForBit (index)` gives the tick a bit starts at.

  Add both files to `CassoEmuCore.vcxproj` and `.filters`
- [X] T025 [US2] Make `NibblizationLayer`'s reads take bits from a track source (`CassoEmuCore/Machines/Apple2/Common/NibblizationLayer.h` and `.cpp`). This covers `ReadNibbleAt`, `DecodeTracks`, both `Denibblize` overloads, `SalvageSectors`, and the bit read near cpp:900. A bit slot uses `DiskImage`'s packed bits as today. A flux slot uses a `FluxBitView` built for the call. Bit-track decoding must give identical results (FR-009)
- [X] T026 [US2] Add flux sector writes to `NibblizationLayer`. In the write path that today calls `RenibblizeTracks`, send flux slots to a new routine:
  - for each changed sector, find its data field in the track's `FluxBitView`;
  - encode the new field with the helper `AppendDataField` uses;
  - call `FluxTrack::SpliceWrite` at `GetTickForBit` of the field's first bit;
  - mark the slot dirty.

  Bit slots keep `RenibblizeTracks` unchanged. A sector whose data field cannot be found fails with the existing "sector not found" error, never a regenerated track
- [X] T027 [P] [US2] Write `UnitTest/EmuTests/FluxSectorAccessTests.cpp` and add it to `UnitTest.vcxproj`. It covers:
  - a nominal flux track and a ±3% flux track of a DOS 3.3 image decode to the same 256-byte sectors as the bit original;
  - long gaps decode as zeros, deterministically;
  - `Denibblize` over a mixed bit/flux image;
  - a sector write to a flux track that reads back as written, with every flux byte outside that data field unchanged and total ticks unchanged;
  - a write to a missing sector failing cleanly;
  - a `disk`-command style write through `DiskImageStore` over a synthetic flux image, flushed and reloaded;
  - bit-track decode and write results unchanged.
- [X] T028 [US2] Check *Bandits* by hand again. Boot it and let it write if it does (otherwise write through the `disk` command), then flush, reload and boot again (US2 scenario 4). Also open it in Casso Explorer and confirm its flux tracks are listed and readable, not blank. Done: Casso's save of Bandits is byte-identical to the Applesauce original and boots. Bandits does not write and its half tracks make sector writes refuse by design; the Explorer check is left for the owner (the same `VolumeImage` path is covered by `FluxSectorAccessTests`)

**Checkpoint**: US1 and US2 complete.

---

## Phase 5: User Story 3 - Open a WOZ image with damaged tracks (P3)

**Goal**: damaged tracks, flux or bit, mount read-only with a report. A short FLUX chunk is refused, and salvage reads flux tracks.

**Independent test**: each damaged-track case in US3 mounts read-only and its report lists the tracks. The short FLUX chunk is refused (SC-006). Salvage of a damaged flux disk recovers the readable flux tracks' sectors.

- [X] T029 [US3] Add the damaged-track list to `DiskImage` (`CassoEmuCore/Devices/Disk/DiskImage.h` and `.cpp`). A nested plain struct `DamagedTrack { int trkIndex; bool isFlux; DamageReason reason; }` goes in `m_damagedTracks`, with `GetDamagedTracks()` and `HasDamagedTracks()`. `IsWriteProtected()` is also true when the list is not empty
- [X] T030 [US3] Make track damage non-fatal in `WozLoader::Load` (R9). When `ParseV2Track`, the flux parse or the v1 record check (now at cpp:651, reason `V1RecordPastTrks`) fails, record a `DamagedTrack`, leave the slot unformatted and continue, instead of `CHR (hrTrack)` / `CBR`. Structural failures (missing INFO, TMAP or TRKS, a truncated chunk, a TRKS record table cut short, a FLUX chunk under 160 bytes) still refuse. An all-zero record stays unformatted, not damaged
- [X] T031 [US3] Offer salvage for damaged tracks. In `DiskImageStore::AssessSalvage` and `IsSalvageOffered` (`CassoEmuCore/Devices/Disk/DiskImageStore.cpp`), treat `HasDamagedTracks()` like `HasSourceCrcMismatch()`. Check that `SetImageWriteProtect` refuses the same way (cpp:1373) and that `FlushEntry` stays correct. Salvage reads flux tracks through T025
- [X] T032 [US3] Write a core formatter for the damaged-mount report in `CassoEmuCore/Devices/Disk/DamagedMountReport.h` and `.cpp`, per `contracts/damaged-mount-report.md`:
  - it says the disk is read-only;
  - it lists tracks as whole, .25, .5 and .75 numbers, giving the first eight plus a count beyond that;
  - a checksum fault and damaged tracks share one report;
  - a flux image gets the salvage sentence about flux timing and copy protection.

  Show the owner the wording for approval before it is final
- [X] T033 [US3] Switch `EmulatorShell::ReportDamagedMount` (`CassoEmuCore/Shell/EmulatorShellDisks.cpp`, around line 553) to the formatter's text. Trigger it for damaged tracks as well as a checksum mismatch, both at the post-mount call (around line 228) and on the `WM_APP_REPORT_DAMAGE` path
- [X] T034 [P] [US3] Write `UnitTest/EmuTests/DamagedDiskMountTests.cpp` and add it to `UnitTest.vcxproj`. Damage the WOZ bytes, not a `DiskImage`, and cover:
  - a flux track with a trailing 255;
  - a FLUX entry whose blocks lie past the end of the file;
  - a v2 TMAP bit track past the end of the file;
  - a v1 record past the end of TRKS.

  Each mounts read-only, its track reads as unformatted, and its tracks are listed with the right `DamageReason`. Also cover:
  - a FLUX chunk under 160 bytes refused as `MalformedWoz`;
  - salvage offered;
  - salvage of a damaged image whose other tracks are flux recovering those tracks' sectors;
  - the formatter's output for the list, the cutoff after eight, the combined checksum case and the flux sentence.

  Check that the existing `DamagedDiskFlushTests` and `DiskFailureModeTests` still pass

**Checkpoint**: all three stories complete.

---

## Phase 6: Polish and cross-cutting

- [X] T035 [P] Add a flux scenario test in `ScenarioTests/GuestVisibleFluxTests.cpp` and add it to `ScenarioTests.vcxproj`. Convert the DOS 3.3 System Master to a 2.1 image with every track as flux and stretches 3% fast and slow, using T007's converter and `BuildSyntheticV21`. Then:
  - boot it, `SAVE` an Applesoft file, flush, reload, boot again and `CATALOG`, and check the file is listed;
  - write a second file through the `disk` command path, then boot and `CATALOG`, and check DOS lists it and `LOAD`s it (the guest-visible check of FR-013).

  Assert that the disk loaded before asserting on the result (degraded operation must be observable)
- [X] T036 [P] Add a microbenchmark to `UnitTest/EmuTests/PerformanceTests.cpp` that ticks `Disk2NibbleEngine` for the same number of cycles on a bit track and on a flux track. It runs in Release only, skipped in Debug like `CycleEmulation_SkippedInDebug`. It prints both times and asserts only the loose sanity bound flux ≤ 1.5 × bit (R10). The 2% budget is T037
- [X] T037 Check SC-005 by hand: maximum-speed emulation with *Bandits* spinning against a bit-only demo disk, Release, pinned to one CCD, within 2%. Run T036 pinned at the same time and record both results. Recorded: the engine alone runs flux at 100.3-100.9% of bit over three unpinned runs; the engine is a fraction of a machine cycle, so the machine-level difference is smaller still. The pinned full-machine run is left for the owner
- [X] T038 Run `scripts/RunTests.ps1 -Build -Scenario` (required: drive and loader change), and report it in the summary even if it cannot run
- [X] T039 SC-004: boot each copy-protected WOZ demo in `Apple2/Demos` that boots on master and confirm each still boots
- [X] T040 Mutation spot check. Stub each of these in turn and confirm the matching tests go red:
  - the flux pulse delivery;
  - the FLUX precedence;
  - `SpliceWrite`'s tick preservation;
  - the `FluxBitView` rounding;
  - the flux sector-write routing;
  - the commit before flush;
  - the damaged-track tolerance.

  Touch each restored file before rebuilding
- [X] T041 Pre-merge gate: full unit suite in x64 Debug and Release, `scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis`, ARM64 build, `scripts/CheckStyle.ps1 -Mode Tree` (after `git add -A`, excluding `.specify/feature.json`), and `rg -n '\w \(\)'` over the touched files
- [ ] T042 Draft the CHANGELOG `[Unreleased]` entries (GH #159 flux support, including the sector-level tools; damaged WOZ tracks mount read-only) and the README update. Show both to the owner for approval only after they have tested the build

---

## Phase 7: Follow-ups from review

- [ ] T043 [US3] Restructure the damaged-disk report on insert (`CassoEmuCore/Devices/Disk/DamagedMountReport.cpp`, tests in `UnitTest/EmuTests/DamagedDiskMountTests.cpp`) as a problem list, once the owner approves this draft:

  > Casso found these problems in this disk image:
  >
  > • The stored checksum does not match the contents.
  > • Unable to read tracks 1-5, 8-9, 13, 20.
  >
  > {path}
  >
  > Casso has loaded the disk so you can read it, and has write-protected it for this session, because rewriting the file would hide the damage. Unreadable tracks read as blank. A salvaged copy is a standard disk image of the readable sectors, without this disk's flux timing or its copy protection.

  - One bullet per problem, using `s_kchBullet` from `Core/UnicodeSymbols.h`. Every damaged track goes in one "Unable to read" bullet with no reason clause: the owner judged "missing from the file" and "cut short" to mean the same thing to a user (2026-10-02). `DamageReason` stays recorded for tests and diagnostics only.
  - Track lists become runs of tracks one whole track apart ("1-5, 8-9, 13"; Bandits' half tracks read "1.5-19.5"), with "track" or "tracks" by how many the list covers. This replaces the eight-then-"N more" cutoff in `FormatTrackList`.
  - "Unreadable tracks read as blank." only with damaged tracks; the salvage sentence only on flux disks. A checksum-only disk has one bullet.
  - The drive tooltip and the write-protect refusal keep their single-sentence wording.
- [ ] T044 Owner checks still open: open a flux disk in Casso Explorer (T028) and run the pinned full-machine speed comparison (T037). The six real flux disks for this and any later check are in `%LOCALAPPDATA%\Casso\FluxTestDisks` (never committed; keep until 038 closes). Baseline 2026-10-02: Bandits, Minotaur, Fly Wars, Cyclod, Lemmings and Jellyfish all boot in Casso, save byte-identical, and match AppleEm's Disk Inspector cell count on every flux track (104 tracks).

---

## Dependencies

- Setup (T001-T002), then Foundational (T003-T007), then the stories.
- US1 needs Foundational. US2 needs US1's loader and engine (T008-T013).
- US3 needs Foundational and T008. Its flux salvage (T031, T034) also needs T025.
- Within US1: T008, then T010, T011, T012 and T013 in that order (same file); T014 runs beside the engine work; T015 comes last.
- Within US2: T016 first; T017 before T019 and T020; T021 needs T017; T023 needs T021; T024 before T025 and T026; T027 needs T026; T028 comes last.
- Polish needs every story done; T035 also needs T021 and T026.

## Parallel opportunities

- T003 and T004 (different files); T006 and T007 once T005 is done.
- T009 beside T010-T013; T014 beside the engine work.
- T018 beside T019; T023 beside T022.
- T024-T027 (sector tools) beside T019-T023 once T017 is done.
- US3 T029, T030, T032 and T033 beside US2 once T008 has landed.
- T035 and T036.

## Implementation strategy

All three stories and the polish phase ship together in one merge to master
after T041. Nothing merges partway. The phase order is only build order: US1
first, so *Bandits* is checked against real playback before writes are built
on top of it.
