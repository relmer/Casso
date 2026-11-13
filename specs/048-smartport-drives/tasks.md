---

description: "Task list for spec 048, SmartPort and hard drive devices"
---

# Tasks: SmartPort and hard drive devices

**Input**: Design documents from `specs/048-smartport-drives/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: Tests first, per the constitution's testing discipline. Each
implementation task is preceded by the test task that covers it, and each
test is checked to fail before the code exists (stub the implementation and
confirm the test goes red). A test without its ROM or image fails;
it never skips.

**Organization**: Phases follow the plan's delivery order, not the spec's
story numbering: the SCSI card comes first because MAME has reference boots
for it and spec 033's T573 runs on it, and the stacks come before the UI that
configures them. Each story phase ends at a boot a scenario test checks.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: The spec's user story (US1 to US7)

## Path conventions

`CassoEmuCore/Devices/BlockDevice/` (BD), `CassoEmuCore/Devices/Floppy35/`
(F35), `UnitTest/BlockDeviceTests/` (UT-BD), `UnitTest/Floppy35Tests/`
(UT-F35), `ScenarioTests/`. Paths below are written in full.

---

## Phase 1: Setup

**Purpose**: Folders, ROM fetch, project files.

- [ ] T001 Create `CassoEmuCore/Devices/BlockDevice/`, `CassoEmuCore/Devices/Floppy35/`, `CassoEmuCore/Machines/Apple2/Apple2cPlus/`, `UnitTest/BlockDeviceTests/` and `UnitTest/Floppy35Tests/`, and add filters for them to `CassoEmuCore/CassoEmuCore.vcxproj.filters` and `UnitTest/UnitTest.vcxproj.filters`
- [ ] T002 Add the catalog rows of `contracts/firmware-fetch.md` to `scripts/FetchRoms.ps1` with a `Sha1` column verified after download (a mismatch deletes the file and fails the fetch), and add the test subset to `-Fixtures`
---

## Phase 2: Foundational

**Purpose**: The media layer, event feeds, registry and config plumbing that
every device uses. No story starts before this phase is done.

### Media

- [ ] T003 [P] Tests in `UnitTest/BlockDeviceTests/BlockDeviceMediaTests.cpp`: detect `.po`, `.hdv`, ProDOS-order `.2mg`, DiskCopy 4.2 and WOZ 2 3.5-inch by header then extension; fail the attach of a DOS-order 2IMG and of a size over the drive's capacity ("At most 65,535 (32 MB) per ProDOS unit; at most the ProFile model's capacity"); report the 2IMG locked flag and a read-only host file as write protection with the Disk II's wording; fail a second attach of one path ("One writer per path across all drives (FR-018)"); and confirm the file is byte-identical after every failed attach. All over in-memory files through `IDiskFileIo`
- [ ] T004 Implement `BlockDeviceMedia` in `CassoEmuCore/Devices/BlockDevice/BlockDeviceMedia.h/.cpp` over spec 033's `CachingBlockStore`, `FileBlockStore` and `OffsetBlockStore`
- [ ] T005 [P] Tests in `UnitTest/Floppy35Tests/DiskCopy42ImageTests.cpp`: read the header, data and tag checksums of a DiskCopy 4.2 image; write blocks and recompute both checksums; fail the attach of an image with a bad checksum as damaged media
- [ ] T006 Implement `DiskCopy42Image` in `CassoEmuCore/Devices/Floppy35/DiskCopy42Image.h/.cpp` as an `IBlockStore`

### Event feeds

- [ ] T007 [P] Tests in `UnitTest/BlockDeviceTests/BlockDeviceEventRingTests.cpp` and `UnitTest/Floppy35Tests/Floppy35EventRingTests.cpp`: single-producer ring order, overflow counting, and no cost with no sink attached, modeled on `Disk2EventRingTests.cpp`
- [ ] T008 Add `IBlockDeviceEventSink.h`, `BlockDeviceEvent.h` and `BlockDeviceEventRing.h/.cpp` in `CassoEmuCore/Devices/BlockDevice/`, and `IFloppy35EventSink.h`, `Floppy35Event.h` and `Floppy35EventRing.h/.cpp` in `CassoEmuCore/Devices/Floppy35/`, with the fields of `contracts/device-events.md`
- [ ] T009 Add both sinks to `MachineHost::AttachObservers` in `CassoEmuCore/Shell/MachineHost.h/.cpp`, with a test in `UnitTest/EmuTests/MachineHostObserverTests.cpp`

### Registry and configuration

- [ ] T010 [P] Tests in `UnitTest/EmuTests/MachineConfigTests.cpp`: parse every slot and port form in `contracts/machine-config.md` (SCSI IDs as ports, `chain`, `realTiming` defaulting to `false`); fail to load a device a machine cannot host, with an error that lists the machines that accept it and the slot empty; resolve card ROMs under `Devices/<Card>/`
- [ ] T011 Extend `MachineConfig` in `CassoEmuCore/Core/MachineConfig.h/.cpp` with the card and drive entries, a per-device host-machine list, and ROM resolution
- [ ] T012 [P] Tests in `UnitTest/EmuTests/MachineConfigUpgradeTests.cpp`: a bumped `$cassoMachineVersion` adds nothing to a user copy's configured slots
- [ ] T013 Update `MachineConfigUpgrade` in `CassoEmuCore/Core/MachineConfigUpgrade.h/.cpp`
- [ ] T014 Add constants for the device ids (`apple-scsi-card`, `hd20sc`, `scsi-disk`, `profile-card`, `profile-5mb`, `profile-10mb`, `liron-card`, `unidisk35`, `apple35-card`, `apple35-drive`, `disk-iic`) in `CassoEmuCore/Core/MachineConfig.h`, and display labels in sentence case in `CassoEmuCore/Ui/Settings/SettingsJson.cpp`
- [ ] T015 [P] Add a missing-ROM path test in `UnitTest/EmuTests/ComponentRegistryTests.cpp`: a card whose ROM is absent reports the file and the folders searched, and the card does not run (FR-016)
- [ ] T016 Add a `TestMachine` helper in `UnitTest/EmuTests/TestMachine.h/.cpp` that puts a card in a slot over in-memory images through `Customize`, for the scenario tests and for spec 033's T573 (FR-025)

**Checkpoint**: Media, feeds and config are in place. Device stacks can start.

---

## Phase 3: User Story 1, Apple II SCSI Card (Priority: P1)

**Goal**: A hard disk boots on the SCSI card and matches MAME's boot.

**Independent test**: Boot every image of spec 033's MAME table on the SCSI
card in Casso.

- [ ] T017 [P] [US1] Tests in `UnitTest/BlockDeviceTests/Ncr5380Tests.cpp`: every register's read and write (current data, initiator command, mode, target command, bus status, bus and status, input data, reset parity and interrupts), arbitration, selection with and without a responding target, the phase-match bit, and the DMA and pseudo-DMA handshakes, against the NCR 5380 data sheet
- [ ] T018 [US1] Implement `Ncr5380` in `CassoEmuCore/Devices/BlockDevice/Ncr5380.h/.cpp` and `ScsiBus` in `CassoEmuCore/Devices/BlockDevice/ScsiBus.h/.cpp` with the phase machine of `data-model.md` ("Selection of an absent ID times out back to `BusFree`")
- [ ] T019 [P] [US1] Tests in `UnitTest/BlockDeviceTests/ScsiHardDiskTests.cpp`: TEST UNIT READY, REQUEST SENSE (sense cleared after), INQUIRY with the HD20SC's strings, READ(6), READ(10), WRITE(6), WRITE(10), READ CAPACITY and MODE SENSE; a read past the end sets ILLEGAL REQUEST; a write to protected media sets DATA PROTECT; real timing off completes at once and on adds spin-up and seek delays (research R15)
- [ ] T020 [US1] Implement `ScsiHardDisk` in `CassoEmuCore/Devices/BlockDevice/ScsiHardDisk.h/.cpp` over `BlockDeviceMedia` ("ID 7 is the card. Unique per card"), with spin-up and seek events
- [ ] T021 [P] [US1] Tests in `UnitTest/BlockDeviceTests/AppleScsiCardTests.cpp`: ROM and RAM banking at `$C800-$CFFF` through the bank register, the 5380 at `$C0n0-$C0n7`, I/O SELECT and I/O STROBE expansion ROM arbitration through `IExpansionRomCard`, and a firmware run in a test machine that issues INQUIRY to each ID
- [ ] T022 [US1] Implement `AppleScsiCard` in `CassoEmuCore/Devices/BlockDevice/AppleScsiCard.h/.cpp` (research R5), register `apple-scsi-card`, `hd20sc` and `scsi-disk`
- [ ] T023 [US1] Scenario tests in `ScenarioTests/BlockDeviceBootTests.cpp`: each image of spec 033's MAME table boots on the SCSI card in slot 7 to the same screen; images at two IDs are both online; with no image attached the boot moves to the next slot
- [ ] T024 [US1] Run each image in MAME with `scripts/MameSnapshot.ps1` and Casso side by side, and record the comparison in `specs/048-smartport-drives/validation.md` (FR-026, SC-001)

**Checkpoint**: The SCSI card boots everything MAME boots.

---

## Phase 4: User Story 5, ProFile (Priority: P3)

**Goal**: A 5 MB or 10 MB ProFile boots ProDOS.

- [ ] T025 [P] [US5] Tests in `UnitTest/BlockDeviceTests/ProFileDriveTests.cpp`: the handshake states of `data-model.md` (`Idle -> CommandWait ($01) -> CommandReceive (6 bytes) -> ReadyForData ($02 or $03) -> Transfer (532 bytes) -> Status ($04, 4 bytes) -> Idle`), "512-byte blocks; the 20 tag bytes are zeros on read and discarded on write", block counts 9,728 and 19,456, a too-large image fails to attach with the model's capacity in the error
- [ ] T026 [US5] Implement `ProFileDrive` in `CassoEmuCore/Devices/BlockDevice/ProFileDrive.h/.cpp`
- [ ] T027 [P] [US5] Tests in `UnitTest/BlockDeviceTests/ProFileCardTests.cpp`: the card's handshake lines (BSY, STRB, CMD, R/W, PARITY) and its ROM at `$Cn00`, with 341-0271-A for the 5 MB model and 341-0299-B for the 10 MB model
- [ ] T028 [US5] Implement `ProFileCard` in `CassoEmuCore/Devices/BlockDevice/ProFileCard.h/.cpp`, register `profile-card`, `profile-5mb` and `profile-10mb`
- [ ] T029 [US5] Scenario test in `ScenarioTests/BlockDeviceBootTests.cpp`: a bootable 5 MB image boots on a //e with the ProFile card, and a written file round-trips

---

## Phase 5: User Story 3, Apple 3.5 Drive on the 3.5 controller card (Priority: P2)

**Goal**: An Apple 3.5 Drive boots 800K images and protected WOZ disks.

### IWM, one implementation

- [ ] T030 [US3] Run the full Disk II test set (`RunTests.ps1 -Filter Disk2` and the scenario suite) and record the pass count as the baseline for the factor-out
- [ ] T031 [P] [US3] Tests in `UnitTest/Floppy35Tests/IwmTests.cpp`: the mode register bits (latch, asynchronous handshake, bit cell, timer, fast clock), status and handshake registers, the read and write data paths in both bit-cell sizes, and the phase and motor switches, against Apple's IWM specification, driven through a fake `IDiskDriveBus`
- [ ] T032 [US3] Factor `Iwm` out of `Disk2Controller`'s IWM mode into `CassoEmuCore/Devices/Floppy35/Iwm.h/.cpp` with `IDiskDriveBus` in `CassoEmuCore/Devices/Floppy35/IDiskDriveBus.h`, and route the //c's internal drive through it; the full Disk II baseline from T030 passes

### Sony mechanism and media

- [ ] T033 [P] [US3] Tests in `UnitTest/Floppy35Tests/Sony35DriveTests.cpp`: every status line through CA0-CA2 and SEL, stepping 0 to 79, side select, zone per track ("`track / 16`; sets the rotation speed and sectors per track"), eject after the eject travel time, disk-in-place and write-protect sense
- [ ] T034 [US3] Implement `Sony35Drive` in `CassoEmuCore/Devices/Floppy35/Sony35Drive.h/.cpp` with a floppy event per operation
- [ ] T035 [P] [US3] Tests in `UnitTest/Floppy35Tests/Gcr35TrackTests.cpp`: an 800K block image builds tracks with 12, 11, 10, 9 and 8 sectors per zone and 2:1 interleave; decoding the built tracks returns the blocks; a write to a built track writes the decoded blocks back; a 400K single-sided image builds side 0 only
- [ ] T036 [US3] Implement `Gcr35Track` in `CassoEmuCore/Devices/Floppy35/Gcr35Track.h/.cpp`
- [ ] T037 [P] [US3] Tests in `UnitTest/Floppy35Tests/Woz35Tests.cpp`: a WOZ 2 3.5-inch image loads per track and side with its bit counts, writes change its bits, and the INFO chunk's write protection is honored
- [ ] T038 [US3] Extend `WozLoader` in `CassoEmuCore/Machines/Apple2/Common/WozLoader.h/.cpp` for 3.5-inch disk type, and add `Floppy35Media` in `CassoEmuCore/Devices/Floppy35/Floppy35Media.h/.cpp`

### SWIM and the card

- [ ] T039 [P] [US3] Tests in `UnitTest/Floppy35Tests/SwimTests.cpp`: IWM mode matches `Iwm`; the mode switch sequence; ISM register probing as 341-0438-A performs it (research R8)
- [ ] T040 [US3] Implement `Swim` in `CassoEmuCore/Devices/Floppy35/Swim.h/.cpp` over `Iwm`
- [ ] T041 [P] [US3] Tests in `UnitTest/Floppy35Tests/DeviceCpuTests.cpp`: a `Cpu65C02` over a separate `MemoryBus` steps at its clock ratio against the host cycle count, parks while inactive ("`Parked` while the device is inactive, `Running` while active"), and resumes on activity
- [ ] T042 [US3] Implement `DeviceCpu` in `CassoEmuCore/Devices/Floppy35/DeviceCpu.h/.cpp`
- [ ] T043 [P] [US3] Tests in `UnitTest/Floppy35Tests/Apple35ControllerCardTests.cpp`: the card's memory map (its 65C02, 8 KB RAM, 32 KB ROM, SWIM, the window shared with the host), the host-side slot page, and a firmware run to a block read
- [ ] T044 [US3] Implement `Apple35ControllerCard` in `CassoEmuCore/Devices/Floppy35/Apple35ControllerCard.h/.cpp`, register `apple35-card` and `apple35-drive`, //e and enhanced //e only
- [ ] T045 [US3] Scenario tests in `ScenarioTests/Floppy35BootTests.cpp`: an 800K `.po`, a DiskCopy 4.2 image and a WOZ 2 3.5-inch image boot; a written file round-trips with DiskCopy checksums updated; record against MAME's `a2superdrive` in `validation.md`
- [ ] T046 [US3] Find a copy-protected 3.5-inch WOZ image (4am's archive), confirm it boots in MAME, and add a scenario boot for it against `%USERPROFILE%\Downloads\Casso` (SC-004)

---

## Phase 6: User Story 2, UniDisk 3.5 on the //c and the Liron card (Priority: P2)

**Goal**: A UniDisk 3.5 boots on the //c disk port and on the Liron card, and
chains.

- [ ] T047 [P] [US2] Tests in `UnitTest/Floppy35Tests/SmartPortBusTests.cpp`: REQ on phase 0, bus enable on phases 1 and 3, ACK on the sense line, packet framing (sync, `$C3`, 7-to-8 groups, checksum, `$C8`), INIT assigning IDs down the chain, and reset clearing them (research R10)
- [ ] T048 [US2] Implement `SmartPortBus` and `SmartPortChain` in `CassoEmuCore/Devices/Floppy35/SmartPortBus.h/.cpp`
- [ ] T049 [P] [US2] Tests in `UnitTest/Floppy35Tests/UniDisk35DriveTests.cpp`: the drive's memory map from the a2-liron disassembly (2 KB RAM, gate array at `$0800-$0801`, IWM at `$0A00-$0A0F`, 8 KB ROM at `$E000`), the gate array's bits in each direction, and a firmware run that replies to a SmartPort STATUS packet and a READ BLOCK
- [ ] T050 [US2] Implement `UniDisk35Drive` in `CassoEmuCore/Devices/Floppy35/UniDisk35Drive.h/.cpp`: `DeviceCpu` at 2 MHz, gate array, internal `Iwm`, `Sony35Drive`, on the chain
- [ ] T051 [P] [US2] Tests in `UnitTest/Floppy35Tests/LironCardTests.cpp`: the card's ROM, its IWM, and its slot page as `liron-if.asm` documents
- [ ] T052 [US2] Implement `LironCard` in `CassoEmuCore/Devices/Floppy35/LironCard.h/.cpp`, register `liron-card` and `unidisk35`
- [ ] T053 [US2] Connect the //c disk port to a `SmartPortChain` that ends in an optional 5.25-inch drive (`disk-iic`), in `CassoEmuCore/Machines/Apple2/Apple2c/` and `MachineConfig`
- [ ] T054 [US2] Scenario tests in `ScenarioTests/Floppy35BootTests.cpp`: an 800K image boots on a //c with a UniDisk 3.5 on the disk port; the same image boots on a //e from the Liron card in slot 5; two chained UniDisks are online; a 5.25-inch drive at the chain's end boots; a SmartPort eject call ejects the disk

---

## Phase 7: User Story 4, //c Plus (Priority: P2)

**Goal**: The //c Plus boots from its internal 3.5-inch drive at 4 MHz.

- [ ] T055 [P] [US4] Tests in `UnitTest/EmuTests/Apple2cPlusTests.cpp`: ROM 05 banking, the MIG RAM window, the internal Apple 3.5 Drive on the IWM, and the external port chain accepting `apple35-drive`, `unidisk35` and `disk-iic`
- [ ] T056 [US4] Implement `Apple2cPlus` in `CassoEmuCore/Machines/Apple2/Apple2cPlus/Apple2cPlus.h/.cpp` and `Resources/Machines/Apple2cPlus/Apple2cPlus.json`, and register it in the machine catalog
- [ ] T057 [P] [US4] Tests in `UnitTest/EmuTests/Apple2cPlusAcceleratorTests.cpp`: a timing loop runs at the accelerated speed with acceleration on and at 1 MHz with it off; disk, speaker and I/O accesses run at 1 MHz; Esc at power-on turns acceleration off (research R12)
- [ ] T058 [US4] Implement the accelerator in `CassoEmuCore/Machines/Apple2/Apple2cPlus/Apple2cPlusAccelerator.h/.cpp` within the existing CPU speed modes
- [ ] T059 [US4] Scenario tests in `ScenarioTests/Apple2cPlusTests.cpp`: an 800K image boots from the internal drive and from an external Apple 3.5 Drive; record against MAME's `apple2cp` in `validation.md` (SC-005a)

---

## Phase 8: User Story 6, configure cards and attach images (Priority: P1)

**Goal**: Every card and drive is reachable from the Machine tab, the desk
and the command line, and persists.

- [ ] T060 [P] [US6] Tests in `UnitTest/Settings/HardwarePageModelTests.cpp`: the device tree lists each card with its ports (SCSI IDs 0-6, chain positions), offers a card only on a machine that hosts it with a tip that lists the machines that do, confirms before replacing a card in a filled slot, and limits the //c chain to its firmware's length
- [ ] T061 [US6] Extend the device tree in `CassoEmuCore/Ui/Settings/HardwarePage.h/.cpp` and `SettingsPanelState` with cards, drives, image attach, and the per-drive real timing switch
- [ ] T062 [P] [US6] Tests in `UnitTest/EmuTests/CommandLineTests.cpp`: `--hd <slot>=`, `--hd <slot>.<unit>=` and `--smartport <n>=` per `contracts/command-line.md`, each error in the fixed error layout, and the saved configuration untouched after the run
- [ ] T063 [US6] Implement the flags in core beside `--disk1`
- [ ] T064 [P] [US6] Tests: dropping an image on a drive attaches it with the same validation as the Machine tab (FR-017), in `UnitTest/Ui/DriveWidgetStateTests.cpp`
- [ ] T065 [US6] Extend `DriveWidgetState` and `IDriveCommandSink` in `CassoEmuCore/Ui/` for block devices and 3.5-inch drives
- [ ] T066 [US6] Persistence test: configure every card, quit, relaunch through `MachineConfig` round trip, and compare

---

## Phase 9: User Story 7, see, hear and watch every drive (Priority: P2)

**Goal**: Each external drive is a realistic model with the Disk II's
interaction, sounds and events.

### Models

- [ ] T067 [P] [US7] Write `scripts/modelgen/cad_unidisk35.py` from measurements of reference photographs (proportions written in the script), render with `preview.py`, and check the render before wiring
- [ ] T068 [P] [US7] Write `scripts/modelgen/cad_apple35.py` the same way, with a //c Plus color variant for its internal drive
- [ ] T069 [P] [US7] Write `scripts/modelgen/cad_hd20sc.py` the same way
- [ ] T070 [P] [US7] Write `scripts/modelgen/cad_profile.py` in CadQuery, replacing `gen_profile.py`, and update the table in `scripts/modelgen/README.md`
- [ ] T071 [US7] Check every model color against the `DeskSceneModel` palette ("at least a channel clear of the palette" per the modelgen README), bake them in `Resources/Models/`, and add each to the `BakedMesh` list in `Casso/Casso.vcxproj` and `Casso.rc`
- [ ] T072 [P] [US7] Tests in `UnitTest/Ui/DeskSceneModelTests.cpp` and `DeskSceneLayoutTests.cpp`: each new `DeskDeviceKind` loads with its lamp and, for removable media, its door or slot; automatic placement of every configuration the Machine tab allows has no overlap (FR-024)
- [ ] T073 [US7] Add `UniDisk35`, `Apple35Drive`, `HardDisk20SC` and `ProFile` to `DeskDeviceKind` in `CassoEmuCore/Ui/Scene/DeskSceneModel.h` and place them in `DeskSceneLayout`
- [ ] T074 [P] [US7] Tests in `UnitTest/Ui/DeskSceneHitTesterTests.cpp`: hover, eject-click, body-click and drop on each new drive match the Disk II's behavior and tip wording
- [ ] T075 [US7] Wire hit testing and tips in `DeskSceneHitTester` and `ShellDeskScene`
- [ ] T076 [US7] Compact theme widgets for each new drive in `CassoEmuCore/Ui/Chrome/DriveWidget.h/.cpp` (FR-023)

### Audio

- [ ] T077 [US7] Search for CC0 or CC-BY recordings of a Sony 800K drive, an HD20SC and a ProFile; record the sources and licenses in `research.md` R13, or record that none exists
- [ ] T078 [P] [US7] Tests in `UnitTest/Audio/Floppy35AudioSourceTests.cpp` and `HardDiskAudioSourceTests.cpp`: spin-up, seek, stepping and eject events produce sound, with samples and with synthesis; the drive audio switch silences both
- [ ] T079 [US7] Implement `Floppy35AudioSource` and `HardDiskAudioSource` in `CassoEmuCore/Audio/`, panned by desk position through `DriveAudioMixer`, and the sample fetch in `AssetBootstrap`

### Events

- [ ] T080 [US7] Feed the drive lamps and audio sources from the block device and floppy event feeds, with tests that a read lights the lamp and a seek starts its sound
- [ ] T081 [US7] Write a note in `specs/048-smartport-drives/handoff.md` for spec 035 that lists the two feeds, their event structs, and where `MachineHost::AttachObservers` attaches them, so 035's debugger can log them as it logs `Disk2Event` (research R14)

---

## Phase 10: Polish and cross-cutting

- [ ] T082 Performance test in `UnitTest/EmuTests/IdleDevicePerformanceTests.cpp`: a machine with every card attached and idle runs as fast as one without, within measurement noise
- [ ] T083 Real-timing check: a 32 MB boot on the SCSI card with real timing on is within 10 percent of MAME's time (SC-007)
- [ ] T084 Owner review of each desk model beside reference photographs (SC-008)
- [ ] T085 Run the 033 block device scenario tests (T573) on the SCSI card (SC-009), in coordination with whoever is working on spec 033's Story 23
- [ ] T086 `CHANGELOG.md` and `README.md` entries for the new cards, drives and machine, terse, after the owner approves the text
- [ ] T087 Pre-merge gate: `scripts/CheckStyle.ps1 -Mode Tree`, `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`, `scripts/RunTests.ps1 -Build`, `scripts/RunTests.ps1 -Build -Scenario`, `scripts/RunTests.ps1 -Build -Sanitize -Configuration Release -Scenario`

---

## Dependencies and execution order

- Phase 1 then Phase 2 block everything.
- Block stack: Phase 3 (SCSI) and Phase 4 (ProFile) depend only on Phase 2
  and are independent of each other.
- 3.5-inch stack: Phase 5 (IWM, Sony, SWIM, card) before Phase 6 (UniDisk),
  which reuses the IWM, the Sony mechanism and `DeviceCpu`. Phase 7 (//c
  Plus) depends on Phases 5 and 6.
- Phase 8 (configuration) depends on the devices it configures; its config
  tasks for a device can follow that device's phase.
- Phase 9 (desk, audio, events): models (T067-T070) can start any time;
  wiring follows each device's phase.
- Phase 10 is last.

## Parallel opportunities

- The four CAD models (T067-T070) run alongside any device work.
- Within Phase 2, the media, event and config test tasks marked [P] are in
  separate files.
- The block stack and the 3.5-inch stack share only Phase 2, so they can run
  side by side in separate sessions if the owner chooses.

## Implementation strategy

Every story ships; there is no reduced first release (the owner's rule). The
order gets the most-used device booting first, so each later phase builds on
tested pieces: SCSI (MAME reference, unblocks 033), the
ProFile, then the 3.5-inch stack bottom-up, the //c Plus, and
the UI and desk work per device. Push the branch after each checkpoint.
