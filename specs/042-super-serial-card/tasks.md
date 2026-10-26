# Tasks: Super Serial Card

**Input**: Design documents from `specs/042-super-serial-card/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/serial-attachment.md](contracts/serial-attachment.md), [quickstart.md](quickstart.md)

**Tests**: Required by the constitution (Principle II). Every new test is checked by mutating what it covers and confirming it fails.

**Organization**: by user story. The feature ships whole in one merge; phase order is build order only.

Every new `.h`/`.cpp` is added to `CassoEmuCore/CassoEmuCore.vcxproj` and its `.filters`, and every new test file to `UnitTest/UnitTest.vcxproj` and its `.filters`, in the task that creates it.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on an unfinished task)
- **[Story]**: the user story the task is part of (US1-US6)

---

## Phase 1: Setup

- [X] T001 Add the 6551 baud-rate table (codes 1-15: 50, 75, 109.92, 134.58, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600, 19,200; code 0 = crystal / 16) and the 1,843,200 Hz crystal constant as a file-scope `static constexpr` table in `CassoEmuCore/Devices/Acia6551.cpp`, with a `GetBaudRate (int code)` static that returns bits per second as a double (R-006)

---

## Phase 2: Foundational (blocks every story)

### The 6551 rework

- [X] T002 [P] Create `SerialByteQueue` in `CassoEmuCore/Devices/SerialByteQueue.h/.cpp`: mutex-guarded deque with an atomic count; `Push (const Byte *, size_t)` and `GetCount()` callable from any thread, `TryPop (Byte &)` and `Clear()` on the emulation thread (data-model `rxQueue`, contract "thread-safe")
- [X] T003 [P] Tests for `SerialByteQueue` in `UnitTest/EmuTests/SerialByteQueueTests.cpp`: order, count, clear, a push from a second thread while the owner pops
- [X] T004 Add timing state to `Acia6551` in `CassoEmuCore/Devices/Acia6551.h/.cpp`: `SetCpuClockHz (uint32_t)`, `Tick (uint32_t cycles)`, fixed-point (cycles x 65536) character-time computation from control bits 0-4 and 5-7 and command bits 5-7: 1 start + word length + parity bit if command bit 5 set + stop bits (1; 2 with control bit 7; 1.5 with control bit 7, a 5-bit word and no parity) (R-006). An idle chip returns from `Tick` after one test
- [X] T005 Rework the transmitter in `CassoEmuCore/Devices/Acia6551.cpp` per R-007: TDR and shift register; a data write fills TDR and clears TDRE; TDR loads into an idle shift register when CTS is true, DTR is on and TIC is not 00, and TDRE sets; the byte is passed to `IAciaEndpoint::OnByteTransmitted` when the shift register finishes, one character time later; transmitter IRQ when TDRE sets with TIC = 01; TIC = 11 sends break (no shifting) and calls `OnBreakChanged`
- [X] T006 Rework the receiver in `CassoEmuCore/Devices/Acia6551.cpp` per R-008: `EnqueueReceivedBytes (const Byte *, size_t)` and `GetQueuedReceiveCount()` over `SerialByteQueue`; on `Tick`, while DTR is on, move one byte into RDR per receiver character time (control bit 4 clear selects crystal / 16), the first one character time after a byte arrives at an idle receiver; overrun and replacement when RDRF is set; mask to the word length; receiver IRQ; echo mode (command bit 4) retransmits each received byte. Remove `ReceiveByte`
- [X] T007 Add modem lines and the IRQ gate to `CassoEmuCore/Devices/Acia6551.h/.cpp` and `CassoEmuCore/Devices/IAciaEndpoint.h` per R-009 and the contract: `SetModemInputs (bool cts, bool dsr, bool dcd)` through atomics applied at the next tick; status bits 5 and 6 read 0 while DCD and DSR are true; a DCD or DSR change sets the IRQ bit while DTR is on; `IAciaEndpoint::OnModemControlChanged (bool dtr, bool rts)` and `OnBreakChanged (bool)` with no-op defaults, called on change and once from `SetEndpoint`; `SetEndpoint (nullptr)` empties the queue and restores CTS, DSR, DCD to true; `SetIrqConnected (bool)` gates the controller line while status bit 7 latches regardless (R-002)
- [X] T008 Apply the reset transitions from data-model.md in `CassoEmuCore/Devices/Acia6551.cpp`: RESB "clears command and control, empties both transmit registers, clears RDRF and the error and IRQ bits, and sets TDRE; `rxQueue` is not emptied"; program reset "clears command bits 0-4 and does not alter control or command bits 5-7, and it clears the error bits and RDRF"; notify the endpoint of DTR and RTS off after RESB
- [X] T009 Extend `UnitTest/EmuTests/Acia6551Tests.cpp`: transmit of 1,000 bytes at every baud code within 1% of the computed time (SC-004); TDRE double buffering (TDRE stays clear for one character time after the second write); CTS false stops loading; break notification; queue delivery one character time apart; overrun on the first byte not read in time (SC-003 at chip level); DTR off pauses delivery; word-length mask; IRQ raise and clear for RX, TX and DCD/DSR change; IRQ gate off with status bit 7 latching; both reset transitions; echo. Update existing tests that assumed instant transmit or `ReceiveByte`
- [X] T010 Rewire `AciaLoopbackEndpoint` in `CassoEmuCore/Devices/AciaEndpoints.h/.cpp` as a loopback plug: transmitted bytes go to `EnqueueReceivedBytes`; RTS drives CTS and DTR drives DSR and DCD through `SetModemInputs` from `OnModemControlChanged`
- [X] T011 Tick every 6551: add a list of `Acia6551 *` to the machine host refs and tick each in `MachineHost::StepOne` and the cycle-batch path in `CassoEmuCore/Shell/MachineHost.cpp`, beside the disk controller and Mockingboard (R-010); in `CassoEmuCore/Shell/MachineBuilder.cpp` set the //c ports' CPU clock from `config.clockSpeed` and add them to the list
- [X] T012 Run the //c's existing serial tests (`RunTests.ps1 -Build -Filter Acia`, then `-Filter Apple2c`) and fix what the paced transmitter changed (SC-006)

### Slot ROM selection

- [X] T013 [P] Create `IExpansionRomCard` in `CassoEmuCore/Machines/Apple2/Common/IExpansionRomCard.h`: `OnSlotPageAccess (Word address, bool isWrite, Byte value)` returning the byte read, `IsExpansionRomSelected()`, `ReadExpansionRom (Word address)`, `DeselectExpansionRom()`
- [X] T014 Create `SlotRomPage` (a `MemoryDevice` at `$Cs00`-`$CsFF` forwarding to an `IExpansionRomCard`) in `CassoEmuCore/Machines/Apple2/Common/SlotRomPage.h/.cpp` and `ExpansionRomBus` (a `MemoryDevice` at `$C800`-`$CFFF`: registers cards; reads `$C800`-`$CEFF` as the AND of every selected card's byte (R-005) or the bus's floating value when none is selected; any access to `$CF00`-`$CFFF` deselects every card and reads floating) in `CassoEmuCore/Machines/Apple2/Common/ExpansionRomBus.h/.cpp`
- [X] T015 Tests in `UnitTest/EmuTests/ExpansionRomBusTests.cpp` with a fake card: select by page read and by page write, deselect by `$CF00`, `$CF80` and `$CFFF`, floating when none selected, AND of two selected cards
- [X] T016 Delegate the //e's expansion space in `CassoEmuCore/Machines/Apple2/Common/CxxxRomRouter.h/.cpp`: `SetExpansionRomBus (ExpansionRomBus *)`; in `ResolveByte` and `Write`, `$C800`-`$CFFF` with INTCXROM and INTC8ROM both reset goes to the bus (reads and the `$CFxx` deselect); otherwise the bus is not called, so a `$CFFF` access under INTC8ROM clears INTC8ROM only (R-004)
- [X] T017 Router tests in `UnitTest/EmuTests/CxxxRomRouterTests.cpp` (extend the existing file if present): INTCXROM hides page and expansion and an access then does not select; INTC8ROM hides expansion and a `$CFFF` access under it does not deselect; slot 3 requires SLOTC3ROM

**Checkpoint**: the 6551 is paced and the slot-ROM mechanism exists; both are unit-tested.

---

## Phase 3: User Story 1 - Put a Super Serial Card in a slot (P1)

**Goal**: the card in slot 2 of a ][, ][+ or //e, turned on from the Hardware tab, with Apple's firmware mapped.

**Independent Test**: a //e with a synthetic 2 KB firmware: `$C205`/`$C207`/`$C20B`/`$C20C` read `$38`/`$18`/`$01`/`$31` from the image's `$705`/`$707`/`$70B`/`$70C`; `$C800`-`$CEFF` follow after a `$C200` access; `$CFxx` deselects.

- [X] T018 [P] [US1] Create `SscSwitches` in `CassoEmuCore/Machines/Apple2/Common/SscSwitches.h/.cpp`: positions SW1-1..7 and SW2-1..7 (true = ON); `GetDipSwitch1Byte (Byte floating)` and `GetDipSwitch2Byte (bool ctsTrue, Byte floating)` per R-001 ("A switch that is OFF reads 1"; DIPSW1 bit 0 SW1-6, bit 1 SW1-5, bits 4-7 SW1-4..SW1-1; DIPSW2 bit 0 CTS (0 when true), bits 1-3 SW2-5..SW2-3, bit 5 SW2-2, bit 7 SW2-1; undriven bits from `floating`); `GetMode()`, `GetBaudCode()`, `IsIrqConnected()`; `MakeDefault()` = "SW1 = OFF OFF OFF ON ON ON ON, SW2 = ON ON OFF ON OFF OFF OFF"; `ToText` / `TryParseText` for `"off off off on on on on"` with "Missing or malformed values fall back to the default per bank"
- [X] T019 [P] [US1] Tests in `UnitTest/EmuTests/SscSwitchesTests.cpp`: every switch at its bit in both polarities, undriven bits from the floating byte, CTS bit, mode and baud decoding against the reference card's table, the default, text round trip and malformed fallback
- [X] T020 [US1] Create `SuperSerialCard` in `CassoEmuCore/Machines/Apple2/Common/SuperSerialCard.h/.cpp`: a `MemoryDevice` for `$C080 + slot*16` to `+$F` implementing `IExpansionRomCard`; owns an `Acia6551` at `+8` and a `SlotRomPage`; `+1`/`+2` from `SscSwitches` with the ACIA's CTS; other offsets floating; `$Cs00` page reads firmware `$700 + offset` and sets the latch; expansion reads firmware `$000`-`$6FF`; empty firmware reads floating; `SetFirmware (vector<Byte>)`, `SetSwitches (const SscSwitches &)` (applies SW2-6 to `Acia6551::SetIrqConnected` at once); `Reset`/`SoftReset`/`PowerCycle` reset the ACIA and clear the latch; `static Create (const DeviceConfig &, MemoryBus &)`
- [X] T021 [US1] Tests in `UnitTest/EmuTests/SuperSerialCardTests.cpp`: I/O window decode, ACIA pass-through, page and expansion mapping from a synthetic image, latch set by read and write, cleared by reset, missing firmware reading floating, SW2-6 gating the IRQ line
- [X] T022 [US1] Register `"super-serial-card"` in `CassoEmuCore/Core/ComponentRegistry.cpp`
- [X] T023 [US1] Extend `CassoEmuCore/Core/MachineConfig.h/.cpp`: `SlotConfig::firmware`, `firmwareResolvedPath`, `firmwareStatus` (Present / Missing / WrongSize, data-model.md), `switchesText` (sw1, sw2); `PortConfig::path`; `LoadSlots` resolves `firmware` under `Devices/SuperSerialCard` for `super-serial-card` without failing when absent; `ParsePorts` reads `path`; `CollectRomFiles` ignores `firmware`; and `AssetBootstrap::GetRequiredRoms` in `CassoEmuCore/AssetBootstrap.cpp` likewise (R-011)
- [X] T024 [US1] Config tests in `UnitTest/EmuTests/MachineConfigTests.cpp`: `firmware` resolved, missing and wrong size each loading successfully with the right status; `switches` and `ports[].path` parsed; `CollectRomFiles` not listing the firmware
- [X] T025 [US1] Wire the card in `CassoEmuCore/Shell/MachineBuilder.cpp`: create `ExpansionRomBus` once per slot machine (bus device on ][/][+, `CxxxRomRouter::SetExpansionRomBus` on the //e); for an enabled `super-serial-card` slot, read the firmware file (2,048 bytes, else WrongSize) into the card, apply the slot entry's switches, register the card's `SlotRomPage` (bus device on ][/][+, `SetSlotIoDevice` on the //e) and the card with the expansion bus, attach the ACIA to the interrupt controller, set its clock and add it to the tick list (T011); store a `superSerialCard` ref for the shell
- [X] T026 [US1] Add `{ "slot": 2, "device": "super-serial-card", "firmware": "SSC.rom", "enabled": false }` to `Resources/Machines/Apple2/Apple2.json`, `Apple2Plus/Apple2Plus.json`, `Apple2e/Apple2e.json` and `Apple2eEnhanced/Apple2eEnhanced.json`; bump each `$cassoMachineVersion`; add the previous files' normalized hashes to the prior-hash table in `CassoEmuCore/AssetBootstrap.cpp`
- [X] T027 [US1] Slot-2 migration in `CassoEmuCore/Core/MachineConfigUpgrade.h/.cpp` beside the slot-1 printer migration: add the entry to a user config with no slot-2 entry; "The migration MUST NOT replace a card already in slot 2"; never on a config with no slots (the //c); tests in `UnitTest/EmuTests/MachineConfigUpgradeTests.cpp` and `BackwardsCompatTests.cpp`
- [X] T028 [US1] Display name "Super Serial Card" for `super-serial-card` in `SettingsPanelState::ExtractHardware`, `CassoEmuCore/Ui/Settings/SettingsPanelState.cpp`, so the Hardware tab lists the slot-2 row with its checkbox
- [X] T029 [US1] Whole-machine tests in `UnitTest/EmuTests/SuperSerialCardMachineTests.cpp` using `TestMachine` with a synthetic firmware: //e and ][+ ID bytes, select and deselect from guest code, a ][+ with the Disk II slot ROM in slot 6 unaffected, //e INTCXROM hiding the card, card off leaving slot 2 empty

**Checkpoint**: US1 passes its independent test.

---

## Phase 4: User Story 2 - Print and type through Apple's firmware (P1)

**Goal**: `PR#2` and `IN#2` through Apple's firmware send and receive through the connector.

**Independent Test**: with Apple's firmware and a file attachment, `PR#2` + `LIST` writes the listing to the file (manual check, quickstart step 3; Apple's firmware is not allowed in the test tree).

- [X] T030 [US2] Build the connector attachment from `ports[0]` in `CassoEmuCore/Shell/MachineBuilder.cpp`: `""` none, `serial-loopback` `AciaLoopbackEndpoint`, `serial-file-out` `AciaFileEndpoint` opened at `path` (a file that fails to open results in no attachment, reported through the same status as the infobar); owned by the host and closed on teardown
- [X] T031 [US2] Whole-machine test in `UnitTest/EmuTests/SuperSerialCardMachineTests.cpp` with a synthetic firmware whose `$Cs00` entry selects the card, jumps into `$C800` code and writes a character through the ACIA: guest `JSR $C200` delivers the byte to a recording endpoint one character time after the write, at the baud set by the switches
- [X] T032 [US2] Manual validation with Apple's firmware per quickstart steps 1-4, written up in `specs/042-super-serial-card/validation.md`

---

## Phase 5: User Story 3 - Drive the 6551 directly, with or without the firmware (P1)

**Goal**: with no firmware, a working 6551 and an infobar with a Download button.

**Independent Test**: a loopback test program at 19,200 baud with no firmware; the infobar appears on start.

- [X] T033 [P] [US3] Catalog row and on-demand download in `CassoEmuCore/AssetBootstrap.h/.cpp`: row `{ "", "SSC.rom", "", "Devices/SuperSerialCard", 2048, "Super Serial Card firmware (341-0065-A)", "mirrors.apple2.org.za", "/Apple%20II%20Documentation%20Project/Interface%20Cards/Serial/Apple%20II%20Super%20Serial%20Card/ROM%20Images/Apple%20II%20Super%20Serial%20Card%20ROM%20-%20341-0065-A.bin", "apple2.org.za" }`; public `DownloadOptionalRom (string_view cassoName, fs::path & outPath, string & outError)` built on `DownloadHttp`, size-checked
- [X] T034 [US3] Firmware infobar in `CassoEmuCore/Shell/Components/ShellSerialCard.h/.cpp`, hosted like the disk-change banner in `ShellDisks.cpp`: on machine start with an enabled card whose status is Missing or WrongSize, a `DxuiActionBanner` with the message and a Download button; a successful download replaces it with a Reset button; a failed one reports the error in the banner
- [X] T035 [US3] Message text in a core function (testable, sentence case) in `CassoEmuCore/Shell/Components/ShellSerialCard.cpp` or a small model class, with tests: missing and wrong-size wording including the path searched
- [X] T036 [US3] Whole-machine test in `UnitTest/EmuTests/SuperSerialCardMachineTests.cpp`: no firmware, guest code programs 19,200 baud 8N1 through `$C0AB`/`$C0AA`, sends bytes over loopback and reads them back; baud code 0 runs at 115,200 (SC-005, US3 scenario 4)

---

## Phase 6: User Story 4 - Set the card's switches (P2)

**Goal**: a Serial card page in Settings with the switch settings.

**Independent Test**: printer mode at 1,200 baud from the page; `$C0A1` reads the new positions with no reset.

- [X] T037 [P] [US4] Create `SscSettingsModel` in `CassoEmuCore/Ui/Settings/SscSettingsModel.h/.cpp` per data-model.md's table: settings per mode with choices and the switches each writes; SIC P8/P8A list the printer settings; "Custom" for a combination no choice produces; the readout text of SW1 and SW2; printer line width and video choices from the reference card
- [X] T038 [P] [US4] Tests in `UnitTest/UiTests/SscSettingsModelTests.cpp`: each setting writes exactly its switches, mode change swaps the setting list, Custom, readout text
- [X] T039 [US4] Stage the slot entry's `switches` and `ports` in `CassoEmuCore/Ui/Settings/SettingsPanelState.h/.cpp`; dirty without requiring a reset; written back into the slot entry by `BuildJson`; tests in `UnitTest/UiTests/SettingsPanelStateTests.cpp`
- [X] T040 [US4] Create `SerialCardPage` in `CassoEmuCore/Ui/Settings/SerialCardPage.h/.cpp` following `PrintingPage`/`DiskPage`: drop-downs from the model, the readout, the firmware status with Download; added to the sheet in `SettingsSheet.cpp` only while the staged hardware has the SSC enabled
- [X] T041 [US4] Live apply: a switch change is applied to the running card through `ISettingsApplySink` (`ApplySerialCardSwitches`) and `SuperSerialCard::SetSwitches`; tests with the recording sink
- [X] T042 [US4] Turning the card on in the Hardware tab with no firmware offers the download at once (`HardwarePage.cpp` or the apply controller), using `DownloadOptionalRom`
- [X] T052 [US4] Replace the readout with `SscSwitchesView` in `CassoEmuCore/Ui/Settings/SscSwitchesView.h/.cpp`: SW1 and SW2 drawn as the card's DIP switch banks, a click flips a switch and the drop-downs follow
- [X] T053 [US4] The SIC emulations in full in the Mode list, 115,200 baud for all four baud switches on, a note under Mode, info tips on line feed after carriage return and interrupts (`SscSettingsModel::GetNote`, `GetInfoTip`, `SettingsInfoTip`); tests in `SscSettingsModelTests.cpp`
- [X] T054 [US4] Move the update indicator's shimmer into `Dxui/Widgets/DxuiShimmer.h/.cpp` and sweep it over the Serial card tab's label every five seconds from when turning the card on adds the tab until the tab is opened (`SettingsSheet::PaintSerialSparkle`); tests in `UnitTest/Dxui/DxuiShimmerTests.cpp`

---

## Phase 7: User Story 5 - Receive at real-hardware speed (P2)

**Goal**: bytes delivered one character time apart at the machine level.

**Independent Test**: 64 KB at 19,200 baud through a guest read loop without overruns.

- [X] T043 [US5] Whole-machine test in `UnitTest/EmuTests/SuperSerialCardMachineTests.cpp`: a guest loop that polls RDRF and stores bytes receives 64 KB enqueued at once with no overrun bit set and successive arrivals one character time apart (SC-003); a deliberately slow loop reports overrun on the first missed byte; DTR off pauses delivery; IRQ-driven receive with SW2-6 ON raises the CPU IRQ and with it OFF does not
- [X] T044 [US5] Test that emulated pacing is independent of speed mode: running the same receive under the host's maximum-speed path yields the same cycle stamps

---

## Phase 8: User Story 6 - Choose what the connector is attached to (P3)

**Goal**: none, loopback, file out, file in from Settings.

**Independent Test**: a file sent in arrives through `IN#2`-style reads one character time apart.

- [X] T045 [P] [US6] Create `AciaByteSourceEndpoint` in `CassoEmuCore/Devices/AciaByteSourceEndpoint.h/.cpp`: given a byte vector, enqueues it when attached; transmitted bytes discarded; tests in `UnitTest/EmuTests/AciaEndpointsTests.cpp`
- [X] T046 [US6] Build `serial-file-in` in `CassoEmuCore/Shell/MachineBuilder.cpp`: read the file at `path` through the existing file seam and attach an `AciaByteSourceEndpoint`
- [X] T047 [US6] Attachment drop-down and path (browse button) on `SerialCardPage`; changing it rebuilds only the attachment (plan design note 5)
- [X] T055 [US6] Connector labels "None", "Loopback plug", "Write output to file" and "Read input from file", with a note under the drop-down for each

---

## Phase 9: Polish and cross-cutting

- [X] T048 `CHANGELOG.md` `[Unreleased]` entries (Added: Super Serial Card; Changed: //c serial ports paced at their baud rate) and `README.md` feature list, drafted and approved by the owner before commit
- [X] T049 Run `scripts/RunTests.ps1 -Build` in Debug and Release x64, `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`, ARM64 build, and `git add -A` + `scripts/CheckStyle.ps1 -Mode Tree`
- [X] T050 Prose check on every new comment block and commit message (`Check-Prose.ps1`); the empty-paren and American-English greps from the instructions
- [X] T051 Launch the build for the owner with `--title`, validate the manual steps 1-6 in quickstart.md, capture screenshots of the Serial card page and the infobar

---

## Dependencies

- Phase 2 blocks everything. Within it, T004-T008 are sequential (one file); T002/T003 and T013 run beside them; T014-T017 follow T013.
- US1 (Phase 3) blocks US2-US6: each of them uses the card.
- US2's T030 is reused by US6 (T046 extends it).
- US3 T033 is used by US4 T040 and T042.
- US5 depends only on Phase 2 and US1.
- Polish last.

## Parallel examples

- Phase 2: T002+T003 alongside T004-T008; T013 alongside both.
- US1: T018+T019 alongside T020's skeleton; T023+T024 alongside T020-T021.
- US4: T037+T038 alongside US3's T033-T035.

## Implementation strategy

Build in phase order. The chip rework lands first because the //c's ports share it and its tests anchor everything after. The card, its routing and its config follow (US1), then the firmware path and attachments, then Settings. Nothing merges to master until every phase is done and the pre-merge gates in T049 pass.
