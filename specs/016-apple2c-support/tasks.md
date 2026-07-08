# Tasks: Apple //c Machine Support

**Input**: Design documents from `specs/016-apple2c-support/` (plan.md, spec.md, research.md, data-model.md, quickstart.md)

**Tests**: Included — the constitution mandates unit tests, and the spec defines test-based success criteria (SC-001 CPU conformance, per-story suites).

**Organization**: Grouped by user story (US1–US5) so each ships independently.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency)
- **[Story]**: US1–US5 (maps to the spec's P1–P5)
- Exact paths included.

---

## Phase 1: Setup

- [ ] T001 Verify baseline: full existing `UnitTest` suite green on branch `016-apple2c-support` (guards against pre-existing breakage before //c work).
- [ ] T002 [P] Acquire assets into `UnitTest/Fixtures/` and the machine-ROM pipeline: the //c Memory Expansion ROM (ROM 4, 32K), Klaus Dormann's 65C02 functional test, and Tom Harte's `wdc65c02` SingleStepTests vectors.

---

## Phase 2: Foundational (Blocking Prerequisites)

**⚠️ Blocks all user stories** — the config-driven CPU-selection seam every //c machine relies on.

- [ ] T003 Extend `MachineConfig` (`CassoEmuCore/Core/MachineConfig.{h,cpp}`) to accept `cpu` ∈ {`6502`, `65C02`} (currently hard-validated to `6502` at `MachineConfig.cpp:726`); add a CPU factory mapping the string → `ICpu`.
- [ ] T004 Wire `MachineManager` (`Casso/Shell/MachineManager.cpp:~726`, today `make_unique<EmuCpu>(memoryBus)`) to build `EmuCpu` with the injected strategy per `config.cpu` (default `6502`; existing machines unchanged).
- [ ] T005 [P] Unit tests (`UnitTest/EmuTests/MachineConfigTests.cpp`): `65C02` parses; unknown CPU rejected; default/`6502` path preserved.

**Checkpoint**: config-driven CPU selection exists; II/II+/`//e` still run NMOS (no behavior change).

---

## Phase 3: User Story 1 — 65C02 core (Priority: P1) 🎯 MVP

**Goal**: Software requiring the 65C02 runs correctly — and the existing //e-Enhanced profile (NMOS today) is corrected.

**Independent Test**: Set //e-Enhanced `cpu: 65C02`; Dormann 65C02 + Harte `wdc65c02` pass 100%; NMOS suite unchanged.

### Tests (write first, expect FAIL)

- [ ] T006 [P] [US1] `UnitTest/EmuTests/Cpu65C02Tests.cpp`: wire Klaus Dormann 65C02 functional test.
- [ ] T007 [P] [US1] `Cpu65C02Tests.cpp`: wire Tom Harte `wdc65c02` SingleStepTests (per-opcode cycle accuracy).

### Implementation

- [ ] T008 [US1] Implement `Cpu65C02` (`CassoEmuCore/Core/Cpu65C02.{h,cpp}`, `ICpu` sharing the `Cpu6502` dispatch): new opcodes — `STZ`, `PHX`/`PLX`/`PHY`/`PLY`, `BRA`, `TSB`/`TRB`, `INC A`/`DEC A`, `RMB`/`SMB`, `BBR`/`BBS`.
- [ ] T009 [US1] Add 65C02 addressing modes (`(zp)`, `(abs,X)` for `JMP`) + corrected behaviors (fixed indirect-`JMP` page-boundary bug, decimal flag/cycle correctness, RMW cycle counts) + 65C02 timing.
- [ ] T010 [US1] Register `65C02` → `Cpu65C02` in the CPU factory (T003).
- [ ] T011 [US1] Set `Apple2eEnhanced` config `cpu: 65C02` (`Resources/Machines/Apple2eEnhanced/*.json`) — the fix for the existing defect.
- [ ] T012 [US1] Make Dormann + Harte 65C02 pass; run the NMOS + II/II+/`//e` regression (unchanged).

**Checkpoint**: //e-Enhanced runs on the 65C02. SC-001 + SC-004 green. **Shippable standalone.**

---

## Phase 4: User Story 2 — //c profile + firmware map (Priority: P2)

**Goal**: Select **Apple //c** and cold-boot to its monitor/BASIC, peripherals at fixed phantom-slot addresses.

**Independent Test**: Pick Apple //c → reaches `]`/monitor; probe finds peripherals at $C1xx/$C2xx/$C3xx/$C4xx/$C6xx.

### Tests (expect FAIL)

- [ ] T013 [P] [US2] `UnitTest/EmuTests/Apple2cBootTests.cpp`: //c cold-boots to monitor/Applesoft (ROM signature, 128K).
- [ ] T014 [P] [US2] `Apple2cBootTests.cpp`: built-in peripherals answer at phantom-slot addresses; no user-insertable slots.

### Implementation

- [ ] T015 [US2] Add the //c ROM 4 asset to `AssetBootstrap` (`Casso/AssetBootstrap.cpp`): entry, SHA, "Apple //c" display name, picker entry.
- [ ] T016 [US2] Implement the 32K bank-switched ROM mapping ($C800 expansion-ROM bank switch) in the ROM/router path.
- [ ] T017 [US2] Create `Resources/Machines/Apple2c/Apple2c.json`: `cpu: 65C02`, 128K, //e substrate components (`apple2e-keyboard`, `apple2e-softswitches`, MMU, `language-card`), phantom-slot firmware map.
- [ ] T018 [US2] Wire the //c firmware slices into `CxxxRomRouter::SetSlotRom` (slots 1/2/3/4/6); ROM-4 SmartPort + mem-expansion firmware present but peripherals report absent (FR-006a).
- [ ] T019 [US2] Make boot + phantom-slot tests pass.

**Checkpoint**: **Apple //c boots.** SC-002.

---

## Phase 5: User Story 3 — Dual 6551 ACIA serial (Priority: P3)

**Goal**: The //c's two serial ports transmit/receive through the 6551 + firmware.

**Independent Test**: Configure + TX → file endpoint; loopback RX → correct bytes + status/IRQ flags.

### Tests (expect FAIL)

- [ ] T020 [P] [US3] `UnitTest/EmuTests/Acia6551Tests.cpp`: register-level TX (file endpoint), RX (loopback), baud/framing, status + IRQ flags.

### Implementation

- [ ] T021 [US3] Implement `Acia6551` (`CassoEmuCore/Devices/Acia6551.{h,cpp}`): data/status/command/control registers, RX/TX, baud/framing, IRQ via `InterruptController`.
- [ ] T022 [US3] Add v1 endpoints: TX → host file (printing), loopback (comms); interface consumable by spec 015 (FR-011).
- [ ] T023 [US3] Register `acia-6551` in `ComponentRegistry`; add two instances (ports 1+2) to `Apple2c.json`.
- [ ] T024 [US3] Wire the //c serial firmware (slots 1/2) to the ACIAs.
- [ ] T025 [US3] Make ACIA tests pass; confirm exactly one ACIA impl (SC-005).

**Checkpoint**: //c serial works; the shared ACIA is ready for spec 015.

---

## Phase 6: User Story 4 — Mouse + interrupts (Priority: P4)

**Goal**: Mouse-driven //c software works via the built-in mouse firmware + VBL/mouse IRQs.

**Independent Test**: Host X/Y/click → firmware position/button; VBL/mouse IRQ delivered; $C019 status correct.

### Tests (expect FAIL)

- [ ] T026 [P] [US4] `UnitTest/EmuTests/AppleMouseTests.cpp`: X/Y/button reporting, VBL/mouse IRQ delivery, $C019 status.

### Implementation

- [ ] T027 [US4] Implement `AppleMouse` (`CassoEmuCore/Devices/AppleMouse.{h,cpp}`): firmware entry points, X/Y/button, clamp, VBL+mouse IRQ sources.
- [ ] T028 [US4] Add //c interrupt soft switches ($C019 VBL, mouse IRQ enable/status) to the //c soft-switch path.
- [ ] T029 [US4] Register `apple-mouse` in `ComponentRegistry`; add to `Apple2c.json` (slot-4 firmware).
- [ ] T030 [US4] Map the host pointer → guest mouse in the shell input path.
- [ ] T031 [US4] Make mouse tests pass.

**Checkpoint**: mouse-driven //c software (e.g. MousePaint) works.

---

## Phase 7: User Story 5 — IWM disk (Priority: P5)

**Goal**: //c boots/reads/writes its internal + external drives via the IWM, reusing the WOZ engine.

**Independent Test**: WOZ in internal drive → boots via IWM (slot-6 space); external drive → second-drive access.

### Tests (expect FAIL)

- [ ] T032 [P] [US5] `UnitTest/EmuTests/Apple2cBootTests.cpp` (disk): //c boots from internal drive via IWM; external-drive read.

### Implementation

- [ ] T033 [US5] Implement `Iwm` (`CassoEmuCore/Devices/Iwm.{h,cpp}`): motor/phase/Q6-Q7 state, delegating to the existing Disk II / WOZ nibble engine (compose, do not fork).
- [ ] T034 [US5] Register `iwm` in `ComponentRegistry`; add to `Apple2c.json` (slot-6, internal + external drive).
- [ ] T035 [US5] Make //c disk-boot tests pass; regression: existing Disk II machines unchanged.

**Checkpoint**: //c boots from internal/external drive.

---

## Phase 8: Polish & Cross-Cutting

- [ ] T036 [P] Run full `quickstart.md` validation across all stories.
- [ ] T037 [P] Update `README.md` + `CHANGELOG.md` for the new Apple //c machine (user-facing).
- [ ] T038 Full suite green across Debug+Release × x64+ARM64; Code Analysis clean (constitution Quality Gates).
- [ ] T039 SC-003: three representative titles run end-to-end on the //c — one serial/terminal (or serial-print), MousePaint, and a disk game.

---

## Dependencies & Execution Order

### Phase dependencies
- **Setup (P1)** → no deps.
- **Foundational (P2)** → after Setup; **blocks all stories** (CPU-selection seam).
- **US1 (P3)** → after Foundational. Ships standalone (fixes //e-Enhanced).
- **US2 (P4)** → after **US1** (a //c needs the 65C02).
- **US3 / US4 / US5** → after **US2** (need the //c profile); mutually independent, can parallelize.
- **Polish (P8)** → after the desired stories.

### Critical path
`Setup → Foundational → US1 → US2 → {US3 ∥ US4 ∥ US5} → Polish`

### Parallel opportunities
- T002 [P] alongside Setup; T005 [P] within Foundational.
- Test tasks (T006/T007, T013/T014, T020, T026, T032) [P] before their impl.
- **US3, US4, US5 run in parallel** once US2 lands (distinct devices + files).

---

## Implementation Strategy

### MVP first
1. Setup → Foundational → **US1 (65C02)**.
2. **STOP + VALIDATE**: Dormann/Harte 65C02 green; //e-Enhanced fixed; NMOS unchanged.
3. Ship — value delivered before any //c-specific work.

### Incremental delivery
US1 (65C02 + Enhanced //e fix) → US2 (bootable //c) → US3/US4/US5 (serial / mouse / disk) → Polish. Each is an independently testable, independently shippable increment; commit per phase (constitution Commit Discipline).

---

## Notes
- `[P]` = different files, no dependency. `[Story]` maps a task to US1–US5 for traceability.
- Write each story's tests first and confirm they FAIL before implementing.
- The 6551 (US3) is built to spec 015's needs too — single ACIA (SC-005), no duplication.
- The IWM (US5) delegates to the existing WOZ engine — do not fork it.
- v1 defers UniDisk 3.5 + memory-expansion peripherals (their ROM-4 firmware is present but reports absent).
- Commit after each task or logical group; stop at any checkpoint to validate a story independently.
