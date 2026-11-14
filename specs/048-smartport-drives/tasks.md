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

**Research status**: `research.md` marks each hardware fact as checked or to
verify. A task that depends on a "to verify" fact comes after the task that
checks it, and the checking task updates `research.md` and `data-model.md`
before any test is written from them.

**Organization**: Phases follow the plan's delivery order, not the spec's
story numbering: the SCSI card comes first because MAME has reference boots
for it and spec 033's T573 runs on it, the stacks come before the UI that
configures them, and the default cards come last so every earlier phase runs
on machines as they ship today.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: The spec's user story (US1 to US7)

---

## Phase 1: Setup

**Purpose**: Folders, ROM fetch, project files.

- [X] T001 Create `CassoEmuCore/Devices/BlockDevice/`, `CassoEmuCore/Devices/Floppy35/`, `CassoEmuCore/Machines/Apple2/Apple2cPlus/`, `UnitTest/BlockDeviceTests/` and `UnitTest/Floppy35Tests/`, and add filters for them to `CassoEmuCore/CassoEmuCore.vcxproj.filters` and `UnitTest/UnitTest.vcxproj.filters`. The tree has no `.filters` files, so the folders arrive with the first file each phase adds
- [X] T002 Add the catalog rows of `contracts/firmware-fetch.md` to `scripts/FetchRoms.ps1` with a checksum column verified after download (SHA-1 from MAME for 341-0437-A, 341-0438-A and the //c Plus ROM; SHA-256 from a2-liron for the Liron card and UniDisk 3.5 ROMs; a pinned SHA-1 for the two ProFile ROMs). A mismatch deletes the file and fails the fetch. Add the ROMs the tests use to `-Fixtures`
- [X] T003 Run the fetch, confirm every checksum, and write the two ProFile SHA-1 values and the result of the a2-liron comparison into `research.md` R1

---

## Phase 2: Foundational

**Purpose**: The media layer, event feeds, registry and config plumbing that
every device uses. No story starts before this phase is done.

### Media

- [X] T004 [P] Tests in `UnitTest/BlockDeviceTests/BlockDeviceMediaTests.cpp`: detect `.po`, `.hdv`, ProDOS-order `.2mg`, DiskCopy 4.2 and WOZ 2 3.5-inch by header then extension; fail the attach of a DOS-order 2IMG and of a size over the drive's capacity ("At most 65,535 (32 MB) per ProDOS unit; at most the ProFile model's capacity"); report the 2IMG locked flag and a read-only host file as write protection with the Disk II's wording; fail a second attach of one path ("One writer per path across all drives (FR-018)"); confirm the file is byte-identical after every failed attach; a machine reset in the middle of a block write results in the whole block or the old block, never a mix; and an image changed on the host while attached goes through spec 033's external change handling. All over in-memory files through `IDiskFileIo`
- [X] T005 Implement `BlockDeviceMedia` in `CassoEmuCore/Devices/BlockDevice/BlockDeviceMedia.h/.cpp` over spec 033's `CachingBlockStore`, `FileBlockStore` and `OffsetBlockStore`, committing through the durable commit path the Disk II uses (FR-009)
- [X] T006 [P] Superseded: the tree already has `DiskCopy42Container` and `BlockImageFile`, which read both checksums and rewrite the data checksum on commit; `BlockDeviceMediaTests::DiskCopyChecksums` covers the write and the damaged attach. Original text: Tests in `UnitTest/Floppy35Tests/DiskCopy42ImageTests.cpp`: read the header, data and tag checksums of a DiskCopy 4.2 image; write blocks and recompute both checksums; fail the attach of an image with a bad checksum as damaged media
- [X] T007 Superseded by `BlockImageFile` (see T006). Original text: Implement `DiskCopy42Image` in `CassoEmuCore/Devices/Floppy35/DiskCopy42Image.h/.cpp` as an `IBlockStore`

### Event feeds

- [X] T008 [P] Tests in `UnitTest/BlockDeviceTests/BlockDeviceEventRingTests.cpp` and `UnitTest/Floppy35Tests/Floppy35EventRingTests.cpp`: single-producer ring order, overflow counting, and no cost with no sink attached, modeled on `Disk2EventRingTests.cpp`
- [X] T009 Add `IBlockDeviceEventSink.h`, `BlockDeviceEvent.h` and `BlockDeviceEventRing.h/.cpp` in `CassoEmuCore/Devices/BlockDevice/`, and `IFloppy35EventSink.h`, `Floppy35Event.h` and `Floppy35EventRing.h/.cpp` in `CassoEmuCore/Devices/Floppy35/`, with the fields of `contracts/device-events.md`. Built as header-only rings over one template, `Core/SpscEventRing.h`
- [X] T010 Add both sinks to `MachineHost::AttachObservers` in `CassoEmuCore/Shell/MachineHost.h/.cpp`, with a test in `UnitTest/EmuTests/MachineHostObserverTests.cpp`

### Registry and configuration

- [X] T011 [P] Tests in `UnitTest/EmuTests/MachineConfigTests.cpp`: parse every slot and port form in `contracts/machine-config.md` (SCSI IDs as ports, `chain`, `"image": "stock"`, `realTiming` defaulting to `false`); fail to load a device a machine cannot host, with an error that lists the machines that accept it and the slot empty; resolve card ROMs under `Devices/<Card>/`
- [X] T012 Extend `MachineConfig` in `CassoEmuCore/Core/MachineConfig.h/.cpp` with the card and drive entries, a per-device host-machine list, and ROM resolution
- [X] T013 Add constants for the device ids (`apple-scsi-card`, `hd20sc`, `profile-card`, `profile-5mb`, `profile-10mb`, `liron-card`, `unidisk35`, `apple35-card`, `apple35-drive`, `disk-iic`) in `CassoEmuCore/Core/MachineConfig.h`, and display labels in sentence case in `CassoEmuCore/Ui/Settings/SettingsJson.cpp`. The ids, labels, ROM folders and host lists are in `CassoEmuCore/Devices/BlockDevice/BlockDeviceCatalog.h/.cpp`, which the loader and the Machine tab both read
- [X] T014 [P] Add a missing-ROM test in `UnitTest/EmuTests/ComponentRegistryTests.cpp`: a card whose ROM is absent reports the file and the folders searched, the card does not run, and the machine starts without it (FR-016). Covered by `MachineConfigTests::Load_BlockCard_MissingFirmwareDoesNotFailTheLoad` and `AppleScsiCardTests::WrongSizeFirmware`; `MachineBuilder::WireAppleScsiCard` leaves a card with a missing ROM without firmware, so the slot scan passes over it
- [X] T015 Add a `TestMachine` helper in `UnitTest/EmuTests/TestMachine.h/.cpp` that puts a card in a slot over in-memory images through `Customize`, for the scenario tests and for spec 033's T573 (FR-025)

**Checkpoint**: Media, feeds and config are in place. Device stacks can start.

---

## Phase 3: User Story 1, Apple II SCSI Card (Priority: P1)

**Goal**: A hard disk boots on the SCSI card and matches MAME's boot.

**Independent test**: Boot every image of spec 033's MAME table on the SCSI
card in Casso.

- [X] T016 [P] [US1] Tests in `UnitTest/BlockDeviceTests/Ncr5380Tests.cpp`: every register's read and write (current data, initiator command, mode, target command, bus status, bus and status, input data, reset parity and interrupts), arbitration, selection with and without a responding target, the phase-match bit, and the DMA handshake with DRQ, against the NCR 5380 data sheet
- [X] T017 [US1] Implement `Ncr5380` in `CassoEmuCore/Devices/BlockDevice/Ncr5380.h/.cpp` and `ScsiBus` in `CassoEmuCore/Devices/BlockDevice/ScsiBus.h/.cpp` with the phase machine of `data-model.md` ("Selection of an absent ID times out back to `BusFree`")
- [X] T018 [P] [US1] Tests in `UnitTest/BlockDeviceTests/ScsiHardDiskTests.cpp`: TEST UNIT READY, REQUEST SENSE (sense cleared after), INQUIRY, READ(6), READ(10), WRITE(6), WRITE(10), READ CAPACITY and MODE SENSE; a read past the end sets ILLEGAL REQUEST; a write to protected media sets DATA PROTECT; "no media means the target does not answer selection"; real timing off completes at once and on adds spin-up and seek delays (research R15)
- [X] T019 [US1] Implement `ScsiHardDisk` in `CassoEmuCore/Devices/BlockDevice/ScsiHardDisk.h/.cpp` over `BlockDeviceMedia` ("ID 7 is the card. Unique per card"), with spin-up and seek events, and `modelLabel` ("The image's size in MB, rounded to a whole number, then "SC"", "An empty drive is "20SC"")
- [X] T020 [P] [US1] Tests in `UnitTest/BlockDeviceTests/AppleScsiCardTests.cpp` for the map checked in research R5: 5380 at `$C0n0-$C0n7`; pseudo-DMA data at `$C0n8`; `$80` read at `$C0n9`; bank latch at `$C0nA` (bits 0-3 a 1 KB ROM bank at `$CC00-$CFFF`, bits 4-6 a 1 KB RAM bank at `$C800-$CBFF`, cleared at reset); 5380 reset at `$C0nB`; DRQ in bit 7 of `$C0nE`; ROM page 0 at `$Cn00`; `$C0nC` and `$C0nD` decoded with no effect; and expansion ROM arbitration through `IExpansionRomCard`
- [X] T021 [US1] Implement `AppleScsiCard` in `CassoEmuCore/Devices/BlockDevice/AppleScsiCard.h/.cpp`, and register `apple-scsi-card` and `hd20sc` in `CassoEmuCore/Core/ComponentRegistry.cpp`
- [X] T022 [US1] Boot Apple's ROM on the card in a test machine with a command log on the SCSI target, and write the commands the ROM and ProDOS issue into `research.md` R5; add any command missing from T018 and T019
- [X] T023 [US1] Scenario tests in `ScenarioTests/BlockDeviceBootTests.cpp`: each image of spec 033's MAME table boots on the SCSI card in slot 7 of an enhanced //e to the same screen; images at two IDs are both online; a file written by the guest is in the image afterward and spec 033's integrity check reports it clean; with no image attached, and with a blank-boot-block image attached, the floppy in slot 6 boots
- [X] T024 [US1] Measure Casso's emulated time from power-on to the first instruction of the Disk II boot ROM with real timing off, in the three setups of research R5 (no card, card with no disk, card with the blank-boot disk), and write the figures into `validation.md` beside MAME's from research R5 (FR-014c)
- [X] T025 [US1] Boot the card on Casso's ][+ and unenhanced //e, set the card's host-machine list from the result, and write it into `research.md` R19
- [ ] T026 [US1] Run each image in MAME with `scripts/MameSnapshot.ps1` and in Casso, and write the comparison into `specs/048-smartport-drives/validation.md` (FR-026, SC-001)

**Checkpoint**: The SCSI card boots everything MAME boots, and spec 033's T573 has a card to run on.

---

## Phase 4: User Story 5, ProFile (Priority: P3)

**Goal**: A 5 MB or 10 MB ProFile boots ProDOS.

- [ ] T027 [US5] Disassemble 341-0271-A and 341-0299-B with Casso's disassembler, read Apple's ProFile interface card and ProFile documentation on the mirror, and replace research R6 and the `ProFileDrive` section of `data-model.md` with the card's register map, the handshake, the command block, the block size and tag handling, both capacities, and the machines the card works in. The next four tasks are written from that text
- [ ] T028 [P] [US5] Tests in `UnitTest/BlockDeviceTests/ProFileDriveTests.cpp`: the handshake states, command block, block transfer and status from T027; both capacities; a too-large image fails to attach with the model's capacity in the error
- [ ] T029 [US5] Implement `ProFileDrive` in `CassoEmuCore/Devices/BlockDevice/ProFileDrive.h/.cpp`
- [ ] T030 [P] [US5] Tests in `UnitTest/BlockDeviceTests/ProFileCardTests.cpp`: the card's registers and handshake lines from T027, and its ROM at `$Cn00`, with 341-0271-A for the 5 MB model and 341-0299-B for the 10 MB model
- [ ] T031 [US5] Implement `ProFileCard` in `CassoEmuCore/Devices/BlockDevice/ProFileCard.h/.cpp`, and register `profile-card`, `profile-5mb` and `profile-10mb`
- [ ] T032 [US5] Scenario test in `ScenarioTests/BlockDeviceBootTests.cpp`: a bootable 5 MB image boots on each machine T027 lists, and a written file round-trips (SC-005)

---

## Phase 5: User Story 3, Apple 3.5 Drive on the 3.5 controller card (Priority: P2)

**Goal**: An Apple 3.5 Drive boots 800K images and protected WOZ disks.

### IWM, one implementation

- [ ] T033 [US3] Run the full Disk II test set (`RunTests.ps1 -Build -Filter Disk2` and `RunTests.ps1 -Build -Scenario`) and write the pass counts into `validation.md` as the baseline for the factor-out
- [ ] T034 [US3] Read Apple's IWM specification and write the mode, status and handshake register bits into `research.md` R7
- [ ] T035 [P] [US3] Tests in `UnitTest/Floppy35Tests/IwmTests.cpp`: the register bits from T034, the read and write data paths in both bit-cell sizes, and the phase, motor and drive select switches in the Disk II's order, driven through a fake `IDiskDriveBus`
- [ ] T036 [US3] Factor `Iwm` out of `Disk2Controller`'s IWM mode into `CassoEmuCore/Devices/Floppy35/Iwm.h/.cpp` with `IDiskDriveBus` in `CassoEmuCore/Devices/Floppy35/IDiskDriveBus.h`, and route the //c's internal drive through it; the full Disk II baseline from T033 passes

### Sony mechanism and media

- [ ] T037 [US3] Read the 3.5-inch drive chapters of the Apple IIgs Firmware Reference and Inside Macintosh, and write the status line numbering, the control commands, the zone speeds and the sector format into `research.md` R9
- [ ] T038 [P] [US3] Tests in `UnitTest/Floppy35Tests/Sony35DriveTests.cpp`: every status line and control command from T037 through CA0-CA2 and SEL, stepping 0 to 79, side select, zone per track ("`track / 16`; sets the rotation speed and sectors per track"), eject after the eject travel time, disk-in-place and write-protect sense
- [ ] T039 [US3] Implement `Sony35Drive` in `CassoEmuCore/Devices/Floppy35/Sony35Drive.h/.cpp` with a floppy event per operation
- [ ] T040 [P] [US3] Tests in `UnitTest/Floppy35Tests/Gcr35TrackTests.cpp`: an 800K block image builds tracks with 12, 11, 10, 9 and 8 sectors per zone and 2:1 interleave in the sector format from T037; decoding the built tracks returns the blocks; a write to a built track writes the decoded blocks back; a 400K single-sided image builds side 0 only
- [ ] T041 [US3] Implement `Gcr35Track` in `CassoEmuCore/Devices/Floppy35/Gcr35Track.h/.cpp`
- [ ] T042 [US3] Find where `Disk2Controller` and `WozLoader` apply the write-protect and save rules to a 5.25-inch WOZ image, and write the locations into `research.md` R9
- [ ] T043 [P] [US3] Tests in `UnitTest/Floppy35Tests/Woz35Tests.cpp`: a WOZ 2 3.5-inch image loads per track and side with its bit counts; the INFO chunk's write protection is applied; writes change its bits and are saved by the rules found in T042 (FR-008)
- [ ] T044 [US3] Extend `WozLoader` in `CassoEmuCore/Machines/Apple2/Common/WozLoader.h/.cpp` for the 3.5-inch disk type, and add `Floppy35Media` in `CassoEmuCore/Devices/Floppy35/Floppy35Media.h/.cpp`, saving through the code T042 found
- [ ] T045 [P] [US3] Tests in `UnitTest/BlockDeviceTests/BlockDeviceMediaTests.cpp`: a 3.5-inch WOZ image attached to a SCSI disk or a ProFile is decoded to blocks when every block decodes, and the attach fails with the unreadable block numbers otherwise
- [ ] T046 [US3] Add the WOZ decode to `BlockDeviceMedia` through `Gcr35Track`

### SWIM, the card's CPU and the card

- [ ] T047 [P] [US3] Tests in `UnitTest/Floppy35Tests/DeviceCpuTests.cpp`: a `Cpu65C02` over a separate `MemoryBus` steps at its clock ratio against the host cycle count, parks while inactive ("`Parked` while the device is inactive, `Running` while active"), and resumes on activity
- [ ] T048 [US3] Implement `DeviceCpu` in `CassoEmuCore/Devices/Floppy35/DeviceCpu.h/.cpp`
- [ ] T049 [P] [US3] Tests in `UnitTest/Floppy35Tests/SwimTests.cpp`: IWM mode matches `Iwm` register for register, and the sequence that switches the chip to ISM mode
- [ ] T050 [US3] Implement `Swim` in `CassoEmuCore/Devices/Floppy35/Swim.h/.cpp` over `Iwm`
- [ ] T051 [P] [US3] Tests in `UnitTest/Floppy35Tests/Apple35ControllerCardTests.cpp` for the map checked in research R11: 32 KB RAM at the card CPU's `$0000-$7FFF` and ROM at `$8000-$FFFF`; SWIM at `$0A00-$0A0F`; head select at `$0A40` and `$0A41`; the Apple's `$Cn00-$CnFF` reads card RAM `$7B00-$7BFF` and writes there have no effect; any access to `$C0n0-$C0nF` latches the bank; `$C800-$CBFF` is card RAM at the bank times `$400`; `$CC00-$CFFF` is card RAM `$7C00-$7FFF`; the card CPU runs at 2.046 MHz from reset until the slot page is filled
- [ ] T052 [US3] Implement `Apple35ControllerCard` in `CassoEmuCore/Devices/Floppy35/Apple35ControllerCard.h/.cpp`, and register `apple35-card` and `apple35-drive`
- [ ] T053 [US3] Run 341-0438-A on the card with an 800K Apple 3.5 Drive and a log of SWIM accesses, write into `research.md` R8 whether the firmware depends on ISM mode, and add the ISM register behavior it uses to T049 and T050
- [ ] T054 [US3] Scenario tests in `ScenarioTests/Floppy35BootTests.cpp`: an 800K `.po`, a DiskCopy 4.2 image and a WOZ 2 3.5-inch image boot on a //e and an enhanced //e; a written file round-trips with DiskCopy checksums updated; write the comparison with MAME's `superdrive` card into `validation.md`
- [ ] T055 [US3] Find a WOZ image of a copy-protected 3.5-inch 8-bit disk and confirm it boots on MAME's `superdrive` card. Where it has a stable download, fetch it in `scripts/FetchStockDisks.ps1` with a checksum and add a scenario boot for it; otherwise boot it manually and write the image's source, checksum and result into `validation.md` (SC-004)

---

## Phase 6: User Story 2, UniDisk 3.5 on the //c and the Liron card (Priority: P2)

**Goal**: A UniDisk 3.5 boots on the //c disk port and on the Liron card, and
chains.

- [ ] T056 [US2] Read the SmartPort chapters of the Apple IIc Technical Reference and the Apple IIgs Firmware Reference, and write into `research.md` R10 the bus handshake timing, the packet layout byte by byte, and the chain limit in Apple's manuals and in the //c ROM 4 and Liron firmware
- [ ] T057 [P] [US2] Tests in `UnitTest/Floppy35Tests/SmartPortBusTests.cpp`: REQ on phase 0, bus enable on phases 1 and 3, ACK on the sense line, the packet layout from T056 (sync, `$C3`, 7-byte groups and odd bytes, checksum, `$C8`), INIT assigning unit numbers down the chain, and reset clearing them
- [ ] T058 [US2] Implement `SmartPortBus` and `SmartPortChain` in `CassoEmuCore/Devices/Floppy35/SmartPortBus.h/.cpp`
- [ ] T059 [P] [US2] Tests in `UnitTest/Floppy35Tests/UniDisk35DriveTests.cpp` for the map checked in research R11: 8 KB ROM at `$E000-$FFFF`, gate array at `$0800-$0801` with its bits in each direction, IWM at `$0A00-$0A0F`; and a firmware run that replies to a SmartPort STATUS packet and a READ BLOCK
- [ ] T060 [US2] Implement `UniDisk35Drive` in `CassoEmuCore/Devices/Floppy35/UniDisk35Drive.h/.cpp`: `DeviceCpu`, gate array, internal `Iwm`, `Sony35Drive`, on the chain. Set the clock from the firmware's timing loops against the bit cell, and write the figure into `research.md` R11
- [ ] T061 [P] [US2] Tests in `UnitTest/Floppy35Tests/LironCardTests.cpp`: the 4 KB ROM (slot page and `$C800` space, released by `$CFFF`), the IWM at `$C080-$C08F` plus the slot offset, and the boot code's rule ("requires its first byte to be 1 and its second to be nonzero, and otherwise returns to the monitor's slot scan")
- [ ] T062 [US2] Implement `LironCard` in `CassoEmuCore/Devices/Floppy35/LironCard.h/.cpp`, and register `liron-card` and `unidisk35`
- [ ] T063 [P] [US2] Tests in `UnitTest/EmuTests/Apple2cDiskPortTests.cpp`: the //c disk port builds a `SmartPortChain` from the machine file's `chain`, SmartPort devices first and at most one 5.25-inch drive last; a chain over the limit fails to load with the limit in the error; the internal drive is first in boot order
- [ ] T064 [US2] Connect the //c disk port to a `SmartPortChain` that ends in an optional 5.25-inch drive (`disk-iic`), in `CassoEmuCore/Machines/Apple2/Apple2c/` and `MachineConfig`, with the chain limit from T056
- [ ] T065 [US2] Scenario tests in `ScenarioTests/Floppy35BootTests.cpp`: an 800K image boots on a //c with a UniDisk 3.5 on the disk port; the same image boots from the Liron card in slot 5 of a ][+, a //e and an enhanced //e; two chained UniDisks are online; a 5.25-inch drive at the chain's end boots; a SmartPort eject call ejects the disk; an empty UniDisk in slot 5 and a floppy in slot 6 boot the floppy (SC-003)

---

## Phase 7: User Story 4, //c Plus (Priority: P2)

**Goal**: The //c Plus boots from its internal 3.5-inch drive at 4 MHz.

- [ ] T066 [P] [US4] Tests in `UnitTest/EmuTests/MigChipTests.cpp` for the map checked in research R12: the 32-byte RAM window at `$CE00-$CE1F`, the advancing window at `$CE20-$CE3F` wrapping at 2 KB, the page reset at `$CEA0`, the internal drive on at `$CC80-$CC9F` and off at `$CCC0-$CCDF`, head select by reads and 3.5-inch select by writes at `$CE40-$CE7F`, and the IWM reset at `$CC40`
- [ ] T067 [US4] Implement `MigChip` in `CassoEmuCore/Machines/Apple2/Apple2cPlus/MigChip.h/.cpp`
- [ ] T068 [P] [US4] Tests in `UnitTest/EmuTests/Apple2cPlusAcceleratorTests.cpp`: unlock by `$5A` four times at `$C05A` and lock by `$A5`; fast by `$08` at `$C05B` and normal by `$08` at `$C05A`; the slowdown mask at `$C05C`, with accesses to a masked slot or the speaker at 1 MHz; Esc down at reset starts normal; and Casso's setting ("Authentic" is 4 MHz when `fast`, 1 MHz otherwise; "2x" is 8 MHz; "Max" is unthrottled), and the Machine tab's clock row for the //c Plus reads 4 MHz
- [ ] T069 [US4] Implement `Apple2cPlusAccelerator` in `CassoEmuCore/Machines/Apple2/Apple2cPlus/Apple2cPlusAccelerator.h/.cpp` within the existing CPU speed modes, and display 4 MHz for the //c Plus in the Machine tab's hardware rows
- [ ] T070 [P] [US4] Tests in `UnitTest/EmuTests/Apple2cPlusTests.cpp`: ROM 05 banking, the internal Apple 3.5 Drive on the IWM through `MigChip`, and the external port chain accepting `apple35-drive`, `unidisk35` and `disk-iic` in the order the firmware supports
- [ ] T071 [US4] Implement `Apple2cPlus` in `CassoEmuCore/Machines/Apple2/Apple2cPlus/Apple2cPlus.h/.cpp` and `Resources/Machines/Apple2cPlus/Apple2cPlus.json`, and register it in the machine catalog
- [ ] T072 [US4] Install the //c Plus ROM in MAME's roms folder from the fetch, boot `apple2cp`, and write into `research.md` R12 the slowdown durations and the external chain's order rules
- [ ] T073 [US4] Scenario tests in `ScenarioTests/Apple2cPlusTests.cpp`: an 800K image boots from the internal drive and from an external Apple 3.5 Drive; a timing loop runs four times as fast as on the //c; write the comparison with MAME's `apple2cp` into `validation.md` (SC-005a)

---

## Phase 8: User Story 6, configure cards and attach images (Priority: P1)

**Goal**: Every card and drive is reachable from the Machine tab, the desk
and the command line, and persists.

- [ ] T074 [P] [US6] (partly done: drive rows, images and the real timing switch, in `HardwarePageTests::BuildNodes_BlockCardDrivesAndRealTiming` and `SettingsPanelStateTests`; offering a card per slot and replacing a card remain) Tests in `UnitTest/Settings/HardwarePageModelTests.cpp`: the device tree lists each card with its ports (SCSI IDs 0-6, chain positions), offers a card only on a machine in its host list with a tip that lists the machines that accept it, confirms before replacing a card in a filled slot, limits a chain to its length limit with the limit in the tip, and has a real timing switch on each hard disk, off by default and saved with the machine
- [ ] T075 [US6] (partly done, see T074) Extend the device tree in `CassoEmuCore/Ui/Settings/HardwarePage.h/.cpp` and `SettingsPanelState` with cards, drives, image attach, and the per-drive real timing switch (FR-023b)
- [X] T076 [P] [US6] Tests in `UnitTest/EmuTests/CommandLineTests.cpp`: `--hd <slot>=`, `--hd <slot>.<unit>=` and `--smartport <n>=` per `contracts/command-line.md`, each error in the fixed error layout, the saved configuration untouched after the run, and both flags as rows of the `--help` dialog, which `CommandLineHelp::GetEmulatorRows` builds from the parser's `s_kEmulatorFlags` table, so a row added there appears in the dialog
- [X] T077 [US6] Implement the flags in core beside `--disk1`, and add their rows to `s_kEmulatorFlags` in `CassoCore/CommandLineParser.cpp`
- [ ] T078 [P] [US6] Tests in `UnitTest/Ui/DriveWidgetStateTests.cpp`: dropping an image on a drive attaches it with the same validation as the Machine tab (FR-017), and a missing image file results in an empty drive and a notice
- [ ] T079 [US6] Extend `DriveWidgetState` and `IDriveCommandSink` in `CassoEmuCore/Ui/` for block devices and 3.5-inch drives
- [X] T080 [US6] Done as `SettingsPanelStateTests::BuildJson_KeepsDriveImagesAndChains`, which caught the Machine tab writing ports back as bare strings and dropping the images. Original text: Persistence test in `UnitTest/EmuTests/MachineConfigTests.cpp`: configure every card and drive, write and reload the machine file, and compare (FR-014)

---

## Phase 9: User Story 7, see, hear and watch every drive (Priority: P2)

**Goal**: Each external drive is a realistic model with the Disk II's
interaction, sounds and events.

### Models

- [ ] T081 [P] [US7] Write `scripts/modelgen/cad_unidisk35.py` from measurements of reference photographs (proportions written in the script), render with `preview.py`, and check the render before wiring
- [ ] T082 [P] [US7] Write `scripts/modelgen/cad_apple35.py` the same way, with a //c Plus color variant for its internal drive
- [ ] T083 [P] [US7] Write `scripts/modelgen/cad_hd20sc.py` the same way, from the owner's reference photograph (`https://macs.billiehawkins.com/wp-content/uploads/2018/11/20sc.jpg`), with the model number left off the mesh
- [ ] T084 [P] [US7] Write `scripts/modelgen/cad_profile.py` in CadQuery, replacing `gen_profile.py`, and update the table in `scripts/modelgen/README.md`
- [ ] T085 [US7] Check every model color against the `DeskSceneModel` palette ("at least a channel clear of the palette" per the modelgen README), bake the models in `Resources/Models/`, and add each to the `BakedMesh` list in `Casso/Casso.vcxproj` and `Casso.rc`
- [ ] T086 [P] [US7] Tests in `UnitTest/Ui/DeskSceneModelTests.cpp` and `DeskSceneLayoutTests.cpp`: each new `DeskDeviceKind` loads with its lamp and, for removable media, its door or slot; automatic placement of every configuration the Machine tab allows has no overlap (FR-024)
- [ ] T087 [US7] Add `UniDisk35`, `Apple35Drive`, `HardDisk20SC` and `ProFile` to `DeskDeviceKind` in `CassoEmuCore/Ui/Scene/DeskSceneModel.h` and place them in `DeskSceneLayout`
- [ ] T088 [P] [US7] Tests in `UnitTest/Ui/DeskSceneLayoutTests.cpp`: the //c Plus scene has its monitor and its internal drive as an `Apple35Drive` in the //c Plus colors, and each drive on its external chain beside it
- [ ] T089 [US7] Build the //c Plus scene in `ShellDeskScene` and `DeskSceneLayout`
- [ ] T090 [P] [US7] Tests in `UnitTest/Ui/DeskSceneFaceLabelTests.cpp`: the HD20SC's front label is `modelLabel` in the position and lettering of the reference photograph: "20SC" for a 20 MB image and for an empty drive, "32SC" for 32 MB, "5SC" for 5 MB
- [ ] T091 [US7] Draw the HD20SC's model number as a face label in `CassoEmuCore/Ui/Scene/DeskSceneFaceLabels.h`, as the Disk II's "DRIVE n" is drawn
- [ ] T092 [P] [US7] Tests in `UnitTest/Ui/DeskSceneHitTesterTests.cpp`: hover, eject-click, body-click and drop on each new drive match the Disk II's behavior and tip wording
- [ ] T093 [US7] Wire hit testing and tips in `DeskSceneHitTester` and `ShellDeskScene`
- [ ] T094 [US7] Compact theme widgets for each new drive in `CassoEmuCore/Ui/Chrome/DriveWidget.h/.cpp`, with tests of the tip and clicks (FR-023)

### Audio

- [ ] T095 [US7] Search for CC0 or CC-BY recordings of a Sony 800K drive, an HD20SC and a ProFile, and read the two drives' service manuals for spin-up and seek times; write the sources, licenses and times into `research.md` R13 and R15, or that none exists
- [ ] T096 [P] [US7] Tests in `UnitTest/Audio/Floppy35AudioSourceTests.cpp` and `HardDiskAudioSourceTests.cpp`: spin-up, seek, stepping and eject events produce sound, with samples and with synthesis; the drive audio switch silences both
- [ ] T097 [US7] Implement `Floppy35AudioSource` and `HardDiskAudioSource` in `CassoEmuCore/Audio/`, panned by desk position through `DriveAudioMixer`, and the sample fetch in `AssetBootstrap`

### Events

- [ ] T098 [US7] Feed the drive lamps and audio sources from the block device and floppy event feeds, with tests that a read lights the lamp and a seek starts its sound
- [ ] T099 [US7] Write a note in `specs/048-smartport-drives/handoff.md` for spec 035 that lists the two feeds, their event structs, and where `MachineHost::AttachObservers` attaches them, so 035's debugger can log them as it logs `Disk2Event` (FR-022, research R14)

---

## Phase 10: Default cards and the stock image

**Purpose**: The owner's clarifications on defaults (FR-014a, FR-014b,
FR-014c, research R18). Last among the feature phases, so every earlier phase
runs on machines as they ship today.

- [X] T100 [P] Tests in `UnitTest/BlockDeviceTests/StockHardDiskImageTests.cpp`: first use creates an empty ProDOS volume of 65,535 blocks with a blank block 0 ("Created on first use, never overwritten"); a second use of an edited image results in a byte-identical file; a create that fails results in an empty drive and a notice. Over an in-memory file system
- [X] T101 Implement `StockHardDiskImage` in `CassoEmuCore/Devices/BlockDevice/StockHardDiskImage.h/.cpp` through the code `casso disk create --size 32M` runs, and resolve `"image": "stock"` to it in `MachineConfig`
- [ ] T102 [P] (SCSI half done: `UserConfigStoreTests::Merge_NewDefaultCard_KeepsTheUsersCardInThatSlot`; the Liron half follows Phase 6) Tests in `UnitTest/EmuTests/MachineConfigUpgradeTests.cpp`: the //e and enhanced //e defaults have the SCSI card in slot 7 with an `hd20sc` at ID 6 on the stock image and the Liron card in slot 5 with one `unidisk35`, enabled; a user copy with slot 5 or 7 configured is not changed in that slot; a user copy with them unconfigured gains the entries; the //c default is not changed; a machine outside a card's verified host list (T025, T065) does not have that card by default
- [ ] T103 (the SCSI entry is in, at version 16 and 11; the Liron entry follows Phase 6) Add the two slot entries to `Resources/Machines/Apple2e/Apple2e.json` and `Resources/Machines/Apple2eEnhanced/Apple2eEnhanced.json`, raise `$cassoMachineVersion`, and update `MachineConfigUpgrade` in `CassoEmuCore/Core/MachineConfigUpgrade.h/.cpp`
- [ ] T104 Update `TestMachine`'s `Slots::AsShipped` and every test that depends on the as-shipped slots, and add scenario tests in `ScenarioTests/BlockDeviceBootTests.cpp`: an as-shipped //e and enhanced //e boot a floppy in slot 6 first, and the stock volume is online in ProDOS
- [ ] T105 [P] Tests in `UnitTest/EmuTests/AssetBootstrapTests.cpp`: the download catalog has every ROM of `contracts/firmware-fetch.md` with its checksum; the two default cards' ROMs are fetched at startup with the machine ROMs; selecting a card or machine whose ROM is absent starts its download; a failed download or a checksum mismatch results in a notice and a machine that runs without that card (FR-002a, FR-016)
- [ ] T106 Add the ROMs to the catalog in `CassoEmuCore/AssetBootstrap.cpp` and start a download from the Machine tab's card and machine selection through `StartupDownloadDialog`

---

## Phase 11: Polish and cross-cutting

- [ ] T107 Performance test in `UnitTest/EmuTests/IdleDevicePerformanceTests.cpp`: a machine with every card attached and idle runs as fast as one without, within measurement noise
- [ ] T108 Owner review of each desk model beside reference photographs (SC-008)
- [ ] T109 Run the 033 block device scenario tests (T573) on the SCSI card (SC-009), in coordination with whoever is working on spec 033's Story 23
- [ ] T110 Confirm that no "to verify" marker is left in `research.md`, and that `data-model.md` matches it
- [ ] T111 `CHANGELOG.md` and `README.md` entries for the new cards, drives and machine, terse, after the owner approves the text
- [ ] T112 Pre-merge gate: `scripts/CheckStyle.ps1 -Mode Tree`, `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis`, `scripts/RunTests.ps1 -Build`, `scripts/RunTests.ps1 -Build -Scenario`, `scripts/RunTests.ps1 -Build -Sanitize -Configuration Release -Scenario`

---

## Requirement coverage

| Requirement | Tasks |
|---|---|
| FR-001 real firmware | T002, T021, T031, T052, T060, T062, T071 |
| FR-002 ROMs fetched | T002, T003 |
| FR-002a ROMs downloaded by the app | T105, T106 |
| FR-003 hardware at register level | T016-T021, T027-T031, T035, T036, T049-T052, T059-T062, T066-T069 |
| FR-004 clean-room | A rule for every task; MAME is read for behavior and its sources are deleted after use |
| FR-005 SmartPort at signal level | T056-T060 |
| FR-006 3.5-inch mechanism | T037-T041 |
| FR-007 block images | T004-T007, T045, T046 |
| FR-008 WOZ 3.5 | T042-T044 |
| FR-009 durable writes | T005 |
| FR-010 capacity limits | T004, T019, T028, T040 |
| FR-013 host machines | T025, T027, T054, T063, T064, T065 |
| FR-013a //c Plus | T066-T073 |
| FR-014 configure and persist | T074-T080 |
| FR-014a, FR-014b, FR-014c defaults | T024, T100-T105 |
| FR-015 card offered per machine | T074 |
| FR-016 missing ROM | T014, T105, T106 |
| FR-017, FR-018 attach rules | T004, T078 |
| FR-019 models | T081-T087 |
| FR-020 desk interaction | T092, T093 |
| FR-021 sounds | T095-T097 |
| FR-022 event feeds | T008-T010, T098, T099 |
| FR-023 compact widgets | T094 |
| FR-023b real timing | T018, T075 |
| FR-024 placement | T086, T087, T088, T089 |
| FR-025 cards in tests | T015 |
| FR-026 MAME comparison | T026, T054, T073 |
| SC-001 | T023, T026 |
| SC-003 | T054, T065 |
| SC-004 | T055 |
| SC-005, SC-005a | T032, T073 |
| SC-006 guest writes | T023, T032, T054, T065 |
| SC-007 | T018, T075 |
| SC-008 | T108 |
| SC-009 | T109 |

## Dependencies and execution order

- Phase 1 then Phase 2 block everything.
- Block stack: Phase 3 (SCSI) and Phase 4 (ProFile) depend only on Phase 2
  and are independent of each other.
- 3.5-inch stack: Phase 5 (IWM, Sony, SWIM, card) before Phase 6 (UniDisk),
  which reuses the IWM, the Sony mechanism and `DeviceCpu`. Phase 7 (//c
  Plus) depends on Phases 5 and 6.
- Phase 8 (configuration) depends on the devices it configures; its tasks
  for a device can follow that device's phase.
- Phase 9 (desk, audio, events): models (T081-T084) can start any time;
  wiring follows each device's phase.
- Phase 10 (defaults) depends on Phases 3 and 6.
- Phase 11 is last.

## Parallel opportunities

- The four CAD models (T081-T084) run alongside any device work.
- Within Phase 2, the media, event and config test tasks marked [P] are in
  separate files.
- The block stack and the 3.5-inch stack share only Phase 2, so they can run
  side by side in separate sessions if the owner chooses.

## Implementation strategy

Every story ships; there is no reduced first release (the owner's rule). The
order gets the most-used device booting first, so each later phase builds on
tested pieces: SCSI (MAME reference, unblocks 033), the ProFile, then the
3.5-inch stack bottom-up, the //c Plus, the UI and desk work per device, and
the default cards last. Push the branch after each checkpoint.
