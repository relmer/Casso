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
- [X] T002 [P] Acquire assets into `UnitTest/Fixtures/` + the machine-ROM pipeline: //c Memory Expansion ROM (ROM 4, 32K), Dormann 65C02 functional test, Harte `synertek65c02` SingleStepTests vectors. (Needed by US1/US2; the ACIA in Phase 2 needs none.) *(All acquired: `UnitTest/Fixtures/Apple2c.rom` (341-0445-B, 32768 B, download-on-demand, uncommitted); Dormann 65C02 assembled in-house (T011); Harte `synertek65c02` vectors wired (T012).)*

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

- [X] T018 [P] [US2] `UnitTest/EmuTests/Apple2cBootTests.cpp`: //c cold-boots to monitor/Applesoft (ROM signature, 128K). *(`ColdBootsToCheckDiskDrive` scrapes the "Apple //c" banner + "Check Disk Drive." no-disk state; `BuildsAndResetsToMonitorEntry` asserts the RESET vector `$FA62`/CLD + 128K wiring. The //c has no BASIC-on-cold-boot, so "Check Disk Drive." is the correct terminal no-disk screen rather than `]`/monitor.)*
- [ ] T019 [P] [US2] `Apple2cBootTests.cpp`: built-in peripherals answer at phantom-slot addresses; no user-insertable slots.

### Implementation

- [X] T020 [US2] Add the //c ROM 4 asset to `AssetBootstrap` (`Casso/AssetBootstrap.cpp`): catalog entry, "Apple //c" display name, picker entry. *(AppleWin has no //c ROM — sourced the 32K ROM 4 / 341-0445-B (32768 bytes) from the apple2.org.za mirror; extended `RomSpec` with an optional alternate host/urlPath/label. Size-checked, download-on-demand, not committed.)*
- [X] T021 [US2] Implement the 32K bank-switched ROM mapping. *(`Apple2cRomBank` + `IRomBankSwitch` hook in the //e soft-switch bank: `$C028` toggles the two 16K banks across `$C100-$FFFF` (LC `$D000-$FFFF` + `CxxxRomRouter` `$C100-$CFFF`); `/RESET` -> bank 0. Verified: 65C02 resets to the monitor entry `$FA62` with the ROM correctly mapped through the LC (`BuildsAndResetsToMonitorEntry`). Production `MachineManager` path wired: `CreateMemoryDevices` adds bank 0 as a flat device (normal LC/Cxxx split), then `WireApple2cRomBank` layers the coordinator + `$C028` hook; `EmulatorShell` owns it and tears it down before the LC/MMU. ROM-free banking unit tests pass.)*
- [X] T022 [US2] Create `Resources/Machines/Apple2c/Apple2c.json`: `cpu: 65C02`, 128K, //e substrate. *(JSON + banked systemRom schema (`romBankSize`/`romBankSelect`) + parse tests. Embedded as `IDR_MACHINE_APPLE2C`; `EnsureMachineConfigs` writes it to disk and `MachineScanner::Scan` surfaces "Apple //c" in the picker; selecting it pulls the ROMs via the catalog.)*

> **✅ The //c cold-boots its firmware end-to-end.** With no disk it clears the
> screen, shows the "Apple //c" banner, probes the built-in IWM drive, and reaches
> the correct **"Check Disk Drive."** no-disk state (`ColdBootsToCheckDiskDrive`).
> Booting actual software needs a bootable disk image (the //c has no cassette /
> BASIC-on-cold-boot). The bring-up chain that got here:
>
> 1. ✅ **Rockwell bit ops (RMB/SMB/BBR/BBS)** — RESOLVED. Casso's `Cpu65C02`
>    now models the **Rockwell R65C02** (bit ops via `InstallBitOps`; WDC
>    WAI/STP `$CB`/`$DB` stay NOPs). Covered by `Cpu65C02Tests`. **Two conformance
>    follow-ups:** (a) Casso's assembler can't yet emit the BBR/BBS zero-page-
>    relative form, so the Dormann integration path runs the common opcode subset;
>    (b) the Harte 65C02 corpus should be regenerated as `rockwell65c02` (the 34
>    `$x7`/`$xF`/`$CB`/`$DB` slots are currently skipped there, covered by unit tests).
> 2. ✅ **//c `$C100-$CFFF` routing** — RESOLVED. The //c has no card slots, so
>    the whole window (incl. the `$C800` expansion space) always reads internal
>    firmware: `CxxxRomRouter::SetNoExternalSlots(true)` (set in `WireApple2cRomBank`
>    + `BuildApple2c`). This subsumes T023 (no per-slot ROM slices needed).
> 3. ✅ **`$C028` ROM-bank toggle never fired** — RESOLVED (two bugs, fixed
>    together): (a) `Apple2eKeyboard` (front device for `$C000-$C063`) now forwards
>    `$C028` read+write to the soft-switch bank; (b) `Cpu::FetchOperandAbsolute`
>    no longer pre-reads a STORE's target (a spurious read double-toggled the
>    any-access `$C028` flip-flop). Guards: `KeyboardTests::IIeKeyboard_ForwardsC028...`
>    (ROM-free) + `Apple2cBootTests::StaC028TogglesRomBankExactlyOnce` (e2e).
> 4. ✅ **Slot-6 IWM** — RESOLVED. The bank-1 firmware at `$CC29` writes the IWM
>    MODE register then reads it back via the STATUS register to confirm the
>    built-in drive. The //c drive is an Integrated Woz Machine, so
>    `Disk2Controller` gained an IWM mode (`SetIwmMode`): MODE register
>    (Q6H+Q7H+motor-off write) + STATUS register (Q6H+Q7L read, low 5 bits =
>    MODE). Created in `MachineManager::CreateMemoryDevices` (production) +
>    `BuildApple2c` (harness) as a built-in slot-6 device, not a config slot.
> 5. ⏳ **Serial 6551 ACIA** (T024) — the two ACIA ports aren't wired into the //c yet (loopback/file endpoints); not needed to boot, and **fully in 016 scope** (self-contained, no dependency on 015 or #87). The serial *printer bridge* is separate **downstream** work in #87, which depends on 016 + 015 — never the reverse.
- [X] T023 [US2] ~~Wire the //c firmware slices into `CxxxRomRouter::SetSlotRom` (slots 1/2/3/4/6)~~ — **subsumed by blocker 2's no-slots routing.** The //c firmware for every phantom slot already lives in the internal `$C100-$CFFF` image; `SetNoExternalSlots(true)` serves it for the whole window, so no per-slot `SetSlotRom` slices are needed (FR-006a).
- [X] T024 [US3] Wire the two `Acia6551` instances (Phase 2) into the //c serial ports (slots 1 + 2) + the //c serial firmware, with **loopback/file** endpoints. *(**Done.** Two built-in 6551 ACIAs created like the IWM (the //c is slotless) at the phantom-slot addresses — port 1 $C098, port 2 $C0A8 — in both build paths: `MachineManager::CreateMemoryDevices` (production, attached to the shared `InterruptController`) and `HeadlessHost::BuildApple2c` (harness). v1 endpoints are loopback (`AciaLoopbackEndpoint`); the serial firmware is already in the internal //c ROM. Covered by `Apple2cBootTests::SerialPortsLoopBackViaBuiltInAcia` (both ports round-trip a byte); //c still boots (verified in-app + boot tests green). Endpoints owned via `EmulatorShell::m_ownedAciaEndpoints`. The printer-endpoint bridge is downstream in #87, **not** part of T024.)*

> **Out of 016 scope — strictly downstream in #87** (`Apple //c serial printer integration + per-port device selection`). The //c **serial printer** front door (`PrinterSink` ring hoist + `AciaPrinterEndpoint` + port-1 binding) and the Hardware-tab **serial endpoint selector** belong to **#87, not 016**. #87 depends on this spec **and** spec 015 landing on `master`, so it is downstream of both — **016 never waits on #87** (that would be circular, since #87 requires 016 complete). Brief: `serial-printer-integration.md`; fixture: `reference/printshop-color-testpage.bin`.

- [X] T025 [US2] Make boot + phantom-slot tests pass; assert //c machine selection persists + restores like any other machine (FR-016). *(Boot + phantom-slot green: `Apple2cBootTests` `ColdBootsToCheckDiskDrive` / `BuildsAndResetsToMonitorEntry` + `StaC028TogglesRomBankExactlyOnce` (no-external-slots $Cxxx routing), `Apple2cRomBankTests` (5). Persistence: `//c` selection round-trips via the machine-agnostic `GlobalUserPrefs.lastSelectedMachine` string (covered by `GlobalUserPrefsTests::RoundTrip_FullPrefs`); the //c is a first-class registered machine (AssetBootstrap + ComponentRegistry + MachineScanner), so FR-016 holds without a //c-specific duplicate.)*

**Checkpoint**: **Apple //c boots** with working serial ports (loopback/file endpoints). SC-002. Serial *printing* + endpoint selection are issue #87 (post-015/016).

---

## Phase 6: User Story 4 — Mouse + interrupts

**Goal**: Mouse-driven //c software via the built-in mouse firmware + VBL/mouse IRQs.

**Independent Test**: Host X/Y/click → firmware position/button; VBL/mouse IRQ delivered; $C019 status correct.

> **Approach locked (design spike done)**: **full IOU hardware model** — run the real //c ROM mouse firmware and emulate the hardware it touches (chosen over firmware-entry HLE, which would need a new CPU execution-trap/register seam). The exact soft-switch contract was **derived by disassembling the //c ROM 4 mouse firmware** (it is the authoritative oracle) and is captured in [`reference/mouse-hardware.md`](reference/mouse-hardware.md): `$C066`/`$C067` bit7 = mouse X0/Y0 (game-port PADDL2/3 repurposed), `$C048` = X0/Y0 IRQ ack, `$C058-$C05F` = mouse/VBL int enables+edge (bracketed by `$C079`/`$C078` IOU gate), `$C05A`/`$C05B` = DISVBL/ENVBL, `$C063` bit7 = button (active-low), `$C019` = VBL (already implemented). Integration surface mapped: the mouse soft-switches live in the keyboard-claimed `$C000-$C063` range + the `Apple2eSoftSwitchBank` page (which returns paddle-timer values for `$C066`/`$C067` today), so the device integrates **via** those, not as a standalone slot device. T026 will drive the **real firmware entry points** (SETMOUSE/READMOUSE/SERVEMOUSE) in the headless harness so exact bit/edge/ack semantics are pinned empirically.

### Tests (expect FAIL)

- [X] T026 [P] [US4] `UnitTest/EmuTests/AppleMouseTests.cpp`: X/Y/button, VBL/mouse IRQ, $C019 status; assert IRQs neither starve nor double-fire across acknowledge (edge case). *(**Done — 7 tests.** Device tier: `MovementLatch_HoldsUntilAck_NeverDoubleFires` (the edge case: level-held until $C048 ack, one IRQ per unit, queue drains), direction polarity, VBL latch/ENVBL gate/$C070 clear, active-low button, DISXY mask-not-clear. Firmware tier (the oracle): `FirmwareIdentifiesMouseAtSlot7` + `FirmwareTracksMotionAndButton_TransparentMode` — a RAM driver calls the REAL ROM 4 protocol entries (INITMOUSE $C740 → SETMOUSE $C71C mode 1) and injected +5/+3 host motion flows entirely through the firmware's IRQ service; READMOUSE reports x=5,y=3 in the slot-7 screen holes and the button reads through the status hole.)*

### Implementation

- [X] T027 [US4] Implement `AppleMouse` (`CassoEmuCore/Devices/AppleMouse.{h,cpp}`): X/Y/button, VBL + mouse IRQ sources (X0/Y0 latch/ack), `AttachInterruptController` like `Acia6551`. *(Done. Not a bus device — integrates via keyboard/bank forwarding (the $C028 precedent); host motion accumulates atomically and latches one unit per axis per interrupt; firmware owns position + clamping. Ticked per-instruction via the new `ICycleSink` seam on `EmuCpu::AddCycles`.)*
- [X] T028 [US4] Add //c interrupt soft switches to the //c soft-switch path. *(Done, per the corrected `reference/mouse-hardware.md` map: keyboard forwards $C048 (RSTXY) + $C063 (button, active-low, replacing the //e shift read); the bank serves $C015/$C017 (X0/Y0 int status) + $C019 (VBL latch) status overrides, $C066/$C067 (MOUX1/MOUY1), $C070 VBL-clear side effect, $C078/$C079 IOUDIS gate, and $C058-$C05F as the IOU programming bank while access is enabled — all //c-gated (mouse attached), so //e behavior is untouched. **Bonus root-cause fix**: `EmuCpu::StepOne` now polls the IRQ/NMI lines (`Cpu6502::DispatchPendingInterrupt`) — the StepOne+AddCycles hosts (production slice loop, headless RunCycles) had NEVER dispatched a hardware interrupt; the mouse is the first real source through `InterruptController`.)*
- [X] T029 [US4] ~~Register `apple-mouse` in `ComponentRegistry`; add to `Apple2c.json` (slot-4 firmware)~~ — **satisfied by built-in creation (matches the IWM/ACIA pattern, T034).** The //c mouse is built-in hardware, not a config slot: created directly in `MachineManager::CreateMemoryDevices` (production, //c-gated) + `HeadlessHost::BuildApple2c` (harness); its slot-7 firmware is already in the internal //c ROM (**ROM 4 puts the mouse at phantom slot 7** — slot 4 is the memory-expansion firmware; spec's "slot 4" assumption corrected by the firmware itself).
- [ ] T030 [US4] Map the host pointer → guest mouse in the shell input path. **Mouse mode is non-capturing**: while the host cursor is over the emulator viewport, motion + buttons drive the guest mouse (absolute mapping: host position in the viewport → guest mouse position; host cursor hidden over the viewport); leaving the viewport releases to the host. Contrast with Paddle mode, which captures. *(Emulation-side API ready: `AppleMouse::MoveBy`/`SetButton` are thread-safe host-side entry points.)*
- [ ] T030a [US4] Add `Mouse` to `InputMappingMode` (`GlobalUserPrefs`: `Off`/`Joystick`/`Paddle`/`Mouse` + string mapping), **gated to mouse-capable machines** (//c, //e-with-mouse; hidden on ][ / ][+ / plain //e). Replace the `JoystickToggleButton` cycle-toggle with a **segmented device selector using skeuomorphic icons** (joystick / paddle / mouse); wire it to the existing "Cycle Input Mode" path + persistence. *(Needs joystick/paddle/mouse icon assets.)*
- [X] T031 [US4] Make mouse tests pass. *(All 7 green; full suite 2276/2276 — the CPU interrupt-dispatch change regressed nothing across Dormann/Harte/boot/disk; production //c boots DOS 3.3 with the mouse + ACIAs + live IRQ dispatch wired.)*

**Checkpoint**: mouse-driven //c software (e.g. MousePaint) works.

---

## Phase 7: User Story 5 — IWM disk

**Goal**: //c boots/reads/writes internal + external drives via the IWM, reusing the WOZ engine.

**Independent Test**: WOZ in internal drive → boots via IWM (slot-6 space); external drive → second-drive access.

### Tests (expect FAIL)

- [X] T032 [P] [US5] `Apple2cBootTests.cpp` (disk): //c boots from internal drive via IWM; external-drive read. *(`BootsFromInternalDriveViaIwm`: stamps an in-house 18-byte boot sector into T0S0, cold-boots the //c, and asserts it read the sector via the IWM and ran it — the marker "IWM" lands at $0300 and the CPU spins in the booted halt loop, not the Check-Disk-Drive loop. `ExternalDriveIsReadableViaDriveSelect`: selects drive 2 ($C0EB), spins it up, and samples a valid nibble off the external drive.)*

### Implementation

- [X] T033 [US5] ~~Implement `Iwm` (`CassoEmuCore/Devices/Iwm.{h,cpp}`)~~ — **satisfied by the existing `Disk2Controller` IWM mode, verified end-to-end (compose, do not fork).** `Disk2Controller::SetIwmMode` already added the MODE/STATUS registers on top of the shared Disk II / WOZ nibble engine; the CPU-visible RDDATA path (Q6L/Q7L) is unchanged, so a mounted disk streams nibbles to the boot ROM. T032's two tests prove both drives read through it — no separate `Iwm` class needed.
- [X] T034 [US5] Register `iwm` in `ComponentRegistry`; add to `Apple2c.json` (slot-6, internal + external drive). *(**Emulation done**: the built-in slot-6 IWM is created directly in `MachineManager::CreateMemoryDevices` (guarded by `romBankSize != 0`), not via `ComponentRegistry` — the //c drive is built in, not a config slot. Both drives (internal = drive 1, external = drive 2) read at the controller level, proven by T032. GUI surfacing is T034a, now complete.)*
- [X] T034a [US5] Hardware tab: the //c **external drive** is a **Connected / Not connected** toggle that shows/hides the existing drive-mount widget (image name, Mount…, Eject, write-protect). No new media machinery — "Connected" just reveals the existing drive widget; persist the connected state per machine. *(Done. A per-machine `$cassoUiPrefs.externalDriveConnected` pref (defaults not-connected) drives a checkable "External drive" node in the Settings > Hardware tree — only for machines with a banked ROM (`SettingsMachineInfo.supportsExternalDrive`, the //c). Toggling it is a live UI pref (no reset): `SettingsPanelState::SetExternalDriveConnected` → `ISettingsApplySink::ApplyExternalDriveConnected` → `IDM_DRIVE_EXTERNAL_CONNECT/DISCONNECT` → `EmulatorShell::ShouldShowExternalDrive()` gates `m_driveChrome[1]` (widget + hit rect) in both drive-layout paths, re-laid via `ReflowChromeForMachineChange`. Seeded at initial launch and on menu machine-switch. Non-//c machines: the gate is always open, second drive unchanged. Covered by `ExternalDriveConnected_RoundTripsAndPushesLiveNoReset`, `SupportsExternalDrive_TrueForBankedRomOnly`, and 3 `HardwarePageTests` `BuildNodes_*ExternalDrive*` cases.)*
- [X] T035 [US5] Make //c disk-boot tests pass; regression: existing Disk II machines unchanged. *(Both //c disk tests green; full suite 2268/2268, existing Disk II machines unaffected.)*

**Checkpoint** ✅: //c boots from its internal drive + reads its external drive via the IWM (emulation complete + tested), **and** the external-drive GUI toggle (T034a) ships. Phase 7 / User Story 5 complete.

---

## Phase 8: Polish & Cross-Cutting

- [ ] T036 [P] Run full `quickstart.md` validation across all stories.
- [ ] T037 [P] Update `README.md` + `CHANGELOG.md` for the new Apple //c machine.
- [~] T038 Full suite green across Debug+Release × x64+ARM64; Code Analysis clean. *(**Validated so far on this x64 host**: builds clean (0W/0E) for all four Debug/Release × x64/ARM64; x64 tests green — Debug 2268/2268, Release 2265/2265 (Debug has 3 assert-only tests); **Code Analysis clean** across the whole solution after suppressing a lone pre-existing C6262 in `Cpu65C02Tests` (per-file pragma matching the sibling test files). **Remaining**: ARM64 **test execution** needs an ARM64 host/CI (can't run ARM64 binaries on x64); final signoff after US4.)*
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
