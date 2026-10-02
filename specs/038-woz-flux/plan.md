# Implementation Plan: WOZ Flux Track Support

**Branch**: `038-woz-flux` | **Date**: 2026-10-02 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/038-woz-flux/spec.md`

## Summary

Casso passes the WOZ 2.1 FLUX chunk through unread, so *Bandits*' 19 flux
tracks read as unformatted and the boot hangs (GH #159). The plan:

- **Loading:** read FLUX and keep each flux track's raw bytes in memory at file
  size.
- **Playback:** `Disk2NibbleEngine` plays flux by time on an exact integer
  clock (176/45 flux ticks per LSS clock). Each transition goes to the real P6
  sequencer on the LSS clock where it falls.
- **Writing:** a write on a flux track is buffered for one burst, then spliced
  back in as flux at the controller's own cell timing.
- **Saving:** Serialize rebuilds FLUX and the INFO flux fields, and copies
  unwritten flux tracks verbatim.
- **Damaged tracks, flux or bit:** the disk mounts read-only and enters the
  existing 1.17.0 damaged-disk path (report on insert, salvage) instead of
  refusing the mount.

## Technical Context

**Language/Version**: C++ (MSVC v145, VS 2026), C++23 as configured in the
solution

**Primary Dependencies**: None new. All code is in `CassoEmuCore`.

**Storage**: WOZ files through the existing `DiskImageStore` / `IDiskFileIo`
seam

**Testing**: Microsoft Native CppUnitTest (`UnitTest`), plus `ScenarioTests`
(required here), run by `scripts/RunTests.ps1 [-Scenario]`

**Target Platform**: Windows x64 (ARM64 build-only)

**Project Type**: Desktop emulator, core static libraries + thin exe

**Performance Goals**: Flux playback within 2% of bit playback at maximum
speed (SC-005). The bit path must not get slower (FR-009/FR-011).

**Constraints**: Bit-track playback and every non-flux format behave exactly
as before. Flux is never converted to bits at load. Memory holds about the file
size (about 38 KB per flux track).

**Scale/Scope**: Up to 160 quarter tracks. *Bandits*: 19 flux tracks of
about 38 KB, 25,000 transitions and 1.6 M ticks (200.5 ms) per revolution.

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Code Quality | Pass | EHM throughout. `FluxTrack` gets its own `.h/.cpp` pair; no magic numbers (176, 45 and 1408 become named constants with the derivation in a comment) |
| II. Testing Discipline | Pass | Unit tests for every FR, each shown to fail with its implementation stubbed. The scenario suite runs because the drive and loader change. No test reads real files; images are synthesized |
| III. UX Consistency | Pass | The damaged-track report reuses the existing dialog and error format; wording goes to the owner for approval |
| IV. Performance | Pass, with measurement | Cached slot removes a per-clock lookup; microbenchmark plus a maximum-speed check (R10) |
| V. Simplicity | Pass | Damaged tracks reuse the checksum-damage path. The bit path is untouched apart from the cached slot. One new class |
| VI. Thin Executable | Pass | Everything is in `CassoEmuCore`. The report text comes from a core formatter `UnitTest` reaches |

Re-checked after Phase 1 design: no violations, and Complexity Tracking is
empty.

## Project Structure

### Documentation (this feature)

```text
specs/038-woz-flux/
├── spec.md
├── plan.md               # this file
├── research.md           # R1-R11 decisions
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── woz-file.md
│   └── damaged-mount-report.md
└── checklists/requirements.md
```

### Source code (touched)

```text
CassoEmuCore/
├── Devices/Disk/
│   ├── FluxTrack.h / .cpp              # NEW: raw flux, cursor, splice
│   ├── DiskImage.h / .cpp              # slot kind, flux tracks, damaged-track list
│   └── DiskImageStore.cpp              # salvage offered for damaged tracks
├── Machines/Apple2/Common/
│   ├── Disk2NibbleEngine.h / .cpp      # cached slot, flux time base, weak bits, write burst
│   ├── WozLoader.h / .cpp              # FLUX read, damaged-track tolerance, Serialize FLUX/INFO
│   └── WozMetadata.h                   # FLUX no longer pass-through
└── Shell/
    └── EmulatorShellDisks.cpp          # report lists damaged tracks (text from core formatter)

UnitTest/EmuTests/
├── FluxTrackTests.cpp                  # NEW
├── Disk2NibbleEngineFluxTests.cpp      # NEW: timing, angle, weak bits, writes
├── WozLoaderTests.cpp                  # FLUX parse, precedence, save round trip
├── DamagedDiskMountTests.cpp           # NEW: damaged tracks, map refusals, report text
└── PerformanceTests.cpp                # bit vs flux microbenchmark (Release)

ScenarioTests/
└── GuestVisibleFluxTests.cpp           # NEW: DOS 3.3 on a flux disk: boot, SAVE, flush, reboot
```

**Structure Decision**: This uses the existing layout. The only new production
file pair is `FluxTrack`.

## Implementation order

1. **`FluxTrack`, plus the FLUX read in `WozLoader::Load`.** `DiskImage` gets
   slot kinds. This includes the `BuildSyntheticV21` test helper. It is
   testable with no engine changes.
2. **Engine: cached slot** (a perf baseline, no behavior change). Run the
   existing engine tests to confirm FR-009.
3. **Engine: flux playback.** The time base, pulse delivery, head-angle
   conversion and weak bits. Check SC-002 here.
4. ***Bandits* by hand** (SC-001). This is the earliest real-world check of
   steps 1-3.
5. **Writes**: the burst buffer, `SpliceWrite`, and dirty marking.
6. **Serialize**: the FLUX chunk, INFO fields and verbatim flux bytes, plus the
   round-trip tests (SC-003, US2).
7. **Damaged tracks**: loader tolerance, the `DiskImage` list, read-only,
   salvage offer and report formatter (US3, SC-006).
8. **Scenario test and microbenchmark.** Then the pre-merge gate in
   quickstart §5.

## Risks

- **The head-angle mapping is wrong for *Bandits*' bit-to-flux steps.**
  Mitigation: step 4 runs before writes are built, and a unit test steps across
  the boundary mid-sector.
- **The scenario disk at ±3% fails on DOS** for reasons unrelated to the
  feature. Mitigation: narrow the spread. Engine tests still own SC-002.
- **Merging master late with renames** (CLAUDE.md hazard). Mitigation: merge
  master before step 5.

## Complexity Tracking

None.
