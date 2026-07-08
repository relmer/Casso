# Tasks: Apple //c Machine Support

**Input**: Design documents from `specs/016-apple2c-support/` (plan.md, spec.md, research.md, data-model.md, quickstart.md)

**Tests**: Included — the constitution mandates unit tests, and the spec defines test-based success criteria.

**Organization**: Grouped by user story (US1–US5 from the spec). **Delivery order re-sequenced** (per project decision): US3's 6551 ACIA *device* is built first because it blocks the in-flight **spec 015** (see Phase 2). US3's //c-specific serial *wiring* still follows the //c bring-up (Phase 5, T024).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency)
- **[Story]**: US1–US5 (maps to the spec's user stories)

---

## Phase 1: Setup

- [X] T001 Verify baseline: full existing `UnitTest` suite green on branch `016-apple2c-support`. *(1959 tests green.)*
- [ ] T002 [P] Acquire assets into `UnitTest/Fixtures/` + the machine-ROM pipeline: //c Memory Expansion ROM (ROM 4, 32K), Dormann 65C02 functional test, Harte `synertek65c02` SingleStepTests vectors. (Needed by US1/US2; the ACIA in Phase 2 needs none.)

---

## Phase 2: User Story 3 (device) — 6551 ACIA 🚧 UNBLOCKS SPEC 015

**Delivery-elevated ahead of US1.** The reusable dual-port 6551 ACIA is the shared component spec 015 (printer) is blocked on. Independent of the CPU and the //c profile.

**Independent Test**: register-level TX → host-file endpoint; loopback RX → correct bytes + status/IRQ flags. No //c or CPU required.

### Tests (write first, expect FAIL)

- [X] T003 [P] [US3] `UnitTest/EmuTests/Acia6551Tests.cpp`: TX (file endpoint), RX (loopback), baud/framing, status + IRQ flags. *(11 tests.)*

### Implementation

- [X] T004 [US3] Implement `Acia6551` (`CassoEmuCore/Devices/Acia6551.{h,cpp}`): data/status/command/control registers, RX/TX, baud/framing, IRQ via `InterruptController`.
- [X] T005 [US3] Add v1 endpoints: TX → host file (printing), loopback (comms); interface consumable by spec 015 (FR-011). *(`IAciaEndpoint`, `AciaLoopbackEndpoint`, `AciaFileEndpoint`.)*
- [X] T006 [US3] Register `acia-6551` in `ComponentRegistry`.
- [X] T007 [US3] Make ACIA tests pass; confirm exactly one ACIA implementation (SC-005). *(1970 tests green; single `Acia6551`.)*

**Checkpoint**: 6551 ACIA device complete + tested standalone → **spec 015 is unblocked.** (Independent of everything below — can proceed in parallel with the CPU track.)

---

## Phase 3: Foundational — CPU-selection seam

**⚠️ Blocks the 65C02 + //c stories** (not the ACIA above).

- [X] T008 Extend `MachineConfig` (`CassoEmuCore/Core/MachineConfig.{h,cpp}`) to accept `cpu` ∈ {`6502`, `65C02`} (currently hard-validated to `6502` at `MachineConfig.cpp:726`); add a CPU factory mapping the string → `ICpu`. *(`CpuFactory`; 65C02 → E_NOTIMPL until Phase 4.)*
- [X] T009 Wire `MachineManager` (`Casso/Shell/MachineManager.cpp:~726`) to build `EmuCpu` with the injected strategy per `config.cpu` (default `6502`; existing machines unchanged).
- [X] T010 [P] Unit tests (`UnitTest/EmuTests/MachineConfigTests.cpp`): `65C02` parses; unknown rejected; default/`6502` preserved.

**Checkpoint**: config-driven CPU selection; II/II+/`//e` still run NMOS.

---

## Phase 4: User Story 1 — 65C02 core 🎯

**Goal**: 65C02 software runs; the existing //e-Enhanced profile (NMOS today) is corrected.

**Independent Test**: //e-Enhanced `cpu: 65C02`; Dormann 65C02 + Harte `synertek65c02` pass 100%; NMOS suite unchanged.

### Tests (expect FAIL)

- [ ] T011 [P] [US1] `UnitTest/EmuTests/Cpu65C02Tests.cpp`: Klaus Dormann 65C02 functional test. *(Base CMOS tier: build/run with Rockwell + WDC opcode options disabled — the Apple 65C02 lacks RMB/SMB/BBR/BBS and WAI/STP; see research.md D7.)*
- [ ] T012 [P] [US1] `Cpu65C02Tests.cpp`: Tom Harte `synertek65c02` SingleStepTests. *(Base tier — not `rockwell65c02`/`wdc65c02`.)*

### Implementation

- [ ] T013 [US1] Implement `Cpu65C02` (`CassoEmuCore/Core/Cpu65C02.{h,cpp}`, `ICpu` sharing `Cpu6502` dispatch): new opcodes — `STZ`, `PHX`/`PLX`/`PHY`/`PLY`, `BRA`, `TSB`/`TRB`, `INC A`/`DEC A`, `RMB`/`SMB`, `BBR`/`BBS`.
- [ ] T014 [US1] Addressing modes (`(zp)`, `(abs,X)` JMP) + corrected behaviors (indirect-`JMP` fix, decimal flags/cycles, RMW cycles) + 65C02 timing.
- [ ] T015 [US1] Register `65C02` → `Cpu65C02` in the CPU factory (T008).
- [ ] T016 [US1] Set `Apple2eEnhanced` config `cpu: 65C02` (`Resources/Machines/Apple2eEnhanced/*.json`) — the fix for the existing defect.
- [ ] T017 [US1] Make Dormann + Harte 65C02 pass; run NMOS + II/II+/`//e` regression (unchanged).

**Checkpoint**: //e-Enhanced runs on the 65C02. SC-001 + SC-004 green. **Shippable standalone.**

---

## Phase 5: User Story 2 — Apple //c boot + firmware map (+ US3 serial wiring)

**Goal**: Select **Apple //c** and cold-boot to monitor/BASIC; peripherals at phantom-slot addresses; the two serial ports (Phase 2 ACIA) wired in.

**Independent Test**: Pick Apple //c → reaches `]`/monitor; probe finds peripherals at $C1xx/$C2xx/$C3xx/$C4xx/$C6xx; serial ports respond; printing to serial port 1 drives the ImageWriter pipeline.

### Tests (expect FAIL)

- [ ] T018 [P] [US2] `UnitTest/EmuTests/Apple2cBootTests.cpp`: //c cold-boots to monitor/Applesoft (ROM signature, 128K).
- [ ] T019 [P] [US2] `Apple2cBootTests.cpp`: built-in peripherals answer at phantom-slot addresses; no user-insertable slots.
- [ ] T019a [P] [US3] `UnitTest/EmuTests/AciaPrinterEndpointTests.cpp`: feed the Print Shop Color reference capture (`printshop-color-testpage.bin`, from spec 015) through `AciaPrinterEndpoint`; assert the bytes land in the shared printer ring / produce the expected `ImageWriterInterpreter` events. Mocked, no real serial (Test Isolation).

### Implementation

- [ ] T020 [US2] Add the //c ROM 4 asset to `AssetBootstrap` (`Casso/AssetBootstrap.cpp`): entry, SHA, "Apple //c" display name, picker entry.
- [ ] T021 [US2] Implement the 32K bank-switched ROM mapping ($C800 expansion-ROM bank switch).
- [ ] T022 [US2] Create `Resources/Machines/Apple2c/Apple2c.json`: `cpu: 65C02`, 128K, //e substrate components, phantom-slot firmware map.
- [ ] T023 [US2] Wire the //c firmware slices into `CxxxRomRouter::SetSlotRom` (slots 1/2/3/4/6); ROM-4 SmartPort + mem-expansion firmware present but peripherals report absent (FR-006a).
- [ ] T024 [US3] Wire the two `Acia6551` instances (Phase 2) into the //c serial ports (slots 1 + 2) in `Apple2c.json` + the //c serial firmware. *(US3's //c-specific integration.)*

**Serial printer front door** — the //c has no parallel card, so its printer rides serial port 1 into spec 015's finished, card-agnostic pipeline (behind `PrinterByteRing`). Full brief: `serial-printer-integration.md`. **Depends on spec 015 merged to `master`** (provides the printer pipeline + reference capture).

- [ ] T024a [US3] Hoist `PrinterByteRing` (+ its `PrinterJob`) out of `PrinterCard` into a machine-level `PrinterSink` owned at the `EmulatorShell`/machine level; `PrinterCard` (//e) and the //c serial endpoint both target the shared ring; `PrinterWorker` drains the sink. Mechanical hoist — **no pipeline logic change**. Regression: the //e parallel Print Shop path is unaffected.
- [ ] T024b [US3] Add `AciaPrinterEndpoint` (`CassoEmuCore/Devices/Printer/` or alongside the other endpoints): an `IAciaEndpoint` whose TX path pushes each byte into the `PrinterSink` ring; RX = "printer ready" stub (confirm against Print Shop's serial driver if it stalls). Make T019a pass.
- [ ] T024c [US3] Bind //c **serial port 1** → `AciaPrinterEndpoint` in `Apple2c.json` (port 2 stays comms/loopback); no parallel-printer slot row on the //c.
- [ ] T025 [US2] Make boot + phantom-slot tests pass; assert //c machine selection persists + restores like any other machine (FR-016).

**Checkpoint**: **Apple //c boots** with working serial ports; serial port 1 prints via the ImageWriter pipeline. SC-002. Full end-to-end Print Shop validation on the //c is covered by T039 (SC-003).

---

## Phase 6: User Story 4 — Mouse + interrupts

**Goal**: Mouse-driven //c software via the built-in mouse firmware + VBL/mouse IRQs.

**Independent Test**: Host X/Y/click → firmware position/button; VBL/mouse IRQ delivered; $C019 status correct.

### Tests (expect FAIL)

- [ ] T026 [P] [US4] `UnitTest/EmuTests/AppleMouseTests.cpp`: X/Y/button, VBL/mouse IRQ, $C019 status; assert IRQs neither starve nor double-fire across acknowledge (edge case).

### Implementation

- [ ] T027 [US4] Implement `AppleMouse` (`CassoEmuCore/Devices/AppleMouse.{h,cpp}`): firmware entry points, X/Y/button, clamp, VBL + mouse IRQ sources.
- [ ] T028 [US4] Add //c interrupt soft switches ($C019 VBL, mouse IRQ enable/status) to the //c soft-switch path.
- [ ] T029 [US4] Register `apple-mouse` in `ComponentRegistry`; add to `Apple2c.json` (slot-4 firmware).
- [ ] T030 [US4] Map the host pointer → guest mouse in the shell input path.
- [ ] T031 [US4] Make mouse tests pass.

**Checkpoint**: mouse-driven //c software (e.g. MousePaint) works.

---

## Phase 7: User Story 5 — IWM disk

**Goal**: //c boots/reads/writes internal + external drives via the IWM, reusing the WOZ engine.

**Independent Test**: WOZ in internal drive → boots via IWM (slot-6 space); external drive → second-drive access.

### Tests (expect FAIL)

- [ ] T032 [P] [US5] `Apple2cBootTests.cpp` (disk): //c boots from internal drive via IWM; external-drive read.

### Implementation

- [ ] T033 [US5] Implement `Iwm` (`CassoEmuCore/Devices/Iwm.{h,cpp}`): motor/phase/Q6-Q7, delegating to the existing Disk II / WOZ nibble engine (compose, do not fork).
- [ ] T034 [US5] Register `iwm` in `ComponentRegistry`; add to `Apple2c.json` (slot-6, internal + external drive).
- [ ] T035 [US5] Make //c disk-boot tests pass; regression: existing Disk II machines unchanged.

**Checkpoint**: //c boots from internal/external drive.

---

## Phase 8: Polish & Cross-Cutting

- [ ] T036 [P] Run full `quickstart.md` validation across all stories.
- [ ] T037 [P] Update `README.md` + `CHANGELOG.md` for the new Apple //c machine.
- [ ] T038 Full suite green across Debug+Release × x64+ARM64; Code Analysis clean.
- [ ] T039 SC-003: three representative titles run end-to-end on the //c (serial/terminal, MousePaint, disk game).

---

## Dependencies & Execution Order

### Critical path

`Setup → [US3 device] 6551 ACIA (unblocks 015)` **∥** `(Foundational → [US1] 65C02) → [US2] //c boot + serial wiring → {[US4] mouse ∥ [US5] disk} → Polish`

- **Phase 2 (ACIA) is independent** — it can run fully in parallel with the CPU track; it is listed first to unblock **spec 015** as early as possible.
- **T024** (US3's //c serial wiring) is the one US3 task that depends on the //c profile (US2), so it lives in Phase 5.

### Phase dependencies
- **Setup** → none.
- **Phase 2 (6551 ACIA)** → Setup only; independent of everything below. **Unblocks spec 015.**
- **Foundational** → Setup; blocks US1 + //c.
- **US1 (65C02)** → Foundational.
- **US2 (//c)** → US1.
- **US4 / US5** → US2 (mutually independent, parallelizable).
- **Polish** → desired stories.

### On the re-sequencing
"Prioritize US3 over US1": US3's **ACIA device** (the piece spec 015 needs) is now Phase 2 — ahead of US1 — and independent. Its **//c-specific serial wiring** (T024) still requires the //c to exist, so it remains in the //c bring-up (Phase 5).

---

## Implementation Strategy

### Unblock spec 015 first
1. Setup → **Phase 2 (6551 ACIA device)** → validate (file + loopback) → **spec 015 unblocked**.
2. Then the //c track: Foundational → 65C02 (US1, also fixes //e-Enhanced) → //c boot + serial wiring (US2) → mouse (US4) / disk (US5).

### Parallel opportunity
The ACIA track (Phase 2) and the CPU track (Foundational → US1) share no files and can be worked concurrently.

---

## Notes
- `[P]` = different files, no dependency. `[Story]` maps a task to US1–US5.
- Write each story's tests first; confirm FAIL before implementing.
- The 6551 (US3) is the single ACIA (SC-005) — spec 015 consumes it; no duplication.
- The IWM (US5) delegates to the existing WOZ engine — do not fork it.
- v1 defers UniDisk 3.5 + memory-expansion peripherals (ROM-4 firmware present but reports absent).
- Commit after each task or logical group; stop at any checkpoint to validate a story independently.
