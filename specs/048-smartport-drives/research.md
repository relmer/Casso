# Research: SmartPort and hard drive devices

Phase 0 decisions for [plan.md](plan.md). Each entry contains the decision, the
reason, and what else was considered. Each hardware fact is marked with where
it comes from:

- **Checked**: read from the ROM, from a disassembly, or from MAME 0.289's
  device (read for behavior, not copied), or measured in MAME on 2026-11-13.
- **To verify**: from memory or a secondary source. A task checks it against
  the ROM or Apple's documentation before code depends on it.

## R1. Firmware sources

**Decision**: Fetch every ROM from the Apple II Documentation Project mirror
(`mirrors.apple2.org.za`), the source `scripts/FetchRoms.ps1` already uses,
with a checksum per file. Where MAME has the same ROM, its SHA-1 is the
reference.

| Device | File on the mirror | Size | Checksum reference |
|---|---|---|---|
| Apple II SCSI Card (Rev. C ROM) | 341-0437-A | 16 KB | MAME `a2scsi`, SHA-1 `451c85c46b92e6ad2ad930f055ccf0fe3049936d` |
| Apple II 3.5 Disk Controller Card | `Apple II 3.5 Disk Controller Card ROM - 341-0438-A.bin` | 32 KB | MAME `a2superdrive`, SHA-1 `440c3f84176c7b9f542da0b6ddf4fb13ec326c46` |
| UniDisk 3.5 interface card (Liron) | `Apple 3.5 Floppy Disk Drive Interface Card ROM.bin` | 4 KB | a2-liron, SHA-256 `42c1ae66c6bec932669239599eb8989a5364752d56fa5c8997bb23d8dfc0b657` |
| UniDisk 3.5 drive controller | `Apple UniDisk 3.5 ROM.bin` | 8 KB | a2-liron, SHA-256 `9f9f9160938f0b69c6f532f9acf194e3b48de8b195b6b47faad8423d6c1e3cce` |
| ProFile interface card, 5 MB | `Apple II Interface 341-0271-A ROM 2716 for 5MB Profile.bin` | 2 KB | none; SHA-1 pinned at the first fetch, `d9d4a7a2c14232c3872f9bbf55aff1b31f9a1cb5` |
| ProFile interface card, 10 MB | `Apple II Interface 341-0299-B ROM 2716 for 10MB Profile.bin` | 2 KB | none; SHA-1 pinned at the first fetch, `81750f023f964c2f3781d687cee5b1ea5b49d4ea` |
| //c Plus | `Apple IIc plus ROM 05 - 342-0625-A - 1988.bin` | 32 KB | MAME `apple2cp`, SHA-1 `1a27ae26966bbafd825d08ad1a24742d3e33557c` |

The two a2-liron checksums are from the Makefile of Eric Smith's 2018
disassembly (`git.applefritter.com/Apple-2-SW/a2-liron`), which rebuilds both
ROMs byte for byte.

**Checked** (fetched 2026-11-13): every file on the mirror matches its
reference. The mirror's Liron and UniDisk 3.5 files have the a2-liron SHA-256
values; their SHA-1 values, which the fetch checks, are
`fa94ecc2aa23f336f7da9e18bc8f4a6cfbb4ea60` and
`69f3dee914027eb2fa6eda512f8f5c649ad4c227`. The //c Plus video ROM,
342-0265-A, has SHA-1 `b2b5d87f52693817fc747df087a4aa1ddcdb1f10`. The SCSI
card's file on the mirror is `Apple II SCSI Card ROM Rev. C - 341-0437-A.bin`
in `Interface Cards/SCSI Controllers/Apple II SCSI Card/ROM Images`.

**Alternatives considered**: Assembling the disassembly at fetch time depends on
the `asl` assembler, an executable download, which the security rules forbid.
Firmware written for Casso was ruled out by the owner.

## R2. A2Pico: dropped

**Owner's decision (2026-11-13)**: the A2Pico is out of scope. No other
emulator models it (MAME 0.289, MAME's current source and AppleWin have no
A2Pico or A2retroNET device), so there is no reference boot. A2retroNET's 6502
firmware is MIT licensed, and the Pico side is a command processor behind a
`CTRL` and `DATA` register pair, which is the starting point if the card is
added later.

## R3 and R4. CFFA cards: dropped

**Owner's decision (2026-11-13)**: the CFFA 2.0 and CFFA 3000 are out of
scope. The CFFA 3000's firmware is `CFFA.bin` for its AVR microcontroller
plus `CFFA.xsv` CPLD logic, on dreher.net's CD packages, and running it
means emulating the AVR. Either card can be added later; spec 033's CFFA
layout reader stays in Casso Explorer.

## R5. Apple II SCSI Card

**Checked** against MAME's `a2scsi` device:

| Address | Function |
|---|---|
| `$C0n0-$C0n7` | NCR 5380 registers, in the chip's order |
| `$C0n8` | Pseudo-DMA data read and write, with DACK |
| `$C0n9` | Read: the card's SCSI ID as a bit mask (`$80` for ID 7) |
| `$C0nA` | Write: bank latch. Bits 0-3 select a 1 KB ROM bank, bits 4-6 a 1 KB RAM bank. Cleared at reset |
| `$C0nB` | Write: reset the 5380 |
| `$C0nC` | Write: IIgs block mode on (a read of `$C800-$CBFF` then returns the next DMA byte) |
| `$C0nD` | Write: pseudo-DMA mode (block mode off) |
| `$C0nE` | Read: DRQ in bit 7 |
| `$Cn00-$CnFF` | The first page of the ROM |
| `$C800-$CBFF` | Banked RAM, 8 KB in all |
| `$CC00-$CFFF` | Banked ROM, 16 KB in all |

The first draft of this plan had the RAM and ROM windows the other way round.

**Decision**: `AppleScsiCard` implements that map. `Ncr5380` implements the
chip from its data sheet, and `ScsiBus` the SCSI-1 phases. IIgs block mode is
decoded and has no effect, since Casso has no 65816. Each target is a
`ScsiHardDisk`: TEST UNIT READY, REQUEST SENSE, INQUIRY, READ(6), READ(10),
WRITE(6), WRITE(10), READ CAPACITY and MODE SENSE.

**Checked** (Casso, 2026-11-13, a command log on the target): booting ProDOS
1.0.1 and ProDOS 2 from a hard disk at ID 6, Apple's ROM issues TEST UNIT
READY twice, INQUIRY, READ(6) of block 0, READ CAPACITY and INQUIRY again,
then ProDOS reads with READ(6). A guest `CREATE` writes with WRITE(6). No
10-byte command, REQUEST SENSE or MODE SENSE was issued; the target executes
them anyway, since other software may issue them.

**Checked** in MAME (enhanced //e, card in slot 7, bootable ProDOS floppy in
slot 6):

- Apple's ROM passes over a hard disk whose block 0 is blank, and the floppy
  boots to the ProDOS prompt.
- `casso disk create --size 32M` without `--bootable` already writes a blank
  block 0, so the stock image of FR-014b is what that command makes.
- Emulated time from power-on to the first instruction of the Disk II boot ROM:
  0.13 s with no card, 1.5 s with the card and no hard disk (MAME's default CD
  drive at ID 1 was attached in that run), and 2.8 s with the card and the blank
  hard disk at ID 6.

**Owner's decision (2026-11-13)**: the delay is accepted and the SCSI card
stays on by default. MAME's card adds 1.4 to 2.7 seconds. Part of that is the ROM's selection timeout for each empty SCSI
ID, which is 6502 code and runs at the speed of the CPU. Casso's figure with
real timing off is measured once the card runs (R18).

**Checked** (Casso, 2026-11-13): the card boots ProDOS on the ][+ and the
unenhanced //e with `PR#7`. Neither machine's ROM starts it at power-on,
since their slot scan looks for the Disk II's signature byte at `$Cn07`,
which the SCSI card's ROM does not have; the enhanced //e's scan boots it at
power-on. A floppy in slot 6 therefore boots first on those two machines
whatever the card has.

**Measured** in Casso with real timing off, from power-on to the first
instruction of the Disk II ROM on an enhanced //e: 0.13 s with no card, 4.8 s
with the card and no hard disk, and 1.2 s with the card and a blank 32 MB hard
disk at ID 6. The empty card is slower than in MAME, whose run had a CD drive
at ID 1; the ROM's wait for an empty bus is 6502 code.

## R6. ProFile

**To verify**, all of it. MAME has no Apple II ProFile card, and nothing here
was checked against the ROMs or Apple's documentation:

- The card: a 2 KB ROM, and a parallel port with handshake lines (BSY, STRB,
  CMD, R/W, PARITY).
- The protocol: the drive's state bytes `$01` to `$04` with the host's `$55`
  acknowledgment, a 6-byte command block, and 532-byte blocks (512 data and
  20 tag bytes).
- Capacities: 9,728 blocks for the 5 MB drive and 19,456 for the 10 MB drive.
- Which Apple II models the card works in.

**Decision**: The first ProFile task disassembles both card ROMs with Casso's
disassembler, reads Apple's ProFile documentation on the mirror, and writes
the register map and protocol into this section. The ProFile's test tasks are
written from that, not from the list above. Validation is a boot and file
round trips, since there is no reference emulator.

**Checked** (2026-11-13): the mirror has the two ROM images and photographs,
and no documentation or schematic for the card. From a disassembly of
341-0271-A:

- The 2 KB ROM maps as the Super Serial Card's does: its last page, offset
  `$700`, at `$Cn00`, and the first `$700` bytes at `$C800-$CEFF`. The `$Cn00`
  page has `$20`, `$00`, `$03` at offsets 1, 3 and 5 and `$55` at offset 7,
  so the unenhanced machines' slot scan does not start it. Its tail is the
  ProDOS block device table: block count `$2600` (9,728) at `$CnFC`, status
  byte `$47` at `$CnFE`, and the driver at `$CnEA`, which jumps to `$C81C`.
  The 10 MB ROM, 341-0299-B, differs in 1,711 of its 2,048 bytes.
- The card's I/O, at `$C080` plus the slot times 16: `+0` written is the data
  out latch; `+1` read is the data in latch; `+2` read is a status byte, bit 7
  the drive's BSY line (1 is idle, which the status call reports as ready)
  and bit 0 set when no drive is connected (the driver returns ProDOS error
  `$28`); `+3` is read and written around each transfer and command.
- The handshake lines are driven by READS of the card's `$Cn00` page:
  the driver reads `$Cn01` and `$Cn03` before sending the command block,
  `$Cn05` and `$Cn07` after it, `$Cn03` then `$Cn00` after writing the `$55`
  acknowledgment, and `$Cn05`, `$Cn07` and `$Cn04` while waiting for a state
  byte. **Checked** by running the ROM against a drive model in a test
  machine: the reads set a latch of lines, the address's bits 1 and 2 the
  line and bit 0 its level. Line 2 falling while line 3 is set starts a
  handshake (the drive asserts BSY and offers its state byte), and any
  access to `$Cn00` ends one. The boot code's own fetches from `$Cn00-$Cn07`
  pass with line 3 clear.
- The protocol, as the driver runs it: wait for BSY, read the drive's state
  byte and compare it with `$01` (ready for a command), answer `$55` (or
  `$AA` on a mismatch, then retry); send six bytes, the command (0 read, 1
  write), the block number high, middle and low (high `$FF` for the drive's
  special blocks), a retry count of 3 and a sparing threshold of `$0A`; wait
  for state `$02` for a read, answer `$55`; read four status bytes; then move
  the block through `+1` (read) or `+0` (write) in a loop in the `$Cn00`
  page. **Checked**: the 512 data bytes come first and the 20 tag bytes
  after; the ROM moves the 512 and starts the next handshake, which ends the
  block. A write is command 2 (write with verify), answered by state `$04`;
  after the block the drive offers `$06`, then four status bytes. After
  each finished transfer the ROM sends operation `$FF`, a reset with no
  state handshake after it.
- **Checked**: both ROMs boot ProDOS from a 5 MB and a 10 MB drive in Casso,
  a guest `CREATE` writes the image, and the card boots on the ][+ and the
  unenhanced //e with `PR#5`. The original ][ is not in the card's host
  list, since ProDOS needs 64K.

## R7. IWM: one implementation

**Decision**: Factor the IWM out of `Disk2Controller`'s IWM mode into
`Floppy35/Iwm`, behind an `IDiskDriveBus` (phase lines CA0-CA2 and LSTRB,
SEL, ENABLE1/2, RD, WR, WRREQ). The //c's internal 5.25-inch drive, the //c
Plus, the Liron card and the UniDisk 3.5's internal IWM use it. The Disk II
tests are green throughout the move.

**Checked**: the Liron card addresses its IWM at `$C080-$C08F` plus the slot
offset, and the UniDisk 3.5's controller addresses one at `$0A00-$0A0F`, both
in the Disk II's switch order (a2-liron).

**To verify**: the mode register bits, against Apple's IWM specification.

## R8. SWIM

**Checked**: the 3.5 controller card has a SWIM (the 1987 chip, MAME's
`swim1`) clocked at 15.6672 MHz, and MAME attaches high-density 3.5-inch
drives to it. The card dates from 1991 and is also known as the SuperDrive
card.

**Decision**: Emulate the SWIM in IWM mode for 800K GCR, with the sequence that
switches it into ISM mode. **To verify**: how far the 341-0438-A firmware
depends on ISM mode when only an 800K Apple 3.5 Drive is attached. If the
firmware cannot start without it, ISM register behavior is added; 1.44 MB
media stay out of scope either way.

**Checked** (2026-11-13, 341-0438-A on Casso's card with a trace of the
card CPU): the firmware can't start without ISM mode. After its ROM checksum
it runs a SWIM self-test at `$9697`: IWM mode register writes and reads, then
mode bit 6 written 1, 0, 1, 1 (`$4F`, `$0F`, `$4F`, `$4F`), then in ISM mode
the phase register must read `$F0` and the mode register `$4x`, and all
sixteen parameter RAM bytes must read back what was written, with the
counter reset by a write to the mode-zeros register. `$F8` written to mode
zeros returns the chip to IWM mode, and everything after the test is in IWM
mode. A failure writes `JMP $FF59` to `$7B00` and blinks the LED. Only that
ISM subset is implemented, in `Iwm` itself rather than a `Swim` class over
it, since the firmware uses ISM mode only for the test.

**Update**: the card CPU's page table is mapped to the card's RAM and ROM.
`MemoryBusCpu` maps `$0000-$BFFF` to the CPU's built-in RAM array at
construction, which on a device bus would hide the device's memory.

## R9. Sony 3.5-inch mechanism and media

**Decision**: `Sony35Drive` implements the drive status lines read through
CA0-CA2/SEL (direction, step, motor, eject, disk in place, write protect, two
sides, track 0, tachometer, ready) and the five speed zones of 800K GCR (12, 11,
10, 9 and 8 sectors per track, 16 tracks per zone). Media are bit streams per
track and side: a WOZ 2 3.5-inch image as captured, or tracks built by
`Gcr35Track` from a block image (ProDOS-order or DiskCopy 4.2), with
interleave 2:1.

Writes to a built track are decoded back to blocks. Writes to a WOZ track
change its bits and are saved by the rules the Disk II applies to a 5.25-inch
WOZ image (the owner's clarification). **To verify**: where those rules are in
`Disk2Controller` and `WozLoader`, so the 3.5-inch drive calls the same code.

**To verify**: the status line numbering and the sector format, against the
Apple IIgs Firmware Reference and Inside Macintosh.

**Checked** (2026-11-13, NetBSD's `sys/arch/mac68k/obio/iwmreg.h`, BSD
licensed, which restates Apple's table): the register index is CA1, CA0,
SEL, CA2 from bit 3 down. Status, read on the IWM's sense line, 0 for yes:
`$0` step direction (0 in), `$1` and `$3` select head 0 and head 1, `$2` disk
in place, `$4` stepping, `$6` write protected, `$8` motor on, `$9`
single-sided, `$A` at track 0, `$B` head loaded and ready, `$D` drive
installed, `$E` tachometer (60 pulses a revolution), `$F` HD disk. Commands
are strobed with LSTRB high then low, CA2 the value: `$0` step direction
in, `$1` step direction out, `$4` step, `$8` motor on, `$9` motor off, `$D`
eject. The head is SEL, which is index bit 1.

**Checked** (2026-11-13, against 341-0438-A booting ProDOS): the data field
checksum group's first nibble has c3's top bits in bits 4-5, c2's in bits
2-3 and c1's in bits 0-1, then c3, c2 and c1. A sector in the other
order fails the firmware's checksum. It also strobes commands `$3`, `$7` and `$A` (SEL high), which are a
disk-switched reset and SuperDrive controls; an 800K drive ignores them.

## R10. SmartPort bus at signal level

**Decision**: `SmartPortBus` models the bus as the host drives it through the
IWM: REQ on phase 0, the bus enable state (phases 1 and 3 high), ACK on the
write-protect sense line, and data as IWM write and read streams. Packets are
framed as Apple's SmartPort protocol defines (sync bytes, `$C3` packet begin,
the 7-to-8 group encoding, checksum, `$C8` end). The UniDisk 3.5 receives the
packets through its gate array and internal IWM, so its firmware decodes them,
not Casso.

**Checked** (a2-liron): the bus commands are STATUS `$00`, READ BLOCK `$01`,
WRITE BLOCK `$02`, FORMAT `$03`, CONTROL `$04`, INIT `$05` (which assigns unit
numbers), and OPEN, CLOSE, READ and WRITE for character devices. The card's
receive code counts 7-byte groups and odd bytes, and reads a packet type, a
source ID and a destination ID.

**Checked**: the Liron card's boot code reads block 0, requires its first byte
to be 1 and its second to be nonzero, and otherwise returns to the monitor's
slot scan when it was entered from that scan. An empty UniDisk 3.5 therefore
does not stop a floppy in slot 6 from booting. Entered by `PR#n`, it prints
"NO DISK TO BOOT".

**Decision on chain length**: Casso allows up to four SmartPort devices on a
chain, the number ProDOS 8 2.x mounts from one slot. **To verify**: the limit
in Apple's UniDisk 3.5 and //c manuals, and in the //c ROM 4 and //c Plus
firmware. The Machine tab uses the lower of the two.

## R11. Device 65C02s

**Checked** (a2-liron, MAME `a2superdrive`):

| | UniDisk 3.5 controller | 3.5 controller card |
|---|---|---|
| ROM | 8 KB at `$E000-$FFFF` | 32 KB at `$8000-$FFFF` |
| RAM | below `$0800` | 32 KB at `$0000-$7FFF` |
| I/O | gate array at `$0800-$0801`, IWM at `$0A00-$0A0F` | `$0A00-$0AFF`: SWIM at `$0A00-$0A0F`, head select low `$0A40` and high `$0A41`, diagnostic LED `$0A80/$0A81` |
| Clock | **To verify** (2 MHz per a secondary source) | 2.046 MHz (the slot's 7 MHz clock times 2/7) |

The UniDisk's gate array (a2-liron's labels): register 0 has LASTONE, BUSEN,
WRREQ, GATENBL and HDSEL; register 1 reads SENSE, BLATCH1, BLATCH2, LIRONEN
and CA0, and writes RSTIWM, BLATCH, DRIVE1 and DRIVE2. The firmware also
supports an unreleased DuoDisk 3.5, which is the reason for two drive selects.

The 3.5 controller card has no ROM on the Apple's side. The Apple's view of
the card is windows into the card's RAM, which the card's 65C02 fills:

| Apple address | Card RAM |
|---|---|
| `$C0n0-$C0nF` | Any access latches the low 4 address bits as the bank for `$C800-$CBFF` |
| `$Cn00-$CnFF` | `$7B00-$7BFF`, read-only from the Apple |
| `$C800-$CBFF` | 1 KB at the latched bank times `$400` |
| `$CC00-$CFFF` | `$7C00-$7FFF` |

The first draft of this plan had 8 KB of RAM for the card.

**Decision**: Each device has a `Cpu65C02` over a separate `MemoryBus`,
stepped in lockstep with the host's cycle count at its clock ratio. It runs
only while the device is active (bus enabled, motor on, or a host access to
the card's windows pending) and is parked otherwise. The 3.5 controller card's
CPU also runs from reset until its slot page is filled, since the Apple
cannot find the card before that.

## R12. //c Plus

**Checked** against MAME's `apple2cp`:

- ROM 05, 32 KB. The CPU is a 65C02 with a licensed Zip Chip accelerator at
  4 MHz.
- An internal Apple 3.5 Drive (800K, with no controller inside) on the IWM,
  with a chip called MIG. The external port takes a IIgs-style chain.
- MIG, in `$CC00-$CFFF`: writes to `$CC40` reset the IWM; `$CC80-$CC9F` turn
  the internal drive on and `$CCC0-$CCDF` off; `$CE00-$CE1F` is a 32-byte
  window into 2 KB of MIG RAM; `$CE20-$CE3F` is the same window with the page
  advanced after each access; `$CEA0` resets the page; `$CE40-$CE5F` and
  `$CE60-$CE7F` select head 0 and 1 on a read, and 5.25-inch or 3.5-inch drives
  on a write.
- Accelerator registers, compatible with the Zip Chip: `$5A` written four times
  to `$C05A` unlocks; `$08` to `$C05B` selects fast and `$08` to `$C05A`
  normal; `$A5` to `$C05A` locks; `$C05C` is the speaker and slot slowdown
  byte, which the firmware sets to `$67` (slots 1, 2, 5, 6 and the speaker
  slow). The firmware stores its state in MIG RAM. The Zip Chip's firmware
  checks for Esc at reset.
- The 3.5-inch disk code runs at the fast speed; per MAME's notes that is the
  reason for the MIG.

**Decision**: `Apple2cPlus` derives from `Apple2c`, with `MigChip`, the
internal drive, the external chain, and `Apple2cPlusAccelerator` implementing
those registers and temporary slowdowns. Casso's speed setting (the owner's
clarification): "Authentic" is the real machine, 4 MHz with its slowdowns,
and the Machine tab displays 4 MHz; "2x" is 8 MHz; "Max" is unthrottled. With
the accelerator switched to normal by software or by Esc, "Authentic" runs at
1 MHz, as the real machine does.

**To verify**: the slowdown durations, and the external chain's order rules
(3.5-inch drives before SmartPort devices before 5.25-inch drives).

## R13. Drive sounds

**Checked**: OpenEmulator's samples, the source of the Disk II sounds, cover
only 5.25-inch mechanisms (Alps and Shugart).

**Decision**: A task looks for permissively licensed recordings (CC0 or CC-BY)
of a Sony 800K drive, an HD20SC and a ProFile, fetched at runtime as the Disk
II samples are. Where none exists, synthesize: a filtered noise bed for the
spin-up and the spindle hum, an impulse train for head stepping, and a motor
sweep for the 3.5-inch eject. The audio sources work with either.

## R14. Event feed and the debugger

**Checked**: the Disk II debug window of spec 006 was removed on 2026-10-10
(`c35609ac3`). The debugger on the unmerged 035 branch builds its disk log on
`IDisk2EventSink` and `Disk2Event`.

**Decision**: Add `IBlockDeviceEventSink` with a `BlockDeviceEvent` struct, and
`IFloppy35EventSink` with a `Floppy35Event` struct, each with a ring buffer as
`Disk2EventRing` has, attached through `MachineHost::AttachObservers`.

**Owner's decision (2026-11-13)**: the log view goes into 035's debugger. This
spec adds the feeds and no standalone window.

## R15. Real timing setting

**Decision**: Per hard disk, a `realTiming` flag, off by default (the owner's
clarification). Off: the target is ready at power-on and completes each
command in the minimum time the bus protocol allows. On: spin-up delay and
average seek and rotational latency per access. Sounds are audible in both
modes. **To verify**: the spin-up and seek times of the HD20SC and the ProFile,
from their service manuals.

## R16. Desk scene

**Decision**: Four new `DeskDeviceKind` values: `UniDisk35`, `Apple35Drive`,
`HardDisk20SC` and `ProFile`. Models are CadQuery programs under
`scripts/modelgen/` (`cad_unidisk35.py`, `cad_apple35.py`, `cad_hd20sc.py`,
and `cad_profile.py` replacing `gen_profile.py`), with proportions measured
from reference photographs and written in each script, previewed with
`preview.py` before wiring. They are baked by `MeshCreator` and placed by
`DeskSceneLayout`'s automatic placement. The //c Plus's internal drive is drawn
as an Apple 3.5 Drive in its colors. User arrangement stays with spec 009.

**Owner's clarification**: every SCSI disk is an HD20SC, and the model number
on its front follows the image: the size in MB, then "SC". The number is a
face label drawn by the scene, not part of the mesh, as the Disk II's "DRIVE n"
label is (`DeskSceneFaceLabels.h`), so one baked model serves every size.
Reference photograph:
`https://macs.billiehawkins.com/wp-content/uploads/2018/11/20sc.jpg`.

## R17. Command line

**Decision**: `--hd <slot>[.<unit>]=<image>` attaches an image to a block
device unit, and `--smartport <n>=<image>` attaches to the nth device in a
SmartPort chain. Both parse in core, next to `--disk1`. See
[contracts/command-line.md](contracts/command-line.md).

## R18. Default cards and the stock image

**Owner's clarifications**: the //e and enhanced //e ship with the SCSI card on
in slot 7 with an HD20SC at ID 6, and the Liron card on in slot 5 with one
UniDisk 3.5. The HD20SC has a stock image, an empty 32 MB ProDOS volume with a
blank boot block. The //c's default does not change.

**Decision**:

- The embedded machine files for the //e and enhanced //e gain the two slot
  entries, enabled, and their `$cassoMachineVersion` increases.
  `MachineConfigUpgrade` adds them to a user copy only where slots 5 and 7 are
  unconfigured.
- The stock image is created on first use under the user's Casso data folder,
  through the code `casso disk create --size 32M` runs. It is never
  overwritten once it exists.
- `TestMachine`'s `Slots::AsShipped` then includes both cards, which depend on
  fetched ROMs. The ROMs join the `-Fixtures` set, and tests that boot a
  floppy on an as-shipped machine check that the floppy boots first.
- A missing card ROM on a default card is a notice, not a failed start: the
  machine runs without that card (FR-016).

**Boot delay (FR-014c)**: MAME's figures are in R5. A SCSI boot task measures
Casso's figure with real timing off and writes it into `validation.md`. The
owner set no limit on it.

## R19. Host machines for each card

| Card | Machines | Status |
|---|---|---|
| Apple II SCSI Card | ][+, //e, enhanced //e | **Checked**: boots on all three in Casso; on the ][+ and //e `PR#7` starts it (R5) |
| UniDisk 3.5 interface card | ][+, //e, enhanced //e | The ROM is 6502 code (a2-liron assembles it for the 6502); boots **to verify** in Casso |
| 3.5 controller card | //e, enhanced //e | **To verify**; Apple sold it for the //e and IIgs |
| ProFile interface card | **To verify** (R6) | |

The Machine tab's list for each card is set from the verified result, and a
machine is removed from a card's list when the boot fails there on real
firmware.
