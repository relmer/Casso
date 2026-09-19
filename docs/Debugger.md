# The Casso debugger

Casso's debugger stops a running Apple II program, shows and changes its
memory and registers, and runs it again under your control. One engine sits
behind three ways in, and every command works the same in each.

## Ways in

| Way in | How | Best for |
|---|---|---|
| Batch | `CassoCli debug --machine Apple2e --script steps.txt` | repeatable scripts, tests, CI |
| Debugger window | **Debug > Debugger...**, or start Casso with `--debugger` | debugging by hand beside the emulator |
| Debug channel | `CassoCli debug --attach <pid>`, or your own client on the named pipe | tools, editors, attaching to a running program |

The window and the channel share one session: a breakpoint set in either shows
up in the other. The channel is documented in [DebugChannel.md](DebugChannel.md).

## Command modes

The debugger reads each line in one of four modes.

- **AppleWin** (the default) uses AppleWin's debugger command names: `BP`, `G`,
  `T`, `D`, `U`, `SYM` and so on. Values may be expressions and symbols.
- **Monitor** uses the Apple II Monitor's own syntax: `300.30F`,
  `300: A9 41`, `300L`, `300G`, `^E`. Values are plain hex digits; there are
  no expressions or symbols. Control-key commands are typed as `^` and a letter.
- **GSSquared** uses the GSSquared emulator's debugger commands, described
  below.
- **WinDbg** is WinDbg-flavored: the WinDbg commands a 6502 session uses, with
  WinDbg's arguments and layouts, described below. It is not WinDbg; the rest
  of WinDbg has no meaning on this machine and says so.

Switch with `MODE MONITOR`, `MODE GSSQUARED`, `MODE WINDBG` and
`MODE APPLEWIN`; `MODE` alone
shows the current mode. Batch mode starts in the mode `--mode` gives.

### The `/` prefix

In Monitor mode, a line starting with `/` is read as an AppleWin line. This
reaches everything the Monitor has no command for:

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
| `bp addr`, `bp first.last`, `bp` | set an execution breakpoint, or list the breakpoints and watchpoints |
| `bpd addr r\|w\|rw`, `bpi addr r\|w\|rw` | stop on a read, a write or either; `bpi` takes an I/O address, $C000-$C0FF |
| `nobp id`, `nobp addr` | clear a breakpoint by id, or the execution breakpoint at an address |
| `watch addr`, `watch first.last`, `watch`, `nowatch id` | add, list and clear watches; a range adds one watch per address |
| `load "file" addr`, `save "file" first.last` | load and save a binary, as `BLOAD` and `BSAVE` |
| `sload "file"`, `slookup addr`, `sclear` | load, look up and clear symbols |
| `s`, `o`, `r`, `g` | step into, step over, step out, run |

- Addresses are hex with no prefix, and names are case-insensitive. `bp`,
  `bpd` and `bpi` take a trailing `IF expression`, as in AppleWin mode.
- An address may carry a bank, as on a IIgs: `00/300` is $0300. Any other bank
  is refused, since only bank 00 exists on these machines.
- `m`, `x`, `map`, `video` and `novideo` are IIgs commands, and `debug` and
  `nodebug` open device panels; these reply that they are not available.
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
| `r`, `r a=41` | show or set registers (`a`, `x`, `y`, `sp`, `pc`, `fl`) | `R` |
| `u [addr]`, `x pattern`, `k` | disassemble, find symbols (`*` and `?` match), call stack | `U`, `SYM`, `CALLS` |
| `? expr`, `.formats value` | evaluate | `CALC` |
| `l+s`, `l-s`, `lsa` | step by source line on, off; the line at PC | `SRC` |

- A bare number is hex; `0x300` and `$300` are the same, and `0n10` is
  decimal. A length is `l` and a count in the command's own units.
- The prompt is `0:000>`. A stop prints AppleWin's stop line, then `r`'s.
- Threads, modules, exceptions, the kernel, dump files, types and scripting
  (`~`, `lm`, `sxe`, `!pte`, `.dump`, `dt`, `.foreach` and the like) reply
  `Error: no meaning on this machine` with the family. `wt`, `s -a`, `s -b`
  and `lsa file:line` are not available yet.

### Output format

Replies are written in an output format of their own: AppleWin, Monitor,
GSSquared or WinDbg. `MODE` sets the format along with the mode; `OUTPUT name` then
changes the format alone, and `OUTPUT` shows it. Batch mode takes `--output`.
A reply a format has no layout for is written as AppleWin writes it, and
GSSquared has no stop line of its own, so in its format a stop reads as in
AppleWin's.

## Getting help

`HELP` (or `?`) lists every command by family. `HELP name` describes one, and
says whether it is an alias, needs the debugger window, or is not available.

## Casso engine commands

These are Casso's own, alongside AppleWin's names. Each mode reaches them
through its own marker:

| Mode | Marker | Example |
|---|---|---|
| AppleWin | the bare name | `SWITCHES` |
| Monitor | `/` | `/switches` |
| GSSquared | the bare name; `/` is its bank separator | `switches` |
| WinDbg | `!` | `!switches` |

| Command | Effect |
|---|---|
| `MODE [APPLEWIN\|MONITOR\|GSSQUARED\|WINDBG]` | shows or sets the command mode, and sets the output format to match |
| `OUTPUT [APPLEWIN\|MONITOR\|GSSQUARED\|WINDBG]` | shows or sets the output format alone |
| `PAUSE` | stops a running machine |
| `BUDGET n` | limits every later run to `n` cycles (decimal); `BUDGET 0` removes the limit |
| `SWITCHES` | lists the soft switches and whether each is on |
| `STACK` | shows the stack pointer and the stack page above it |
| `PATCH addr value...` | writes bytes the way a memory window edit does: into RAM, or into ROM so the machine runs the patched code; it will not write an I/O address, which is what `OUT` is for |

## Running and stopping

| Command | Effect |
|---|---|
| `G` | runs until something stops it |
| `G addr` | runs until the PC reaches `addr` |
| `addrG` | sets the PC to `addr`, then runs |
| `T [n]` | steps into `n` instructions (default 1) |
| `P [n]` | steps over `n` instructions, running a `JSR` to its return |
| `RTS [n]` | steps out of `n` subroutines |

A stop is reported with its reason and address, for example
`Breakpoint #0 at $FDED` or `Step at $0302`. In the Monitor output format a stop prints the
register line instead.

Breakpoints (`BP`, `BPM`, `BPMR`, `BPMW`, `BPR`, `BPL`, `BPC`, `BPD`, `BPE`),
watches (`W`, `WL`, `WC`) and the rest of AppleWin's commands are listed by
`HELP`.

## Symbols

Symbol tables give addresses names in disassembly and in expressions. Casso
builds the Monitor, Applesoft, DOS 3.3 and ProDOS tables in, adding the
//e and //c memory soft switches on those machines.

```
SYM LOAD "game.dbg"       load into the User table
SYMUSER2 LOAD "lib.sym",4000   load another table, with a hex offset
SYM COUT                  look a name up in every enabled table
SYM FDED                  look an address up
SYM                       count the symbols in each table
```

`SYM` on its own acts on the User table for `LOAD`, `SAVE`, `CLEAR`, `ON`,
`OFF`, `name = addr` (add) and `! name` (remove). The other tables use their own
name: `SYMMAIN`, `SYMBASIC`, `SYMUSER2`, `SYMSRC` and so on. Loading replaces
the table's contents.

A symbol file may be in any of these formats, recognized from its contents:

| Format | Lines look like |
|---|---|
| Casso debug file (`CassoCli merlin -g`, `CassoCli as65 -g`) | `COUT=$FDED` |
| Merlin assembly listing (`CassoCli merlin -l`, or Merlin itself) | the symbol table after `SYMBOL TABLE` |
| AppleWin `.SYM` | `FDED COUT` |
| VICE label file | `al FDED .COUT` |

Lines starting with `;` are ignored.

## Loading and saving binaries

```
BLOAD file[,format] [addr[,len]]
BSAVE file addr.last
```

`BLOAD` detects AppleSingle, Intel HEX and Motorola S-record files from their
contents; anything else loads as raw bytes. A DOS 3.3 binary cannot be told
from raw data, so it needs `,DOS`. The file extension is never used.

| Format word | Format | Address |
|---|---|---|
| `RAW` | raw bytes | required |
| `DOS` | DOS 3.3 binary (4-byte address and length header) | from the header, unless you give one |
| `AS` | AppleSingle | from the ProDOS aux type, unless you give one |
| `HEX` | Intel HEX | from the file; giving one is an error |
| `SREC` | Motorola S-record | from the file; giving one is an error |

`BSAVE` and the Monitor's `W` write raw bytes. The Monitor's `R` reads raw
bytes: `300.3FFR "data.bin"`. In batch and on the channel, `R` and `W` need a
file name; the debugger window asks for one when it is missing.

## Batch mode

`CassoCli debug` builds a machine, stops it at power-on, runs the script's lines
and then each `--command` line, and exits after the last one. Nothing waits for
a person, so a batch run gives the same result every time; `--seed` fixes the
power-on memory pattern, so two runs match byte for byte.

```
CassoCli debug --machine Apple2e --script stop.txt
CassoCli debug --machine Apple2e --disk1 game.woz --command "bpmr C000" --command g --json
```

A script holds one command per line. Lines starting with `;` are comments, and
blank lines are skipped except in the line assembler, where a blank line ends
it.

```
; Stop in COUT and show who called it.
BP COUT
G
STACK
```

- Every run carries the `--max-cycles` budget (default 100,000,000), so a
  script that never reaches its breakpoint still ends.
- Guest disk writes go to an in-memory copy unless `--write-disks` is given.
- `--json` prints one JSON record per reply and stop, in the format the debug
  channel uses.

| Exit status | Meaning |
|---|---|
| 0 | every command returned ok or not available |
| 1 | at least one command returned error or unknown |
| 2 | usage error, or the machine or a disk could not be loaded |
| 3 | the last run ended on its cycle budget |

`CassoCli debug --attach <pid>` runs the same kind of script against a running
Casso; see [DebugChannel.md](DebugChannel.md#from-the-command-line).

## Conditional and value breakpoints

`BP`, `BPX`, `BPM`, `BPMR`, `BPMW`, `BPMV` and `BP file:line` take a trailing
`IF` and an expression over registers, symbols and memory, where `*addr` reads
a byte:

```text
BP Loop IF X == 3 & *PTR != 0
BPMW 400 BEFORE IF Y = 2
```

The expression is evaluated only when the address or the access hits. A false
one neither stops the machine nor counts a hit; a true one stops, and the stop
line shows the expression and its value. On a watchpoint, `ACCESS` and `VALUE`
are the address and byte of the access. An expression is checked as it is set,
so an unknown symbol or a read of an I/O address is an error then, not at the
first hit.

`BPMV addr value` stops after a write leaves `addr` holding `value`, and on no
other write.

## Source-level debugging

A debug file maps addresses to source lines. Casso reads cc65's debug format
(`ca65 -g`, `ld65 --dbgfile`), writes it from its own assembler
(`CassoCli as65 -g`), and reads a Merlin 8/16 listing, which becomes its own
source view.

- `SYM LOAD file.dbg` loads the symbols and the line table. The window opens a
  source pane beside the disassembly and follows the PC.
- The source files are looked for beside the debug file, then in the folders
  where sources were found before. When one is not found, the pane says so;
  drag the file onto the debugger and it is matched by its hash, and its
  folder remembered. A file whose hash differs from the one recorded opens
  with a warning.
- `SRC` shows the current file and line. `SRC ON` makes every step command,
  in every mode, step by source line; `SRC OFF` steps by instruction. In the
  window, the pane with the focus decides: the source pane steps by line, the
  disassembly by instruction.
- `BP file:line` sets a breakpoint on the first instruction of a line, at
  each place a macro body line was expanded.

## The call stack

`CALLS` shows the chain of calls to the PC, innermost first. Each frame has
its call site, the routine it entered, and how it was found:

- **Recorded** frames come from watching `JSR`, `BRK` and interrupts go by
  while the debugger is open, and `RTS` and `RTI` end them.
- **Guessed** frames come from walking the stack page for return addresses
  whose `JSR` is two bytes below; with a debug file loaded, only calls to
  known routines count.

`CALLS MODE RECORDED|WALK|HYBRID` chooses the mechanism. Hybrid, the default,
uses the recorded frames and extends below them with the walk.

A program that manages its own stack breaks the chain, and the call stack
says where rather than guessing past it: a `TXS`, a pull into a return
address, a frame left by a jump, a return to somewhere other than its call
site, the stack wrapping, a reset, and the point where recording began. A
routine that returns a few bytes past its call site, over inline parameters
the way ProDOS's MLI does, is noted and not treated as a break.

## The step filter

`SKIP name`, `SKIP addr` or `SKIP first.last` adds a routine to the step
filter: a step into its `JSR` runs the call and stops after it, so stepping
never lands inside `COUT` and its like. `SKIP` lists the filter, `SKIP - name`
removes an entry and `SKIP CLEAR` empties it.

## The instruction trace

`HISTORY ON` records every instruction the machine runs, keeping the newest
100,000: the cycle count, the registers before it, and the memory access it
made, with symbols. `HISTORY OFF` stops recording and keeps what it has.

```text
HISTORY               the newest 20 entries
HISTORY 5000 40       40 entries from entry 5000 (decimal)
HISTORY SAVE run.txt  every retained entry
```

The trace costs nothing while it is off.

## Profiling

`PROFILE ON` counts every instruction a debugger-driven run executes, and
`PROFILE OFF` stops. `PROFILE LIST` shows the count, cycles and share for each
mnemonic and addressing mode, and the penalty cycles apart: page crossings on
indexed reads, taken branches, and branches that cross a page. `PROFILE LIST
ADDR` shows the twenty hottest addresses; `PROFILE SAVE file` writes both
tables; `PROFILE RESET` clears them.
## The debugger window

**Debug > Debugger...** opens the window beside the emulator; `--debugger` opens
it at start. It shows the code around the PC, the registers and flags,
breakpoints, watches, the stack and memory, with a command box and console
below the code.

- **Step**, **Step Over**, **Run**, **Run to Cursor** and **Pause** run the
  machine. **Follow PC** returns the code pane to the PC.
- Double-clicking a code line sets or clears its breakpoint. Double-clicking a
  breakpoint clears it.
- The memory box moves the memory pane; the poke box takes an address and a
  byte, such as `0300 A9`.
- The command box runs any command in the current mode, with the same reply
  batch mode gives.

AppleWin's window commands work in the command box: `.` returns to the PC,
`RET` goes to the return address on the stack, `^` and `V` move one
instruction, `PAGEUP` and `PAGEDN` a pane, and `MD1 addr` (or `MA1`, `MT1`,
and the `2` forms) moves the memory pane. The window shows every pane at once,
so the layout commands (`CODE`, `DATA`, `WIN`) change nothing. The screen
views (`TEXT`, `HGR` and their forms) and the appearance commands (`BW`,
`COLOR`, `FONT`) are not available; the emulator window shows the screen.

Closing the window closes the debug channel. Reopening it keeps the breakpoints.

### Panes and docking

Every pane can be moved. Drag a tab onto the drop zones that appear to dock it
beside another pane, against an edge of the window, or as a tab of another
group; drag a divider to resize. A tab dragged out of the window floats in a
window of its own; closing that window docks it back. Right-click a pane, or
press Shift+F10, for **Dock To**, which offers each edge, each group, Float and
Auto Hide; Alt+Shift with an arrow key moves the focused pane. An auto-hidden
pane is a tab on the window's edge that slides the pane out on a hover or a
click, and back when you click elsewhere; a tab marks new output it has not
shown. The arrangement, including floating windows and the monitor each is
on, is kept between sessions.

The panes are the disassembly, the source, the console, registers,
breakpoints, watches, the stack, the call stack, the trace, up to four memory
windows, and one panel for each device the machine has.

### Memory windows

**+ Memory** opens another memory window, up to four; **- Memory** closes the
last. Type over the hex or the text to edit memory, in RAM or in ROM; Ctrl+Z
undoes the last edit in that window. **Bytes** cycles the grouping through
bytes, words and longs.

### Device panels

**Panels** lists the machine's devices: the Disk II controller, the //e MMU
and its memory map, the video switches, the keyboard, the Mockingboard, the
printer card and the clock. Each opens as a pane showing its registers and
state, with the disk head position, the memory map, or level meters where the
device has them. `PANEL LIST`, `PANEL name` and `PANEL CLOSE name` do the same
from the command box.