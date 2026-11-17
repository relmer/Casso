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

**Not yet read**: the scanned documents with no text layer, which have to be
read page by page: the Service Reference Manual, the schematics, the SOS
Device Driver Writer's Guide, the Standard Device Drivers Manual, the
Owner's Guides and the Monitor /// manual. They are the source for the
environment register's bits, each VIA line, video mode switches and color
encoding, timing, the keyboard codes, and every model dimension.

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
from `Apple3` and replaces the keyboard layout, the memory options and the
interlace capability. Both are added to `MachineDefinitions` with resource
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

1. Its constructor changes the addressing mode of the affected opcodes in its
   instruction table to two new `GlobalAddressingMode` values, the extended forms of
   zero-page indirect Y and zero-page X indirect (Apple's rule covers both).
   The shared fetch switch in `Cpu.cpp` gets a case for each. This is how `Cpu65C02` adds its modes, and `Microcode.h` already
   documents that a mode added to the shared enum costs the other cores
   nothing at run time.
2. The new mode's fetch function computes the effective address as the
   ordinary mode does, then calls `Apple3Mmu` to resolve it against the
   extension byte. For `$80-$8E` the MMU maps the one page the access touches
   from the bank pair for the rest of the instruction; for `$8F` from bank 0
   with the RAM under the VIA registers visible; otherwise it does nothing.
   `Cpu6502Apple3::Step` restores the mapping after the instruction. The
   instruction's read or write then goes through the normal bus path, so
   watch pages, data breakpoints, the heat map and the trace observe it.

`CpuFactory` creates it for the CPU type the Apple /// machines return.
`Cpu.h` and the Apple II cores get no new member and no new test.

**Rationale**: it follows the hardware (a remap for one access, outside the
chip) and the code base's precedent for a CPU variant. The Apple II cores
never execute the new case, so their speed and their Dormann and Harte
results cannot change. The 6502 itself is not duplicated.

**Alternatives not taken**:

- A null-tested extension pointer in the shared indirect fetch (this plan's
  first draft). It works, but it adds a member and a test to every machine's
  CPU for one machine's benefit.
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

## R24. Order of work and merges

**Decision**: memory and CPU first, then VIAs, video and keyboard up to the
ROM's diagnostics screen, then the disk controller up to a SOS boot. The
debugger's address work runs beside the machine work because it does not
depend on SOS booting. Device panels, history, sound, clock, serial and
joysticks follow the boot. Emulation mode, the Plus and Business BASIC
debugging come next. Models are independent of all of it. The ProFile is
last, after a merge from `035-debugger` that includes 048. Merges come only
from `035-debugger`, early and often, per `CLAUDE.md`.
