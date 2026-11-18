# Research: Apple /// Machine

Each entry is a decision the plan depends on, why it was made, and which
alternative was not taken. Hardware facts are in `contracts/hardware-map.md`, each with its
source and whether it has been checked against an Apple document.

## R1. Documents and their verification status

**Decision**: The first implementation task is to collect Apple's documents
into `%USERPROFILE%\Downloads\Casso\apple3\docs` (never the repo) and to
check every row of `contracts/hardware-map.md` against them, adding the
document and page to each row. No device code is written from an unchecked
row.

**Status on 2026-11-17**: 36 documents are in the docs folder, downloaded
from `mirrors.apple2.org.za/ftp.apple.asimov.net/documentation/apple3/` and,
for the Service Reference Manual, from the Apple II Documentation Project
folder on the same mirror. Text was extracted from those with a text layer.
The hardware map's **Checked** column is filled for what these confirmed:
the SOS Reference Manual volume 1 (memory, banks, enhanced indirect
addressing, zero pages and stacks, speeds), Apple's SOS 1.3, console driver
and RS-232 driver source listings (register and I/O addresses, drive
selection, the sound port, VIA initialization), and Apple's specifications
sheet (video modes, connectors, keyboard, CPU type).

**Second pass, 2026-11-17**: text-layer copies of the Device Driver Writer's
Guide and the Standard Device Drivers Manual were added, and the Service
Reference Manual (730 page images) was read by page for the chapters on
memory, the VIAs, clocks and timing, video, the keyboard and Apple II
emulation, with the Monitor III Owner's Manual and the Apple /// Plus
Owner's Guide. These confirmed the environment register's bits, both VIAs
line by line, the video mode switch table and every mode's buffers, the
color byte layout, the 16 colors, character download, the keyboard's two
ports and its full code table, the A/D selection table, line and frame
timing, and the Monitor ///'s dimensions.

**Open after the second pass**: the rows in the hardware map's "Open rows"
table. They are settled from the schematics (`Apple III Schematic
Diagrams.zip`, not yet opened), the PROM listings in chapters 6 and 17 of
the Service Reference Manual, or an existing emulator. No emulator source
has been read yet.

**Third pass, 2026-11-17**: MAME's Apple /// driver (BSD-3-Clause) was read
for the rows Apple's documents do not settle; the facts taken are in the
hardware map's "Settled from an existing emulator" table. The main ones:

- The Apple II lockout is E VIA port A bit 6 driven low, which removes the
  VIAs from `$FFD0-$FFEF`. That is why the three registers cannot change
  afterward, and why only a reset of the VIAs ends it.
- The extension byte is read in the same cycle as the pointer byte, with no
  added cycle, and applies until the next opcode fetch. R4's remap therefore
  ends at the instruction boundary, as planned.
- Smooth scroll takes its three-bit line offset from the disk controller's
  first three phase latches.
- The clock chip's register is selected by the zero page register's value.
- A character is 7 dots by 8 rows; bit 7 of a glyph row selects flashing.

**Fourth pass, 2026-11-17**: Apple's PROM logic expressions (Service
Reference Manual 17.11 to 17.17) and two schematic sheets settled three of
the four rows left open. The 1 MHz rule is the gearshift PROM's equation:
slow for the 1 MHz mode, for the VIAs, ACIA and clock chip, and for RAM
accesses while the display signal is active. E VIA port B bit 6 is the
blanking signal. The machine outputs a color number, drawn as a linear gray
level in monochrome and as the Apple II lo-res color in color. Two rows
stay open: the exact span of the display signal, and `ENSEL`/`ENSIO`
(unused without the Silentype).

**Case dimensions**: no dimensioned drawing exists in the service manual;
its chapter 16 drawings are exploded views. Photographs of a real
Apple /// with a ProFile on top (Trinity College Dublin's collection) are
the source instead. Scaled by the ProFile's documented size, the rear body
is about 150 mm tall, so Apple's stated 4.8 inches is not the rear height.
The back panel's layout and labels are measured from the same photographs.
A square-on side photograph has not been found yet. Details are in
`contracts/desk-models.md`.

**Not yet read**: the Apple /// Owner's Guide (page images), the schematics,
the Service Reference Manual's chapters 7, 11 to 17, and its assembly
drawings, which are the source for the body's and back panel's dimensions.

**Corrections the documents made to the first draft**:

- Enhanced indirect addressing applies to both the X and the Y indirect
  forms, not `(zp),Y` alone.
- The extension byte is enabled by bit 7 alone, with bits 4-6 unused; the
  article's "outside `$80-$8F`" is narrower than Apple's rule.
- The highest bank is 2 for 128K and 6 for 256K. The article's 3 and 7 are
  wrong.
- An extended address whose low part is `$0000-$00FF` goes to the current
  zero page, and with the highest bank the upper half is undefined.
- Apple's notation for a bank-switched address is `b:nnnn`, and for an
  extended address `8x:nnnn` (SOS Reference Manual 2.1.1 and 2.1.2). The
  debugger uses Apple's `b:nnnn`, with `/` in the two command modes where
  `:` is the deposit command (R15).
- The Plus's text interlace is a switch on the left side of the case, not a
  software control. It is a case switch in Casso, as the //c's switches are.
- The Apple II 40-column mode has flashing text as well as inverse.
- The Monitor /// is 17 3/16 by 10 5/8 by 12 1/2 inches (Apple's manual).
  The museum catalog figures quoted earlier were wrong or were of the box.
- The beeper is a 0.1 second 1 kHz tone.
- The keyboard's second port reads its modifier bits active low, and its
  bit layout differs from the byte the console driver returns to programs.
- Apple's Business BASIC and SOS source listings exist. They replace
  observation as the source for Business BASIC's internals (R19).

**Documents to collect**: Apple /// Owner's Guide; SOS Reference Manual
volumes 1 and 2 (chapter 2 of volume 1 has bank-switched and enhanced indirect
addressing, with Table 2-2); SOS Device Driver Writer's Guide; Standard Device
Drivers Manual; Apple /// Service Reference Manual (levels 1 and 2); Apple ///
Plus Owner's Guide; Apple Business BASIC Reference Manual volumes 1 and 2;
Apple II Emulation manual; the ProFile Owner's Manual; data sheets for the
6502A, 6522, 6551 and MM58167A. The apple2.org.za Documentation Project
mirror, already used for the ROM and SOS disks, and archive.org have them.

**Existing emulators** (owner decision, 2026-11-17): where Apple's documents
are silent or ambiguous, an existing emulator with Apple /// support can be
consulted to confirm an assumption or settle the point. Limits:

- Apple's documents come first. An emulator is consulted for a row only after
  the documents have been searched for it.
- Read for the fact, never for code. No code, table or comment is copied, and
  Casso's implementation is written from the fact alone.
- Reading any emulator's source is allowed, GPL included. Copying it is not.
  List the fact's source in the row's **Checked** column as
  `emulator: <project>, <file>`.
- A fact taken from an emulator is confirmed by behavior where it can be: the
  ROM's diagnostics, the Confidence Test disk, or a SOS boot.

Candidates: MAME's `apple3` driver, and any other open-source Apple ///
emulator found during T001. MAME is also run as a program for the frame
comparisons.

## R2. Machine family

**Decision**: `Apple3` implements `IMachine` directly, as a new root beside
the Apple ][ chain, in `CassoEmuCore/Machines/Apple3/`. `Apple3Plus` derives
from `Apple3` and replaces the memory options and the interlace capability.
The Plus keyboard has the Apple ///'s keys and codes (Apple /// Plus Owner's
Guide, chapter 2 and appendix D), so the Plus uses the Apple ///'s layout.
The Plus has 256K (Apple /// Plus Data Sheet). Both are added to `MachineDefinitions` with resource
directories `Resources/Machines/Apple3` and `Resources/Machines/Apple3Plus`.

**Rationale**: `IMachine.h` already cites a second family as the reason
the interface exists. The Apple /// shares chips with the Apple II and
nothing of its memory map.

**Consequence**: `IMachine` has Apple II questions (`HasCassettePort`,
`HasAnnunciators`, `GetGamePortAxisCount`). The Apple /// returns the values
that turn those features off. New questions the Apple /// requires
(`GetBuiltInDriveCount`, `HasBankedMemory`) are added with defaults on the
Apple ][ root.

## R3. Memory and the three registers

**Decision**: A new `Apple3Mmu` contains the RAM as a system bank and
numbered 32K banks, and maps pages into `MemoryBus` through `SetReadPage` and
`SetWritePage` whenever the environment, bank or zero page register changes.
ROM, I/O and write protection are page-table changes too.

**Rationale**: The //e MMU works the same way, so the CPU's inline read path,
the watch pages and the fetch bits apply to the Apple /// with no change.

**Zero page and stack relocation**: Jeppson's PEEK routine moves the zero
page to page 0 before reading a pointer into `$00xx`, which indicates the
hardware substitutes the zero page register for the high address byte on
every access whose page is `$00`, absolute accesses included, and the
alternate stack page on every access whose page is `$01`. If the documents
confirm it, relocation is two page-table entries: bus page `$00` points at
RAM page `Z` and bus page `$01` at the stack page in use. The CPU core is
not changed for relocation.

**Alternative not taken**: A zero page base and stack base inside `Cpu`. It adds an
addition to every zero page and stack access on every machine, and it is not
what the hardware does if the substitution above is confirmed.

**Confirmed** (SOS Reference Manual 2.1.1): the S-bank is `$0000-$1FFF` and
`$A000-$FFFF`; the highest bank is 2 for 128K and 6 for 256K.

**To verify**: the behavior of an absent bank, and the substitution rule
above.

## R4. Extended addressing in the CPU

**Background**: the Apple ///'s processor is a stock NMOS 6502. Extended
addressing is done by logic outside the chip, which recognizes the indirect
opcodes as they are fetched and changes the memory mapping for the data
access of that instruction. So the Apple /// does not have a new CPU; it has
a 6502 with machine-specific address decoding around it.

**Decision**: a `Cpu6502Apple3` class in `CassoEmuCore/Machines/Apple3/`,
derived from `MemoryBusCpu` as `Cpu65C02` is. It is the existing NMOS 6502
with two differences:

1. Its constructor sets an `isExtendedIndirect` flag on every `(zp,X)` and
   `(zp),Y` entry of its instruction table (Apple's rule covers both forms).
   The addressing modes are left as they are, so the disassembler, the
   assemblers, the effective-address code and the debugger, which all switch
   on the mode, are untouched.
2. For a flagged instruction, `Cpu::FetchOperand` calls the virtual
   `ResolveExtendedIndirect` once the pointer is read, and `Cpu6502Apple3`
   passes it to `Apple3Mmu`. For `$80-$8E` the MMU maps the one page the
   access touches from the bank pair; for `$8F` from the system bank and
   bank 0 with the RAM under the VIAs and the I/O space; otherwise it does
   nothing. `Cpu::StepOne` calls `EndExtendedAccess` when the instruction
   completes, which restores the page. The mapping is restored in `StepOne`
   rather than in `Step` because every emulator host calls `StepOne`
   directly. The read or write goes through the normal bus path, so watch
   pages, data breakpoints, the heat map and the trace observe it.

The Apple /// builder creates it, since its constructor takes the memory
map. The Apple II cores test the flag once per indirect instruction and a
`bool` member once per instruction; both are always false for them.

**Validation, 2026-11-17** (T015): with the flag and the hooks in place,
the Dormann functional test and the Harte vectors pass. Both the NMOS core
and the 65C02 ran full depth, 10,000 vectors per opcode, from
`%LOCALAPPDATA%\Casso\HarteTests`. `scripts/GenerateHarteTests.py` writes to `UnitTest/<cpu>/`
by default, which the test runner never reads, so a full set has to be
moved to the cache folder before it runs. The per-instruction timing
comparison has not been run yet.

**The plan's earlier choice, replaced before it was built**: two new
`GlobalAddressingMode` values. They require a new case wherever code switches
on the addressing mode (the disassembler, the assemblers, the
effective-address code and the debugger), and the line assembler cannot
find the Apple /// table's indirect instructions under them.

**Rationale**: it follows the hardware (a remap for one access, outside the
chip) and the code base's precedent for a CPU variant. The Apple II cores
never execute the new case, so their speed and their Dormann and Harte
results cannot change. The 6502 itself is not duplicated.

**Alternatives not taken**:

- A null-tested extension pointer in the shared indirect fetch (this plan's
  first draft). It works, but it puts a pointer to an Apple /// interface in
  every machine's CPU.
- A wholly separate Apple /// CPU class. It would duplicate the 6502 and its
  validation.
- Doing it all in the bus, as the hardware does, by watching opcode fetches.
  Casso's core runs an instruction at a time, not a cycle at a time, so the
  bus cannot identify the data access of an indirect instruction by itself.
- Modifying the 65C02. The Apple /// is NMOS; its undocumented opcodes and
  its `JMP (indirect)` page-wrap behavior are the NMOS ones.

**Confirmed** (SOS Reference Manual 2.1.2 and 2.4.2.2): both indirect forms
are affected; the extension byte is at the extension page offset of the
pointer's high byte; bit 7 alone enables it; a low part of `$0000-$00FF`
goes to the current zero page; past the highest bank the result is
undefined.

**To verify**: whether the extension byte fetch costs a cycle, and what the
hardware does in the undefined cases (an existing emulator may settle the
second).

**Validation**: Dormann and full-depth Harte run against `Cpu6502Apple3`
with extended addressing inactive, which must match the plain 6502 exactly,
and against the existing cores to prove they did not change (SC-011, SC-012).

## R5. Clock rates

**Decision**: The machine counts time in ticks of the 2 MHz clock. A CPU
cycle costs one tick at 2 MHz and two ticks when the environment register
selects 1 MHz or the documented slowing applies. Video, the VIAs, the 6551,
the disk engine and the clock chip are driven from ticks, each at its
documented rate. History's cycle numbers are ticks.

**Rationale**: Disk and serial timing follow real time on the real machine
(the spec's edge case), and the existing devices take a cycle count, so each
is given cycles of the clock it really runs on.

**To verify**: exactly when the CPU is slowed (active display, I/O page
accesses, slot accesses), the VIA clock source, and the frame's tick count.

**Alternative not taken**: Changing `clockSpeed` on a speed switch. Devices would count
the wrong time between switches inside one frame.

## R6. The two VIAs

**Decision**: `Apple3SystemVias` is a `MemoryDevice` at `$FFD0-$FFEF` with
two `Via6522` members. After each register write it compares the port
outputs with their previous values and calls `Apple3Mmu`, the sound DAC, the
character set loader and the other consumers. Inputs (keyboard and slot
interrupt lines, vertical blanking, the clock chip) are set through
`SetPortAInput`, `SetPortBInput`, `SetCa1` and `SetCb1`.

**Rationale**: `Via6522` already exposes its ports, control lines and
interrupt state, saves its state for history, and appends diagnostics
rows. The wiring is Apple /// specific, so it goes in the wrapper.

## R7. The 6551

**Decision**: An `Acia6551` at `$C0F0-$C0F3` (article), created by
`MachineBuilder` and attached through the same port configuration the //c's
serial ports use, under the port name `serial`.

## R8. Floppy drives

**Decision**: A new `Apple3DiskController` reuses `Disk2NibbleEngine`, the
drive mechanics and the event ring, with four drives and the Apple ///'s
drive select lines. Drive numbers in events, panels and logs become 1 to 4.

**To do before design is final**: read `Disk2Controller` for every place the
drive count is two (state arrays, event fields, the debug target's
`DebugDiskState`, the drive filter, the audio sources) and list them in the
tasks.

**To verify**: the select addresses in `$C0D0-$C0DF`, the motor and enable
behavior with several drives, and the external drive daisy chain signals.

**Two-drive survey (T034, `origin/master` at `17f7bb75c`, 2026-11-17)**: 048
(SmartPort drives, merged into master through 033) lifted the limit for
3.5-inch and SmartPort drives only. `MachineHost::FindFloppy35` resolves any
number of units for the //c Plus chain, the //c SmartPort, Liron and the 3.5
controller, and `DriveLabel` prints "unit N" past drive 2. The 5.25-inch path,
the one the Apple /// drives use, is two drives everywhere:

| Place | The two-drive assumption |
|---|---|
| `Disk2Controller.h` | `kDriveCount = 2`; the phase, quarter track, visit, access log, write block, IWM drive, image, active disk and engine arrays are sized by it, several with two-element initializers |
| `DiskImageStore.h` | `kDriveCount = 2` per slot: mounting, persistence and `MachineHost`'s restore loop |
| `DiskManager.cpp` and `.h` | `drive == 0 \|\| drive == 1` asserts on insert and eject; slot 6 only; the eject command chosen per drive (`IDM_DISK_EJECT2`); widget, chrome, write-protect, nibble count and source path arrays of two |
| `ShellDisks.h`, `ShellChrome`, `ShellChromeDriveBar.cpp`, `ShellRenderer.h` | drive widgets, widget state, user write protect and the write-protect and info icon flags are `std::array<..., 2>` |
| `ShellDeskScene` | labels, info icons, their rectangles and label paths are arrays of two; the drive count comes from `MachineConfig::AttachedDiskIiDriveCount` |
| `EmulatorWindow.cpp` | only drive 0 is drawn, plus drive 1 when the external drive is attached |
| `ShellInspectorHost.h`, `InspectorStatusPublisher.h`, `InspectorRequestQueue.h` | `kDriveCount = 2` and arrays sized by it |
| `AutoMountResolver` | the persisted snapshot is two paths |
| `CommandLineParser.cpp`, `CommandLineOptions.h` | `--disk1` and `--disk2` only; a bare image path fills drive 1, then drive 2 |
| `MachineBuilder.cpp` | the Disk II drive count is `Disk2Controller::kDriveCount` |

CassoLink's `kDriveCount = 2` is the real Apple II's drive pair on the serial
link and is out of scope. The 3.5-inch path is the pattern for the change: a
port with an internal drive, a count of external drives, and lookup by index.

## R9. Video and the character set

**Decision**: Seven `IVideoMode` classes in `CassoEmuCore/Machines/Apple3/`.
The text modes read glyphs through a small glyph source interface, which
`Apple3CharacterSet` (the RAM) implements. The loader follows the documented
mechanism; nothing else writes the RAM. In a monochrome style a color index
is drawn as one of 16 gray levels; in the color style as the documented
color.

**Emulation mode**: the existing Apple II text, lo-res and hi-res classes are
used, with the text mode reading glyphs through the same interface, since the
emulation disk loads an Apple II character set into the RAM.

**To verify**: the loading mechanism (the article lists `$FFEC`, `$FFEE`,
`$C0DA` and `$C0DB` and marks them unconfirmed), the glyph count and cell
size, the smooth scroll control, the 16 colors' values and gray levels, where
each graphics mode's buffers are, and the Plus's interlace control.

## R10. Keyboard

**Decision**: `Apple3Keyboard` at `$C000`, `$C008` and `$C010` (article), with
the keyboard interrupt wired to a VIA. Two layouts, `Apple3` and
`Apple3Plus`, are added to the layout tables and the settings key list.

## R11. Sound

**Decision**: Three sources mixed into the existing audio path: the speaker
toggle at `$C030` through `AppleSpeaker`, a beeper at `$C040`, and a 6-bit DAC
on a VIA port, sampled per tick.

**To verify**: the beeper's frequency and duration, the DAC's port and bits,
and the mixing levels.

## R12. Clock chip

**Decision**: A new `Mm58167` device model in `CassoEmuCore/Devices/`,
reusable by any later machine. Host time enters through `InputJournal`, so a
replay reads the stored values.

**To verify**: the chip's address and how the VIA lines select its
registers.

## R13. Joystick ports

**Decision**: An `Apple3AnalogInputs` device reads the PC controllers that
034's controller support assigns to port A and port B.

## R14. Apple II emulation mode

**Decision**: The lockout is a state of `Apple3Mmu`. Once set it changes the
mapping to the 48K Apple ][+ layout, ignores writes to the three registers,
and selects the Apple II video modes and keyboard codes.
Power-off clears it; reset does not. The debug target reports an Apple II
machine while it is set.

**To verify**: the bit that sets the lockout, what clears it, and how the
Apple II paddle and speaker addresses are produced.

## R15. Bank-qualified addresses in the debugger

**Decision**: A `DebugAddress` value type: a 16-bit address and a space. The
space is one of current view, system bank, bank `n`, or ROM. `IDebugTarget`
gets `DebugAddress` forms of `TryPeek`, `TryPoke`, `TryPatch` and `GetRegion`,
and a method that resolves a 16-bit address in the current view to its
`DebugAddress`. The breakpoint, watch, bookmark, symbol, data block and heat
tables store `DebugAddress`. `HeatSpace` gets numbered banks.

**Scale**: the debugger core and its panes have several hundred `Word`
address parameters. The migration is staged: the type and the target first,
then the tables, then the command parsers and formatters of the four modes,
then the panes. A `Word` converts implicitly to a current-view
`DebugAddress`, so each stage compiles and Apple II behavior is the same
after each one.

**Syntax** (owner decision, 2026-11-17): Apple's `b:nnnn`, for example
`2:3000`, everywhere it does not conflict; `b/nnnn` in the Apple II Monitor
and GSSquared modes, where `addr:` starts a deposit; `S:` for the system
bank where it has to be distinguished. See `contracts/debugger-addresses.md`.

**Alternative not taken**: A flat 24-bit address with the bank in the high byte, as a IIgs
debugger would use. The Apple ///'s banks overlap one 32K window, the system
bank is not a numbered bank, and a typed struct stops a bank from being
dropped silently in the places that today pass a `Word`.

## R16. Trace, heat map and history formats

**Decision**: `Cpu::TraceEntry` and `TraceRecord` get the bank of the program
counter and the `DebugAddress` of the data access. The state format version
is raised; an older saved state loads with every address in the current view.

## R17. Disassembly: extension bytes and SOS calls

**Decision**: The disassembler takes an optional machine helper. For an
indirect instruction at the stopped program counter, or one in the trace, the
helper returns the resolved `DebugAddress`, which is appended to the line. A
second helper call recognizes an inline call sequence: for SOS, a `BRK`, a
call number and a parameter list address. The line is printed as the call's
SOS name and the parameter address, and the next line starts after the
inline bytes. Step over uses 035's stack rule and already runs the whole
call.

**Note**: the same helper could later format ProDOS MLI calls on the Apple II
machines. That is not part of this feature.

## R18. Device panels

**Decision**: Each new device implements `IDiagnosticsProvider`. Two payloads
are added to `DiagnosticsSnapshot`: the character set glyphs and the DAC's
recent samples. `DiagnosticsPane`'s graphic kinds get a character set view
and a DAC meter. The memory map bar's page sources get numbered banks, I/O
and write-protected RAM.

## R19. Business BASIC debugging

**Decision**: A `BusinessBasicProgram` beside `ApplesoftProgram`, behind the
interface the BASIC panes and `BasicStopMonitor` use. If the panes use
`ApplesoftProgram` directly, that interface is factored out first. The
release in memory is identified by a signature of the interpreter's code; an
unknown signature is reported as not supported.

**Sources**: the token table and data types come from the Business BASIC
Reference Manual. The interpreter's pointers (program start, variable tables,
the current line, the GOSUB and FOR stack) come from Apple's Business BASIC
source listing in the docs folder, checked by observing the interpreter run
in Casso's debugger.

**Risk**: the listing is of one release; the other releases on the fetched
disks are matched to it by signature and observation. The work is scheduled
after the debugger's bank work, which it depends on.

## R20. 3D models and the desk scene

**Decision**: Three CadQuery generators in `scripts/modelgen/`:
`cad_apple3.py`, `cad_monitor3.py` and `cad_disk3.py`. `DeskDeviceKind` gets
the Apple /// body, the Monitor /// and the Disk ///. `MonitorCatalog` gets
the Monitor /// with green as its phosphor. The ProFile models arrive with
048.

The body is the first computer body in the desk scene: the Apple II machines
are drawn as a monitor and drives only. `DeskSceneLayout` gets a stack: the
body on the ground plane, an optional ProFile on it, the monitor on top, with
the camera solve containing the taller composition. Side drives use the
existing forward row.

**Dimensions known so far**: Apple's specification sheet lists the whole
machine as 17.5 in wide, 18.2 in deep and 4.8 in high (444.5 by 462.3 by
122.0 mm), keyboard included. The upper body's depth, the drive bay, every
back panel connector position and the Monitor ///'s dimensions are not yet
sourced; Computer History Museum catalog entries for the monitor disagree
with each other. See `contracts/desk-models.md`.

**To do**: find the Service Reference Manual's mechanical drawings; where a
dimension is in no Apple document, measure it from photographs as a fraction
of a documented dimension, as `cad_monitor2.py` does, and list it in the
model's README as measured.

## R21. Comparing frames with MAME

**Decision**: `scripts/CompareMameFrame.ps1` runs MAME headless with the
snapshot script from the memory note, runs Casso's headless machine to the
same emulated time, maps each program's pixels to color indices, and reports
the differing pixels. A human starts it; it is not part of the unit suite,
since MAME is not on CI.

## R22. Disks for the scenario suite

**Decision**: `scripts/FetchStockDisks.ps1` also fetches the Business BASIC,
System Utilities, Confidence Test and Apple II Emulation disks into
`Disks/Apple/`, gitignored, by the rules it uses for the DOS 3.3 and ProDOS
masters.

## R23. ROM

**Decision**: `Apple3.rom`, 4K at `$F000-$FFFF`, added to the ROM catalog and
to `FetchRoms.ps1 -Fixtures`. The copy on the owner's PC has CRC `55e8eec9`.
Which revisions exist and whether the Plus uses the same image is to be
verified; the catalog lists each accepted revision.

## R25. The ProFile card on the Apple ///

**Source**: Apple's SOS `.PROFILE` driver 1.30 source listing (`Apple 3
Source - Profile HD Driver.pdf`), read 2026-11-18. The driver is on the System
Utilities disk.

**Facts from the listing**:

- The card's I/O registers are at `$C080` plus the slot offset in X:
  `$C080` writes a byte to the drive's Z8 controller, `$C081` reads one,
  `$C082` is the status (busy, and the parity error in bit 6), and a write to
  `$C083` clears the parity error.
- The handshake lines are set by reads of `$Cn00` plus an offset. Bits 0 and
  1 of the offset select the line, bit 2 is its level, and bit 3 is the
  reset: command 0 and 4, read and write 1 and 5, interrupt 2 and 6, data
  read and write 3 and 7, reset `$0C` and clear `$08`.
- The driver runs at 1 MHz while it accesses the card, and moves a block
  through the pseudo DMA code in the ROM at `$F800`.

**Open**: 048's `ProFileCard` decodes the line offsets with bits 1 and 2
selecting the line and bit 0 its level, from the Apple II card's ROM. The
two decodes disagree, so the Apple /// card, or one of the two readings,
differs. T137 compares the two listings before T138 reuses the drive.

## R24. Order of work and merges

**Decision**: memory and CPU first, then VIAs, video and keyboard up to the
ROM's diagnostics screen, then the disk controller up to a SOS boot. The
debugger's address work runs beside the machine work because it does not
depend on SOS booting. Device panels, history, sound, clock, serial and
joysticks follow the boot. Emulation mode, the Plus and Business BASIC
debugging come next. Models are independent of all of it. The ProFile is
last, after a merge from `035-debugger` that includes 048. Merges come only
from `035-debugger`, early and often, per `CLAUDE.md`.
