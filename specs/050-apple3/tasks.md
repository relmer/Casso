# Tasks: Apple /// Machine

**Input**: Design documents in `specs/050-apple3/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required by FR-068 to FR-070. Each test task is written before the
code it covers and is confirmed to fail without that code.

**Rules for every task**: follow `.github/copilot-instructions.md`. Write
device code only from rows of `contracts/hardware-map.md` whose **Checked**
column is filled. Build and run the touched tests per task; the full gate
runs once, in the last phase. New UI strings come from
`contracts/ui-strings.md` and ship only after the owner approves them.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on an
  unfinished task)
- **[Story]**: the user story in spec.md the task is part of

## Phase 1: Setup

- [x] T001 Collect the Apple documents listed in research.md R1 into `%USERPROFILE%\Downloads\Casso\apple3\docs` and list each file with its source URL in `specs/050-apple3/research.md` under R1
- [ ] T002 (done for every row with a **Checked** entry; the "Open rows" table remains) Check every row of `specs/050-apple3/contracts/hardware-map.md` against the documents: fill **Checked** with document and page, correct rows the documents contradict, settle rows the documents do not cover by consulting an existing emulator under the limits in research.md R1, and add the rows marked as missing (every VIA port line, drive select, character set loading, smooth scroll, color encoding, timing)
- [ ] T003 Update `specs/050-apple3/spec.md`, `data-model.md` and `research.md` wherever T002 changed a fact a requirement or decision depended on, and list the changes for the owner
- [ ] T004 [P] Add `Apple3.rom` (each accepted revision with size and CRC) to the ROM catalog and to `scripts/FetchRoms.ps1 -Fixtures`
- [ ] T005 [P] Add the Business BASIC, System Utilities, Confidence Test and Apple II Emulation disks to `scripts/FetchStockDisks.ps1` and to `.gitignore`'s existing fetched-disk entries under `Disks/Apple/`
- [ ] T006 [P] Create `CassoEmuCore/Machines/Apple3/`, `UnitTest/Apple3Tests/` and their filter entries in `CassoEmuCore/CassoEmuCore.vcxproj` and `UnitTest/UnitTest.vcxproj`
- [ ] T007 Merge `035-debugger` into `050-Apple-III` when its merge of master is announced, fix stale call sites the compiler reports, and run `scripts/CheckStyle.ps1 -Mode Tree`

---

## Phase 2: Foundational

**Blocks every user story.** Memory, the CPU hook, time, the VIAs, the
machine definition and the debugger's address type.

### Tests

- [ ] T008 [P] Write mapping tests for every environment register value, each bank and each zero page value, including the alternate stack and write protection, in `UnitTest/Apple3Tests/Apple3MmuTests.cpp`
- [ ] T009 [P] Write extended addressing tests (extension byte off, `$80-$8E`, `$8F`, bank pair boundary, absent bank, zero page outside `$18-$1F`) in `UnitTest/Apple3Tests/Apple3ExtendedAddressingTests.cpp`
- [ ] T010 [P] Write VIA wiring tests (register writes change the mapping; input lines set interrupt flags) in `UnitTest/Apple3Tests/Apple3SystemViasTests.cpp`
- [ ] T011 [P] Write tick tests (cycle cost at 2 MHz, at 1 MHz and while slowed; device time across a speed switch inside a frame) in `UnitTest/Apple3Tests/Apple3TimeBaseTests.cpp`
- [ ] T012 [P] Write `DebugAddress` tests (equality, conversion from `Word`, text form parse and print per `contracts/debugger-addresses.md`, error cases) in `UnitTest/DebuggerTests/DebugAddressTests.cpp`
- [ ] T013 [P] Write machine definition tests (both machines load; `characterRom`, bad `memory`, bad `externalDrives` and unsupported slot devices fail with the listed messages) in `UnitTest/Apple3Tests/Apple3MachineConfigTests.cpp`

### Implementation

- [ ] T014 Add the extended zero-page indirect Y and zero-page X indirect modes to `CassoCore/GlobalAddressingModes.h` and their fetch cases to `CassoCore/Cpu.cpp`, and implement `Cpu6502Apple3` (derived from `MemoryBusCpu`; the affected opcodes' table entries changed to the new mode; the mapping restored after each instruction in `Step`) in `CassoEmuCore/Machines/Apple3/Cpu6502Apple3.h` and `.cpp`, created by `CassoEmuCore/Core/CpuFactory.cpp`
- [ ] T015 Run Dormann and full-depth Harte against `Cpu6502Apple3` with extended addressing inactive and against the existing cores (`scripts/RunDormannTest.ps1`, `scripts/RunHarteTests.ps1 -SkipGenerate`), and the per-instruction timing comparison from `docs/testing.md` for the existing cores before and after T014; store the results in `specs/050-apple3/research.md` under R4
- [ ] T016 Implement `Apple3Mmu` (RAM for 128K and 256K, the three registers, page table mapping through `MemoryBus::SetReadPage` and `SetWritePage`, ROM, I/O, write protection, zero page and stack pages, `IMachineState`) in `CassoEmuCore/Machines/Apple3/Apple3Mmu.h` and `.cpp`
- [ ] T017 Implement extended access resolution in `Apple3Mmu` (extension byte lookup, the one-page remap from the bank pair for the rest of the instruction, the `$8F` case with the RAM under the VIAs, the restore, and the resolved `DebugAddress`) in `CassoEmuCore/Machines/Apple3/Apple3Mmu.cpp`
- [ ] T018 Implement the tick time base (cycle cost by environment bit 7 and the documented slowing; device clocks derived from ticks) in `CassoEmuCore/Machines/Apple3/Apple3TimeBase.h` and `.cpp`, wired through `ICycleSink`
- [ ] T019 Implement `Apple3SystemVias` (two `Via6522` at `$FFD0-$FFEF`, output change detection, calls into `Apple3Mmu`, input line setters, interrupt wiring) in `CassoEmuCore/Machines/Apple3/Apple3SystemVias.h` and `.cpp`
- [ ] T020 Add `GetBuiltInDriveCount` and `HasBankedMemory` to `CassoEmuCore/Machines/IMachine.h` with Apple ][ defaults, and implement `Apple3` in `CassoEmuCore/Machines/Apple3/Apple3.h`
- [ ] T021 Register `Apple3` in `CassoEmuCore/Machines/MachineDefinitions.cpp` and add `Resources/Machines/Apple3/Apple3.json` per `contracts/machine-config.md`, with the new keys parsed and validated in `CassoEmuCore/Core/MachineConfig.h` and its loader, and the version step in `CassoEmuCore/Core/MachineConfigUpgrade.cpp`
- [ ] T022 Factor an `Apple3MachineBuilder` class out of the build path (`CassoEmuCore/Shell/Apple3MachineBuilder.h` and `.cpp`), called from `CassoEmuCore/Shell/MachineBuilder.cpp` for the Apple /// family: create the MMU, VIAs, `Cpu6502Apple3` and the bus wiring
- [ ] T023 Implement `DebugAddress` (address, space, bank; implicit conversion from `Word`; parse and print) in `CassoEmuCore/Debugger/DebugAddress.h` and `.cpp`
- [ ] T024 Add the `DebugAddress` forms of `TryPeek`, `TryPoke`, `TryPatch` and `GetRegion`, and the current-view resolver, to `CassoEmuCore/Debugger/IDebugTarget.h`, with Apple II implementations in `CassoEmuCore/Debugger/MachineDebugTarget.cpp` and `DebugMemoryView.cpp` that accept the current view only

**Checkpoint**: an Apple /// machine builds headless, maps memory and
resets into the ROM. The Apple II debugger tests pass with their
expectations as they were.

---

## Phase 3: User Story 1 - Boot SOS and run Business BASIC (Priority: P1)

**Goal**: a Business BASIC disk boots to its prompt and runs a typed program.

**Independent Test**: boot headless, type `10 PRINT 6*7` and `RUN`, compare
the screen text.

### Tests

- [ ] T025 [P] [US1] Write character set tests (loading through the documented mechanism changes glyphs; nothing else does; power-on contents) in `UnitTest/Apple3Tests/Apple3CharacterSetTests.cpp`
- [ ] T026 [P] [US1] Write text mode decoding tests for 40-column, 40-column color and 80-column, normal and inverse, in `UnitTest/Apple3Tests/Apple3TextModeTests.cpp`
- [ ] T027 [P] [US1] Write keyboard tests (code and flags for every key of the Apple /// layout, strobe, repeat, interrupt) in `UnitTest/Apple3Tests/Apple3KeyboardTests.cpp`
- [ ] T028 [P] [US1] Write disk controller tests for the built-in drive (select, motor, read latch, write) in `UnitTest/Apple3Tests/Apple3DiskControllerTests.cpp`
- [ ] T029 [US1] Write scenario tests (no-disk screen with an empty drive; Business BASIC prompt on 128K and 256K; `PRINT 6*7` prints 42; a missing disk or ROM fails the test) in `ScenarioTests/Apple3SosTests.cpp`

### Implementation

- [ ] T030 [P] [US1] Implement `Apple3CharacterSet` and its loader behind a glyph source interface in `CassoEmuCore/Machines/Apple3/Apple3CharacterSet.h` and `.cpp`
- [ ] T031 [US1] Implement `Apple3Video` (mode switches, display buffer, video enable, vertical blanking to the VIAs, beam position) in `CassoEmuCore/Machines/Apple3/Apple3Video.h` and `.cpp`
- [ ] T032 [P] [US1] Implement `Apple3TextMode`, `Apple3ColorTextMode` and `Apple380ColTextMode` in `CassoEmuCore/Machines/Apple3/`
- [ ] T033 [P] [US1] Implement `Apple3Keyboard` and add the `Apple3` layout to the keyboard layout tables in `CassoEmuCore/Machines/Apple3/Apple3Keyboard.h` and `.cpp`
- [ ] T034 [US1] List in `specs/050-apple3/research.md` under R8 every place `Disk2Controller` and its consumers assume two drives
- [ ] T035 [US1] Implement `Apple3DiskController` over `Disk2NibbleEngine` with the built-in drive working, in `CassoEmuCore/Machines/Apple3/Apple3DiskController.h` and `.cpp`
- [ ] T036 [US1] Wire video, keyboard, disk and power-on state into `CassoEmuCore/Shell/Apple3MachineBuilder.cpp` and the frame renderer in `CassoEmuCore/Video/MachineFrameRenderer.cpp`
- [ ] T037 [US1] Report a missing or wrong-size `Apple3.rom` through the existing missing-ROM path (FR-005) in `CassoEmuCore/Shell/Apple3MachineBuilder.cpp`
- [ ] T038 [US1] Boot the Business BASIC disk headless, fix what stops it, and get T029 passing

**Checkpoint**: SOS boots to Business BASIC.

---

## Phase 4: User Story 2 - Read and write disks in four drives (Priority: P1)

**Goal**: `.D1` to `.D4` read, write and format.

**Independent Test**: format a blank image in `.D2`, copy a file to it, read
it back with Casso's disk tools.

### Tests

- [ ] T039 [P] [US2] Extend `UnitTest/Apple3Tests/Apple3DiskControllerTests.cpp` for four drives: selection, an unconnected drive, write protect, and that selecting one drive does not affect the others' motors or lamps
- [ ] T040 [US2] Add scenario tests (volumes listed for all four drives; format; copy between drives and read back byte for byte; write-protect error; `.D3` with one external drive) to `ScenarioTests/Apple3SosTests.cpp`

### Implementation

- [ ] T041 [US2] Add the external drives and the Apple /// drive selection to `CassoEmuCore/Machines/Apple3/Apple3DiskController.cpp`, with the count from `externalDrives`
- [ ] T042 [US2] Change each two-drive assumption listed by T034 (event drive field, `DebugDiskState`, audio sources, mount and eject paths) in `CassoEmuCore/Machines/Apple2/Common/` and `CassoEmuCore/Debugger/`, with the Apple II behavior the same
- [ ] T043 [P] [US2] Add Drive 3 and Drive 4 to the disk menus, drop targets, drive widgets and `--disk3`, `--disk4` (errors on an Apple II machine) in `CassoEmuCore/Shell/` and `CassoEmuCore/Cli/`
- [ ] T044 [P] [US2] Give each of the four drives the Disk II drive sounds through `Disk2AudioSource` in `CassoEmuCore/Shell/Apple3MachineBuilder.cpp`
- [ ] T045 [US2] Make the boot disk picker work for the built-in drive (FR-017)

**Checkpoint**: four drives work under SOS.

---

## Phase 5: User Story 3 - See every video mode (Priority: P1)

**Goal**: all seven modes match MAME by color index.

**Independent Test**: `scripts/CompareMameFrame.ps1` per mode.

### Tests

- [ ] T046 [P] [US3] Write decoding tests for the four graphics modes (known buffer bytes to color indices) in `UnitTest/Apple3Tests/Apple3GraphicsModeTests.cpp`
- [ ] T047 [P] [US3] Write tests for gray levels in the monochrome styles, the color style, the video enable bit and smooth scroll in `UnitTest/Apple3Tests/Apple3VideoTests.cpp`

### Implementation

- [ ] T048 [P] [US3] Implement `Apple3HiResMode`, `Apple3ColorHiResMode`, `Apple3Color140Mode` and `Apple3HiRes560Mode` in `CassoEmuCore/Machines/Apple3/`
- [ ] T049 [US3] Implement smooth scroll and mode selection for all seven modes in `CassoEmuCore/Machines/Apple3/Apple3Video.cpp`
- [ ] T050 [US3] Add the 16-color palette and the 16 gray levels, applied by the color, green, amber and white styles (FR-021), in `CassoEmuCore/Machines/Apple3/Apple3Palette.h`
- [ ] T051 [US3] Add `AppleMonitorIII` with green as its phosphor to `CassoEmuCore/Config/MonitorCatalog.h` (its mesh arrives in Phase 12; until then it uses the Monitor II mesh)
- [ ] T052 [US3] Write `scripts/CompareMameFrame.ps1` (headless MAME snapshot, headless Casso frame at the same emulated time, color index mapping, differing pixel report)
- [ ] T053 [US3] Run T052 for each mode with a disk that draws in it; check each difference against the documents before changing code; store the results in `specs/050-apple3/validation/mame-frames.md`
- [ ] T054 [US3] Confirm screenshot capture and the CRT effects work with each mode (FR-023)

**Checkpoint**: every mode matches.

---

## Phase 6: User Story 6 - Choose and set up the machine (Priority: P1)

**Goal**: the machine is in the list, with its settings and no Apple II-only options.

**Independent Test**: set each option, restart, check SOS's report.

- [ ] T055 [US6] Get the owner's approval of the machine list, settings, menu and message strings in `specs/050-apple3/contracts/ui-strings.md` and mark each approved
- [ ] T056 [P] [US6] Write settings model tests (options offered per machine; Apple II-only options absent; values saved and applied on restart) in `UnitTest/UiTests/Apple3SettingsTests.cpp`
- [ ] T057 [US6] Add the Apple /// options (memory, external drives, slots, serial port, joystick ports) to the machine settings page in `CassoEmuCore/Ui/Settings/`
- [ ] T058 [US6] Audit every feature that depends on the machine type (menus, command line, debug channel, MCP adapter, screenshot capture, disk tools, game controllers, drive audio, cassette, printer, update check) and make each work or be disabled with "Not available on the Apple ///" (FR-035); list the audit in `specs/050-apple3/validation/feature-audit.md`
- [ ] T059 [P] [US6] Add `--machine Apple3` handling and drag-and-drop mounting to the built-in drive in `CassoEmuCore/Cli/` and `CassoEmuCore/Shell/`
- [ ] T060 [P] [US6] Extend the flat themes' drive and status widgets to four drives (FR-067) in `CassoEmuCore/Ui/Chrome/`

---

## Phase 7: User Story 7 - Debug an Apple /// program across banks (Priority: P1)

**Goal**: every debugger feature works with banks, a moved zero page and a moved stack.

**Independent Test**: a test program calls into another bank through
extended addressing with a bank-qualified breakpoint set.

Steps T061 to T069 migrate one layer each. After each one the Apple II
debugger tests pass with their expectations as they were.

### Tests

- [ ] T061 [P] [US7] Write bank-qualified stop tests (breakpoint, data breakpoint and watch in a bank; all-banks entries; 1,000 alternating executions for SC-007) in `UnitTest/DebuggerTests/BankBreakpointTests.cpp`
- [ ] T062 [P] [US7] Write command tests for the text form in each of the four modes (accept, print, the "illegal address" errors) in `UnitTest/DebuggerTests/BankAddressCommandTests.cpp`
- [ ] T063 [P] [US7] Write disassembly tests for the extended access annotation and for SOS calls in `UnitTest/DebuggerTests/Apple3DisassemblyTests.cpp`
- [ ] T064 [P] [US7] Write stack tests (stack pane, call stack, step over and step out across a zero page change and the alternate stack bit) in `UnitTest/DebuggerTests/Apple3StackTests.cpp`

### Implementation

- [ ] T065 [US7] Implement the `DebugAddress` target methods for the Apple /// (any bank, the system bank, ROM, with no side effects) in `CassoEmuCore/Debugger/MachineDebugTarget.cpp` and `DebugMemoryView.cpp`
- [ ] T066 [US7] Store `DebugAddress` in `BreakpointTable`, `WatchpointTable`, `WatchTable`, `SymbolTable`, `DataBlockTable` and bookmarks in `CassoEmuCore/Debugger/`, with the matching rule from `data-model.md`
- [ ] T067 [US7] Add the bank of the program counter and the `DebugAddress` of the data access to `Cpu::TraceEntry` in `CassoCore/Cpu.h` and `TraceRecord` in `CassoEmuCore/Debugger/Reply.h`, and raise the state format version with a load path for older states
- [ ] T068 [US7] Add numbered banks to `HeatSpace` and resolve them in `CassoEmuCore/Debugger/HeatBankMap.cpp`, the profiler and the heat map panes (FR-046)
- [ ] T069 [US7] Parse and print the text form in the command parsers and formatters of the Casso, Apple II Monitor, GSSquared and WinDbg modes, the debug channel JSON and the MCP adapter, per `contracts/debugger-addresses.md`
- [ ] T070 [US7] Add the bank column and bank selection to the memory windows, disassembly views, trace pane, breakpoints, watches, bookmarks, symbols and data blocks panes and every Go to box in `CassoEmuCore/Ui/Debugger/Panes/`, hidden on machines without banks
- [ ] T071 [US7] Add E, B and Z to the registers pane with bit tips and editing, and to the register commands of every mode (FR-040)
- [ ] T072 [US7] Make the stack pane, call stack, step over, step out and the stack stop rules use the stack page in use (FR-041) in `CassoEmuCore/Debugger/CallStack.cpp` and the stack pane
- [ ] T073 [US7] Add the bank, environment and zero page change breakpoints (FR-043) to `CassoEmuCore/Debugger/` and the Breakpoints pane
- [ ] T074 [US7] Add the machine helper to `CassoCore/Disassembler.h` and `.cpp`: the resolved access annotation (FR-039) and inline call formatting (FR-044)
- [ ] T075 [P] [US7] Write `SosCallNames` and `Apple3Symbols` (I/O addresses, VIA registers, documented ROM entry points) from the Apple documents in `CassoEmuCore/Machines/Apple3/`
- [ ] T076 [US7] Store and load `DebugAddress` in debugger projects, with older projects loading as current view (FR-047), in `CassoEmuCore/Debugger/Project/`
- [ ] T077 [US7] Bind a debug file's addresses to the bank a program loaded in for source-level debugging (FR-048) in `CassoEmuCore/Debugger/Source/`
- [ ] T078 [US7] Get the owner's approval of the debugger strings in `specs/050-apple3/contracts/ui-strings.md`

**Checkpoint**: the P1 stories are complete.

---

## Phase 8: User Story 4 - Type, hear and tell the time (Priority: P2)

**Goal**: full keyboard, the three sound paths, the clock and joysticks.

**Independent Test**: a test disk reads every key, drives each sound path,
reads the clock and both joystick ports.

- [ ] T079 [P] [US4] Write `Mm58167` tests (counters, latches, set and read, interrupts, host time through the input journal) in `UnitTest/EmuTests/Mm58167Tests.cpp`
- [ ] T080 [P] [US4] Write sound tests (speaker toggle, beeper tone and length, DAC samples and pitch at both speeds for SC-006) in `UnitTest/Apple3Tests/Apple3SoundTests.cpp`
- [ ] T081 [P] [US4] Write analog input tests (axes and buttons on ports A and B) in `UnitTest/Apple3Tests/Apple3AnalogInputsTests.cpp`
- [ ] T082 [P] [US4] Implement `Mm58167` in `CassoEmuCore/Devices/Mm58167.h` and `.cpp` and wire its address lines and interrupt through `Apple3SystemVias`
- [ ] T083 [P] [US4] Implement `Apple3Sound` (speaker through `AppleSpeaker`, beeper, DAC, mixing) and its audio source in `CassoEmuCore/Machines/Apple3/Apple3Sound.h` and `.cpp`
- [ ] T084 [P] [US4] Implement `Apple3AnalogInputs` over 034's controller assignments in `CassoEmuCore/Machines/Apple3/Apple3AnalogInputs.h` and `.cpp`
- [ ] T085 [US4] Complete the keyboard: PC key mappings for every Apple /// key in the settings key list, repeat rates, pasted text (FR-025), and Control with Reset obeying the environment register's reset bit
- [ ] T086 [US4] Run the Confidence Test disk in the scenario suite (SC-002) in `ScenarioTests/Apple3SosTests.cpp`
- [ ] T087 [US4] Write a timing test program and check SC-005 at 2 MHz and 1 MHz in `UnitTest/Apple3Tests/Apple3TimingTests.cpp`

---

## Phase 9: User Story 5 - Use the built-in serial port (Priority: P2)

- [ ] T088 [P] [US5] Write a loopback test through `.RS232` at each baud rate in `ScenarioTests/Apple3SosTests.cpp` and a wiring test in `UnitTest/Apple3Tests/Apple3SerialTests.cpp`
- [ ] T089 [US5] Create the `Acia6551` at its documented address with its interrupt wired, and attach it through the `serial` port configuration, in `CassoEmuCore/Shell/Apple3MachineBuilder.cpp`
- [ ] T090 [US5] Offer every attachment the Super Serial Card offers for the built-in port in settings, and label its traffic "Built-in" in the serial log (FR-053)

---

## Phase 10: User Story 8 - Inspect the Apple ///'s devices (Priority: P2)

**Goal**: the twelve panels of `contracts/device-panels.md`, three new visuals and the logs.

- [ ] T091 [P] [US8] Write panel tests: set each device to a known state and compare every row, per panel id, in `UnitTest/DebuggerTests/Apple3PanelTests.cpp`; include that no Apple /// panel is listed on an Apple II machine
- [ ] T092 [US8] Implement `IDiagnosticsProvider` on `Apple3Mmu`, `Apple3SystemVias` (one panel per VIA, port bits labeled by function), `Apple3Video`, `Apple3CharacterSet`, `Apple3Keyboard`, `Apple3DiskController`, `Apple3Sound`, `Mm58167`, `Apple3AnalogInputs` and `Apple3Slots`
- [ ] T093 [US8] Add the character set, DAC trace and page source payloads to `CassoEmuCore/Debugger/DiagnosticsSnapshot.h`
- [ ] T094 [P] [US8] Implement `CharacterSetView` (glyph grid, code under the pointer) in `CassoEmuCore/Ui/Debugger/Panes/CharacterSetView.h` and `.cpp`
- [ ] T095 [P] [US8] Implement `DacMeterView` (meter and waveform) in `CassoEmuCore/Ui/Debugger/Panes/DacMeterView.h` and `.cpp`
- [ ] T096 [US8] Add banks, I/O and write-protected RAM to `CassoEmuCore/Ui/Debugger/Panes/MemoryMapBar.cpp` and its legend (FR-042)
- [ ] T097 [US8] Add the new graphic kinds to `CassoEmuCore/Ui/Debugger/Panes/DiagnosticsPane.h` and `.cpp`
- [ ] T098 [US8] Extend the disk event log, the drive filter, the disk head graphic and disk breakpoints to four drives (FR-051, FR-055)
- [ ] T099 [P] [US8] Add the Apple /// keyboard and joystick reads to the input log (FR-052) and the Apple /// function labels to the I/O log (FR-054)
- [ ] T100 [US8] Measure that every panel updates within one frame (SC-008)

---

## Phase 11: User Story 9 - Rewind an Apple /// session (Priority: P2)

- [ ] T101 [P] [US9] Write save and load round-trip tests for every new device and the MMU in `UnitTest/Apple3Tests/Apple3StateTests.cpp`
- [ ] T102 [US9] Write a history test: store a SOS boot, seek to 100 random cycles, compare the full state hash with the live run at each (SC-009), in `ScenarioTests/Apple3HistoryTests.cpp`
- [ ] T103 [US9] Register every bank with history's page diff and keyframes, and add every new device to the machine state list, in `CassoEmuCore/Shell/Apple3MachineBuilder.cpp`
- [ ] T104 [US9] Route the clock chip's host time and the analog inputs through `InputJournal` (FR-057)
- [ ] T105 [US9] Rebuild the stack and call stack panes for the stack in use after a seek, and report banks in the rebuilt trace, heat map and event logs

---

## Phase 12: User Story 10 - See the Apple /// on the desk (Priority: P2)

**Goal**: measured models and the stacked arrangement. Independent of
Phases 3 to 11; can start after T001.

- [ ] T106 [US10] Resolve the height discrepancy between Apple's stated size and the patent drawings (see `contracts/desk-models.md`), then fill the dimension tables of `specs/050-apple3/contracts/desk-models.md` from the Service Reference Manual and Owner's Guides; where a dimension is in no Apple document, measure it from photographs as a fraction of a documented one and mark it `measured`; tell the owner which dimensions are measured
- [ ] T107 [P] [US10] Write `scripts/modelgen/cad_apple3.py` (body ending at the front of the upper body, drive bay with door, latch and lamp in the existing palette colors, back panel with every connector at its position, badge variants) and `Resources/Models/Apple3/README.md` with sources
- [ ] T108 [P] [US10] Write `scripts/modelgen/cad_monitor3.py` (case, spherical-sag glass, anti-glare mesh, controls, lamp, brand stamp) and `Resources/Models/Monitor3/README.md`
- [ ] T109 [P] [US10] Write `scripts/modelgen/cad_disk3.py` and `Resources/Models/Disk3/README.md`
- [ ] T110 [US10] Render a preview of each model with `scripts/modelgen/preview.py`, check each mesh for bad fillets, and send the previews to the owner before wiring
- [ ] T111 [US10] Copy the generated meshes to `Resources/Models/`, add them to the `BakedMesh` list in `Casso/Casso.vcxproj` and the `RCDATA` lines in `Casso/Casso.rc`, and update `scripts/modelgen/README.md`
- [ ] T112 [P] [US10] Write layout tests for the stack and fit rules of `contracts/desk-models.md` (each arrangement; containment by the camera; side row clear of the stack) in `UnitTest/UiTests/DeskSceneStackTests.cpp`
- [ ] T113 [US10] Add `Apple3Body`, `Monitor3` and `Disk3` to `DeskDeviceKind` and the model loader in `CassoEmuCore/Ui/Scene/DeskSceneModel.h` and `.cpp`
- [ ] T114 [US10] Add the stack to `CassoEmuCore/Ui/Scene/DeskSceneLayout.h` and `.cpp`: body, optional ProFile, monitor, with the side row for other drives
- [ ] T115 [US10] Give the built-in drive and the Disk /// the lamp, door, drop and eject interactions (FR-062, FR-063) in `CassoEmuCore/Ui/Scene/DeskSceneHitTester.cpp` and the scene's drive state
- [ ] T116 [US10] Point `AppleMonitorIII` in `CassoEmuCore/Config/MonitorCatalog.h` at the Monitor /// mesh, with the emulated screen on its tube
- [ ] T117 [US10] Ask the owner, then launch Casso with `--title` on the portrait monitor, capture each arrangement and compare with the reference photographs

---

## Phase 13: User Story 13 - Run an Apple /// Plus (Priority: P2)

- [ ] T118 [P] [US13] Write Plus tests (256K only; every Plus key's code; interlace decoding) in `UnitTest/Apple3Tests/Apple3PlusTests.cpp`
- [ ] T119 [US13] Implement `Apple3Plus` in `CassoEmuCore/Machines/Apple3/Apple3Plus.h`, register it, and add `Resources/Machines/Apple3Plus/Apple3Plus.json`
- [ ] T120 [P] [US13] Add the `Apple3Plus` keyboard layout and its PC key mappings
- [ ] T121 [US13] Implement text interlace (560 by 384) in `CassoEmuCore/Machines/Apple3/Apple3Video.cpp`, turned on by a case switch on the Plus through the existing case switch mechanism (`IMachine::HasCaseSwitches`) and its setting
- [ ] T122 [US13] Use the Plus badge variant of the body model for the Plus (FR-073) and add a Plus boot to `ScenarioTests/Apple3SosTests.cpp`

---

## Phase 14: User Story 14 - Debug a Business BASIC program at source level (Priority: P2)

Depends on Phase 7.

- [ ] T123 [US14] Find the interpreter's pointers from Apple's Business BASIC source listing in the docs folder, and match each release on the fetched disks to it by signature and by observing it in Casso's debugger; write the releases, signatures and pointers in `specs/050-apple3/research.md` under R19
- [ ] T124 [US14] Factor the interface the BASIC panes and `BasicStopMonitor` use out of `ApplesoftProgram` in `CassoEmuCore/Debugger/Basic/`, if they use the class directly, with the Applesoft tests passing as they were
- [ ] T125 [P] [US14] Write Business BASIC tests (listing as `LIST` prints it, every variable type including long integers, arrays, GOSUB and FOR frames, an unknown release reported) from captured memory images in `UnitTest/DebuggerTests/BusinessBasicProgramTests.cpp`
- [ ] T126 [US14] Implement `BusinessBasicProgram` (token table from the Reference Manual, release identification, lines, variables, arrays, frames) in `CassoEmuCore/Debugger/Basic/BusinessBasicProgram.h` and `.cpp`
- [ ] T127 [US14] Add Business BASIC statement and line stepping, line and variable breakpoints and the error stop to `CassoEmuCore/Debugger/Basic/BasicStopMonitor.cpp`
- [ ] T128 [US14] Select the BASIC dialect by machine and interpreter in the BASIC panes, the commands of every mode and the debug channel (FR-076)
- [ ] T129 [US14] Add a scenario test: type the ten-line program of the story's Independent Test, break on line 40, check variables and stack, step a statement

---

## Phase 15: User Story 12 - Run Apple II software in emulation mode (Priority: P3)

- [ ] T130 [P] [US12] Write lockout tests (set by the documented write; register writes ignored afterward; survives reset; cleared by power-off; the Apple II mapping) in `UnitTest/Apple3Tests/Apple3EmulationModeTests.cpp`
- [ ] T131 [US12] Implement the lockout and the Apple II mapping in `CassoEmuCore/Machines/Apple3/Apple3Mmu.cpp`
- [ ] T132 [US12] Select the Apple II text, lo-res and hi-res classes in emulation mode, with the text mode reading glyphs from `Apple3CharacterSet`, in `CassoEmuCore/Machines/Apple3/Apple3Video.cpp`
- [ ] T133 [US12] Produce Apple II keyboard codes, speaker, paddle and disk behavior at their Apple II addresses in emulation mode (FR-031b)
- [ ] T134 [US12] Make the debugger follow the mode: machine info, symbols, memory map, the lockout row, and history storing the lockout (FR-031d)
- [ ] T135 [US12] Add a scenario test: boot the Apple II Emulation disk, start emulation, boot the DOS 3.3 System Master, draw in lo-res and hi-res, compare frames with the ][+ machine

---

## Phase 16: User Story 11 - Use a ProFile on the Apple /// (Priority: P3)

Blocked until a merge from `035-debugger` includes 048.

- [ ] T136 [US11] Merge `035-debugger` once it contains 048; confirm `ProFileDrive`, `ProFileCard` and the ProFile models are present
- [ ] T137 [P] [US11] Write tests for the ProFile interface card in an Apple /// slot (handshake, read, write, both sizes) in `UnitTest/Apple3Tests/Apple3ProFileTests.cpp`
- [ ] T138 [US11] Implement `Apple3Slots` card support and the Apple /// ProFile interface card over 048's `ProFileDrive` in `CassoEmuCore/Machines/Apple3/`
- [ ] T139 [US11] Offer the card in slot settings, attach it by default in slot 4 in both machine files, and add the ProFile placement setting (on the computer, to the side, hidden)
- [ ] T140 [US11] Place 048's ProFile models in the stack, check the fit rules against the body's measured top (FR-065), and model the card's connector in its slot opening (FR-060)
- [ ] T141 [US11] Add a scenario test: boot SOS with `.PROFILE`, format, copy files, read them back from the image, for 5 MB and 10 MB

---

## Phase 17: Polish and merge

- [ ] T142 Run the quickstart's in-app checks with the owner's go-ahead for anything on screen, and fix what they find
- [ ] T143 Confirm Apple II emulation speed is within measurement noise of the speed before the feature (SC-011) and store the numbers in `specs/050-apple3/validation/performance.md`
- [ ] T144 [P] Document the Apple /// in `docs/` (machine, settings, debugger addresses) and update the agent doc for `CassoCli --attach` with the address text form
- [ ] T145 Write the `CHANGELOG.md` and `README.md` entries last and get the owner's approval of the text
- [ ] T146 Merge `035-debugger`, run `scripts/CheckStyle.ps1 -Mode Tree`, then `scripts/RunMergeGate.ps1`, Dormann and full-depth Harte
- [ ] T147 Run `/checkpoint` to reconcile spec, plan and tasks with what was built

---

## Dependencies & Execution Order

### Phase dependencies

- Phase 1 blocks everything. T002 gates all device code.
- Phase 2 blocks every story.
- Phase 3 (US1) blocks Phases 4, 5, 8, 9, 11, 13, 14 and 15.
- Phase 7 (US7) depends on Phase 2 only for T061 to T070 and can run beside
  Phases 3 to 6; T071 onward use a booted machine.
- Phase 10 depends on Phases 7 and 8. Phase 11 depends on Phase 8.
- Phase 12 depends on T001 only.
- Phase 14 depends on Phases 3 and 7.
- Phase 16 depends on the 048 merge and on Phase 12 for the fit check.
- Phase 17 is last.

### Within a story

Tests first, confirmed failing; then devices; then wiring; then the scenario
check.

### Parallel opportunities

- T004 to T006; T008 to T013; the test tasks at the head of each story.
- Phase 12 beside any other phase.
- Phase 7's migration steps beside Phases 3 to 6, by one person at a time
  per file.

## Implementation Strategy

The feature ships whole, in one merge to master. There is no reduced first
release. The phase checkpoints are for validation along the way: the ROM's
diagnostics screen, the SOS boot, four drives, the seven modes, then the
debugger, then the rest. Work serially in this checkout unless the owner
says otherwise.

## Notes

- Apple's documents come first. An existing emulator is consulted only for a
  point the documents do not settle, for the fact and never for code, under
  the limits in research.md R1. Never commit Apple's software.
- Ask the owner before launching Casso or MAME on screen; every Casso launch
  has `--title`.
- A test that cannot find its ROM or disk fails.
- Commit after each task or small group; push freely on this branch.
