# Feature Specification: Apple /// Machine

**Feature Branch**: `050-Apple-III` (cut from `035-debugger`; merges to master after 033 and 035)

**Created**: 2026-11-17

**Status**: Draft

**Input**: Owner request, 2026-11-17: emulate the Apple /// as a machine type in
Casso. The CPU's extended addressing, banked memory, the two 6522 VIAs, the
built-in 6551 serial port, the built-in and external floppy drives, every video
mode with the character set in RAM, the keyboard, beeper, 6-bit DAC and clock,
the machine definition and settings, full integration with 035's debugger
including new device panels, and measured 3D models of the Apple /// and the
Monitor /// for the skeuomorphic theme.

## Background

Casso emulates the Apple ][, ][+, //e, enhanced //e and //c. The Apple ///
(1980) uses the same 6502 family and the same disk format, and is otherwise a
different computer: memory is banked in 32K units, the zero page and the stack
can be moved, indirect addressing can cross banks through an extension byte,
the system is configured through two 6522 VIAs, the character set is in RAM,
and the operating system is SOS.

Three existing pieces are reused. The Mockingboard's 6522 model serves as the
two system VIAs. The Super Serial Card's 6551 model serves as the built-in
RS-232-C port. The Disk II drive and nibble engine serve as the built-in drive
and the external Disk /// drives; the drive selection is new.

Related work on other branches, none of it an Apple /// emulator:

- 033's User Story 29 is a CassoLink client program that runs on a real or
  emulated Apple ///. This machine can run it once both are merged.
- 048 (on 033) has the ProFile drive, the ProFile 5 MB and 10 MB 3D models,
  and SOS volume reading. This feature attaches 048's ProFile to the Apple ///.
- 047 (on 035) attaches a 6551 port to a COM port, to TCP and to a modem. The
  Apple ///'s port gets those attachments when 047 merges.

The branch is cut from `035-debugger` because the debugger work is a large
part of the feature: every debugger pane written for a 64K Apple II address
space has to work with banks, a moved zero page and a moved stack.

### Sources and the clean-room rule

The emulation is written from Apple's published Apple /// documentation: the
Apple /// Owner's Guide, the SOS Reference Manual, the SOS Device Driver
Writer's Guide, the Apple /// Service Reference Manual, the Standard Device
Drivers Manual, the Apple /// Plus Owner's Guide, and the data sheets of the
6502, 6522, 6551 and MM58167. No GPL emulator source is read, MAME's included.
MAME is used only as a running program, for comparing output on the same disk.

Where an address or a bit assignment appears in this spec, the documents above are
the authority. The plan cites the document and page for each one, and a
difference between this spec and the documents is resolved in favor of the
documents.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Boot SOS and run Business BASIC (Priority: P1)

A user picks the Apple /// in the machine list, puts a Business BASIC disk in
the built-in drive and turns the machine on. The ROM's diagnostics run, SOS
loads its kernel, interpreter and drivers, the character set appears, and the
Business BASIC prompt comes up. The user types a short program and runs it.

**Why this priority**: Every other part of the feature depends on SOS booting.
SOS uses the bank register, the moved zero page, extended addressing, both
VIAs, the RAM character set and the disk drive during boot.

**Independent Test**: Boot a Business BASIC disk headless, type a program that
prints a computed value, and compare the screen text with the correct text.

**Acceptance Scenarios**:

1. **Given** an Apple /// with its ROM installed and a Business BASIC disk in
   the built-in drive, **When** the machine is turned on, **Then** the Business
   BASIC prompt appears with no error message.
2. **Given** the Business BASIC prompt, **When** the user types and runs
   `10 PRINT 6*7` and `RUN`, **Then** `42` appears.
3. **Given** a 128K machine and a 256K machine, **When** each boots the same
   disk, **Then** SOS reports the memory size of each correctly.
4. **Given** no disk in the built-in drive, **When** the machine is turned on,
   **Then** the ROM's no-disk screen appears, as on the real machine.
5. **Given** the machine with no ROM file installed, **When** the user picks
   it, **Then** Casso reports the missing ROM as it does for the other
   machines and does not start a machine with a blank ROM.

---

### User Story 2 - Read and write disks in four drives (Priority: P1)

A user has disks in the built-in drive and in up to three external Disk ///
drives. SOS lists them as `.D1` to `.D4`. The user copies files between them
with System Utilities, formats a blank disk, and finds the changes in the image
files afterward.

**Why this priority**: Booting proves reading from one drive. Real use also
involves writing, formatting and more than one drive.

**Independent Test**: Boot System Utilities, format a blank image in `.D2`,
copy a file from `.D1` to it, and read the file back from the image with
Casso's disk tools.

**Acceptance Scenarios**:

1. **Given** disks in all four drives, **When** SOS lists the volumes, **Then**
   each of `.D1` to `.D4` reports the volume in that drive.
2. **Given** a file copied to a disk in an external drive, **When** the image
   is read on the PC, **Then** the file is there with the same bytes.
3. **Given** a blank image, **When** System Utilities formats it, **Then** the
   format completes and the image is a valid SOS volume.
4. **Given** a write-protected image, **When** SOS writes to it, **Then** SOS
   reports the write-protect error.
5. **Given** a machine set up with one external drive, **When** SOS accesses
   `.D3`, **Then** SOS reports a missing device, as on a real machine without that drive.

---

### User Story 3 - See every video mode (Priority: P1)

A user runs software that uses each of the Apple ///'s video modes: 40-column
black and white text, 40-column text with 16 foreground and background colors,
80-column text, 280 by 192 black and white graphics, 280 by 192 graphics with
16 colors, 140 by 192 graphics with 16 colors, and 560 by 192 black and white
graphics. Each one looks as it does in MAME on the same disk. Text uses
whatever character set the program loaded.

**Why this priority**: Every other result is judged on the screen, and
text is unreadable until the RAM character set mechanism works.

**Independent Test**: For each mode, run a disk that draws in it, capture the
frame before the CRT effects, and compare it with MAME's frame of the same
moment.

**Acceptance Scenarios**:

1. **Given** each video mode, **When** the same disk runs in Casso and in
   MAME, **Then** the two frames match pixel for pixel by color index.
2. **Given** a program that loads a different character set, **When** it
   finishes loading, **Then** all text on the screen is drawn with the new
   glyphs.
3. **Given** the Monitor ///, which is monochrome, **When** a color mode is on
   the screen, **Then** each of the 16 colors is drawn as its gray level.
4. **Given** a color monitor chosen in settings, **When** a color mode is on
   the screen, **Then** the 16 colors are drawn in color.
5. **Given** the environment register's video enable bit off, **Then** the
   screen is blank.
6. **Given** a program that scrolls with the smooth scroll feature, **Then**
   the text moves one scan line at a time.

---

### User Story 4 - Type, hear and tell the time (Priority: P2)

A user types on the PC keyboard and every Apple /// key is available,
including the numeric keypad, Alpha Lock, the two Apple keys and the four arrow
keys. The beeper sounds, output written to the 6-bit DAC is audible,
SOS stamps files with the date and time, and a joystick works in either port.

**Why this priority**: These complete the machine for interactive use, after
boot, disks and video.

**Independent Test**: A test disk reads every key and modifier, writes a tone
through each sound path, reads the clock and reads both joystick ports; the
results are compared with the input sent and the PC's time.

**Acceptance Scenarios**:

1. **Given** each key on the Apple /// keyboard, **When** the mapped PC key is
   pressed, **Then** the program reads that key's code and modifier flags.
2. **Given** a key pressed and not released, **Then** it repeats at the documented rate, and
   faster with the Apple key that speeds up repeat.
3. **Given** a program that writes a waveform to the DAC, **Then** the
   waveform is audible at the right pitch at both CPU speeds.
4. **Given** a file saved under SOS, **Then** the file's date and time are the
   PC's date and time.
5. **Given** a PC game controller, **Then** a program reads its axes and
   buttons on port A or port B, as chosen in settings.
6. **Given** Control with Reset, **Then** the machine resets, and a program
   that turned off the reset key through the environment register is not
   interrupted.

---

### User Story 5 - Use the built-in serial port (Priority: P2)

A user runs a terminal program on the Apple ///. The built-in RS-232-C port
works as the Super Serial Card's 6551 does: the same attachments, the same
serial log in the debugger.

**Why this priority**: The port is part of the machine, 033's Apple /// client
depends on it, and the 6551 model already exists.

**Independent Test**: With the loopback attachment, a SOS program sends bytes
through `.RS232` and reads the same bytes back at each baud rate.

**Acceptance Scenarios**:

1. **Given** the loopback attachment, **When** a program writes to `.RS232`,
   **Then** it reads the same bytes back.
2. **Given** each attachment the Super Serial Card offers, **Then** the
   Apple ///'s port offers it too.
3. **Given** serial traffic, **Then** the debugger's serial log lists it with
   the port labeled as the Apple ///'s built-in port.

---

### User Story 6 - Choose and set up the machine (Priority: P1)

A user picks the Apple /// from the machine list and sets it up in settings:
128K or 256K of memory, how many external drives, what is in each of the four
slots, the monitor, the serial port's attachment and the joystick ports. The
choices apply on the next power-on and are saved.

**Why this priority**: Nothing else is reachable until the machine can be
chosen, and memory size and drive count change what software runs.

**Independent Test**: Set each option, restart Casso, and check that the
machine comes up with those options and that SOS reports the matching memory
size and drives.

**Acceptance Scenarios**:

1. **Given** the machine list, **Then** the Apple /// is listed with its
   release year, and picking it starts it.
2. **Given** the settings, **Then** the memory, drive, slot, monitor, serial
   and joystick options are present and none of the Apple II options that do
   not apply (the Language Card, the 80-column card, the cassette recorder)
   appear.
3. **Given** a change to memory size, **When** the machine restarts, **Then**
   SOS reports the new size.
4. **Given** a disk image dropped on the window or passed on the command
   line, **Then** it mounts in the built-in drive, as for the other machines.
5. **Given** every existing command-line option and menu command that depends
   on the machine, **Then** each one works with the Apple /// or is disabled
   with the reason given.

---

### User Story 7 - Debug an Apple /// program across banks (Priority: P1)

A developer stops a SOS program in the debugger. Every address is
bank-qualified where it has to be. The disassembly follows the code in the
bank it runs in, an indirect instruction that uses an extension byte is
disassembled with the bank and address it will access, the stack pane follows
the stack wherever the program moved it, and the registers pane lists the
environment, bank and zero page registers. Breakpoints and watches accept an
address in a bank and stop only for that bank.

**Why this priority**: The debugger is the reason the branch is based on 035.
Without bank awareness the debugger displays the wrong bytes for most of a
running SOS system.

**Independent Test**: Run a test program that calls a routine in another bank
through extended addressing with a breakpoint on an address in that bank, and
check the stop, the disassembly, the memory window, the stack and the
registers against the known state.

**Acceptance Scenarios**:

1. **Given** a breakpoint on an address in bank 2, **When** the same address
   executes in bank 1, **Then** the machine does not stop; **When** it
   executes in bank 2, **Then** it stops.
2. **Given** a memory window, **When** the user enters a bank and address,
   **Then** the window lists that bank's bytes whatever bank is switched in,
   and an edit changes that bank's byte.
3. **Given** an instruction of the form `LDA (zp),Y` whose extension byte
   enables extended addressing, **Then** the disassembly line includes the
   bank and address the instruction accesses, and a data breakpoint on that
   bank and address stops on it.
4. **Given** a program that moves the zero page and switches to the alternate
   stack, **Then** the stack pane, the call stack, step over and step out
   follow the stack in use.
5. **Given** the registers pane, **Then** the environment, bank and zero page
   registers are listed with a tip decoding each bit, and each can be edited.
6. **Given** a write to the bank register, **When** a breakpoint on bank
   changes is set, **Then** the machine stops with the old and new bank
   reported.
7. **Given** a SOS call (a `BRK` followed by the call number and the
   parameter list address), **Then** the disassembly displays it as one
   statement with the call's SOS name, and step over runs the whole call.
8. **Given** each command mode (Casso, Apple II Monitor, GSSquared, WinDbg),
   **Then** a bank-qualified address is accepted wherever an address is.

---

### User Story 8 - Inspect the Apple ///'s devices (Priority: P2)

A developer opens device panels for the Apple ///: the memory map with every
page's bank, both VIAs with each port bit labeled by its Apple /// function,
the video state with the character set in RAM drawn as a grid of glyphs, the
keyboard, the four floppy drives, the sound paths with a meter for the DAC,
the clock chip, the serial port and the joystick ports. The event logs list
the Apple ///'s disk, input and I/O events.

**Why this priority**: The Apple ///'s behavior is controlled by registers that
Apple II machines lack, and a driver author debugging SOS drivers reads these constantly.

**Independent Test**: Run a test program that sets each register and device to
a known state, and compare every panel's rows with that state.

**Acceptance Scenarios**:

1. **Given** the memory map panel, **Then** each page is colored and labeled
   by what it reads from and writes to: a bank, the system bank, ROM, I/O, or
   write-protected RAM.
2. **Given** the VIA panel, **Then** each VIA's ports, data directions,
   timers, shift register and interrupt registers are listed, and each port
   bit has the label of its Apple /// function.
3. **Given** the character set panel, **When** a program loads a new set,
   **Then** the grid redraws within one frame.
4. **Given** the disk panel, **Then** it has all four drives, each with head
   position, motor and read or write state.
5. **Given** a machine other than the Apple ///, **Then** the Apple /// panels
   are not in the panel list.

---

### User Story 9 - Rewind an Apple /// session (Priority: P2)

A developer rewinds and seeks through an Apple /// session. The machine state
at every point matches the live run, including the bank, environment and zero
page registers, both VIAs, the character set, the drives and the clock.

**Why this priority**: 035's history is how most debugging is done, and a
machine that rewinds wrongly is worse than one that cannot rewind.

**Independent Test**: Record a SOS boot, seek to several points, and compare
the full machine state and the screen with the live run at the same cycles.

**Acceptance Scenarios**:

1. **Given** a stored session, **When** the user seeks to any cycle,
   **Then** memory in every bank, the CPU, both VIAs, the video state and the
   drives match the live run at that cycle.
2. **Given** a program that read the clock, **When** the session replays,
   **Then** the program reads the same time as it did live.
3. **Given** the trace, the heat map and the event logs, **Then** each one
   reports addresses with their banks.

---

### User Story 10 - See the Apple /// on the desk (Priority: P2)

In the skeuomorphic theme the Apple /// is drawn as a 3D model: the main
body with the built-in drive in its bay, without the keyboard. The ProFile
sits on top of the body and the Monitor /// sits on the ProFile. With the
ProFile moved to the side or hidden, the monitor sits directly on the body.
External drives and any other added drives stack to the side. The built-in
drive has the lamp, door and disk interactions of the other drive models.

**Why this priority**: Measured 3D models are a hallmark of Casso, and every
other machine has them.

**Independent Test**: Render the scene in each arrangement and compare the
models' dimensions and details with the source documents and photographs.

**Acceptance Scenarios**:

1. **Given** the Apple /// in the skeuomorphic theme, **Then** the body, the
   ProFile if attached and the Monitor /// are drawn stacked in that order,
   each resting on the one below with no gap and no overlap.
2. **Given** the ProFile moved to the side or hidden, **Then** the monitor
   rests directly on the body.
3. **Given** external floppy drives or additional hard drives, **Then** they
   stack beside the body, never under the body or the monitor.
4. **Given** the built-in drive, **Then** its lamp lights while the drive is
   selected with the motor on, its door opens and closes, and a disk can be
   dropped on it and ejected from it.
5. **Given** the back of the body, **Then** every connector and opening of the
   real back panel is present at its measured position and size.
6. **Given** the emulated screen, **Then** it is drawn on the Monitor ///'s
   tube with that monitor's phosphor.

---

### User Story 11 - Use a ProFile on the Apple /// (Priority: P3)

A user attaches a ProFile 5 MB or 10 MB through a ProFile interface card in a
slot and SOS, with the `.PROFILE` driver on the boot disk, reads and writes it.

**Why this priority**: The ProFile was made for the Apple /// and is part of
the default desk arrangement, but it depends on 048's drive.

**Independent Test**: Boot a SOS disk configured with `.PROFILE`, format the
ProFile, copy files to it and read them back from the image.

**Acceptance Scenarios**:

1. **Given** a ProFile interface card and a ProFile with an image, **When**
   SOS boots with the `.PROFILE` driver, **Then** the ProFile's volume is
   listed and files can be read and written.
2. **Given** the 5 MB and the 10 MB model, **Then** SOS reports the size of
   each correctly.

---

### User Story 12 - Run Apple II software in emulation mode (Priority: P3)

[NEEDS CLARIFICATION: Is the Apple II emulation mode in scope or deferred? In
scope: booting the Apple II Emulation disk turns the machine into a 48K
Apple ][+ until power-off, with the Apple II video modes, keyboard codes and
paddles, and the debugger follows the change.]

### Edge Cases

- A program writes to the environment register so that the ROM, the I/O space
  or the write protection changes under the running code: the next instruction
  fetch reads from the new mapping.
- A program moves the zero page to a page in the banked region or to the page
  the stack is on: reads, writes and the extension page follow the documented
  rule for that case.
- An extension byte with the enable bit off: the instruction addresses the
  current bank as an ordinary 6502 does.
- An extended address that crosses the end of a bank: the access continues in
  the next bank, per the SOS Reference Manual.
- A bank number beyond the installed memory (128K against 256K): reads and
  writes follow the Service Reference Manual's description of absent memory.
- The 1 MHz setting changed in the middle of a disk access or a serial byte:
  disk and serial timing follow real time, not CPU instruction counts.
- Text on the screen before any character set is loaded: drawn from the
  character set RAM's power-on contents, not from a built-in font.
- A drive selected that is not connected: reads return no data and the motor
  and lamp of the other drives are not affected.
- A debugger breakpoint with no bank on an address in the banked region:
  it stops in every bank, and the breakpoint list labels it as all banks.
- A debugger layout saved on an Apple II and opened on the Apple ///, or the
  reverse: panels the current machine lacks are omitted, per 035's FR-053.
- A history seek across a change of the zero page register: the stack and
  call stack panes rebuild for the stack in use at the target cycle.
- A ROM file of the wrong size or an unrecognized revision: reported when the
  machine is picked, with the correct size.
- The ProFile is attached in settings but 048's drive is not in the build
  (before 048 is merged in): see Assumptions.

## Requirements *(mandatory)*

### Functional Requirements

**Machine and memory**

- **FR-001**: Casso MUST offer the Apple /// as a machine type, selectable
  wherever the other machines are: the machine list, settings and the command
  line.
- **FR-002**: The machine MUST have 128K or 256K of memory, chosen in
  settings, organized as the system bank and 32K banks switched in at
  `$2000-$9FFF` through the bank register (`$FFEF`).
- **FR-003**: The environment register (`$FFDF`) MUST control, per its
  documented bits: ROM or RAM at `$F000-$FFFF`, the ROM selected, the primary
  or alternate stack, write protection of `$C000-$FFFF`, the reset key, the
  video signal, the I/O space at `$C000-$CFFF`, and the 1 MHz clock.
- **FR-004**: The zero page register (`$FFD0`) MUST select the page used for
  zero page addressing, and the alternate stack MUST be located from it as
  documented.
- **FR-005**: The ROM MUST be supplied by the user and found, verified and
  reported missing as for the other machines. No Apple ROM is distributed
  with Casso.
- **FR-006**: The machine MUST start from power-on in the documented state of
  every register and MUST run the ROM's diagnostics to completion with no
  failure reported.

**CPU**

- **FR-007**: The CPU MUST be Casso's 6502 with the Apple ///'s extended
  addressing: with the zero page moved, the indirect addressing modes MUST
  read the extension byte for the pointer from the extension page and, when
  the byte enables it, MUST access the bank and address the byte and pointer
  select.
- **FR-008**: Zero page and stack accesses MUST go to the pages the zero page
  register and the environment register select.
- **FR-009**: The CPU MUST run at the documented 2 MHz and 1 MHz rates,
  including the documented slowing during the active part of the video frame
  and during I/O accesses, so that software timing loops, the DAC and the
  disk routines run at the real machine's speed.
- **FR-010**: The 6502 behavior the Apple II machines use MUST NOT change:
  the Dormann and Harte suites MUST pass at full depth with the extended
  addressing code present.

**VIAs**

- **FR-011**: The machine MUST have the two 6522 VIAs at their documented
  addresses, using Casso's 6522 model, with every port line wired to its
  documented function: the environment and zero page registers, the bank
  register, the keyboard and its interrupts, the timers, the sound DAC, the
  video and character set loading controls, the drive and slot signals, the
  clock chip's address lines and the interrupt sources.
- **FR-012**: VIA interrupts MUST interrupt the CPU as documented, including the
  vertical blanking, keyboard, clock, serial and slot interrupt sources.

**Serial port**

- **FR-013**: The machine MUST have the built-in 6551 at its documented
  address, using the Super Serial Card's 6551 model, with every attachment
  that model offers, set in the machine's settings.

**Floppy drives**

- **FR-014**: The machine MUST have the built-in drive and zero to three
  external drives, chosen in settings, using the Disk II drive and nibble
  engine, selected through the Apple ///'s documented drive selection.
- **FR-015**: Every image format the Disk II drive mounts MUST mount in each
  of the four drives, read and write, with the same write-protect and save
  behavior.
- **FR-016**: Each drive MUST have the Disk II's drive sounds.
- **FR-017**: The drives MUST appear in Casso's disk menus and drop targets
  labeled Drive 1 to Drive 4, and the boot disk picker MUST work for the
  built-in drive.

**Video**

- **FR-018**: The machine MUST draw the seven Apple /// video modes: 40-column
  black and white text, 40-column 16-color text, 80-column text, 280 by 192
  black and white, 280 by 192 with 16 colors, 140 by 192 with 16 colors, and
  560 by 192 black and white, selected by the documented switches.
- **FR-019**: Text MUST be drawn from the character set RAM. The RAM MUST be
  loaded only through the documented loading mechanism, and no built-in font
  MUST be substituted for it.
- **FR-020**: Normal and inverse text, the smooth scroll feature, the video
  enable bit and every other documented video control MUST work.
- **FR-021**: On a monochrome monitor the 16 colors MUST be drawn as the gray
  levels the black and white video output produces; on a color monitor they
  MUST be drawn as the 16 documented colors.
- **FR-022**: Video timing MUST produce the vertical blanking signal and
  interrupt the VIAs expose, at the documented frame rate.
- **FR-023**: Screenshot capture, the CRT effects and every other feature
  that consumes the emulated frame MUST work with each mode.

**Keyboard, sound, clock and joysticks**

- **FR-024**: The keyboard MUST return the documented code and modifier flags
  for every key of the Apple /// keyboard, through the documented data, flag
  and strobe addresses, with the documented repeat behavior and keyboard
  interrupt. Each Apple /// key MUST have a PC key mapping, listed in
  settings as for the other machines.
- **FR-025**: Pasted text MUST be typed into the machine as for the other
  machines.
- **FR-026**: The beeper, the speaker toggle and the 6-bit DAC MUST each
  produce sound, mixed as on the real machine.
- **FR-027**: The clock chip MUST be present, readable and settable through
  its documented registers and interrupts, and MUST start from the PC's date
  and time.
- **FR-028**: Joystick ports A and B MUST return the axes and buttons of the
  PC controllers assigned to them in settings, through the documented analog
  input mechanism.

**Slots and the ProFile**

- **FR-029**: The machine MUST have its four slots, each empty or with a card
  chosen in settings from the cards that work in an Apple ///.
- **FR-030**: The ProFile interface card MUST be offered for a slot, and
  048's ProFile 5 MB and 10 MB MUST attach to it and work under SOS's
  `.PROFILE` driver.

**Apple II emulation mode**

- **FR-031**: [NEEDS CLARIFICATION: Apple II emulation mode, in scope or
  deferred. See User Story 12.]

**Machine definition and settings**

- **FR-032**: The machine MUST have a machine definition file like the other
  machines', with its ROM, timing, monitor, memory size, drives, slots and
  ports.
- **FR-033**: The settings pages MUST list the Apple ///'s options and MUST
  NOT list options that apply only to Apple II machines.
- **FR-034**: Every new UI string MUST be in sentence case and MUST be
  approved by the owner before it ships.
- **FR-035**: Every existing feature that depends on the machine type (menus,
  the command line, the debug channel, the MCP adapter, screenshot capture,
  disk tools, game controllers, drive audio, the update check's machine
  report) MUST either work with the Apple /// or be disabled for it with the
  reason given. None MUST fail silently or act on Apple II assumptions.

**Debugger: addresses and banks**

- **FR-036**: The debugger MUST accept and print a bank-qualified address,
  written `bank/address`, wherever it accepts or prints an address: commands
  in every mode, the Go to boxes, breakpoints, watches, bookmarks, symbols,
  data blocks, the trace, the call stack, the heat map and the event logs. An
  address outside the banked region is printed without a bank.
- **FR-037**: Memory windows and the disassembly MUST be able to display any
  bank, whatever bank is switched in, read and written without side effects,
  and MUST also offer the view the CPU has now.
- **FR-038**: A breakpoint, data breakpoint or watch with a bank MUST apply
  to that bank only. One without a bank on an address in the banked region
  MUST apply to every bank and MUST be labeled so.
- **FR-039**: The disassembly of an indirect instruction MUST include, when
  the machine is stopped at it or when the trace stored it, the extension
  byte's effect: the bank and address accessed. Data breakpoints, the heat
  map and the trace MUST attribute the access to that bank and address.
- **FR-040**: The registers pane MUST list the environment, bank and zero
  page registers, editable, each with a tip that decodes its bits. The
  commands that print and set registers MUST include them in every mode.
- **FR-041**: The stack pane, the call stack, step over, step out and the
  stack-based stop rules MUST use the stack the program is using, including
  after the zero page register or the alternate stack bit changes.
- **FR-042**: The memory map bar MUST color and label every page by its read
  and write source for the Apple ///: system bank, a numbered bank, ROM, I/O
  or write-protected RAM.
- **FR-043**: Breakpoints MUST be offered on a change of the bank register,
  the environment register and the zero page register, each optionally for a
  specific value or bit.
- **FR-044**: The disassembler MUST display a SOS call as one statement with
  the call's SOS name and parameter list address, MUST not disassemble the
  call's inline bytes as instructions, and step over MUST run the whole call.
- **FR-045**: The debugger MUST have machine symbols for the Apple ///: the
  I/O addresses, the VIA registers, the ROM's documented entry points and the
  SOS call numbers, written from Apple's documentation.
- **FR-046**: The heat map, the profiler and the trace MUST account for
  banks, so the same address in two banks is two locations.
- **FR-047**: A debugger project MUST store bank-qualified breakpoints,
  watches, bookmarks, symbols and data blocks, and a project made on an
  Apple II machine MUST open as it does today.
- **FR-048**: Source-level debugging MUST work for a program assembled by
  Casso and loaded into a bank: a debug file's addresses MUST be bindable to
  the bank the program loaded in.

**Debugger: device panels and logs**

- **FR-049**: Under 035's FR-049 to FR-053, the Apple /// MUST publish panels
  for: memory and banking (the three registers decoded, each page's source);
  each of the two VIAs (ports with each bit labeled by its Apple /// function,
  data directions, timers, shift register, interrupt flags and enables); the
  video state (mode, each switch, smooth scroll, the beam position); the
  keyboard (data, flags, strobe, interrupt state); the floppy drives (all
  four, with the Disk II panel's rows and the drive select lines); sound
  (beeper, speaker, DAC value); the clock chip (each register, the time, the
  interrupt state); the serial port (the 6551's registers and line states);
  the joystick ports (the selected input, each axis and button); and the
  slots (the card in each).
- **FR-050**: Three visuals MUST be added to those of 035's FR-052: a
  character set view that draws the glyphs in the character set RAM as a
  grid, with the code of the glyph under the pointer; a DAC meter and short
  waveform trace; and the memory map bar's bank display of FR-042.
- **FR-051**: The Disk II event log and disk head graphic MUST cover all four
  drives, with the drive filter listing Drive 1 to Drive 4.
- **FR-052**: The input log MUST list the Apple ///'s keyboard and joystick
  reads at their Apple /// addresses.
- **FR-053**: The serial log MUST list the built-in port's traffic.
- **FR-054**: The I/O log MUST label accesses to the Apple ///'s I/O
  addresses and VIA registers by their Apple /// function.
- **FR-055**: Disk breakpoints (035's User Story 20) MUST work for each of
  the four drives.

**History**

- **FR-056**: 035's history, rewind and seek MUST work for the Apple ///:
  every part of the machine state MUST be captured and restored, including
  every bank, the three registers, both VIAs, the character set RAM, the
  clock chip, the 6551 and the drives.
- **FR-057**: The clock chip's time, like every other input from outside the
  machine, MUST be stored in the input journal so a replay reads the same
  values.

**3D models**

- **FR-058**: The skeuomorphic theme MUST have a 3D model of the Apple ///'s
  main body without the keyboard, with the built-in drive in its bay, built
  to the dimensions in Apple's technical documents.
- **FR-059**: The back panel MUST be modeled with every connector and opening
  of the real one, each at its measured position and with its real form: the
  power switch and power inlet, the black and white video jack, the color
  video connector, the audio jack, joystick ports A and B, the RS-232-C
  connector, the external floppy connector and the four slot openings.
- **FR-060**: A card installed by default MUST be modeled where its bracket
  or cable appears in a slot opening.
- **FR-061**: The theme MUST have a 3D model of the Apple Monitor ///, built
  to the dimensions in Apple's documents, with the emulated screen drawn on
  its tube and its phosphor in the monitor catalog. It MUST be the Apple
  ///'s default monitor.
- **FR-062**: The theme MUST have a 3D model of the external Disk /// drive,
  with the Disk II model's lamp, door and disk interactions.
- **FR-063**: The built-in drive MUST have the same lamp, door, drop and
  eject interactions as the other drive models.
- **FR-064**: The desk arrangement MUST be: the body on the desk; the ProFile
  on the body when attached and not moved or hidden; the Monitor /// on the
  ProFile, or directly on the body when the ProFile is to the side, hidden or
  absent; external floppy drives and any additional hard drives stacked to
  the side.
- **FR-065**: The ProFile 5 MB and 10 MB models MUST fit the top of the body
  as the real units do: the footprints align, and no part of one passes
  through the other.
- **FR-066**: Each model MUST be generated by a script with its dimensions as
  constants, with a README listing the source documents and
  photographs, as the existing models are.
- **FR-067**: The flat themes MUST draw the Apple /// with their existing
  drive and status widgets, extended to four drives.

**Testing and verification**

- **FR-068**: Unit tests MUST cover the memory mapping for every combination
  of the environment register's mapping bits, the extended addressing rules,
  the stack and zero page relocation, each VIA wiring, the drive selection,
  each video mode's decoding, the character set loading, the keyboard codes,
  the clock chip and the debugger's bank-qualified addresses.
- **FR-069**: The scenario suite MUST boot SOS on the emulated Apple /// and
  check the Business BASIC prompt, a format, and a file copy between drives.
- **FR-070**: A script MUST compare a Casso frame with a MAME frame of the
  same disk for each video mode, headless, and report the differing pixels.

### Key Entities

- **Apple /// machine**: the machine type, with a memory size, a ROM, the
  drives, the slots and the monitor.
- **Bank**: one 32K unit of memory that appears at `$2000-$9FFF` when
  selected, and the system bank that is always present.
- **Environment, bank and zero page registers**: the three VIA-backed
  registers that select the memory mapping, the stack, the clock rate and the
  zero page.
- **Extension byte**: the byte paired with a zero page pointer that enables
  extended addressing and selects the bank.
- **Bank-qualified address**: a bank and a 16-bit address, the form every
  debugger feature uses for a location in the banked region.
- **Character set RAM**: the glyphs the text modes draw, loaded by software.
- **Drive selection**: which of the four drives the disk controller is
  addressing.
- **Device panel**: 035's rows-and-visuals description of a device's state.
- **Desk model**: a measured 3D model and its place in the stack: the body,
  the ProFile, the Monitor ///, the Disk ///.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A Business BASIC disk boots to the prompt on a 128K and on a
  256K Apple ///, and a typed program runs.
- **SC-002**: The ROM's diagnostics and the Apple /// Confidence Test disk
  report no failure for memory, the VIAs, the serial port, the keyboard, the
  clock and the disk drive.
- **SC-003**: System Utilities formats a disk and copies files between any
  two of the four drives, and the files read back from the images byte for
  byte.
- **SC-004**: For each of the seven video modes, Casso's frame and MAME's
  frame of the same disk at the same moment match on every pixel by color
  index.
- **SC-005**: A timing test program measures the same duration, within 1%, as
  the documented cycle counts predict, at 2 MHz and at 1 MHz.
- **SC-006**: A DAC tone at a known frequency is measured within 1% of that
  frequency in Casso's audio output.
- **SC-007**: A bank-qualified breakpoint stops only in its bank across 1,000
  executions of the same address in alternating banks.
- **SC-008**: Every Apple /// device panel updates within one frame of the
  device's state changing, as 035's SC-015 requires.
- **SC-009**: A seek to any of 100 random cycles of a stored SOS boot
  produces a machine state identical to the live run at that cycle.
- **SC-010**: Each 3D model's overall dimensions are within 1 mm of the
  source documents, and each back panel connector is within 1 mm of its
  measured position.
- **SC-011**: The Apple II machines are not slowed: their emulation speed
  with this feature merged is within measurement noise of the speed before.
- **SC-012**: The Dormann and full-depth Harte suites pass.

## Assumptions

- The feature ships whole, in one merge, after 033 and 035. There is no
  reduced first release.
- The original Apple /// is the machine. The Apple /// Plus (256K, a changed
  keyboard, interlaced video, the clock chip as standard) is covered by the
  256K setting and the clock chip; its keyboard and interlace are not
  included unless the owner adds them.
- The clock chip is fitted on every emulated machine, although many original
  units shipped without one.
- User Story 11 and FR-030 depend on 048's ProFile drive and models, which
  are on 033's branch. The ProFile work is done after this branch has taken a
  merge that includes 048; the rest of the feature does not depend on it.
- The ProFile is attached by default, with its interface card in a slot,
  which is why the default desk arrangement includes it.
- 047's COM port, TCP and modem attachments work on the Apple ///'s port once
  047 and this branch are both merged; no work for them is in this feature.
- SOS, Business BASIC and the other Apple disks are Apple's software and are
  never committed. The scenario suite fetches the disks it boots, as it does
  the DOS 3.3 and ProDOS masters.
- MAME is installed on the owner's PC with an Apple /// ROM set and is run
  headless for comparisons. The owner approves before MAME or Casso is
  launched on screen.
- Frame comparison with MAME is by color index before any CRT effect, since
  the two programs' palettes and filters differ.
- The model sources are Apple's technical documents for dimensions and
  connector positions, with photographs for details those documents omit.
- Out of scope: the Silentype printer, the Apple /// Plus keyboard and
  interlace, third-party memory beyond 256K, Business BASIC or Pascal
  source-level debugging, a SOS file browser in Casso Explorer beyond what
  048 provides, and cards other than the ProFile interface card.
