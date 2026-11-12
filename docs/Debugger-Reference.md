# The Casso debugger reference

Casso's debugger stops a running Apple II program, displays and changes its
memory and registers, and runs it again under your control. One engine sits
behind every way in, and every command works the same in each.

The walk-throughs in [Debugger-Tutorial.md](Debugger-Tutorial.md) use the demo
disks in `Apple2/Demos`. Every command of every mode, with its syntax, is in
[Debugger-Commands.md](Debugger-Commands.md); what the commands and the window
do is documented here, with links there for the syntax.

## Contents

1. [Ways in](#ways-in): [opening the window](#opening-the-window),
   [attaching](#attaching), [batch mode](#batch-mode)
2. [The window](#the-window): [menus](#menus),
   [the command bar](#the-command-bar), [the console](#the-console),
   [the status bar](#the-status-bar), [panes and docking](#panes-and-docking),
   [registers and stack](#registers-and-stack)
3. [Code views](#code-views): [disassembly views](#disassembly-views),
   [source rows](#source-rows), [branch arrows](#branch-arrows),
   [the row menu](#the-row-menu), [data marking](#data-marking),
   [bookmarks](#bookmarks), [heat columns](#heat-columns)
4. [Breakpoints](#breakpoints): [execution](#execution-breakpoints),
   [data](#data-breakpoints), [opcodes and interrupts](#opcode-and-interrupt-breakpoints),
   [the video beam](#beam-breakpoints), [disk](#disk-breakpoints),
   [conditions](#conditions), [breakpoint name](#breakpoint-name),
   [the Breakpoints pane](#the-breakpoints-pane)
5. [Stepping and history](#stepping-and-history):
   [running and stopping](#running-and-stopping),
   [controlling the machine](#controlling-the-machine),
   [the step filter](#the-step-filter), [stepping back](#stepping-back),
   [history options](#history-options),
   [the history timeline](#the-history-timeline),
   [the network in history](#the-network-in-history),
   [the call stack](#the-call-stack)
6. [Symbols and source](#symbols-and-source):
   [symbol tables](#symbol-tables), [symbol tips](#symbol-tips),
   [symbols for a program that isn't in memory](#symbols-for-a-program-that-isnt-in-memory),
   [debug files](#debug-files), [source documents](#source-documents),
   [the Symbols pane](#the-symbols-pane)
7. [Memory, watches and heat](#memory-watches-and-heat):
   [memory windows](#memory-windows),
   [loading and saving binaries](#loading-and-saving-binaries),
   [watches](#watches), [the heat map](#the-heat-map)
8. [Devices and logs](#devices-and-logs): [device panels](#device-panels),
   [Disk II](#disk-ii), [the Mockingboard](#the-mockingboard),
   [video and sound logs](#video-and-sound-logs),
   [serial and network logs](#serial-and-network-logs),
   [the input log](#the-input-log), [the cassette deck](#the-cassette-deck)
9. [Applesoft](#applesoft): [BASIC breakpoints and steps](#basic-breakpoints-and-steps),
   [loading and saving programs](#loading-and-saving-programs),
   [Applesoft in the window](#applesoft-in-the-window)
10. [Profiling and trace](#profiling-and-trace): [the profiler](#the-profiler),
    [the Profiler pane](#the-profiler-pane),
    [the flame graph and timeline](#the-flame-graph-and-timeline),
    [the stopwatch](#the-stopwatch), [the instruction trace](#the-instruction-trace)
11. [Projects](#projects)
12. [Command modes and scripting](#command-modes-and-scripting):
    [command modes](#command-modes), [GSSquared mode](#gssquared-mode),
    [WinDbg mode](#windbg-mode), [output format](#output-format),
    [getting help](#getting-help), [mistyped commands](#mistyped-commands),
    [Casso commands](#casso-commands), [the debug channel](#the-debug-channel),
    [MCP](#mcp)
13. [Colors and keys](#colors-and-keys): [themes](#themes),
    [color keys](#color-keys), [key schemes](#key-schemes)

## Ways in

| Way in | How | Best for |
|---|---|---|
| Batch | `CassoCli debug --machine Apple2e --script steps.txt` | repeatable scripts, tests, CI |
| Debugger window | **Debug > Debugger...**, or start Casso with `--debugger` | manual debugging beside the emulator |
| Debug channel | `CassoCli debug --attach <pid>`, or a separate client on its pipe | tools, editors, attaching to a running program |
| MCP server | `CassoCli mcp`, added to an MCP client's servers | AI assistants, with or without a shell |

The window and the channel share one session: a breakpoint set in either
appears in the other. The channel is documented in
[DebugChannel.md](DebugChannel.md).

### Opening the window

**Debug > Debugger...** in the emulator window opens the debugger beside the
emulator, as do F12, F7 (AppleWin's key) and Ctrl+F12. Ctrl+F12 is for running
Casso under a debugger that takes F12 as its break key. `--debugger` on Casso's
command line opens the window at start without moving the foreground or the
keyboard focus off the emulator window; the focus moves to the debugger when
you click in it. When the window opens, the keyboard focus goes to the pane
that had it when the window last closed, including a pane that opens a moment
later, such as a second disassembly view or a device panel, unless you have
clicked or typed in the window by then.

**Debug > Restart under debugger** in the emulator window power-cycles the
machine with the debugger open, so [the call stack](#the-call-stack) has every
call from power-on. With the debugger open, its **Machine > Power cycle** does
the same.

**Debug > Detach** closes the debugger and lets the machine run, resuming it
first if it is stopped. Closing the window closes the debug channel. The
breakpoints are restored when the window opens again.

### Attaching

`CassoCli debug --attach <pid>` runs a script against a running Casso, as
batch mode runs one against a new machine; `CassoCli debug --list` lists the
instances with a channel open. See
[DebugChannel.md](DebugChannel.md#from-the-command-line), and
[DebuggerForAgents.md](DebuggerForAgents.md) for scripts an AI agent runs.

### Batch mode

`CassoCli debug` builds a machine, stops it at power-on, runs the script's lines
and then each `--command` line, and exits after the last one. No step pauses
for input, so a batch run gets the same result every time; `--seed` fixes the
power-on memory pattern, so two runs match byte for byte.

```
CassoCli debug --machine Apple2e --script stop.txt
CassoCli debug --machine Apple2e --disk1 game.woz --command "bpmr C000" --command g --json
```

A script has one command per line. Lines starting with `;` are comments, and
blank lines are skipped except in the line assembler, where a blank line ends
it.

```
; Stop in COUT and list the stack above it.
BP COUT
G
STACK
```

- Every run has the `--max-cycles` budget (default 100,000,000), so a script
  whose breakpoint is never hit ends anyway.
- Guest disk writes go to an in-memory copy unless `--write-disks` is given.
- `--tape file` puts a WAV, AIFF or FLAC recording in the cassette recorder,
  stopped at its start; see [the cassette deck](#the-cassette-deck).
- `--json` prints one JSON record per reply and stop, in the format the debug
  channel uses.
- `--mode` sets the [command mode](#command-modes) the script starts in, and
  `--output` the [output format](#output-format).

| Exit status | Meaning |
|---|---|
| 0 | every command returned ok or not available |
| 1 | at least one command returned error or unknown |
| 2 | usage error, or the machine, a disk or a tape could not be loaded |
| 3 | the last run ended on its cycle budget |

The commands that act through the emulator, listed under
[controlling the machine](#controlling-the-machine), reply in a batch run that
they are not available.

## The window

The window displays the code around the PC, the registers and flags,
breakpoints, watches, the stack and memory, and has a command box and console
below the code.

Counts of 1,000 or more in the window are written with the locale's thousands
separator. The tip on **Undo** or **Redo** is the change it undoes or redoes,
as "Undo delete all breakpoints".

- **Step**, **Step over**, **Run**, **Run to cursor** and **Pause** run the
  machine. **Follow PC** returns the code pane to the PC. Run to cursor sends
  Casso's `G address` in Casso mode, whatever mode the console is in, since
  GSSquared's `g` doesn't take an address.
- Double-clicking a code line sets or clears its breakpoint. Double-clicking a
  breakpoint in the Breakpoints pane clears it.
- The Address box moves the memory pane. Type over the bytes, or use a write
  command at the console such as AppleWin's `MEB 0300 A9`, to write memory.
- The command box runs any command in the current mode, with the same reply
  batch mode prints.

AppleWin's window commands work in the command box: `.` returns to the PC,
`RET` goes to the return address on the stack, `^` and `V` move one
instruction, `PAGEUP` and `PAGEDN` a pane, and `MD1 addr` (or `MA1`, `MT1`)
moves the first memory window and `MD2`, `MA2` and `MT2` the second; the
address is an expression. `CODE`, `DATA` and `CONSOLE` bring the disassembly,
a memory window or the console forward, and `CODE2` and `DATA2` open the second
view. The screen views (`TEXT`, `HGR` and their forms) and the appearance
commands (`BW`, `COLOR`, `FONT`) are not available; the emulator window
displays the screen.

Ctrl+F opens a find bar over the pane with the keyboard focus, as Visual Studio
Code's does: the console, a source document, the disassembly, an event log, or
a list pane (Breakpoints, Watches, Stack, Call stack, Trace, BASIC program,
Symbols, Bookmarks, Data ranges). From any other pane it opens over the
console. The search button on the disassembly's bar opens the same find bar.
In a list it searches the text of the visible columns of the rows that pass the
filters, and selects the match's row. One pane has the bar at a time. The bar has the text to find, then toggles for
match case (Aa), match whole word (ab) and regular expression (.*), the match's
place among all of them ("2 of 5"), up and down arrows for the previous and
next match, and a close button. With no toggle on, the text is found as a plain
substring, ignoring case. Enter and F3 find the next match, Shift+Enter and
Shift+F3 the one before, and Escape closes the bar.

In a disassembly view, while the machine is stopped, the find searches every
line of the address space, not only the rows on screen: the address, the
bytes, the label and the instruction with its symbols, each while its column
is visible, and with source rows on, the source line above an instruction.
The operand and result column isn't searched, since its values are worked
out for the rows on screen alone. A match is brought into view and selected,
and Enter walks the matches round the end of memory. The lines' text is made
once the machine has been stopped for a moment with no step, so it costs
nothing while you step; until then, and while the machine runs, the find
searches the rows on screen.

### Menus

- **File**: Open source file..., Open symbol or debug file..., Unload symbol
  file (a row for each loaded symbol or debug file, which clears its table as
  `SYM<table> CLEAR` does), Load breakpoints..., Save breakpoints..., Save
  trace..., Load file into memory..., Save memory to file..., Load BASIC
  program..., Save BASIC program..., Open project..., Save project as...,
  Project settings... and Close.
- **Edit**: Copy, Select all, Find, Find next and Find previous.
- **Machine**: Reset and Power cycle.
- **View**: every pane, in sections divided by separators. The code first:
  the disassembly views and the source files, each a submenu, then Call stack,
  Breakpoints, Bookmarks and the symbol tables. Then the machine's state and
  memory: Registers, Stack, Watches, the memory windows' submenu, Data ranges
  and the heat map's panes. Then Console, Trace, Profiler and the **Logs**
  submenu, then the **Device panels** submenu, and last **Reset window
  layout**. Each pane's key is beside it; see [key schemes](#key-schemes).
- **Debug**: Run, Break and Detach; Step into, Step over, Step out, Run to
  cursor, Run one frame, Show beam on screen and Show next statement; Step back
  into, Step back over, Step back out, Reverse continue and Go live; Step by
  source line, Trace and Step filters....
- **Tools**: Keyboard scheme, Theme and Options....

What the colors mean is behind each pane's info button; see
[color keys](#color-keys). The debugger has no Help menu.

### The command bar

The command bar below the menu bar has the run and step buttons as icons, and
Trace. Each button's tip lists the command and its key in the scheme in force.
A command with two keys has both in its tip and menu row, for example
"Alt+F11 or Ctrl+R, F11".

Drag the bar's grab handle to move it along its edge or, pulled well away, to
float it in a separate window, icons alone. While it is dragged, the bar lifts
off its place, raised over a shadow. Dragged back over any edge of the window,
a floating bar snaps into that edge, which makes room for it, and slides along
it with the pointer until the button is released; a long bar also snaps into
the edge its far end is dragged up to. Its place is saved between sessions.

### The console

The console has a toolbar with **Dialect**, which sets the
[command mode](#command-modes) the console and the memory Address box read.

The command box completes as PowerShell does. Tab completes the command word
from the current mode's commands, and pressing it again steps through the
other matches; Shift+Tab steps back. The newest earlier line that starts with
what is typed appears in gray, and Right arrow at the end of the line takes it.
F8 steps back through the earlier lines that start with what is typed, and F7
lists the earlier lines to pick from, above the command line with the newest
at the bottom. A line too long for the list ends in an ellipsis and is drawn
whole while it is highlighted.

`CLS`, or `CLEAR`, clears the console's output, in every command mode and
with or without the mode's prefix, as does Ctrl+L in the command box or
**Clear** on the output's context menu. The command box and its earlier lines
are left as they are.

Every command that changes breakpoints, watches, memory, registers, flags,
symbols, disks or settings prints one line about what changed, such as
"Removed breakpoint 3 (exec C000)" or "Removed watch 0 (0400)".

An action taken in the window prints nothing in the console unless it fails,
or unless its reply is what it produces, as a search's matches are. **Show UI
actions as commands** on the console output's context menu prints each action
as the command that does the same, with its reply.

A file that a console line mentions, such as the file `SYM LOAD`, `BLOAD`,
`BSAVE` or `BPSAVE` read or wrote, is a link with the file's name in place of
its full path. Its tip has the full path, and a click on it opens a new File
Explorer window at the file's folder with the file selected. When a debug project
loads, the console has "Loaded the debug project for symbols-demo.dbg", where
"project" is a link to the `.cassoproj` file and the debug file's name is a
link to the debug file, or to the binary for a binary's project. The debug
channel and batch mode write the full paths, without links.

### The status bar

A status bar runs along the bottom of the window. From the right:

- **Text size**, as a percentage. Clicking it opens a slider from 50% to 300%
  in steps of 10%, the same size Ctrl+Plus, Ctrl+Minus and Ctrl+wheel set.
  A click anywhere else, or Escape, closes the slider; the arrow keys move it
  while it is open. The size is saved and restored the next time the
  debugger opens.
- **Speed**, the emulator's: 1x, 2x or Maximum. Clicking it opens a menu of
  the three, which sets the speed as the emulator's speed menu does.
- **History full**, the share of the reverse-execution memory budget the
  history uses, with a meter that is green while empty and blends smoothly to
  blue as it fills. A full budget is not a fault: the oldest snapshots make
  room for new ones. The text is "History off" while history recording is off.
- **Begins at**, the time of day the history begins at, in your regional time
  format, then in parentheses the emulated time since the machine started, for
  example `Begins at 10:42:07 PM (0.0 s)`. Once the budget is full the oldest
  snapshots are dropped to make room, so both times move forward as the
  machine runs.
- **Scanline:cycle**, where the video beam is, in decimal and then in hex
  as `VIDEOINFO` prints it, for example `192:1 ($0C0:01)`. Clicking it turns
  **Show beam on screen** on and off.
- At the left, **Replaying history** while a reverse command or a seek is
  replaying history to get to its point. Once a reverse run, a step back over
  or a step back out has searched for longer than about 200 ms, the note adds
  how much of the history it has covered. Escape or Pause stops the search,
  and the machine stops at the last position the search got to, with
  **Stopped** in the history band. The window responds throughout; commands
  typed meanwhile run once the replay ends.

### Panes and docking

Every pane can be moved. Drag a tab onto the drop zones that appear to dock it
beside another pane, against an edge of the window, or as a tab of another
group; drag a divider to resize. A tab dragged out of the window floats in a
separate window. Every pane, docked or floating, has a close button; the
**View** menu lists every pane and brings the chosen one back where it was,
docked or floating. Right-click a pane's tab or title bar, or press Shift+F10,
for **Dock to**, which offers each edge, each group, Float and Auto hide; a
right-click over a pane's content opens only the content's menu, if it has one; Alt+Shift with an
arrow key moves the focused pane. The pin on a pane's title bar hides it too.
An auto-hidden pane is a tab on the window's edge: click it and the pane
slides out over the others, click it again or anywhere else and it slides
back; the tab is marked when its pane has new output. A slid-out pane has a
title bar: its pin docks the pane back where it was, and dragging it docks the
pane wherever it is dropped. The arrangement, including floating windows and
the monitor each is on, is saved between sessions, and **View > Reset window
layout** puts back the default arrangement.

The panes are the disassembly, the source, the console, registers,
breakpoints, watches, the stack, the call stack, the trace, the heat map, up to
four memory windows, and one panel for each device the machine has. The View
menu also opens these, which stay closed until first opened:
[Symbols](#the-symbols-pane), [Bookmarks](#bookmarks),
[Profiler](#the-profiler-pane), [Data ranges](#data-marking),
[Heat map ranges](#ranges), [BASIC program](#applesoft-in-the-window) and
[the logs](#devices-and-logs).

### Registers and stack

Under the registers, **Cycles** is the CPU's count since power-on and **Trip**
the count since the trip count was last reset, by **Reset trip count** on
either row's menu or by `RCC`; the total is never reset. **Last branch** is
the last instruction that sent the PC elsewhere, a taken branch, a jump, a
call, a return or BRK, and where it went, as `$8759 → $8751`; its menu has
**Go to branch** and **Go to target**. While the machine is stopped, the stack
pane's menu has **Push byte...**, **Pop byte** and **Pop word**, which run
`PUSH`, `POP` and `PPOP`.

## Code views

### Disassembly views

Each disassembly view's toolbar has check boxes for its viewing options, as
Visual Studio's: Show address, Show code bytes, Show source code, Show symbol
names, Show line numbers and [Show heat](#heat-columns). Source and line
numbers are offered once a debug file maps the code. All start checked; the
choices apply to every view and are saved. Show source code's tip lists the
ways to get source while no source is available, and Show symbol names' tip lists where
the symbols came from. **Tools > Options...** has a Disassembly page with a
check box for every option (the ones above, Show operand values, Show pointers,
Show branch arrow and Show colon after address), **While source is visible, dim** (Source lines, Disassembly
lines or Neither), and a Preview. **View > Disassembly** opens another view.

- Hovering a disassembly view's breakpoint column draws a gray breakpoint on
  an instruction's row or the source line representing it, where a click sets
  one. The PC's yellow arrow is to the right of the breakpoint's dot, never
  over it.
- The Label column is fitted to the labels again whenever symbols or a debug
  file load, in place of a width it was dragged to or a project saved.
- With source rows, the view counts only the instructions whose rows are in
  view: a stop on one below them brings the view to put the PC in the middle.
- While paused, dragging the PC's yellow arrow to another instruction sets the
  PC there without running anything, as **Set next statement** on a row's
  context menu does.
- A drag selects text across the columns; a Ctrl+drag selects only in the
  column it began in.
- While the machine runs, the view following the PC isn't moved to the PC for
  five seconds after the wheel, the arrow keys or the scroll bar last scrolled
  it, or while its row menu is open, so the code around can be read. It then
  follows the PC again, with the PC in its middle. **Follow PC** brings it
  back at once.
- The tips over a symbol, a branch arrow or a heat cell in a code view or a
  source document open at once, without the hover delay.
- A code view's scroll bar covers the whole address space. The top of the
  puck is the first address in view and its length the addresses in view, so
  dragging it moves the view anywhere in memory; the arrows scroll an
  instruction and the track a view's worth.
- Right-click a symbol in a code view or a source document for **Go to
  definition**: the source line that defines it opens in its document, or,
  without a debug file that maps it, its address comes into view in the
  disassembly. A constant has no definition to go to.

### Source rows

With source displayed, each source line sits on a row above the code it
produced, and the row represents the first instruction under it: its
breakpoint, the PC's arrow and the branch arrow are on the source line, and a
click in its breakpoint column sets or clears the instruction's breakpoint. A
line that produced one instruction has the instruction's operand and result in
their column, before the line's comment. A macro or a C line has them on its
instructions' rows. A source line has the heat of the instructions it assembled
to; see [heat columns](#heat-columns).

### Branch arrows

The branch arrow at the PC is in the PC's yellow when the current flags take
the branch, and gray when they do not. Its tip is "This branch will be taken"
or "This branch won't be taken", or "jump" or "call" for a JMP or JSR, and
the row where the branch goes is in the accent color. A dotted arrow marks
the branch, jump or call the last step took; its tip is "This branch was
taken". A green row is the line brought into view by Go to, the call stack or
a branch arrow.

Click the PC's arrow to go to where it points, and the dotted arrow to go to
the branch taken by the last step, even where the two overlap; the next click
on either goes back to the PC. In a source document the click scrolls the
document rather than bringing up the disassembly.

An arrow whose PC row or target is scrolled out of view runs to that edge of
the view, in the disassembly and in a source document, so it can be followed
while any part of it crosses the view. Where the dotted arrow ends at the PC
and the PC's arrow starts there, the two lines reach the PC's row a little apart,
and when both run the same way from the row, the PC's is drawn a lane further
left. A label is cut off short of the arrows' lanes rather than drawn under
them.

### The row menu

A disassembly row's menu has these, each acting on the row, or on the selected
rows when it is among them:

- **Set next statement**, while paused.
- **Mark as bytes**, **words**, **addresses**, **text** or **floats** and
  **Clear data marking**; see [data marking](#data-marking).
- While the heat columns are on, on a row whose bytes were read or written and
  never run, **Mark $60C1-$60FF as data (read or written, never run)** marks
  the run of such bytes around the row as bytes, up to the first byte that ran
  or that nothing touched.
- **Add symbol here...**, which runs `SYM name = addr`.
- **Never step into this routine**, which runs `SKIP` on the routine a `JSR`
  row calls, or on the row's address; the call stack's menu has it for a
  frame's routine. See [the step filter](#the-step-filter).
- **Add bookmark** or **Remove bookmark**, which run `BMA` and `BMC`.
- **Start stopwatch here**, then **Stop stopwatch here** on another row,
  which runs `STOPWATCH start stop`; see [the stopwatch](#the-stopwatch).
- While the machine is stopped, **Call subroutine here**, which runs `JSR`;
  **Edit instruction...**, which assembles an instruction over the row's,
  padded with NOP when it is shorter and, when it is longer, written only
  after a prompt that lists the instructions it covers; and **Replace with
  NOPs**. Neither changes ROM, both write only over the bytes in the rows,
  and **Undo** and **Redo** on the menu put the bytes back and write them
  again.

### Data marking

**Mark as bytes**, **words**, **addresses**, **text** or **floats** and
**Clear data marking** on a row's menu run `DB`, `DW`, `DA`, `ASC`, `DF` and
`X` over the rows' bytes. A drag across rows selects them for these. A range
marked without a name is called after the symbol at its start, or after its
kind and address without a symbol there, and a symbol labels its line. The
commands and their forms are in [Debugger-Commands.md](Debugger-Commands.md).

The bytes a debug file maps to a data directive (`.byte`, `.word`, `.addr`,
`.asciiz`, `.res`, Merlin's `ASC`, `DA` and `HEX`, and the like) are
disassembled as data while the file matches memory: text for a string, words
for a word directive and bytes for the rest, each line under the label of its
symbol. After **Clear data marking**, or `X`, they are disassembled as code
until the file loads again.

A data line's directive is in italics, and the tip over it has the reason the
bytes are data: marked by a data directive, or mapped to one by the debug file.
A data line's text runs on across the operand column, and the zero byte or
other control characters that end a string follow its quotes as bytes:
`ASC "SYMBOLS DEMO",$00`.

**View > Data ranges** lists each range marked as data, with **Go to** and
**Delete**. A click on a heading sorts the list by that column, and a second
click reverses the order. The Kind heading's chevron opens a menu of the kinds
in the list, each checked while its ranges are listed.

### Bookmarks

**View > Bookmarks** lists each bookmark's number, address and symbol, with
**Go to** and **Delete**. A click on a heading sorts the list by that column,
and a second click reverses the order. Ctrl+K, K adds or removes a bookmark on the selected
disassembly line, and Ctrl+K, N and Ctrl+K, P go to the next and the previous
bookmark.

### Heat columns

**Show heat** on a disassembly view's bar adds a column after the margin,
filled from a scale for what touched the line's hottest byte most: red to
yellow for a byte run, blue to pale cyan for one read and purple to pink for
one written as data more than it is run, brighter the hotter. A byte nothing
touched is gray. The tip over a heat cell lists the run, read and write
counts of the line's bytes and draws the cell's scale with an arrow at its
level. A source line between the instructions has the heat of the
instructions it assembled to. The heading's tip has the scales' definitions.

With the option on, the source view fills a space for each byte, up to four,
ahead of each line's number, over the first range of bytes the line assembled
to, and the BASIC program pane has a **Heat** column that colors each line by
how often BASIC began it: hotter for more often, with no color for a line that
hasn't run. A line of several statements, split by colons, has its busiest
statement's color there, and where its statements differ in heat, each is also
tinted in place by how often it ran. Point at such a line and its tints fade
to the usual syntax colors; they come back when the pointer moves off. The tip
over a line or a statement has the count. **Heat** on a memory window's bar
tints each byte by its heat.

The columns follow [the heat map](#the-heat-map)'s fade or cumulative choice
and read the CPU's address space, and the heat is counted while any of them is
on, as it is while the heat map is visible.

## Breakpoints

Every kind of breakpoint is a row of the [Breakpoints pane](#the-breakpoints-pane)
and of `BPL`. `BPL`, `BPC`, `BPD`, `BPE`, `BPCHANGE`, `BPEDIT` and `BPSAVE`
treat every kind alike; their syntax is in
[Debugger-Commands.md](Debugger-Commands.md). Watches (`W`, `WL`, `WC`) are in
[watches](#watches), the Applesoft breakpoints in
[BASIC breakpoints and steps](#basic-breakpoints-and-steps), and the break on
a read of unwritten RAM in
[reads before written and changed values](#reads-before-written-and-changed-values).

### Execution breakpoints

`BP addr` (or `BPX`) stops before the instruction at an address runs; F9 or a
double-click on a code line sets or clears one. `BP file:line` sets a
breakpoint on the first instruction of a source line, at each place a macro
body line was expanded.

An execution breakpoint set by a symbol's name, as `BP COUT`, is a function
breakpoint: its Kind is "Function", `BPSAVE` writes it by name, and when a
symbol load puts the name at another address, the breakpoint moves there. One
set by address is "Breakpoint", a symbol at that address included.

### Data breakpoints

`BPM`, `BPMR` and `BPMW` stop on a read or write, a read, or a write of memory,
after the instruction that made the access unless `BEFORE` is given.
`BPMV addr[,len|:last] value` stops after a write sets a byte of the range to
`value`, and on no other write. `BPR IF expr` stops before any instruction at
which the expression is true, wherever the PC is; the breakpoint dialog's
Condition type sets it.

### Opcode and interrupt breakpoints

`BRK ON` stops on the `BRK` instruction, and `BRKOP op` on any opcode: a byte,
or a mnemonic for every opcode of it on the machine's CPU (`BRKOP JSR` is
`$20`; `BRKOP LDA` is each `LDA`). `BRKOP 00` and `BRKOP BRK` are `BRK ON`.
`BRK 1 ON`, `BRK 2 ON` and `BRK 3 ON` add an opcode breakpoint for each
undocumented opcode of that instruction length on the machine's CPU,
`BRK ALL ON` adds all of them plus `BRK`, and `BRK` alone reports which lengths
are on. The length is the one the CPU executes:

- **6502**: the stable undocumented opcodes (`SLO`, `LAX`, the multi-byte
  `NOP`s and the rest) take the length of their addressing mode, and the
  unstable ones (`ANE`, `SHX`, `LAS` and the rest) the length of their operand.
  Only the twelve `JAM` opcodes, which halt the CPU, are one byte.
- **65C02**: every opcode is defined, and the undocumented ones are `NOP`s: 32
  one-byte, eleven two-byte and three three-byte (`$5C`, `$DC`, `$FC`).

`BRKINT ON` stops on an interrupt.

### Beam breakpoints

`VIDEOINFO` reports where the beam is: the scanline (0 to 261) and the cycle
within it (0 to 64), in hex. The status bar has the same, in decimal and then
in hex. A scanline draws in its last 40 cycles; the 25 before them are
horizontal blank, and scanlines 192 to 261 are vertical blank.

| Command | Effect |
|---|---|
| `BPBEAM line cycle` | stops when the beam gets to that scanline and cycle |
| `BPBEAM VBL` | stops where vertical blank starts, scanline `C0` |
| `BPV line[,len]` | stops when the beam enters a scanline |
| `BPBEAM OFF`, `BPV OFF` | clear the beam position or scanline breakpoint |
| `FRAME [n]` | runs until the beam comes back to where it is, `n` times |

An instruction takes several cycles, so `BPBEAM` stops at the first instruction
at or past the place it was given. It fires once and then clears, as `BPV`
does, and `BPC *` clears it. The Breakpoints pane lists each as a row after the
table's breakpoints. `FRAME` stops early when anything else stops the machine
first, and its frame break goes with that stop.

**Debug > Run one frame**, or F6 in every key scheme, runs `FRAME` in Casso
mode whatever the console's mode. **Debug > Show beam on screen** marks the
beam on the emulator's screen while the machine is stopped: its scanline is a
red line across the picture, and the cycle it is on a bar across that line. A
beam in horizontal blank is marked at the left edge, and one in vertical blank
along the bottom edge.

The beam also determines what a read returns when nothing drives the data
bus. On the ][, ][+ and //e, a read of an unused I/O address or of a display switch ($C050 to $C05F)
returns the byte the video scanner fetched from main memory on that cycle, as
real hardware does, so a program that polls for a byte it placed in screen
memory (vapor lock) syncs to the correct scanline. In blanking the scanner
fetches as well, from the addresses the hardware's counters produce, which in
horizontal blank are the row's screen holes. On the //c a read returns the last
value on the bus.

### Disk breakpoints

`BPDISK` stops on what the Disk II controller does: a sector read, an address
field, a head move, a write, a DOS 3.3 sector, a ProDOS block, a call to RWTS
or to ProDOS's disk driver, the motor, a drive select, and a disk going in or
out. It is a Casso command, `/bpdisk` in Monitor mode, `bpdisk` in GSSquared
mode and `!bpdisk` in WinDbg mode. `D1` or `D2` before the event limits it to
one drive; numbers are hex, and `*` matches any.

| Example | Stops |
|---|---|
| `BPDISK READ 11 0` | after the instruction that reads the checksum nibble of track $11's physical sector 0 |
| `BPDISK READ 11 0 PASSED` | the same, and also on a data field the program's search passed over |
| `BPDISK ADDRESS 11 * FE BADSUM` | after an address field of track $11, volume $FE, whose checksum is wrong (`WRONGTRACK`: one whose track is not the head's) |
| `BPDISK HEAD T 11` | when the head arrives on track $11 (`QT 44` in quarter tracks, `HALF` on any half track, `BUMP` on a step against the end stop) |
| `BPDISK WRITE`, `BPDISK WRITE 11 0`, `BPDISK WRITE BLOCKED` | on entering write mode, on a data prologue written after track $11 sector 0's address field, and on a write a write-protected disk drops |
| `BPDISK DOS 11 5` | when DOS 3.3's logical sector 5 of track $11 is read, through the physical sector it is stored in |
| `BPDISK BLOCK 7` | when both halves of ProDOS block 7 are read on one visit to its track |
| `BPDISK RWTS 11 5 READ` | before RWTS's first instruction, when its IOB's request is that track, sector and command |
| `BPDISK DRIVER BLOCK 2 WRITE` | before the ProDOS driver's first instruction, for that block and command |
| `BPDISK MOTOR ON` | when the motor starts (`OFF` when it is switched off, `STOPPED` when it stops a second later) |
| `BPDISK SELECT` | when the drive selected changes (`EVERY` on each selection) |
| `BPDISK D2 INSERT`, `BPDISK EJECT` | when a disk goes into drive 2, or out of either |

Custom marks follow the event: `ADDR D5 AA ?? DATA D5 AA AD` matches fields with
those prologues in place of the standard ones, and `SECTORS 13` reads a 5-and-3
data field after custom address marks. `IF` adds a [condition](#conditions), as
on any breakpoint. The Breakpoints pane has "Disk event..." on its New menu.

**The stop.** A disk breakpoint stops at the instruction boundary after the
event, as a data breakpoint does; an RWTS or driver breakpoint stops before the
routine runs. The report has the event, the track, sector and volume of the
field, the address and data checksums as stored and as computed, the three
nibbles after the data field, the head's position, the motor, and the
addresses of the instructions that found the address prologue, the data
prologue and the checksum nibble:

```text
Disk breakpoint #3: sector read, track $11 (17) sector $00 volume $FE, drive 1, 16-sector
  address checksum $EF, expected $EF; data checksum $2C, expected $2C; epilogue DE AA EB
  head on track 17 (quarter track $44), motor on; read by $B925, address field found by $B94F, data field by $B8E1
```

A breakpoint or data breakpoint that stops at the same boundary reports its
reason, with the disk report after its line.

**Which reads count.** RWTS and the boot ROM search for a sector by reading
every field that passes, so most data fields go by unread. A data field counts
as read when the instruction that read its checksum nibble is not the one that
found its address field: the search loop that passes a field over reads both
in one place, and the read routine reads the data in another. `PASSED` counts
the fields passed over as well.

**Disk values.** Any expression (an `IF`, `CALC`, an address) can read
`DISK.DRIVE`, `DISK.QTRACK`, `DISK.QTRACK1`, `DISK.QTRACK2`, `DISK.MOTOR`,
`DISK.WRITING`, `DISK.PROTECTED`, `DISK.INSERTED`, and `DISK.ADDR.TRACK`,
`DISK.ADDR.SECTOR` and `DISK.ADDR.VOLUME`, the selected drive's last address
field with a good checksum ($FFFF until one is read). `DISK.QTRACK` is where
the head is; `DISK.ADDR.TRACK` is the track written on the disk, which a
copy-protected disk can make differ. `DISK.ATRACK`, `DISK.ASECTOR` and
`DISK.AVOLUME` are older names for the address field's values. They come from the
controller, never through the bus, so reading them changes nothing, and they
take precedence over a program symbol of the same name. On a machine with no
Disk II they are unknown.

```text
BPDISK MOTOR ON IF DISK.QTRACK == 0
CALC DISK.ADDR.TRACK
```

**RWTS and driver breakpoints follow their vectors.** DOS 3.3 can be loaded at
other addresses, and a program can patch RWTS or install a replacement, so
`BPDISK RWTS` stops at the routine the `JMP` at $03D9 leads to, and
`BPDISK DRIVER` at the routine slot 6's device vector points to ($BF1C for
drive 1, $BF2C for drive 2) once ProDOS's global page begins with its `JMP`.
The routine is looked up again whenever a vector's bytes change. Until the
vector has a target the breakpoint is unresolved, which `BPL` and the pane's
Address column indicate, and it stops on nothing. A call that does not go
through the vector, as DOS's calls through $B7B5 do, stops at the routine too.

**The head.** Casso's stepper moves the head in half-track steps, so a head
breakpoint matches whole and half tracks only; a `QT` range matches whatever
positions in it the head arrives at.

**Reverse continue** stops on disk breakpoints of every kind, at the boundary a
forward run stops at, including a sector whose reading began before the
snapshot the replay starts from. Replays count no hits.

**Not yet.** Three kinds are deferred until the disk inspector's preservation
work:

- **A nibble sequence with `??` wildcards.** What a protection check reads
  depends on where the latch frames the bits, which the inspector displays and
  searches; built here, it would be a second matcher that ignores framing.
- **A field passing under the head with no CPU read.** It depends on where each
  field lies on the track, which the inspector's analyzer finds.
- **A latch read during the motor's spin-up.** The spin-up window is a
  constant taken from AppleWin's model, while real cards vary up to about three
  times as long, so the breakpoint would stop on an approximation.

The [Disk II log](#disk-ii) lists the same events as they happen, whether or
not a breakpoint is set.

### Conditions

`BP`, `BPX`, `BPM`, `BPMR`, `BPMW`, `BPMV`, `BP file:line`, `BRKOP`, `BRK ON`,
`BRKINT ON`, `BPDISK`, `BPBASIC`, `BPBASICVAR` and `BRKBASIC ON` take a
trailing `IF` and an expression over registers, flags, symbols and memory,
where `*addr` reads a byte:

```text
BP Loop IF X == 3 & *PTR != 0
BPMW 400 BEFORE IF Y = 2
```

The expression is evaluated only when the address or the access hits. A false
one neither stops the machine nor counts a hit; a true one stops, and the stop
line has the expression and its value. On a data breakpoint, `ACCESS` and
`VALUE` are the address and byte of the access. An expression is checked as it
is set, so an unknown symbol or a read of an I/O address is an error then, not
at the first hit.

An expression has `+ - * / %`, `& | ^ ~`, `! && ||`, the comparisons `== != <
> <= >=`, and parentheses. Numbers are hex; `0n10` is decimal. A register is
`A`, `X`, `Y`, `P`, `S` or `PC`, and a symbol is its address; `*addr` is the
byte at an address, and `<` and `>` before a value are its low and high byte.
After a value, `.b` is the byte at that address and `.w` the word there, as in
a watch, so `count.b == 5` compares the byte at `count`. A watch also takes
`.s` for the text there; an expression doesn't, since text isn't a number.
`P.C`, `P.Z`, `P.I`, `P.D`, `P.B`, `P.V` and `P.N` are the flags, each 0 or 1:

```text
BPR IF P.C && A >= 80
BP COUT IF *FB == 0
```

### Breakpoint name

After `BPNAME 3 "Loop top"`, breakpoint or data breakpoint #3 has that name, and
`BPL`, the Breakpoints pane's Name column and the channel use it in place of
the default; `BPNAME 3` with no name returns it to the default. In the pane,
F2, a double-click on the Name cell or Rename on the row's menu edits the name
in place, and Ctrl+Z undoes it as it undoes the pane's other changes. The name
survives `BPEDIT`. `BPSAVE` doesn't write the name, so its script is one
AppleWin reads.

### Breakpoint logging

After `BPLOG 3 "A={A} count={count.b}"`, each hit of breakpoint #3 writes a
line to the console, with each expression in braces evaluated at the hit, as
`Breakpoint #3: A=$42 count=$07`, and then stops. `BPLOG 3 GO "..."` writes the
line and runs on, as Visual Studio's Continue execution does, and `BPLOG 3
CLEAR` removes the message. The Breakpoints pane's When hit column lists the
message, and the debug project saves it. The breakpoint dialog sets the message and Continue
after logging, which is `GO`.

In `CassoCli debug`, each message is printed after the line whose run wrote it,
before the stop; with `--json`, as `{"type":"log","text":"Breakpoint #3: A=$42"}`.

`BPLOGS [n]` lists the last n messages, 20 by default, of the last 1000
written. A debug channel client whose hello sets `"logs":true` receives each
one as a `log` notification ([DebugChannel.md](DebugChannel.md)); the MCP
server's `breakpoint_messages` tool lists them with `BPLOGS`.

### The Breakpoints pane

The Breakpoints pane's **New** drop-down lists Execution's types (Address,
Source line, Condition, Opcode, BRK, BRK on interrupt) and the Memory, BASIC and Hardware
submenus. **Delete all** opens a confirmation with Delete and Cancel first. A
type with fields opens the breakpoint dialog on it, sized to those fields;
BRK, BRK on interrupt, Reading uninitialized memory and BASIC errors have none,
so they are set or cleared at once and checked while on, and **Edit...** on
the row adds a condition. Every type that is a row of the table takes a
condition, and the info tip after the Condition label lists what it can use,
with examples; the tip stays open while the pointer is over it or its tip, or
while it has the focus. **Source line** takes a file of the loaded debug file
and a line number in it, and is offered only while a debug file is loaded.
Every type has **Log message**, which sets `BPLOG` (see [breakpoint logging](#breakpoint-logging)),
and **Don't break**: a hit is counted, and logged when there is a message,
and the machine runs on. The Breakpoints pane marks such a breakpoint's hit
count "(don't break)".

A field is checked when the focus moves off it, and OK checks the whole
breakpoint first, against the debugger's symbols and breakpoints: a definition
that does not parse, a number out of range, an unknown name in the condition,
or an execution breakpoint already set is reported under the fields, and the
dialog stays open. The Address field takes an address, a symbol or a range, as
`300:3FF` or `300,100`; Memory's Address has Read and Write on one line, and
Value takes a range. A Disk event has a field for each of its arguments, the
volume in decimal, and the address and data prologues and the 13-sector format
for the events that read or write a field; the info tips after Disk event and
Stops at list the events and the positions.

Reading uninitialized memory, a video line and a beam position are rows of the
pane after the table's breakpoints; unchecking or removing one runs
`BRKUNINIT OFF`, `BPV OFF` or `BPBEAM OFF`, and a read before written is listed
as counting from the next power cycle until the heat map tracks writes.

The Kind, Trigger and File headings each have a chevron that opens a menu of
the column's values, each checked while its breakpoints are listed. Clearing a
check hides those rows at once and the menu stays open, as File Explorer's
does. A filtered heading has a funnel, and **Clear filter** on its menu lists
every row again. Hiding a column clears its filter. The filters last until the
debugger window closes.

### Breakpoint groups

As in Visual Studio, breakpoints can be put in groups, each with a name.
A breakpoint or a data breakpoint is in at most one group, and a group
exists while something is in it. A group's name matches without regard to
case and is displayed as first typed.

While any group exists, the Breakpoints pane lists the breakpoints in no group
first, then each group under a heading with its name and how many breakpoints
it has; a click on a heading collapses or expands it. A row's **Add to
breakpoint group** submenu has each group and **New group...**, and acts on
every selected breakpoint; **Remove from group** takes them out. A heading's
menu has **Enable all**, **Disable all**, **Rename group...**, **Ungroup**,
after which the breakpoints are in no group, and **Delete group and its
breakpoints**, after a confirmation. Each is one step on the pane's undo list.

| Command | Effect |
|---|---|
| `BPGROUP` | List each group and its breakpoint ids |
| `BPGROUP name #[,#...]` | Put breakpoints in a group, making it; a name with spaces is quoted |
| `BPGROUP ! #[,#...]` | Take breakpoints out of their groups |
| `BPGROUP ENABLE name`, `BPGROUP DISABLE name` | Enable or disable every breakpoint in a group |
| `BPGROUP RENAME old new` | Rename a group |
| `BPGROUP UNGROUP name` | Take every breakpoint out of a group |
| `BPGROUP DELETE name` | Delete a group and its breakpoints |

The debug project saves each breakpoint's group, and a breakpoint is in the
same group after `BPEDIT`. `BPSAVE` writes AppleWin's script, which has no
groups.

## Stepping and history

### Running and stopping

| Command | Effect |
|---|---|
| `G` | runs until something stops it |
| `G addr` | runs until the PC gets to `addr` |
| `addrG` | sets the PC to `addr`, then runs |
| `T [n]` | steps into `n` instructions (default 1) |
| `P [n]` | steps over `n` instructions, running a `JSR` to its return |
| `RTS [n]` | steps out of `n` subroutines |
| `FRAME [n]` | runs `n` video frames (default 1), 17,030 cycles each |

A stop is reported with its reason and address, for example
`Breakpoint #0 at $FDED` or `Step at $0302`. In the Monitor output format a
stop prints the register line instead. The whole list, with `PAUSE` and
`BUDGET`, is in [Debugger-Commands.md](Debugger-Commands.md); the window's
buttons and keys run the same commands (see [key schemes](#key-schemes)).

In Monitor mode, `addrG` first pushes the Monitor's return address ($FF69), as
the Monitor's `G` does, so a program that ends in `RTS` comes back and stops
there as a run to, rather than running on into the ROM. That stop stays armed
until the program gets to it, through any breakpoint or step along the way;
another Monitor `addrG`, a reset or a machine change replaces or clears it.

In Monitor mode, `addrS` steps one instruction from `addr`, and a bare `S`
steps one instruction from the current PC, as the Monitor does after the
first `S`.

`SRC ON` makes every step command step by source line; see
[source documents](#source-documents).

### Controlling the machine

These do what the menu command of the same purpose does, so a reset or a disk
change is in history as the menu's is. They act through the emulator, so a
batch run replies that they are not available.

| Command | Effect |
|---|---|
| `RESET`, `RESET POWER` | resets the machine as its reset key does, or power cycles it |
| `PASTE "text"` | types the text as Edit > Paste does; `\n` is a return, and `\t`, `\"` and `\\` are a tab, a quote and a backslash |
| `DISK INSERT [D1\|D2] file`, `DISK EJECT [D1\|D2]` | inserts or ejects a disk, drive 1 by default |
| `SCREEN` | lists the text screen as displayed, 40 or 80 columns |
| `SCREENSHOT [file]` | saves the machine's picture as a PNG file, by default `casso-screenshot-` and the date and time in the current directory |
| `EXIT` | closes Casso, as File > Exit does |

A paused machine doesn't read the keyboard, so the text `PASTE` types stays in
the paste buffer until the machine runs again.

### The step filter

`SKIP name`, `SKIP addr` or `SKIP first.last` adds a routine to the step
filter: a step into its `JSR` runs the call and stops after it, so stepping
never lands inside `COUT` and its like. `SKIP` lists the filter, `SKIP - name`
removes an entry and `SKIP CLEAR` empties it. **Debug > Step filters...** lists
the routines a step goes over, each a symbol, an address or a range as `SKIP`
takes it; one is added on the last row and deleted with Delete or its row's
menu. **Never step into this routine** on a disassembly row's menu or the call
stack's menu adds one.

An interrupt taken during a step over or a step out runs to its `RTI`
without ending the step, which then goes on from where the interrupt came
in: `P` on a `JSR` with an interrupt pending runs the handler, then the call,
and stops after it. A breakpoint in the handler stops there. A step into stops
on the handler's first instruction.

A step out ends when the innermost call on record returns. With call
recording on but no call on record, or with the PC moved since the stop, the
calls on record were made by other code, so the step out ends at the first
`RTS` or `RTI` that raises the stack above where it began. The console and
the status bar have a line about it.

### Stepping back

While history recording is on (see [history options](#history-options)), the
machine can run backward through the history:

| Debug menu | Command | Effect |
|---|---|---|
| Step back into | `T-` | steps back one instruction |
| Step back over | `P-` | steps back over a whole subroutine call as one step |
| Step back out | `GU-` | steps back out to the `JSR` that called the current subroutine |
| Reverse continue | `G-` | runs backward through the history to the latest breakpoint |
| Go live | `LIVE` | returns to where the machine was running |
| Seek | `SEEK -5s`, `SEEK CYCLE n`, `SEEK POS n` | moves to a time back from live (`s` or `ms`), a cycle or a position, as a click on the history band does |

Each mode's forms are in [Debugger-Commands.md](Debugger-Commands.md), and the
keys in [key schemes](#key-schemes): in every scheme, a reverse command's key
is its forward command's key with Alt added. While a reverse command replays,
the status bar has **Replaying history**; see [the status bar](#the-status-bar).
A change made in the past makes the machine live there.

Reverse continue stops on [disk breakpoints](#disk-breakpoints) and
[BASIC breakpoints](#basic-breakpoints-and-steps) where a forward run does, and
doesn't stop on a [read before written](#reads-before-written-and-changed-values).
[The heat map](#the-heat-map), [the call stack](#the-call-stack), the logs and
[the instruction trace](#the-instruction-trace) follow the machine through
history.

Keys and mouse buttons released while the machine is behind live are released
in the machine when it goes live again; a key pressed down at that point stays
down.

### History options

**Tools > Options...** has a History page. **Record history for stepping
back**, on by default, turns history recording on and off, and **Memory for
history** sets its budget, 64 MB by default. History has a keyframe every ten
frames of machine time by default. Once the budget is full, the oldest part of
the history is dropped, and a stretch run at Maximum speed adds nothing to
history.

### The history timeline

Above the command bar, the history timeline displays small pictures of the
screen at points in the history: the oldest at the left, the live end at the
right, and evenly spaced points between, as many as fit across the window.
Each picture is the screen at a snapshot of history, so the point it marks is
exactly where a click goes. Pictures are drawn in the background, a few a
second and only while the timeline is visible, so an empty cell fills in
shortly; while the machine runs, the points stay put until the live end has
moved on by a whole point. Resting the pointer on a picture displays it full
size beside the timeline. Clicking one moves the machine to that point in
history, stopping the machine first if it is running, and the machine stays
stopped there; clicking the live end goes live, also stopped. The timeline
docks and floats, and its place is restored, as the command bar's is.
Floating, it is as long as it was docked, and dragging either end resizes it.

### The network in history

History covers the Uthernet II in use. What the network sends the card is
stored in history as it arrives, and a step back or a seek replays it from
there. A replay sends nothing to the network and receives nothing from it.
A change made in the past makes the machine live there. Each connection
that matches a socket the program has open at that point stays open, so a
session goes on without connecting again. A connection the other end has
closed since reads as closed by it, and a socket whose connection is gone
reads as closed by the remote end. A connection the program has no socket
for at that point is closed. What the network sent between that point and
where the machine had run to is delivered again, ahead of anything new, and
the same goes for a serial port. This includes a program that runs a TCP/IP
stack on socket 0 in MACRAW mode. What the program sent in that time was
delivered to the other end, so the other end can get it twice. Loading a state
file closes every connection.

### The call stack

`CALLS` lists the chain of calls to the PC, innermost first. Each frame has
its call site, the routine it entered, and how it was found:

- **Recorded** frames come from watching `JSR`, `BRK` and interrupts go by
  while the debugger is open, and `RTS` and `RTI` end them.
- **Guessed** frames come from walking the stack page for return addresses
  whose `JSR` is two bytes below; with a debug file loaded, only calls to
  known routines count.

`CALLS MODE RECORDED|WALK|HYBRID` chooses the mechanism. Hybrid, the default,
uses those frames and extends below them with the walk.

A program that manages the stack itself breaks the chain, and the call stack
marks where rather than guessing past it: a `TXS`, a pull into a return
address, a frame left by a jump, a return to somewhere other than its call
site, the stack wrapping, a reset, and the point where recording began or
where history starts. A routine that returns a few bytes past its call site,
over inline parameters the way ProDOS's MLI does, is noted and not treated as
a break. A store into a return address marks its frame when it runs, with the
store's address. In the window, the call stack pane lists the active `GOSUB`
returns and `FOR` loops first while an Applesoft program runs; see
[Applesoft in the window](#applesoft-in-the-window).

Recording starts when the debugger opens. While history recording is on, the
calls made before then are rebuilt from history right away: history is
replayed on a second machine, so the running machine, its sound and its
picture are left alone, and the record from the replay takes over from the one
begun when the debugger opened. Until then the call stack displays "Rebuilding
earlier calls from history" and how far along it is. This first rebuild takes
roughly half a second to two seconds for each minute of machine time it
replays, depending on what the program was doing, and it replays nothing
before the last gap, change of disk or power cycle in history, since the
record starts again there anyway.

Every move through history rebuilds the record for wherever the machine
lands, and that is quick: a copy of the record is saved at each of history's
keyframes, so the rebuild replays at most from the nearest keyframe at or
before the landing. A copy takes a few dozen bytes for a program a few calls
deep, and the copies may use up to an eighth of history's budget (8 MB at the
default 64 MB); past that the oldest go, and every copy goes with its
keyframe. A reverse command after which the machine is where it was, such as a
step back out with no caller, doesn't rebuild the record.

When history goes back to power-on, the chain ends at power-on, or at the
last reset or power cycle since, just as it does when the debugger was open
for them, and nothing is guessed below it. History may not go back that far:
its oldest part is dropped to stay within its budget, a stretch run at
Maximum speed adds nothing to history, and a stretch with another disk in a drive
cannot be replayed over the disk there now. The chain then ends where the
history it could replay starts, with the note "Calls before history starts
at $xxxx are not available.", and the walk guesses below. When no rebuild
can follow a move, because history was turned off, could not get to where
the machine landed, or the rebuild failed, the record has the calls since
the landing and the note is "Calls before $xxxx are not available."
instead. Without history, calls made before the debugger opened come from
the walk; to record everything, start Casso with `--debugger` or choose
**Debug > Restart under debugger** in the emulator window, which power-cycles
the machine with the debugger open.

## Symbols and source

[Tutorial 1](Debugger-Tutorial.md#1-a-first-stop-on-the-symbols-demo) and
[tutorial 2](Debugger-Tutorial.md#2-source-level-debugging-an-as65-program)
walk through the symbol tips and the source pane on the symbols demo.

### Symbol tables

A symbol table maps addresses to symbols for the disassembly and for
expressions. Casso
builds the Monitor, Applesoft, DOS 3.3 and ProDOS tables in, adding the
//e and //c memory soft switches on those machines.

```
SYM LOAD "game.dbg"       load into the User table
SYMUSER2 LOAD "lib.sym",4000   load another table, with a hex offset
SYM COUT                  look a name up in every enabled table
SYM FDED                  look an address up
SYM                       count the symbols in each table
```

`SYM` alone acts on the User table for `LOAD`, `SAVE`, `CLEAR`, `ON`, `OFF`,
`name = addr` (add) and `! name` (remove). Each other table has a separate
command: `SYMMAIN`, `SYMBASIC`, `SYMUSER2`, `SYMSRC` and so on. Loading
replaces the table's contents, so a second debug file loaded with `SYM LOAD`
replaces the first; `SYMUSER2 LOAD` loads another file's symbols beside it. **File > Open symbol or debug file...**, or a
file dropped on the window, loads a file as `SYM LOAD` does, and **File >
Unload symbol or debug file** clears one. `SRC CLEAR` unloads the debug file
and the symbols it loaded.

A symbol file may be in any of these formats, recognized from its contents:

| Format | Lines look like |
|---|---|
| Casso debug file (`CassoCli merlin -g`, `CassoCli as65 -g`) | `COUT=$FDED` |
| Merlin assembly listing (`CassoCli merlin -l`, or Merlin itself) | the symbol table after `SYMBOL TABLE` |
| AppleWin `.SYM` | `FDED COUT` |
| VICE label file | `al FDED .COUT` |

Lines starting with `;` are ignored.

An equate (`WIDTH = 40`) loads as a constant rather than a label. A constant
works in expressions like any symbol, and `SYM` and `SYMLIST` mark it
`constant`, but it never replaces a number in the disassembly: `LDA #$28` and
`LDA $28` are displayed as they are, and an address never looks up to a
constant. A load offset moves labels and doesn't move constants. A cc65
debug file marks its equates (`type=equ`), and `SYM SAVE` writes constants
under a `; constants` heading so they load back as constants. AppleWin,
VICE and Merlin symbol tables do not distinguish an equate from a label, so
everything in them loads as a label.

### Symbol tips

The registers of the Super Serial Card, the Uthernet II and the //c's serial
ports have no symbols. Hover over an operand at one of them in the
disassembly for a tip with the register's description, under the card's name
and slot: "Super Serial Card, slot 2: 6551 status register".

The tip over a symbol in the disassembly, and the Profiler's tips on a
routine, have the symbol's description and, for a label in a loaded program's
source, the comment on the line that defines it: the first line whose code
starts at the label, or the label alone on the line above it.

The tip over a word of a source line, in a source document or on a source row
of the disassembly, has the symbol it is and its value, and after `<` or `>`
the byte that operator takes. Over `lda #>count_text` the tip is
`count_text = $60F2` and `#>count_text = #$60, its high byte`, which is why
the instruction under the line is `LDA #$60`.

### Symbols for a program that isn't in memory

After a reboot with another disk, the program a symbol or debug file was made
for may be gone from memory. At each stop, the debugger checks each loaded
file against memory:

- A debug file is checked against its source. On each instruction line, the
  opcode in memory must be the line's mnemonic, and its length the line's.
  A Merlin listing is checked against the bytes on its lines.
- A symbol file without lines (Casso's, AppleWin's, VICE's) is checked
  against the bytes at its labels in RAM, from $0200 to $BFFF. Those bytes
  are read at the first stop with the PC among its labels, so a file loaded
  before its program is matched to the program once it runs. Loading the
  file again reads them again.

A file matches while at least a quarter of its lines or labels do, so a
variable or a patched instruction doesn't count against it. While a file
doesn't match, its labels are hidden in the disassembly. A debug file's
source lines are hidden between the instructions too, its data is
disassembled as instructions, its open source documents aren't mapped to the
code, and steps go by instruction. A banner over each disassembly view, and
over each of the file's source documents, has the file's name. All of it comes
back at the first stop where the program is in memory again. Symbols resolve
by name in expressions throughout, and equates are never checked.

### Debug files

A debug file maps addresses to source lines. Casso reads cc65's debug format
(`ca65 -g`, `ld65 --dbgfile`), writes it from its assembler
(`CassoCli as65 -g`; see [Assembler.md](Assembler.md#debug-files)), and reads a
Merlin 8/16 listing, which becomes a source view.

- `SYM LOAD file.dbg` loads the symbols and the line table. The window opens a
  source pane beside the disassembly and follows the PC. With source displayed
  in the disassembly, source lines are [rows of the disassembly](#source-rows).
- At a stop, the source document of the PC's file comes to the front, except
  over the pane you stepped from: after a step from a disassembly view that
  shares the document's tab group, the disassembly stays in front. A step from a
  source document to an instruction with no source line brings the
  disassembly to the front.
- The bytes a debug file maps to a data directive are disassembled as data;
  see [data marking](#data-marking).
- Dropping a debug or symbol file (`.dbg`, `.sym`, `.lbl`, `.vs`) on the
  debugger window, or on one of its floating panes, loads it as `SYM LOAD`
  would. A dropped source (`.a65`, `.s`, `.asm`, `.inc` and the like, Merlin's
  `.S` among them) opens in a separate document, as **File > Open source
  file...** does, with or without a debug file loaded. Any other file loads as
  symbols when it reads as a symbol file; otherwise it is displayed as text, or
  as a hex dump when it is binary.
- Opening a source with no debug file loaded first loads a `.dbg` or `.sym`
  of the same name beside it. A source with no symbols has a banner with a
  Load symbols button, which picks the debug or symbol file to load; the
  source is matched against it once it loads.
- The source files are looked for beside the debug file, then in the folders
  where sources were found before. When one is not found, the pane reports it;
  drag the file onto the debugger and it is matched by its hash, and its
  folder is saved for next time. A file whose hash differs from the one in the
  debug file opens with a warning.

### Source documents

- The source pane has an instruction line's operand and `Result: ` on the
  same row after the code, starting 40 columns in, or after a longer line's
  end. The operand is in the comment color, and the result in the theme's
  result color: cyan, amber in Casso Retro Terminal, and maroon on a light
  theme such as System light. Lines don't wrap: a bar along the bottom, or
  Shift with the wheel, scrolls the text sideways.
- `SRC` reports the current file and line. `SRC ON` makes every step command,
  in every mode, step by source line; `SRC OFF` steps by instruction, as does
  unchecking **Debug > Step by source line**. In the window, the pane with the
  focus determines it: the source pane steps by line, the disassembly by
  instruction.
- `BP file:line` sets a breakpoint on a source line; see
  [execution breakpoints](#execution-breakpoints).
- Source is colored with the grammar of the assembler that wrote it: as65,
  Merlin or ca65, chosen from the file's directives, labels and origin. Text
  with none of them, or as many of one as another, is read as as65's; the
  file's extension doesn't affect the choice among them. The Syntax entry on a
  document's tab menu sets it for that document.
- A `.c` or `.h` file is colored as C: keywords in the mnemonic color, types
  and preprocessor lines in the directive color, and numbers, strings,
  characters and comments in their colors. A block comment runs over as many
  lines as it spans.
- A Merlin or ca65 listing displayed as source has its address and byte
  columns, colored as an address and bytes, ahead of its colored source.
- With heat on, the source view has the heat of each line's bytes; see
  [heat columns](#heat-columns).

### The Symbols pane

**View > Symbols** lists each symbol table under a heading, the name of the
file loaded into it, with its count and a check box that runs `SYM<table> ON`
or `OFF`, and its symbols under it. The tip over the file's name has its full
path. A symbol's menu has **Go to** and **Remove**, and any row's has **Clear
table** and **Save table...**; Find searches the symbols. AppleWin's spare
tables, USER2, SRC and SRC2, are listed only once `SYMUSER2`, `SYMSRC` or
`SYMSRC2` has loaded something into one.

## Memory, watches and heat

### Memory windows

Each memory window's bar has Undo and Redo, the **Address** box, **Heat** and
**Refresh**. The Address box takes an address, a register, a symbol or an
expression, and its drop-down lists the addresses entered before. **Heat**
tints each byte with its heat map color. **Refresh** reads the window's bytes
again. The window's menu sets **Columns**, the values in a row (Auto, 1, 2, 4,
8 or 16; Auto, the default, fits as many as the window's width allows), and
**Group by**, each value as a byte, a word or a long. **View > Memory** opens
another window, up to four; a window closes from its tab. What does not fit a
narrow pane moves into the bar's **...** menu.

Type over the hex or the text to edit memory, in RAM or in ROM. A hex digit,
Enter or a double-click on a value opens a box over it for the new value;
Enter writes it, and Escape closes the box with the value unwritten. Ctrl+Z
undoes the last edit in that window and Ctrl+Y redoes it. Neither writes over
a byte that has changed since the edit; that byte's address appears in the
console instead.
The tip over a byte has its symbol, its value, its character and the region
of memory it is in.
On a byte's menu, **Go to $2000, the word here** goes to the address that
byte and the next one make, low byte first, as a pointer there reads.

Ctrl+F or **Edit > Find** opens the find bar over the window, which searches
all of memory: hex bytes, as `A9 00` or `$A9 $00`, or else text, matched by
each byte's low seven bits so normal and high-bit characters both match, or a
regular expression. **Find...** on the window's menu searches all of memory
as `S` does: hex bytes, a word, text in quotes (`'text'` with the high
bit set), `??` for any byte and `?n` or `n?` for half of one. The window goes
to the first match and selects it, and F3 and Shift+F3 walk the matches, round
to the other end. On a selection, while the machine is stopped, **Fill
selection...** fills it with the bytes typed, repeated, as `F` does, and
**Copy selection to...** copies it to the address typed, as `M` does.
**Compare with...** compares the selection with the range at the address
typed, as `MC` does: the bytes that differ are drawn in the result color, and
each one's tip has the other range's value. **View the other range's values**
draws those values in their place, and **Clear comparison** ends it.

A window reads the bytes around its rows as it scrolls. Until a read gets to
rows scrolled into view, they display what an earlier read left there, in the
same colors; rows that no read has gotten to yet are zeros in a muted color, with
the tip "not read yet". Addresses are always the 64K's four digits, wherever
Go to starts the rows.

Each region of memory a window displays that is not main RAM has a box drawn
around its bytes: ROM, each slot's ROM (labeled with its slot, such as Slot 6
ROM), a card's expansion ROM, I/O, the Language Card's two banks and aux RAM,
as the machine's memory map was when the window was read. The box follows
the values: where a region starts or ends partway along a row, its outline
steps around the bytes rather than taking in the whole row. Its label sits on
the box's top edge, centered and breaking the line; where that edge is too
short for it, as when a region starts near a row's end, the label moves down
to the edge along the next row. Rows move apart only where an edge runs
between them, so no line or label covers a byte. ROM and I/O boxes are drawn
in their bytes' colors, and the Language Card's and aux RAM's in the memory
map's. While the boxes are drawn, the gaps before and after the values are a
character wider, so a box's edge doesn't crowd the addresses or the text.

Right-click a byte for **Show last writer**, **Show last reader**, **Rewind to last
write time** and **Rewind to last read time**; see
[last writer and last reader](#last-writer-and-last-reader).
**Add as pointer watch** over a byte in zero page runs `ZP`.

### Loading and saving binaries

```
BLOAD file[,format] [addr[,len]]
BSAVE file addr.last
```

`BLOAD` detects AppleSingle, Intel HEX and Motorola S-record files from their
contents; anything else loads as raw bytes. A DOS 3.3 binary cannot be
distinguished from raw data, so it requires `,DOS`. The file extension is never used.

The reply has the byte count and the address, such as `Loaded 17 bytes at
$0300`. A load that would stop short loads nothing and is an error: a file
smaller than the length you pass, or one that would run past `$FFFF`. When a
debug file (`.dbg`) or, failing that, a symbol file (`.sym`) sits beside the
binary under the same base name, `BLOAD` loads it too, as `SYM LOAD` would,
and reports it in one more line.

Every file command's reply includes the file's path only when you typed it
relative, and then as the absolute path it resolved to.

| Format word | Format | Address |
|---|---|---|
| `RAW` | raw bytes | required |
| `DOS` | DOS 3.3 binary (4-byte address and length header) | from the header, unless you pass one |
| `AS` | AppleSingle | from the ProDOS aux type, unless you pass one |
| `HEX` | Intel HEX | from the file; passing one is an error |
| `SREC` | Motorola S-record | from the file; passing one is an error |

`BSAVE` and the Monitor's `W` write raw bytes. The Monitor's `R` reads raw
bytes: `300.3FFR "data.bin"`. In batch and on the channel, `R` and `W` require
a file name; the debugger window prompts for one when it is missing.

In the debugger window, **File > Load file into memory...**, or its button at
the end of the toolbar, runs `BLOAD` of a file you pick, with the format,
address and length typed in a dialog after it; it is off while the machine
runs. **File > Save memory to file...** runs `BSAVE` of the address and length
typed. The dialogs open with the fields typed last time, and the console has
the command each one ran and its reply.

### Watches

`W addr` adds a watch, `WL` lists the watches and `WC` clears one; the rest are
in [Debugger-Commands.md](Debugger-Commands.md). The console's `W` is
AppleWin's address watch, and `W EXPR text` is the typed form.

The Watches pane has an ID column: **Auto** for what the instruction at the
PC and the one just run touch, the watch's number for a watch, and ZPn for a
zero-page pointer. What is typed in the add row is watched as what it is,
resolved once: a register's value, the word at an address or a symbol
(followed to wherever the symbol moves), or an expression's value, each read
again at every stop. A `.b` or `.w` after an address, a symbol or an
expression reads a byte or a word there, as `FB+205.w`, and `.s` reads the text
there, to a zero byte; a `$` before a number makes it hex and doesn't set a
width. A symbol at the start of a text data block, such as a debug file's
`.byte "COUNT:"`, reads as its text without `.s`. A symbol's Type cell has its
address, as `Symbol at $60F2`, and `&count_text` watches the address itself.
When an entry can't be watched, the reason runs across the add row, and its
tip has all of it. An entry that doesn't parse as any of these stays in the edit box with
the reason above the list. In an Applesoft program, the pane lists the
variables as well; see [Applesoft in the window](#applesoft-in-the-window).

Under **Zero-page pointers** the pane lists the zero-page pointers: each
zero-page address, then the address its two bytes form and the first four
bytes there, as `$0800: 00 00 00 00`, with Remove on its menu running `ZPC`.
**Add pointer watch...** on the pane's menu, and **Add as pointer watch**
on a Memory pane's menu over a byte in zero page, run `ZP`.

### The heat map

The heat map is a tab beside **Memory 1** in the default layout, and **View >
Heat map** (Ctrl+Alt+H) brings it forward. It displays the whole 64 KB address
space as a grid, one cell an address, filling the pane: each row has a power of
two of addresses from 1 to 1,024, so every row starts at a round address and
its label down the left (such as $0400) is the first address in it. The row
has the count whose cells come nearest the zoom's size, and the cells are then
widened or narrowed so the row spans the pane's whole width, every column
within a pixel of the others; a cell is between three quarters and one and a
half times the zoom's width (wider at the smallest zoom, which cannot narrow a
one-pixel cell, and in rows of 1,024, and narrower in a pane too narrow for
even one cell at the zoom's width), and always the zoom's height. A row never
runs past the pane, so the map never scrolls across: a larger zoom puts fewer
addresses in a row and more rows down the map. Each address is drawn in the
color of what touched it: code (the bytes of every instruction run) in the
disassembly's instruction color, reads in green and writes in red. An address
nothing touched is a dark gray (a light gray on a light theme), and every cell
is set apart from its neighbors by a one-pixel gap of the page. **All**, **Code**, **Data** and **Changed**
above the map choose what it displays: everything, with code ahead of data
where an address is both; executes alone; reads and writes alone; or only the
writes that changed a value (see
[reads before written and changed values](#reads-before-written-and-changed-values)).

The pane's bar has the rest. **Fading**, the default, marks how recently and
how often each address was touched. An address the program touches constantly is
bright, and its brightness follows how many times a second it is touched, on a
logarithmic scale, so a byte read a few times a second and a tight loop's
thousands both appear. Once the program moves off an address it dims, and a
single access is gone after the fade time, 10 seconds of machine time unless
the **Fade** drop-down sets 2, 5, 10, 20, 30 or 60 seconds; a busy address
takes a few times longer to dim away. **Cumulative** counts everything: nothing
fades, and each address's brightness is its total count on a logarithmic scale
against the busiest address, so a few hot loops do not wash out the rest.
The Reset counts glyph starts the totals over. The choice and the fade time
are saved between sessions.

The mouse wheel over the map scrolls it up and down; a scrollbar appears
beside the map when its rows are taller than the pane, and dragging the map
pans it up and down. Ctrl with the wheel over the map zooms it in and out;
elsewhere in the debugger Ctrl with the wheel sizes the text. Zooming changes
the cells' height at every step and, about every third step, how many
addresses a row has; the row under the mouse stays at the mouse's height, and
when the row length does not change, the address under the mouse stays under
it too.

The zoom appears in the map's bottom-right corner as a percentage of the
starting size, three pixels a cell at 100% scaling. Hovering over it brings up
a tip about Ctrl with the wheel. Clicking it opens a slider above it, from one
pixel a cell at the left to the largest at the right, each step along it
scaling the cells by the same factor, and **Reset**, which goes back to the
starting size with $0000 at the top left. Clicking it again, or anywhere else
on the map, closes the slider.

The address the mouse is over appears at once in a tip beside it, with what
touched it and how much, and then its last writer and reader (see below):
"$C65E  executed 120/s, read 3.4/s" while fading, or "$C65E  executed 1,200
times, read once" while cumulative, the numbers to two figures since the map
has 255 levels; a frame is drawn around its cell. While cells are small, a
mouse over an untouched cell picks the busiest cell within a few pixels
instead, so a lone hot byte is easy to land on. Clicking a cell displays its
address in a memory window: the one last used if it is in view, otherwise the
first one that is, otherwise Memory 1, whose tab comes forward.

Over the heat the map marks where the machine is and what will stop it. The
cell at the PC is outlined in the disassembly's PC yellow, and the stack
pointer's byte on page $01 in cyan; in a bank's view each is outlined only
when that bank contains it, so aux RAM's view marks the stack only while the
zero page and stack are switched to aux. Every enabled breakpoint is
outlined: one on an address in the disassembly's breakpoint red, a read data
breakpoint in green and a write data breakpoint in orange, a data breakpoint
on both with both rings. An address with a symbol is tinted toward the symbol
color. Each outline lies around the cell like the hover frame, and a second
one on the same cell sits inside the first. The tip lists what marks the cell:
"PC", "Stack pointer", "Breakpoint #2", "Read data breakpoint #3", "Write data
breakpoint #4", "Read and write data breakpoint #5" and "Symbol COUT".
Breakpoints and symbols are on the CPU's addresses, so in a RAM bank's view a
breakpoint at $D100 marks both language card banks.

Code is drawn two ways. A byte fetched as an instruction's opcode takes the
full code color, and a byte only ever run as an operand is drawn part of the
way to the gray, so each instruction's start is clear and code entered at
an odd offset appears as a dim byte drawn bright. The tip for such a byte is
"executed as an operand".

Zoomed in far enough, a cell displays its contents. Once the text fits at a
size of 9 pixels or more at 100% scaling (cells about 15 pixels), the cell has
its value in hex; once two lines fit with ten characters across (about 26
pixels tall and 58 wide), a second line has the instruction's mnemonic with
its operand's form, such as "LDA (..),Y", for a byte fetched as an opcode,
and the byte's bits otherwise. The text is the theme's text color or its page
color, whichever contrasts more with the cell. The values are the bank's as
they are now: the CPU's view reads what the CPU would, leaving I/O unread;
ROM's view only where a read gets to ROM now; a RAM bank's straight from that
bank. A set of a few small ranges is the easy way to look at this, since a set
starts at the largest zoom that fits.

**Blend** on the bar mixes the colors of an address touched more than one
way, each weighted by how hot it is, instead of the hottest kind alone. An
address both run as code and written, self-modifying code, takes a separate
magenta, which the key lists while Blend is on. Blend is off by default and is
saved between sessions. Whether or not Blend is on, the tip of such an address
is "Self-modifying: run as code and written".

#### Ranges

The map can be focused on the regions a program uses rather than all 64 KB.
The bar's set drop-down, **All memory** until a set is chosen, lists **All
memory** and every saved set; choosing a set displays only its ranges, stacked
in the set's order, each under a header with its name and span, such as
"Hi-res page 1  $2000-$3FFF". Each range's rows follow the same rule as the
whole map, starting at the range's first address and stretched to the pane,
and a range smaller than a row is one row across the pane. A newly chosen set
starts at the largest zoom at which every range fits the pane without
scrolling, and fits again as the pane is resized until the map is zoomed; one
zoom applies to every range, and the zoom's **Reset** fits the set again.
Hovering, the tip, the pick of a busy cell nearby and a click to display the
address in memory all work in every range as on the whole map.

**Edit ranges...** on the bar, or **View > Heat map ranges** (Ctrl+Alt+R),
opens the pane that edits the sets; it stays closed until first opened. Its bar
has the set being edited as a drop-down, **New set**, **Rename set** and
**Delete set**, then **Edit**, **Remove**, **Move up** and **Move down** for
the set's ranges. The list has each range's **Name**, **Start**, **End**,
**Size** and whether it is **Built-in**, with a check box in front to include
the range in the map or not.

Every set has the built-in ranges: **Zero page**, **Stack**, **Text page 1**,
**Text page 2**, **Hi-res page 1**, **Hi-res page 2**, **I/O**, **Language
Card**, and **My program**, which spans the symbols loaded from files and is
listed only while some are loaded. The first time the pane is used there is a
set called **Default** with all of them included; a new set starts with all of
them left out. A built-in range can be checked in or out and moved, but not
edited or removed.

The list's last row, **Add new range**, adds a range: a click opens its cell
for typing, Enter or Tab goes on along the row, and once it has a start the
range is added and a new last row follows it. A start accepts a whole span:
`$6000-$95FF`, `ZP_VARS..+$20` (a symbol and a length), the console's
`$300:$3FF` and `$300,$100`, or a symbol alone, which takes the size its cc65
debug file has for it. A click on a cell, **Edit** or F2 edits a range's name,
start, end or size in place; a size is stored as a length. Every start and end
is read by the console's evaluator, so symbols, decimal and arithmetic work as
in any command. For a range that does not read, the reason is under the bar
and the box stays open; Escape cancels the edit. The gripper at a row's left
drags it to another place in the set. Delete, or **Delete** on the row's menu,
removes the selected range, and Insert starts a new one. The sets, and the set
on the map, are saved between sessions.

#### Counting and history

The machine counts accesses for the heat map only while its pane is open and in
front, or while a [heat column](#heat-columns) is on. Closed, or behind another
tab, it counts nothing and costs nothing, and the cumulative totals include
only what was counted.

The heat map displays the machine where it is in reverse execution's history,
not where the host's clock has gotten to. After a step back, a seek, a click or
a drag on the timeline, a reverse run or a return to live, it displays the map
as it was at the instruction the machine landed on, and running or stepping on
from there counts on from there:

- **Cumulative** totals are the counts from **Reset counts**, or from when the
  pane was opened, up to the current position. History saves the counts made
  between each keyframe and the next, packed small, beside the keyframe, so
  any position's totals follow from them. They count against the history
  budget and appear in its meter: a few percent more history for an idle
  machine, and about two thirds more for a busy game, so the default 64 MB
  covers about 26 minutes of such a game rather than 43. A position before a
  reset, or before the pane was opened, is blank.
- **Fading** heat is rebuilt after each move by replaying three times the
  fade time before the landing, since a busy address stays warm well past
  the fade time, as far back as history goes, on a second machine in the
  background, so neither the machine nor the debugger is delayed. The newest
  two seconds come first and the older seconds fill in after them, and the bar
  has **Rebuilding** in place of the color key until they are all in.
  During a drag of the timeline the rebuild starts once the button is
  released.

A stretch of history made while the pane was closed has no counts, so a
seek across it adds nothing for it.

#### Banks

On a machine with banked memory (the //e, the //c, and a ][+ with its
Language Card), the **Bank** drop-down on the bar, after the ranges, chooses
which memory the map displays. **CPU**, the default, is the 64 KB the CPU
addresses, whatever is banked in at the time of each access. The others
display where each access really landed:

- **Main RAM** and **Aux RAM** (on the //e and //c) are 64 KB each, laid out
  as the //e's memory chips contain them: $0000-$BFFF is the RAM the CPU
  accesses there, and the top 16 KB has the Language Card's RAM, bank 1 of
  $D000 at $C000-$CFFF, bank 2 at $D000-$DFFF and the high RAM at
  $E000-$FFFF. A write with RAMWRT on appears in Aux RAM, a read with ALTZP on
  of $0000-$01FF or of the Language Card appears in Aux RAM, and so on.
- **Language card** and **Aux language card** display that top 16 KB alone.
- **ROM** displays the reads that got to ROM, at the addresses they were made
  at, and any write to an address where ROM is banked in.

I/O ($C000-$C0FF) appears in the CPU's view only. Each view fades, counts,
resets and follows history just as the CPU's does, and the tip includes where
in the bank the cell is: "Aux RAM $2000", "Main language card bank 1 $D123",
"High RAM $E000" on a ][+, "ROM $F800". The choice is saved between sessions;
on a machine without that bank the map displays the CPU's view.

#### Last writer and last reader

For every address, in every bank, the heat map also tracks the instruction
that last wrote it and the one that last read it as data. Once the mouse has
been over a cell for a moment, the tip adds them on two more lines: "Last
written by $6A12 STA ($06),Y at cycle 1,234,567", with the symbol at that
address when one is loaded ("$6A12 DRAWROW: STA ($06),Y"), and "Not read
since counting started" when nothing has read it. The instruction is
disassembled from the code at that address as it is now. An interrupt's
pushes onto the stack count as the interrupted instruction's.

- **Ctrl+click** a cell to display its last writer in the disassembly, and
  **Ctrl+Shift+click** to display its last reader. A plain click displays the
  address in a memory window.
- **Ctrl+Alt+click** a cell to go back through history to its last write, and
  **Ctrl+Alt+Shift+click** to its last read. The machine stops first if it is
  running, and lands just after the instruction that made the access: the
  byte has the value it wrote, the instruction is the one just above the
  PC, and a step back runs to it. A write older than the history, or made
  while history recording was off, is reported in the console instead.
- Right-click a cell, or a byte in a memory window, for the same four actions
  as menu rows: **Show last writer**, **Show last reader**, **Rewind to last
  write time** and **Rewind to last read time**, labeled **Fast-forward** instead while
  history is replayed from before the access. A row is grayed out for an access
  that never happened, or a rewind to one older than the history. A memory window's rows use the CPU's view
  of the byte.

The last writers and readers follow the machine through history as the
totals do: after a seek, a step or a run on from the past, they are what was
true at that position. They add nothing to history. The map tracks, for each
address, the latest access up to the furthest the machine has run; where that
is later than where the machine is, the access before it is found when the tip
or a click requires it: the counts in history identify the stretch between
keyframes it was in, and that one stretch is replayed on a second machine,
which takes a few milliseconds. An access older than the history is reported
as such. Nothing from before the pane was opened, or from before **Reset
counts**, is displayed.

#### Reads before written and changed values

For every byte of main and aux RAM, the Language Card's banks among them,
the heat map tracks whether the CPU has written it since power-on. A read of
a byte nothing has written is a **read before written**: the program is
reading whatever power-on left there. In **All** and **Data** such reads
are drawn in violet over whatever else the cell displays, as hot as they
were, and the tip adds "read before written" with how often. An instruction
fetched from RAM nothing wrote is code, not a read, and is not counted. Only
a power cycle makes RAM unwritten again; after a reset (Ctrl+Reset) RAM counts
as written, as the RAM retains its bytes.

The debugger's writes (a memory window's edit, a poke, a fill, a binary
loaded, the line assembler) are not the program's, so the heat map counts
them as nothing: no write, no change, no last writer, and not written for
the purpose above. Each byte the debugger wrote is marked as edited instead,
the tip over it ends with "Edited in the debugger", and a read of it is no
read before written, since the debugger put the value there. A power cycle
clears the marks with the RAM.

The map can track what was written only from power-on. Opened later, it
counts every byte as written and so doesn't find reads before written until
the next power cycle; a power cycle with the pane open (or with
`BRKUNINIT` on) starts the tracking. The ROM's start-up code reads some bytes
before anything writes them, such as the power-up byte at $03F4, and a
program may probe memory the same way.

`BRKUNINIT ON` stops the machine after such a read, as a data breakpoint does:
"Read before written: $00 from $2000 by $0300" is the value, the address read
and the instruction that read it. `BRKUNINIT OFF` turns it off, and
`BRKUNINIT` alone reports the setting, whether the map is tracking yet, and
how many reads before written it has counted at how many addresses. While the
break is on, the heat map counts whether or not its pane is open. `BPC *`
clears it with the breakpoints, `BPSAVE` writes it, and opening another
machine or loading a machine state turns it off. A reverse run doesn't stop on
it.

To exclude known reads, put their ranges in a set (see [ranges](#ranges))
and choose it in the bar's **Uninitialized reads** drop-down: reads before
written in its included ranges neither stop the machine nor appear on the map
nor count in what `BRKUNINIT` reports, though the map counts them. A range
given in a bank's view is matched at the address the CPU accesses the byte at,
so bank 1 of the Language Card is at $D000. While the chosen set is renamed or
deleted, every read before written counts until a set is chosen again.

A write that stores the value a byte already has changes nothing. **Ignore
writes that don't change the value** on the bar excludes such writes from the
writes displayed, in every view, fading or cumulative; the **Changed** view
displays only the writes that changed a value, in rose. A write to I/O, or to
an address that no memory responds to, cannot be read back first, and always counts
as a change.

Both follow the machine through history as the totals do. Beside each
keyframe, history saves which RAM had been written, which bytes the
debugger edited and which were fetched as opcodes (the marks that distinguish
an operand from code), packed, or a note that they are as at the keyframe
before; a seek, a step back or a run on from the past puts it back, so a read
counts as a read before written exactly as it did the first time, and a
rebuild of fading heat starts from it. **Reset counts** zeroes their counts
with the rest and doesn't change what was written. Loading a machine state
starts the counts over, and, as the map comes on again away from power-on,
every byte counts as written.

The map's tables, with what is saved to follow history, take about 60 MB of
memory while the pane is open (or `BRKUNINIT` is on), and 35 MB more while
fading heat is rebuilt; none while it is closed. Which RAM was written adds
little to history: saved whole, with the edited and opcode marks, only at
the keyframes where any of them changed, it packs to about 64 bytes each.

## Devices and logs

### Device panels

The **View > Device panels** submenu lists the machine's devices: the Disk II
controller, the //e MMU and its memory map, the video switches, the keyboard,
the Mockingboard, the printer card, the Super Serial Card, the //c's two serial
ports, the Uthernet II, the cassette deck and the clock. Each opens as a pane
with its registers and state, and the disk head position, the memory map, or
level meters where the device has them. `PANEL LIST`, `PANEL name` (or
`PANEL OPEN name` or `PANEL name OPEN`) and `PANEL CLOSE name` (or
`PANEL name CLOSE`) do the same from the command box. In GSSquared mode,
`debug` lists the panels, `debug "name"` opens one and `nodebug "name"` closes
it.

The logs are in the **View > Logs** submenu: Disk II log, Input log, Video
log, Sound log, Serial log and Network log.

Each log filters its rows from its columns' headings, as File Explorer does.
A chevron at the right of a heading opens the column's filter. A column of a
few values, such as Source, Severity, Event or Track, lists each value its
events show with a check box, and checking values lists only their rows. A
column of numbers, such as Time, Cycle, Address or Since, filters by Less
than, Greater than, Equal to or Between, typed as the column shows the value:
a count with its commas, an address after a $, a time of day. A filtered
column's heading has a funnel, Clear filter on its chevron ends the filter,
and the filters are saved with the log's settings.

### Disk II

The Disk II panel's bar chooses the drives it draws: **D1**, **D2** or
**Both**, the default, which is saved with the debugger's settings. With both,
each drive is drawn side by side where the pane is wide enough and one over
the other where it isn't. Each drive's name is ahead of its head's ruler.
Under the ruler are the head's track, the word Motor, lit while the motor is
on, and the stepper's phases in the order 3 2 1 0, each digit before its lamp,
the last ending at the ruler's right edge. Only the selected drive's phases
and motor are lit. In the rows under the drawing, the Head group lists the
track, then the quarter track, then the phases in the same order.

The **Disk II log** (Ctrl+Alt+L, D) has an entry for each thing the controller
does: the motor, a head step or bump, an address field, a sector read or
written, the door and the drive selected, with a Severity of Info, Warning or
Error. Its Track, Sector and Volume columns have the event's place: the quarter
track a step or a bump is at, an address or data field's numbers, and `?` for a
number not read yet.

To stop on these events, see [disk breakpoints](#disk-breakpoints).

### The Mockingboard

The Mockingboard panel is a graphical view of the card's chips, in as many
columns as fit the pane. On a Mockingboard C, the SSI-263's card is across the
top: the phoneme by name, its waveform, its three formants over the pitch's
harmonics, a strip of the recent phonemes with widths by duration under a
Recent phonemes heading, a row under the strip with the same phonemes' labels
in order and an arrow under the one playing, its timing values and its lamps.
To go back through the strip, tilt the wheel over it or turn the wheel while
pressing Shift. Each AY-3-8910's card has a row per channel: a mute, the
note and frequency, tone and noise indicators, a level meter and a scope. Below
them are a picture of the envelope, the noise frequency, and its 6522's bus
state, AY write count, last AY write, registers, T1 rate and lamps. A tip on
each part has the raw register values. A mute changes the host's audio only, so
it isn't part of machine state or history, and the muted channel's scope is
unaffected. `PANEL` in the console and a saved state have the chips' values as
rows and fields.

### Video and sound logs

The video log has an entry for each change of video mode (`TEXT`, `MIXED`,
`PAGE2`, `HIRES`, `80COL`, `DHIRES`, `ALTCHARSET` and `80STORE`) with the
machine's cycle count and the scanline and cycle within the line the beam was
at, grouped under the frame each fell in. An access that does not change the
mode doesn't make an entry. The sound log has an entry for each speaker toggle
and each write to the Mockingboard's 6522s, with its cycle count and the cycles
since the last entry of its kind; a port B write that clocks a value into an AY
register also makes an entry for that write, with the register and the value.
A write to a Mockingboard C's speech chip makes an entry too, decoded by
register: the phoneme by name and its duration, the rate, the inflection, the
power state, the articulation and amplitude, or the filter. Entries are added
to both logs all the time, from the start of the session, whether or not the
debugger started the run.

`VIDEOLOG` and `SOUNDLOG` take `CLEAR`, which hides the entries made so far,
`LIST [n]` (the last `n` entries, 100 unless given; a bare `VIDEOLOG` or
`SOUNDLOG` lists) and `SAVE [file]`, which writes every entry since the log
was cleared (`VideoLog.txt` or `SoundLog.txt` unless given). The newest entries
are in memory; older ones are rebuilt from history for each listing, as far
back as history goes, and a rewind drops the entries after the position it
rewinds to. A listing stops rebuilding after five seconds, and its first line
then ends `older history was not searched:`. The cycle count and beam position
are those at the access.

The debugger window lists each log in a separate pane, **Video log** and
**Sound log**, opened from **View > Logs** or from the Video and Mockingboard
panels' menus. A row is an entry, with the same text the command lists, in
columns that can be sorted, displayed or hidden: the video log has the frame,
the scanline and the line cycle (the CPU cycle within the scanline); the sound
log has the cycles since the last entry of its kind and its source, which the
Source heading's chevron narrows to the speaker, the 6522s, the AY-3-8910s or the
SSI-263. Scrolling to the top rebuilds older entries from history. Clear and
Save on the pane's bar do what `CLEAR` and `SAVE` do, so the pane and the
command always list the same log.

A speaker toggle costs about fourteen nanoseconds of host time, so a program
sounding a note runs one to three percent slower at maximum speed, and a loop
that toggles the speaker as fast as a 6502 can about a sixth slower. Other
I/O, a keyboard poll among it, costs nothing measurable.

### Serial and network logs

The serial log has the traffic of each 6551: the Super Serial Card's and the
//c's two ports. The network log has the Uthernet II's sockets. Each opens from
**View > Logs**, or from the serial and Uthernet II panels' menus.

Each pane has a graph across the top and a list under it. The serial log's
graph has a lane for each of TxD, RxD, RTS, CTS, DTR, DSR, DCD and the
interrupt line. TxD and RxD are drawn bit by bit: each byte's start bit, data
bits low bit first, parity bit and stop bits, laid out from the word length,
parity and stop bits in force and the baud rate, as the 6551 times them. A
line rests at mark. The network log's graph has a lane for each socket,
colored by its state, with a tick up for each send and down for each receive.

The bar has a toggle for each lane, and Zoom in and Zoom out. The graph ends
just after the newest row, or centers on the row the keyboard is on; the
serial graph is of that row's port. Rest the pointer on the graph for the state
of each lane turned on: on or off for a control line, mark or space for a data
line with which bit of which byte it is, and for a socket its state and the
bytes it has sent and received.

The list has a row for each byte a port sent or received, or each send and
receive of a socket, with the bytes as text and in hex. Reads of the 6551's
status register don't make an entry. Both logs are rebuilt from history, as the
video and sound logs are, so they follow a rewind or a seek.
What a step back does to the network's connections is in
[the network in history](#the-network-in-history).

### The input log

The **Input log** (Ctrl+Alt+L, I) lists the machine's input. Its filters and
columns are saved with the [project](#projects).

### The cassette deck

`TAPE PLAY`, `TAPE STOP`, `TAPE REWIND` and `TAPE EJECT` work the transport,
`TAPE INSERT file` loads another tape, and `TAPE` reports the transport, the
place and the length. In batch mode, `--tape file` puts a recording in the
recorder. The //c has no cassette jacks.

## Applesoft

The debugger stops an Applesoft program where the interpreter itself passes:
as a line begins, after a statement changes a variable, and on an error. The
commands are Casso commands: `/bpbasic` in Monitor mode, `bpbasic` in GSSquared
mode and `!bpbasic` in WinDbg mode.
[Tutorial 4](Debugger-Tutorial.md#4-debugging-applesoft) uses them on the
Joyport test program.

### BASIC breakpoints and steps

| Command | Effect |
|---|---|
| `BPBASIC 100` | stops as line 100 begins, before its first statement runs |
| `BPBASICVAR A`, `BPBASICVAR N$`, `BPBASICVAR X(2,3)` | stops after the statement that changes the variable or array element |
| `BRKBASIC ON` | stops on an error, whether `ONERR` handles it or not; `BRKBASIC OFF` removes the stop |
| `BASICSTEP` | runs to the start of the next statement, into a subroutine a `GOSUB` calls |
| `BASICLINE` | runs to the start of the next line |
| `BASICOUT` | runs to the statement after the newest `GOSUB` returns |
| `BASICVARS` | lists the simple variables and the arrays, each value as `PRINT` writes it |
| `BASICSTACK` | lists the active `GOSUB` returns and `FOR` loops, newest first |

```text
Breakpoint #0 at BASIC line 100
Breakpoint #1: A changed from 0 to 1 in line 10, stopped at line 20
BASIC step to line 30
BASIC error: ?DIVISION BY ZERO ERROR IN 20 (code $85, ONERR on)
```

- A variable is matched as Applesoft matches it, by the first two characters
  of its name and its type, so `BPBASICVAR COUNT` watches `CO`. A variable
  that does not exist yet reads as 0 or the empty string, so a breakpoint set
  before the program runs stops the first time the program sets another
  value.
- A variable's value at each statement's start is compared with its value at
  the start of the statement before. A value typed at the prompt, and the
  variables `RUN` clears, stop nothing.
- A line breakpoint may be set before the line is in the program.
- After a line, variable or step stop the PC is $D820, where the interpreter
  starts each statement; after an error stop it is $D412, before the message
  is printed or `ONERR`'s handler runs.
- The steps and the two lists work only with Applesoft in the $D000 bank. On
  a machine with other code there, they report that Applesoft is not in
  memory.
- In the Breakpoints pane their Kind is "BASIC" and their Trigger "Line",
  "Variable change" or "Error", and its New menu has "BASIC line...", "BASIC
  variable change..." and "BASIC error...".
- Reverse continue stops on them where a forward run does. A change made
  before the snapshot a replay starts from, by a statement that ends after
  it, is not found. The BASIC steps run forward only.
- With none set, nothing is tested: the interpreter's pages are watched only
  while a BASIC breakpoint or step is active.

### Loading and saving programs

`BASICLOAD file`, or **File > Load BASIC program...**, replaces the Applesoft
program in memory with the listing in a text file; the window prompts before
replacing a program, and opens the BASIC program pane. `BASICSAVE file`, or
**File > Save BASIC program...**, saves the program in memory to a text file as
a listing.

### Applesoft in the window

**View > Source files > BASIC program** (Ctrl+Alt+P, P) opens the program in a
pane, which stays open across restarts until closed. The pane also opens at
the line when the machine stops in the interpreter while a program runs, and
when a BASIC line breakpoint is set.

- **BASIC program**, a tab beside the disassembly, lists the program as LIST
  prints it, colored, read from memory as the program changes. The line being
  run has the PC's row color and arrow; its statement is in full color and the
  line's other statements are muted. Enter or a double-click on a row sets a
  line breakpoint there, or clears the one there. With the keyboard focus in
  this pane, Step into runs `BASICSTEP`, Step over `BASICLINE` and Step out
  `BASICOUT`. With heat on, its **Heat** column colors each line by how often
  it ran; see [heat columns](#heat-columns).
- **Watches** lists the variables while the machine is stopped in a running
  program. The Automatic section has the ones the statement being run uses,
  then in italics the ones the statement just run used, so a step past
  `I = I + 1` lists I there too; a value that changed since the last stop is in
  the changed color. `W` with a variable or array element as Applesoft writes
  it, such as `A$`, `N%` or `X(2,3)`, watches it, as it does a plain name the
  stopped program has, unless a symbol has the name; text typed in the add row
  is read the same way. `W BASIC name` always watches the variable. `WL` lists
  each with its value, and the debug project saves them as `WA BASIC name`.
  The BASIC variables heading expands to every simple variable and array
  element, each value as `PRINT` writes it. Applesoft reads only a name's first
  two characters, so `HIGH` and `HI` are one variable, and the characters past
  the second are dimmed.
- **Call stack** lists the active `GOSUB` returns and `FOR` loops first,
  newest first, each with its line; Enter or a double-click brings the line
  into view in the program pane.

At the `]` prompt no line is marked.

## Profiling and trace

### The profiler

`PROFILE HISTORY seconds` builds the profile of the last that many seconds of
history, and `PROFILE HISTORY ALL` of all of it: the history is replayed on a
scratch machine in the background, so the machine does not slow down, and the
finished counts become the profile the lists below read. The history is cut
into blocks at keyframes and the blocks are replayed on several scratch
machines at once; each block's counts are cached, so a longer period, or the
same one built again, replays only the blocks not counted before.

`PROFILE ON` counts every instruction the machine executes, in a run the
debugger starts or one the machine makes by itself, and `PROFILE OFF` stops.
The machine runs slower while profiling is on. `PROFILE LIST` lists the
count, cycles and share for each mnemonic and addressing mode, and the penalty
cycles apart: page crossings on indexed reads, taken branches, and branches
that cross a page. `PROFILE LIST ADDR` lists the twenty hottest addresses.

Profiling also counts the cycles of each routine. A JSR, a BRK or an interrupt
starts a routine at the address it lands on, and the routine ends when the
stack pointer rises back to where it was before the call, so RTS, RTI and a
routine that pulls its return address all end it. Code outside any call
is Top level. `PROFILE LIST ROUTINES` lists each routine's self cycles, its
total with every routine it called, its calls and its cycles per call; a
routine that calls itself is counted once in its total. `PROFILE LIST
TREE` lists the call tree, each routine under its caller. `PROFILE SAVE file`
writes every table; `PROFILE RESET` clears them.

### The Profiler pane

**View > Profiler** lists the profile as `PROFILE` lists it, read while the
machine is paused. Its bar builds the profile of a period of history with
**Show** (Last 5 s, Last 10 s, Last 60 s, Last 5 min or Since history start),
with "Loading profile... (n%)" over the list while the profile builds, starts
the counts over with the Reset glyph, writes them to a text file with the Save
glyph, and chooses **Routines**, the view it opens on, **Call tree**, **Flame
graph**, **Addresses** or **Opcodes**. While the pane is open, the chosen
period's profile is built again at a stop a frame or more from the last build,
so a step doesn't rebuild it.

A click on a column's heading sorts the list by it, and a second click reverses
the order; each view opens sorted by cycles, by self for the routines, or by
total for the call tree, most first, and the call tree sorts each routine's
callees under it. The routines and the call tree have a square of each
routine's timeline color before its name. A line above the list has the totals
and the time the profile's cycles cover, so a period longer than the history
is reported as the shorter time there is; while a profile builds, its progress
bar is drawn over the list. In every view, activating a routine moves the
disassembly to it.

### The flame graph and timeline

A timeline of the period runs across the top of the Profiler pane, each bar a
frame, or several frames joined when the period has more than fit, with its
cycles by routine, the eight hottest in color and the rest in gray; resting
the pointer on a bar lists its routines. The wheel over the timeline zooms it
around the pointer, with or without Ctrl, and a horizontal wheel or
Shift+wheel pans it; once the wheel stops, the zoomed span is profiled from
history, so its bars are single frames. A drag across the timeline selects
whole frames, and the list and the flame graph then cover only the selection;
a click on the timeline clears it.

The flame graph draws Top level across the top and each routine under its
caller, as wide as its total: a click selects a routine, outlining its bar,
moves the disassembly to it and selects its row the next time you switch to
the routines or the call tree; a double-click zooms into it, and a
double-click on Top level or a caller above zooms back out.

Each zoom, selection and flame graph zoom is a step that the mouse's back
and forward buttons, or Alt+Left and Alt+Right, move through, as in a
browser, without profiling again.

### The stopwatch

`STOPWATCH start [stop]` times every pass from one address to another in runs
the debugger starts: a lap begins when the instruction at `start` is about to
run and ends when the one at `stop` is, and is the exact cycle count between
the two. Each address is an expression, as `BP` takes one, so `STOPWATCH
DrawRow DrawRow_Exit` times a routine from its entry to its exit. With one
address, each lap is the time from one arrival to the next, such as a game
loop's period. A routine that calls itself is timed from its outermost entry.
A bare `STOPWATCH` lists the laps: their count, the last, the shortest, the
longest and the average. `STOPWATCH RESET` clears the laps, and `STOPWATCH OFF`
stops timing without clearing them. **Start stopwatch here** and **Stop
stopwatch here** on a disassembly row's menu set the two addresses.

To time between two breakpoints instead, stop at the first, run `RCC`, run to
the second, and `CYCLES PART` reports the cycles in between.

### The instruction trace

With `HISTORY ON`, every instruction the machine runs is saved, up to the
newest 100,000: the cycle count, the registers before it, and the memory access it
made, with symbols. `HISTORY OFF` stops recording without clearing the trace.

While history recording is on, the trace starts where history starts,
`HISTORY ON` or not, and the window's Trace switch is unavailable: the window
turns the live ring on only while the trace pane is visible and the machine
runs at 1x, the only time the newest instructions are watched as they come.
Older instructions are rebuilt from history by replaying it, at once while the
machine is stopped and in the background while it runs, and the 100,000 newest
are a cache that makes the newest quick to list. While the machine runs, the
trace ends at history's newest keyframe unless `HISTORY ON` is set, and then
the ring has the instructions since. A row history cannot replay, because its
disks were changed since, reads "Not available". `HISTORY SAVE` writes at most
the newest 1,000,000 entries.

```text
HISTORY                  the newest 20 entries
HISTORY 1175751201 40    40 entries from the first at or after that cycle (decimal)
HISTORY SAVE run.txt     every retained entry
```

Each entry is identified by its cycle count, which no later instruction
changes.

The trace costs nothing while it is off.

The window's trace pane lists its keys above its rows, and a click on a key
there does what the key does. With the pane focused, `Space`, `O` and `R` step
into, over and out, `Enter` runs, `T` turns the trace off and on, `B` turns
the bytes column off and on, and `S` saves the trace to a file you pick, as
`HISTORY SAVE` does. **File > Save trace...** saves it too. While the machine
is stopped, the next few instructions from the PC follow the last entry,
taking each branch the way the current flags send it and ending at one an
earlier instruction may have changed the flags for.

## Projects

A debug project, one for each program, has the debugger's configuration:

- breakpoints and the name of each, watches, zero-page pointers, bookmarks,
  data directives, step filters, loaded symbol files, the symbols you added
  with `SYM name = addr` or **Add symbol here...**, and which symbol tables
  are on;
- the command mode, the `CD` directory, the instruction trace and trace file,
  the profiler, the stopwatch and where the trip count starts;
- the pane layout, the closed panes, the pane with the focus, where the
  command bar and the timeline are docked, and the open views;
- the console history, each pane's find history and options, the column
  widths you set, and the undo history of the breakpoints, the watches, the
  registers, the stack and the memory windows;
- the disassembly options, the heat map's options and range sets, and the
  Disk II and input logs' filters and columns.

Each breakpoint, data breakpoint, watch and bookmark comes back under the
number it had, and the next one you add takes the number it would have taken
before. The key scheme, the theme, the text size and the history settings are
yours rather than a program's, and are the same for every project. The program
is the most specific of these:

1. the debug or symbol file `SYM LOAD` last loaded, by its content rather
   than its path, so moving the file doesn't break the match;
2. the binary `BLOAD` last loaded, by its content and address;
3. the machine with the disk drive 1 had at its last power-on, by the disk's
   path;
4. the machine.

Projects are saved in `%LOCALAPPDATA%\Casso\DebugProjects` two seconds after a
change and when the debugger is closed. Loading a more specific program that
has a project replaces the debugger's items with that project's; answer Yes to
the prompt that follows to add the items set before as well. A program with no
project yet starts from the items set so far.

**File > Save project as...** writes the project to a file of your choosing,
and that file is the program's project from then on; **File > Open
project...** opens one. `PROJECT` lists the open project and its file,
`PROJECT SAVEAS file` and `PROJECT OPEN file` match the two menu items, and
`PROJECT MERGE` adds the items of the project open before the last switch.

**File > Project settings...** lists the startup commands, each with its
command mode, which run in order each time the project opens. The list is
edited in place, as the heat map's ranges are: the last row, **Add startup
command**, adds one, a click on a command or its mode edits it, a mode left
empty is AppleWin, a row's gripper drags it to another place in the order, and
Delete or the row's menu deletes it. A project file that cannot be read is
copied to the same file name with `.unreadable-` and the date appended, and an
empty project opens in its place.

## Command modes and scripting

### Command modes

The debugger reads each line in one of five modes.

- **AppleWin** (the default) uses AppleWin's debugger commands: `BP`, `G`,
  `T`, `D`, `U`, `SYM` and so on. Values may be expressions and symbols. A
  bare number is hex; `$` or `#` also marks hex, as in AppleWin, and `0n`
  marks decimal, as in `0n192`. A word is a loaded symbol first, as in
  AppleWin, then a register, then a hex number, so with Applesoft's symbols
  loaded `FADD` is the routine and `$FADD` the number. `@A`, `@X`, `@Y`, `@P`,
  `@S` and `@PC` are always the register. A word resolves the same way in
  Casso and GSSquared modes.
- **Monitor** uses the Apple II Monitor's syntax: `300.30F`, `300: A9 41`,
  `300L`, `300G`, `^E`. Values are plain hex digits; there are no expressions
  or symbols. Control-key commands are typed as `^` and a letter.
- **GSSquared** uses the GSSquared emulator's debugger commands; see
  [GSSquared mode](#gssquared-mode).
- **WinDbg** is WinDbg-flavored: the WinDbg commands a 6502 session uses, with
  WinDbg's arguments and layouts; see [WinDbg mode](#windbg-mode). It is
  not WinDbg; the rest of WinDbg has no meaning on this machine, and the reply
  to it is an error.
- **Casso** is Casso's complete native command set: every command Casso has,
  AppleWin's and Casso's additions, each under its full word, with replies in
  AppleWin's format.

Switch with `MODE MONITOR`, `MODE GSSQUARED`, `MODE WINDBG`, `MODE CASSO` and
`MODE APPLEWIN`, or from the **Dialect** drop-down on the console's toolbar;
`MODE` alone reports the current mode. Batch mode starts in the mode `--mode`
sets. Every mode's commands are in [Debugger-Commands.md](Debugger-Commands.md).

In Monitor mode, a line starting with `/` is read as a Casso command, so it
gets to everything the Monitor has no command for:

```
*/BPL
*/SYM LOAD "game.dbg"
```

### GSSquared mode

| Command | Effect |
|---|---|
| `addr` | examine one byte |
| `first.last` | dump the range, sixteen bytes a line with the characters after them |
| `addr: b b ...`, `set addr b b ...` | deposit bytes |
| `move first.last dest` | copy a range |
| `l addr`, `addrl`, `l` | disassemble from an address, or continue |
| `bp addr`, `bp first.last`, `bp` | set an execution breakpoint, or list the breakpoints and data breakpoints |
| `bpd addr r\|w\|rw`, `bpi addr r\|w\|rw` | stop on a read, a write or either; `bpi` takes an I/O address, $C000-$C0FF |
| `nobp id`, `nobp addr` | clear a breakpoint by id, or the execution breakpoint at an address |
| `watch addr`, `watch first.last`, `watch`, `nowatch id` | add, list and clear watches; a range adds one watch per address |
| `load "file" addr`, `save "file" first.last` | load and save a binary, as `BLOAD` and `BSAVE` |
| `sload "file"`, `slookup addr`, `sclear` | load, look up and clear symbols |
| `s`, `o`, `r`, `g` | step into, step over, step out, run |

- Addresses and bytes are hex with no prefix, and `0n` marks a decimal one, as
  in `bp 0n768`; ids are decimal and may also have `0n`. Command words are
  case-insensitive. `bp`, `bpd` and `bpi` take a trailing `IF expression`, as
  in AppleWin mode.
- An address may have a bank, as on a IIgs: `00/300` is $0300. Any other bank
  is an error, since only bank 00 exists on these machines.
- `map` lists the current machine's memory map, as `MAP` does. `m`, `x`,
  `video` and `novideo` are IIgs commands; the reply to these is that they are
  not available. In the debugger window, `debug` lists the device panels,
  `debug "name"` opens one and `nodebug "name"` closes it, as `PANEL` does.
- GSSquared itself steps by key rather than by command. In the debugger
  window, with the command line empty, Space and F10 step and Return runs, as
  in GSSquared; `s`, `o`, `r` and `g` are how a script does the same.
- A breakpoint set in GSSquared mode is the same breakpoint `BPL` lists in
  AppleWin mode.

### WinDbg mode

Each command has the effect of the AppleWin command beside it.

| Command | Effect | AppleWin |
|---|---|---|
| `t`, `p`, `gu` | step into, over, out | `T`, `P`, `RTS` |
| `g [addr]`, `pa addr`, `ta addr` | resume, or run to an address | `G` |
| `bp addr`, ``bp `file:line` `` | execution breakpoint | `BP` |
| `ba r1\|w1\|e1 addr` | stop on a read, a write, or execution of one byte; a larger size covers more | `BPMR`, `BPMW`, `BP` |
| `bl`, `bc n\|*`, `bd n\|*`, `be n\|*` | list, clear, disable, enable | `BPL`, `BPC`, `BPD`, `BPE` |
| `db`, `dw`, `dd`, `da` `addr [l n]` | bytes, words, double words, a string to a zero | `D` |
| `eb addr b ...`, `ew addr w ...`, `ea addr "text"` | deposit | `MEB`, `MEW` |
| `f addr l n b`, `s addr l n b ...`, `m src l n dest` | fill, search, move | `F`, `S`, `M` |
| `r`, `r a=41` | display or set registers (`a`, `x`, `y`, `sp`, `pc`, `fl`) | `R` |
| `u [addr]`, `x pattern`, `k` | disassemble, find symbols (`*` and `?` match), call stack | `U`, `SYM`, `CALLS` |
| `? expr`, `.formats value` | evaluate | `CALC` |
| `l+s`, `l-s`, `lsa` | step by source line on, off; the line at PC | `SRC` |

- A bare number is hex; `0x300` and `$300` are the same, and `0n10` is
  decimal. A loaded symbol comes before a number, and `@a`, `@x`, `@y`, `@sp`,
  `@pc` and `@fl` are the registers. A length is `l` and a count in the
  command's units.
- The prompt is `0:000>`. A stop prints AppleWin's stop line, then `r`'s.
- Threads, modules, exceptions, the kernel, dump files, types and scripting
  (`~`, `lm`, `sxe`, `!pte`, `.dump`, `dt`, `.foreach` and the like) reply
  `Error: no meaning on this machine` with the family. `wt`, `s -a`, `s -b`
  and `lsa file:line` are not available yet.

### Output format

Replies are written in an output format: AppleWin, Monitor, GSSquared or
WinDbg. `MODE` sets the format along with the mode; `OUTPUT name` then changes
the format alone, and `OUTPUT` reports it. Batch mode takes `--output`. A reply
a format has no layout for is written as AppleWin writes it, and GSSquared has
no stop line, so in its format a stop is written as in AppleWin's.

### Getting help

Each mode's help command (`HELP` or `?`, `/HELP` in Monitor, `help` in
GSSquared, `.help` in WinDbg) alone lists the help sections and the command for
each: `help breakpoints`, `help memory` and so on list one section, and `help
all` lists everything that can be typed in that mode, grouped by category and
alphabetical within each, syntax on the left and what it does on the right:
the mode's commands first, then the Casso commands available in it, written as
they are typed there. A Casso command the mode has a form of is not repeated,
and one whose word the mode already uses (GSSquared's `r`, or any hex word,
which GSSquared reads as an address) is not listed. Help for one command
is its description, or the modes that run it; for a word that is both a command
and a section, help is the command's description, with a line on the command
for the section.
Any other text is a search: every command whose syntax or description contains
it, ignoring case, with `*` and `?` as wildcards (`help bp*`) or a regular
expression between slashes (`help /^bp[de]/`).
[Debugger-Commands.md](Debugger-Commands.md) has every mode's `help all`,
generated from the same table.

### Mistyped commands

The reply to a word that is another mode's command is that mode's title and the
current mode's equivalent, with the same arguments: in WinDbg mode, the reply
to `nobp 0` is "nobp is a GSSquared command. WinDbg's is bc 0." A word several
modes use is matched to the first of AppleWin, Casso, Monitor, GSSquared and
WinDbg that has it. When the current mode has no equivalent the reply reports
that. The reply to a word no mode has is the command whose letters are closest.
The suggested line appears in gray after the caret in the command box; Tab runs
nothing but puts it into the box, and any other key clears it. The reply to a
command given the wrong arguments is its syntax line.

### Casso commands

Every mode gets to Casso's commands (AppleWin's and Casso's additions) through
a marker, unless the mode uses the word for something else:

| Mode | Marker | Example |
|---|---|---|
| AppleWin, Casso | the bare word | `SWITCHES`, `DISK` |
| Monitor | `/` | `/switches`, `/disk` |
| GSSquared | the bare word; `/` is its bank separator | `switches`, `disk` |
| WinDbg | `!` | `!switches`, `!disk` |

These are Casso's additions to AppleWin's set:

| Command | Effect |
|---|---|
| `MODE [APPLEWIN\|MONITOR\|GSSQUARED\|WINDBG\|CASSO]` | reports or sets the command mode, and sets the output format to match |
| `OUTPUT [APPLEWIN\|MONITOR\|GSSQUARED\|WINDBG]` | reports or sets the output format alone |
| `PAUSE` | stops a running machine |
| `BUDGET n` | limits every later run to `n` cycles (decimal); `BUDGET 0` removes the limit |
| `SWITCHES` | lists the soft switches and whether each is on |
| `MAP` | lists, for each address range, where reads and writes go now (main or auxiliary RAM, ROM, language card bank 1 or 2, slot or internal ROM) as the language card, auxiliary memory and ROM switches set them. $C800-$CEFF is the ROM of the slot whose card is selected there, or unused (floating bus) when no card is |
| `STACK` | lists the stack pointer and the stack page above it |
| `PATCH addr value...` | writes bytes the way a memory window edit does: into RAM, or into ROM so the machine runs the patched code; it doesn't write an I/O address, which is what `OUT` is for |

The rest, among them the [Applesoft](#applesoft), [disk](#disk-breakpoints),
[profiler](#the-profiler) and [machine](#controlling-the-machine) commands,
are in the Casso commands of each mode in
[Debugger-Commands.md](Debugger-Commands.md).

### The debug channel

The debug channel is a Windows pipe, open whenever the debugger window is. A tool
attaches to it with `CassoCli debug --attach <pid>` (see
[attaching](#attaching)), or connects to the pipe directly; the protocol, its
replies and its notifications are in [DebugChannel.md](DebugChannel.md).
Attaching from an AI agent or another automated tool is in
[DebuggerForAgents.md](DebuggerForAgents.md).

### MCP

`CassoCli mcp` is a Model Context Protocol server for an AI assistant whose
client supports MCP, with or without a shell. It connects to Casso through the
debug channel only, and can launch a Casso with its debugger open. Its setup and
its tools are in
[DebuggerForAgents.md](DebuggerForAgents.md#through-an-mcp-client).

## Colors and keys

### Themes

**Tools > Theme** sets the debugger's theme: Same as Casso, Casso Skeuomorphic,
Casso Dark Modern, Casso Retro Terminal, System light or System dark. The theme
under the menu's highlight is previewed until the menu closes. The theme is the
same for every project.

### Color keys

Every pane whose colors mean something has an info button (an i in a circle)
at the end of its toolbar; the call stack and a device panel with a memory
map or a disk head, which have no toolbar, have a band for it. Resting the
pointer on the button displays that pane's color key under it: a line for each
color, its swatch beside its meaning, in the theme in force. The key goes away
when the pointer moves off. Clicking the button, or tabbing to it and pressing
Space or Enter, pins the key open until the button is pressed again, Escape is
pressed, the focus moves on, or a click lands elsewhere. Hovering over any of
these colors in the window brings up the same meaning as a tip; where the
element already has a tip, such as a symbol or the flags, the meaning is added
to it. The colors of syntax (mnemonics, directives, labels, numbers, strings
and addresses) mean what the text is and are not listed.

- **Disassembly**: the PC's yellow arrow and row, the next instruction to run,
  whose arrow can be dragged to set the next statement; a green row, the line
  brought into view by Go to, the call stack or a branch arrow; a row in the
  accent color, where the branch at the PC goes; the branch arrows (see
  [branch arrows](#branch-arrows)); a red dot for a breakpoint, a red ring for
  a disabled one, and a gray dot where a click sets one; bytes in the changed
  color, changed since the last stop; the operand in the comment green, what
  the instruction reads; and `Result: `, its result, in the result
  color.
- **Source**: the PC's row, a red dot for a breakpoint and a red ring for a
  disabled one, and `Result: ` in the result color.
- **Registers, watch and stack**: values in the changed color, changed since
  the last stop; in the watch pane, a dimmed automatic watch, touched by the
  previous instruction, and a dimmed watch, disabled.
- **Call stack**: a dimmed frame, unverified, which may no longer be a live
  call; the dimmed last return, a call that has returned and exited the stack.
- **Memory**: bytes in the changed color; ROM bytes and I/O bytes in their
  colors, with the boxes around them; bytes that no read has gotten to yet in the
  muted color; and the boxes around the Language Card's banks and aux RAM.
- **Memory map**: main RAM, aux RAM, each Language Card bank, ROM, slot ROM and
  I/O, each in a color of the theme's, with a key below the map.
- **Heat map**: code in the disassembly's instruction color, an operand byte
  dimmer, reads in green and writes in red; with Blend on, magenta for
  self-modifying code; a cell tinted toward the symbol color for an address
  with a symbol; outlines in the PC's yellow for the PC, cyan for the stack
  pointer, the breakpoint red for a breakpoint, green for a read data
  breakpoint and orange for a write data breakpoint; violet for a read before
  written, and rose for a write that changed a value in the **Changed** view.
- **Disk head**: the head muted while the motor is off, flashing as it steps,
  and settling to the accent on its track; a lamp lit while its phase magnet
  or the motor is on.
- **Breakpoints**: a red dot for a breakpoint and a red ring for a disabled
  one.
- **Status bar**: the history meter, green while the buffer is empty, blending
  to blue as it fills; its tip has the same meaning, and it has no info button.

### Key schemes

**Tools > Keyboard scheme** chooses Visual Studio (the default), AppleWin or
GSSquared. Each command's key in the scheme in force is beside it on the menus
and in the command bar's tips.

| Command | Visual Studio | AppleWin | GSSquared |
|---|---|---|---|
| Run | F5 | Enter | Enter |
| Break | Shift+F5, Ctrl+Break | Shift+F5 | Shift+F5 |
| Step into | F11 | Space | Space |
| Step over | F10 | Ctrl+Space | O |
| Step out | Shift+F11 | Shift+Space | R |
| Run to cursor | Ctrl+F10 | Ctrl+Down | Ctrl+F10 |
| Toggle breakpoint | F9 | F9 | F9 |
| Run one frame | F6 | F6 | F6 |
| Step back into | Alt+F11, or Ctrl+R, F11 | Alt+Space | Alt+Space |
| Step back over | Alt+F10, or Ctrl+R, F10 | Ctrl+Alt+Space | Alt+O |
| Step back out | Alt+Shift+F11, or Ctrl+R, Shift+F11 | Alt+Shift+Space | Alt+R |
| Reverse continue | Alt+F5 | Alt+Enter | Alt+Enter |
| Find, find next, find previous | Ctrl+F, F3, Shift+F3 | Ctrl+F, F3, Shift+F3 | Ctrl+F, F3, Shift+F3 |

In every scheme a reverse command's key is its forward command's key with Alt
added. In the AppleWin and GSSquared schemes the debugger window takes
Alt+Space and Alt+Enter, so they do not open the window menu. In every scheme,
Ctrl+Shift+R resets the machine, Ctrl+Shift+P power-cycles it, and Pause runs
or pauses it, as in the emulator window.

Every scheme has the same keys to open a pane, or bring it forward, and focus
it, and the View menu has each pane's key beside it. They are Visual Studio's
where it has the window, and keys of the same kind where it does not. A chord's
second key may be pressed with the first key's Ctrl and Alt down.

| Pane | Key |
|---|---|
| Disassembly | Ctrl+Alt+D |
| Breakpoints | Ctrl+Alt+B |
| Call stack | Ctrl+Alt+C |
| Watches | Ctrl+Alt+W, 1 |
| Memory 1 to 4 | Ctrl+Alt+M, 1 to 4 |
| Registers | Ctrl+Alt+G |
| Console | Ctrl+\` or Ctrl+Alt+I |
| Stack | Ctrl+Alt+K |
| Trace | Ctrl+Alt+T |
| Heat map | Ctrl+Alt+H |
| Heat map ranges | Ctrl+Alt+R |
| Disk II log, Input log | Ctrl+Alt+L, D and Ctrl+Alt+L, I |
| BASIC program | Ctrl+Alt+P, P |

Bookmarks have Visual Studio's Ctrl+K chords in every scheme; see
[bookmarks](#bookmarks). In GSSquared mode, with the command line empty, Space
and F10 step and Return runs, whatever the scheme; see
[GSSquared mode](#gssquared-mode).
