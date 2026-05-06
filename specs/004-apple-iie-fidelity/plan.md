# Implementation Plan: Apple //e Fidelity

**Branch**: `004-apple-iie-fidelity` | **Date**: 2026-05-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification at `specs/004-apple-iie-fidelity/spec.md`
**Authoritative requirements source**: [`docs/iie-audit.md`](../../docs/iie-audit.md)
**Constitution**: `.specify/memory/constitution.md` v1.4.0

## Summary

Deliver a fully correct Apple //e (non-Enhanced) emulation: //e ROM cold-boot,
complete //e MMU + soft-switch surface, auxiliary memory, Language Card with
correct two-odd-read pre-write, full keyboard ($C061-$C063), composed video
(40-col / 80-col / lo-res / hi-res with NTSC artifact / DHR / MIXED+80COL),
nibble-level Disk II controller with WOZ + .dsk/.do/.po support, ROM mapping
honoring INTCXROM / SLOTC3ROM / INTC8ROM, IRQ infrastructure with vectored
dispatch, VBL tied to a video timing model, distinct soft-vs-power reset
semantics, and a true headless test harness — all delivered by **composition
over branching** so that ][/][+ remain unmodified and 65C02, //c, //e Enhanced,
clock card, mouse card, and Mockingboard are trivial future additions.

The CPU is refactored behind a strategy interface. The MMU absorbs and replaces
the buggy `AuxRamCard` ($C003-$C006) soft-switch routing into a coherent
//e-MMU subsystem driven by a single banking-changed callback over the existing
page-table fast path. Tests use a true headless host — no window, no audio
device, no host filesystem outside in-repo `UnitTest/Fixtures/`.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145 (VS 2026)
**Primary Dependencies**: Windows SDK + STL only — no third-party libraries
**Storage**: In-repo binary fixtures under `UnitTest/Fixtures/`; runtime ROMs under `ROMs/`
**Testing**: Microsoft C++ Unit Test Framework (CppUnitTestFramework) in `UnitTest/`
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Desktop GUI application + static libraries + unit-test DLL (existing 5-project solution)
**Performance Goals**: ≤ ~1% of one host core for an idle 1.023 MHz //e (FR-042); 60 Hz steady frame pacing
**Constraints**: Test isolation NON-NEGOTIABLE (constitution §II) — no host I/O outside in-repo fixtures; ][/][+ behavior MUST NOT regress; deterministic outputs across runs
**Scale/Scope**: ~40-50 new/modified C++ files, ~8-12k LOC delta, ~150-250 new test cases, 4 disk-image fixtures, 1 DHR golden framebuffer hash

## Constitution Check

*GATE: re-checked post Phase 1 design (see end of section). v1.4.0 amendments
are explicitly enumerated.*

### Principle I — Code Quality (NON-NEGOTIABLE) — v1.4.0 amendments

| Rule | How this plan complies |
|---|---|
| Formatting Preservation (no blank-line / column-alignment loss) | All edits will be surgical; existing aligned blocks (e.g. `MemoryBus` page-table struct, `AppleSoftSwitchBank` register table) preserved. New files use the project's 5-blank-line / 3-blank-line rules. |
| EHM applies to **any** function with fallible internals (1.4.0 expansion) | New subsystems (`AppleIIeMmu`, `DiskIINibbleEngine`, `WozLoader`, `NibblizationLayer`, `InterruptController`, `HeadlessHost`, `VideoTiming`) declared up front to follow EHM internally even when their public return type is `void` or domain-typed. Helper functions that allocate, parse, or call Win32 follow `HRESULT hr; ... Error: ...; return hr;` and surface results to callers via out-params. |
| No calls inside macro arguments (1.4.0) | All macro callsites — `CHR(...)`, `Assert::*`, `Logger::*`, `IGNORE_RETURN_VALUE` — will store call results into a local first. Reviewed during code review of every PR phase. |
| Single exit via `Error:` label | Every new HRESULT-returning function uses the project pattern. |
| Avoid Nesting (1-2 typical, 3 max indent beyond EHM) (1.4.0) | New device classes are factored aggressively. The MMU dispatch (the deepest natural nest) extracts per-region resolvers into helpers (`ResolveZeroPage`, `ResolveMain02_BF`, `ResolveText04_07`, `ResolveHires20_3F`, `ResolveCxxx`, `ResolveLcD000_FFFF`) so each helper stays at ≤ 2 indent levels. |
| Variable Declarations at Top of Scope (1.4.0) | All new functions declare locals at top of the enclosing scope, column-aligned per project rules. Reviewed in PR. |
| No Unnecessary Scope Blocks (1.4.0) | No `{ … }` blocks introduced for visual grouping. |
| Function Comments in `.cpp` Only (1.4.0) | All new headers carry only declaration comments; doc blocks live in `.cpp` with the 80-`/` delimiters. Header comment audit added to the post-implementation polish phase. |
| Function Spacing — `func()`, `func (a, b)`, `if (...)`, `for (...)` (1.4.0) | New code adheres; review checklist includes a grep for `\b[a-zA-Z_]\w*\(` violations on changed lines. |
| Smart Pointers | `unique_ptr` for owned subsystems registered in `ComponentRegistry`; no `shared_ptr` introduced. |

### Principle V — Function Size & Structure (NON-NEGOTIABLE)

All new functions targeted ≤ 50 lines (100 absolute max), 1-2 indent levels
beyond EHM (3 max). Anticipated worst-case offenders are pre-decomposed in
the data-model:
- `AppleIIeMmu::OnSoftSwitchChanged` → dispatches to per-region rebinders.
- `DiskIINibbleEngine::Tick` → state-machine helper per LSS phase.
- `Apple80ColTextMode::Render` (mixed-mode aware) → row renderer extracted.

### Principle II — Testing Discipline (NON-NEGOTIABLE: Test Isolation)

| Rule | Compliance plan |
|---|---|
| No real filesystem | Disk fixtures and ROM fixtures are bundled in-repo and read once at test setup via an `IFixtureProvider` that resolves paths inside `UnitTest/Fixtures/` only. The `HeadlessHost` injects an `IDiskImageStore` which can be backed by either a real file (production) or an in-memory blob (tests). |
| No registry / network / env | None of the new subsystems use Win32 registry or network. Environment access (e.g. user data dir for ROMs in production) goes through the existing `PathResolver` interface, mocked in tests. |
| No real audio device | The existing speaker path already separates `AppleSpeaker` (logic) from a `IAudioSink`; the headless harness binds it to a `MockAudioSink` that drops samples and counts toggles. |
| No real window / input | New `IHostShell` interface owns window + input. GUI implementation lives in `Casso/`. Tests use `HeadlessHost` (no window, no message pump). |
| Deterministic | Power-on RAM uses a seeded PRNG (`SplitMix64` in EmuCore). Production uses a per-launch random seed (`time(nullptr) ^ pid`-derived) so each cold boot is genuinely random — matching real //e DRAM indeterminacy. The headless test harness pins seed = `0xCA550001` for reproducibility. The seed source is injected via the same constructor — production is NOT a hardcoded fallback, it's an explicit "random seed" code path. |

### Principle III — UX Consistency

No new CLI surface. Machine selection remains via existing `Machines/*.json` +
GUI menu. `apple2e.json` will be updated to declare the new MMU + IRQ
controller; `apple2.json` and `apple2plus.json` remain unchanged.

### Principle IV — Performance

Budget owned by FR-042 / SC-007. Plan includes `EmuTests/PerformanceTests.cpp`
that measures wall-clock cost of N emulated cycles in a tight loop and asserts
against an upper bound. CI integration: the perf test runs in Release on the
standard `Build + Test Release` task; failure on it fails CI. See
`research.md` §Perf for the budget derivation.

### Gates

- **Initial gate (pre Phase 0)**: ✅ PASS. All v1.4.0 rules accounted for; no
  exceptions requested.
- **Post Phase 1 gate**: ✅ PASS (see end of doc).

No entries in **Complexity Tracking** — nothing waived.

## Project Structure

### Documentation (this feature)

```text
specs/004-apple-iie-fidelity/
├── plan.md              # This file
├── spec.md              # Feature spec (already clarified, Session 2026-05-05)
├── research.md          # Phase 0 — decisions and rationale
├── data-model.md        # Phase 1 — entities, state machines, relationships
├── quickstart.md        # Phase 1 — build & run the //e + harness
├── contracts/           # Phase 1 — internal module contracts (C++ interfaces)
│   ├── ICpu.md
│   ├── I6502DebugInfo.md
│   ├── IInterruptController.md
│   ├── IMmu.md
│   ├── ISoftSwitchBank.md
│   ├── IDiskController.md
│   ├── IDiskImage.md
│   ├── IVideoMode.md
│   ├── IVideoTiming.md
│   ├── IHostShell.md
│   └── IFixtureProvider.md
├── checklists/          # (pre-existing)
└── tasks.md             # Phase 2 — emitted by /speckit.tasks (NOT here)
```

### Source Code (repository root)

The existing 5-project solution is retained; **no new projects**. Files added
under existing projects, grouped by subsystem.

```text
CassoCore/                                 # 6502 CPU + assembler (existing)
├── Cpu.h / Cpu.cpp                        # MODIFIED: extract ICpu interface; expose interrupt lines
├── ICpu.h                                 # NEW: CPU-family-agnostic pluggable-CPU contract
├── I6502DebugInfo.h                       # NEW: 6502-family register inspection (debugger/tests)
├── Cpu6502.h / Cpu6502.cpp                # NEW: existing Cpu logic re-homed; impl ICpu + I6502DebugInfo
└── (no other CPU variants — 65C02, 65C816, Z80 all OUT of scope)

CassoEmuCore/                              # Apple // platform (existing)
├── Core/
│   ├── EmuCpu.h/.cpp                      # MODIFIED: holds unique_ptr<ICpu>; fan-out IRQ
│   ├── MemoryBus.h/.cpp                   # MODIFIED: page-table extended for //e MMU regions
│   ├── ComponentRegistry.h/.cpp           # MODIFIED: register new MMU + IRQ + HostShell
│   ├── MachineConfig.h/.cpp               # MODIFIED: parse new device kinds
│   ├── InterruptController.h/.cpp         # NEW: aggregates IRQ asserters; drives ICpu IRQ
│   ├── Prng.h/.cpp                        # NEW: SplitMix64 — deterministic RAM seeding (test pins seed; production uses time^pid)
│   └── (PathResolver, Json* unchanged)
├── Devices/
│   ├── AppleSoftSwitchBank.h/.cpp         # UNCHANGED externally; layered as base
│   ├── AppleIIeSoftSwitchBank.h/.cpp      # MODIFIED: take ownership of //e write switches
│   │                                      #   $C000-$C00F and status reads $C011-$C01F per audit
│   ├── AppleIIeMmu.h/.cpp                 # NEW: //e MMU subsystem — owns RAMRD/RAMWRT/ALTZP/
│   │                                      #   80STORE/INTCXROM/SLOTC3ROM/INTC8ROM and rebinds
│   │                                      #   the MemoryBus page table on every change
│   ├── AuxRamCard.h/.cpp                  # DELETED (folded into AppleIIeMmu — see research §MMU)
│   ├── AppleKeyboard.*                    # UNCHANGED (][ base)
│   ├── AppleIIeKeyboard.h/.cpp            # MODIFIED: extend GetEnd() to cover $C063 (Shift)
│   │                                      #   $C061/$C062 already in range per audit
│   ├── AppleSpeaker.*                     # UNCHANGED logic; speaker contract verified by tests
│   ├── LanguageCard.h/.cpp                # MODIFIED: rewrite pre-write state machine
│   │                                      #   (any-two-odd reads), aux-bank routing via ALTZP,
│   │                                      #   power-on default MF_BANK2|MF_WRITERAM, soft-reset
│   │                                      #   preserves contents
│   ├── RamDevice.*                        # UNCHANGED structurally; constructor seeds via Prng
│   ├── RomDevice.*                        # UNCHANGED
│   ├── DiskIIController.h/.cpp            # MAJOR REWRITE: nibble-level LSS + Q6/Q7 latches +
│   │                                      #   motor + head step + write-protect; per-slot soft
│   │                                      #   switches $C0E0-$C0EF (slot 6 = $C0E0-$C0EF)
│   ├── DiskIINibbleEngine.h/.cpp          # NEW: bit-stream engine driving the head
│   ├── Disk/
│   │   ├── IDiskImage.h                   # NEW: in-memory nibble track buffer interface
│   │   ├── WozLoader.h/.cpp               # NEW: WOZ v1/v2 native loader
│   │   ├── NibblizationLayer.h/.cpp       # NEW: .dsk/.do/.po → nibble + inverse for save-back
│   │   ├── DiskImage.h/.cpp               # NEW: concrete IDiskImage holding nibble tracks
│   │   └── DiskImageStore.h/.cpp          # NEW: load/flush coordinator (auto-flush on eject)
├── Video/
│   ├── AppleTextMode.*                    # UNCHANGED (40-col path)
│   ├── Apple80ColTextMode.h/.cpp          # MODIFIED: corrected aux/main interleave +
│   │                                      #   ALTCHARSET + flash semantics; expose row renderer
│   │                                      #   used in MIXED+80COL bottom-4-rows path
│   ├── AppleHiResMode.*                   # MODIFIED: NTSC artifact color rendering
│   ├── AppleDoubleHiResMode.h/.cpp        # MODIFIED: correct aux/main DHR interleaving
│   ├── AppleLoResMode.*                   # UNCHANGED
│   ├── VideoTiming.h/.cpp                 # NEW: cycle-accurate-enough timing model;
│   │                                      #   exposes IsInVblank() consumed by $C019 reader
│   ├── VideoOutput.h                      # MODIFIED: composed mode dispatcher (FR-020)
│   └── (CharacterRom* unchanged)
└── Audio/
    └── AudioGenerator.*                   # UNCHANGED; sink is injected (mock in tests)

Casso/                                     # Win32 GUI app (existing)
├── HostShell.h/.cpp                       # MODIFIED: implements IHostShell over Win32
└── (no other meaningful changes)

UnitTest/                                  # CppUnitTestFramework DLL (existing)
├── Fixtures/                              # NEW DIRECTORY (in-repo binary fixtures)
│   ├── README.md                          # provenance + license per fixture
│   ├── apple2e.rom                        # already in repo under ROMs/; symlinked or copied
│   ├── apple2e-video.rom                  # ditto
│   ├── dos33.dsk                          # public-domain DOS 3.3 system disk
│   ├── prodos.po                          # public-domain ProDOS boot disk
│   ├── sample.woz                         # WOZ test image (project-original or PD)
│   ├── copyprotected.woz                  # public-domain copy-protected sample
│   └── golden/
│       ├── dhr_testpattern.hash           # SHA-256 of expected DHR framebuffer
│       └── 80col_mixed.hash               # SHA-256 of expected mixed-mode framebuffer
└── EmuTests/
    ├── HeadlessHost.h/.cpp                # NEW: test-side IHostShell + IFixtureProvider
    ├── MockAudioSink.h/.cpp               # NEW
    ├── MockIrqAsserter.h/.cpp             # NEW: drives InterruptController in unit tests
    ├── MmuTests.cpp                       # NEW
    ├── SoftSwitchTests.cpp                # MODIFIED: full $C011-$C01F coverage incl. floating bus
    ├── LanguageCardTests.cpp              # MODIFIED: pre-write state machine; aux LC; reset
    ├── KeyboardTests.cpp                  # MODIFIED: $C061-$C063; strobe-clear isolation
    ├── VideoModeTests.cpp                 # MODIFIED: 80-col interleave; ALTCHARSET; DHR; MIXED+80COL
    ├── VideoRenderTests.cpp               # MODIFIED: NTSC artifact; framebuffer hashes
    ├── VideoTimingTests.cpp               # NEW: VBL transitions
    ├── DiskIITests.cpp                    # MAJOR REWRITE: nibble-level controller
    ├── WozLoaderTests.cpp                 # NEW
    ├── NibblizationTests.cpp              # NEW
    ├── DiskImageStoreTests.cpp            # NEW: auto-flush semantics
    ├── InterruptControllerTests.cpp       # NEW
    ├── ResetSemanticsTests.cpp            # NEW: soft vs power
    ├── EmuIntegrationTests.cpp            # MODIFIED: cold-boot //e to BASIC prompt; PR#3;
    │                                      #   DOS 3.3 boot; ProDOS boot; WOZ boot; CP boot
    ├── BackwardsCompatTests.cpp           # NEW: re-asserts pre-existing ][/][+ scenarios
    └── PerformanceTests.cpp               # NEW: FR-042 budget assertion
```

**Structure Decision**: Retain the existing 5-project layout (CassoCore,
CassoEmuCore, Casso, CassoCli, UnitTest). All new functionality lives inside
those projects, organized into the existing `Core/` `Devices/` `Video/`
`Audio/` subfolders — plus one new `Devices/Disk/` subfolder for the disk
stack and one new `UnitTest/Fixtures/` for in-repo binary test data. No
project additions; no architectural restructuring at the solution level.

## Phase 0 — Outline & Research

See [`research.md`](./research.md). Summary of decisions:

1. **MMU consolidation**: replace `AuxRamCard` with `AppleIIeMmu`. The MMU
   owns RAMRD / RAMWRT / ALTZP / 80STORE / INTCXROM / SLOTC3ROM / INTC8ROM.
   Soft-switch reads of these flags ($C013-$C018) come from the MMU via the
   //e soft-switch bank's read-status path. AuxRamCard's $C003-$C006 bug
   (audit C6) is fixed by deletion + correct re-homing in $C000-$C00F.
2. **Modifier keys**: keep them in `AppleIIeKeyboard` and extend its
   `GetEnd()` to $C063, rather than re-homing to the soft-switch bank.
   Rationale: keyboard contract stays cohesive; fewer cross-device couplings.
3. **Floating bus**: source low-7 from the keyboard latch (current
   precedent, matches AppleWin's most-common case). Documented in research.
4. **Disk II**: full nibble-level rewrite folded with WOZ + .dsk/.do/.po +
   auto-flush. Closes issue #61. No sector-level shortcut path retained.
5. **CPU strategy**: define `ICpu` as a CPU-family-agnostic interface and `I6502DebugInfo` as the 6502-family debug interface; move existing `Cpu` into `Cpu6502` implementing both. `EmuCpu` owns `unique_ptr<ICpu>`. Zero behavior change for ][/][+; sets up 65C02 swap (and trivially other CPU families).
6. **IRQ infra**: `InterruptController` aggregates assertions; `ICpu` exposes `SetInterruptLine(CpuInterruptKind, bool)` for both `kMaskable` (level-sensitive) and `kNonMaskable` (edge-detected). Vectored dispatch via `$FFFE` (IRQ) and `$FFFA` (NMI) in `Cpu6502`.
7. **Reset semantics**: separate `SoftReset()` and `PowerCycle()` paths in
   the machine. Memory init via seeded `Prng`.
8. **Headless harness**: `IHostShell` interface; `HeadlessHost` test impl;
   no window, no audio, no host I/O outside `UnitTest/Fixtures/`.
9. **Performance**: ~1% of one core ≈ ~10 MHz of pure emulation throughput
   on a modern host; current code is well clear of this. Asserted by
   `PerformanceTests.cpp` against a generous 5 % ceiling locally and a
   stricter informational track in CI.
10. **Fixtures provenance**: only public-domain or project-original artifacts.
    Provenance documented in `UnitTest/Fixtures/README.md`.

## Phase 1 — Design & Contracts

See:
- [`data-model.md`](./data-model.md) — entities, state machines, relationships.
- [`contracts/`](./contracts/) — internal module contracts (C++ interface docs).
- [`quickstart.md`](./quickstart.md) — build, run, test, headless invocation.

Agent context updated: `.github/copilot-instructions.md` SPECKIT block now
references this plan.

## Implementation Phasing (recommended; consumed by /speckit.tasks)

> Foundational first; user stories layered. **Each phase commits independently
> per constitution Commit Discipline.**

### Phase F0 — Setup
- Create `UnitTest/Fixtures/` and the `IFixtureProvider` plumbing.
- Add `Prng` (SplitMix64). Add `MockAudioSink`, `MockIrqAsserter`, `HeadlessHost`.
- Add `ICpu`, `I6502DebugInfo`, `IInterruptController`, `IHostShell`, `IMmu`, `IDiskImage`,
  `IVideoTiming` headers (declarations only).

### Phase F1 — CPU strategy + IRQ infrastructure (FR-030, FR-031, FR-032)
- Extract `Cpu6502` from `Cpu` behind `ICpu`. Wire IRQ line + $FFFE dispatch.
- Implement `InterruptController`. Tests: `InterruptControllerTests`.
- Gate: existing `CpuOperationTests`, `Dormann`, `Harte` all still green.

### Phase F2 — //e MMU consolidation (FR-001..FR-007, FR-026..FR-029)
- Build `AppleIIeMmu`. Move soft-switch ownership from `AuxRamCard` and from
  `AppleIIeSoftSwitchBank`'s incorrect mapping into the MMU. Delete
  `AuxRamCard`. Wire status reads $C013-$C018 + RD80STORE.
- Implement INTCXROM, SLOTC3ROM, INTC8ROM ROM-mapper rules — unblocks slot 6.
- Tests: `MmuTests`, expanded `SoftSwitchTests`. ][/][+ tests must stay green.

### Phase F3 — Language Card rewrite (FR-008..FR-012)
- Rewrite pre-write state machine; ALTZP-controlled aux LC bank routing;
  power-on default; reset preservation. Tests: `LanguageCardTests` rewrite.

### Phase F4 — Reset semantics + RAM seeding (FR-034, FR-035)
- Split `SoftReset()` / `PowerCycle()` paths. Seed RAM via `Prng` on power.
- Tests: `ResetSemanticsTests`.

### Phase F5 — Video timing + VBL (FR-033, FR-020)
- Add `VideoTiming` driving `IsInVblank()`. Wire $C019 status read through it.
- Tests: `VideoTimingTests`.

### Phase F6 — Keyboard completion (FR-013, FR-014)
- Extend `AppleIIeKeyboard::GetEnd()` to $C063; ensure $C011-$C01F do NOT
  clear the strobe. Tests: `KeyboardTests` updates.

### Then layer User Stories:

### US1 (P1) — Cold boot to BASIC prompt + PR#3
Depends on F0..F6. Adds: integration test scenarios in `EmuIntegrationTests`.

### US3 (P1) — //e-specific memory & LC programs
Depends on F2, F3, F4. Adds: targeted memory + LC scenarios.

### Phase D1 — Disk II nibble engine (FR-021, FR-022, FR-024)
Depends on F2 (so slot 6 is reachable). Adds: `DiskIINibbleEngine`,
nibble-level controller rewrite, `WozLoader`. Tests: `DiskIITests` rewrite,
`WozLoaderTests`.

### Phase D2 — Nibblization layer + auto-flush (FR-023, FR-025)
Depends on D1. Adds: `NibblizationLayer`, `DiskImageStore`. Tests:
`NibblizationTests`, `DiskImageStoreTests`.

### US2 (P1) — Disk-based real software incl. CP
Depends on D1+D2. Adds: DOS 3.3 / ProDOS / WOZ / CP integration scenarios.

### Phase V1 — Video correctness (FR-016, FR-017, FR-017a, FR-018, FR-019)
Adds: 80-col interleave fix, ALTCHARSET, mixed-mode 80-col routing through
the same composed renderer, NTSC artifact, DHR interleave fix. Tests:
`VideoModeTests`, `VideoRenderTests` golden-hash assertions.

### US4 (P1) — Headless harness validation suite
Depends on F0..F6, V1, D1+D2. Adds: complete FR-045 scenario set in
`EmuIntegrationTests`.

### US5 (P1) — Backwards compat
Continuous gate from F0 onward. Adds: explicit `BackwardsCompatTests`
re-asserting pre-feature ][/][+ behaviors so regressions are caught precisely.

### US6 (P2) — Performance
Adds: `PerformanceTests` budget assertion (FR-042 / SC-007).

### Phase P1 — Polish
- Header-comment audit (constitution §I 1.4.0).
- Macro-argument grep audit on changed lines.
- Function-spacing grep audit on changed lines.
- Update `CHANGELOG.md`, `README.md` (test counts / roadmap).
- Run Dormann + Harte before final commit.

## Performance measurement strategy

`UnitTest/EmuTests/PerformanceTests.cpp` runs in **Release** only. It executes
1,000,000 emulated cycles on a //e at the BASIC idle loop and asserts the
wall-clock cost is below a threshold computed for ~10 % of one host core
(generous local margin) — the spec's ~1 % is the production target with
throttling; the unthrottled measurement just needs to demonstrate ≥ 10x
headroom over real //e speed. CI integration: included in the standard
`Build + Test Release` task; failure fails CI. The threshold is centralized
in a single named constant for tuning.

## Test fixtures (provenance + license posture)

All under `UnitTest/Fixtures/`, documented in `Fixtures/README.md`:

| Fixture | Source | License |
|---|---|---|
| `apple2e.rom`, `apple2e-video.rom` | Already in `ROMs/`. Used as-is by tests via `IFixtureProvider`. | Apple-distributable ROM image used in many open-source emulators; status documented in repo README. |
| `dos33.dsk` | DOS 3.3 system master, public-domain release | Public domain |
| `prodos.po` | ProDOS boot disk, public-domain release | Public domain |
| `sample.woz` | Project-original WOZ image generated by repo tooling | Original — MIT (project) |
| `copyprotected.woz` | Public-domain CP sample (e.g. demo with custom track sync) | Public domain |
| `golden/*.hash` | Project-generated SHA-256 framebuffer hashes | Original — MIT (project) |

Fixtures are committed as binary blobs in-repo. They are read **only** through
`IFixtureProvider`, which constrains all paths to within `UnitTest/Fixtures/`.

## Constitution Re-Check (post Phase 1) — ✅ PASS

Re-verified against the v1.4.0 amendments after data-model and contracts were
written. No new violations introduced. No entries required in Complexity
Tracking. The architecture preserves Principle V (Simplicity) by removing one
device (`AuxRamCard`) for every two added (`AppleIIeMmu`, `InterruptController`)
— net device count is reduced after counting the `Disk/*` regrouping.

## Complexity Tracking

> *Empty — Constitution Check PASSES with no waivers.*

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| *(none)*  | *(n/a)*    | *(n/a)*                              |
