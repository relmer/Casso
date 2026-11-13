# Research: SmartPort and hard drive devices

Phase 0 decisions for [plan.md](plan.md). Each entry contains the decision, the
reason, and what else was considered.

## R1. Firmware sources

**Decision**: Fetch every Apple ROM from the Apple II Documentation Project
mirror (`mirrors.apple2.org.za`), the source `scripts/FetchRoms.ps1` already
uses, with a SHA-1 check per file. Where MAME has the same ROM, its checksum is
the reference.

| Device | File on the mirror | Size | MAME set |
|---|---|---|---|
| Apple II SCSI Card | 341-0437-A | 16 KB | `a2scsi`, SHA-1 `451c85c4...` |
| Apple II 3.5 Disk Controller Card | `Apple II 3.5 Disk Controller Card ROM - 341-0438-A.bin` | 32 KB | `a2superdrive`, SHA-1 `440c3f84...` |
| UniDisk 3.5 interface card (Liron) | `Apple 3.5 Floppy Disk Drive Interface Card ROM.bin` | 4 KB | none |
| UniDisk 3.5 drive controller | `Apple UniDisk 3.5 ROM.bin` | 8 KB | none |
| ProFile interface card, 5 MB | `Apple II Interface 341-0271-A ROM 2716 for 5MB Profile.bin` | 2 KB | none |
| ProFile interface card, 10 MB | `Apple II Interface 341-0299-B ROM 2716 for 10MB Profile.bin` | 2 KB | none |
| //c Plus | `Apple IIc plus ROM 05 - 342-0625-A - 1988.bin` | 32 KB | `apple2cp` (`341-0625-a.256`), SHA-1 `1a27ae26...` |
| A2Pico | A2retroNET `6502/SMARTPORT.S` and its slot page source, MIT | source | none |

The UniDisk 3.5 drive ROM is cross-checked against Eric Smith's 2018
disassembly (`git.applefritter.com/Apple-2-SW/a2-liron`, `liron.asm`, origin
`$E000`), whose Makefile verifies a byte-exact rebuild by SHA-256. The
disassembly also documents the drive's memory map: a gate array at
`$0800-$0801` (bus enable, write request, head select, SENSE, the SmartPort
latch, drive select) and the IWM at `$0A00-$0A0F`. It also mentions support
for an unreleased DuoDisk 3.5. `liron-if.asm` documents the interface card's
ROM the same way.

**Rationale**: One mirror for all Apple ROMs, the mirror Casso already trusts;
checksums make a wrong file fail loudly.

**Alternatives considered**: Assembling Eric Smith's disassembly at fetch time
depends on the `asl` assembler, an executable download, which the security rules
forbid. Writing firmware for Casso was ruled out by the owner.

## R2. A2retroNET firmware and the Pico side

**Decision**: Vendor A2retroNET's 6502 sources under
`Resources/Firmware/A2retroNET/` with the MIT notice, and assemble them with
Casso's Merlin dialect at build time into an embedded resource. Emulate the
Pico side as a command processor behind the card's register window: the
firmware writes a command to `CTRL`, polls bit 7 of `CTRL`, and streams
parameters and data through `DATA`. Command `$01` runs a ProDOS call and
returns a status code, `$40` runs the configuration menu (the Pico supplies
the 40 by 24 screen text).

**Rationale**: The 6502 side is the real firmware, unmodified. The Pico side
is a microcontroller program whose behavior is in MIT-licensed C, so it can be
read and its behavior reproduced with attribution; Casso does not run
RP2040 code.

**Open item for implementation**: only `SMARTPORT.S` (the `$C800-$CEFF`
space) and `incbin.S` are downloaded. The slot page (`$Cn00`), the register
addresses `CTRL` and `DATA`, and the full command set come from the rest of
the repository, fetched into `%USERPROFILE%\Downloads\Casso\a2retronet`.

**Alternatives considered**: Emulating the RP2040 running A2retroNET's binary
was ruled out: two ARM cores and PIO state machines to run a mailbox whose
behavior is documented in source.

## R3 and R4. CFFA cards: dropped

**Owner's decision (2026-11-13)**: the CFFA 2.0 and CFFA 3000 are out of
scope. The CFFA 3000's firmware is `CFFA.bin` for its AVR microcontroller
plus `CFFA.xsv` CPLD logic, on dreher.net's CD packages, and running it
means emulating the AVR. Either card can be added later; spec 033's CFFA
layout reader stays in Casso Explorer.

## R5. Apple II SCSI Card

**Decision**: Emulate the card as: 16 KB ROM banked into `$C800-$CBFF`, 8 KB
static RAM banked into `$CC00-$CFFF` (per the card's bank register), an NCR
5380 at `$C0n0-$C0n7`, a bank register and DMA/pseudo-DMA port at
`$C0n8-$C0nF`, and a SCSI bus with up to seven targets. Each target is a
`ScsiHardDisk` implementing TEST UNIT READY, REQUEST SENSE, INQUIRY, READ(6),
READ(10), WRITE(6), WRITE(10), READ CAPACITY and MODE SENSE, with the HD20SC's
INQUIRY strings. The exact register map is checked against MAME's boot
behavior and the card's ROM, not copied.

**Rationale**: The 5380 is the documented chip (NCR 5380 data sheet); bus
phases and selection timeouts follow the SCSI-1 standard. The target command
set is what Apple's ROM and ProDOS's SCSI driver issue.

## R6. ProFile

**Decision**: Emulate the ProFile interface card's 2 KB ROM, its 6522-style
parallel handshake (BSY, STRB, CMD, R/W, PARITY), and the ProFile's command
protocol: the `$01/$02/$03/$04` handshake bytes, the 6-byte command block, and
532-byte blocks (512 data, 20 tag bytes). The 5 MB and 10 MB models differ in
block count (9,728 and 19,456) and card ROM.

**Rationale**: The protocol is documented in the Lisa hardware manual and the
ProFile service manual. MAME does not emulate the Apple II card, so the
validation is a boot plus ProDOS file round trips.

## R7. IWM: one implementation

**Decision**: Factor the IWM out of `Disk2Controller`'s IWM mode into
`Floppy35/Iwm`, behind an `IDiskDriveBus` (phase lines CA0-CA2 and LSTRB,
SEL, ENABLE1/2, RD, WR, WRREQ). The //c's internal 5.25-inch drive, the //c
Plus, the Liron card and the UniDisk 3.5's internal IWM use it. The Disk II
tests stay green throughout the move.

**Rationale**: Four hosts, one chip. The Apple IWM specification documents its
mode register (latch mode, asynchronous handshake, 1 or 2 microsecond bit
cells, timer disable, fast clock) and its status and handshake registers.

## R8. SWIM

**Decision**: Emulate the SWIM in IWM mode for 800K GCR, with the mode switch
sequence that selects it. ISM (MFM) mode is emulated far enough to report the
chip and handle the 341-0438-A firmware's probing; 1.44 MB media are out of
scope for this spec, since neither the Apple 3.5 Drive nor the UniDisk 3.5 is
an FDHD.

## R9. Sony 3.5-inch mechanism and media

**Decision**: `Sony35Drive` implements the drive status lines read through
CA0-CA2/SEL (direction, step, motor, eject, disk in place, write protect, two
sides, track 0, tachometer, ready) and the five speed zones of 800K GCR (12, 11,
10, 9 and 8 sectors per track, 16 tracks per zone). Media are bit streams per
track and side: a WOZ 2 3.5-inch image as captured, or tracks built by
`Gcr35Track` from a block image (ProDOS-order or DiskCopy 4.2), with
interleave 2:1. Writes to a built track are decoded back to blocks; writes to
a WOZ track change its bits.

**Rationale**: The zone layout and sector format are in Inside Macintosh and
the Apple IIgs Firmware Reference, which documents the 3.5-inch drive for the
Apple II family.

## R10. SmartPort bus at signal level

**Decision**: `SmartPortBus` models the bus as the host drives it through the
IWM: REQ on phase 0, the bus enable state (phases 1 and 3 high), ACK on the
write-protect sense line, and data as IWM write and read streams. Packets are
framed as Apple's SmartPort protocol defines (sync bytes, `$C3` packet begin,
the 7-to-8 group encoding, checksum, `$C8` end). Daisy chain IDs are assigned
by the INIT sequence. The UniDisk 3.5 receives the packets through its gate
array and internal IWM, so its firmware decodes them, not Casso.

**Rationale**: FR-005. Software that drives SmartPort directly gets the same
handshake as on hardware, and the drive's ROM, not Casso, replies.

## R11. Device 65C02s

**Decision**: The UniDisk 3.5 (2 MHz, 2 KB RAM, 8 KB ROM, gate array, IWM)
and the 3.5 controller card (its 65C02, 8 KB RAM, 32 KB ROM, SWIM, shared
window to the host) each have a `Cpu65C02` over a separate `MemoryBus`. They
run in lockstep with the host's cycle count at their clock ratio, stepped only
while the device is active (bus enabled or motor on) and parked otherwise.

**Rationale**: `Cpu65C02` takes any `MemoryBus`, so a device CPU uses the existing
core. Parking an idle CPU satisfies the performance goal.

## R12. //c Plus

**Decision**: Add `Apple2cPlus` deriving from `Apple2c`: ROM 05 (32 KB, banked
as the //c's), the MIG chip and its RAM window used by the firmware's 3.5-inch
driver, the internal Apple 3.5 Drive on the IWM, the external port chain, and
the accelerator: 4 MHz with a cache, dropping to 1 MHz for disk, speaker and
I/O accesses as the real machine does, with acceleration switchable by Esc at
power-on and by the firmware's control. The machine JSON is
`Resources/Machines/Apple2cPlus/Apple2cPlus.json`.

**Rationale**: The owner chose to add the //c Plus (clarification Q2). MAME's
`apple2cp` boots for reference. The accelerator's slowdown rules come from
the Apple IIc Technical Reference, second edition, which covers the //c Plus.

## R13. Drive sounds

**Finding**: OpenEmulator's samples, the source of the Disk II sounds, cover
only 5.25-inch mechanisms (Alps and Shugart).

**Decision**: Look for permissively licensed recordings (CC0 or CC-BY) of a
Sony 800K drive, an HD20SC and a ProFile, fetched at runtime as the Disk II
samples are. Where none exists, synthesize: a filtered noise bed for the
spin-up and the spindle hum, an impulse train for head stepping, and a motor
sweep for the 3.5-inch eject. The audio sources work with either.

## R14. Event feed and the debugger

**Finding**: The Disk II debug window of spec 006 was removed on 2026-10-10
(`c35609ac3`). The debugger on the unmerged 035 branch builds its disk log on
`IDisk2EventSink` and `Disk2Event`.

**Decision**: Add `IBlockDeviceEventSink` with a `BlockDeviceEvent` record, and
`IFloppy35EventSink` with a `Floppy35Event` record, each with a ring buffer as
`Disk2EventRing` has, and attach them through `MachineHost::AttachObservers`.
The log view itself goes into 035's debugger after 035 merges.

**Owner's decision (2026-11-13)**: the log view goes into 035's debugger. This
spec adds the feeds and no standalone window.

## R15. Real timing setting

**Decision**: Per hard disk, a `realTiming` flag, off by default
(clarification Q3). Off: the target is ready at power-on and completes each
command in the minimum time the bus protocol allows. On: spin-up delay (HD20SC
about 15 s, ProFile about 30 s), average seek and rotational latency per
access. Sounds are audible in both modes.

## R16. Desk scene

**Decision**: Four new `DeskDeviceKind` values: `UniDisk35`, `Apple35Drive`,
`HardDisk20SC` and `ProFile`. Models are CadQuery programs under
`scripts/modelgen/` (`cad_unidisk35.py`, `cad_apple35.py`, `cad_hd20sc.py`,
and `cad_profile.py` replacing `gen_profile.py`), with proportions measured
from reference photographs and written in each script, previewed with
`preview.py` before wiring. They are baked by `MeshCreator` and placed by
`DeskSceneLayout`'s automatic placement. The //c Plus's internal drive is drawn
as an Apple 3.5 Drive in its colors. An A2Pico card gets a drive band
indicator, a 2D strip with a lamp and a tip, in every theme (the owner's
answer on the desk). User arrangement stays with spec 009.

## R17. Command line

**Decision**: `--hd <slot>[.<unit>]=<image>` attaches an image to a block
device unit, and `--smartport <n>=<image>` attaches to the nth device in a
SmartPort chain. Both parse in core, next to `--disk1`. See
[contracts/command-line.md](contracts/command-line.md).
