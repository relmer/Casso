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

The debugger reads each line in one of five modes.

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
- **Casso** is Casso's complete native command set: every command Casso has,
  AppleWin's names and Casso's own, by name, with replies in AppleWin's
  format.

Switch with `MODE MONITOR`, `MODE GSSQUARED`, `MODE WINDBG`, `MODE CASSO`
and `MODE APPLEWIN`, or from the **Dialect** drop-down on the console's toolbar; `MODE` alone shows the current
mode. Batch mode starts in the mode `--mode` gives.

### The `/` prefix

In Monitor mode, a line starting with `/` is read as a Casso command, so it
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
- `map` shows the current machine's memory map, as `MAP` does. `m`, `x`,
  `video` and `novideo` are IIgs commands; these reply that they are not
  available. In the debugger window, `debug` lists the device
  panels, `debug "name"` opens one and `nodebug "name"` closes it, as `PANEL`
  does.
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

Each mode's help command (`HELP` or `?`, `/HELP` in Monitor, `help` in
GSSquared, `.help` in WinDbg) alone lists the help sections and how to ask for
each: `help breakpoints`, `help memory` and so on list one section, and `help
all` lists everything that can be typed in that mode, grouped by
category and alphabetical within each, syntax on the left and what it does on
the right: the mode's own commands first, then the Casso commands it reaches,
written as they are typed there. A Casso command the mode has its own form of
is not repeated, and one whose name the mode already uses -- GSSquared's `r`,
or any hex word, which GSSquared reads as an address -- is not listed. Asking
help for one command describes it, or says which modes run it; a word that is
both a command and a section is described as the command, with a line on how
to ask for the section. Any other text is a search: every command whose syntax
or description contains it, ignoring case, with `*` and `?` as wildcards
(`help bp*`) or a regular expression between slashes (`help /^bp[de]/`).
[Debugger-Commands.md](Debugger-Commands.md) holds every mode's `help all`,
generated from the same table.

### Mistyped commands and confirmations

A word that is another mode's command is answered with that mode's title and
the current mode's equivalent, with the same arguments: in WinDbg mode, `nobp 0`
says "nobp is a GSSquared command. WinDbg's is bc 0." A word several modes use
belongs to the first of AppleWin, Casso, Monitor, GSSquared and WinDbg that has
it. When the current mode has no equivalent the reply says so. A word no mode
has is answered with the closest command by spelling. The suggested line shows
in gray after the caret in the command box; Tab runs nothing but takes it into
the box, and any other key drops it. A command given the wrong arguments is
answered with its syntax line.

The command box completes as PowerShell does. Tab completes the command word
from the current mode's commands, and pressing it again steps through the
other matches; Shift+Tab steps back. The newest earlier line that starts with
what is typed shows in gray, and Right arrow at the end of the line takes it.
F8 steps back through the earlier lines that start with what is typed, and F7
lists the earlier lines to pick from.

Every command that changes breakpoints, watches, memory, registers, flags,
symbols, disks or settings prints one line saying what changed, such as
"Removed breakpoint 3 (exec C000)" or "Removed watch 0 (0400)".

## Casso commands

Every mode reaches Casso's commands -- AppleWin's names and Casso's own --
through its own marker, unless the mode uses the name for something else:

| Mode | Marker | Example |
|---|---|---|
| AppleWin, Casso | the bare name | `SWITCHES`, `DISK` |
| Monitor | `/` | `/switches`, `/disk` |
| GSSquared | the bare name; `/` is its bank separator | `switches`, `disk` |
| WinDbg | `!` | `!switches`, `!disk` |

These are Casso's own additions to AppleWin's set:

| Command | Effect |
|---|---|
| `MODE [APPLEWIN\|MONITOR\|GSSQUARED\|WINDBG\|CASSO]` | shows or sets the command mode, and sets the output format to match |
| `OUTPUT [APPLEWIN\|MONITOR\|GSSQUARED\|WINDBG]` | shows or sets the output format alone |
| `PAUSE` | stops a running machine |
| `BUDGET n` | limits every later run to `n` cycles (decimal); `BUDGET 0` removes the limit |
| `SWITCHES` | lists the soft switches and whether each is on |
| `MAP` | shows, for each address range, where reads and writes go now -- main or auxiliary RAM, ROM, language card bank 1 or 2, slot or internal ROM -- as the language card, auxiliary memory and ROM switches set them |
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
| `FRAME [n]` | runs `n` video frames (default 1), 17,030 cycles each |

A stop is reported with its reason and address, for example
`Breakpoint #0 at $FDED` or `Step at $0302`. In the Monitor output format a stop prints the
register line instead.

In Monitor mode, `addrG` first pushes the Monitor's return address ($FF69), as
the Monitor's own `G` does, so a program that ends in `RTS` comes back and
stops there as a run to, rather than running on into the ROM. That stop stays
armed until the program reaches it, through any breakpoint or step along the
way; another Monitor `addrG`, a reset or a machine change replaces or clears it.

In Monitor mode, `addrS` steps one instruction from `addr`, and a bare `S`
steps one instruction from the current PC, as the Monitor does after the
first `S`.

Breakpoints (`BP`, `BPM`, `BPMR`, `BPMW`, `BPR`, `BPL`, `BPC`, `BPD`, `BPE`),
watches (`W`, `WL`, `WC`) and the rest of AppleWin's commands are listed by
`HELP`.

## The video beam

`VIDEOINFO` shows where the beam is: the scanline (0 to 261) and the cycle within it (0 to 64), in hex. The status bar shows the same, in decimal and then in hex. A scanline draws in its last 40 cycles; the 25 before them are horizontal blank, and scanlines 192 to 261 are vertical blank.

| Command | Effect |
|---|---|
| `BPBEAM line cycle` | stops when the beam reaches that scanline and cycle |
| `BPBEAM VBL` | stops where vertical blank starts, scanline `C0` |
| `BPV line[,len]` | stops when the beam enters a scanline |
| `FRAME [n]` | runs until the beam comes back to where it is, `n` times |

An instruction takes several cycles, so `BPBEAM` stops at the first instruction at or past the place it was given. It fires once and then clears, as `BPV` does, and `BPC *` clears it. `FRAME` stops early when anything else stops the machine first, and its frame break goes with that stop.

**Debug > Run one frame**, or F6 in every key scheme, runs `FRAME` in Casso mode whatever the console's mode. **Debug > Show beam on screen** marks the beam on the emulator's screen while the machine is stopped: its scanline is a red line across the picture, and the cycle it is on a bar across that line. A beam in horizontal blank is marked at the left edge, and one in vertical blank along the bottom edge.

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

An equate (`WIDTH = 40`) loads as a constant rather than a label. A constant
works in expressions like any symbol, and `SYM` and `SYMLIST` mark it
`constant`, but it is never shown in place of a number: `LDA #$28` and
`LDA $28` stay as they are in the disassembly, and an address never looks up
to a constant. A load offset moves labels and leaves constants alone. A cc65
debug file marks its equates (`type=equ`), and `SYM SAVE` writes constants
under a `; constants` heading so they load back as constants. AppleWin,
VICE and Merlin symbol tables do not tell an equate from a label, so
everything in them loads as a label.

## Loading and saving binaries

```
BLOAD file[,format] [addr[,len]]
BSAVE file addr.last
```

`BLOAD` detects AppleSingle, Intel HEX and Motorola S-record files from their
contents; anything else loads as raw bytes. A DOS 3.3 binary cannot be told
from raw data, so it needs `,DOS`. The file extension is never used.

The reply gives the byte count and the address, such as `Loaded 17 bytes at
$0300`. A load that would stop short loads nothing and is an error: a file
smaller than the length you give, or one that would run past `$FFFF`. When a
debug file (`.dbg`) or, failing that, a symbol file (`.sym`) sits beside the
binary under the same base name, `BLOAD` loads it too, as `SYM LOAD` would,
and says so in one more line.

Every file command's reply gives the file's path only when you typed it
relative, and then as the absolute path it resolved to.

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

## Breaking on opcodes

`BRK ON` stops on the `BRK` instruction, and `BRKOP op` on any opcode. `BRK 1
ON`, `BRK 2 ON` and `BRK 3 ON` add an opcode breakpoint for each undocumented
opcode of that instruction length on the machine's CPU, `BRK ALL ON` adds all
of them plus `BRK`, and `BRK` alone reports which lengths are on. The length is
the one the CPU executes:

- **6502**: the stable undocumented opcodes (`SLO`, `LAX`, the multi-byte
  `NOP`s and the rest) take the length of their addressing mode, and the
  unstable ones (`ANE`, `SHX`, `LAS` and the rest) the length of their operand.
  Only the twelve `JAM` opcodes, which halt the CPU, are one byte.
- **65C02**: every opcode is defined, and the undocumented ones are `NOP`s: 32
  one-byte, eleven two-byte and three three-byte (`$5C`, `$DC`, `$FC`).

## Source-level debugging

A debug file maps addresses to source lines. Casso reads cc65's debug format
(`ca65 -g`, `ld65 --dbgfile`), writes it from its own assembler
(`CassoCli as65 -g`), and reads a Merlin 8/16 listing, which becomes its own
source view.

- `SYM LOAD file.dbg` loads the symbols and the line table. The window opens a
  source pane beside the disassembly and follows the PC.
- Dropping a debug or symbol file (`.dbg`, `.sym`, `.lbl`, `.vs`) on the
  debugger window, or on one of its floating panes, loads it as `SYM LOAD`
  would. A dropped source (`.a65`, `.s`, `.asm`, `.inc` and the like, Merlin's
  `.S` among them) opens in a document of its own, as File > Open source
  file does, with or without a debug file loaded. Any other file loads as
  symbols when it reads as a symbol file; otherwise it is shown as text, or
  as a hex dump when it is binary.
- Opening a source with no debug file loaded first loads a `.dbg` or `.sym`
  of the same name beside it. A source with no symbols shows a banner with a
  Load symbols button, which picks the debug or symbol file to load; the
  source is matched against it once it loads.
- Each disassembly view's toolbar has check boxes for its viewing options,
  as Visual Studio's: Show address, Show code bytes, Show source code, Show
  symbol names and Show line numbers. With source shown, each source line sits
  on a row above the code it produced. Source and line numbers are offered once
  a debug file maps the code. All start checked; the choices hold for every
  view and are saved. Show source code's tip says how to get source while
  there is none, and Show symbol names' tip lists where the symbols came from.
- Hovering a disassembly view's breakpoint column shows a gray breakpoint on
  an instruction's row, where a click sets one.
- While paused, dragging the PC's yellow arrow to another instruction sets the
  PC there without running anything, as Set next statement on a row's context
  menu does.
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
- Source is colored with the grammar of the assembler that wrote it: as65,
  Merlin or ca65, chosen from the file's directives, labels and origin. Text
  that shows none of them, or as many of one as another, is read as as65's;
  the file's extension plays no part. The Syntax entry on a document's tab
  menu sets it for that document.
- A Merlin or ca65 listing shown as source keeps its address and byte
  columns, colored as an address and bytes, ahead of its colored source.
- While the machine is stopped, the instructions each line in the PC's files
  produced are listed on rows below it, in syntax colors faded halfway toward
  the background. A line of
  data, such as `.byte` or `HEX`, has none.
- The operand and result column shows a result after `Result: `, in cyan.

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
the way ProDOS's MLI does, is noted and not treated as a break. A store into
a return address marks its frame when it runs, naming the store.

Recording normally starts when the debugger opens, so calls made before then
come from the walk. To record everything, start Casso with `--debugger` or
choose Debug > Restart Under Debugger, which power-cycles the machine with the
debugger open: the chain then ends at power-on, and nothing is guessed below
it.

## The step filter

`SKIP name`, `SKIP addr` or `SKIP first.last` adds a routine to the step
filter: a step into its `JSR` runs the call and stops after it, so stepping
never lands inside `COUT` and its like. `SKIP` lists the filter, `SKIP - name`
removes an entry and `SKIP CLEAR` empties it.

An interrupt taken during a step over or a step out runs to its `RTI`
without ending the step, which then goes on from where the interrupt came
in: `P` on a `JSR` with an interrupt pending runs the handler, then the call,
and stops after it. A breakpoint in the handler still stops there. A step
into stops on the handler's first instruction.

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

The window's trace pane lists its keys above its rows. With the pane focused,
`Space`, `O` and `R` step into, over and out, `Return` runs, `T` turns the
trace off and on, `B` shows and hides the bytes column, and `S` saves the
trace to a file you pick, as `HISTORY SAVE` does. While the machine is
stopped, the next few instructions from the PC follow the last entry, taking
each branch the way the current flags send it and ending at one an earlier
instruction may have changed the flags for.

## Profiling

`PROFILE ON` counts every instruction a debugger-driven run executes, and
`PROFILE OFF` stops. `PROFILE LIST` shows the count, cycles and share for each
mnemonic and addressing mode, and the penalty cycles apart: page crossings on
indexed reads, taken branches, and branches that cross a page. `PROFILE LIST
ADDR` shows the twenty hottest addresses; `PROFILE SAVE file` writes both
tables; `PROFILE RESET` clears them.
## The stopwatch

`STOPWATCH start [stop]` times every pass from one address to another in runs
the debugger starts: a lap begins when the instruction at `start` is about to
run and ends when the one at `stop` is, and is the exact cycle count between
the two. Each address is an expression, as `BP` takes one, so `STOPWATCH
DrawRow DrawRow_Exit` times a routine from its entry to its exit. With one
address, each lap is the time from one arrival to the next, such as a game
loop's period. A routine that calls itself is timed from its outermost entry.
A bare `STOPWATCH` shows the laps: their count, the last, the shortest, the
longest and the average. `STOPWATCH RESET` clears the laps, and `STOPWATCH OFF`
stops timing and keeps them.

To time between two breakpoints instead, stop at the first, run `RCC`, run to
the second, and `CYCLES PART` gives the cycles in between.

## Video and sound logs

`VIDEOLOG ON` records each change of video mode -- `TEXT`, `MIXED`, `PAGE2`,
`HIRES`, `80COL`, `DHIRES`, `ALTCHARSET` and `80STORE` -- with the machine's
cycle count and the scanline and cycle within the line the beam was at,
grouped under the frame each fell in. An access that leaves the mode as it was
records nothing. `SOUNDLOG ON` records each speaker toggle and each write to
the Mockingboard's 6522s, with its cycle count and the cycles since the last
entry of its kind; a port B write that clocks a value into an AY register also
records that write, with the register and the value. Both logs follow the
machine whether the debugger started the run or not.

Each log takes `ON`, `OFF`, `CLEAR`, `LIST [n]` (the last `n` entries, 100
unless given; a bare `VIDEOLOG` or `SOUNDLOG` lists) and `SAVE [file]`, which
writes every entry the log holds (`VideoLog.txt` or `SoundLog.txt` unless
given). A log keeps its last 100,000 entries, and the listing gives how many
were recorded in all. The count and beam position are those at the start of
the instruction that made the access. While both logs are off they cost
nothing: the I/O page goes back to the bus's ordinary path.
## The debugger window

**Debug > Debugger...** opens the window beside the emulator, as do F12, F7
(AppleWin's key) and Ctrl+F12 in the emulator window; `--debugger` opens it at
start. Ctrl+F12 is for running Casso under a debugger that takes F12 as its
break key. It shows the code around the PC, the registers and flags,
breakpoints, watches, the stack and memory, with a command box and console
below the code.

- **Step**, **Step over**, **Run**, **Run to cursor** and **Pause** run the
  machine. **Follow PC** returns the code pane to the PC. Run to cursor sends
  Casso's `G address` in Casso mode, whatever mode the console is in, since
  GSSquared's `g` takes no address.
- Double-clicking a code line sets or clears its breakpoint. Double-clicking a
  breakpoint clears it.
- The Address box moves the memory pane. Type over the bytes, or use a write
  command at the console such as AppleWin's `MEB 0300 A9`, to write memory.
- The command box runs any command in the current mode, with the same reply
  batch mode gives.

AppleWin's window commands work in the command box: `.` returns to the PC,
`RET` goes to the return address on the stack, `^` and `V` move one
instruction, `PAGEUP` and `PAGEDN` a pane, and `MD1 addr` (or `MA1`, `MT1`)
moves the first memory window and `MD2`, `MA2` and `MT2` the second; the
address is an expression. The window shows every pane at once,
so the layout commands (`CODE`, `DATA`, `WIN`) change nothing. The screen
views (`TEXT`, `HGR` and their forms) and the appearance commands (`BW`,
`COLOR`, `FONT`) are not available; the emulator window shows the screen.

Closing the window closes the debug channel. Reopening it keeps the breakpoints.

Ctrl+F opens a find bar over the pane with the keys -- the console or a source
document -- as Visual Studio Code's does, and over the console from any other
pane. One pane has the bar at a time. The bar holds the
text to find, then toggles for match case (Aa), match whole word (ab) and
regular expression (.*), the match's place among all of them ("2 of 5"), up
and down arrows for the previous and next match, and a close button. With no
toggle on, the text is found as a plain substring, ignoring case. Enter and F3
find the next match, Shift+Enter and Shift+F3 the one before, and Escape
closes the bar.

### Menus and toolbars

The menu bar holds **File** (open a source file, open a symbol or debug file,
load and save breakpoints, save the trace), **Edit** (copy, select all, find,
find next and find previous), **View** (every pane, the device panels, and
**Reset window layout**), **Debug** (run, break,
detach, the steps, run to cursor, run one frame, show beam on screen, show next statement, step by source
line and trace, then reset, power cycle and restart under debugger) and **Tools**
(**Keyboard scheme**). The command bar below it holds the run and step buttons as icons,
each tip giving the command and its key in the scheme in force, and Trace.
A command with two keys shows both in its tip and menu row, for example
"Alt+F11 or Ctrl+R, F11".
In every scheme a reverse command's key is its forward command's key with Alt
added. The Visual Studio scheme steps back into, over and out with Alt+F11,
Alt+F10 and Alt+Shift+F11, also takes Visual Studio's Ctrl+R chords (Ctrl+R,
F11 and so on), runs backward (reverse continue) with Alt+F5, and pauses with
Ctrl+Break as well as Shift+F5. The AppleWin scheme uses Alt+Space,
Ctrl+Alt+Space, Alt+Shift+Space and Alt+Enter; the GSSquared scheme uses
Alt+Space, Alt+O, Alt+R and Alt+Enter. In those two schemes the debugger window
keeps Alt+Space and Alt+Enter, so they do not open the window menu.
Keys and the mouse button let go of while the machine is behind live are
released in the machine when it goes live again; a key still held stays down.
Drag its grab handle to move it along its edge or, pulled well away, to float
it in a window of its own, icons alone. Dragged back over any edge of the
window, a floating bar snaps into that edge, which makes room for it, and
slides along it with the pointer until the button is let go. Its place is
kept between sessions.
**Detach** closes the debugger and leaves the machine running, resuming it
first if it is stopped.
The console has a toolbar of its own with **Dialect**, which sets the words
the console and the memory Address box read.

### The status bar

A status bar runs along the bottom of the window. From the right:

- **Text size**, as a percentage. Clicking it opens a slider from 50% to 300%
  in steps of 10%, the same size Ctrl+Plus, Ctrl+Minus and Ctrl+wheel set.
  A click anywhere else, or Escape, closes the slider; the arrow keys move it
  while it is open. The size is saved and comes back the next time the
  debugger opens.
- **History full**, how much of the reverse-execution memory budget the
  recorded history holds, with a meter that is green while empty and blends
  smoothly to blue as it fills. A full budget is not a fault: the oldest
  snapshots make room for new ones. It reads "History off" while history is
  not recorded.
- **Begins at**, the emulated time since the machine started at which the
  recorded history begins. Once the budget is full the oldest snapshots are
  dropped to make room, so this time moves forward as the machine runs.
- **Scanline:cycle**, where the video beam is, in decimal and then in hex
  as `VIDEOINFO` gives it, for example `192:1 ($0C0:01)`. Clicking it turns
  **Show beam on screen** on and off.
- At the left, **Replaying history** while a reverse command or a seek is
  replaying recorded history to reach its point. Once a reverse run, a step
  back over or a step back out has searched for longer than about 200 ms, the
  note adds how much of the history it has covered. Escape or Pause stops
  the search, and the machine stays at the last position it reached, with
  **Stopped** in the history band. The window keeps answering throughout;
  commands typed meanwhile run once the replay ends.

### Panes and docking

Every pane can be moved. Drag a tab onto the drop zones that appear to dock it
beside another pane, against an edge of the window, or as a tab of another
group; drag a divider to resize. A tab dragged out of the window floats in a
window of its own. Every pane, docked or floating, has a close button; the
**View** menu lists every pane and shows the chosen one where it was,
docked or floating. Right-click a pane, or
press Shift+F10, for **Dock to**, which offers each edge, each group, Float and
Auto hide; Alt+Shift with an arrow key moves the focused pane. The pin on a
pane's title bar hides it too. An auto-hidden pane is a tab on the window's
edge: click it and the pane slides out over the others, click it again or
anywhere else and it slides back; a tab marks new output it has not shown. A
slid-out pane has a title bar of its own: its pin docks the pane back where it
was, and dragging it docks the pane wherever it is dropped. The arrangement,
including floating windows and the monitor each is on, is kept between
sessions.

The panes are the disassembly, the source, the console, registers,
breakpoints, watches, the stack, the call stack, the trace, the heat map, up to
four memory windows, and one panel for each device the machine has.

### The heat map

**View > Heat map** opens a pane that shows the whole 64 KB address space as
a square: one row a page, $00 at the top and $FF at the bottom, and one
dot an address. Each address is drawn in the color of what touched it over the
last few frames, brighter the more it was touched, and fades back to the
background within about a second after the program stops touching it:
code (the bytes of every instruction run) in the disassembly's instruction
color, reads in green and writes in red. **All**, **Code** and **Data** above
the map choose what it shows: everything, with code ahead of data where an
address is both; executes alone; or reads and writes alone. Hovering over the
map shows the address under the mouse and what touched it.

The machine records for the heat map only while its pane is open and in
front. Closed, or behind another tab, it records nothing and costs nothing.

### Memory windows

Each memory window has Visual Studio's bar. The **Address** box takes an
address, a register, a symbol or an expression, and its drop-down lists the
addresses entered before. **Refresh** reads the window's bytes again.
**Columns** sets the values in a row: Auto, 1, 2, 4, 8 or 16. Auto, the
default, fits as many as the window's width holds. **Group by**
shows each value as a byte, a word or a long. View > Memory opens
another window, up to four; a window closes from its tab. What does not fit
a narrow pane moves into the bar's **...** menu. Type over the hex or the
text to edit memory, in RAM or in ROM; Ctrl+Z undoes the last edit in that
window.

### Device panels

**Window > Device panels** lists the machine's devices: the Disk II controller, the //e MMU
and its memory map, the video switches, the keyboard, the Mockingboard, the
printer card and the clock. Each opens as a pane showing its registers and
state, with the disk head position, the memory map, or level meters where the
device has them. `PANEL LIST`, `PANEL name` (or `PANEL OPEN name` or
`PANEL name OPEN`) and `PANEL CLOSE name` (or `PANEL name CLOSE`) do the same
from the command box.
