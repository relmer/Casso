# Implementation Plan: SmartPort and hard drive devices

**Branch**: `048-smartport-drives` | **Date**: 2026-11-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/048-smartport-drives/spec.md`

## Summary

Add the cards and drives that boot ProDOS volumes larger than 140K: the Apple
II SCSI Card with an HD20SC, the ProFile on its interface card, the CFFA 2.0
and CFFA 3000, the A2Pico running A2retroNET, the UniDisk 3.5 (on the //c disk
port and on the Liron card), the Apple 3.5 Drive (on the 3.5 controller card),
and the //c Plus as a new machine. Every device runs its real firmware over
emulated hardware: an NCR 5380 and a SCSI target, an ATA task file, the
A2Pico's register window, the IWM and SWIM, a variable-speed Sony 3.5-inch
mechanism, the SmartPort bus at signal level, and a second and third 65C02 for
the UniDisk 3.5 and the 3.5 controller card. Each external drive gets a CAD
model, sounds, and an event feed, as the Disk II has.

The approach, from [research.md](research.md): build bottom-up in three
stacks that share only the media layer and the event feed. The block stack
(SCSI, ProFile, CFFA, A2Pico) presents an `IBlockStore` behind a bus chip. The
3.5-inch stack (IWM, SWIM, Sony mechanism, GCR and WOZ tracks) is driven by
host firmware or by a drive's internal 65C02. The machine stack adds the //c
Plus. The desk scene, audio and configuration work lands per device as each
stack boots.

## Technical Context

**Language/Version**: C++ (MSVC v145, C++23 as the solution sets it), Python 3
with CadQuery for the CAD models, PowerShell 7 for scripts.

**Primary Dependencies**: `CassoCore` (6502 and 65C02 cores, the assembler and
its ca65 dialect), `CassoEmuCore` (MemoryBus, slot and expansion ROM buses,
ComponentRegistry, Disk II controller, the block store and partition readers
from spec 033, the desk scene, the drive audio mixer), Dxui for settings UI.
No new third-party code. A2retroNET's `6502/SMARTPORT.S` (MIT) is vendored as
source and assembled by Casso's assembler at build time.

**Storage**: Disk images on the host: `.po`, `.hdv`, `.2mg`, DiskCopy 4.2
(`.dc42`, `.image`), WOZ 2 3.5-inch, CFFA card images and A2Pico card folders.
Writes go through spec 033's `CachingBlockStore` and the durable commit path.

**Testing**: `UnitTest` (Microsoft Native CppUnitTest) for chips, buses,
firmware handshakes and media; `ScenarioTests` for boots on `TestMachine` with
each card over in-memory images; MAME 0.289 reference boots through
`scripts/MameSnapshot.ps1` for the devices MAME emulates.

**Target Platform**: Windows 11 x64 and ARM64 (ARM64 build-only).

**Project Type**: Desktop application over static core libraries.

**Performance Goals**: An idle device costs nothing per cycle (FR-024
assumption and SC-007). A UniDisk 3.5 or 3.5 controller card adds one 65C02
stepped only while its device is active. A 32 MB ProDOS boot with real timing
off is faster than MAME's.

**Constraints**: Clean-room (FR-004). ROMs fetched, never committed (FR-002).
No serial port is opened and none of the CassoLink capture files are touched
by this work. Each `.cpp` file stays under the CS0026 size limit.

**Scale/Scope**: Seven slot cards, the //c disk port chain, one new machine,
four drive models, two audio sources, one event feed.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | How the design satisfies it |
|---|---|---|
| I. Code quality | Pass | EHM throughout; each chip is one class per `.h`/`.cpp` pair; no magic numbers (register maps, timing constants and bus phases are constants). |
| II. Testing discipline | Pass | Tests first per task. Each chip has register-level tests driven by a fake bus; each card has a firmware boot test; each story has a scenario boot. A test without its ROM fails, it does not skip. |
| III. UX consistency | Pass | Drives reuse the Disk II's desk interaction, tips and wording; settings live in the Machine tab's device tree; sentence case. |
| IV. Performance | Pass | Device CPUs and mechanisms run only while active; block devices do no per-cycle work. |
| V. Simplicity | Pass | Three stacks with one shared media layer; the IWM is factored out of `Disk2Controller` rather than written twice. |
| VI. Thin exe, testable core | Pass | Everything is in `CassoEmuCore`. The exe does not gain logic; the command-line flags parse in core. |

No violations. Complexity Tracking is empty.

**Post-design re-check**: Pass. The data model does not add exe-side state, and
every contract in [contracts/](contracts/) is reachable from `UnitTest`.

## Project Structure

### Documentation (this feature)

```text
specs/048-smartport-drives/
├── plan.md              # This file
├── research.md          # Phase 0 decisions
├── data-model.md        # Entities, state machines
├── quickstart.md        # Validation guide
├── contracts/
│   ├── machine-config.md    # JSON for cards, drives and the //c Plus
│   ├── command-line.md      # --hd, --smartport and related flags
│   ├── device-events.md     # IBlockDeviceEventSink and Floppy35Event
│   └── firmware-fetch.md    # ROM catalog entries and checksums
├── checklists/
│   └── requirements.md
└── tasks.md             # /speckit-tasks output
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Devices/
│   ├── BlockDevice/                 # new: the block stack
│   │   ├── IBlockDeviceEventSink.h, BlockDeviceEvent.h, BlockDeviceEventRing.*
│   │   ├── BlockDeviceMedia.*       # attach, validate, write-protect, one-writer rule
│   │   ├── Ncr5380.*                # SCSI controller chip
│   │   ├── ScsiBus.*                # phases, arbitration, selection
│   │   ├── ScsiHardDisk.*           # SCSI target: INQUIRY, READ(6/10), WRITE, MODE SENSE
│   │   ├── AppleScsiCard.*          # 341-0437-A, 5380, RAM, bank latch
│   │   ├── ProFileDrive.*, ProFileCard.*
│   │   ├── AtaDevice.*              # CF task file for the CFFA
│   │   ├── CffaCard.*               # CFFA 2.0 and 3000 bus side
│   │   └── A2PicoCard.*             # A2retroNET register window and Pico side
│   └── Floppy35/                    # new: the 3.5-inch stack
│       ├── Iwm.*                    # factored out of Disk2Controller's IWM mode
│       ├── Swim.*                   # IWM plus ISM mode for the 3.5 controller card
│       ├── IDiskDriveBus.h          # CA0-CA2, SEL, LSTRB, ENABLE, RD, WR, WRREQ
│       ├── Sony35Drive.*            # mechanism: zones, sides, stepping, status, eject
│       ├── Gcr35Track.*             # 800K block image <-> GCR track bits
│       ├── DiskCopy42Image.*        # read, write, header checksums
│       ├── SmartPortBus.*           # REQ/ACK over the phase lines, packet framing
│       ├── UniDisk35Drive.*         # 65C02, gate array, internal IWM, Sony mechanism
│       ├── LironCard.*              # UniDisk 3.5 interface card
│       └── Apple35ControllerCard.*  # 65C02, SWIM, RAM, 341-0438-A
├── Machines/Apple2/
│   ├── Apple2cPlus/                 # new machine: MIG, accelerator, internal 3.5 drive
│   └── Common/Disk2Controller.*     # uses Floppy35/Iwm for //c IWM mode
├── Audio/
│   ├── Floppy35AudioSource.*
│   └── HardDiskAudioSource.*
├── Ui/Scene/                        # DeskDeviceKind additions, drive band indicator
├── Ui/Settings/                     # device tree: cards, IDs, drives, real-timing switch
└── Core/                            # ComponentRegistry, MachineConfig, CLI flags

Resources/
├── Firmware/A2retroNET/             # SMARTPORT.S, LICENSE (MIT), assembled at build
├── Machines/Apple2cPlus/Apple2cPlus.json
└── Models/{UniDisk35,Apple35Drive,HardDisk20SC,ProFile}/

scripts/modelgen/
├── cad_unidisk35.py, cad_apple35.py, cad_hd20sc.py
└── cad_profile.py                   # replaces gen_profile.py

scripts/FetchRoms.ps1                # new catalog entries

UnitTest/BlockDeviceTests/, UnitTest/Floppy35Tests/
ScenarioTests/BlockDeviceBootTests.cpp, Floppy35BootTests.cpp, Apple2cPlusTests.cpp
```

**Structure Decision**: Two new device folders in `CassoEmuCore/Devices`, one
per stack, and a machine folder for the //c Plus. Test folders mirror them.
The IWM is factored out of `Disk2Controller` into `Floppy35/Iwm` so the //c,
the //c Plus, the Liron card and the UniDisk 3.5 run one IWM.

## Delivery order

Each step ends at a boot that a scenario test can check.

1. Shared: ROM catalog, media layer, event feed, registry and config plumbing.
2. Block stack: A2Pico first (firmware in the tree, unblocks spec 033's
   T573), then the SCSI card (MAME reference), then the CFFA 2.0, the ProFile.
3. 3.5-inch stack: IWM factor-out with the Disk II tests green, the Sony
   mechanism and GCR tracks, the 3.5 controller card with the Apple 3.5
   Drive, then the SmartPort bus, the UniDisk 3.5 on the //c and on the Liron
   card.
4. The //c Plus.
5. Desk models, audio, drive band indicator, Machine tab and command line,
   per device as it boots.
6. CFFA 3000, after the owner's decision on its firmware (research R4).

## Complexity Tracking

No constitution violations.
