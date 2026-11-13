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
story numbering: the A2Pico comes first because its firmware is in the tree
and spec 033's T573 depends on it, and the stacks come before the UI that
configures them. Each story phase ends at a boot a scenario test checks.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: The spec's user story (US1 to US9)

## Path conventions

`CassoEmuCore/Devices/BlockDevice/` (BD), `CassoEmuCore/Devices/Floppy35/`
(F35), `UnitTest/BlockDeviceTests/` (UT-BD), `UnitTest/Floppy35Tests/`
(UT-F35), `ScenarioTests/`. Paths below are written in full.

---

## Phase 1: Setup

**Purpose**: Folders, firmware, project files.

- [ ] T001 Create `CassoEmuCore/Devices/BlockDevice/`, `CassoEmuCore/Devices/Floppy35/`, `CassoEmuCore/Machines/Apple2/Apple2cPlus/`, `UnitTest/BlockDeviceTests/` and `UnitTest/Floppy35Tests/`, and add filters for them to `CassoEmuCore/CassoEmuCore.vcxproj.filters` and `UnitTest/UnitTest.vcxproj.filters`
- [ ] T002 Add the catalog rows of `contracts/firmware-fetch.md` to `scripts/FetchRoms.ps1` with a `Sha1` column verified after download (a mismatch deletes the file and fails the fetch), and add the test subset to `-Fixtures`
- [ ] T003 [P] Fetch the rest of A2retroNET (slot page source, register equates, the Pico-side C sources) into `%USERPROFILE%\Downloads\Casso\a2retronet`, and record in `research.md` R2 the `CTRL` and `DATA` addresses and the command set with their parameter and reply layouts
- [ ] T004 [P] Vendor A2retroNET's 6502 sources and `LICENSE` (MIT) under `Resources/Firmware/A2retroNET/`, and add a build step in `CassoEmuCore/CassoEmuCore.vcxproj` that assembles them with Casso's Merlin dialect into an embedded resource
- [ ] T005 [P] Extract `cffa20eec02.bin` from dreher.net's Run6 CD package in the fetch script, verifying it against MAME's SHA-1 `080ff88f19de22328e162954ee2b51ee65f9d5cd`

---

## Phase 2: Foundational

**Purpose**: The media layer, event feeds, registry and config plumbing that
every device uses. No story starts before this phase is done.

### Media

- [ ] T006 [P] Tests in `UnitTest/BlockDeviceTests/BlockDeviceMediaTests.cpp`: detect `.po`, `.hdv`, ProDOS-order `.2mg`, DiskCopy 4.2 and WOZ 2 3.5-inch by header then extension; fail the attach of a DOS-order 2IMG and of a size over the drive's capacity ("At most 65,535 (32 MB) per ProDOS unit; at most the ProFile model's capacity"); report the 2IMG locked flag and a read-only host file as write protection with the Disk II's wording; fail a second attach of one path ("One writer per path across all drives (FR-018)"); and confirm the file is byte-identical after every failed attach. All over in-memory files through `IDiskFileIo`
- [ ] T007 Implement `BlockDeviceMedia` in `CassoEmuCore/Devices/BlockDevice/BlockDeviceMedia.h/.cpp` over spec 033's `CachingBlockStore`, `FileBlockStore` and `OffsetBlockStore`
- [ ] T008 [P] Tests in `UnitTest/Floppy35Tests/DiskCopy42ImageTests.cpp`: read the header, data and tag checksums of a DiskCopy 4.2 image; write blocks and recompute both checksums; fail the attach of an image with a bad checksum as damaged media
- [ ] T009 Implement `DiskCopy42Image` in `CassoEmuCore/Devices/Floppy35/DiskCopy42Image.h/.cpp` as an `IBlockStore`

### Event feeds

- [ ] T010 [P] Tests in `UnitTest/BlockDeviceTests/BlockDeviceEventRingTests.cpp` and `UnitTest/Floppy35Tests/Floppy35EventRingTests.cpp`: single-producer ring order, overflow counting, and no cost with no sink attached, modeled on `Disk2EventRingTests.cpp`
- [ ] T011 Add `IBlockDeviceEventSink.h`, `BlockDeviceEvent.h` and `BlockDeviceEventRing.h/.cpp` in `CassoEmuCore/Devices/BlockDevice/`, and `IFloppy35EventSink.h`, `Floppy35Event.h` and `Floppy35EventRing.h/.cpp` in `CassoEmuCore/Devices/Floppy35/`, with the fields of `contracts/device-events.md`
- [ ] T012 Add both sinks to `MachineHost::AttachObservers` in `CassoEmuCore/Shell/MachineHost.h/.cpp`, with a test in `UnitTest/EmuTests/MachineHostObserverTests.cpp`

### Registry and configuration

- [ ] T013 [P] Tests in `UnitTest/EmuTests/MachineConfigTests.cpp`: parse every slot and port form in `contracts/machine-config.md` (SCSI IDs as ports, `folder`, `chain`, `realTiming` defaulting to `false`); fail to load a device a machine cannot host, with an error that lists the machines that accept it and the slot empty; resolve card ROMs under `Devices/<Card>/`
- [ ] T014 Extend `MachineConfig` in `CassoEmuCore/Core/MachineConfig.h/.cpp` with the card and drive entries, a per-device host-machine list, and ROM resolution
- [ ] T015 [P] Tests in `UnitTest/EmuTests/MachineConfigUpgradeTests.cpp`: a bumped `$cassoMachineVersion` adds nothing to a user copy's configured slots
- [ ] T016 Update `MachineConfigUpgrade` in `CassoEmuCore/Core/MachineConfigUpgrade.h/.cpp`
- [ ] T017 Add constants for the device ids (`apple-scsi-card`, `hd20sc`, `scsi-disk`, `profile-card`, `profile-5mb`, `profile-10mb`, `cffa2`, `cffa3000`, `cf-card`, `a2pico`, `liron-card`, `unidisk35`, `apple35-card`, `apple35-drive`, `disk-iic`) in `CassoEmuCore/Core/MachineConfig.h`, and display labels in sentence case in `CassoEmuCore/Ui/Settings/SettingsJson.cpp`
- [ ] T018 [P] Add a missing-ROM path test in `UnitTest/EmuTests/ComponentRegistryTests.cpp`: a card whose ROM is absent reports the file and the folders searched, and the card does not run (FR-016)
- [ ] T019 Add a `TestMachine` helper in `UnitTest/EmuTests/TestMachine.h/.cpp` that puts a card in a slot over in-memory images through `Customize`, for the scenario tests and for spec 033's T573 (FR-025)

**Checkpoint**: Media, feeds and config are in place. Device stacks can start.

---

## Phase 3: User Story 2, A2Pico card (Priority: P1)

**Goal**: An A2Pico card folder from Casso Explorer boots, and every drive in
`A2retroNET.txt` is online.

**Independent test**: Boot a card folder made by Casso Explorer and catalog
every drive.

- [ ] T020 [P] [US2] Tests in `UnitTest/BlockDeviceTests/A2PicoCardTests.cpp`: the assembled firmware is byte-identical across builds and occupies `$C800-$CEFF`; a `CTRL` write starts a command and bit 7 of a `CTRL` read indicates done; ProDOS STATUS, READ BLOCK and WRITE BLOCK through `DATA` return the image's bytes and block count; SmartPort STATUS on a missing drive returns offline; `A2retroNET.txt` is reread at the card's reset (FR-012)
- [ ] T021 [US2] Implement `A2PicoCard` in `CassoEmuCore/Devices/BlockDevice/A2PicoCard.h/.cpp`: slot page and `$C800` firmware from the embedded resource, the register window, and the command processor of research R2 over `BlockDeviceMedia` per drive, reading the folder through spec 033's `A2retroNetConfig`, with a block device event per command
- [ ] T022 [US2] Register `a2pico` in `CassoEmuCore/Core/ComponentRegistry.cpp`
- [ ] T023 [P] [US2] Test the configuration menu command (`$40`) in `UnitTest/BlockDeviceTests/A2PicoCardTests.cpp`: the 40 by 24 screen text the Pico side returns, uppercase on a ][ or ][+
- [ ] T024 [US2] Scenario tests in `ScenarioTests/BlockDeviceBootTests.cpp`: an enhanced //e with an A2Pico in slot 4 over an 800K `.po`, a 32 MB `.hdv` and a 32 MB `.2mg` boots drive 1 and lists the other volumes online; a written file is present in the image afterward and spec 033's integrity check reports it clean
- [ ] T025 [US2] Update `ScenarioTests/A2PicoCardTests.cpp` (spec 033's T694) to boot the card folder on the A2Pico instead of the Disk II workaround, and update spec 033's `validation.md` entry

**Checkpoint**: The A2Pico boots; spec 033's T573 has a card to run on.

---

## Phase 4: User Story 1, Apple II SCSI Card (Priority: P1)

**Goal**: A hard disk boots on the SCSI card and matches MAME's boot.

**Independent test**: Boot every image of spec 033's MAME table on the SCSI
card in Casso.

- [ ] T026 [P] [US1] Tests in `UnitTest/BlockDeviceTests/Ncr5380Tests.cpp`: every register's read and write (current data, initiator command, mode, target command, bus status, bus and status, input data, reset parity and interrupts), arbitration, selection with and without a responding target, the phase-match bit, and the DMA and pseudo-DMA handshakes, against the NCR 5380 data sheet
- [ ] T027 [US1] Implement `Ncr5380` in `CassoEmuCore/Devices/BlockDevice/Ncr5380.h/.cpp` and `ScsiBus` in `CassoEmuCore/Devices/BlockDevice/ScsiBus.h/.cpp` with the phase machine of `data-model.md` ("Selection of an absent ID times out back to `BusFree`")
- [ ] T028 [P] [US1] Tests in `UnitTest/BlockDeviceTests/ScsiHardDiskTests.cpp`: TEST UNIT READY, REQUEST SENSE (sense cleared after), INQUIRY with the HD20SC's strings, READ(6), READ(10), WRITE(6), WRITE(10), READ CAPACITY and MODE SENSE; a read past the end sets ILLEGAL REQUEST; a write to protected media sets DATA PROTECT; real timing off completes at once and on adds spin-up and seek delays (research R15)
- [ ] T029 [US1] Implement `ScsiHardDisk` in `CassoEmuCore/Devices/BlockDevice/ScsiHardDisk.h/.cpp` over `BlockDeviceMedia` ("ID 7 is the card. Unique per card"), with spin-up and seek events
- [ ] T030 [P] [US1] Tests in `UnitTest/BlockDeviceTests/AppleScsiCardTests.cpp`: ROM and RAM banking at `$C800-$CFFF` through the bank register, the 5380 at `$C0n0-$C0n7`, I/O SELECT and I/O STROBE expansion ROM arbitration through `IExpansionRomCard`, and a firmware run in a test machine that issues INQUIRY to each ID
- [ ] T031 [US1] Implement `AppleScsiCard` in `CassoEmuCore/Devices/BlockDevice/AppleScsiCard.h/.cpp` (research R5), register `apple-scsi-card`, `hd20sc` and `scsi-disk`
- [ ] T032 [US1] Scenario tests in `ScenarioTests/BlockDeviceBootTests.cpp`: each image of spec 033's MAME table boots on the SCSI card in slot 7 to the same screen; images at two IDs are both online; with no image attached the boot moves to the next slot
- [ ] T033 [US1] Run each image in MAME with `scripts/MameSnapshot.ps1` and Casso side by side, and record the comparison in `specs/048-smartport-drives/validation.md` (FR-026, SC-001)

**Checkpoint**: The SCSI card boots everything MAME boots.

---

## Phase 5: User Story 3, CFFA 2.0 (Priority: P2)

**Goal**: A CFFA 2.0 card image boots and presents its partitions as MAME does.

**Independent test**: The four-volume card from spec 033's table.

- [ ] T034 [P] [US3] Tests in `UnitTest/BlockDeviceTests/AtaDeviceTests.cpp`: task file registers, IDENTIFY DEVICE with the image's sector count, READ SECTORS and WRITE SECTORS in LBA mode, BSY and DRQ sequencing, the error register on a read past the end
- [ ] T035 [US3] Implement `AtaDevice` in `CassoEmuCore/Devices/BlockDevice/AtaDevice.h/.cpp`
- [ ] T036 [P] [US3] Tests in `UnitTest/BlockDeviceTests/CffaCardTests.cpp`: the EEPROM at `$C800`, the register window with the 16-bit data latch, and partition geometry identical to spec 033's `CffaLayout` for card images it made
- [ ] T037 [US3] Implement `CffaCard` (2.0 variant) in `CassoEmuCore/Devices/BlockDevice/CffaCard.h/.cpp`, register `cffa2` and `cf-card`
- [ ] T038 [US3] Scenario tests in `ScenarioTests/BlockDeviceBootTests.cpp`: the four-volume card boots with ProDOS 2.4.2 and its STARTUP catalogs CARD2, CARD3 and CARD4; with ProDOS 1.x CARD2 is read and CARD3 is not; record against MAME in `validation.md`

---

## Phase 6: User Story 7, ProFile (Priority: P3)

**Goal**: A 5 MB or 10 MB ProFile boots ProDOS.

- [ ] T039 [P] [US7] Tests in `UnitTest/BlockDeviceTests/ProFileDriveTests.cpp`: the handshake states of `data-model.md` (`Idle -> CommandWait ($01) -> CommandReceive (6 bytes) -> ReadyForData ($02 or $03) -> Transfer (532 bytes) -> Status ($04, 4 bytes) -> Idle`), "512-byte blocks; the 20 tag bytes are zeros on read and discarded on write", block counts 9,728 and 19,456, a too-large image fails to attach with the model's capacity in the error
- [ ] T040 [US7] Implement `ProFileDrive` in `CassoEmuCore/Devices/BlockDevice/ProFileDrive.h/.cpp`
- [ ] T041 [P] [US7] Tests in `UnitTest/BlockDeviceTests/ProFileCardTests.cpp`: the card's handshake lines (BSY, STRB, CMD, R/W, PARITY) and its ROM at `$Cn00`, with 341-0271-A for the 5 MB model and 341-0299-B for the 10 MB model
- [ ] T042 [US7] Implement `ProFileCard` in `CassoEmuCore/Devices/BlockDevice/ProFileCard.h/.cpp`, register `profile-card`, `profile-5mb` and `profile-10mb`
- [ ] T043 [US7] Scenario test in `ScenarioTests/BlockDeviceBootTests.cpp`: a bootable 5 MB image boots on a //e with the ProFile card, and a written file round-trips

---

## Phase 7: User Story 5, Apple 3.5 Drive on the 3.5 controller card (Priority: P2)

**Goal**: An Apple 3.5 Drive boots 800K images and protected WOZ disks.

### IWM, one implementation

- [ ] T044 [US5] Run the full Disk II test set (`RunTests.ps1 -Filter Disk2` and the scenario suite) and record the pass count as the baseline for the factor-out
- [ ] T045 [P] [US5] Tests in `UnitTest/Floppy35Tests/IwmTests.cpp`: the mode register bits (latch, asynchronous handshake, bit cell, timer, fast clock), status and handshake registers, the read and write data paths in both bit-cell sizes, and the phase and motor switches, against Apple's IWM specification, driven through a fake `IDiskDriveBus`
- [ ] T046 [US5] Factor `Iwm` out of `Disk2Controller`'s IWM mode into `CassoEmuCore/Devices/Floppy35/Iwm.h/.cpp` with `IDiskDriveBus` in `CassoEmuCore/Devices/Floppy35/IDiskDriveBus.h`, and route the //c's internal drive through it; the full Disk II baseline from T044 passes

### Sony mechanism and media

- [ ] T047 [P] [US5] Tests in `UnitTest/Floppy35Tests/Sony35DriveTests.cpp`: every status line through CA0-CA2 and SEL, stepping 0 to 79, side select, zone per track ("`track / 16`; sets the rotation speed and sectors per track"), eject after the eject travel time, disk-in-place and write-protect sense
- [ ] T048 [US5] Implement `Sony35Drive` in `CassoEmuCore/Devices/Floppy35/Sony35Drive.h/.cpp` with a floppy event per operation
- [ ] T049 [P] [US5] Tests in `UnitTest/Floppy35Tests/Gcr35TrackTests.cpp`: an 800K block image builds tracks with 12, 11, 10, 9 and 8 sectors per zone and 2:1 interleave; decoding the built tracks returns the blocks; a write to a built track writes the decoded blocks back; a 400K single-sided image builds side 0 only
- [ ] T050 [US5] Implement `Gcr35Track` in `CassoEmuCore/Devices/Floppy35/Gcr35Track.h/.cpp`
- [ ] T051 [P] [US5] Tests in `UnitTest/Floppy35Tests/Woz35Tests.cpp`: a WOZ 2 3.5-inch image loads per track and side with its bit counts, writes change its bits, and the INFO chunk's write protection is honored
- [ ] T052 [US5] Extend `WozLoader` in `CassoEmuCore/Machines/Apple2/Common/WozLoader.h/.cpp` for 3.5-inch disk type, and add `Floppy35Media` in `CassoEmuCore/Devices/Floppy35/Floppy35Media.h/.cpp`

### SWIM and the card

- [ ] T053 [P] [US5] Tests in `UnitTest/Floppy35Tests/SwimTests.cpp`: IWM mode matches `Iwm`; the mode switch sequence; ISM register probing as 341-0438-A performs it (research R8)
- [ ] T054 [US5] Implement `Swim` in `CassoEmuCore/Devices/Floppy35/Swim.h/.cpp` over `Iwm`
- [ ] T055 [P] [US5] Tests in `UnitTest/Floppy35Tests/DeviceCpuTests.cpp`: a `Cpu65C02` over a separate `MemoryBus` steps at its clock ratio against the host cycle count, parks while inactive ("`Parked` while the device is inactive, `Running` while active"), and resumes on activity
- [ ] T056 [US5] Implement `DeviceCpu` in `CassoEmuCore/Devices/Floppy35/DeviceCpu.h/.cpp`
- [ ] T057 [P] [US5] Tests in `UnitTest/Floppy35Tests/Apple35ControllerCardTests.cpp`: the card's memory map (its 65C02, 8 KB RAM, 32 KB ROM, SWIM, the window shared with the host), the host-side slot page, and a firmware run to a block read
- [ ] T058 [US5] Implement `Apple35ControllerCard` in `CassoEmuCore/Devices/Floppy35/Apple35ControllerCard.h/.cpp`, register `apple35-card` and `apple35-drive`, //e and enhanced //e only
- [ ] T059 [US5] Scenario tests in `ScenarioTests/Floppy35BootTests.cpp`: an 800K `.po`, a DiskCopy 4.2 image and a WOZ 2 3.5-inch image boot; a written file round-trips with DiskCopy checksums updated; record against MAME's `a2superdrive` in `validation.md`
- [ ] T060 [US5] Find a copy-protected 3.5-inch WOZ image (4am's archive), confirm it boots in MAME, and add a scenario boot for it against `%USERPROFILE%\Downloads\Casso` (SC-004)

---

## Phase 8: User Story 4, UniDisk 3.5 on the //c and the Liron card (Priority: P2)

**Goal**: A UniDisk 3.5 boots on the //c disk port and on the Liron card, and
chains.

- [ ] T061 [P] [US4] Tests in `UnitTest/Floppy35Tests/SmartPortBusTests.cpp`: REQ on phase 0, bus enable on phases 1 and 3, ACK on the sense line, packet framing (sync, `$C3`, 7-to-8 groups, checksum, `$C8`), INIT assigning IDs down the chain, and reset clearing them (research R10)
- [ ] T062 [US4] Implement `SmartPortBus` and `SmartPortChain` in `CassoEmuCore/Devices/Floppy35/SmartPortBus.h/.cpp`
- [ ] T063 [P] [US4] Tests in `UnitTest/Floppy35Tests/UniDisk35DriveTests.cpp`: the drive's memory map from the a2-liron disassembly (2 KB RAM, gate array at `$0800-$0801`, IWM at `$0A00-$0A0F`, 8 KB ROM at `$E000`), the gate array's bits in each direction, and a firmware run that replies to a SmartPort STATUS packet and a READ BLOCK
- [ ] T064 [US4] Implement `UniDisk35Drive` in `CassoEmuCore/Devices/Floppy35/UniDisk35Drive.h/.cpp`: `DeviceCpu` at 2 MHz, gate array, internal `Iwm`, `Sony35Drive`, on the chain
- [ ] T065 [P] [US4] Tests in `UnitTest/Floppy35Tests/LironCardTests.cpp`: the card's ROM, its IWM, and its slot page as `liron-if.asm` documents
- [ ] T066 [US4] Implement `LironCard` in `CassoEmuCore/Devices/Floppy35/LironCard.h/.cpp`, register `liron-card` and `unidisk35`
- [ ] T067 [US4] Connect the //c disk port to a `SmartPortChain` that ends in an optional 5.25-inch drive (`disk-iic`), in `CassoEmuCore/Machines/Apple2/Apple2c/` and `MachineConfig`
- [ ] T068 [US4] Scenario tests in `ScenarioTests/Floppy35BootTests.cpp`: an 800K image boots on a //c with a UniDisk 3.5 on the disk port; the same image boots on a //e from the Liron card in slot 5; two chained UniDisks are online; a 5.25-inch drive at the chain's end boots; a SmartPort eject call ejects the disk

---

## Phase 9: User Story 6, //c Plus (Priority: P2)

**Goal**: The //c Plus boots from its internal 3.5-inch drive at 4 MHz.

- [ ] T069 [P] [US6] Tests in `UnitTest/EmuTests/Apple2cPlusTests.cpp`: ROM 05 banking, the MIG RAM window, the internal Apple 3.5 Drive on the IWM, and the external port chain accepting `apple35-drive`, `unidisk35` and `disk-iic`
- [ ] T070 [US6] Implement `Apple2cPlus` in `CassoEmuCore/Machines/Apple2/Apple2cPlus/Apple2cPlus.h/.cpp` and `Resources/Machines/Apple2cPlus/Apple2cPlus.json`, and register it in the machine catalog
- [ ] T071 [P] [US6] Tests in `UnitTest/EmuTests/Apple2cPlusAcceleratorTests.cpp`: a timing loop runs at the accelerated speed with acceleration on and at 1 MHz with it off; disk, speaker and I/O accesses run at 1 MHz; Esc at power-on turns acceleration off (research R12)
- [ ] T072 [US6] Implement the accelerator in `CassoEmuCore/Machines/Apple2/Apple2cPlus/Apple2cPlusAccelerator.h/.cpp` within the existing CPU speed modes
- [ ] T073 [US6] Scenario tests in `ScenarioTests/Apple2cPlusTests.cpp`: an 800K image boots from the internal drive and from an external Apple 3.5 Drive; record against MAME's `apple2cp` in `validation.md` (SC-005a)

---

## Phase 10: User Story 8, configure cards and attach images (Priority: P1)

**Goal**: Every card and drive is reachable from the Machine tab, the desk
and the command line, and persists.

- [ ] T074 [P] [US8] Tests in `UnitTest/Settings/HardwarePageModelTests.cpp`: the device tree lists each card with its ports (SCSI IDs 0-6, chain positions), offers a card only on a machine that hosts it with a tip that lists the machines that do, confirms before replacing a card in a filled slot, and limits the //c chain to its firmware's length
- [ ] T075 [US8] Extend the device tree in `CassoEmuCore/Ui/Settings/HardwarePage.h/.cpp` and `SettingsPanelState` with cards, drives, image attach, and the per-drive real timing switch
- [ ] T076 [P] [US8] Tests in `UnitTest/EmuTests/CommandLineTests.cpp`: `--hd <slot>=`, `--hd <slot>.<unit>=`, `--smartport <n>=` and `--a2pico <slot>=` per `contracts/command-line.md`, each error in the fixed error layout, and the saved configuration untouched after the run
- [ ] T077 [US8] Implement the flags in core beside `--disk1`
- [ ] T078 [P] [US8] Tests: dropping an image on a drive attaches it with the same validation as the Machine tab (FR-017), in `UnitTest/Ui/DriveWidgetStateTests.cpp`
- [ ] T079 [US8] Extend `DriveWidgetState` and `IDriveCommandSink` in `CassoEmuCore/Ui/` for block devices and 3.5-inch drives
- [ ] T080 [US8] Persistence test: configure every card, quit, relaunch through `MachineConfig` round trip, and compare

---

## Phase 11: User Story 9, see, hear and watch every drive (Priority: P2)

**Goal**: Each external drive is a realistic model with the Disk II's
interaction, sounds and events.

### Models

- [ ] T081 [P] [US9] Write `scripts/modelgen/cad_unidisk35.py` from measurements of reference photographs (proportions written in the script), render with `preview.py`, and check the render before wiring
- [ ] T082 [P] [US9] Write `scripts/modelgen/cad_apple35.py` the same way, with a //c Plus color variant for its internal drive
- [ ] T083 [P] [US9] Write `scripts/modelgen/cad_hd20sc.py` the same way
- [ ] T084 [P] [US9] Write `scripts/modelgen/cad_profile.py` in CadQuery, replacing `gen_profile.py`, and update the table in `scripts/modelgen/README.md`
- [ ] T085 [US9] Check every model color against the `DeskSceneModel` palette ("at least a channel clear of the palette" per the modelgen README), bake them in `Resources/Models/`, and add each to the `BakedMesh` list in `Casso/Casso.vcxproj` and `Casso.rc`
- [ ] T086 [P] [US9] Tests in `UnitTest/Ui/DeskSceneModelTests.cpp` and `DeskSceneLayoutTests.cpp`: each new `DeskDeviceKind` loads with its lamp and, for removable media, its door or slot; automatic placement of every configuration the Machine tab allows has no overlap (FR-024)
- [ ] T087 [US9] Add `UniDisk35`, `Apple35Drive`, `HardDisk20SC` and `ProFile` to `DeskDeviceKind` in `CassoEmuCore/Ui/Scene/DeskSceneModel.h` and place them in `DeskSceneLayout`
- [ ] T088 [P] [US9] Tests in `UnitTest/Ui/DeskSceneHitTesterTests.cpp`: hover, eject-click, body-click and drop on each new drive match the Disk II's behavior and tip wording
- [ ] T089 [US9] Wire hit testing and tips in `DeskSceneHitTester` and `ShellDeskScene`
- [ ] T090 [P] [US9] Tests and implementation of the drive band indicator for CFFA and A2Pico cards, a lamp and a tip with the card's images in every theme, in `CassoEmuCore/Ui/Chrome/DriveWidget.h/.cpp`
- [ ] T091 [US9] Compact theme widgets for each new drive in `CassoEmuCore/Ui/Chrome/DriveWidget.h/.cpp` (FR-023)

### Audio

- [ ] T092 [US9] Search for CC0 or CC-BY recordings of a Sony 800K drive, an HD20SC and a ProFile; record the sources and licenses in `research.md` R13, or record that none exists
- [ ] T093 [P] [US9] Tests in `UnitTest/Audio/Floppy35AudioSourceTests.cpp` and `HardDiskAudioSourceTests.cpp`: spin-up, seek, stepping and eject events produce sound, with samples and with synthesis; the drive audio switch silences both
- [ ] T094 [US9] Implement `Floppy35AudioSource` and `HardDiskAudioSource` in `CassoEmuCore/Audio/`, panned by desk position through `DriveAudioMixer`, and the sample fetch in `AssetBootstrap`

### Events

- [ ] T095 [US9] Feed the drive lamps and audio sources from the block device and floppy event feeds, with tests that a read lights the lamp and a seek starts its sound
- [ ] T096 [US9] Debug log view per the owner's decision in research R14: either a log view in 035's debugger over the two feeds after 035 merges, or a log window in this spec

---

## Phase 12: User Story 3, CFFA 3000 (Priority: P2)

**Pending**: the owner's decision in research R4. The tasks below follow the
recommended option 2.

- [ ] T097 [US3] Extract the 6502 code from `CFFA.bin` (firmware 3.1.1) and document the AVR's register interface from the CFFA3000 reference manual v1.3 in `research.md` R4
- [ ] T098 [P] [US3] Tests in `UnitTest/BlockDeviceTests/Cffa3000Tests.cpp`: the register interface, drive image selection from the card's folder, and the menu behavior the manual documents
- [ ] T099 [US3] Implement the CFFA 3000 variant of `CffaCard` and register `cffa3000`
- [ ] T100 [US3] Scenario test: a CFFA 3000 boots its first drive image and the others are online

---

## Phase 13: Polish and cross-cutting

- [ ] T101 Performance test in `UnitTest/EmuTests/IdleDevicePerformanceTests.cpp`: a machine with every card attached and idle runs as fast as one without, within measurement noise
- [ ] T102 Real-timing check: a 32 MB boot on the SCSI card with real timing on is within 10 percent of MAME's time (SC-007)
- [ ] T103 Owner review of each desk model beside reference photographs (SC-008)
- [ ] T104 Run the 033 block device scenario tests (T573) on the A2Pico and the SCSI card (SC-009), in coordination with whoever is working on spec 033's Story 23
- [ ] T105 `CHANGELOG.md` and `README.md` entries for the new cards, drives and machine, terse, after the owner approves the text
- [ ] T106 Pre-merge gate: `scripts/CheckStyle.ps1 -Mode Tree`, `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`, `scripts/RunTests.ps1 -Build`, `scripts/RunTests.ps1 -Build -Scenario`, `scripts/RunTests.ps1 -Build -Sanitize -Configuration Release -Scenario`

---

## Dependencies and execution order

- Phase 1 then Phase 2 block everything.
- Block stack: Phase 3 (A2Pico), Phase 4 (SCSI), Phase 5 (CFFA 2.0), Phase 6
  (ProFile) depend only on Phase 2 and are independent of each other.
- 3.5-inch stack: Phase 7 (IWM, Sony, SWIM, card) before Phase 8 (UniDisk),
  which reuses the IWM, the Sony mechanism and `DeviceCpu`. Phase 9 (//c
  Plus) depends on Phases 7 and 8.
- Phase 10 (configuration) depends on the devices it configures; its config
  tasks for a device can follow that device's phase.
- Phase 11 (desk, audio, events): models (T081-T084) can start any time;
  wiring follows each device's phase.
- Phase 12 waits for the owner's R4 decision.
- Phase 13 is last.

## Parallel opportunities

- The four CAD models (T081-T084) run alongside any device work.
- Within Phase 2, the media, event and config test tasks marked [P] are in
  separate files.
- The block stack and the 3.5-inch stack share only Phase 2, so they can run
  side by side in separate sessions if the owner chooses.

## Implementation strategy

Every story ships; there is no reduced first release (the owner's rule). The
order gets the most-used device booting first, so each later phase builds on
tested pieces: A2Pico (no fetch, unblocks 033), SCSI (MAME reference), the
rest of the block stack, then the 3.5-inch stack bottom-up, the //c Plus, and
the UI and desk work per device. Push the branch after each checkpoint.
