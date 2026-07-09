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

- [X] T011 [P] [US1] `UnitTest/DormannIntegrationTests.cpp`: Klaus Dormann 65C02 functional test. *(Base CMOS tier, rkwl_wdc_op/wdc_op disabled; assembled in-house — required adding 65C02 support to Casso's assembler — and run to the `$2434` success trap in ~22M instructions.)*
- [X] T012 [P] [US1] `UnitTest/HarteTestRunner.cpp` (`HarteSynertek65C02`): Tom Harte `synertek65c02` SingleStepTests, **256/256** (2.56M vectors) via flat-memory `TestCpu65C02`. *(Base tier; the 33 reserved-NOP opcodes assert Casso's canonical 1-byte model — see research.md D7 / commit notes — rather than Synertek's 2-3-byte quirk, which Apple never shipped.)*

### Implementation

- [X] T013 [US1] Implement `Cpu65C02` (`CassoEmuCore/Core/Cpu65C02.{h,cpp}`, sharing `Cpu6502` dispatch): `STZ`, `PHX`/`PLX`/`PHY`/`PLY`, `BRA`, `TSB`/`TRB`, `INC A`/`DEC A`; `RMB`/`SMB`/`BBR`/`BBS` + `WAI`/`STP` decode as base-tier NOPs.
- [X] T014 [US1] Addressing modes (`(zp)`, `(abs,X)` JMP) + corrected behaviors (indirect-`JMP` page fix, decimal ADC/SBC flags/cycles incl. invalid-BCD borrow, `$CF` NMOS-undocumented reclaim) + 65C02 timing.
- [X] T015 [US1] Register `65C02` → `Cpu65C02` in the CPU factory (T008). *(`CpuFactory.cpp` builds `Cpu65C02`; `FactoryBuilds65C02AndRejectsUnknown`.)*
- [~] T016 [US1] ~~Set `Apple2eEnhanced` config `cpu: 65C02`~~ — **deferred to issue #86**: no `Apple2eEnhanced` machine profile exists yet (only Apple2 / Apple2Plus / Apple2e). The 65C02 core is ready to power it once #86 adds the profile.
- [X] T017 [US1] Dormann + Harte 65C02 pass; full regression **1988/1988** (NMOS + II/II+/`//e` unchanged).

**Checkpoint**: //e-Enhanced runs on the 65C02. SC-001 + SC-004 green. **Shippable standalone.**

---

## Phase 5: User Story 2 — Apple //c boot + firmware map (+ US3 serial wiring)

**Goal**: Select **Apple //c** and cold-boot to monitor/BASIC; peripherals at phantom-slot addresses; the two serial ports (Phase 2 ACIA) wired in.

**Independent Test**: Pick Apple //c → reaches `]`/monitor; probe finds peripherals at $C1xx/$C2xx/$C3xx/$C4xx/$C6xx; serial ports respond; printing to serial port 1 drives the ImageWriter pipeline.

### Tests (expect FAIL)

- [ ] T018 [P] [US2] `UnitTest/EmuTests/Apple2cBootTests.cpp`: //c cold-boots to monitor/Applesoft (ROM signature, 128K).
- [ ] T019 [P] [US2] `Apple2cBootTests.cpp`: built-in peripherals answer at phantom-slot addresses; no user-insertable slots.

### Implementation

- [X] T020 [US2] Add the //c ROM 4 asset to `AssetBootstrap` (`Casso/AssetBootstrap.cpp`): catalog entry, "Apple //c" display name, picker entry. *(AppleWin has no //c ROM — sourced the 32K ROM 4 / 341-0445-B (32768 bytes) from the apple2.org.za mirror; extended `RomSpec` with an optional alternate host/urlPath/label. Size-checked, download-on-demand, not committed.)*
- [X] T021 [US2] Implement the 32K bank-switched ROM mapping. *(`Apple2cRomBank` + `IRomBankSwitch` hook in the //e soft-switch bank: `$C028` toggles the two 16K banks across `$C100-$FFFF` (LC `$D000-$FFFF` + `CxxxRomRouter` `$C100-$CFFF`); `/RESET` -> bank 0. Verified: 65C02 resets to the monitor entry `$FA62` with the ROM correctly mapped through the LC (`BuildsAndResetsToMonitorEntry`). Production `MachineManager` path wired: `CreateMemoryDevices` adds bank 0 as a flat device (normal LC/Cxxx split), then `WireApple2cRomBank` layers the coordinator + `$C028` hook; `EmulatorShell` owns it and tears it down before the LC/MMU. ROM-free banking unit tests pass.)*
- [X] T022 [US2] Create `Resources/Machines/Apple2c/Apple2c.json`: `cpu: 65C02`, 128K, //e substrate. *(JSON + banked systemRom schema (`romBankSize`/`romBankSelect`) + parse tests. Embedded as `IDR_MACHINE_APPLE2C`; `EnsureMachineConfigs` writes it to disk and `MachineScanner::Scan` surfaces "Apple //c" in the picker; selecting it pulls the ROMs via the catalog.)*

> **⚠️ US2 is NOT complete — the //c does not yet cold-boot to BASIC.** An
> earlier boot test was a false positive (it matched a stray `]` in random
> RAM). Bring-up with the real ROM 4 found three blockers:
>
> 1. ✅ **Rockwell bit ops (RMB/SMB/BBR/BBS)** — RESOLVED. Casso's `Cpu65C02`
>    now models the **Rockwell R65C02** (bit ops installed via `InstallBitOps`;
>    WDC WAI/STP `$CB`/`$DB` stay NOPs). Covered by `Cpu65C02Tests`
>    (`RockwellBitOpsExecute` / `WdcWaiStpDecodeAsNop`). With this, the //c reset
>    no longer derails — it runs deep into firmware (to `$C90F`). **Two conformance
>    follow-ups:** (a) Casso's assembler can't yet emit the BBR/BBS zero-page-
>    relative form, so the Dormann integration path runs the common opcode subset;
>    (b) the Harte 65C02 corpus should be regenerated as `rockwell65c02` (the 34
>    `$x7`/`$xF`/`$CB`/`$DB` slots are currently skipped there, covered by unit tests).
> 2. ⏳ **//c `$C800-$CFFF` routing** — the firmware jumps into the `$C800`
>    expansion window, which the //e `CxxxRomRouter` leaves as floating `$FF`
>    until `INTC8ROM` latches. The //c has no slots, so that window must *always*
>    read the internal firmware (a //c router mode). **This is the next blocker.**
> 3. ⏳ **Built-in peripherals** — serial 6551 ACIA (T024) + IWM disk the reset
>    firmware probes. Depth TBD once (2) lands.
- [ ] T023 [US2] Wire the //c firmware slices into `CxxxRomRouter::SetSlotRom` (slots 1/2/3/4/6); ROM-4 SmartPort + mem-expansion firmware present but peripherals report absent (FR-006a).
- [ ] T024 [US3] Wire the two `Acia6551` instances (Phase 2) into the //c serial ports (slots 1 + 2) in `Apple2c.json` + the //c serial firmware, with **loopback/file** endpoints. *(US3's //c-specific integration.)*

> **Deferred → issue #87** (`Apple //c serial printer integration + per-port device selection`). The //c **serial printer** front door (`PrinterSink` ring hoist + `AciaPrinterEndpoint` + port-1 binding) and the Hardware-tab **serial endpoint selector** live there, not here — they need spec 015 on `master` **and** a bootable //c, so they follow this phase. Brief: `serial-printer-integration.md`; fixture: `reference/printshop-color-testpage.bin`.

- [ ] T025 [US2] Make boot + phantom-slot tests pass; assert //c machine selection persists + restores like any other machine (FR-016).

**Checkpoint**: **Apple //c boots** with working serial ports (loopback/file endpoints). SC-002. Serial *printing* + endpoint selection are issue #87 (post-015/016).

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
- [ ] T030 [US4] Map the host pointer → guest mouse in the shell input path. **Mouse mode is non-capturing**: while the host cursor is over the emulator viewport, motion + buttons drive the guest mouse (absolute mapping: host position in the viewport → guest mouse position; host cursor hidden over the viewport); leaving the viewport releases to the host. Contrast with Paddle mode, which captures.
- [ ] T030a [US4] Add `Mouse` to `InputMappingMode` (`GlobalUserPrefs`: `Off`/`Joystick`/`Paddle`/`Mouse` + string mapping), **gated to mouse-capable machines** (//c, //e-with-mouse; hidden on ][ / ][+ / plain //e). Replace the `JoystickToggleButton` cycle-toggle with a **segmented device selector using skeuomorphic icons** (joystick / paddle / mouse); wire it to the existing "Cycle Input Mode" path + persistence.
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
- [ ] T034a [US5] Hardware tab: the //c **external drive** is a **Connected / Not connected** toggle that shows/hides the existing drive-mount widget (image name, Mount…, Eject, write-protect). No new media machinery — "Connected" just reveals the existing drive widget; persist the connected state per machine.
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
