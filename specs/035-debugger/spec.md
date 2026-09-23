# Feature Specification: Debugger

**Feature Branch**: `035-debugger`

**Created**: 2026-09-13

**Updated**: 2026-09-18

**Status**: Draft

**Input**: User description: "Casso debugger (GH #51): an engine that breaks into the running emulator, AppleWin and Apple II Monitor command modes, batch scripting, named-pipe protocol, and GUI debugger window." Expanded 2026-09-18: when released, the debugger is at least at parity with AppleWin's and GSSquared's, and ahead of both where Casso can be: a window worth releasing, in-place memory editing, Visual Studio-style docking, an instruction trace ring, device diagnostic panels, source-level debugging on a cc65-compatible debug file, expression and value breakpoints, a GSSquared command mode, and cycle profiling. Expanded again 2026-09-18 (design review): a call-stack pane with selectable backtrace mechanisms that report their own breaks, a WinDbg-flavored command mode, a step filter, and one rule for how each mode reaches the engine commands.

## Overview

A debugger that breaks into the machine Casso is already running -- a game, a
boot disk, a program at the `]` prompt -- and shows the whole machine, not only
the CPU.

This replaces the original design in GH #51, which launched a single program
under a standalone debugger. That design fits bare CPU simulators, not an
emulator: every mature emulator debugger (AppleWin, VICE, MAME, Mesen, Stella)
is built into the running emulator. GH #59 (the GUI panel) is merged into this
feature.

One engine, four command modes, three ways in:

- **Command modes**: AppleWin (default), the Apple II System Monitor,
  GSSquared, and a WinDbg-flavored mode.
- **Ways in**: a GUI debugger window with a command line, a batch mode in the
  command-line tool, and a debug channel that lets another program attach to a
  running Casso.

**Where this stands.** Stories 1 to 3 and the first version of the window are
implemented on the feature branch. A comparison against AppleWin and GSSquared
(2026-09-17) found the engine at parity or ahead: both syntaxes, batch
scripting with exit codes, an attach channel, symbol import from four formats,
watchpoints that stop before or after an access, and conditional breakpoints.
The gaps are the window itself, an instruction trace, device panels, and
source-level debugging. This feature does not release until those are closed:
there is no schedule pressure, and a first release that trails both
competitors in the parts a user sees first would waste the launch.

## Clarifications

### Session 2026-09-13

- Q: Which prefix reaches debugger commands with no Monitor equivalent from Monitor mode? → A: `/`; the rest of the line is read as an AppleWin-mode command (`/bpl`).
- Q: Which AppleWin commands does the first version accept? → A: The whole command table, phased. Phase 1 accepts every command that does not only change a display. Window, cursor, view and appearance commands arrive with the window. Commands that need absent hardware or features report that the command is not available (see Assumptions).
- Q: When does a running Casso listen on the debug channel? → A: Only while its debugger window is open, and only for the current user. Until the window ships, the command-line switch that will open the window opens the channel instead.
- Q: How does a client choose which running Casso to attach to? → A: The channel is identified by process ID; a list command enumerates live instances with PID, title label, machine and disks.
- Q: How many clients may attach to one instance at once? → A: Any number; commands run one at a time in arrival order, and every client receives every notification.
- Q: Which symbol and binary file formats must the debugger load? → A: Symbols: Casso's `-g` debug file, the symbol table in a Merlin assembly listing, AppleWin `.SYM`, and VICE label files. Binaries: raw bytes with an address, DOS 3.3 binary, Intel HEX, Motorola S-record, and AppleSingle. Casso's Merlin mode must first produce symbol output.

### Session 2026-09-18

- Q: Does the window ship as it stands, with the rest in a later feature? → A: No. Fonts, in-place memory editing, docking, the trace ring, device panels and source-level debugging are all part of this feature's release.
- Q: What does a memory edit write to? → A: RAM is written through. ROM is patched in the loaded image, and the patch is undoable. I/O addresses are not edited from a memory window; the explicit `OUT` command performs a bus write.
- Q: What is the scope of undo? → A: Per memory window. Writes made from the command line or by an attached client are not undoable.
- Q: Which debug file format do Casso's assemblers emit? → A: cc65's debug-info format, version 2 (`file`, `line`, `span`, `seg`, `sym`, `mod`, `scope` records), with an added `sha1` key on each `file` record. The current `NAME=$ADDR` file is a Casso construct that never shipped; the reader keeps accepting it.
- Q: How is a source file recognized as the one the program was built from? → A: By the SHA-1 of its text with line endings normalized to LF. Size is the filter before hashing; timestamps are a hint only, since copies, checkouts and editors change them.
- Q: How is a source file found when it is not beside the debug file? → A: By the path recorded relative to the debug file, then in a remembered list of folders where sources were found before (per program, then global), then by a file the user drags onto the debugger, matched by hash.
- Q: What does step over do on a `JSR`? → A: Runs until the stack pointer rises back above its value before the call, then stops at the next instruction. That handles inline parameters after the `JSR` (ProDOS MLI calls) and recursion, which a breakpoint on the next instruction does not.
- Q: What happens when an interrupt fires during a source-level step into? → A: The step lands in the interrupt handler. That is correct and is documented, not a defect.
- Q: How do macros appear in the source view? → A: Each expanded address range maps to both its invocation line and the macro body line, with nesting, so the view shows the invocation and can step into the body, the way C++ debuggers treat inlined functions.
- Q: Is the GSSquared syntax a third mode or aliases in AppleWin mode? → A: A third command mode, so its names (`bp`, `l`) do not collide with AppleWin's.
- Q: Does the channel change to a binary protocol? → A: No. JSON records over a named pipe stay; a binary frame may be added later for streaming the trace ring.
- Q: Does the trace ring stay on during normal use? → A: It is opt-in. When off, the emulation path is the same code as before the debugger existed.
- Q: Are expression breakpoints evaluated on every instruction? → A: No. An expression is attached to an address or an access and evaluated only when that location or access hits.

### Session 2026-09-18 (clarify)

- Q: Can a device diagnostic panel change device state, or is it read-only? -> A: Read-only. Panels show state; changes go through commands (`OUT`, `MEB`, `R`).
- Q: Must GSSquared mode print replies in GSSquared's own output format? -> A: The output format is a setting of its own, separate from the input mode: AppleWin, Monitor or GSSquared. Changing the input mode sets the output format to match it; the user can then change the output format alone. GSSquared's output format is verified against its documented examples and, where the documentation shows none, against a capture from GSSquared itself, checked in as a fixture. A Casso-native output format is not defined in this feature.
- Q: Is the saved layout one for all machines, or one per machine type? -> A: One layout. A pane for a device the current machine lacks is closed on restore and keeps its saved place, so it reopens there when that machine returns.
- Q: Which keyboard shortcuts drive stepping and running in the window? -> A: Three selectable schemes, saved in preferences and independent of the command mode: Visual Studio's by default (F5 run, F10 step over, F11 step into, Shift+F11 step out, F9 toggle breakpoint, Shift+F5 pause), AppleWin's (Space step, Ctrl+Space step over, Enter run, and its function keys), and GSSquared's (Space step, Return resume, O step over, R step out).
- Q: Which Merlin listings must import as a source view? -> A: Merlin 8/16 listings, verified against the corpus. Merlin 32 listings are a follow-up, tracked as a GitHub issue.

### Session 2026-09-18 (design review)

- Q: How does each command mode reach Casso's own engine commands (`MODE`, `PAUSE`, `BUDGET`, `SWITCHES`, `STACK`, `PATCH`, `SRC`)? -> A: Each mode uses the marker native to it. Monitor mode keeps `/` (Ctrl-Y is the machine's own user command, and `!` is its mini-assembler). AppleWin and GSSquared modes use bare names: neither has an extension marker, none of the engine names collide with their tables, and `/` cannot serve GSSquared because it is that debugger's bank separator. WinDbg mode uses `!`, WinDbg's own extension-command prefix. Every engine command is reachable in every mode.
- Q: Is a WinDbg/cdb dialect in scope after all? -> A: Yes, as a fourth command mode, described as WinDbg-flavored rather than as parity: the commands a 6502 session uses, with the process, thread, module, exception, kernel, dump, type and scripting families excluded with a defined reply. It follows GSSquared's mode and the call stack, since its `k` needs the call stack.
- Q: How is a call stack shown, given the 6502 has no frames? -> A: A call-stack pane, separate from the raw stack pane, with three selectable mechanisms: calls recorded as they execute, a walk of the stack page, and a hybrid default that uses recorded frames and extends below them with the walk. Every frame says which mechanism produced it, and each way a mechanism can be defeated is detected and shown as a break in the chain rather than hidden.
- Q: Does stepping into a call ever have to land inside the ROM? -> A: No. A step filter, reachable from every mode, lists routines a step into treats as a step over.
- Q: What did GSSquared's source settle about its mode? -> A: Its typed command table (FR-022a); that stepping in its window is by key (Space and F10 step, O over, R out, Return resumes), so typed `o` and `r` are Casso's additions for scripts; that an address may be bank-qualified with `/` and only bank `00` exists here; and that `m`, `x`, `map` and `video` are IIgs commands, refused with a message.

### Session 2026-09-21 (fit-and-finish review)

- Q: How does the code pane follow the PC without jumping on every update? -> A: It moves only when it has to. While the PC is on a line the pane already shows, the pane stays put and only the PC marker moves. When the PC leaves the shown lines, the pane re-anchors with the PC in its vertical middle, so the code that led there is on screen with it. The pane shows as many lines as fit its height.
- Q: How far can the code pane scroll, given the 6502 cannot be disassembled backward? -> A: Through the whole 64 KB. Above the first shown line it disassembles backward by assuming the preceding bytes are code and choosing the alignment that ends exactly on an instruction boundary at the line already shown. Over data this can be wrong; that is accepted, as it is in other disassemblers.
- Q: How is a breakpoint set in the code pane? -> A: By clicking the left gutter of its row. Double-clicking a row no longer toggles a breakpoint; the pane otherwise behaves as text, so a double-click selects the word under the pointer.
- Q: Does the call-stack pane still offer a choice of mechanism? -> A: No selector in the window. The pane always shows the best data available: recorded frames where the recorder has them, extended below by the stack walk. `CALLS MODE` keeps all three mechanisms as a command. Each frame says in plain words where it came from.
- Q: What does the memory pane's address box accept? -> A: It is a Go to box. It takes a hex address, a register, or a 6502 addressing expression resolved against the current registers and memory, and goes to the effective address, which is placed at the pane's top-left rather than at the start of its 16-byte row.
- Q: What are automatic watches drawn from? -> A: Every register, memory address and individual flag read or written by the instruction at the current PC or at the previous PC. They are listed above manual watches with a separator; an automatic watch's expression is fixed, its value editable.
- Q: What does Ctrl+Plus, Ctrl+Minus and Ctrl+0 affect? -> A: The text size of every debugger content pane, floating panes included, and nothing else: not other Casso windows, not the caption, not the command bar. Whether this is a font-size change or a zoom is a planning decision.
- Q: Where do the command bar's icons come from where the icon font has no matching glyph? -> A: They are drawn, to match Visual Studio's debugging toolbar icons. Run to Cursor is a right arrow ending at a vertical bar.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Break into a running program from a script (Priority: P1, delivered)

A developer, or an automated session diagnosing a bug, runs a disk headlessly,
stops at an address, inspects registers and memory, steps a few instructions,
and resumes -- entirely from a script, with no window.

**Why this priority**: This is the engine and the command language with the
smallest possible surface around them. It is useful the day it lands (bug
diagnosis such as the //c VBL spin needed exactly this), it is fully testable
without a window, and every later story is another way into the same engine.

**Independent Test**: Run the command-line tool in debug mode against a
fixture machine with a script that sets a breakpoint, runs, prints registers,
dumps memory, steps, and exits. Compare output to expected text and to the
structured form.

**Acceptance Scenarios**:

1. **Given** a script that enters a loop at $0300 reading $C019, sets a
   memory-read breakpoint on $C019 (`bpmr C019`), runs, and prints registers,
   **When** the tool runs it, **Then** execution stops after the instruction
   that read $C019, the stop reports that instruction's address, and the
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

### User Story 2 - Use Apple II Monitor syntax (Priority: P1, delivered)

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

### User Story 3 - Attach to a running Casso (Priority: P2, delivered)

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
   `bpmr C000`, **Then** it receives a reply with the same content batch mode's
   structured output contains for that command.
2. **Given** a connected client and a set breakpoint, **When** the machine
   reaches it, **Then** the client receives a stop notification without polling.
3. **Given** two Casso instances running at once with their debugger windows
   open, **When** a client lists instances and connects to one process ID,
   **Then** it reaches exactly that instance and the other is unaffected.
4. **Given** a Casso run by a different user account, **When** a client
   connects, **Then** the connection is not accepted.
5. **Given** a Casso whose debugger window is closed, **When** a client tries
   to connect, **Then** no channel exists and the instance is not listed.
6. **Given** two connected clients, **When** one sets a breakpoint and the
   machine reaches it, **Then** both receive the stop notification.

---

### User Story 4 - Debug in a window beside the emulator (Priority: P1)

A user opens a debugger window beside the running emulator and debugs with the
mouse and a command line: disassembly with the current line marked, each
line's own label and symbolic operands, registers, stack, watches, breakpoints
set by double-clicking in the disassembly, and buttons for step, step over,
step out, run and run to cursor. The panes are dense: a monospace font, rows
no taller than the text needs, columns sized to their contents with minor
padding, and every pane tall enough to show its list.

**Why this priority**: The window is what a user sees first, and the first
version's oversized rows, blank symbol column and one-row breakpoint pane read
as unfinished beside AppleWin. Everything else in this feature is shown
through these panes.

**Independent Test**: With the Mockingboard speech demo running and its debug
file loaded, open the window, stop at `Sing`, and confirm the disassembly
shows `Sing` on its line and `STA PTR` rather than `STA $06`; confirm the
breakpoint, watch and stack panes show every entry; measure the row height
against the font's line height.

**Acceptance Scenarios**:

1. **Given** a running machine, **When** the user opens the debugger window and
   pauses, **Then** disassembly around the program counter, registers, flags,
   stack, breakpoints, watches and memory are shown, and the current line is
   marked.
2. **Given** symbols are loaded, **When** the disassembly shows an instruction
   at a symbol's address, **Then** the symbol is shown on that line, and an
   operand equal to a symbol's address is shown as the symbol.
3. **Given** the window is open, **When** the user double-clicks a disassembly
   line, **Then** a breakpoint is set there and shown in the breakpoint list;
   a second double-click clears it.
4. **Given** three breakpoints, two watches and a stack, **When** the window
   is at its default size, **Then** all of them are visible without scrolling.
5. **Given** the window's command line, **When** the user enters any command
   valid in the selected mode, **Then** it behaves exactly as it does in batch
   mode, and the reply appears in the console pane.
6. **Given** a Monitor-mode `R` or `W` with no file name, **When** the user
   enters it in the window, **Then** a file picker opens and the command runs
   with the chosen file.

---

### User Story 5 - Edit memory in place (Priority: P1)

A user opens up to four memory windows, each at its own address, chooses bytes,
words or double words, clicks a cell and types a new value. Each value is
written the moment it is complete and focus moves to the next cell, so a run
of bytes can be typed straight through. The text column beside the hex is
editable the same way. A mistake is undone with the usual key.

**Why this priority**: Poking memory through a separate box is the first
version's most visible shortcut. Direct editing is how every desktop debugger
works, and the speech demo's parameter tables are the kind of data a user
wants to change while watching the result.

**Independent Test**: Open a memory window at $0300, type `A9 41 60` into
three consecutive byte cells, confirm the machine's memory holds those bytes
after each keystroke pair, undo twice, and confirm the first byte remains and
the other two are restored.

**Acceptance Scenarios**:

1. **Given** a memory window showing bytes, **When** the user clicks a cell and
   types two hex digits, **Then** the byte is written to the machine and focus
   moves to the next cell.
2. **Given** a memory window showing words, **When** the user types four hex
   digits, **Then** the word is written low byte first and focus moves to the
   next word.
3. **Given** a memory window's text column, **When** the user types a
   character over a cell, **Then** the corresponding byte becomes that
   character's code and focus moves to the next character.
4. **Given** edits were made in a window, **When** the user presses undo,
   **Then** the most recent edit's previous value is written back, one edit at
   a time, in reverse order.
5. **Given** a cell in ROM, **When** the user edits it, **Then** the value shows
   in the window and in disassembly and the machine reads the patched value;
   undo restores the original.
6. **Given** a cell in the I/O range, **When** the user tries to edit it,
   **Then** the cell is not editable and the window says how to write an I/O
   address.
7. **Given** four memory windows, **When** each is set to a different address
   and grouping, **Then** each shows its own view and edits in one appear in
   any other showing the same address.

---

### User Story 6 - Debug at source level (Priority: P1)

A user assembles a program with Casso's assembler, loads the debug file, and
debugs against the source: a source window follows the program counter as the
machine stops and steps, breakpoints are set on source lines, and step into,
step over and step out move by source line. A program built with another
assembler that writes cc65's debug format debugs the same way. A program
whose only artifact is a listing from real Merlin debugs against the listing.

**Why this priority**: Neither AppleWin nor GSSquared has it, and Casso is the
only Apple II emulator that ships its own assembler. Live memory beside the
user's own source is the feature the others cannot copy without first writing
an assembler.

**Independent Test**: Assemble the Mockingboard speech demo with `-g`, load its
debug file, set a breakpoint on the source line of `Sing`, run, and confirm
the source window shows that line marked; step over a `JSR` with inline
parameters after it and confirm the step lands on the next source line;
change one byte of the source, drag it onto the debugger, and confirm the
mismatch warning.

**Acceptance Scenarios**:

1. **Given** a debug file whose source files are beside it, **When** the user
   loads it and the machine stops, **Then** the source window shows the file
   and line for the program counter with the line marked, and the disassembly
   pane shows the same instruction.
2. **Given** the source window, **When** the user steps into, over or out,
   **Then** the machine stops at the first instruction of a different source
   line (into), the next source line in the same routine after any calls
   return (over), or the line after the call that entered the routine (out).
3. **Given** a `JSR` followed by inline parameter bytes, **When** the user
   steps over it, **Then** the machine stops on the source line after the
   parameters, not inside them.
4. **Given** a recursive routine, **When** the user steps over its `JSR` to
   itself, **Then** the machine stops when that call returns, not when a deeper
   call reaches the same address.
5. **Given** a macro invocation, **When** the machine stops inside its
   expansion, **Then** the source window shows the invocation line, marks that
   the position is inside a macro, and can show the body line on request.
6. **Given** a source file is not beside the debug file, **When** the debug
   file is loaded, **Then** the folders where sources were found before are
   searched, and a file with the recorded name, size and hash is used without
   asking.
7. **Given** no matching file is found, **When** the user drags a source file
   onto the debugger, **Then** it is matched by hash to the debug file's entry
   and its folder is remembered for later.
8. **Given** a file whose text differs from the one the program was built
   from, **When** it is opened, **Then** the source window opens it with a
   visible warning that lines may not match.
9. **Given** a debug file written by another assembler in cc65's format,
   **When** it is loaded, **Then** its files, lines and symbols work exactly as
   Casso's own.
10. **Given** a listing from real Merlin and no source files, **When** it is
    loaded, **Then** the listing itself is the source view, with its addresses
    mapped to its lines.

---

### User Story 7 - Arrange the debugger like Visual Studio (Priority: P2)

A user drags the panes into the layout they want: side by side, stacked,
tabbed together, or floated onto a second monitor. A pane can be pinned to
auto-hide at an edge. The layout is saved and restored, and the whole thing
works from the keyboard for a user who does not drag.

**Why this priority**: Every pane in this feature is useful on its own, but
real work needs several visible at once in an order the user chooses. A fixed
layout makes the source window, four memory windows, the trace and the device
panels compete for one grid.

**Independent Test**: Drag the memory pane to the right edge, tab the trace
under it, float the source pane onto a second monitor, close and reopen the
debugger, and confirm the layout is restored; unplug the second monitor and
confirm the floated pane comes back onto the primary one.

**Acceptance Scenarios**:

1. **Given** the debugger window, **When** the user drags a pane's title,
   **Then** drop zones appear for each side of each group and for tabbing, and
   dropping places the pane there.
2. **Given** two panes side by side, **When** the user drags the splitter
   between them, **Then** both resize, and neither shrinks below its minimum.
3. **Given** a docked pane, **When** the user drags it outside the window or
   chooses Float, **Then** it becomes its own top-level window, and it can be
   dragged back into a drop zone.
4. **Given** a pane, **When** the user pins it to auto-hide, **Then** it
   collapses to a tab on the window edge and slides out on hover or click.
5. **Given** a pane, **When** the user opens its Dock To menu, **Then** it can
   be docked to any side, tabbed with any listed pane, or floated without the
   mouse; arrow keys move it within its group.
6. **Given** a floated pane on a monitor with a different scale factor,
   **When** it is dragged across the boundary, **Then** it renders at each
   monitor's scale with no blurring.
7. **Given** a saved layout, **When** the debugger is reopened, **Then** the
   layout is restored; a pane whose monitor is absent opens on the primary
   monitor.

---

### User Story 8 - Look back at what ran (Priority: P2)

A user turns on tracing, lets a program run to a crash or a breakpoint, and
scrolls back through the instructions that led there: for each, the cycle
count, program counter, opcode bytes, registers, the address read or written,
the direction, the data byte, and the symbol at that address.

**Why this priority**: Retrospective debugging answers "how did it get here",
which breakpoints cannot. GSSquared retains 100,000 instructions; AppleWin
only writes a trace to a file. Every field is already computed by the
cycle-accurate core.

**Independent Test**: Turn tracing on, run the speech demo to a watchpoint on
the speech chip's data register, and confirm the trace pane shows the write
that stopped it as its last entry, with the register values and the symbol
`SPHON`; scroll back and confirm entries are in execution order with
increasing cycle counts.

**Acceptance Scenarios**:

1. **Given** tracing is off, **When** the machine runs, **Then** emulation
   speed is the same as before the debugger existed.
2. **Given** tracing is on, **When** the machine runs 100,000 instructions or
   more, **Then** the most recent 100,000 are retained and older ones are
   dropped.
3. **Given** a retained trace, **When** the machine stops, **Then** the trace
   pane shows the entries ending at the stop, and the user can scroll to any
   earlier entry.
4. **Given** a trace entry that read or wrote memory, **When** it is shown,
   **Then** it carries the address, the direction and the data byte, and the
   symbol for that address when one is loaded.
5. **Given** a retained trace, **When** the user saves it, **Then** a file with
   every retained entry is written.

---

### User Story 9 - Inspect a device (Priority: P2)

A user opens a panel for a device and watches its state: the Disk II's phase
magnets, quarter track, motor and read state; the MMU's soft switches and
which bank each page reads and writes from; the video mode; the keyboard
latch and modifiers; the Mockingboard's timers and sound registers; the
printer's head; the cycle counters. The panels update while the machine runs
and freeze when it stops.

**Why this priority**: GSSquared ships nine such panels and AppleWin shows the
soft switches; Casso has none. The state already exists in the device models,
several of which are richer than either competitor's.

**Independent Test**: Boot a disk with the Disk II panel open and watch the
quarter track and phases change during the boot; stop, and confirm the panel
freezes on the stopped state; switch to a machine without a Mockingboard and
confirm that panel is not offered.

**Acceptance Scenarios**:

1. **Given** the debugger window, **When** the user opens the panel list,
   **Then** every device in the current machine that publishes diagnostics is
   listed, and only those.
2. **Given** an open device panel and a running machine, **When** the device's
   state changes, **Then** the panel shows the new state within one frame.
3. **Given** a register shown with a bit decode, **When** its value changes,
   **Then** each bit's label and state are shown.
4. **Given** the MMU panel, **When** the user views the memory map, **Then** a
   bar shows every page colored by what it reads from and writes to.
5. **Given** the Disk II panel, **When** the drive seeks, **Then** the head
   graphic moves across the tracks at quarter-track resolution.
6. **Given** a machine is switched, **When** the new machine has different
   devices, **Then** the panel list changes to match and panels for absent
   devices close.

---

### User Story 10 - Stop on a condition (Priority: P3)

A user sets a breakpoint that stops only when an expression is true at that
address, such as `BP Loop IF X == 3 & *PTR != 0`, and a value breakpoint
that stops when a memory location becomes a given value. `BPR A=0` works with
or without spaces.

**Why this priority**: AppleWin's register conditions set the bar and Casso
already matches them. Expressions over memory and symbols put Casso ahead of
both competitors, and the expression evaluator already exists.

**Independent Test**: In batch, set `BP 0300 IF A == 41` on a loop that
counts A up, run, and confirm the stop reports A as $41; set a value
breakpoint on $0400 becoming $C1, run a program that stores $C1 there on its
tenth pass, and confirm the stop's hit count.

**Acceptance Scenarios**:

1. **Given** a breakpoint with an expression, **When** the machine reaches the
   address and the expression is false, **Then** the machine does not stop and
   the breakpoint's hit count does not change.
2. **Given** the same breakpoint, **When** the expression is true, **Then** the
   machine stops and the stop reports the expression and its value.
3. **Given** an expression that cannot be evaluated (an unknown symbol), **When**
   the breakpoint is set, **Then** the command reports the error and no
   breakpoint is created.
4. **Given** a value breakpoint on an address, **When** a write leaves the
   address holding the given value, **Then** the machine stops; a write of any
   other value does not stop it.
5. **Given** `BPR A=0`, **When** entered without spaces, **Then** it sets the
   same breakpoint as `BPR A 0`.

---

### User Story 11 - Find the expensive code (Priority: P3)

A user profiles a run and reads which opcodes and addressing modes consumed
the cycles, which addresses ran hottest, and how many cycles were avoidable
penalties: indexed reads that crossed a page, taken branches, and branches
that crossed a page.

**Why this priority**: AppleWin's `PROFILE LIST` reports cycles by opcode;
GSSquared has nothing. Attributing avoidable penalties is a step past both,
and it matches how Casso's own performance work is done.

**Independent Test**: Profile a loop of `LDA $10FF,X` with X crossing the
page half the time, and confirm the histogram attributes the page-crossing
cycles separately from the instruction's base cycles.

**Acceptance Scenarios**:

1. **Given** profiling is on, **When** the machine runs, **Then** `PROFILE
   LIST` shows, per opcode and addressing mode, the count, the cycles and the
   share of the total.
2. **Given** profiling is on, **When** the user asks for the per-address view,
   **Then** the addresses that consumed the most cycles are listed with their
   symbols.
3. **Given** a run with page crossings and taken branches, **When** the user
   lists the profile, **Then** the penalty cycles are shown apart from base
   cycles, by kind.
4. **Given** a profile, **When** the user saves it, **Then** a file with the
   same content is written.

---

### User Story 12 - Use GSSquared's syntax (Priority: P3)

A user coming from GSSquared switches the debugger to its command mode and
uses its words: `bp C000.C0FF`, `bpd C010 rw`, `bpi C010 rw`, `nobp 3`,
`watch 40.4F`, `l C000`, `sload "labels.lbl"`, `load`, `save`, `set`, `move`,
and `o` and `r` to step over and out. In GSSquared itself stepping is done
with keys in its window (Space and F10 step, O steps over, R steps out,
Return resumes), which the GSSquared key scheme matches; `o` and `r` as typed
commands are Casso's additions so a script can step in this mode.

**Why this priority**: It is a third parser over the same engine, small and
independent, and it removes the last reason a GSSquared user would need to
learn new words.

**Independent Test**: In batch with the GSSquared mode selected, run each
command in its command list against a fixture machine and compare the effect
with the same command in AppleWin mode.

**Acceptance Scenarios**:

1. **Given** GSSquared mode, **When** the user enters `bpd C010 rw`, **Then** a
   read-and-write watchpoint is set on $C010 and is listed by `bp`, in
   GSSquared's listing format.
2. **Given** GSSquared mode, **When** the user enters `2000.201F`, **Then**
   memory prints in GSSquared's dump format, 16 bytes per line with ASCII.
3. **Given** GSSquared mode, **When** the user sets the output format to
   AppleWin and enters `2000.201F`, **Then** memory prints in AppleWin's
   format; **and when** the user switches the input mode to Monitor, **Then**
   the output format becomes Monitor's.
4. **Given** GSSquared mode, **When** the user enters `2000:AA 55`, **Then**
   $2000 and $2001 hold $AA and $55.
5. **Given** a breakpoint set in GSSquared mode, **When** the user switches to
   AppleWin mode and enters `BPL`, **Then** the same breakpoint is listed.
6. **Given** GSSquared mode, **When** the user enters `00/300`, **Then** it
   means $0300; **and when** the user enters `E1/300`, **Then** the reply says
   only bank 00 exists on this machine and nothing changes.
7. **Given** GSSquared mode, **When** the user enters `map` or `video`,
   **Then** the reply says the command needs a IIgs and nothing changes.

---

### User Story 13 - See how the program got here (Priority: P2)

A user stopped deep inside a program opens the call-stack pane and sees the
chain of calls that led to the current instruction: each caller's address and
symbol, innermost first. They double-click a frame and the disassembly moves
to the call site. When the program has done something to the stack that no
mechanism can see through, the pane says where the chain breaks and why,
instead of showing frames it has lost track of.

**Why this priority**: AppleWin shows only the raw stack bytes. Mesen tracks
calls as they run; VICE has a backtrace. Casso can do both and, unlike either,
say when the answer cannot be trusted. It shares the run hook with the trace
ring, so it follows User Story 8.

**Independent Test**: On a fixture program with a recursive routine, a routine
that reads inline parameters after its `JSR`, and an interrupt handler, stop
at a known depth and compare the pane's frames with the true chain; then run
each stack manipulation in FR-069 and confirm the pane reports the break at
the instruction that caused it.

**Acceptance Scenarios**:

1. **Given** the machine stopped three calls deep with recording on, **When**
   the user opens the pane, **Then** three frames are shown innermost first,
   each labeled recorded, with the caller's address and symbol.
2. **Given** the debugger attached after the program had already made two
   calls, **When** the user opens the pane, **Then** the frames recorded since
   attaching are labeled recorded, the two below them are labeled guessed from
   the stack walk, and the boundary between them is marked.
3. **Given** a routine that pulls its return address, reads two inline
   parameter bytes and returns past them, **When** it returns, **Then** the
   pane reports that the frame returned past inline parameters and does not
   mark the chain broken.
4. **Given** a program that executes `TXS`, **When** the user opens the pane,
   **Then** a separator row names `TXS` and its address, and every frame below
   it is shown as unverified.
5. **Given** the machine stopped inside an interrupt handler, **When** the
   user opens the pane, **Then** the interrupt is a frame of its own, labeled
   as an interrupt, above the frame it interrupted.
6. **Given** the pane open, **When** the user double-clicks a frame, **Then**
   the disassembly moves to the `JSR` that made the call.
7. **Given** the mechanism set to stack walk only, **When** the user opens the
   pane, **Then** every frame is labeled guessed and no recorded frame is used.

---

### User Story 14 - Use WinDbg's syntax (Priority: P3)

A user who lives in WinDbg switches the debugger to its command mode and uses
the words they know: `bp 300`, `ba w1 c010`, `bl`, `db 2000 l20`,
`eb 300 a9 41`, `t`, `p`, `gu`, `k`, `r`, `u`, `x start*`, `? 300+10`,
`.formats 41`, and `!switches` for Casso's own commands.

**Why this priority**: It is a fourth parser over the same engine, like
GSSquared's, and no other Apple II debugger offers it. It follows User
Story 12 and User Story 13, since `k` needs the call stack.

**Independent Test**: In batch with WinDbg mode selected, run each command in
FR-022c against a fixture machine and compare the effect with the same
command in AppleWin mode.

**Acceptance Scenarios**:

1. **Given** WinDbg mode, **When** the user enters `ba w1 c010`, **Then** a
   write watchpoint is set on $C010 and `bl` lists it in WinDbg's listing
   layout.
2. **Given** WinDbg mode, **When** the user enters `db 2000 l20`, **Then** 32
   bytes print in WinDbg's dump layout, with the address, hex bytes and ASCII.
3. **Given** WinDbg mode and a stopped machine, **When** the user enters `k`,
   **Then** the call stack prints, one frame per line, with each frame's
   provenance.
4. **Given** WinDbg mode, **When** the user enters `bp main.a65:12`, with or
   without WinDbg's backquotes, **Then** a breakpoint is set on that source
   line.
5. **Given** WinDbg mode, **When** the user enters `~` or `lm`, **Then** the
   reply says the command has no meaning on this machine and nothing changes.
6. **Given** WinDbg mode, **When** the user enters `!switches`, **Then** the
   soft switches are listed, the same as `SWITCHES` in AppleWin mode.
7. **Given** WinDbg mode, **When** the user enters `0x300`, `300` or `$300` as
   an address, **Then** all three mean $0300.

---

### User Story 15 - Skip routines when stepping (Priority: P3)

A user stepping through their program adds `COUT` and `RDKEY` to the step
filter. From then on a step into `JSR COUT` behaves as a step over, so they
never land in the ROM's output routine, while a step into their own routines
still enters them.

**Why this priority**: Small, reachable from every mode and every key scheme,
and it removes the most common reason to reach for step over by hand on the
Apple II.

**Independent Test**: With `COUT` in the filter, step into a `JSR COUT` and
confirm the stop is at the instruction after the `JSR`; clear the filter and
confirm the same step stops at `COUT`'s first instruction.

**Acceptance Scenarios**:

1. **Given** `COUT` in the filter, **When** the user steps into `JSR COUT`,
   **Then** the stop is at the instruction after the `JSR`.
2. **Given** an empty filter, **When** the user steps into the same `JSR`,
   **Then** the stop is at $FDED.
3. **Given** a filter entry given as an address range, **When** the user steps
   into any routine in the range, **Then** it is stepped over.
4. **Given** source-level stepping on, **When** a source line calls a filtered
   routine, **Then** the step lands on the next source line, not inside the
   routine.
5. **Given** the filter set in one mode, **When** the user switches mode,
   **Then** the filter still applies.

---

### User Story 16 - Read the program in the code pane (Priority: P1)

A user stepping through their program watches the code pane. It stays still
while the PC moves down the lines it shows, and when the PC leaves them it
comes back with the PC in the middle. Beside each instruction the pane shows
what the instruction will touch: the byte at its effective address, the value
behind a symbol, the flags a branch tests. The user scrolls up through the
code that led here, clicks the gutter to set a breakpoint, and double-clicks a
word to select it.

**Why this priority**: The code pane is where a debugging session is spent.
A pane that jumps on every update, hides what led to the PC, or makes the user
compute effective addresses by hand fails the window's first purpose.

**Independent Test**: Pause on a loop, step through it, and confirm the pane's
first line does not change while the PC stays on shown lines; step out of the
shown lines and confirm the PC returns mid-pane; scroll to $0000 and to $FFFF;
confirm `STA $067B` shows the byte at $067B and `BEQ` shows Z.

**Acceptance Scenarios**:

1. **Given** the PC on a shown line, **When** the user steps to another shown
   line, **Then** the pane does not scroll and only the PC marker moves.
2. **Given** the PC on a shown line, **When** a step takes it off the shown
   lines, **Then** the pane re-anchors with the PC in its vertical middle.
3. **Given** a pane of any height, **When** it is displayed, **Then** it is
   filled with lines.
4. **Given** the pane at any address, **When** the user scrolls up or down,
   **Then** it scrolls through the whole address space.
5. **Given** a row, **When** the user clicks its left gutter, **Then** a
   breakpoint is set there, shown as a filled red dot; clicking again clears it.
6. **Given** a row, **When** the user double-clicks a word in it, **Then** the
   word is selected and no breakpoint changes.
7. **Given** `STA $067B`, `LDA KBD` and `BEQ $C918`, **When** they are shown,
   **Then** they carry the byte at $067B; KBD's address and the byte there; and
   the value of Z.
8. **Given** the current instruction is a branch or jump whose destination is
   on screen, **When** the pane is shown, **Then** the destination row is
   highlighted.
9. **Given** another pane moves the code pane to an address, **When** the
   pane scrolls, **Then** the target row is marked.

---

### User Story 17 - Work the data panes directly (Priority: P2)

A user inspects and changes state from the panes themselves. They type an
addressing expression into the memory pane's Go to box and land on the
effective address at the top-left. Bytes that change between updates light
up. In the watch pane, automatic watches show what the current instruction
touches; the user adds their own watches, edits an expression or a value in
place, and undoes a change. In the breakpoints pane they disable a breakpoint
by its circle, jump to one by double-clicking it, and edit its type from a
context menu.

**Why this priority**: These panes exist to be worked, not only read; each
interaction here replaces a typed command with the gesture the user already
knows from Visual Studio.

**Independent Test**: Enter `(3E),Y` in Go to and confirm the pane's first cell
is the effective address; change a byte from the console and confirm it is
highlighted in the pane; double-click a watch value, type a new byte, and
confirm memory holds it and undo restores it; click a breakpoint's circle and
confirm it disables without being removed.

**Acceptance Scenarios**:

1. **Given** the Go to box, **When** the user enters a hex address, a register
   or an addressing expression, **Then** the pane goes to the effective address
   and shows it at its top-left.
2. **Given** a memory pane, **When** a shown byte changes between updates,
   **Then** it is highlighted, and unchanged bytes are not.
3. **Given** a focused byte, **When** the pane is shown, **Then** it has a
   distinctive background and a bright foreground.
4. **Given** an instruction at the PC, **When** the watch pane updates, **Then**
   its automatic watches list what that instruction and the previous one read
   or write, above a separator and the manual watches.
5. **Given** a manual watch, **When** the user double-clicks its expression or
   its value and types, **Then** the expression is replaced or the value is
   written; undo in the watch pane reverses it without touching the memory
   pane's history.
6. **Given** a breakpoint row, **When** the user clicks its circle, **Then** it
   toggles between enabled (filled) and disabled (outline).
7. **Given** a breakpoint row, **When** the user double-clicks it, **Then** the
   code pane goes to its address and the breakpoint remains.
8. **Given** a breakpoint row, **When** the user opens its context menu,
   **Then** it can be removed, or edited including its type and the fields that
   type needs.

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
  Memory-window undo histories are cleared by a machine switch, since their
  addresses belonged to the old machine.
- **Client disconnects while the machine is paused**: the machine stays paused
  until resumed from another way in.
- **Debugger window closed while clients are attached**: the channel closes and
  every client is disconnected; breakpoints and the paused state are unchanged.
- **Lowercase input in Monitor mode**: accepted for commands and hex.
- **Interrupt during a source-level step into**: the step stops in the
  interrupt handler, which has a source line of its own or none. This is
  correct: the machine did execute the handler.
- **Step over a call that never returns**: the run continues until a
  breakpoint, a pause, or the budget in batch; the window's Pause ends it.
- **Step over a routine that discards its return address**: the stack pointer
  rises above its pre-call level when the address is popped, so the step stops
  at the instruction after the pop, wherever it is.
- **Source line with no code** (a comment, a directive, an equate): a
  breakpoint set there moves to the next line that has code, and the window
  says so.
- **One address in several source lines** (a macro body used by several
  invocations): the source view shows the invocation on the current stack,
  and the body line on request.
- **Included file used by several modules**: each inclusion has its own line
  entries; the view opens the file once.
- **Debug file from a linker with several segments**: line entries are
  segment-relative and resolve through the segment table; a segment placed
  at a different address by a later link resolves correctly.
- **Source file found by name and size but with a different hash**: it is
  opened with the mismatch warning, since a same-size edit is common.
- **Two candidate files with the same name and size**: the first whose hash
  matches wins; if none matches, the user is asked.
- **A file dragged onto the debugger that matches no entry**: it opens as a
  plain text view with no line mapping, and the window says so.
- **Editing memory while the machine runs**: the write lands between
  instructions, as a poke from the command line does.
- **ROM patch on a machine that switches ROM banks** (the //c): the patch is
  applied to the loaded image the bank comes from, so it survives a bank
  flip.
- **Undo after the machine has changed the same byte**: undo restores the
  edit's previous value regardless, since that is what undo means; the window
  shows the value it wrote.
- **Trace ring while stepping**: each step adds its instructions; the ring
  does not clear on a stop.
- **Trace ring across a machine switch**: cleared, since its addresses and
  symbols belonged to the old machine.
- **Expression breakpoint whose expression reads an I/O address**: setting it
  reports an error, since evaluating the expression would change the machine.
- **Expression breakpoint on a watchpoint access**: the expression may refer
  to the accessed address and the value read or written.
- **Device panel for a device that is removed at run time** (a card slot
  emptied in settings): the panel closes.
- **Saved layout from an older version**: unknown pane ids are dropped and the
  rest is restored; an unreadable layout falls back to the default.
- **Floating pane's monitor absent at restore**: the pane opens on the
  primary monitor at its saved size.
- **Drop zone under a pane's own group**: dropping a pane onto itself is a
  no-op.
- **Auto-hidden pane while its content changes**: the tab shows an indicator;
  the pane does not slide out on its own.
- **GSSquared mode and Monitor deposit syntax**: `2000:AA 55` and `2000.201F`
  mean the same in both modes.
- **`BPR` register condition with no value**: still an error, in either
  spacing.
- **Call stack when a routine pulls its return address and jumps away**: the
  frame ends at the jump, and the pane shows it as ended by a jump rather than
  a return.
- **Call stack after a reset**: every recorded frame is discarded and the pane
  says so; the stack walk starts again from the new stack pointer.
- **Return address that differs from the one pushed**: a difference of a few
  bytes past the pushed address is the inline-parameter case and is reported
  as such; any other difference is a break at the `RTS`.
- **Stack wrap**: a push at $0100 wrapping to $01FF is a break; recorded frames
  above it are kept and marked unverified.
- **WinDbg command with no 6502 meaning**: a defined reply saying so, never
  "unknown command".
- **Filtered routine that never returns**: the step continues until a
  breakpoint, a pause, or the budget, as a step over of any such call does.

- **Scrolling backward into data**: the backward disassembly assumes code and
  may misalign over data; the lines already shown stay as they were, and the
  misalignment is confined to what scrolled into view.
- **Go to expression that reads an I/O address**: resolving `(abs)` or
  `(zp),Y` reads memory to find the effective address; a read that would
  touch an I/O address is not made, and the box reports that the expression
  cannot be resolved without changing the machine.
- **Watch or annotation on an I/O address**: the value is shown as unavailable
  rather than read, as the memory pane does; writing a watch value to an I/O
  address is refused, as FR-037 refuses it from a memory window.
- **PC in data**: the code pane anchors on the PC itself when no alignment
  reaches it from above, as it did before this review.
- **Change highlight after a pane moves**: bytes newly scrolled into view are
  not highlighted; only a byte that changed while shown is.

## Requirements *(mandatory)*

### Functional Requirements

**Engine**

- **FR-001**: The debugger MUST break into the machine Casso is running,
  without restarting it or loading a program under a separate debugger.
- **FR-002**: Users MUST be able to pause, resume, step one instruction
  (entering subroutines), step over a subroutine call, step out of a
  subroutine, and run to a chosen address. Step over MUST run until the stack
  pointer rises back above its value before the call and then stop at the
  next instruction, so a call followed by inline parameters, and a recursive
  call, both step over correctly.
- **FR-003**: Users MUST be able to set, list, enable, disable and clear
  breakpoints by address, by opcode, and by condition on registers or memory.
- **FR-004**: Users MUST be able to set watchpoints that stop execution on a
  read of, or a write to, an address or range. A watchpoint stops in one of
  two modes, chosen per watchpoint:
  - **After the access (the default)**: the machine stops at the next
    instruction boundary, and the stop reports the address, the value read or
    written, the value a write replaced where that value is known, and the
    address of the instruction that made the access. This mode sees every
    access, including stack pushes and pulls, interrupt vector reads and DMA,
    and tells a read-modify-write's read from its write.
  - **Before the access**: the machine stops before executing an instruction
    whose operand addresses fall in the range, leaving the memory unchanged so
    the user can change registers or memory, or move the program counter, so
    the access never happens. This mode sees only what an instruction's own
    operand addresses predict: it does not see stack, interrupt or DMA
    accesses, and it stops once for a read-modify-write without telling the
    read from the write.
  An instruction that stops a before-mode watchpoint does not stop an
  after-mode watchpoint on the same range when the run resumes: one
  instruction produces one stop.
- **FR-004a**: A write reported after the access MUST carry the value it
  replaced, except where reading it would disturb the machine: the value is
  absent for any address not backed by memory the debugger can read without
  side effects, such as the soft switches at $C000-$C0FF. When one
  instruction reads and then writes the same address, the stop reports the
  write.
- **FR-005**: Users MUST be able to view and change the registers and flags.
- **FR-006**: Users MUST be able to view and change memory as the CPU currently
  sees it, including which of ROM, RAM or language-card memory is mapped at an
  address.
- **FR-007**: Users MUST be able to view soft-switch state and the stack,
  through the Casso engine commands `SWITCHES` and `STACK` (`/SWITCHES` and
  `/STACK` in Monitor mode).
- **FR-008**: Every run started by a batch script or by `CassoCli debug
  --attach` MUST have a cycle budget; reaching it MUST end the run and report
  the reason. A run started from the window or by another channel client has
  no budget unless the client sets one, per run or for the session, because a
  person may be using the machine while a tool is attached.
- **FR-009**: Given the same machine, disk and commands, a batch run MUST
  produce identical results every time.
- **FR-010**: The debugger MUST render any single instruction at any address as
  disassembly, for both 6502 and 65C02, including the undocumented opcodes Casso
  already implements.
- **FR-010a**: Disassembly MUST show, for each line, the symbol at that
  address when one is loaded, and MUST show an operand as a symbol when the
  operand's address has one, in every way in.

- **FR-070**: The debugger MUST keep a step filter: a list of routines, by
  symbol, address or address range, that a step into treats as a step over.
  It MUST apply in every mode, to every key scheme and to source-level
  stepping, and MUST be settable and listable from every mode.

**Command modes**

- **FR-011**: The debugger MUST provide four command modes, AppleWin, Apple
  II Monitor, GSSquared and WinDbg, selected by the user; AppleWin MUST be the
  default.
- **FR-012**: All modes MUST operate on the same session state: a breakpoint,
  watch or register change made in one mode MUST be visible in the others.
- **FR-013**: The debugger MUST have an output format, AppleWin, Monitor,
  GSSquared or WinDbg, separate from the input mode. Changing the input mode MUST set
  the output format to that mode's own; the user MUST then be able to change
  the output format alone, from any way in, and every reply MUST be written
  in the current output format.
- **FR-014**: Every engine command Casso adds beyond the dialects (`MODE`,
  `PAUSE`, `BUDGET`, `SWITCHES`, `STACK`, `PATCH`, `SRC`, and any added later)
  MUST be reachable in every mode through that mode's own marker: `/` in
  Monitor mode (which also reaches any AppleWin command with no Monitor
  equivalent), bare names in AppleWin and GSSquared modes, and `!` in WinDbg
  mode. The documentation MUST describe each engine command once and list the
  marker per mode.

**AppleWin mode**

- **FR-015**: AppleWin mode MUST accept every command name and alias in
  AppleWin's debugger command table, in the phases listed in Assumptions. A
  command whose phase has not shipped, or that needs hardware or a feature
  Casso lacks, MUST report that it is not available and change nothing; it
  MUST NOT be reported as an unknown command.
- **FR-015a**: `BPR` MUST accept its register, comparison and value with or
  without spaces between them (`BPR A=0`, `BPR A = 0`, `BPR A 0`).

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
- **FR-020**: `R` and `W` MUST read and write the host file given by an optional
  trailing filename, as raw bytes; `R` MUST read the smaller of file and range
  and report a length mismatch.
- **FR-021**: Monitor mode MUST accept lowercase and uppercase input.

**GSSquared mode**

- **FR-022a**: GSSquared mode MUST accept GSSquared's debugger commands with
  their documented arguments: `bp` (address or range, or list), `bpd` (data
  breakpoint, `r`, `w` or `rw`), `bpi` (I/O breakpoint, same arguments),
  `nobp` (by id or address), `watch` and `nowatch`, `l` (disassemble, with
  and without an address), `sload`, `slookup`, `sclear`, `load`, `save`,
  `set`, `move`, memory read (`C000`), deposit (`2000:AA 55`), dump
  (`2000.201F`), and, as Casso's additions for scripts (GSSquared itself steps only by key), `o` (step over) and `r` (step out). Each
  MUST map onto the same engine operation the equivalent AppleWin command
  performs. GSSquared's output format MUST be verified against its documented
  examples and, where the documentation shows none, against a capture from
  GSSquared checked in as a fixture.
- **FR-022b**: GSSquared commands that need hardware Casso lacks (`m` and `x`
  register width, `map` and `video` on a machine with no IIgs MMU) MUST report
  that they are not available and change nothing. A bank-qualified address
  (`00/300`) MUST be accepted for bank `00` and refused with a message for any
  other bank.

**WinDbg mode**

- **FR-022c**: WinDbg mode MUST accept, with WinDbg's arguments and mapped onto
  the same engine operations as their AppleWin equivalents: stepping and
  running (`t`, `p`, `g`, `gu`, `pa`, `ta`); breakpoints (`bp`, `bl`, `bc`,
  `bd`, `be`, and `ba r1|w1|e1 addr` for access breakpoints); memory display
  (`db`, `dw`, `dd`, `da`, with `l<count>` lengths); memory change (`eb`,
  `ew`, `ea`, `f` fill, `s` search, `m` move); `r` registers, `u` disassemble,
  `x` symbol lookup, `k` call stack, `?` expression; source (`l+s`, `lsa`, and
  `file:line` breakpoints with or without backquotes); and `.formats`.
  Addresses MUST be accepted as `0x300`, `300` and `$300`, and a bare number
  is hex, as in WinDbg.
- **FR-022d**: WinDbg commands whose meaning depends on host processes,
  threads, modules, exceptions, kernel structures, dump files, C types, the
  `dx` data model, extension loading or scripting MUST reply that the command
  has no meaning on this machine and change nothing; they MUST NOT be
  reported as unknown. The documentation MUST describe the mode as
  WinDbg-flavored and list what is excluded.

**Ways in**

- **FR-022**: The command-line tool MUST provide a debug mode that runs a
  machine and a script of commands, printing each command's output, with an
  option for structured output.
- **FR-023**: A running Casso MUST accept connections on a debug channel
  restricted to the current user, one channel per running instance, identified
  by process ID. The channel MUST exist only while that instance's debugger
  window is open; closing the window MUST close the channel and disconnect its
  clients.
- **FR-024**: Over the channel, clients MUST be able to send any command,
  receive a structured reply with the same content as batch structured output,
  and receive notifications for breakpoint hits, stops and resets. Any number
  of clients MAY be connected at once; their commands MUST run one at a time in
  arrival order, and every connected client MUST receive every notification.
- **FR-025**: The channel's message format MUST be documented well enough for
  an independent client (the planned VS Code debug adapter) to be written from
  the documentation alone. The format is one structured text record per line
  over a named pipe; it MUST NOT change to a binary framing in this feature.
- **FR-030**: The command-line tool MUST list the running Casso instances whose
  channel is open, giving each one's process ID, title label, machine and
  disks, and MUST connect to the instance whose process ID is given.

**Symbols, debug files and binaries**

- **FR-031**: The symbol commands MUST load symbol files in these formats:
  cc65's debug-info format version 2, Casso's earlier `-g` file
  (`NAME=$ADDR`), the symbol table at the end of a Merlin assembly listing,
  AppleWin `.SYM` (`ADDR NAME`), and VICE label files (`al ADDR .NAME`). The
  format MUST be recognized from the file's contents, not its extension. The
  shipped ROM symbol tables MUST be authored from Apple's published
  entry-point names, never taken from another emulator's symbol files.
- **FR-032**: Loading a binary (`BLOAD`, and batch mode's load option) MUST
  accept raw bytes at a given address, DOS 3.3 binary (4-byte address and
  length header), Intel HEX, Motorola S-record, and AppleSingle. Intel HEX,
  S-record and AppleSingle MUST be detected from content; raw and DOS 3.3
  binary MUST be selectable explicitly, since their contents cannot be told
  apart reliably. `BSAVE` and Monitor `W` write raw bytes.
- **FR-033**: `CassoCli as65 -g` and `CassoCli merlin -g` MUST write one debug
  file per output object in cc65's debug-info format version 2, carrying
  `file`, `line`, `span`, `seg`, `sym`, `mod` and `scope` records, with an
  added `sha1` key on each `file` record holding the SHA-1 of that file's text
  with line endings normalized to LF. Addresses in `span` records MUST be
  segment-relative, so a later linker can relocate them. Every source file,
  including included files, MUST have a `file` record. Each `-l` listing MUST
  end with a symbol table in Merlin's own listing format, verified against a
  listing produced by Merlin itself, checked in as a fixture.
- **FR-033a**: A macro expansion's address ranges MUST map to both the
  invocation line and the macro body line, with the nesting depth, so a
  source view can show either.
- **FR-033b**: A listing produced by Merlin 8/16 on the Apple II MUST be
  loadable as a debug file whose source is the listing itself: each listing
  line with an address maps to that address, and the listing's symbol table
  provides the symbols. The listing layout is verified against the Merlin
  corpus. Listings from the Merlin 32 cross-assembler are a follow-up.

**Window**

- **FR-026**: Casso MUST provide a debugger window beside the emulator with a
  command line in the selected mode and a console for its replies,
  disassembly with the current line marked and breakpoints marked, registers
  and flags, memory windows, a stack pane, a watch pane, a breakpoint pane, a
  trace pane, a source pane, device panels, a breakpoint gutter (FR-075),
  and controls for step, step over, step out, run, run to cursor and pause.
- **FR-026a**: Every pane MUST use a monospace font; rows MUST be no taller
  than the font's line height plus minor padding; columns MUST be sized to
  their contents with minor padding, and MUST NOT stretch to fill unused
  width; a pane MUST be tall enough by default to show at least eight entries.
- **FR-026b**: A Monitor-mode `R` or `W` entered in the window with no file
  name MUST open a file picker and run the command with the chosen file.
- **FR-026c**: The window's step, step over, step out, run, pause and toggle-
  breakpoint actions MUST have keyboard shortcuts in one of three selectable
  schemes: Visual Studio's (the default: F5, F10, F11, Shift+F11, Shift+F5,
  F9), AppleWin's (Space, Ctrl+Space, Enter, and its function keys) and
  GSSquared's (Space and F10, Return, O, R). The scheme is saved in preferences and
  is independent of the command mode.

- **FR-071**: While the code pane follows the PC, it MUST NOT scroll as long
  as the PC is on a line it already shows; only the PC marker moves. When the
  PC leaves the shown lines, the pane MUST re-anchor with the PC in its
  vertical middle. A pane moved away from the PC stays where it was put.
- **FR-072**: The code pane MUST show as many lines as fit its height.
- **FR-073**: The code pane MUST scroll through the entire address space in
  both directions. Above the first shown line it MUST disassemble backward by
  assuming the preceding bytes are code and choosing the alignment that ends
  exactly on an instruction boundary at the line below; where no alignment
  reaches it, the line below anchors the view.
- **FR-074**: The PC's row MUST have a highlighted background and its marker a
  bright, distinctive color, both from the active theme.
- **FR-075**: Breakpoints MUST be shown in a left gutter as a filled red dot,
  or an outline circle when disabled, sized to read at a glance and colored
  from the active theme. Clicking a row's gutter MUST toggle a breakpoint on
  that row.
- **FR-076**: Apart from the gutter, the code pane MUST behave as text:
  double-clicking selects the word under the pointer, a drag selects text, and
  a selection can be copied. Double-clicking MUST NOT change a breakpoint.
- **FR-077**: When another pane moves the code pane to an address (a call-stack
  frame, a breakpoint, a register, a watch), the target row MUST be marked so
  that what was brought into view is plain, and MUST be on the pane's middle
  line, or as near it as the ends of memory allow. No navigation leaves its
  target on the top or bottom line.
- **FR-078**: Each instruction MUST carry an annotation, in a comment-like
  color to the right of it, showing what it acts on: for a memory operand, the
  effective address where it is computed from registers (indexed and indirect
  modes) and the byte found there; for an operand given as a symbol, the
  symbol's address and the byte there; and the registers and individual flags
  the instruction reads (a conditional branch shows the flag it tests). The
  annotation's format MUST follow established practice in other disassembly
  viewers, to be surveyed in planning. Hovering a label or an operand's symbol
  MUST show the symbol's address and, for a shipped ROM symbol, a line saying
  what it is (a soft switch's read and write behavior, a zero-page location,
  a routine).
- **FR-079**: When the current instruction is a branch or jump, its
  destination row MUST be highlighted if it is on screen.
- **FR-080**: Double-clicking the PC in the registers pane MUST move the code
  pane to the PC.
- **FR-081**: The registers pane MUST show the flags decoded, one letter per
  flag (N V - B D I Z C), in a third column on the P register's row, labeled
  "Flags:". Hovering them MUST show each flag's name and value, one per line.
  Double-clicking P MUST open an editor with a checkbox per flag, and
  double-clicking S an editor for the stack pointer; each writes through `R`.
  The flags remain settable by command (`SEC`, `CLC` and the other set and
  clear forms, `R P`).
- **FR-082**: The stack pane MUST list the stack newest first: the most
  recently pushed byte at the top, the oldest at the bottom, as the call-stack
  pane lists frames.
- **FR-083**: Ctrl+Plus, Ctrl+Minus and Ctrl+0 MUST enlarge, reduce and reset
  the text size of every debugger content pane, floating panes included, and
  MUST NOT change any other Casso window, the debugger's caption, or its
  command bar.
- **FR-084**: Every pane MUST have a context menu of actions on its content,
  drawn by the debugger's own menu widget like every other menu in Casso.
  Actions on the tab or window (dock, float, auto-hide, close) MUST be in the
  tab's own context menu.
- **FR-085**: The command bar's Dialect, Panels and Keys entries MUST open
  drop-down menus in the style of the main window's Theme and Color entries.
- **FR-086**: The command bar's icons MUST match Visual Studio's debugging
  toolbar icons for the same actions (continue, break, stop, restart, show next
  statement, step into, step over, step out, run to cursor), drawn where the
  icon font has no matching glyph. Run to cursor is a right arrow ending at a
  vertical bar.

**Memory editing**

- **FR-034**: The window MUST offer up to four memory windows, each with its
  own start address and grouping (8, 16 or 32 bits per value), each showing
  hex values and their text interpretation side by side.
- **FR-035**: A user MUST be able to click a hex cell or a text cell and type
  a new value; the value MUST be written to the machine as soon as it is
  complete (two, four or eight hex digits, or one character), and focus MUST
  move to the next cell. Escape MUST abandon a partly typed value.
- **FR-036**: Each memory window MUST keep an undo history of its own edits;
  undo MUST restore an edit's previous value, most recent first. The history
  MUST be cleared by a machine switch. Writes from the command line or from
  an attached client are not part of any history.
- **FR-037**: A write to RAM MUST go through as a bus write. A write to ROM
  MUST patch the loaded ROM image so the machine reads the new value; the
  patch MUST survive a ROM bank switch on machines that have one, and undo
  MUST restore the original byte. An I/O address MUST NOT be editable from a
  memory window; the `OUT` command remains the way to write one.

- **FR-087**: A memory window MUST highlight, in a theme color, each shown byte
  whose value changed since the previous update.
- **FR-088**: The byte with keyboard focus MUST have a distinctive background
  and a bright foreground, both from the active theme.
- **FR-089**: A memory window's controls (the Go to box, the poke box, Poke,
  Bytes, and removing a memory window) MUST sit in a command bar at the top of
  the memory pane. A + after the last memory tab MUST open the next memory
  window, as a browser opens a tab, until four are open.
- **FR-090**: The address box MUST be a Go to box accepting a hex address; a
  register (PC goes to the address the PC holds, and A, X and Y go to their
  value on the zero page); or a 6502 addressing expression resolved against
  the current registers and memory -- `zp`, `abs`, `zp,X`, `zp,Y`, `abs,X`,
  `abs,Y`, `(zp,X)`, `(zp),Y` and `(abs)` -- going to the effective address.
  Resolving an expression MUST NOT read an I/O address.
- **FR-091**: Go to MUST place the target address at the pane's top-left, not
  at the start of the 16-byte row containing it.

**Breakpoints pane**

- **FR-092**: Each breakpoint row MUST begin with a circle, filled red when
  enabled and outlined when disabled; clicking it MUST toggle the breakpoint
  enabled without removing it.
- **FR-093**: Double-clicking a breakpoint row MUST move the code pane to the
  breakpoint's address and MUST NOT change the breakpoint.
- **FR-094**: A breakpoint row's context menu MUST offer removing the
  breakpoint and editing it, in a dialog of its type and the fields that type
  requires.
- **FR-102**: The console MUST read as a command prompt: the dialect in force
  as a fixed prompt ahead of what is typed ("WinDbg>"), which cannot be edited,
  and a hint giving that dialect's help command. Every control that sends an
  engine command (the Dialect menu's `MODE` among them) MUST send it through
  the dialect's marker (FR-014), so no dialect can trap the session. The
  console's output MUST be text, not a list of rows.
- **FR-103**: Memory, flags and registers MUST be edited only while the machine
  is paused; while it runs, the memory panes are read-only, the register
  editors say to pause, and every command that writes registers or memory
  (`R`, the flag commands, the memory entry, fill, move and patch commands,
  loads into memory, stack pushes and pulls, the assembler) is refused with
  the same instruction, from any way in.
- **FR-104**: Right-clicking a code line MUST offer showing, in each open memory
  pane, the address clicked or, over the operand, the operand's address
  resolved through its addressing mode ("Show (BASL),Y in Memory 1").
- **FR-105**: The code pane MUST scroll by instructions with the mouse wheel
  through the whole address space, paused or running.
- **FR-106**: A + after the disassembly tabs MUST open another disassembly view
  at the PC, up to four. Exactly one view follows the PC; the rest stay where
  they are put and still show the PC's arrow and row when it is on their
  lines. The tabs are titled Disassembly 1 to 4. Once a second view is open,
  the tab of the view following the PC MUST carry a dot in the PC marker's
  color ahead of its title and a tip saying it follows the PC, the first view
  following at first. Every other disassembly tab's menu MUST offer Follow PC,
  which hands the PC to it and leaves the one giving it up where it stands.
  Closing the view that follows hands the PC back to the first.
- **FR-107**: The code pane MUST annotate the line at the PC with what
  executing that instruction would leave behind -- the value it writes to a
  register or an address, the flags it sets, or where it puts the PC -- in a
  column of its own and a color distinct from the operand annotations of
  FR-078. NO OTHER LINE CARRIES ONE, since only the PC's line has register
  values that are true. An instruction whose result is not modeled, decimal
  arithmetic among them, MUST be left unannotated rather than guessed at. An
  instruction that reads a register without touching memory (a compare
  against an immediate, for one) MUST show that register's value among its
  operand annotations.
- **FR-108**: Show Next Statement MUST bring the disassembly view following
  the PC to the front of its tabs and put the PC on its middle line.
- **FR-109**: Each command bar entry MUST carry a tip with its title, the key that
  runs it in the keyboard scheme in force, and what it does, and the tip MUST
  follow a change of scheme.
- **FR-110**: The operand and result annotations of FR-078 and FR-107 MUST be
  built only while the machine is paused. While it runs they carry a byte read
  at an arbitrary moment mid-instruction, which is not what the line will do,
  and no one can read them at speed.
- **FR-111**: A predicted result MUST come from the emulator's own execution
  of the instruction against a copy of the registers, with its reads answered
  without touching the machine and its writes captured rather than performed.
  A SECOND DESCRIPTION OF WHAT AN INSTRUCTION DOES MUST NOT EXIST: a table
  written beside the CPU can disagree with it, and a prediction that disagrees
  with the machine is worse than none. Where a read cannot be answered without
  a side effect, the line MUST be left unannotated.
- **FR-112**: An operand in `$C000-$C0FF` MUST be shown as the soft switch the
  instruction operates, with its title and description, chosen by whether it
  reads or writes it, since one address is two switches (`$C000` read is the
  keyboard, `$C000` written is 80STORE off). Such an operand MUST NOT be
  annotated with a byte value, which does not exist to be read, and a write
  MUST be described as the switch's action rather than as a store.

**Watch pane**

- **FR-095**: The watch pane MUST list automatic watches above manual watches,
  divided by a separator. Automatic watches MUST be every register, memory
  address and individual flag read or written by the instruction at the
  current PC and by the instruction at the previous PC. An automatic watch's
  expression MUST NOT be editable; its value MUST be.
- **FR-096**: A manual watch MUST edit in place: double-clicking its expression
  replaces the expression with what is typed, and double-clicking its value
  writes what is typed to the watched address. A manual watch MUST be
  removable with Delete and from its context menu.
- **FR-097**: The watch pane MUST keep an undo history of its own value and
  expression edits, separate from every memory window's.
- **FR-098**: The watch pane MUST highlight each value that changed since the
  previous update.

**Docking**

- **FR-038**: Panes MUST be arrangeable as a tree of split groups and tab
  groups, nested to any depth, with a resizable splitter between the panes of
  a split group that enforces each pane's minimum size.
- **FR-039**: A pane MUST be movable by dragging its title, with drop zones
  shown for each side of each group and for tabbing into a group; dropping a
  pane onto its own position changes nothing.
- **FR-040**: A pane MUST be floatable as its own top-level window, by
  dragging it out or by command, and dockable again by dragging it into a drop
  zone or by command.
- **FR-041**: A pane MUST be pinnable to auto-hide: collapsed to a tab on the
  nearest window edge, shown on hover or click, and hidden again when focus
  leaves it.
- **FR-042**: Every docking operation MUST be reachable from the keyboard: a
  Dock To menu on each pane offering each side, each pane to tab with, float
  and auto-hide, and arrow keys to move a pane within its group.
- **FR-043**: Floating panes MUST render at the scale of the monitor they are
  on and MUST rescale when dragged across a boundary between monitors of
  different scale.
- **FR-044**: The layout MUST be saved when the debugger closes and restored
  when it opens, including floating panes' monitors, positions and sizes; a
  pane whose monitor is absent MUST open on the primary monitor; a layout the
  debugger cannot read MUST fall back to the default layout. There is one
  layout for every machine: a device panel the current machine lacks MUST be
  closed on restore and MUST keep its saved place, so it reopens there when a
  machine that has the device is loaded.

**Instruction trace**

- **FR-045**: The debugger MUST offer an instruction trace that, while on,
  retains the most recent 100,000 executed instructions, each with its cycle
  count, program counter, opcode bytes, registers before execution, and, for
  an instruction that accessed memory, the address, the direction and the
  data byte.
- **FR-046**: The trace MUST be off by default and switchable from the window,
  from a command, and over the channel. While it is off, emulation MUST run
  the same code path it ran before the debugger existed, with no per-
  instruction test for it.
- **FR-047**: The trace pane MUST show the retained entries ending at the
  most recent, MUST let the user scroll to any entry, and MUST show the
  symbol for an entry's address and accessed address when one is loaded.
- **FR-048**: The retained trace MUST be savable to a file with every entry.

**Call stack**

- **FR-067**: The window MUST provide a call-stack pane, separate from the raw
  stack pane, listing the chain of calls to the current instruction innermost
  first: each frame's call-site address and symbol, and for an interrupt frame
  the vector taken. Double-clicking a frame MUST move the disassembly to the
  call site. The same chain MUST be available as a command in every mode.
- **FR-068**: Three mechanisms MUST exist: recorded calls (a record kept as
  `JSR`, `BRK`, interrupt dispatch, `RTS` and `RTI` execute, kept only while
  the debugger is attached); stack walk (the stack page scanned for return
  addresses whose preceding opcode is `JSR` and, when a debug file is loaded,
  whose `JSR` targets a known routine entry); and hybrid, which uses recorded
  frames where they exist and extends below them with the walk. The pane MUST
  always show hybrid, with no selector; all three MUST be selectable by
  command (`CALLS MODE`). Every frame MUST say which mechanism produced it
  (FR-099).
- **FR-069**: The pane MUST detect and show, as a separator row naming the
  instruction and its address, each event that breaks a chain: `TXS`
  reloading the stack pointer; a pull that consumes a frame's return address;
  a frame ending by a jump rather than a return; a return whose address
  differs from the one its call pushed; the stack pointer wrapping past $0100;
  a reset; and, for the recorded mechanism, the point below which tracking had
  not yet begun. Frames below a break MUST be shown as unverified. A return
  whose address is a few bytes past the one pushed MUST be reported as
  returning past inline parameters and MUST NOT be treated as a break.

- **FR-099**: Each frame MUST state its source in plain words (for example,
  "recorded" or "found on the stack"), and the marker for the point below
  which the recorder has no history MUST say that the recorder began there,
  when the debugger attached, and that nothing below it is known -- worded so
  it cannot be read as a reference to the trace pane.
- **FR-100**: When recording begins before the machine has run an
  instruction since power-on, or a power cycle happens while it records, the
  bottom of the chain MUST be marked as power-on, with the reset address and
  cycle 0, and the stack walk MUST NOT extend below it. The Debug menu MUST
  offer Restart Under Debugger, which opens the debugger and power-cycles the
  machine so the record starts at power-on. `--debugger` MUST begin recording
  before the first instruction. Neither is the default.
- **FR-101**: A store that changes a byte of a recorded frame's return
  address MUST mark that frame when the store runs, naming the store's
  address, and the frame's return MUST NOT then be reported as a mismatch.
  Pushes, including those that replace a pulled return address, are not
  stores. The undocumented 6502 instructions that load the stack pointer
  (`TAS`, `LAS`) MUST be treated as `TXS`.

**Device panels**

- **FR-049**: Each device model that publishes diagnostics MUST do so as
  named groups of rows, each row a label and a value, with an optional bit
  decode giving each bit a label. The window MUST render any such panel
  without device-specific code, so a device on a future machine gets a panel
  by publishing rows. Panels are read-only: a panel MUST NOT change device
  state; changes go through commands.
- **FR-050**: The first release MUST publish panels for: the Disk II
  controller (phase magnets, quarter track, motor state and spin-up, the read
  state), the //e MMU and soft switches (every switch, and for each page which
  bank it reads and writes from), the video mode, the keyboard (latch, strobe,
  modifiers), the Mockingboard (each 6522's ports, timers and interrupt
  registers; each sound chip's registers), the printer (head position and
  state), and the clock (cycle counters and speed).
- **FR-051**: A panel MUST update at least once per frame while the machine
  runs and MUST show the stopped state when the machine stops.
- **FR-052**: Three visuals MUST be available in addition to rows: a memory
  map bar showing every page colored by its read and write source, a disk
  head-position graphic at quarter-track resolution, and level meters for the
  sound chips' channels and the 6522 timers.
- **FR-053**: The panel list MUST offer only the devices present in the
  current machine and MUST change when the machine changes.

**Source-level debugging**

- **FR-054**: The window MUST provide a source pane that shows the file and
  line for the program counter whenever the machine stops, marks that line,
  and stays synchronized with the disassembly pane: selecting a line in
  either selects the corresponding position in the other.
- **FR-055**: Users MUST be able to set and clear breakpoints on source lines;
  a line with no code MUST move the breakpoint to the next line with code and
  say so.
- **FR-056**: Users MUST be able to step into, over and out by source line:
  into stops at the first instruction of a different source line; over stops
  at the next source line in the same routine, running calls to completion by
  the stack-pointer rule of FR-002; out stops at the line after the call that
  entered the current routine.
- **FR-057**: When the machine stops inside a macro expansion, the source pane
  MUST show the invocation line, indicate that the position is inside a
  macro, and MUST be able to show the body line on request.
- **FR-058**: The debugger MUST find a source file by, in order: the path
  recorded relative to the debug file; the folders where sources for this
  program were found before; the folders where any sources were found before;
  a file the user drags onto the debugger. A candidate MUST match the
  recorded file name and size before its hash is computed, and MUST match the
  recorded hash to be used without a warning.
- **FR-059**: A source file whose hash differs from the recorded one MUST
  open with a visible warning that lines may not match. A file that matches
  no entry MUST open as plain text with no line mapping.
- **FR-060**: The folders where sources were found MUST be remembered across
  sessions, per program and globally.

**Breakpoints and profiling**

- **FR-061**: A breakpoint or watchpoint MUST accept an expression condition
  (`IF <expression>`) over registers, flags, symbols, and memory reads; the
  expression MUST be evaluated only when the breakpoint's address or access
  hits, and the machine MUST stop only when it is true. An expression that
  cannot be evaluated when set MUST be reported and MUST NOT create the
  breakpoint. An expression MUST NOT read an I/O address.
- **FR-062**: A value breakpoint MUST stop the machine when a write leaves a
  given address holding a given value.
- **FR-063**: `PROFILE LIST` MUST report, for the profiled run, the count, the
  cycles and the share of total cycles per opcode and addressing mode; a
  per-address form MUST report the hottest addresses with their symbols; and
  avoidable cycles MUST be reported apart from base cycles by kind:
  page-crossing penalties on indexed reads, taken branches, and branches
  crossing a page. `PROFILE SAVE` MUST write the same content to a file.

**Performance**

- **FR-064**: With no debugger window open and no trace, watch or hook
  active, emulation MUST run the same code path and at the same speed as
  before this feature, within measurement noise. The call-stack record is a
  hook in this sense: it runs only while the debugger is attached.

**Release material**

- **FR-065**: The README MUST show a screenshot of the debugger stopped in the
  Mockingboard speech demo with its symbols loaded, taken from a Casso whose
  window title is not a spec name.

**Verification**

- **FR-027**: A test MUST decode the Monitor command table of every Apple II ROM
  Casso ships and confirm every entry other than filler is a Monitor-mode
  command.
- **FR-028**: A test MUST confirm that disassembling the original Apple ][
  Monitor ROM matches the Monitor listing published in the 1979 *Apple II
  Reference Manual*.
- **FR-029**: Where the `!` mini-assembler lives in the Enhanced //e and //c
  ROMs, and lowercase acceptance on those machines, MUST be verified against
  the fixture ROMs before being relied on. `F666G` is Casso's alias for `!` on
  every machine; $F666 is the mini-assembler only in the original Apple ]['s
  Integer BASIC ROM.
- **FR-066**: A test MUST assemble a source with an include file and a
  two-level macro with both Casso assemblers and confirm the debug file's
  line entries resolve every emitted address to its invocation line and its
  body line, and that cc65's own reader accepts the file.

### Key Entities

- **Debug session**: The debugger's attachment to one running machine; holds
  breakpoints, watchpoints, the selected mode and the paused/running state.
- **Breakpoint**: A stop condition by address, by opcode, by a register
  comparison, by a memory value, or by an expression evaluated when the
  location hits; can be enabled or disabled.
- **Watchpoint**: A stop condition on a read or write of an address or range,
  stopping either after the access (the default, which reports the value and
  the value a write replaced) or before the instruction that would make it;
  may carry an expression condition.
- **Command mode**: AppleWin, Apple II Monitor, GSSquared or WinDbg;
  determines how a command line is read.
- **Output format**: AppleWin, Monitor, GSSquared or WinDbg; determines how
  replies are written. Follows the command mode when it changes, and can be
  set alone.
- **Call-stack frame**: One call in the chain to the current instruction: its
  call site, its target, the mechanism that produced it, and any break
  reported at it.
- **Step filter**: The routines a step into treats as a step over.
- **Command**: One line of input in a mode; produces a reply.
- **Reply**: The result of a command, in text and in structured form.
- **Notification**: An unsolicited message to an attached client: breakpoint
  hit, stop, reset.
- **Cycle budget**: The maximum number of emulated CPU cycles a run may
  execute. Always set for batch and `--attach` runs; optional otherwise.
- **Debug file**: A cc65-format file describing a program: its source files
  with their sizes and hashes, its segments, the address spans each source
  line produced, its symbols, and its modules and scopes.
- **Line table**: The mapping from addresses to source lines and back, built
  from a debug file or a listing; carries macro nesting.
- **Source path list**: The remembered folders where source files were found,
  per program and globally.
- **Memory window**: One view of memory at an address with a grouping, with
  its own undo history.
- **Undo history**: The ordered list of a memory window's edits, each with the
  address, the value written and the value replaced.
- **Trace entry**: One executed instruction's record: cycle count, program
  counter, opcode bytes, registers, and any memory access.
- **Diagnostic panel**: A device's published state as groups of labeled rows,
  optionally with bit decodes and a visual.
- **Layout**: The tree of split groups, tab groups, floating panes and
  auto-hidden panes, with sizes and monitors, saved between sessions.

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
- **SC-005**: No batch script or `--attach` run can run indefinitely: every
  such run ends within its cycle budget.
- **SC-006**: A breakpoint set through any way in stops the machine at the
  correct instruction in 100% of test cases, and is visible from the other two.
- **SC-007**: An independent client written only from the channel
  documentation can set a breakpoint, resume, and receive the stop notification.
- **SC-008**: With the debugger window closed, emulation throughput is within
  1% of the release before this feature, measured the same way on the same
  machine.
- **SC-009**: With the debugger window open and the trace off, emulation
  throughput is within 3% of that baseline.
- **SC-010**: Every source line that produced code in the fixture programs
  resolves to its address and back, including lines inside included files
  and macro expansions, in 100% of cases.
- **SC-011**: A source-level step over lands on the next source line in 100%
  of the fixture cases, including a call with inline parameters and a
  recursive call.
- **SC-012**: A user can edit a byte in a memory window and see the machine
  use it within one frame, and undo it, without touching the command line.
- **SC-013**: The trace retains 100,000 instructions, and the trace pane shows
  any retained entry within one second of the user scrolling to it.
- **SC-014**: A layout with two floating panes on a second monitor is
  restored exactly after closing and reopening the debugger, and restored
  onto the primary monitor when the second is absent.
- **SC-015**: Every device panel in FR-050 updates within one frame of the
  state it shows changing.
- **SC-016**: Every command in GSSquared's command list produces the same
  engine effect as its AppleWin equivalent in 100% of test cases.
- **SC-017**: A debug file written by `CassoCli as65 -g` is read without
  error by cc65's own debug-info reader.
- **SC-018**: On the call-stack fixture (a recursive routine, an
  inline-parameter routine and an interrupt), the recorded mechanism reports
  the true chain at 100% of stops, and each break in FR-069 is detected at the
  instruction that caused it in 100% of the cases that provoke it.
- **SC-019**: Every command in FR-022c produces the same engine effect as its
  AppleWin equivalent in 100% of test cases, and every excluded WinDbg command
  replies that it has no meaning here.
- **SC-020**: With `COUT` in the step filter, no step into `JSR COUT` stops
  inside the ROM, in 100% of fixture cases.
- **SC-021**: Stepping through any run of instructions that stays on the code
  pane's shown lines changes its first line 0 times; a step that leaves them
  puts the PC within one line of the pane's vertical middle, in 100% of cases.
- **SC-022**: Every address from $0000 to $FFFF can be brought into the code
  pane by scrolling alone.
- **SC-023**: For each addressing form in FR-090, a Go to entry lands the memory
  pane with its effective address as the first cell, matching a hand-computed
  address in 100% of test cases.
- **SC-024**: Between two updates, every shown memory byte and watch value that
  changed is highlighted and no unchanged one is, in 100% of test cases.
- **SC-025**: For an instruction of each 6502 addressing mode, the automatic
  watches list exactly the registers, flags and addresses it reads or writes --
  none missing, none extra.
- **SC-026**: A text-size change reaches every debugger content pane, floating
  ones included, and changes the size of nothing outside them.

## Assumptions

- **Engine-command markers per mode**: Monitor `/` (the text after it is read
  as an AppleWin-mode command line, `/bpl`; Ctrl-Y is the machine's own user
  command and `!` its mini-assembler), AppleWin bare names, GSSquared bare
  names (`/` is its bank separator), WinDbg `!` (its extension-command
  prefix).
- **AppleWin command coverage**, by name from AppleWin's command table:
  - **Phase 1 (headless)**:
    - Assembler: `A`.
    - CPU: `=`, `G`, `GG`, `IN`, `KEY`, `JSR`, `NOP`, `OUT`, `LBR`, `PROFILE`,
      `R`, `POP`, `PPOP`, `PUSH`, `P`, `RTS`, `T`, `TF`, `TL`, `U`.
    - Bookmarks: `BM`, `BMA`, `BMC`, `BML`, `BMG`, `BMSAVE`.
    - Breakpoints: `BRK`, `BRKOP`, `BRKINT`, `BP`, `BPA`, `BPR`, `BPX`, `BPIO`,
      `BPM`, `BPMR`, `BPMW`, `BPC`, `BPD`, `BPEDIT`, `BPE`, `BPL`, `BPSAVE`,
      `BPCHANGE`.
    - Config: `BENCHMARK`, `DISASM`, `LOAD`, `SAVE`, `PWD`, `CD`.
    - Cycles: `CYCLES`, `RCC`.
    - Disassembler data: `Z`, `X`, `B`, `DB`, `DB2`, `DB4`, `DB8`, `DW`, `DW2`,
      `DW4`, `ASC`, `DF`, `DA`.
    - Disk: `DISK`.
    - Flags: `CL`, `CLC`, `CLZ`, `CLI`, `CLD`, `CLB`, `CLR`, `CLV`, `CLN`, `SE`,
      `SEC`, `SEZ`, `SEI`, `SED`, `SEB`, `SER`, `SEV`, `SEN`.
    - Help: `?`, `HELP`, `VERSION`, `MOTD`.
    - Memory: `MC`, `ME`, `MEB`, `MEW`, `BLOAD`, `M`, `BSAVE`, `S`, `@`, `SH`,
      `F`, `TSAVE`.
    - Output and scripts: `CALC`, `ECHO`, `LOG`, `PRINT`, `PRINTF`, `RUN`.
    - Symbols: `SYM`, `SYMMAIN`, `SYMBASIC`, `SYMASM`, `SYMUSER`, `SYMUSER2`,
      `SYMSRC`, `SYMSRC2`, `SYMDOS33`, `SYMPRODOS`, `SYMINFO`, `SYMLIST`.
    - Watch: `W`, `WA`, `WC`, `WD`, `WE`, `WL`, `WSAVE`.
    - Zero page: `ZP`, `ZP0`-`ZP7`, `ZPA`, `ZPC`, `ZPD`, `ZPE`, `ZPL`, `ZPSAVE`.
    - Startup: `STARTUP`.
    - Aliases: `INPUT`, `RC` `RZ` `RI` `RD` `RB` `RR` `RV` `RN`, `SC` `SZ` `SI`
      `SD` `SB` `SR` `SV` `SN`, `D`, `ME8`, `ME16`, `MM`, `MS`, `P0`-`P4`,
      `REGISTER`, `TRACE`, `SYMDOS`, `SYMPRO`, `ZAP`.
    - Deprecated: `BENCH`, `EXITBENCH`, `MDB`.
  - **Phase 3 (with the window; delivered)**, commands whose only effect is on
    a display:
    - Cursor: `.`, `RET`, `^`, `v` and their Shift forms, `PAGEUP`,
      `PAGEUP256`, `PAGEUP4K`, `PAGEDN`, `PAGEDOWN256`, `PAGEDOWN4K`, and the
      `->` cursor aliases.
    - Window: `WIN`, `WINDOW`, `CODE`, `CODE1`, `CODE2`, `CONSOLE`, `DATA`,
      `DATA1`, `DATA2`, `SOURCE1`, `SOURCE2`, `\`. With docking, `CODE`,
      `DATA`, `CONSOLE`, `SOURCE1` and `SOURCE2` bring the matching pane to
      the front; the numbered forms select a memory window.
    - Mini memory panes: `MD1`, `MD2`, `MA1`, `MA2`, `MT1`, `MT2`, `M1`, `M2`:
      each moves a memory window and sets its grouping and text column.
    - Views: `TEXT`, `TEXT1`, `TEXT2`, `TEXT80`, `TEXT81`, `TEXT82`, `TEXT40`,
      `TEXT41`, `TEXT42`, `GR`, `GR1`, `GR2`, `DGR`, `DGR1`, `DGR2`, `HGR`,
      `HGR0`-`HGR8`, `DHGR`, `DHGR1`, `DHGR2`: not available; the emulator
      window shows the screen.
    - Appearance: `BW`, `COLOR`, `FONT`, `HCOLOR`, `MONO`: not available; the
      Casso theme sets the window's colors and font.
  - **Not available** (accepted, reported as not available, and tracked as
    follow-ups): `SHR` (needs a IIgs), `SYNC` (needs an assembler-listing
    link beyond what the source pane provides), `NTSC` (AppleWin's palette
    file has no Casso equivalent). `BPV` and `VIDEOINFO` are phase 1, served
    from the machine's video timing. `SOURCE` becomes the source pane.
  - The table is consulted for names and behavior only; `RUN` runs a script of
    commands through the same engine batch mode uses.
- **GSSquared command coverage** is taken from its published debugger
  documentation and, for its command table, its tokenizer's behavior and its
  window's step keys, from a reading of its source on 2026-09-18; its code is
  not copied.
- **WinDbg command coverage** is taken from Microsoft's published command
  reference; the mode is WinDbg-flavored, and its output layouts follow
  WinDbg's documented examples.
- **No standalone console**: interactive command entry is through the window's
  command line; scripting is through batch mode and the channel.
- **The window builds on `032-dxui-command-widgets`** (merged) and on the
  resizable-pane control delivered by `033-cassque`; the docking framework is
  new to this feature and belongs to the UI library so other windows can use
  it.
- **cc65's debug-info format** is version 2 as written by its linker. Its
  reader skips unknown keys and record types with a warning, so the `sha1`
  key on `file` records is read by Casso and ignored by cc65-based tools. The
  format's own documentation calls it subject to change; this feature pins
  version 2. cc65's linker writes `.dbg` by default, the same extension as
  Casso's earlier symbol file, which is why formats are recognized from
  contents. The `mtime` and `size` keys cc65 writes are kept for
  compatibility; only `size` and `sha1` decide a match.
- **Listings from Merlin 8/16** carry line numbers, addresses, bytes and
  source text but no file records. Whether an included (`PUT`) file is marked
  in the listing, and how its lines are numbered, is verified against the
  Merlin corpus during planning.
- **Macros in cc65's format**: a `line` record's `type` and `count` keys
  carry the macro nesting; the exact values are verified against cc65's
  reader during planning.
- **Casso has no cassette device**, so `R`/`W` use host files. `.wav` is
  reserved for cassette audio in a later feature; this feature does not produce
  audio.
- **There is no promise to leave guest state untouched**: commands such as `I`,
  `N`, `^K`, `^P` and `:` change guest memory by design.
- **The CPU's registers are the single truth in all modes.** The real
  Monitor keeps a copy at $45-$49 and reloads it on `G`; Casso's `^E` and `:`
  write both the registers and $45-$49, and `G`, `S` and `T` never reload from
  memory, so a register set in AppleWin mode survives a Monitor `G`.
- **The command mode and output format are session state**: both start as
  AppleWin each time Casso or the batch tool starts, and neither is saved in
  preferences. The layout is saved in preferences. The batch tool's mode
  option sets both; a separate option sets the output format alone.
- **`GG` in the emulator** runs at full speed and restores the previous speed
  setting when the run stops. In batch, every run is unthrottled.
- **Debug channel**: a Windows named pipe whose name holds the process ID, carrying
  one structured record per line, per the Windows-only platform scope. A
  per-instance channel is required because users run several Casso instances
  from different worktrees at once.
- **Trace cost while on** is accepted: the emulator is throttled to 1 MHz in
  normal use, and the cost shows only at full speed.
- **Memory-window edits and I/O**: an I/O write changes banking and switches
  under the running program, so a stray keystroke in a memory window must not
  do it; `OUT` exists for the deliberate case.
- **Docking targets**: a pane's minimum size is what shows one row of its
  content; the default layout is the first version's arrangement with the
  source pane tabbed with the disassembly and the trace tabbed with the
  console.
- **Out of scope**: WinDbg beyond FR-022c (the process, thread, module,
  exception, kernel, dump, type, `dx` and scripting families, and `wt`, which
  maps onto the trace and the profile once they exist); gdb syntax; the VS Code debug adapter (GH #54); whole-program
  disassembly (GH #121); the IIgs, C64 and NES monitors, which arrive with
  those machines as additional modes; beam-position debugging (a crosshair at
  the emulated beam position over a partial frame), deferred until the color
  video model; importing Merlin 32 listings (follow-up issue); the 6502's dummy reads and double writes (GH #150); the `$C3xx`
  write latch (GH #151); command lists attached to breakpoints (GH #152); a
  binary channel framing.
- **Existing starting points**: the CPU's instruction trace and the windowless
  host used by the command-line tool.
- **Clean-room**: AppleWin is consulted for command names and behavior only,
  and its implementation is not read. GSSquared's source was read for its
  command table and input behavior; no code from it is copied.

- **Call-stack recording starts when the debugger attaches**, as FR-068 says,
  and from power-on only when asked for (FR-100). Measured 2026-09-21, pinned,
  against the bare machine: +1.9% at boot and +1.4% at the Applesoft prompt,
  and +124% on a loop that is one-third JSR/RTS, still about 220 times real
  //e speed. Attaching with recording off costs nothing measurable. Watching
  the stack page for FR-101 adds nothing measurable at boot or idle and 30% on
  the call loop. The cost is host time only; the guest's clock is unchanged.
- **Backward disassembly can be wrong over data** (FR-073). The code pane
  assumes code above the line it scrolls from, as other disassemblers do.
- **The annotation format of FR-078 is settled in planning** after a survey of
  other disassembly viewers; the requirement fixes what an annotation says, not
  how it is laid out.
- **Whether FR-083 is a font-size change or a zoom** is a planning decision;
  either satisfies the requirement if nothing outside the content panes changes
  size.

### References

- GH #51 (debugger), GH #59 (merged into #51), GH #121 (whole-program
  disassembly), GH #148 (boot sector listing), GH #54 (VS Code extension),
  GH #150 (dummy reads), GH #151 (`$C3xx` write latch), GH #152 (breakpoint
  command lists).
- AppleWin debugger tutorial:
  https://github.com/AppleWin/AppleWin/blob/master/help/dbg-toc-intro.html
- AppleWin command table:
  https://github.com/AppleWin/AppleWin/blob/master/source/Debugger/Debugger_Commands.cpp
- AppleWin symbol tables:
  https://github.com/AppleWin/AppleWin/blob/master/help/dbg-symbols.html
- WinDbg command reference:
  https://learn.microsoft.com/windows-hardware/drivers/debugger/commands
- Mesen's call-stack window and VICE's `bt` command, as the call stack's
  parity targets.
- GSSquared debugger:
  https://github.com/jawaidbazyar2/gssquared/blob/main/Docs/UsingTheDebugger.md
  and https://github.com/jawaidbazyar2/gssquared/blob/main/Docs/Debugger.md
- cc65 debug-info format, reader source (record keys and lookups):
  https://github.com/cc65/cc65/blob/master/src/dbginfo/dbginfo.c
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