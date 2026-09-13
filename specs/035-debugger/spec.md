# Feature Specification: Debugger

**Feature Branch**: `035-debugger`

**Created**: 2026-09-13

**Status**: Draft

**Input**: User description: "Casso debugger (GH #51): an engine that breaks into the running emulator, AppleWin and Apple II Monitor command modes, batch scripting, named-pipe protocol, and GUI debugger window"

## Overview

A debugger that breaks into the machine Casso is already running -- a game, a
boot disk, a program at the `]` prompt -- and shows the whole machine, not only
the CPU.

This replaces the original design in GH #51, which launched a single program
under a standalone debugger. That design fits bare CPU simulators, not an
emulator: every mature emulator debugger (AppleWin, VICE, MAME, Mesen, Stella)
is built into the running emulator. GH #59 (the GUI panel) is merged into this
feature.

One engine, two command modes, three ways in:

- **Command modes**: AppleWin (default) and the Apple II System Monitor.
- **Ways in**: a GUI debugger window with a command line, a batch mode in the
  command-line tool, and a local channel that lets another program attach to a
  running Casso.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Break into a running program from a script (Priority: P1)

A developer, or an automated session diagnosing a bug, runs a disk headlessly,
stops at an address, inspects registers and memory, steps a few instructions,
and resumes -- entirely from a script, with no window.

**Why this priority**: This is the engine and the command language with the
smallest possible surface around them. It is useful the day it lands (bug
diagnosis such as the //c VBL spin needed exactly this), it is fully testable
without a window, and every later story is another way into the same engine.

**Independent Test**: Run the command-line tool in debug mode against a
fixture disk with a script that sets a breakpoint, runs, prints registers,
dumps memory, steps, and exits. Compare output to expected text and to the
structured form.

**Acceptance Scenarios**:

1. **Given** a script `bp C019`, `run`, `r`, **When** the tool runs it against
   a disk that reads $C019, **Then** execution stops at the breakpoint and the
   registers are printed.
2. **Given** a stopped machine, **When** the script steps three instructions,
   **Then** the program counter advances through exactly those instructions and
   each step is reported.
3. **Given** a script whose stop condition never occurs, **When** it runs,
   **Then** the run ends at its cycle budget and reports that the budget was
   reached, rather than hanging.
4. **Given** the same script and disk, **When** it is run twice, **Then** both
   runs produce identical output.
5. **Given** structured output is requested, **When** the script runs, **Then**
   every reply is a structured record containing the same information as the
   text output.

---

### User Story 2 - Use Apple II Monitor syntax (Priority: P1)

A user fluent in the Apple II Monitor switches the debugger to Monitor mode and
works exactly as at a `*` prompt: `300.3FF`, `300L`, `300: A9 00`,
`dest<start.endM`, `^E`. Every Monitor command from every Apple II ROM works on
every machine.

**Why this priority**: Monitor syntax is the platform's own debugging
language, and supporting both it and AppleWin's syntax is a core decision of
this feature. It shares the engine with Story 1 and adds no new surface, so it
belongs in the first delivery.

**Independent Test**: In a batch script, switch to Monitor mode and run each
command in the Monitor command set against a fixture machine; compare output
with the Monitor's documented output format.

**Acceptance Scenarios**:

1. **Given** Monitor mode, **When** the user enters `300.30F`, **Then** memory
   prints in Monitor format (`0300- A9 00 8D ...`).
2. **Given** Monitor mode on a //e (whose ROM has no step command), **When** the
   user enters `300S`, **Then** the machine steps one instruction at $300.
3. **Given** Monitor mode, **When** the user enters `41<300.3FFS`, **Then** every
   address from $300 to $3FF holding $41 is printed.
4. **Given** Monitor mode, **When** the user enters `^E` or presses Ctrl+E,
   **Then** the registers are shown; **and when** the next input is
   `: 01 02 03`, **Then** A, X and Y become $01, $02 and $03.
5. **Given** Monitor mode, **When** the user enters `800.9FFW out.bin` and later
   `800.9FFR out.bin`, **Then** the range is written to and read back from that
   host file.
6. **Given** a breakpoint set in AppleWin mode, **When** the user switches to
   Monitor mode and lists breakpoints, **Then** the same breakpoint is listed.

---

### User Story 3 - Attach to a running Casso (Priority: P2)

A tool running outside Casso -- a script, or the VS Code debug adapter in a
later feature -- connects to an instance the user is already using, sends
debugger commands, receives replies, and is notified when the machine stops.

**Why this priority**: It is the only way into a session the user started
interactively, and the VS Code debug adapter depends on it. It follows Story 1
because it reuses the same commands and replies over a channel.

**Independent Test**: Start Casso, connect to its channel from a script, set a
breakpoint, resume, and confirm a stop notification arrives when the machine
reaches the address.

**Acceptance Scenarios**:

1. **Given** a running Casso, **When** a client connects and sends
   `bp C019`, **Then** it receives a reply with the same content batch mode's
   structured output contains for that command.
2. **Given** a connected client and a set breakpoint, **When** the machine
   reaches it, **Then** the client receives a stop notification without polling.
3. **Given** two Casso instances running at once, **When** a client connects,
   **Then** it reaches exactly the instance it selected and the other is
   unaffected.
4. **Given** a Casso run by a different user account, **When** a client
   connects, **Then** the connection is refused.

---

### User Story 4 - Debug in a window beside the emulator (Priority: P3)

A user opens a debugger window beside the running emulator and debugs with the
mouse and a command line: disassembly with the current line highlighted,
registers, an editable memory view, stack, watches, breakpoints set by clicking
in the disassembly, and buttons for step, step over, run and run to cursor.

**Why this priority**: It is the primary interactive experience, but it depends
on the command widgets being introduced by `032-dxui-command-widgets`, so it
is delivered last. Stories 1-3 deliver a working debugger without it.

**Independent Test**: With a disk running, open the window, click a line in the
disassembly to set a breakpoint, run to it, edit a byte in the memory view, and
confirm the machine sees the change.

**Acceptance Scenarios**:

1. **Given** a running machine, **When** the user opens the debugger window and
   pauses, **Then** disassembly around the program counter, registers, flags,
   stack and memory are shown and the current line is highlighted.
2. **Given** the window is open, **When** the user clicks a disassembly line,
   **Then** a breakpoint is set there and shown in the breakpoint list.
3. **Given** the machine is paused, **When** the user edits a byte in the memory
   view, **Then** the machine's memory contains the new value.
4. **Given** the window's command line, **When** the user enters any command
   valid in the selected mode, **Then** it behaves exactly as it does in batch
   mode.

---

### Edge Cases

- **Syntax collision in Monitor mode**: AppleWin commands written entirely in
  hex characters (`DB`, `CD`, `F`) are hex addresses in Monitor mode. Monitor
  mode reads them as Monitor input; AppleWin commands are not reachable there
  except through the engine-command prefix.
- **`S` forms**: `value<start.endS` is search; `S` and `addrS` are step. No
  step command has the search form.
- **Commands outside the ROM table**: `F666G` opens the mini-assembler on every
  machine instead of jumping to $F666.
- **Filler entries**: the //c ROM's last command-table entry decodes to `Q` but
  is filler (`$EA` / `$00`); it is not a command.
- **Missing filename for `R`/`W`**: the window prompts; batch scripts and
  attached clients receive an error and nothing is read or written.
- **File length mismatch on `R`**: the smaller of the file and the range is
  read, and the mismatch is reported.
- **Unknown or malformed command**: an error is reported and machine state is
  not changed.
- **Mode switch mid-script**: subsequent lines are read in the new mode;
  breakpoints and watches persist.
- **Machine reset or machine switch while paused**: the session reports the
  event; breakpoints survive a reset and are cleared by a machine switch.
- **Client disconnects while the machine is paused**: the machine stays paused
  until resumed from another way in.
- **Lowercase input in Monitor mode**: accepted for commands and hex.

## Requirements *(mandatory)*

### Functional Requirements

**Engine**

- **FR-001**: The debugger MUST break into the machine Casso is running,
  without restarting it or loading a program under a separate debugger.
- **FR-002**: Users MUST be able to pause, resume, step one instruction
  (entering subroutines), step over a subroutine call, and run to a chosen
  address.
- **FR-003**: Users MUST be able to set, list, enable, disable and clear
  breakpoints by address, by opcode, and by condition on registers or memory.
- **FR-004**: Users MUST be able to set watchpoints that stop execution on a
  read of, or a write to, an address or range.
- **FR-005**: Users MUST be able to view and change the registers and flags.
- **FR-006**: Users MUST be able to view and change memory as the CPU currently
  sees it, including which of ROM, RAM or language-card memory is mapped at an
  address.
- **FR-007**: Users MUST be able to view soft-switch state and the stack.
- **FR-008**: Every run started from a script or attached client MUST have a
  cycle budget; reaching it MUST end the run and report the reason.
- **FR-009**: Given the same machine, disk and commands, a batch run MUST
  produce identical results every time.
- **FR-010**: The debugger MUST render any single instruction at any address as
  disassembly, for both 6502 and 65C02, including the undocumented opcodes Casso
  already implements.

**Command modes**

- **FR-011**: The debugger MUST provide two command modes, AppleWin and Apple
  II Monitor, selected by the user; AppleWin MUST be the default.
- **FR-012**: Both modes MUST operate on the same session state: a breakpoint,
  watch or register change made in one mode MUST be visible in the other.
- **FR-013**: Each mode MUST produce output in that mode's own format.
- **FR-014**: Monitor mode MUST provide a prefix that reaches debugger commands
  with no Monitor equivalent, including switching modes.

**AppleWin mode**

- **FR-015**: AppleWin mode MUST accept AppleWin's debugger command syntax for
  the first-version command subset listed in Assumptions.

**Apple II Monitor mode**

- **FR-016**: Monitor mode MUST accept the union of every Apple II Monitor
  ROM's commands, on every machine, regardless of which ROM that machine has.
- **FR-017**: Monitor mode MUST support: examine and range (`addr`,
  `addr.addr`, `.addr`, space, Return); deposit (`addr: bytes`, `: bytes`);
  list (`L`); move (`<...M`); verify (`<...V`); hex arithmetic (`+`, `-`); go
  (`G`); show and change registers (`^E`, then `:`); inverse and normal text
  (`I`, `N`); input and output hooks (`^K`, `^P`); BASIC cold and warm entry
  (`^B`, `^C`); user vector (`^Y`); step (`S`); trace (`T`); search
  (`value<start.endS`); mini-assembler (`!` and `F666G`); read and write (`R`,
  `W`).
- **FR-018**: Each Monitor command MUST perform its documented effect directly,
  independent of the ROM routine that implements it on real hardware.
- **FR-019**: Control-character commands MUST be accepted both as the control
  character and as `^` followed by the letter.
- **FR-020**: `R` and `W` MUST read and write a host file named by an optional
  trailing filename, as raw bytes; `R` MUST read the smaller of file and range
  and report a length mismatch.
- **FR-021**: Monitor mode MUST accept lowercase and uppercase input.

**Ways in**

- **FR-022**: The command-line tool MUST provide a debug mode that runs a
  machine and a script of commands, printing each command's output, with an
  option for structured output.
- **FR-023**: A running Casso MUST accept connections on a local channel
  restricted to the current user, one channel per running instance.
- **FR-024**: Over the channel, clients MUST be able to send any command,
  receive a structured reply with the same content as batch structured output,
  and receive notifications for breakpoint hits, stops and resets.
- **FR-025**: The channel's message format MUST be documented well enough for
  an independent client (the planned VS Code debug adapter) to be written from
  the documentation alone.
- **FR-026**: Casso MUST provide a debugger window beside the emulator with a
  command line in the selected mode, disassembly with the current line
  highlighted, registers and flags, an editable memory view, stack, watches,
  a breakpoint list, click-to-set breakpoints, and controls for step, step
  over, run and run to cursor.

**Verification**

- **FR-027**: A test MUST decode the Monitor command table of every Apple II ROM
  Casso ships and confirm every entry other than filler is a Monitor-mode
  command.
- **FR-028**: A test MUST confirm that disassembling the original Apple ][
  Monitor ROM matches the Monitor listing published in the 1979 *Apple II
  Reference Manual*.
- **FR-029**: The mini-assembler entry address for `F666G` and lowercase
  acceptance on the Enhanced //e and //c MUST be verified against the fixture
  ROMs before being relied on.

### Key Entities

- **Debug session**: The debugger's attachment to one running machine; holds
  breakpoints, watchpoints, the selected mode and the paused/running state.
- **Breakpoint**: A stop condition by address, by opcode, or by an expression
  over registers and memory; can be enabled or disabled.
- **Watchpoint**: A stop condition on a read or write of an address or range.
- **Command mode**: AppleWin or Apple II Monitor; determines how a command line
  is read and how output is written.
- **Command**: One line of input in a mode; produces a reply.
- **Reply**: The result of a command, in text and in structured form.
- **Notification**: An unsolicited message to an attached client: breakpoint
  hit, stop, reset.
- **Cycle budget**: The maximum number of emulated CPU cycles a scripted or
  client-started run may execute.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can stop a running disk at an address and see registers
  and memory in under 1 minute from launching the debugger, using any of the
  three ways in.
- **SC-002**: 100% of Monitor command-table entries across all shipped Apple II
  ROMs are accepted in Monitor mode on every machine.
- **SC-003**: Disassembly of the original Apple ][ Monitor ROM matches the
  published listing for 100% of instructions.
- **SC-004**: Repeated batch runs of the same script produce byte-identical
  output in 100% of runs.
- **SC-005**: No batch script or client-started run can run indefinitely: every
  run ends within its cycle budget.
- **SC-006**: A breakpoint set through any way in stops the machine at the
  correct instruction in 100% of test cases, and is visible from the other two.
- **SC-007**: An independent client written only from the channel
  documentation can set a breakpoint, resume, and receive the stop notification.

## Assumptions

- **Monitor-mode engine prefix**: `/` (unused by every Monitor command). To be
  confirmed in clarification.
- **AppleWin first-version subset** (to be confirmed and fixed against the
  command table in clarification): execution control, breakpoints and
  watchpoints, registers and flags, memory view/enter/fill/search/move,
  disassembly, binary load/save, cycle counting, and help. **Deferred**: symbol
  tables, bookmarks, video view commands (text/graphics mode display), window
  layout, source-level views, color/font/configuration, profiling and
  benchmarking, disk commands, output logging and printing, and AppleWin's own
  script runner (batch mode covers scripting).
- **No standalone console**: interactive command entry is through the window's
  command line; scripting is through batch mode and the channel.
- **The window depends on `032-dxui-command-widgets`** merging first; Stories
  1-3 do not.
- **Casso has no cassette device**, so `R`/`W` use host files. `.wav` is
  reserved for cassette audio in a later feature; this feature does not produce
  audio.
- **There is no promise to leave guest state untouched**: commands such as `I`,
  `N`, `^K`, `^P` and `:` change guest memory by design.
- **Local channel**: a Windows named pipe carrying one structured record per
  line, per the Windows-only platform scope. A per-instance channel is required
  because users run several Casso instances from different worktrees at once.
- **Out of scope**: cdb/WinDbg syntax (its `.` and `!` command families depend
  on host processes, modules and threads a 6502 lacks, and collide with Monitor
  `.` and `!`); gdb syntax; the VS Code debug adapter (separate feature);
  whole-program disassembly (GH #121); the IIgs, C64 and NES monitors, which
  arrive with those machines as additional modes.
- **Existing starting points**: the CPU's instruction trace and the windowless
  host used by the command-line tool.
- **Clean-room**: AppleWin is consulted for command names and behavior only;
  its implementation is not read or copied.

### References

- GH #51 (debugger), GH #59 (merged into #51), GH #121 (whole-program
  disassembly), GH #148 (boot sector listing), GH #54 (VS Code extension).
- AppleWin debugger tutorial:
  https://github.com/AppleWin/AppleWin/blob/master/help/dbg-toc-intro.html
- AppleWin command table:
  https://github.com/AppleWin/AppleWin/blob/master/source/Debugger/Debugger_Commands.cpp
- Apple II System Monitor, *Apple II Reference Manual* (1979), chapter 3:
  https://archive.org/details/Apple_II_Reference_Manual_1979_Apple
- Woz Monitor (Apple-1), a subset of the Apple II Monitor's syntax:
  https://www.sbprojects.net/projects/apple1/wozmon.php
- cdb command reference (out of scope, cited for the rationale):
  https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/commands

### Monitor ROM findings

Read from the ROMs in `UnitTest\Fixtures`: 23 encoded command characters at
$FFCC, handler offsets at $FFE3, dispatched through the routine at $FFBE.

| Command | ROMs whose table has it |
|---|---|
| examine, range, deposit, `L`, `M`, `<`, `V`, `+`, `-`, `G`, `^E`, `I`, `N`, `^K`, `^P`, `^B`, `^C`, `^Y`, space, Return | all |
| `S` step, `T` trace | original ][, //c |
| `S` search (`value<start.endS`) | Enhanced //e |
| `!` mini-assembler | Enhanced //e, //c |
| `R`, `W` | original ][, ][+, //e, Enhanced //e |

- The ][+ and //e (Autostart ROM) dropped step and trace; those slots hold `^Y`.
- The //c dropped `R`/`W` and put `S`/`T` in the freed slots. Its step is the
  original ]['s moved into internal ROM at $CA43 (`T` = `DEC $34`, `JMP $CA43`;
  `S` = `JMP $CA43`), updated for the 65C02's `JMP (abs,X)` and checking Open
  Apple ($C061) to slow and Solid Apple ($C062) to stop.
- The //c's last entry decodes to `Q` but is filler: character byte `$EA`,
  handler byte `$00`, landing on the operand of `DEC $34` at $FE00.
- The Enhanced //e's `S` at $FED7 compares a one- or two-byte value ($42/$43)
  against each address from A1 to A2 and prints matches.
