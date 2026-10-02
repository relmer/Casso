# Tasks: WOZ Flux Track Support

**Input**: Design documents from `specs/038-woz-flux/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required. The constitution's Principle II applies, and the scenario
suite is mandatory because the drive and the loader change. Every new test is
shown to fail with its implementation stubbed or reverted before it counts.

**Format**: `[ID] [P?] [Story] Description`. `[P]` means the task can run in
parallel (different files, no dependency on an unfinished task).

## Phase 1: Setup

- [ ] T001 Run `scripts/FetchRoms.ps1 -Fixtures`, then build x64 Debug through `Casso.sln` and run `scripts/RunTests.ps1` to record a green baseline (total test count) before any change
- [ ] T002 Copy `00_Bandits.woz` from the investigation scratchpad (`C:\Users\relmer\AppData\Local\Temp\claude\C--Users-relmer-source-repos-relmer-Casso-worktrees-issue-159-woz-flux\55ca770d-0acf-46e8-a8e7-3242a682d185\scratchpad\00_Bandits.woz`) into this session's scratchpad. It stays outside the repo and is never committed (clarification 1)

---

## Phase 2: Foundational (blocks every story)

- [ ] T003 [P] Create `FluxTrack` in `CassoEmuCore/Devices/Disk/FluxTrack.h` and `.cpp`, per data-model.md. Members: `m_bytes` (raw TRKS bytes), `m_totalTicks` (`uint64_t`), and `m_transitionCount`. A factory validates the source: "the byte count is no larger than block count × 512, and the blocks lie inside the file" and "the last byte is not 255". It reports a failure as a damage reason, not a load failure. "A zero byte count is a valid track with no transitions." Add a cursor type {byte index, absolute tick}, plus `FindTransitionAtOrAfter (tick)`, `AdvanceCursor (cursor)` (summing 255 runs and wrapping at `m_totalTicks`), `GetBytes()` and `GetTotalTicks()`. Add both files to `CassoEmuCore.vcxproj` and its `.filters`
- [ ] T004 [P] Write `UnitTest/EmuTests/FluxTrackTests.cpp` and add it to `UnitTest.vcxproj`. It covers total ticks, a 255 run decoding as one gap (255, 255, 10 → 520), wrap at the end of the revolution, seek to a tick inside a gap, zero transitions, and each validation failure (a trailing 255, a count larger than the blocks hold, blocks outside the file)
- [ ] T005 Extend `DiskImage` in `CassoEmuCore/Devices/Disk/DiskImage.h` and `.cpp`. Add a free enum `TrackKind { Bits, Flux }`, `m_slotKind` and `m_fluxTracks` indexed by slot (TRKS index), and `GetTrackKind (slot)` and `GetFluxTrack (slot)` (const and for-write). `EnsureTrackSlots` sizes the new vectors, and `IsTrackDirty` covers flux slots. Bit-slot behavior does not change
- [ ] T006 Add `WozLoader::BuildSyntheticV21` in `CassoEmuCore/Machines/Apple2/Common/WozLoader.h` and `.cpp`, next to `BuildSyntheticV2`. It builds a WOZ 2.1 image from a list of bit tracks and flux tracks, each with its quarter tracks. It lays the file out the way Applesauce does: INFO version 3, FLUX on the first block boundary after TRKS, and INFO bytes +46 and +48 set
- [ ] T007 Add a test-side converter in `UnitTest/EmuTests/FluxTestImages.h` and `.cpp`. It turns a bit stream into flux bytes with a chosen cell length for each stretch, in ticks, carrying the fraction (3.7 µs = 29.6 ticks, 4.1 µs = 32.8 ticks, nominal = 1408/45). `UnitTest` and `ScenarioTests` both use it, so add it to both projects

**Checkpoint**: flux tracks can be built in memory and in synthetic files. The engine and loader don't touch them yet.

---

## Phase 3: User Story 1 - Boot a disk with flux tracks (P1) MVP

**Goal**: quarter tracks mapped in FLUX play by time, and *Bandits* boots.

**Independent test**: a synthetic image that mixes 3.7 µs and 4.1 µs stretches reads each stretch's cell length within one LSS step (SC-002), and *Bandits* reaches its title screen on an Apple //e Enhanced (SC-001, by hand).

### Loader

- [ ] T008 [US1] In `WozLoader::Load` (`CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp`), recognize the FLUX chunk in the chunk walk instead of passing it through. "Only a FLUX chunk shorter than 160 bytes refuses the mount as `MalformedWoz`." INFO's version and flux fields are not consulted (R8). After TMAP, parse every TRKS index FLUX references into a `FluxTrack`. Bit Count holds the byte count. Then apply FLUX map entries over TMAP, so FLUX wins (FR-002). Add named constants for INFO offsets +46 (FLUX block) and +48 (largest flux track)
- [ ] T009 [P] [US1] Add tests to `UnitTest/EmuTests/WozLoaderTests.cpp` covering:
  - FLUX parsed into flux slots;
  - FLUX precedence over TMAP for the same quarter track;
  - a FLUX chunk used at INFO version 2 and with INFO +46/+48 zero;
  - a FLUX chunk under 160 bytes refused as `MalformedWoz`;
  - a bit-only image's slots and map unchanged (FR-009).

### Engine

- [ ] T010 [US1] Cache the resolved slot in `Disk2NibbleEngine` (`CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h` and `.cpp`). Add `m_slot` and `m_isFluxSlot`, refreshed in `SetCurrentTrack`, `SetDiskImage` and `Reset`, so `StepLss` stops calling `ResolveQuarterTrack` on every clock (R10). Run the existing `Disk2NibbleEngine` tests plus `-Filter Disk` to show no behavior change
- [ ] T011 [US1] Add the flux time base to `Disk2NibbleEngine`. Use `m_fluxNow` in 1/45-tick units, advanced by named constants (176 per LSS clock; 1408 per nominal cell, derived in a comment from 14.31818 MHz = 315/22 MHz, CPU = 45/44 MHz, 2 LSS clocks per CPU cycle, 8 MHz flux ticks; R1). Keep `m_fluxCursor`, which holds the due time of the next transition. When `m_isFluxSlot` is set, `StepLss` hands the sequencer a pulse on whichever clock the due time falls, then advances the cursor. When it is clear, the bit path is exactly as before, sampling on `kLssReadClock` (R2)
- [ ] T012 [US1] Add weak bits on flux tracks to `Disk2NibbleEngine`. Track `m_fluxLastPulse`. Once the gap since the last real transition exceeds four nominal cells (4 × 1408 units), inject `NextWeakBit()` pulses at nominal cell cadence until the next real transition is due (R5, FR-005). This includes the zero-transition track
- [ ] T013 [US1] Add head-angle conversion to `Disk2NibbleEngine::SetCurrentTrack` (R4, FR-004). Bit to flux and flux to flux: fraction = position / length, set `m_fluxNow` = fraction × total ticks × 45, then re-seek the cursor with `FindTransitionAtOrAfter`. Flux to bit: `m_bitPos` = fraction × bit count. Bit to bit keeps `m_bitPos %= newBits` unchanged
- [ ] T014 [P] [US1] Write `UnitTest/EmuTests/Disk2NibbleEngineFluxTests.cpp` and add it to `UnitTest.vcxproj`. It covers:
  - a nominal flux track of a known nibble pattern reads the same nibbles as the equivalent bit track;
  - a track with 3.7 µs and 4.1 µs stretches gives pulse spacings within one LSS step of 29.6 and 32.8 ticks (SC-002);
  - no drift after 1,000 revolutions (the cursor tick equals the integer expectation);
  - a long gap produces weak bits that vary between revolutions;
  - a zero-transition track reads noise;
  - stepping bit to flux and back mid-revolution keeps the angular fraction within one cell;
  - a bit-only disk's nibble stream is unchanged from before (FR-009).
- [ ] T015 [US1] Check *Bandits* by hand (SC-001). Build x64 Release and launch Casso minimized with `--title <worktree name>` and `--disk1 <scratchpad>\00_Bandits.woz` on an Apple //e Enhanced. Boot to the title screen, capture a screenshot, then restore `disk1Path` in UserPrefs. If it hangs, find the failing track with the engine's trace before going further

**Checkpoint**: US1 complete. Flux disks boot and the bit path is unchanged.

---

## Phase 4: User Story 2 - Keep flux tracks intact on save (P2)

**Goal**: writes keep flux tracks as flux, and saving round-trips.

**Independent test**: write one flux track through the drive, flush, and reload. Unwritten flux bytes are identical, and the written stretch reads back as written (SC-003).

- [ ] T016 [US2] Merge `origin/master` into `038-woz-flux` before building writes (plan Risks; CLAUDE.md rename hazard). Rebuild, rerun the unit suite, and fix any stale call sites
- [ ] T017 [US2] Add `FluxTrack::SpliceWrite (startTick, bits)` in `CassoEmuCore/Devices/Disk/FluxTrack.cpp` (R6):
  - copy the original flux up to the start tick;
  - write each 1 bit as a transition, with cells of 1408/45 ticks (31.29, clarification 4) rounded to whole ticks with the fraction carried;
  - resume the original stream at the burst's end tick;
  - split each straddling gap so `m_totalTicks` is unchanged;
  - handle a burst that wraps past the end of the revolution;
  - recompute the transition count.
- [ ] T018 [P] [US2] Add splice tests to `UnitTest/EmuTests/FluxTrackTests.cpp`: total ticks preserved, bytes outside the burst identical, a burst across the wrap, a burst of all zeros (one long gap), and the written bits reading back as written through a cursor
- [ ] T019 [US2] Add the write burst to `Disk2NibbleEngine`. While writing on a flux slot, append each written bit to `m_writeBurst` and record the start tick. End the burst on write-mode off, a step, motor off, `SetDiskImage`, `Reset` and flush, and on each end call `SpliceWrite` and `MarkTrackDirty`. A write-protected disk never buffers or splices (edge case)
- [ ] T020 [US2] Extend `WozLoader::Serialize` (`CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp`) per `contracts/woz-file.md`:
  - a flux slot's TRKS record holds its byte count, and its bytes are copied verbatim;
  - TMAP is 0xFF where FLUX claims the quarter track;
  - the FLUX chunk is rebuilt from the map with TRKS indices and placed on the first block boundary after TRKS;
  - INFO version is raised to 3 when FLUX is written, and +44, +46 and +48 are set;
  - a source FLUX pass-through chunk is dropped (also in `WozMetadata.h` handling);
  - META and other pass-through chunks follow FLUX.
- [ ] T021 [US2] Confirm that flush skips a clean image, so an unwritten flux image is never rewritten (R7 point 4). Find the check in `DiskImageStore::FlushEntry` (`CassoEmuCore/Devices/Disk/DiskImageStore.cpp`) and add a test if none covers it
- [ ] T022 [P] [US2] Add round-trip tests to `UnitTest/EmuTests/WozLoaderTests.cpp`. Each serializes then reloads, and checks:
  - an unwritten synthetic 2.1 image gives the same slots, kinds, map and flux bytes, with byte-identical FLUX entries (SC-003);
  - a written flux track stays in FLUX and its stretch reads back as written;
  - a grown track updates TRKS and INFO +48 (US2 scenario 3);
  - INFO +46 equals the FLUX chunk's block;
  - a bit-only image serializes byte-identically to the output before this change.
- [ ] T023 [US2] Check *Bandits* by hand again: boot it, let it write if it does (otherwise write through the disk command path), flush, reload, and boot again (US2 scenario 4)

**Checkpoint**: US1 and US2 complete.

---

## Phase 5: User Story 3 - Open a WOZ image with damaged tracks (P3)

**Goal**: damaged tracks, flux or bit, mount read-only with a report. A short FLUX chunk is refused.

**Independent test**: each damaged-track case in US3 mounts read-only and its report lists the tracks. The short FLUX chunk is refused (SC-006).

- [ ] T024 [US3] Add the damaged-track list to `DiskImage` (`CassoEmuCore/Devices/Disk/DiskImage.h` and `.cpp`). A nested plain struct `DamagedTrack { int trkIndex; bool isFlux; DamageReason reason; }` goes in `m_damagedTracks`, with `GetDamagedTracks()` and `HasDamagedTracks()`. `IsWriteProtected()` is also true when the list is not empty
- [ ] T025 [US3] Make track damage non-fatal in `WozLoader::Load` (R9). When `ParseV2Track`, the flux parse or the v1 record check (now at cpp:651) fails, record a `DamagedTrack`, leave the slot unformatted and continue, instead of `CHR (hrTrack)` / `CBR`. Structural failures (missing INFO, TMAP or TRKS, a truncated chunk, a TRKS record table cut short, a FLUX chunk under 160 bytes) still refuse. An all-zero record stays unformatted, not damaged
- [ ] T026 [US3] Offer salvage for damaged tracks. In `DiskImageStore::AssessSalvage` and `IsSalvageOffered` (`CassoEmuCore/Devices/Disk/DiskImageStore.cpp`), treat `HasDamagedTracks()` like `HasSourceCrcMismatch()`. Check that `SetImageWriteProtect` refuses the same way (cpp:1373) and that `FlushEntry` stays correct
- [ ] T027 [US3] Write a core formatter for the damaged-mount report in `CassoEmuCore/Devices/Disk/DamagedMountReport.h` and `.cpp`, per `contracts/damaged-mount-report.md`:
  - it says the disk is read-only;
  - it lists tracks as whole, .25, .5 and .75 numbers, giving the first eight plus a count beyond that;
  - a checksum fault and damaged tracks share one report;
  - a flux image gets the salvage sentence about flux timing and copy protection.
  
  Show the owner the wording for approval before it is final
- [ ] T028 [US3] Switch `EmulatorShell::ReportDamagedMount` (`CassoEmuCore/Shell/EmulatorShellDisks.cpp`, around line 553) to the formatter's text. Trigger it for damaged tracks as well as a checksum mismatch, both at the post-mount call (around line 228) and on the `WM_APP_REPORT_DAMAGE` path
- [ ] T029 [P] [US3] Write `UnitTest/EmuTests/DamagedDiskMountTests.cpp` and add it to `UnitTest.vcxproj`. Damage the WOZ bytes, not a `DiskImage`, and cover:
  - a flux track with a trailing 255;
  - a FLUX entry whose blocks lie past the end of the file;
  - a v2 TMAP bit track past the end of the file;
  - a v1 record past the end of TRKS.

  Each mounts read-only, its track reads as unformatted, and its tracks are listed. Also cover: a FLUX chunk under 160 bytes refused as `MalformedWoz`, salvage offered, and the formatter's output for the list, the cutoff after eight, the combined checksum case and the flux sentence. Check that the existing `DamagedDiskFlushTests` and `DiskFailureModeTests` still pass

**Checkpoint**: all three stories complete.

---

## Phase 6: Polish and cross-cutting

- [ ] T030 [P] Add a flux scenario test in `ScenarioTests/GuestVisibleFluxTests.cpp` and add it to `ScenarioTests.vcxproj`. Convert the DOS 3.3 System Master to a 2.1 image with every track as flux and stretches 3% fast and slow, using T007's converter and `BuildSyntheticV21`. Boot it, `SAVE` an Applesoft file, flush, reload, boot again and `CATALOG`, then check the file is listed. Assert that the disk loaded before asserting on the result (degraded operation must be observable)
- [ ] T031 [P] Add a microbenchmark to `UnitTest/EmuTests/PerformanceTests.cpp` that ticks `Disk2NibbleEngine` for the same number of cycles on a bit track and on a flux track. It runs in Release only, skipped in Debug like `CycleEmulation_SkippedInDebug`, and asserts flux ≤ 1.02 × bit. Run it pinned to one CCD
- [ ] T032 Check SC-005 by hand: maximum-speed emulation with *Bandits* spinning against a bit-only demo disk, Release, pinned to one CCD, within 2%
- [ ] T033 Run `scripts/RunTests.ps1 -Build -Scenario` (required: drive and loader change), and report it in the summary even if it cannot run
- [ ] T034 SC-004: boot each copy-protected WOZ demo in `Apple2/Demos` that boots on master and confirm each still boots
- [ ] T035 Mutation spot check: stub the flux pulse delivery, the FLUX precedence, `SpliceWrite`'s tick preservation and the damaged-track tolerance one at a time, and confirm the matching tests go red. Touch each restored file before rebuilding
- [ ] T036 Pre-merge gate: full unit suite in x64 Debug and Release, `scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis`, ARM64 build, `scripts/CheckStyle.ps1 -Mode Tree` (after `git add -A`, excluding `.specify/feature.json`), and `rg -n '\w \(\)'` over the touched files
- [ ] T037 Draft the CHANGELOG `[Unreleased]` entries (GH #159 flux support; damaged WOZ tracks mount read-only) and the README update. Show both to the owner for approval only after they have tested the build

---

## Dependencies

- Setup (T001-T002), then Foundational (T003-T007), then the stories.
- US1 needs Foundational. US2 needs US1's loader and engine (T008-T013). US3 needs Foundational and T008 only, so it can run beside US2.
- Within US1: T008, then T010, T011, T012 and T013 in that order (same file); T014 runs beside the engine work; T015 comes last.
- Within US2: T016 first, then T017 before T019; T020 needs T017; T022 needs T020.
- Polish needs every story done; T030 also needs T020 for the flush.

## Parallel opportunities

- T003 and T004 (different files); T006 and T007 once T005 is done.
- T009 beside T010-T013; T014 beside the engine work.
- T018 beside T019; T022 beside T021.
- US3 (T024-T029) beside US2 once T008 has landed.
- T030 and T031.

## Implementation strategy

- **MVP**: Phases 1-3 (US1). *Bandits* boots, and that fixes GH #159 as reported.
- **Then** US2, so a flux disk survives a save. Shipping US1 alone would leave Serialize writing a stale FLUX pass-through after any write, so US2 ships with it.
- **Then** US3 and Polish. One merge to master after T036.
