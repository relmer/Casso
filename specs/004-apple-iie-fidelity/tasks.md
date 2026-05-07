---
description: "Apple //e Fidelity — actionable, dependency-ordered tasks"
---

# Tasks: Apple //e Fidelity

**Input**: Design documents from `/specs/004-apple-iie-fidelity/`
**Prerequisites**: plan.md, spec.md (clarified Session 2026-05-05), research.md, data-model.md, contracts/, quickstart.md
**Authoritative requirements source**: [`docs/iie-audit.md`](../../docs/iie-audit.md)
**Constitution**: `.specify/memory/constitution.md` Phase 12.4.0

**Tests**: Required throughout — every production-code task has a paired test task; every test task honors constitution §II Test Isolation. The harness uses the `IFixtureProvider` plumbing introduced in Phase 0; no test reads or writes host state outside `UnitTest/Fixtures/`.

**Organization**: Foundational phases (Phases 0-6) are layered prerequisites; user-story / subsystem phases (Phases 7-15) layer on top in plan-recommended order; Phase 16 polishes. Each phase ends with an explicit `[GATE]` task that re-asserts pre-existing-tests-still-green and ][/][+ boot continuity.

## Format: `[ID] [P?] [Story?] [GATE?] Description`

- **[P]** — parallel-safe within its phase (different files, no shared-edit conflict)
- **[Story]** — required for tasks inside a user-story phase (Phase 7..Phase 15)
- **[GATE]** — phase-completion gate task; not parallel
- Every task cites the FR(s) and/or audit section it satisfies
- Every production-code task is paired with (or precedes) a test task whose acceptance criterion names the tests that must pass

## Constitution Phase 12.4.0 — applies to every code task

Each task that touches a public function MUST observe:
1. Function comments live in `.cpp` (80-`/` delimiters), not in the header
2. Function/operator spacing: `func()`, `func (a, b)`, `if (...)`, `for (...)`
3. All locals at top of scope, column-aligned (type / `*`-`&` column / name / `=` / value)
4. No non-trivial calls inside macro arguments — store result first, then pass
5. ≤ 50 lines (100 absolute); ≤ 2 indent levels beyond EHM (3 max)
6. Exactly 5 blank lines between top-level constructs; exactly 3 between top-of-function declarations and first statement
7. EHM pattern (`HRESULT hr = S_OK; … Error: … return hr;`) on every fallible function
8. No magic numbers (except 0/1/-1/nullptr/sizeof)

---

## Phase 0: Setup (Shared Infrastructure)

**Purpose**: Stand up the test-isolation plumbing, deterministic PRNG, mocks, and contract headers (declarations only) that every subsequent phase depends on. No production behavior changes yet.

- [X] T001 Create `UnitTest/Fixtures/` directory and add `UnitTest/Fixtures/README.md` documenting the fixture inventory and per-fixture provenance/license matrix from `plan.md` §"Test fixtures (provenance + license posture)". (FR-043, FR-044)
- [X] T002 [P] Stage in-repo binary fixtures: copy/symlink `apple2e.rom` and `apple2e-video.rom` from `ROMs/` into `UnitTest/Fixtures/`, add a placeholder zero-byte `dos33.dsk`, `prodos.po`, `sample.woz`, `copyprotected.woz` so the build sees the paths (real fixtures land in Phase 10/Phase 11). Add `UnitTest/Fixtures/golden/` directory. (FR-043, FR-044)
- [X] T003 [P] Create header `UnitTest/EmuTests/IFixtureProvider.h` per `contracts/IFixtureProvider.md` — interface only. (FR-043)
- [X] T004 [P] Create header `CassoCore/ICpu.h` per `contracts/ICpu.md` — CPU-family-agnostic interface; declarations only. (FR-030)
- [X] T005 [P] Create header `CassoCore/I6502DebugInfo.h` per `contracts/I6502DebugInfo.md` — 6502-family register inspection; declarations only. (FR-030)
- [X] T006 [P] Create header `CassoEmuCore/Core/IInterruptController.h` per `contracts/IInterruptController.md` — declarations only. (FR-032)
- [X] T007 [P] Create header `CassoEmuCore/Devices/IMmu.h` per `contracts/IMmu.md` — declarations only. (FR-001..FR-007, FR-026..FR-029)
- [X] T008 [P] Create header `CassoEmuCore/Devices/Disk/IDiskImage.h` per `contracts/IDiskImage.md` — declarations only. (FR-021..FR-025)
- [X] T009 [P] Create header `CassoEmuCore/Devices/Disk/IDiskController.h` per `contracts/IDiskController.md` — declarations only. (FR-021)
- [X] T010 [P] Create header `CassoEmuCore/Video/IVideoTiming.h` per `contracts/IVideoTiming.md` — declarations only, including `IsInVblank()`. (FR-033)
- [X] T011 [P] Create header `CassoEmuCore/Devices/IVideoMode.h` per `contracts/IVideoMode.md` if not already present — composed-renderer contract; declarations only. (FR-020)
- [X] T012 [P] Create header `CassoEmuCore/Core/IHostShell.h` per `contracts/IHostShell.md` — declarations only. (FR-036)
- [X] T013 [P] Create header `CassoEmuCore/Devices/ISoftSwitchBank.h` per `contracts/ISoftSwitchBank.md` — declarations only. (FR-004)
- [X] T014 Implement `CassoEmuCore/Core/Prng.h` and `Prng.cpp` (SplitMix64). Constructor takes `uint64_t seed`. Production seed source = `static_cast<uint64_t>(time(nullptr)) ^ static_cast<uint64_t>(GetCurrentProcessId())` (computed by callers — Prng itself is pure). Tests pin seed = `0xCA550001`. (FR-035)
- [X] T015 Add `UnitTest/EmuTests/PrngTests.cpp`. Acceptance: tests `PrngTests::SplitMix64MatchesReference`, `PrngTests::SeedDeterminism`, `PrngTests::ZeroSeedProducesNonZeroSequence` pass. Honors §II — no host state. (FR-035)
- [X] T015a Speaker coverage audit + gap closure. Enumerate the existing `AppleSpeaker` test surface (UnitTest/EmuTests/SpeakerTests.cpp et al). For any gap in: `$C030` toggle on read, `$C030` toggle on write, `$C030-$C03F` 16-byte mirror correctness, headless `MockAudioSink::ToggleCount` correctly reflecting CPU writes, soft-vs-power-cycle behavior (toggle counter zeroed by power cycle but speaker state preserved across soft reset), add the missing tests under `UnitTest/EmuTests/SpeakerTests.cpp`. Output a one-page audit log at `specs/004-apple-iie-fidelity/audit-speaker-coverage.md` listing pre-existing tests, identified gaps, and added tests. Acceptance: all enumerated speaker behaviors covered by ≥ 1 test that passes via `HeadlessHost`. (FR-015, audit §5)
- [X] T016 [P] Implement `UnitTest/EmuTests/MockAudioSink.h/.cpp` — `IAudioSink` impl that drops samples and counts toggles. Constructor zero-initializes counters via in-class init. (FR-043)
- [X] T017 [P] Implement `UnitTest/EmuTests/MockIrqAsserter.h/.cpp` — exposes `Assert()` / `Clear()` against an injected `IInterruptController`. (FR-032, FR-043)
- [X] T018 [P] Implement `UnitTest/EmuTests/MockHostShell.h/.cpp` — `IHostShell` impl with no window, no message pump, no input capture; injectable clipboard/window-title sinks return defaults. (FR-036, FR-043)
- [X] T019 [P] Implement `UnitTest/EmuTests/HeadlessHost.h/.cpp` — concrete test host that wires `MockHostShell`, `MockAudioSink`, an `IFixtureProvider` rooted at `UnitTest/Fixtures/`, and a `Prng` pinned to `0xCA550001`. Exposes `BuildAppleII()`, `BuildAppleIIPlus()`, `BuildAppleIIe()` factory methods returning a fully-wired `EmulatorCore`. (FR-036, FR-038, FR-043)
- [X] T020 Add `UnitTest/EmuTests/HeadlessHostTests.cpp`. Acceptance: tests `HeadlessHostTests::ConstructsWithoutWindow`, `HeadlessHostTests::FixtureProviderRejectsPathsOutsideFixtures`, `HeadlessHostTests::DeterministicAcrossTwoBuilds`, `HeadlessHostTests::AudioSinkCountsToggles` pass. (FR-036, FR-038, FR-043)
- [X] T021 [GATE] Build all five projects (CassoCore, CassoEmuCore, Casso, CassoCli, UnitTest) on ARM64+x64, Debug+Release. Run the full pre-existing UnitTest suite. Boot ][ and ][+ machine configs through the headless harness to confirm zero behavioral drift. Acceptance: all previously-passing tests in CassoCore + CassoEmuCore + UnitTest still pass on all four target configurations; ][/][+ boot path unchanged.

---

## Phase 1: CPU Strategy + IRQ Infrastructure

**Purpose**: Refactor the existing `Cpu` into a `Cpu6502` implementation behind `ICpu`, wire `EmuCpu` to hold a `unique_ptr<ICpu>`, and introduce `InterruptController`. Zero behavior change for ][/][+. (FR-030, FR-031, FR-032; audit §9)

- [X] T022 Move existing `CassoCore/Cpu.h` + `Cpu.cpp` content into new `CassoCore/Cpu6502.h` + `Cpu6502.cpp`, implementing `ICpu` and `I6502DebugInfo`. Keep `Cpu.h` as a thin alias header that `#include`s `Cpu6502.h` to preserve existing call sites (one minimal sweep — no behavior change). Locals at top, EHM on any new fallible helpers, function bodies ≤ 50 lines. (FR-030, audit §9)
- [X] T023 Update existing `CpuOperationTests.cpp`, `CpuInitializationTests.cpp`, `AddressingModeTests.cpp`, `Dormann` runner, and `Harte` runner call sites to construct `Cpu6502` directly (via `TestCpu` subclass which now derives from `Cpu6502`). No assertion changes. Acceptance: all previously-passing `CpuOperationTests`, `CpuInitializationTests`, `AddressingModeTests` still pass; Dormann + Harte runs still green. (FR-030)
- [X] T024 Implement IRQ entry-point on `Cpu6502`: implement `ICpu::SetInterruptLine(CpuInterruptKind, bool)` (per `contracts/ICpu.md`), with internal `m_irqLine` flag updated for `kMaskable` and `m_nmiLine` (edge-detected, false→true) for `kNonMaskable`. At the instruction-loop boundary, if `m_irqLine && (P & I_FLAG) == 0`, perform the `$FFFE/$FFFF` vectored dispatch (push PC+P with B=0, set I, jump to `(FFFE)`); for an NMI edge, dispatch via `(FFFA)` regardless of I. Function ≤ 50 lines; locals at top. **Acceptance: covered by T025's `CpuIrqTests`.** (FR-031, audit §9)
- [X] T025 Add `UnitTest/CpuTests/CpuIrqTests.cpp`. Acceptance: tests `CpuIrqTests::AssertedIrqWithIClearDispatchesViaFFFE`, `CpuIrqTests::AssertedIrqWithISetIsIgnored`, `CpuIrqTests::IrqPushesCorrectPCAndStatus`, `CpuIrqTests::IrqSetsIFlagOnEntry`, `CpuIrqTests::ClearIrqBeforeDispatchIsNoop`, `CpuIrqTests::NmiEdgeDispatchesViaFFFAEvenWithISet` pass. All tests drive the CPU exclusively via `ICpu::SetInterruptLine(CpuInterruptKind::kMaskable, ...)` and `(kNonMaskable, ...)` — no direct `m_irqLine` manipulation. Uses `TestCpu::InitForTest()` per §II. (FR-031)
- [X] T026 Implement `CassoEmuCore/Core/InterruptController.h/.cpp` per `contracts/IInterruptController.md`. Aggregates per-source assertions into a single bitmask; on any change in the aggregate, calls `ICpu::SetInterruptLine(CpuInterruptKind::kMaskable, aggregateNonZero)` on the wired CPU. NMI sources (if any are added later) follow the same pattern with `kNonMaskable`. Source IDs allocated via `RegisterSource(name)` returning a token. Locals at top; EHM on registration. **Acceptance: covered by T027's `InterruptControllerTests`.** (FR-032, audit §9)
- [X] T027 Add `UnitTest/EmuTests/InterruptControllerTests.cpp`. Acceptance: tests `InterruptControllerTests::SingleAssertReachesCpu`, `InterruptControllerTests::MultipleAssertersOredTogether`, `InterruptControllerTests::ClearOnlyDeassertsWhenAllSourcesClear`, `InterruptControllerTests::UnregisteredSourceRejected`, `InterruptControllerTests::WorksWithMockIrqAsserter` pass. (FR-032)
- [X] T028 Modify `CassoEmuCore/Core/EmuCpu.h/.cpp` to hold `std::unique_ptr<ICpu>` instead of an embedded `Cpu`. Construction takes the `ICpu` from the machine config wiring. Public surface unchanged for existing callers. **`EmuCpu::SoftReset()` and `EmuCpu::PowerCycle()` (added in Phase 4) both invoke `m_cpu->Reset()`; `ICpu` is intentionally NOT widened with separate soft/power-cycle methods — that would re-couple the family-agnostic interface to a 6502-style reset model.** **Acceptance: covered by T029's `EmuCpuStrategyTests` plus existing `CpuOperationTests`/`Dormann`/`Harte` continuing to pass.** (FR-030, FR-041)
- [X] T029 Wire `InterruptController` into the machine builders for ][, ][+, and //e in `ComponentRegistry` / `MachineConfig`. ][/][+ register zero asserters; //e registers zero asserters today (slot 1/3/4/5/6 reserved tokens for future cards). Acceptance: existing machine-config JSON still parses; `Apple2Tests` boot scenarios unchanged. (FR-032, FR-041)
- [X] T030 [GATE] Build all four configs. Re-run `CpuOperationTests`, `CpuInitializationTests`, `AddressingModeTests`, `CpuIrqTests`, `InterruptControllerTests`, full Dormann (`scripts/RunDormannTest.ps1`), full Harte (`scripts/RunHarteTests.ps1 -SkipGenerate`). Boot ][ and ][+ via headless harness. Acceptance: all previously-passing tests in CassoCore + CassoEmuCore + UnitTest still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot.

---

## Phase 2: //e MMU (consolidate AuxRamCard into AppleIIeMmu)

**Purpose**: Build `AppleIIeMmu`, fold `AuxRamCard`'s responsibilities into it (eliminating audit C2/C4/C6 by deletion), implement INTCXROM/SLOTC3ROM/INTC8ROM, and wire status reads $C013-$C018 + RD80STORE. (FR-001, FR-004, FR-005, FR-006, FR-007, FR-026, FR-027, FR-028, FR-029; audit §1.1, §1.2, §2, §8)

- [X] T031 Implement `CassoEmuCore/Devices/AppleIIeMmu.h/.cpp` per `contracts/IMmu.md` and `data-model.md` §AppleIIeMmu. Owns RAMRD/RAMWRT/ALTZP/80STORE/INTCXROM/SLOTC3ROM/INTC8ROM state. Exposes `OnSoftSwitchChanged` dispatching to per-region resolvers (`ResolveZeroPage`, `ResolveMain02_BF`, `ResolveText04_07`, `ResolveHires20_3F`, `ResolveCxxx`, `ResolveLcD000_FFFF`) — each helper ≤ 50 lines, ≤ 2 indent levels beyond EHM. Function comments in `.cpp` only. (FR-001, FR-004, FR-005, FR-006, FR-007, audit §1.1 / §2)
- [X] T032 Wire `AppleIIeMmu` into the existing `MemoryBus` page-table fast path: every soft-switch change calls into the MMU, which rebinds the affected page table entries (no per-access branch in the bus hot path). Locals at top; ≤ 50 lines per helper. (FR-005, FR-007, FR-040, audit §2)
- [X] T033 Wire $C000-$C00F write switches that the MMU owns (RAMRDON/OFF, RAMWRTON/OFF, ALTZPON/OFF, 80STORE on/off, INTCXROM on/off, SLOTC3ROM on/off, 80COL on/off, ALTCHARSET on/off) through `AppleIIeSoftSwitchBank` so that writes call into the MMU/SoftSwitchBank correctly. Fixes audit C1-C6. (FR-001, audit §1.1 C2/C4/C6)
- [X] T034 Wire $C013-$C018 + $C01E (RD80STORE, RDRAMRD, RDRAMWRT, RDINTCXROM, RDALTZP, RDSLOTC3ROM, RDALTCHAR) status reads in `AppleIIeSoftSwitchBank` to source bit 7 from the MMU's current flag and bits 0-6 from the floating-bus source (keyboard latch, per research.md §Floating Bus). $C019 stub remains until Phase 5. (FR-001, FR-003, audit §1.2 M-items)
- [X] T035 Implement INTCXROM/SLOTC3ROM/INTC8ROM ROM-mapper rules in `AppleIIeMmu::ResolveCxxx` per audit §8: $C100-$CFFF chooses internal vs slot ROM by INTCXROM; $C300-$C3FF chooses 80-col firmware vs slot 3 by SLOTC3ROM; $C800-$CFFF tracks INTC8ROM including the access-pattern that disables it (read of $CFFF clears INTC8ROM). Helper ≤ 50 lines. (FR-026, FR-027, FR-028, FR-029, audit §8)
- [X] T036 Add `UnitTest/EmuTests/MmuTests.cpp` with: `MmuTests::RamRdRoutesReadsToAux`, `RamWrtRoutesWritesToAux`, `AltZpRoutesZeroPageAndStack`, `AltZpSelectsAuxLcBank`, `Store80TextWritesGoToAuxWhenPage2Set`, `Store80HiresWritesGoToAuxWhenPage2Set`, `IntCxRomMapsInternalRomOver$C600`, `SlotC3RomMaps80ColFirmware`, `IntC8RomDisableOnRead$CFFF`, `Slot6IsReachableWhenIntCxRomClear`, `MmuRebindsPageTableOnEverySwitchChange`, `Audit_C2_RAMRDOFF_works`, `Audit_C4_RAMWRTOFF_works`, `Audit_C6_INTCXROMOFF_works`. Use `HeadlessHost.BuildAppleIIe()` per §II. Acceptance: all listed tests pass. (FR-001, FR-005..FR-007, FR-026..FR-028, audit §1.1 / §2 / §8)
- [X] T037 Add `UnitTest/EmuTests/SoftSwitchTests.cpp` updates: `SoftSwitchTests::Status_C013_BSRBANK2_BitFromMmu`, `Status_C014_RDRAMRD_BitFromMmu`, `Status_C015_RDRAMWRT_BitFromMmu`, `Status_C016_RDALTZP_BitFromMmu`, `Status_C017_RDSLOTC3ROM_BitFromMmu`, `Status_C018_RD80STORE_BitFromMmu`, `Status_C01E_RDALTCHAR_BitFromMmu`, `Status_C011_C01F_LowSevenBitsAreFloatingBus`. (FR-001, FR-003, audit §1.2)
- [X] T038 Delete `CassoEmuCore/Devices/AuxRamCard.h` and `AuxRamCard.cpp`. Remove from the `CassoEmuCore.vcxproj` `<ClInclude>` / `<ClCompile>` lists and from any `ComponentRegistry` registration site. Remove or rewrite `AuxRamCardTests.cpp` (rewrite onto `MmuTests.cpp` coverage). Acceptance: solution builds clean on all four configs; no references to `AuxRamCard` remain (`grep -r AuxRamCard` returns only this task's commit message context). Audit C6 fix-by-elimination complete. (FR-040, audit §1.1 C6, §2)
- [X] T039 Update `Machines/apple2e.json` to declare the new `AppleIIeMmu` device and remove `AuxRamCard`. Update `MachineConfig` JSON parser if a new device kind is required. Acceptance: existing `MachineConfigTests` pass; new test `MachineConfigTests::AppleIIeProfileLoadsMmu` passes. (FR-001, FR-040)
- [X] T040 [GATE] Build all four configs. Run `MmuTests`, updated `SoftSwitchTests`, `MachineConfigTests`, plus full pre-existing UnitTest suite. Boot ][ and ][+ via headless harness; ][/][+ behavior MUST be unchanged. Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release.

---

## Phase 3: Language Card Rewrite

**Purpose**: Replace the current LC pre-write state machine entirely. Implement any-two-odd-reads, write-resets-prewrite, MF_BANK2|MF_WRITERAM power-on default, aux-LC routing via ALTZP, and soft-reset content preservation. (FR-008, FR-009, FR-010, FR-011, FR-012; audit §3)

- [X] T041 Rewrite `CassoEmuCore/Devices/LanguageCard.h/.cpp` pre-write state machine: drop `m_lastSwitch == switchAddr` equality requirement; instead track an `m_oddReadCount` (0..2) that increments on any odd $C08x **read** and clears on any LC-region write or any even $C08x access. `WRITERAM` is enabled when `m_oddReadCount == 2` AND the access is an odd read. Locals at top, ≤ 50 lines, function comments in `.cpp`. (FR-008, FR-009, audit §3 M-items)
- [X] T042 In rewritten `LanguageCard`, set power-on default to `MF_BANK2 | MF_WRITERAM` in the `PowerCycle()` path (matches AppleWin `kMemModeInitialState`). Add `SoftReset()` that preserves all RAM banks and resets only the mode flags per audit §3 / §10. Acceptance: paired with T044 tests below. (FR-011, FR-012, audit §3 M-items, §10)
- [X] T043 Add aux-LC routing: when `ALTZP=1` AND LC RAM-read (or RAM-write) is active, the $D000-$FFFF window targets the auxiliary LC RAM bank held inside `AppleIIeMmu`'s aux memory (not a separate LC-owned aux buffer). The MMU's `ResolveLcD000_FFFF` helper does the routing; LC owns only the mode flags + main-side bank buffers. (FR-010, audit §3 / §2)
- [X] T044 Rewrite `UnitTest/EmuTests/LanguageCardTests.cpp` with: `LanguageCardTests::PreWriteRequiresAnyTwoConsecutiveOddReads_NotSameAddress`, `WriteToOddSwitchResetsPreWrite`, `WriteToLcRegionResetsPreWrite`, `EvenAccessClearsPreWrite`, `PowerOnDefaultsToBank2WriteRamPrearmed`, `SoftResetPreservesAllLcRamBanks`, `PowerCyclePopulatesLcRamFromPrng`, `AltZpRoutesLcWindowToAuxBank`, `AltZpClearRoutesLcWindowToMainBank`, `Audit_C81_thenC83_EnablesWrite` (the cross-address case the old impl failed). Acceptance: all listed tests pass via `HeadlessHost.BuildAppleIIe()`. (FR-008..FR-012, audit §3)
- [X] T045 Verify ][/][+ legacy LC scenarios: existing `LanguageCardTests` for ][+ continue to pass with the rewritten state machine (the new "any two odd reads" semantics are correct on ][+ as well — audit notes the old behavior was wrong on ][+ too, but tests asserting that wrong behavior must be updated to assert the correct behavior). Update assertions where they encoded the bug. (FR-039, FR-040)
- [X] T046 [GATE] Build all four configs. Run rewritten `LanguageCardTests`, `MmuTests`, `SoftSwitchTests`, full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot.

---

## Phase 4: Reset + RAM Seed Semantics

**Purpose**: Split `Reset` into `SoftReset` and `PowerCycle` paths in EmulatorShell + every device that defines a `Reset`. Wire `Prng`-seeded RAM init on power-cycle. (FR-034, FR-035; audit §10)

- [X] T047 Modify `CassoEmuCore/Core/EmulatorShell.h/.cpp` (and the equivalent in `Casso/`): split `Reset()` into `SoftReset()` and `PowerCycle()` public methods. `IDM_MACHINE_RESET` calls `SoftReset()`; `IDM_MACHINE_POWERCYCLE` calls `PowerCycle()`. Each fans out to all registered devices via two new vtable entries on the device base. Locals at top, EHM, ≤ 50 lines. (FR-034, FR-035, audit §10)
- [X] T048 Add `SoftReset()` and `PowerCycle()` to the device base interface (alongside or replacing existing `Reset()`). Provide a default `SoftReset() { /* no-op */ }` and `PowerCycle() { SoftReset(); }` so devices that don't care don't have to opt in. (FR-034)
- [X] T049 Implement `SoftReset` / `PowerCycle` on every concrete device that today defines `Reset`: `RamDevice`, `RomDevice`, `LanguageCard`, `AppleIIeMmu`, `AppleSoftSwitchBank`, `AppleIIeSoftSwitchBank`, `AppleSpeaker`, `AppleKeyboard`, `AppleIIeKeyboard`, `DiskIIController`, `EmuCpu`, `MemoryBus`, `InterruptController`, `VideoOutput`, `VideoTiming`, `DiskImageStore`. SoftReset preserves RAM contents, preserves all `DiskImageStore` mounts (auto-flush dirty images), preserves `VideoTiming` cycle counter; PowerCycle re-seeds RAM via injected `Prng`, unmounts all `DiskImageStore` drives (flushing dirty), and zeroes `VideoTiming::m_cycleCounter`. CPU SoftReset sets I=1, SP per-6502-reset, PC from `(FFFC)`. (FR-034, FR-035, audit §10 C/M-items)
- [X] T050 Add seed-source injection to `EmulatorShell`: production constructs `Prng(static_cast<uint64_t>(time(nullptr)) ^ static_cast<uint64_t>(GetCurrentProcessId()))`; the `HeadlessHost` test factory pins `Prng(0xCA550001)`. The seed source is an explicit injection — production is NOT a hardcoded fallback; it's a "random seed" code path with the pid-XOR-time formula. (FR-035)
- [X] T051 Modify `RamDevice` constructor to accept a `Prng &` reference and seed its buffer on `PowerCycle()` (not on construction). `SoftReset()` is a no-op for RamDevice content. (FR-035, audit §10)
- [X] T052 Add `UnitTest/EmuTests/ResetSemanticsTests.cpp`: `SoftResetPreservesMainRam`, `SoftResetPreservesAuxRam`, `SoftResetPreservesLcRamBothBanksMainAndAux`, `SoftResetSetsCpuIFlagAndAdjustsSP`, `SoftResetResetsSoftSwitchesPerMmuRules`, `SoftResetLeaves80ColModeAtDocumented_ResetState`, `SoftResetClearsVideoTimingCycleCounterPerSpec`, `SoftResetPreservesDiskMountsAndFlushesDirty`, `PowerCycleSeedsAllRamFromPrng`, `PowerCycleZeroesNothingButSeedsEverything`, `PowerCycleResetsLcToBank2WriteRamPrearmed`, `PowerCycleSeedDeterministicWithPinnedSeed`, `PowerCycleZeroesVideoTimingCycleCounter`, `PowerCycleUnmountsAndFlushesAllDisks`. Acceptance: all listed tests pass via `HeadlessHost`. (FR-034, FR-035, audit §10)
- [X] T053 Add `UnitTest/EmuTests/EmulatorShellResetTests.cpp` verifying that `IDM_MACHINE_RESET` → `SoftReset` and `IDM_MACHINE_POWERCYCLE` → `PowerCycle` are correctly dispatched, including that the **80-col mode no longer persists across SoftReset on //e** (the originally-reported bug — audit §10). Acceptance: `EmulatorShellResetTests::ResetMenuItemDispatchesSoftReset`, `PowerCycleMenuItemDispatchesPowerCycle`, `Audit_80ColModePersistenceAcrossResetIsFixed` pass. (FR-034, audit §10)
- [X] T054 [GATE] Build all four configs. Run `ResetSemanticsTests`, `EmulatorShellResetTests`, `LanguageCardTests`, `MmuTests`, full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests in CassoCore + CassoEmuCore + UnitTest still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot.

---

## Phase 5: VideoTiming + VBL

**Purpose**: Implement the `VideoTiming` model and expose `IsInVblank()` to the soft-switch read of $C019. (FR-033; audit §1.2 M `RDVBLBAR`)

- [X] T055 Implement `CassoEmuCore/Video/VideoTiming.h/.cpp` per `contracts/IVideoTiming.md` and `data-model.md` §VideoTiming. Tracks the current scanline and pixel relative to the //e's 65-cycle-per-line, 262-line-per-frame timing. `IsInVblank()` returns true for lines 192-261. `Tick(cycles)` advances the model. Locals at top; ≤ 50 lines per method. (FR-033)
- [X] T056 Wire `VideoTiming` into `EmuCpu`'s tick fan-out so it advances every emulated cycle (`VideoTiming::Tick(emuCpu.GetCyclesElapsedSinceLastFanout())`). (FR-033)
- [X] T057 Wire `AppleIIeSoftSwitchBank` $C019 read to return bit 7 = `videoTiming.IsInVblank() ? 0 : 1` (RDVBLBAR is *not* VBL — it's the inverse: 1 during active scan, 0 during vblank). Bits 0-6 from the floating bus. (FR-033, audit §1.2)
- [X] T058 Add `UnitTest/EmuTests/VideoTimingTests.cpp`: `VideoTimingTests::TickAdvancesScanline`, `VblankRegionIsLines192Through261`, `IsInVblankTransitionsAtLine192Boundary`, `OneFullFrameIs17030Cycles`, `RDVBLBAR_BitInvertedRelativeToIsInVblank`, `SpinWaitOnC019TerminatesWithinOneFrame`. Acceptance: all listed tests pass. (FR-033, audit §1.2 M)
- [X] T059 [GATE] Build all four configs. Run `VideoTimingTests`, full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass; ][/][+ machine configs continue to boot.

---

## Phase 6: Keyboard + Soft-Switch Surface Finalization

**Purpose**: Make Open/Closed Apple/Shift reachable, fix the strobe-clear isolation, and complete the //e soft-switch read surface. (FR-001 reads/writes, FR-013, FR-014; audit §1.2, §4)

- [X] T060 Modify `CassoEmuCore/Devices/AppleIIeKeyboard.h/.cpp` to extend `GetEnd()` from `$C01F` to `$C063`, and route reads of $C061/$C062/$C063 through the existing modifier-key logic (currently dead code). Also handle $C010-$C01F: $C010 (read or write) clears the strobe; $C011-$C01F return keyboard latch in bits 0-6 with the appropriate status flag in bit 7 (where applicable) and MUST NOT clear the strobe. Locals at top, ≤ 50 lines. (FR-001, FR-013, FR-014, audit §4 C-item, §1.2)
- [X] T061 Resolve the read overlap between `AppleIIeKeyboard` ($C010-$C063 now) and `AppleIIeSoftSwitchBank` ($C011-$C01F status reads, $C050-$C07F display switches): per the plan's chosen approach, the keyboard owns $C000-$C010 and $C061-$C063; the soft-switch bank owns $C011-$C01F (status reads — bit 7 from MMU/LC/VideoTiming, bits 0-6 from keyboard latch via a read-only accessor on `AppleIIeKeyboard`). Document the split in headers. (FR-001, FR-003, FR-013, FR-014)
- [X] T062 Update `UnitTest/EmuTests/KeyboardTests.cpp`: `KeyboardTests::OpenAppleReadable_C061`, `ClosedAppleReadable_C062`, `ShiftReadable_C063`, `OnlyC010ClearsStrobe`, `C011ReadDoesNotClearStrobe`, `C012ReadDoesNotClearStrobe`, `C019ReadDoesNotClearStrobe`, `C01EReadDoesNotClearStrobe`, `Audit_OpenClosedAppleNoLongerDeadCode`, `Audit_ShiftKeyImplemented`. Acceptance: all listed tests pass. (FR-013, FR-014, audit §4)
- [X] T063 Final wiring of $C011, $C012, $C01D, $C01F status reads: BSRBANK2 (bit 7 from LC bank flag), BSRREADRAM (bit 7 from LC read-mode), RDHIRES (bit 7 from soft-switch HIRES), RD80VID (bit 7 from MMU 80COL flag). Bits 0-6 = floating-bus from keyboard latch. (FR-001, FR-003, audit §1.2 M-items)
- [X] T064 Final wiring of $C01A, $C01B, $C01C status reads: RDTEXT, RDMIXED, RDPAGE2 — sourced from `AppleSoftSwitchBank`'s display-mode flags. (FR-001, FR-002, FR-003)
- [X] T065 Add `UnitTest/EmuTests/SoftSwitchReadSurfaceTests.cpp`: one test per status read $C011-$C01F that asserts (a) bit 7 reflects the canonical source, (b) bits 0-6 mirror the keyboard latch, (c) the read does not clear strobe, (d) reads do not perturb any other state. 15 tests total (one per address). Acceptance: all 15 pass. (FR-001, FR-003, FR-014, audit §1.2)
- [X] T066 [GATE] Build all four configs. Run `KeyboardTests`, `SoftSwitchReadSurfaceTests`, `SoftSwitchTests`, `MmuTests`, full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot.

---

## Phase 7 — User Story 1 (P1) 🎯 MVP: Cold boot to BASIC prompt + PR#3

**Goal**: A stock //e cold-boots through `apple2e.rom` to the Applesoft `]` prompt; injected `HOME` / `PRINT "HELLO"` / `PR#3` produce the expected text-screen state including the 80-column transition. (User Story 1; FR-001..FR-007, FR-016, FR-017, FR-026..FR-029)

**Independent Test**: `EmuIntegrationTests::Phase 7_*` scenarios via `HeadlessHost.BuildAppleIIe()`; scrape main + aux text page; assert against expected character spans.

- [ ] T067 [P] [Phase 7] Add helper `UnitTest/EmuTests/TextScreenScraper.h/.cpp` — converts main+aux text page memory plus the 80COL/PAGE2/MIXED state into a row-major `std::vector<std::string>` for assertion. Uses //e screen-line address arithmetic. Locals at top; ≤ 50 lines per helper. (FR-016, FR-017, FR-037)
- [ ] T068 [P] [Phase 7] Add helper `UnitTest/EmuTests/KeystrokeInjector.h/.cpp` — injects ASCII strings + Return into `AppleIIeKeyboard` with strobe handling and inter-keystroke yield (one emulated frame). (FR-013, FR-037)
- [ ] T069 [Phase 7] Add `UnitTest/EmuTests/EmuIntegrationTests.cpp` (or extend if it exists) with `EmuIntegrationTests::Phase 7_ColdBootReaches_BASIC_Prompt`. Powers on the //e via `HeadlessHost.BuildAppleIIe().PowerCycle()`, runs N cycles equivalent to one real //e boot cycle, scrapes the text screen, and asserts the last visible row contains `]` followed by the cursor character in the expected column. Acceptance: test passes deterministically (re-run produces identical scrape). (Phase 7 acceptance scenario 1; FR-001..FR-029, SC-002)
- [ ] T070 [Phase 7] Add `EmuIntegrationTests::Phase 7_HOME_PRINT_HELLO_Visible`. Boots, then injects `HOME`, Return, `PRINT "HELLO"`, Return; asserts `HELLO` appears on the row above the new prompt. Acceptance: test passes. (Phase 7 acceptance scenario 2; FR-013, FR-016)
- [ ] T071 [Phase 7] Add `EmuIntegrationTests::Phase 7_PR3_Activates_80Column_Mode`. Boots, injects `PR#3`, Return; asserts (a) `RD80VID` ($C01F) bit 7 is set, (b) `RD80STORE` ($C018) bit 7 is set (firmware engages 80STORE), (c) subsequent injected `PRINT "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789012345678901234567890"` (≥ 41 chars) renders correctly across main+aux interleave on the 80-column scrape. Acceptance: test passes. (Phase 7 acceptance scenario 3; FR-001, FR-007, FR-017)
- [ ] T072 [Phase 7] Add `EmuIntegrationTests::Phase 7_OpenClosedApple_Shift_StatusReads`. While at the BASIC prompt, programmatically depress Open Apple, Closed Apple, Shift via the keyboard's modifier-state setters, then BIT-test $C061/$C062/$C063 from injected machine code (or via direct memory bus read by the test) and assert bit-7 transitions correctly. Acceptance: test passes. (Phase 7 acceptance scenario 4; FR-013)
- [ ] T073 [Phase 7] [GATE] Build all four configs. Run all `EmuIntegrationTests::Phase 7_*` plus the full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot; all four Phase 7 scenarios pass deterministically across two consecutive runs.

---

## Phase 8 — User Story 3 (P1): //e-specific memory & Language Card programs

**Goal**: Programs exercising aux RAM via RAMRD/RAMWRT, the LC pre-write sequence, ALTZP banking, and 80STORE-routed hi-res writes behave indistinguishably from real hardware. (User Story 3; FR-005, FR-006, FR-007, FR-008..FR-012, FR-019)

**Independent Test**: Targeted `EmuIntegrationTests::Phase 8_*` scenarios that drive deterministic patterns through every soft-switch combination and compare against a precomputed table.

- [ ] T074 [P] [Phase 8] Add `UnitTest/EmuTests/MemoryProbeHelpers.h/.cpp` — peek/poke into main, aux, LC bank1, LC bank2, aux LC bank1, aux LC bank2 directly via `MemoryBus` overrides, bypassing the CPU. Locals at top. (FR-005, FR-006, FR-010)
- [ ] T075 [Phase 8] Add `EmuIntegrationTests::Phase 8_RamRd_RamWrt_RouteAuxIndependently`. Patterns: write `0xAA` at $4000 with RAMWRT=0; set RAMWRT=1, write `0x55` at $4000; set RAMRD=0, read $4000 → `0xAA`; set RAMRD=1, read → `0x55`. Acceptance: test passes. (Phase 8 acceptance scenario 1; FR-005, audit §2)
- [ ] T076 [Phase 8] Add `EmuIntegrationTests::Phase 8_LcPreWrite_AnyTwoOddReads_EnablesWrite`, `Phase 8_LcPreWrite_InterveningWriteResets`, `Phase 8_LcPowerOnDefaultsToBank2WriteRamPrearmed`. (Phase 8 acceptance scenario 2; FR-008, FR-009, FR-011)
- [ ] T077 [Phase 8] Add `EmuIntegrationTests::Phase 8_AltZp_RoutesZpStackToAux`, `Phase 8_AltZp_RoutesLcWindowToAuxBank`. (Phase 8 acceptance scenario 3; FR-006, FR-010)
- [ ] T078 [Phase 8] Add `EmuIntegrationTests::Phase 8_Store80_PlusHiresPlusPage2_RoutesHiresWritesToAux`. With 80STORE=1, HIRES=1, PAGE2=1, write to $2000-$3FFF and assert the data lands in aux hi-res memory regardless of RAMWRT. (Phase 8 acceptance scenario 4; FR-007)
- [ ] T079 [Phase 8] Add `EmuIntegrationTests::Phase 8_SoftReset_PreservesAuxAndLcRam_AndPostsCpuPostResetState`. Soft-reset and assert aux RAM + both LC banks (main + aux) are unchanged; CPU I=1, SP per-spec, PC = `(FFFC)`. (Phase 8 acceptance scenario 5; FR-012, FR-034)
- [ ] T080 [Phase 8] [GATE] Build all four configs. Run all `EmuIntegrationTests::Phase 8_*`, plus Phase 7, full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot.

---

## Phase 9: Disk II Nibble Engine

**Purpose**: Replace the existing sector-level Disk II controller with a nibble-level LSS-driven controller backed by an `IDiskImage` abstraction. (FR-021, FR-022, FR-024; audit §7)

- [ ] T081 Implement `CassoEmuCore/Devices/Disk/DiskImage.h/.cpp` — concrete `IDiskImage` holding up to 40 variable-length per-track bit streams (35 typical for DOS 3.3, more for WOZ), plus per-track write-protect and dirty bits. (FR-021, FR-024)
- [ ] T082 Implement `CassoEmuCore/Devices/Disk/DiskIINibbleEngine.h/.cpp` per `data-model.md` §DiskIINibbleEngine — bit-stream LSS engine that, on each `Tick(cycles)` call, advances through the current track's bit stream at the correct bit-time per emulated cycle (≈4µs/bit at the standard data rate). Exposes `Tick(cycles)`, `ReadLatch()`, `WriteLatch(uint8_t)` per `contracts/IDiskController.md`; head positioning, motor state, drive selection, and write-protect query are owned by `DiskIIController` (T083) and pushed into the engine via setters. Locals at top; per-method ≤ 50 lines; LSS state-machine helpers extracted per phase to keep ≤ 2 indent levels. (FR-021)
- [ ] T083 Major rewrite of `CassoEmuCore/Devices/DiskIIController.h/.cpp` — owns two `DiskIINibbleEngine` instances (drives 1 + 2), implements $C0Ex/$C0Fx soft switches per audit §7: phase magnets ($C0E0-$C0E7), motor on/off ($C0E8/$C0E9), drive select ($C0EA/$C0EB), Q6/Q7 latches ($C0EC-$C0EF), with full LSS Q6/Q7 cross-product (read-data, read-write-protect, shift-load, write). Function bodies ≤ 50 lines; comments in `.cpp`. (FR-021, audit §7)
- [ ] T084 Wire the rewritten controller's slot 6 ROM ($C600-$C6FF) and (post INTC8ROM access) $C800-$CFFF expansion ROM through the MMU's `ResolveCxxx`. Audit §7 + §8 fix-by-elimination of the ROM shadow. (FR-028, audit §7 / §8)
- [ ] T085 Major rewrite of `UnitTest/EmuTests/DiskIITests.cpp`: `DiskIITests::PhaseMagnetsStepHeadCorrectly`, `MotorOnOffViaC0E8C0E9`, `DriveSelectViaC0EAC0EB`, `Q7ClearQ6ClearReadsNibble`, `Q7ClearQ6SetReadsWriteProtect`, `Q7SetQ6ClearShiftLoad`, `Q7SetQ6SetWritesNibble`, `NibbleStreamAdvancesAtCorrectBitTime`, `WriteProtectedDiskRejectsWrite`, `HeadStepWrapsAtTrackBoundaries`, `LSS_ReadsKnownNibblePattern`, `LSS_WritesPatternRoundTrip`. Acceptance: all listed tests pass. (FR-021, FR-024, audit §7)
- [ ] T086 Add `UnitTest/EmuTests/DiskIINibbleEngineTests.cpp` — pure-unit tests of the engine independent of the controller: `BitTimingMatches4uSPerBit`, `ReadAdvancesPosition`, `WriteAdvancesPositionAndMarksDirty`, `MotorOffFreezesPosition`, `SetCurrentTrackClampsToValidRange`. Acceptance: all listed tests pass. (FR-021)
- [ ] T087 Implement WOZ Phase 12/v2 native loader at `CassoEmuCore/Devices/Disk/WozLoader.h/.cpp`. Parses INFO, TMAP, TRKS chunks; produces a `DiskImage` with native nibble tracks. EHM throughout. (FR-022)
- [ ] T088 Add `UnitTest/EmuTests/WozLoaderTests.cpp`: `WozLoaderTests::ParsesV1Header`, `ParsesV2Header`, `RejectsNonWozMagic`, `RejectsCorruptCRC`, `LoadsAllPresentTracks`, `RoundTripIdentityForKnownFixture`. Acceptance: all listed tests pass; uses a tiny project-original WOZ blob in `UnitTest/Fixtures/sample.woz` (generated by repo tooling — keep ≤ a few KB). (FR-022)
- [ ] T089 [GATE] Build all four configs. Run `DiskIITests`, `DiskIINibbleEngineTests`, `WozLoaderTests`, plus full pre-existing suite (incl. existing `DiskImageTests` if any — those assertions must be migrated to nibble semantics). Boot ][ and ][+ — verify legacy disk path still works (][+ uses the same nibble engine via Drive 1 in slot 6). Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot.

---

## Phase 10: Nibblization Layer + Auto-Flush

**Purpose**: Load `.dsk` / `.do` / `.po` by nibblization on load; flush nibble→sector on eject/switch/exit. (FR-023, FR-025; audit §7)

- [ ] T090 Implement `CassoEmuCore/Devices/Disk/NibblizationLayer.h/.cpp` — `Nibblize(sectorImage, kind) → DiskImage` and the inverse `Denibblize(DiskImage) → sectorImage` for `.dsk` (DOS 3.3 interleave), `.do` (DOS 3.3), `.po` (ProDOS interleave). 6-and-2 encoding; address+data field framing; epilogues; track sync gaps. (FR-023, audit §7)
- [ ] T091 Implement `CassoEmuCore/Devices/Disk/DiskImageStore.h/.cpp` — load coordinator: detects `.woz` vs `.dsk` vs `.do` vs `.po` by extension + magic, dispatches to `WozLoader` or `NibblizationLayer`. Tracks the source file path, the in-memory `DiskImage`, and dirty bits. Provides `Flush()` that writes back via the appropriate inverse path. (FR-023, FR-025)
- [ ] T092 Wire auto-flush hooks: on disk eject (`DiskIIController::Eject`), on machine-config switch (`EmulatorShell::SwitchMachine`), on emulator exit (`EmulatorShell::Shutdown`). Each calls `DiskImageStore::Flush()` for every mounted disk with dirty bits. Add a "graceful exit" path that runs even if WM_CLOSE fires the standard window destruction. (FR-025, audit §7)
- [ ] T093 Add `UnitTest/EmuTests/NibblizationTests.cpp`: `NibblizationTests::DskRoundTripIdentity`, `DoRoundTripIdentity`, `PoRoundTripIdentity`, `AddressFieldFraming`, `DataFieldFraming`, `SixAndTwoEncodingMatchesReference`, `EpilogueBytesPresent`, `NibblizeThenDenibblizeProducesByteEqualOriginal`. Acceptance: all listed tests pass against tiny project-original sector fixtures (e.g. all-zero, all-`0x55`, alternating, random-with-pinned-Prng-seed). (FR-023)
- [ ] T094 Add `UnitTest/EmuTests/DiskImageStoreTests.cpp`: `DiskImageStoreTests::LoadDetectsFormatByExtension`, `LoadWozNativelyNoNibblization`, `LoadDskRunsNibblization`, `FlushDirtyImageWritesBackToSource`, `FlushCleanImageIsNoop`, `EjectAutoFlushes`, `MachineSwitchAutoFlushesAllMounted`, `ShutdownAutoFlushesAllMounted`, `WriteThenEjectProducesByteEqualReference`. Uses `IFixtureProvider` rooted at `UnitTest/Fixtures/`; writes go to an in-memory blob (not the host filesystem). Acceptance: all listed tests pass. (FR-023, FR-025, audit §7)
- [ ] T095 Wire the nibble-level WOZ test image and a tiny project-original `.dsk` round-trip fixture into `UnitTest/Fixtures/`; document provenance in `Fixtures/README.md`. (FR-044)
- [ ] T096 [GATE] Build all four configs. Run `NibblizationTests`, `DiskImageStoreTests`, `WozLoaderTests`, `DiskIITests`, full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot.

---

## Phase 11 — User Story 2 (P1): Disk-based real software incl. copy-protected titles

**Goal**: DOS 3.3, ProDOS, WOZ, and a public-domain copy-protected disk all boot to their post-boot state via the headless harness; write-then-eject produces a byte-equal modified image. (User Story 2; FR-021..FR-025)

**Independent Test**: `EmuIntegrationTests::Phase 11_*` integration scenarios using fixture disks.

- [ ] T097 Bundle a public-domain DOS 3.3 system disk as `UnitTest/Fixtures/dos33.dsk` with provenance documented. (FR-044)
- [ ] T098 Bundle a public-domain ProDOS boot disk as `UnitTest/Fixtures/prodos.po`. (FR-044)
- [ ] T099 Bundle a public-domain copy-protected sample as `UnitTest/Fixtures/copyprotected.woz` (e.g. a CP demo with custom track sync; provenance documented). (FR-024, FR-044)
- [ ] T100 [Phase 11] Add `EmuIntegrationTests::Phase 11_DOS33_Boots_And_Catalog_Works`. Mount `dos33.dsk` in slot 6 drive 1; `PowerCycle()`; run cycles equivalent to a DOS 3.3 boot; inject `CATALOG`, Return; scrape text screen; assert it contains the expected file listing. Acceptance: test passes deterministically. (Phase 11 scenario 1; FR-021, FR-023)
- [ ] T101 [Phase 11] Add `EmuIntegrationTests::Phase 11_ProDOS_Boots_And_CAT_Works`. Mount `prodos.po`; `PowerCycle()`; inject `CAT` (or `PREFIX`), Return; assert expected output. Acceptance: test passes. (Phase 11 scenario 2; FR-021, FR-023)
- [ ] T102 [Phase 11] Add `EmuIntegrationTests::Phase 11_WOZ_Boots_And_FirstTrack_Executes`. Mount `sample.woz`; `PowerCycle()`; assert framebuffer hash matches `Fixtures/golden/woz_titlescreen.hash`. (Generate the golden hash deterministically from the first passing run; check in.) Acceptance: test passes. (Phase 11 scenario 3; FR-022)
- [ ] T103 [Phase 11] Add `EmuIntegrationTests::Phase 11_CopyProtected_Disk_Boots_To_TitleScreen`. Mount `copyprotected.woz`; `PowerCycle()`; assert text-screen scrape OR framebuffer hash matches the recorded reference for the post-CP-check title screen. Acceptance: test passes. (Phase 11 scenario 4; FR-024 — the explicit booting requirement of a CP-protected sample disk to its title screen via the headless harness)
- [ ] T104 [Phase 11] Add `EmuIntegrationTests::Phase 11_WriteThenEject_ProducesByteEqualImage`. Mount a writable in-memory `.dsk`; issue track writes via injected machine code (or controller-level write); call `Eject`; assert the in-memory output blob byte-equals the precomputed reference. Acceptance: test passes. (Phase 11 scenario 5; FR-025)
- [ ] T105 [Phase 11] [GATE] Build all four configs. Run all `EmuIntegrationTests::Phase 11_*`, Phase 7, Phase 8, full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot; all five Phase 11 scenarios pass deterministically across two consecutive runs.

---

## Phase 12: Video Correctness

**Purpose**: 80-col interleave fix + ALTCHARSET + flash; mixed-mode 80-col routing through composed renderer; NTSC artifact in hi-res; DHR aux/main interleave. (FR-016, FR-017, FR-017a, FR-018, FR-019, FR-020; audit §6)

- [ ] T106 Modify `CassoEmuCore/Video/Apple80ColTextMode.h/.cpp`: maintain `m_frameCount` and `m_flashOn` flags; pass `m_altCharSet` to `GetGlyphRow()`; correct aux/main interleave (aux char at even column, main char at odd column per //e). Extract `RenderRow(rowIndex)` helper so it can be called from MIXED+80COL bottom-4-rows path (per FR-020 — same path, not branched). Locals at top; ≤ 50 lines per method. (FR-017, FR-017a, FR-020, audit §6.2 M)
- [ ] T107 Modify `CassoEmuCore/Video/VideoOutput.h` and the composed mode dispatcher: when MIXED is active AND 80COL is active, the bottom 4 rows dispatch to `Apple80ColTextMode::RenderRow` (NOT to a duplicated 40-col path). When MIXED is active AND 80COL is clear, the bottom 4 rows dispatch to the 40-col `AppleTextMode::RenderRow`. Single composed path. (FR-017a, FR-020)
- [ ] T108 Modify `CassoEmuCore/Video/AppleHiResMode.h/.cpp` to emit NTSC artifact color (the 6-color palette derived from bit-pair patterns + delay bit). Reference implementation: AppleWin's NTSC algorithm; ≤ 50 lines per helper. (FR-018, audit §6.4)
- [ ] T109 Modify `CassoEmuCore/Video/AppleDoubleHiResMode.h/.cpp` for correct aux/main DHR interleave per audit §6.5: aux byte at even page address, main byte at odd; 4 bits per byte → 16-color palette. Replace the existing stub. (FR-019, audit §6.5 M / issue #61)
- [ ] T110 Add `UnitTest/EmuTests/VideoModeTests.cpp` updates: `VideoModeTests::TextMode40_RendersASCIIInverseFlash`, `TextMode80_RendersAuxMainInterleave`, `TextMode80_AltCharsetSwitchesGlyphSet`, `TextMode80_FlashesWhenAltCharSetSelectsFlashingSet`, `MixedMode_BottomFourRowsAre40ColWhen80ColClear`, `MixedMode_BottomFourRowsAre80ColWhen80ColSet_RoutedThroughComposedRenderer`, `HiRes_NTSCArtifact_ProducesSixColorOutput`, `DHR_AuxMainInterleaveProduces16ColorOutput`. Acceptance: all listed tests pass. (FR-016, FR-017, FR-017a, FR-018, FR-019, FR-020)
- [ ] T111 Add `UnitTest/EmuTests/VideoRenderTests.cpp` golden-hash assertions: `VideoRenderTests::DhrTestPattern_HashMatches_Fixtures_golden_dhr_testpattern_hash`, `Mixed80Col_TestProgram_HashMatches_Fixtures_golden_80col_mixed_hash`. Generate the golden hashes from the first passing run with `Prng(0xCA550001)`; check the SHA-256 hash files into `Fixtures/golden/`. Acceptance: tests pass deterministically across two runs. (FR-017a, FR-019, FR-038)
- [ ] T112 [GATE] Build all four configs. Run `VideoModeTests`, `VideoRenderTests`, `VideoTimingTests`, Phase 7, Phase 11, Phase 8, full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot.

---

## Phase 13 — User Story 4 (P1): Headless harness validation suite

**Goal**: The full FR-045 validation suite runs in CI, deterministically, with no host I/O outside `UnitTest/Fixtures/`. (User Story 4; FR-036, FR-037, FR-038, FR-043, FR-044, FR-045)

**Independent Test**: Run the suite twice on a clean checkout; identical pass/fail and identical scraped/hashed outputs.

- [ ] T113 [Phase 13] Add `EmuIntegrationTests::Phase 13_NoWindowOpenedDuringSuite` — uses a process-level guard injected by `HeadlessHost` to assert no `CreateWindow*` call occurred, no audio device opened, no host file outside `UnitTest/Fixtures/` was opened. Acceptance: test passes. (Phase 13 scenario 1; FR-036, FR-043, FR-044, SC-006)
- [ ] T114 [Phase 13] Add `EmuIntegrationTests::Phase 13_TwoConsecutiveRunsProduceIdenticalOutputs` — runs the entire Phase 7+Phase 8+Phase 11+Phase 12 scenario set twice and diffs every scraped string, every framebuffer hash, every memory snapshot. Acceptance: zero diffs. (Phase 13 scenario 2; FR-038, SC-005)
- [ ] T115 [Phase 13] Add `EmuIntegrationTests::Phase 13_RegressionDiagnosticPointsToSubsystem` — a meta-test that, for each in-scope subsystem (soft-switch, MMU, LC, keyboard, video, Disk II, ROM mapping, IRQ, VBL, reset), confirms a designated "canary assertion" exists and would fire with a recognizable diagnostic string. Acceptance: all 10 canary assertions are reachable. (Phase 13 scenario 3; FR-036)
- [ ] T116 [Phase 13] Add `EmuIntegrationTests::Phase 13_MixedMode_80Col_GoldenOutput` (the clarify-pass golden output test). Loads a fixture program that draws hi-res in the top region and 80-column text in the bottom 4 rows; powers on; runs to a stable frame; asserts (a) the bottom-4-rows scrape matches expected interleaved 80-col text, (b) the graphics region framebuffer hash matches, (c) the result matches `Fixtures/golden/80col_mixed.hash`. Acceptance: test passes. (Phase 13 scenario 4 — explicit clarify-pass requirement; FR-017a, FR-020)
- [ ] T117 [Phase 13] Add `EmuIntegrationTests::Phase 13_FR045_ValidationSuite_RunsAllScenarios` — a single orchestrator test that invokes (or asserts the existence + green status of) every FR-045 scenario: cold-boot to `]`, `PR#3` 80-col, `HOME`/`PRINT`/`GR`/`HGR`/`HGR2`, aux+LC memory program, soft-reset survives, DOS 3.3 boot+CATALOG, ProDOS boot+CAT, WOZ boot, copy-protected boot. Acceptance: every named sub-scenario reports green. (FR-045, SC-009)
- [ ] T118 [Phase 13] Add `EmuIntegrationTests::Phase 13_HarnessRejectsHostFilesystemPathsOutsideFixtures` — attempts to open a path outside `UnitTest/Fixtures/` via the test `IFixtureProvider` and asserts it returns `E_ACCESSDENIED` (or equivalent HRESULT). Acceptance: test passes. (FR-043, SC-006)
- [ ] T119 [Phase 13] [GATE] Build all four configs. Run all `EmuIntegrationTests::Phase 13_*`, plus Phase 7, Phase 11, Phase 8, Phase 12, full pre-existing suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot; full headless suite is green twice in a row with byte-identical hashes.

---

## Phase 14 — User Story 5 (P1): Backwards compatibility

**Goal**: ][/][+ continue to work; existing tests pass without assertion changes (other than those that encoded bugs since fixed in Phase 3). (User Story 5; FR-039, FR-040)

- [ ] T120 [Phase 14] Add `UnitTest/EmuTests/BackwardsCompatTests.cpp` re-asserting the pre-feature ][/][+ scenarios as explicit named tests: `BackwardsCompatTests::AppleII_ColdBootReachesIntegerBASIC`, `AppleIIPlus_ColdBootReachesApplesoftBASIC`, `AppleIIPlus_DiskIIBootsDOS33`, `AppleIIPlus_LanguageCardScenario_StillPasses`, `AppleII_KeyboardBehavior_Unchanged`, `AppleII_VideoModes_Unchanged`. These tests serve as the explicit regression gate — backwards compat is non-negotiable. Acceptance: all listed tests pass. (Phase 14 acceptance scenarios 1-2; FR-039, FR-040, SC-004)
- [ ] T121 [Phase 14] Audit every test file in `UnitTest/` modified by phases Phase 1-Phase 6 + Phase 12 + Phase 9 + Phase 10 to confirm no ][/][+ assertion has been weakened. Produce an audit log in `specs/004-apple-iie-fidelity/backwards-compat-audit.md` enumerating every modified ][/][+ assertion and its justification. (FR-039, FR-040)
- [ ] T122 [Phase 14] [GATE] Continuous gate: build all four configs; run the **entire** UnitTest suite (all phases combined); explicitly run the existing `Apple2Tests` / `Apple2PlusTests` files. Boot ][ and ][+ via the GUI Casso shell (smoke). Acceptance: every previously-passing test in the entire suite still passes on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot in both headless harness and GUI shell.

---

## Phase 15 — User Story 6 (P2): Performance

**Goal**: //e at 1.023 MHz consumes ≤ ~1% of one host core; verified by an automated perf-measurement test. (User Story 6; FR-042, SC-007)

- [ ] T123 [Phase 15] Implement `UnitTest/EmuTests/PerformanceTests.cpp` per `plan.md` §"Performance measurement strategy". Runs in **Release only** (skipped in Debug). Measures wall-clock cost of 1,000,000 emulated cycles on a //e at the BASIC idle loop using `QueryPerformanceCounter`. Asserts the wall-clock cost is below a threshold computed for ≥ 10× headroom over real //e speed (i.e., `≤ 1,000,000 / 10,230,000 s ≈ 97.75 ms`). The threshold is centralized in a single named `kPerformanceCeilingMs` constant. Locals at top; EHM. Acceptance: test passes consistently in Release on ARM64+x64. (Phase 15 scenarios 1-2; FR-042, SC-007 — perf-measurement test asserting ~1% host CPU when emulating 1.023 MHz //e)
- [ ] T124 [Phase 15] Document the perf-test's sampling protocol and CI gating in `specs/004-apple-iie-fidelity/quickstart.md` under a new "Performance" section: what host class the budget assumes; what to do if the test flakes on slower CI hardware; how to recompute the threshold. (FR-042, SC-007)
- [ ] T125 [Phase 15] [GATE] Build Release on ARM64+x64. Run `PerformanceTests` 5 times in succession; assert all 5 runs pass and that the worst-case wall-clock cost is within 30% of the median (stability gate). Run the full UnitTest suite. Boot ][ and ][+. Acceptance: all previously-passing tests still pass; perf test green on ARM64+x64 Release; ][/][+ machine configs continue to boot.

---

## Phase 16: Polish & Cross-Cutting Audit

**Purpose**: Final constitution Phase 12.4.0 conformance audit, docs, and the Dormann + Harte runs that gate every CPU-touching feature.

- [ ] T126 [P] Header-comment audit: grep every `.h` file changed in this feature for function comments. Confirm function comments live ONLY in `.cpp`. Output a report at `specs/004-apple-iie-fidelity/audit-header-comments.md`; zero violations required. (Constitution §I 1.4.0)
- [ ] T127 [P] Macro-argument audit: grep every changed `.cpp` line for non-trivial calls inside macro arguments (`CHR(`, `CWR(`, `CHRA(`, `CHRF(`, `CBR(`, `CBRF(`, `CBRN(`, `CHRN(`, `IGNORE_RETURN_VALUE(`, `Assert::`, `Logger::`). Each non-trivial-call hit must be refactored to store the result first. Output a report at `specs/004-apple-iie-fidelity/audit-macro-args.md`; zero violations required. (Constitution §I 1.4.0)
- [ ] T128 [P] Function-spacing audit: grep every changed line for `\b[a-zA-Z_]\w*\(` violations of the `func()` / `func (a, b)` / `if (...)` / `for (...)` rules. Output a report at `specs/004-apple-iie-fidelity/audit-function-spacing.md`; zero violations required. (Constitution §I 1.4.0)
- [ ] T128a [P] EHM-on-fallible-internals audit: grep every changed `.cpp` for `void`-returning (and other non-HRESULT-returning) functions whose body contains fallible operations (`new`, `CHR`/`CWR`/`CHRA`/`CHRF`/`CBR`/`CBRF` macros, Win32 `CreateFileW`/`RegOpenKey*`/etc., or any function call documented to fail). For each hit, confirm the function follows the EHM pattern internally (declares `HRESULT hr = S_OK`, uses `Error:` label or equivalent flat-control flow, even though no HRESULT escapes). Output a report at `specs/004-apple-iie-fidelity/audit-ehm-on-fallible.md`; zero violations required. (Constitution §I 1.4.0 — expanded EHM rule)
- [ ] T128b [P] Unnecessary-scope-block audit: grep every changed `.cpp` for `^\s*\{` lines NOT preceded by a control-flow keyword (`if`, `for`, `while`, `do`, `switch`, `try`, `catch`, function definition, struct/class/union, lambda, RAII-lifetime intent). Confirm every brace block exists for a real reason. Output a report at `specs/004-apple-iie-fidelity/audit-scope-blocks.md`; zero violations required. (Constitution §I 1.4.0)
- [ ] T129 [P] Function-size + indent-depth audit: grep every changed function definition for ≤ 50 lines (100 max), ≤ 2 indent levels beyond EHM (3 max), and locals-at-top compliance. Output a report at `specs/004-apple-iie-fidelity/audit-function-size.md`; zero violations required. (Constitution §I + §V 1.4.0)
- [ ] T130 [P] Variable-declaration column-alignment audit on every changed `.cpp` file's top-of-function declaration blocks; report at `specs/004-apple-iie-fidelity/audit-decl-alignment.md`; zero violations required. (Constitution §I 1.4.0)
- [ ] T131 [P] Magic-number audit on every new file produced by this feature; flag every literal that is not 0/1/-1/nullptr/sizeof and confirm it's a named constant. Report at `specs/004-apple-iie-fidelity/audit-magic-numbers.md`; zero violations required. (Constitution §I)
- [ ] T132 Update `CHANGELOG.md` with a `## [Unreleased]` block enumerating user-visible //e changes: cold-boot to BASIC, 80-col + DHR, aux RAM, LC fix, Disk II nibble engine + WOZ + auto-flush, IRQ infrastructure, soft-vs-power reset, headless harness. (Repository convention)
- [ ] T133 Update `README.md`: bump test counts; update the //e roadmap section to reflect this feature's deliverables; document the headless harness; document the new perf gate. (Repository convention)
- [ ] T134 Run `scripts/RunDormannTest.ps1` (Klaus Dormann 6502 functional test). Acceptance: PASS. Required because this feature changes CPU plumbing (Phase 1). (Repository convention — significant CPU change)
- [ ] T135 Run `scripts/RunHarteTests.ps1 -SkipGenerate` (Tom Harte SingleStepTests). Acceptance: PASS. Required because this feature changes CPU plumbing. (Repository convention — significant CPU change)
- [ ] T136 Run `scripts\Build.ps1 -RunCodeAnalysis` on Debug and Release for x64 and ARM64. Acceptance: zero analysis warnings introduced by this feature. (Repository convention — pre-commit gate)
- [ ] T137 [GATE] Final feature gate: build all four configs; run the entire UnitTest suite; run Dormann; run Harte; boot ][ ][+ //e via the GUI Casso shell (smoke); execute the FR-045 validation suite via the headless harness. Acceptance: every previously-passing test still passes on ARM64+x64 Debug+Release; ][/][+ machine configs continue to boot; //e cold-boots to `]` prompt; FR-045 suite green; SC-001 through SC-009 all met.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 0 → Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5 → Phase 6** — strict serial; each phase ends with a `[GATE]` that the next phase depends on.
- **Phase 7** depends on Phases 0-6.
- **Phase 8** depends on Phase 2, Phase 3, Phase 4 (in practice: ≥ Phase 4 gate green).
- **Phase 9** depends on Phase 2 (slot 6 reachable).
- **Phase 10** depends on Phase 9.
- **Phase 11** depends on Phase 9 + Phase 10 + Phase 7.
- **Phase 12** depends on Phase 2 (MMU routing for aux/main video memory) + Phase 5 (timing). Ideally after Phase 8.
- **Phase 13** depends on Phases 0-6 + Phase 12 + Phase 9 + Phase 10 + Phase 7 + Phase 11 + Phase 8.
- **Phase 14** is a continuous gate from Phase 0 onward; its explicit phase only adds the named `BackwardsCompatTests` and the audit log.
- **Phase 15** depends on the //e being functionally correct (after Phase 12 + Phase 11 + Phase 8).
- **P1** depends on everything else complete.

### Within Each Phase

- Header-only contract tasks (`I*.h`) marked `[P]` can land in any order in Phase 0.
- Test tasks for a given subsystem MUST land alongside (or after) the production task; never as `[P]` against the production task that owns the same file.
- Per constitution §II, every test uses `HeadlessHost` / `IFixtureProvider` plumbing — no host state.

### Production-to-test acceptance convention

Every production-only task in this list (a task that creates or modifies code without naming explicit tests in its own line) is **implicitly accepted by the next test task within the same phase that names test cases**. The pattern is:

> Production task TXXX → ... → Test task TYYY (`Acceptance: tests A/B/C pass`) → Phase `[GATE]` task

If a production task has no test task following it in the phase, that's a coverage bug — flag it before the phase `[GATE]`. The phase `[GATE]` task is the final acceptance for every task in its phase (production AND test) and is the merge gate.

Tasks with `**Acceptance: covered by TYYY's tests**` lines added inline (e.g. T024, T026, T028) are an explicit version of this convention; the inline note exists where the test task's name is non-obvious or where one test task covers multiple production tasks.

### Parallel Opportunities

- Phase 0: T002-T013, T016-T019 are `[P]` — independent files, one developer can land them in parallel.
- Phase 1: T024 and T026 both touch the `ICpu::SetInterruptLine` boundary; T026 calls into the Cpu6502 implementation T024 just produced. Kept serial.
- Phase 7 helpers T067-T068 are `[P]`.
- Phase 8 probe helpers T074 and the four scenario tests T075-T078 are independent of each other and could be `[P]` (one developer; kept lightly sequenced).
- P1 audits T126-T131, T128a, T128b are all `[P]`.

---

## Parallel Example: Phase 0 setup

```bash
# Land all eleven contract headers + four mocks + fixture staging together (one PR each):
Task T002: Stage in-repo binary fixtures
Task T003: contracts/IFixtureProvider.h
Task T004: contracts/ICpu.h
Task T005: contracts/I6502DebugInfo.h
Task T006: contracts/IInterruptController.h
Task T007: contracts/IMmu.h
Task T008: contracts/IDiskImage.h
Task T009: contracts/IDiskController.h
Task T010: contracts/IVideoTiming.h
Task T011: contracts/IVideoMode.h
Task T012: contracts/IHostShell.h
Task T013: contracts/ISoftSwitchBank.h
Task T016: MockAudioSink
Task T017: MockIrqAsserter
Task T018: MockHostShell
```

---

## Implementation Strategy

### MVP (Phase 7 only)

1. Complete Phase 0 → Phase 6 in order (each gate green before proceeding).
2. Complete Phase 7 (T067-T073).
3. STOP & VALIDATE: //e cold-boots to `]`, `PR#3` works, modifier keys read.
4. Demo MVP.

### Incremental Delivery

After MVP:
1. Add Phase 8 → //e-specific memory & LC programs work.
2. Add Phase 9 + Phase 10 → disk infrastructure ready.
3. Add Phase 11 → real software (incl. CP) runs.
4. Add Phase 12 → video correctness golden hashes.
5. Add Phase 13 → headless harness validation suite green twice in a row.
6. Add Phase 14 → backwards-compat audit committed.
7. Add Phase 15 → perf gate green.
8. Polish (Phase 16) → docs, audits, Dormann, Harte, final GATE.

### Parallel Team Strategy

- Dev A: Phase 1 + Phase 8 + Phase 15 (CPU/IRQ + memory programs + perf).
- Dev B: Phase 2 + Phase 12 + Phase 7 (MMU + video + boot).
- Dev C: Phase 9 + Phase 10 + Phase 11 (disk stack).
- Shared: Phase 0 (everyone contributes), Phases 3-6 (rotating ownership), Phase 13 + Phase 14 + P1 (joint).

---

## Coverage Matrix (every FR + every audit [CRITICAL]/[MAJOR] item is covered)

| FR | Tasks |
|---|---|
| FR-001 | T031, T033, T034, T036, T037, T060, T061, T063, T064, T065 |
| FR-002 | T064 |
| FR-003 | T034, T037, T061, T063, T065 |
| FR-004 | T013, T031 |
| FR-005 | T031, T032, T036, T074, T075 |
| FR-006 | T031, T036, T074, T077 |
| FR-007 | T031, T032, T036, T071, T078 |
| FR-008 | T041, T044, T076 |
| FR-009 | T041, T044, T076 |
| FR-010 | T043, T044, T077 |
| FR-011 | T042, T044, T076 |
| FR-012 | T042, T044, T079 |
| FR-013 | T060, T061, T062, T068, T072 |
| FR-014 | T060, T062, T065 |
| FR-015 | T015a |
| FR-016 | T106, T110, T067 |
| FR-017 | T106, T110, T071 |
| FR-017a | T106, T107, T110, T111, T116 |
| FR-018 | T108, T110 |
| FR-019 | T109, T110, T111 |
| FR-020 | T011, T106, T107 |
| FR-021 | T081, T082, T083, T085, T086, T100, T101 |
| FR-022 | T087, T088, T102 |
| FR-023 | T090, T091, T093, T094, T100, T101 |
| FR-024 | T081, T085, T099, T103 |
| FR-025 | T091, T092, T094, T104 |
| FR-026 | T035, T036 |
| FR-027 | T035, T036 |
| FR-028 | T035, T036, T084 |
| FR-029 | T035 |
| FR-030 | T004, T005, T022, T023, T028 |
| FR-031 | T024, T025 |
| FR-032 | T006, T017, T026, T027, T029 |
| FR-033 | T010, T055, T056, T057, T058 |
| FR-034 | T047-T053, T079 |
| FR-035 | T014, T015, T050, T051, T052 |
| FR-036 | T012, T018, T019, T020, T113 |
| FR-037 | T067, T068 |
| FR-038 | T019, T020, T111, T114 |
| FR-039 | T045, T120, T121, T122 |
| FR-040 | T032, T038, T120, T121 |
| FR-041 | T028, T029 |
| FR-042 | T123, T124, T125 |
| FR-043 | T001, T002, T003, T016-T020, T113, T118 |
| FR-044 | T001, T002, T095, T097, T098, T099 |
| FR-045 | T117 |

| Audit item | Tasks |
|---|---|
| §1.1 C2 RAMRDOFF (`$C002`) | T031, T033, T036, T038 |
| §1.1 C4 `$C004` mis-routes | T031, T033, T036, T038 |
| §1.1 C6 `$C006` mis-routes | T031, T033, T036, T038 (delete) |
| §1.1 C INTCXROM | T031, T035, T036 |
| §1.1 C ALTZP | T031, T035, T036 |
| §1.1 M SLOTC3ROM | T031, T035, T036 |
| §1.2 M `$C011` BSRBANK2 | T034, T063, T065 |
| §1.2 M `$C012` BSRREADRAM | T034, T063, T065 |
| §1.2 M `$C015` RDINTCXROM | T034, T037, T065 |
| §1.2 M `$C016` RDALTZP | T034, T037, T065 |
| §1.2 M `$C017` RDSLOTC3ROM | T034, T037, T065 |
| §1.2 M `$C019` RDVBLBAR | T055, T057, T058 |
| §1.3 display switches | T064 |
| §2 C RAMRD/RAMWRT page table | T031, T032, T036 |
| §2 C ALTZP no state | T031, T036 |
| §2 C aux LC not impl | T043, T044, T077 |
| §2 M `AuxRamCard::Reset` zeroes | T038 (delete), T049, T052 |
| §3 C LC `Reset` zeroes RAM | T042, T044, T049, T052 |
| §3 M pre-write same-address | T041, T044 |
| §3 M write should reset pre-write | T041, T044 |
| §3 M aux LC RAM | T043, T044 |
| §3 M power-on initial state | T042, T044 |
| §4 C Open/Closed Apple unreachable | T060, T061, T062 |
| §4 M Shift not implemented | T060, T062 |
| §6.2 M flash not impl in 80-col | T106, T110 |
| §6.5 M DHR is a stub (issue #61) | T109, T110, T111 |
| §7 C slot 6 ROM shadowed | T035, T084 |
| §7 M no WOZ | T087, T088, T102 |
| §7 M no auto-flush | T091, T092, T094, T104 |
| §8 C slot ROM shadowed | T035, T084 |
| §8 C INTCXROM | T031, T035 |
| §8 M `$C800` INTC8ROM | T035 |
| §8 M SLOTC3ROM | T035 |
| §9 M no IRQ support | T024-T029 |
| §10 C soft reset doesn't reset switches | T047-T049, T052, T053 |
| §10 C soft reset doesn't reset LC state | T042, T049, T052 |
| §10 C power-cycle zeroes LC RAM | T042, T049, T052 |
| §10 M power-cycle zeroes aux RAM | T031, T049, T051, T052 |
| §10 M soft reset doesn't reset CPU regs | T049, T052 |

**Coverage gap callouts**:

- FR-015 (Speaker accuracy): covered by T015a (Speaker coverage audit + gap closure), inserted in Phase 0 to leverage `MockAudioSink` immediately. The audit log will be checked in by T015a's acceptance.

---

## Notes

- `[P]` = different files, no dependencies on incomplete tasks.
- `[Story]` = Phase 7, Phase 11, Phase 8, Phase 13, Phase 14, Phase 15 mapping. Foundational phases (Phases 0-6, Phase 9, Phase 10, Phase 12) and Polish (Phase 16) carry no story label per the template.
- `[GATE]` = phase-completion gate; not parallel; re-asserts pre-existing-tests-still-green and ][/][+ boot continuity per the user's mandatory inclusion.
- Every production-code task is paired with a test task whose acceptance criterion names the tests that must pass — there is NO production-code task whose acceptance criterion is "compiles".
- Every test task honors constitution §II Test Isolation via `HeadlessHost` + `IFixtureProvider` plumbing from Phase 0.
- Every public-function-touching task accounts for constitution Phase 12.4.0: function comments in `.cpp` only, function spacing, top-of-scope locals, no calls in macro args, ≤ 50/100 lines, ≤ 2/3 indent levels.
- Verify tests fail before implementing (TDD). Commit after each task or logical group. Stop at any `[GATE]` to validate independently.
