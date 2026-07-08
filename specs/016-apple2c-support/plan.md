# Implementation Plan: Apple //c Machine Support

**Branch**: `016-apple2c-support` | **Date**: 2026-07-08 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `specs/016-apple2c-support/spec.md`

## Summary

Add the Apple //c as a new machine profile on top of Casso's existing //e substrate. The work is sequenced as five independently-shippable slices: a **65C02 CPU core** (P1 — also corrects the existing //e-Enhanced profile, which runs on NMOS today), a **slotless "phantom slot" ROM/firmware map** (P2 — yields a bootable //c), **dual 6551 ACIA serial ports** (P3 — the ACIA is also consumed by the in-flight spec 015), a **built-in mouse with VBL/mouse interrupts** (P4), and **IWM disk integration** reusing the existing WOZ engine (P5). Target ROM: the //c **Memory Expansion ROM (ROM 4)**, 32K bank-switched, scoped to a 5.25"/128K machine for v1.

The design uses only existing extension points — no new architecture is introduced (see Architecture & Approach).

## Technical Context

**Language/Version**: C++ (`stdcpplatest`, MSVC v145+) per constitution.

**Primary Dependencies**: Windows SDK + STL only (constitution baseline). **No new third-party dependencies.** 65C02 conformance vectors (Klaus Dormann 65C02 functional + Tom Harte `synertek65c02` SingleStepTests) are test *data*, consistent with the existing NMOS Dormann/Harte corpus. The //c **serial printer** front door (`AciaPrinterEndpoint` + machine-level printer sink) and the Hardware-tab serial endpoint selector are **deferred to issue #87** — they need spec 015 on `master` plus a bootable //c, so they follow this spec (brief: `serial-printer-integration.md`). This spec keeps the ACIA + both serial ports working via loopback/file endpoints; no third-party dependency either way.

**Storage**: Machine config JSON (`Resources/Machines/Apple2c/Apple2c.json`); //c ROM asset (managed like existing machine ROMs via `AssetBootstrap`). Serial TX → host file; no other new persistence.

**Testing**: Microsoft C++ Unit Test Framework (`UnitTest/`), mocked I/O per constitution — serial endpoints are file/loopback (no real device); machine-level tests via `HeadlessHost` with a pinned `Prng`.

**Target Platform**: Windows 10/11, x64 + ARM64.

**Project Type**: Desktop emulator (CassoEmuCore emulation library + Casso GUI shell + CassoCli).

**Performance Goals**: No regression to the real-time emulation frame budget; the 65C02 core shares the NMOS dispatch structure and stays as efficient.

**Constraints**: Reuse the //e substrate unchanged; zero regressions to existing machines (II / II+ / //e); exactly one 6551 implementation, shared with spec 015.

**Scale/Scope**: 1 new CPU core + 3 new devices (6551, AppleMouse, IWM) + 1 machine profile + 1 ROM asset; ~4 new unit-test suites. Reuses aux RAM/80STORE/MMU/soft-switches/80-col/double-hi-res/Disk II-WOZ.

## Constitution Check

*GATE — evaluated against Casso Constitution v1.7.0. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Code Quality (EHM, formatting, function size, no anon ns) | **PASS** — new device `Create` factories and methods follow the existing EHM/HRESULT + single-exit patterns; file-scope `static constexpr` constants, class-`static` helpers, functions kept < 50 lines. |
| II. Testing Discipline (mocked I/O, coverage) | **PASS** — every new component gets a unit suite; serial endpoints are file/loopback so no real device is touched; machine tests use `HeadlessHost` (pinned seed). |
| III. UX Consistency | **PASS** — the //c is selected from the existing machine picker like any other machine; no CLI surface change. |
| IV. Performance | **PASS** — 65C02 reuses the NMOS dispatch shape; no per-frame regression. |
| V. Simplicity (YAGNI, single responsibility, minimal deps) | **PASS** — reuses existing extension points; v1 defers UniDisk 3.5 + memory expansion; no new third-party deps. |
| Tech Constraints (STL / Windows SDK only) | **PASS** — no new dependencies; 65C02 test vectors are data, consistent with existing Dormann/Harte usage (no allowlist change). |

**Result: PASS** — no violations; Complexity Tracking not required.

## Project Structure

### Documentation (this feature)

```text
specs/016-apple2c-support/
├── spec.md          # feature spec (+ Clarifications)
├── plan.md          # this file
├── research.md      # Phase 0: design decisions + rationale
├── data-model.md    # Phase 1: components / entities
├── quickstart.md    # Phase 1: per-story validation
└── tasks.md         # Phase 2 (/speckit.tasks — NOT produced here)
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Core/
│   ├── EmuCpu.{h,cpp}            # [MODIFY] select CPU strategy per MachineConfig.cpu
│   ├── Cpu65C02.{h,cpp}          # [NEW]    65C02 core (ICpu, parallels Cpu6502)
│   ├── MachineConfig.{h,cpp}     # [MODIFY] accept cpu == "65C02"
│   └── ComponentRegistry.cpp     # [MODIFY] register acia-6551, apple-mouse, iwm
├── Devices/
│   ├── Acia6551.{h,cpp}          # [NEW]    6551 ACIA (shared with spec 015)
│   ├── AppleMouse.{h,cpp}        # [NEW]    //c mouse firmware + IRQ source
│   ├── Iwm.{h,cpp}               # [NEW]    IWM presenting the WOZ engine as slotless drives
│   ├── CxxxRomRouter.{h,cpp}     # [REUSE]  phantom-slot firmware map via SetSlotRom
│   └── Disk/…                    # [REUSE]  Disk II / WOZ nibble engine
Casso/
├── Shell/MachineManager.cpp      # [MODIFY] inject CPU strategy per config.cpu (~line 726)
└── AssetBootstrap.cpp            # [MODIFY] //c ROM asset + hash + "Apple //c" display name + picker
Resources/Machines/Apple2c/
└── Apple2c.json                  # [NEW]    //c profile (cpu, rom, devices, phantom-slot map)
UnitTest/EmuTests/
├── Cpu65C02Tests.cpp             # [NEW]    Dormann 65C02 + Harte synertek65c02
├── Acia6551Tests.cpp             # [NEW]
├── AppleMouseTests.cpp           # [NEW]
└── Apple2cBootTests.cpp          # [NEW]    //c boot + phantom-slot probe
```

**Structure Decision**: Single-project emulator layout (CassoEmuCore library + Casso shell + CassoCli). No new projects — new components live in `CassoEmuCore/Devices` + `Core`, wired through the existing `ComponentRegistry` / `MachineConfig` composition and the shell's machine-build path.

## Architecture & Approach (by user story)

**P1 — 65C02 CPU core**
- Extension points already exist: `EmuCpu(MemoryBus&, unique_ptr<ICpu>)` strategy-injection ctor, and the `MachineConfig.cpu` field (today validated as exactly `"6502"` at `MachineConfig.cpp:726`).
- Implement `Cpu65C02` (an `ICpu`, sharing the `Cpu6502` dispatch structure): 65C02 opcode table, new addressing modes (`(zp)`, `(abs,X)` JMP), corrected behaviors (indirect-JMP page-boundary fix, decimal flag/cycle correctness, RMW cycles), 65C02 timing.
- Extend `MachineConfig` to accept `cpu == "65C02"`; change `MachineManager` (~726, currently `make_unique<EmuCpu>(memoryBus)` — always NMOS) to inject the strategy per `config.cpu` (default `6502`).
- Set `Apple2eEnhanced.json` (and the new //c) `cpu` → `65C02` — this is what corrects the Enhanced //e defect, delivering P1 value before //c exists.

**P2 — //c profile + phantom-slot firmware map**
- `Resources/Machines/Apple2c/Apple2c.json`: `cpu: 65C02`, 128K, the //e substrate components (`apple2e-keyboard`, `apple2e-softswitches`, MMU, `language-card`) + the //c peripherals.
- //c ROM (ROM 4, 32K) added to the `AssetBootstrap` machine table (+ SHA + "Apple //c" display name + picker entry).
- Bank-switched ROM: extend the ROM mapping to page the 32K firmware ($C800 expansion-ROM bank switch).
- `CxxxRomRouter::SetSlotRom` for slots 1 (serial 1), 2 (serial 2), 3 (80-col), 4 (mouse), 6 (disk) from the //c firmware; no user-insertable slots on this machine.

**P3 — Dual 6551 ACIA**
- New `Acia6551` device: data / status / command / control registers, RX+TX, baud/framing, IRQ via `InterruptController`. Registered in `ComponentRegistry`; two instances (ports 1 & 2) in the //c JSON.
- v1 endpoints: TX → host file (printing), loopback for comms. Interface designed so **spec 015 consumes this same device** (FR-011).

**P4 — Mouse + interrupts**
- New `AppleMouse` device: mouse firmware entry points, X/Y/button state, VBL + mouse IRQ sources (per-source `InterruptController` lines). Host pointer → guest mapping.
- //c interrupt soft switches ($C019 VBL read, mouse IRQ enable/status) in the //c soft-switch path.

**P5 — IWM disk**
- New `Iwm` device presenting the existing Disk II / WOZ nibble engine as the //c's built-in internal drive (slot-6 space) + external drive port — **delegates to** the nibble/WOZ engine, does not fork it.

## Phasing

Each user story is a self-contained, per-phase commit (constitution Commit Discipline).

**Delivery order (per project decision — mirrored in spec.md Assumptions §Delivery order and tasks.md Phase 2): the 6551 ACIA *device* (US3) is built first.** It is independent of the CPU and the //c profile (tested standalone via file + loopback) and unblocks the in-flight **spec 015** (printer support), so it leads regardless of its P3 priority label.

The remainder then follows the natural dependency order: P1 (65C02) ships standalone value (fixes Enhanced //e) before any //c-specific work; P2 yields a bootable //c; US3's //c-specific serial *wiring* (which requires the //c to exist) lands during //c bring-up; P4 (mouse) and P5 (disk) layer the remaining peripherals on top. See tasks.md for the exact phase sequence.

## Complexity Tracking

None — no constitution violations; no new projects or third-party dependencies.
