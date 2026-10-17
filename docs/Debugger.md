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
  `T`, `D`, `U`, `SYM` and so on. Values may be expressions and symbols. A
  bare number is hex; `$` or `#` also marks hex, as in AppleWin, and `0n`
  marks decimal, as in `0n192`. A name is a loaded symbol first, as in AppleWin, then a register,
  then a hex number, so with Applesoft's symbols loaded `FADD` is the routine
  and `$FADD` the number. `@A`, `@X`, `@Y`, `@P`, `@S` and `@PC` are always
  the register. A name resolves the same way in Casso and GSSquared modes.
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

- Addresses and bytes are hex with no prefix, and `0n` marks a decimal one, as
  in `bp 0n768`; ids are decimal and may also carry `0n`. Names are
  case-insensitive. `bp`, `bpd` and `bpi` take a trailing `IF expression`, as in
  AppleWin mode.
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
  decimal. A loaded symbol comes before a number, and `@a`, `@x`, `@y`, `@sp`,
  `@pc` and `@fl` are the registers. A length is `l` and a count in the
  command's own units.
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

The beam is also what a read that drives no data returns. On the ][, ][+ and //e, a read of an unused I/O address or of a display switch ($C050 to $C05F) gives the byte the video scanner fetched from main memory on that cycle, as real hardware does, so a program that polls for a byte it placed in screen memory (vapor lock) finds the scanline it expects. In blanking the scanner still fetches, from the addresses the hardware's counters give, which in horizontal blank are the row's screen holes. The //c keeps returning the last value on the bus.

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

In the debugger window, **File > Load file into memory...**, or its button at
the end of the toolbar, runs `BLOAD` of a file you pick, with the format,
address and length typed in a dialog after it; it is off while the machine
runs. **File > Save memory to file...** runs `BSAVE` of the address and length
typed. The dialogs open with the fields typed last time, and the console has
the command each one ran and its reply.

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
- `--tape file` puts a WAV, AIFF or FLAC recording in the cassette recorder,
  stopped at its start. `TAPE PLAY`, `TAPE STOP`, `TAPE REWIND` and `TAPE EJECT`
  work the transport, `TAPE INSERT file` loads another tape, and `TAPE` reports
  the transport, the place and the length. The //c has no cassette jacks.
- `--json` prints one JSON record per reply and stop, in the format the debug
  channel uses.

| Exit status | Meaning |
|---|---|
| 0 | every command returned ok or not available |
| 1 | at least one command returned error or unknown |
| 2 | usage error, or the machine, a disk or a tape could not be loaded |
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

## Breakpoint name

After `BPNAME 3 "Loop top"`, breakpoint or watchpoint #3 has that name, and
`BPL`, the Breakpoints pane's Name column and the channel use it in place of
the default; `BPNAME 3` with no name returns it to the default. In the pane,
F2, a double-click on the Name cell or Rename on the row's menu edits the name
in place, and Ctrl+Z undoes it as it undoes the pane's other changes. The name
survives `BPEDIT`. `BPSAVE` writes no name, so its script is one AppleWin
reads.

## Debug projects

A debug project, one for each program, has the debugger's configuration:

- breakpoints and the name of each, watches, zero-page pointers, bookmarks,
  data directives, step filters, loaded symbol files and which symbol tables
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

Each breakpoint, watchpoint, watch and bookmark comes back under the number it
had, and the next one you add takes the number it would have taken before.
The key scheme, the theme, the text size and the history settings are yours
rather than a program's, and are the same for every project. The program is
the most specific of these:

1. the debug or symbol file `SYM LOAD` last loaded, by its content rather
   than its path, so moving the file loses nothing;
2. the binary `BLOAD` last loaded, by its content and address;
3. the machine with the disk drive 1 had at its last power-on, by the disk's
   path;
4. the machine.

Projects are saved in `%LOCALAPPDATA%\Casso\DebugProjects` two seconds after a
change and when the debugger is closed. Loading a more specific program that
has a project replaces the debugger's items with that project's; answer Yes to
the prompt that follows to add the items set before as well. A program with no
project yet starts from the items set so far.

File > Save project as... writes the project to a file of your choosing, and
that file is the program's project from then on; File > Open project... opens
one. `PROJECT` lists the open project and its file, `PROJECT SAVEAS file` and
`PROJECT OPEN file` match the two menu items, and `PROJECT MERGE` adds the
items of the project open before the last switch.

File > Project settings... lists the startup commands, each with its command
mode, which run in order each time the project opens. The list is edited in
place, as the heat map's ranges are: the last row, **Add startup command**,
adds one, a click on a command or its mode edits it, a mode left empty is
AppleWin, a row's gripper drags it to another place in the order, and Delete
or the row's menu deletes it. A project file that
cannot be read is copied to the same name with `.unreadable-` and the date
appended, and an empty project opens in its place.

## Breaking on opcodes

`BRK ON` stops on the `BRK` instruction, and `BRKOP op` on any opcode: a byte,
or a mnemonic for every opcode of it on the machine's CPU (`BRKOP JSR` is
`$20`; `BRKOP LDA` is each `LDA`). `BRKOP 00` and `BRKOP BRK` are `BRK ON`. `BRK 1
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

## Disk breakpoints

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
data field after custom address marks. `IF` adds a condition, as on any
breakpoint. `BPL`, `BPC`, `BPD`, `BPE`, `BPCHANGE`, `BPEDIT` and `BPSAVE` treat
a disk breakpoint as they treat any other, and the Breakpoints pane has "Disk
event..." on its New menu.

**The stop.** A disk breakpoint stops at the instruction boundary after the event,
as a watchpoint does; an RWTS or driver breakpoint stops before the routine
runs. The report has the event, the track, sector and volume of the field, the
address and data checksums as stored and as computed, the three nibbles after
the data field, the head's position, the motor, and the addresses of the
instructions that found the address prologue, the data prologue and the
checksum nibble:

```text
Disk breakpoint #3: sector read, track $11 (17) sector $00 volume $FE, drive 1, 16-sector
  address checksum $EF, expected $EF; data checksum $2C, expected $2C; epilogue DE AA EB
  head on track 17 (quarter track $44), motor on; read by $B925, address field found by $B94F, data field by $B8E1
```

A breakpoint or watchpoint that stops at the same boundary has its own reason,
with the disk report after its line.

**Which reads count.** RWTS and the boot ROM search for a sector by reading every
field that passes, so most data fields go by unread. A data field counts as
read when the instruction that read its checksum nibble is not the one that
found its address field: the search loop that passes a field over reads both
in one place, and the read routine reads the data in another. `PASSED` counts
the fields passed over as well.

**Disk values.** Any expression (an `IF`, `CALC`, an address) can read
`DISK.DRIVE`, `DISK.QTRACK`, `DISK.QTRACK1`, `DISK.QTRACK2`, `DISK.MOTOR`,
`DISK.WRITING`, `DISK.PROTECTED`, `DISK.INSERTED`, and `DISK.ATRACK`,
`DISK.ASECTOR` and `DISK.AVOLUME`, the selected drive's last address field
with a good checksum ($FFFF while there is none). They come from the
controller, never through the bus, so reading them changes nothing, and they
take precedence over a program symbol of the same name. On a machine with no
Disk II they are unknown.

```text
BPDISK MOTOR ON IF DISK.QTRACK == 0
CALC DISK.ATRACK
```

**RWTS and driver breakpoints follow their vectors.** DOS 3.3 can be loaded at
other addresses, and a program can patch RWTS or install its own, so
`BPDISK RWTS` stops at the routine the `JMP` at $03D9 leads to, and
`BPDISK DRIVER` at the routine slot 6's device vector has ($BF1C for drive 1,
$BF2C for drive 2) once ProDOS's global page begins with its `JMP`. The
routine is taken again whenever a vector's bytes change. Until the vector
has a target the breakpoint is unresolved, which `BPL` and the pane's Address
column indicate, and it stops on nothing. A call that does not go through the
vector, as DOS's own calls through $B7B5 do, stops at the routine too.

**The head.** Casso's stepper moves the head in half-track steps, so a head
breakpoint matches whole and half tracks only; a `QT` range matches whatever
positions in it the head arrives at.

**Reverse continue** stops on disk breakpoints of every kind, at the boundary a
forward run stops at, including a sector whose reading began before the
snapshot the replay starts from. Replays count no hits.

**Not yet.** Three kinds wait for the disk inspector's preservation work:

- **A nibble sequence with `??` wildcards.** What a protection check reads
  depends on where the latch frames the bits, which the inspector displays and
  searches; built here, it would be a second matcher that ignores framing.
- **A field passing under the head with no CPU read.** It depends on where each
  field lies on the track, which the inspector's analyzer finds.
- **A latch read during the motor's spin-up.** The spin-up window is a
  constant taken from AppleWin's model, while real cards vary up to about three
  times as long, so the breakpoint would stop on an approximation.

## Applesoft

The debugger stops an Applesoft program where the interpreter itself passes:
as a line begins, after a statement changes a variable, and on an error. The
commands are Casso's own: `/bpbasic` in Monitor mode, `bpbasic` in GSSquared
mode and `!bpbasic` in WinDbg mode.

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
- After a line, variable or step stop the PC is $D81D, where the interpreter
  starts each statement; after an error stop it is $D412, before the message
  is printed or `ONERR`'s handler runs.
- The steps and the two lists work only with Applesoft in the $D000 bank. On
  a machine with other code there, they report that Applesoft is not in
  memory.
- `BPL`, `BPC`, `BPD`, `BPE`, `BPCHANGE`, `BPEDIT` and `BPSAVE` treat these
  breakpoints as any other. In the Breakpoints pane their Kind is "BASIC" and
  their Trigger "Line", "Variable change" or "Error", and its New menu has
  "BASIC line...", "BASIC variable change..." and "BASIC error...".
- Reverse continue stops on them where a forward run does. A change made
  before the snapshot a replay starts from, by a statement that ends after
  it, is not found. The BASIC steps run forward only.
- With none set, nothing is tested: the interpreter's pages are watched only
  while a BASIC breakpoint or step is active.

**In the window.** View > BASIC program opens the program in a pane, which
stays open across restarts until closed. The pane also opens by itself at the
line when the machine stops in the interpreter while a program runs, and when
a BASIC line breakpoint is set.

- **BASIC program**, a tab beside the disassembly, lists the program as LIST
  prints it, colored, read from memory as the program changes. The line being
  run has the PC's row color and arrow; its statement is in full color and the
  line's other statements are muted. Enter or a double-click on a row sets a
  line breakpoint there, or clears the one there. With the keys in this pane,
  Step into runs `BASICSTEP`, Step over `BASICLINE` and Step out `BASICOUT`.
- **Watches** lists the variables while the machine is stopped in a running
  program. The Automatic section has the ones the statement being run uses.
  `W` with a variable or array element as Applesoft writes it, such as `A$`,
  `N%` or `X(2,3)`, watches it, as it does a plain name the stopped program
  has, unless a symbol has the name; text typed in the add row is read the
  same way.
  `W BASIC name` always watches the variable. `WL` lists each with its value,
  and the debug project saves them as `WA BASIC name`. The BASIC variables
  heading expands to every simple variable and array element, each value as
  `PRINT` writes it. Applesoft reads only a name's first two
  characters, so `HIGH` and `HI` are one variable, and the characters past the
  second are dimmed.
- **Call stack** lists the active `GOSUB` returns and `FOR` loops first,
  newest first, each with its line; Enter or a double-click brings the line
  into view in the program pane.

At the `]` prompt no line is marked.

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
  the file's extension does not affect the choice among them. The Syntax
  entry on a document's tab menu sets it for that document.
- A `.c` or `.h` file is colored as C: keywords in the mnemonic color, types
  and preprocessor lines in the directive color, and numbers, strings,
  characters and comments in their own colors. A block comment runs over as
  many lines as it spans.
- A Merlin or ca65 listing shown as source keeps its address and byte
  columns, colored as an address and bytes, ahead of its colored source.
- The operand and result column shows a result after `Result: `, in the
  theme's result color: cyan, amber in Retro terminal, and maroon on a light
  theme such as System light.

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
shows where rather than guessing past it: a `TXS`, a pull into a return
address, a frame left by a jump, a return to somewhere other than its call
site, the stack wrapping, a reset, and the point where recording began or
where history starts. A routine that returns a few bytes past its call site,
over inline parameters the way ProDOS's MLI does, is noted and not treated as
a break. A store into a return address marks its frame when it runs, with the
store's address.

Recording starts when the debugger opens. While Casso keeps the machine's
history (Tools > Options), the calls made before then are rebuilt from it
right away: history is replayed on a second machine, so the running machine,
its sound and its picture are left alone, and the record the replay gives
takes over from the one begun when the debugger opened. Until then the call
stack shows "Rebuilding earlier calls from history" and how far along it is.
This first rebuild takes roughly half a second to two seconds for each
minute of machine time it replays, depending on what the program was doing,
and it replays nothing before the last gap, change of disk or power cycle in
history, since the record starts again there anyway.

Every move through history rebuilds the record for wherever the machine
lands, and that is quick: a copy of the record is kept at each of history's
keyframes, by default ten frames of machine time apart, so the rebuild
replays at most from the nearest keyframe at or before the landing. A copy
takes a few dozen bytes for a program a few calls deep, and the copies may
hold up to an eighth of history's budget (8 MB at the default 64 MB); past
that the oldest go, and every copy goes with its keyframe. A reverse command
that leaves the machine where it was, such as a step back out with no
caller, keeps the record as it is.

When history reaches back to power-on, the chain ends at power-on, or at the
last reset or power cycle since, just as it does when the debugger was open
for them, and nothing is guessed below it. History may not reach that far:
its oldest part is let go to stay within its budget, a stretch run at
Maximum speed records nothing, and a stretch with another disk in a drive
cannot be replayed over the disk there now. The chain then ends where the
history it could replay starts, with the note "Calls before history starts
at $xxxx are not available.", and the walk guesses below. When no rebuild
can follow a move, because history was turned off, could not reach where
the machine landed, or the rebuild failed, the record keeps the calls since
the landing and the note reads "Calls before $xxxx are not available."
instead. Without history, calls made before the debugger opened come from
the walk; to record everything, start Casso with `--debugger` or choose
Debug > Restart under debugger, which power-cycles the machine with the
debugger open.

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

While history recording is on (Tools > Options), the trace starts where history
starts, `HISTORY ON` or not: older instructions are rebuilt from
history by replaying it, at once while the machine is stopped and in the
background while it runs, and the 100,000 newest are a cache that makes the
newest quick to list. While the machine runs, the trace ends at history's
newest keyframe unless `HISTORY ON` is set, and then the ring has the
instructions since. A row
history cannot replay, because its disks were changed since, reads "Not
available". `HISTORY SAVE` writes at most the newest 1,000,000 entries.

```text
HISTORY                  the newest 20 entries
HISTORY 1175751201 40    40 entries from the first at or after that cycle (decimal)
HISTORY SAVE run.txt     every retained entry
```

Each entry is known by its cycle count, which no later instruction changes.

The trace costs nothing while it is off.

The window's trace pane lists its keys above its rows, and a click on a key
there does what the key does. With the pane focused, `Space`, `O` and `R` step
into, over and out, `Return` runs, `T` turns the trace off and on, `B` turns
the bytes column off and on, and `S` saves the trace to a file you pick, as
`HISTORY SAVE` does. While the machine is
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

The video log has an entry for each change of video mode -- `TEXT`, `MIXED`,
`PAGE2`, `HIRES`, `80COL`, `DHIRES`, `ALTCHARSET` and `80STORE` -- with the
machine's cycle count and the scanline and cycle within the line the beam was
at, grouped under the frame each fell in. An access that does not change the
mode makes no entry. The sound log has an entry for each speaker toggle and each
write to the Mockingboard's 6522s, with its cycle count and the cycles since
the last entry of its kind; a port B write that clocks a value into an AY
register also makes an entry for that write, with the register and the value.
Entries are added to both logs all the time, from the start of the session,
whether or not the debugger started the run.

`VIDEOLOG` and `SOUNDLOG` take `CLEAR`, which hides the entries made so far,
`LIST [n]` (the last `n` entries, 100 unless given; a bare `VIDEOLOG` or
`SOUNDLOG` lists) and `SAVE [file]`, which writes every entry since the log
was cleared (`VideoLog.txt` or `SoundLog.txt` unless given). The newest entries
are in memory; older ones are rebuilt from history for each listing, as far
back as history goes, and a rewind drops the entries after the position it
rewinds to. A listing stops rebuilding after five seconds, and its first line
then ends `older history was not searched:`. The cycle count and beam position
are those at the access.

A speaker toggle costs about fourteen nanoseconds of host time, so a program
playing a note runs one to three percent slower at maximum speed, and a loop
that toggles the speaker as fast as a 6502 can about a sixth slower. Other
I/O, a keyboard poll among it, costs nothing measurable.
## The debugger window

**Debug > Debugger...** opens the window beside the emulator, as do F12, F7
(AppleWin's key) and Ctrl+F12 in the emulator window; `--debugger` opens it at
start, without taking the foreground or the keys from the emulator window: it
takes them when you click in it. When it opens, the keys go to the pane that
had them when it last closed, including a pane that opens a moment later, such
as a second disassembly view or a device panel, unless you have clicked or
typed in the window by then. Ctrl+F12 is for running Casso under a debugger
that takes F12 as its break key. It shows the code around the PC, the registers and flags,
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
(**Keyboard scheme**); what the colors mean is behind each pane's info button
(**Colors**, below). The command bar below it holds the run and step buttons as icons,
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

Keys and the mouse button let go of while the machine is behind live are
released in the machine when it goes live again; a key still held stays down.
Drag its grab handle to move it along its edge or, pulled well away, to float
it in a window of its own, icons alone. While it is carried the bar lifts off
its place, raised over a shadow. Dragged back over any edge of the window, a
floating bar snaps into that edge, which makes room for it, and slides along it
with the pointer until the button is let go; a long bar also snaps into the
edge its far end is dragged up to. Its place is kept between sessions.
**Detach** closes the debugger and leaves the machine running, resuming it
first if it is stopped.
The console has a toolbar of its own with **Dialect**, which sets the words
the console and the memory Address box read.

### The history timeline

Above the command bar, the history timeline shows small pictures of the
screen at points in the recorded history: the oldest at the left, the live
end at the right, and evenly spaced points between, as many as fit across the
window. Each picture is the screen at a snapshot of history, so the point it
shows is exactly where a click goes. Pictures are drawn in the background, a
few a second and only while the timeline can be seen, so an empty cell fills
in shortly; while the machine runs, the points stay put until the live end
has moved on by a whole point. Resting the pointer on a picture shows it full
size beside the timeline. Clicking one moves the machine to that point in
history, stopping the machine first if it is running; clicking the live end
goes live. The timeline docks, floats and keeps its place like the command
bar. Floating, it keeps its length, and dragging either end resizes it.

### The status bar

A status bar runs along the bottom of the window. From the right:

- **Text size**, as a percentage. Clicking it opens a slider from 50% to 300%
  in steps of 10%, the same size Ctrl+Plus, Ctrl+Minus and Ctrl+wheel set.
  A click anywhere else, or Escape, closes the slider; the arrow keys move it
  while it is open. The size is saved and comes back the next time the
  debugger opens.
- **Speed**, the emulator's: 1x, 2x or Maximum. Clicking it opens a menu of
  the three, which sets the speed as the emulator's own speed menu does.
- **History full**, how much of the reverse-execution memory budget the
  recorded history holds, with a meter that is green while empty and blends
  smoothly to blue as it fills. A full budget is not a fault: the oldest
  snapshots make room for new ones. It reads "History off" while history is
  not recorded.
- **Begins at**, the time of day the recorded history begins at, in your
  regional time format, then in parentheses the emulated time since the
  machine started, for example `Begins at 10:42:07 PM (0.0 s)`. Once the
  budget is full the oldest snapshots are dropped to make room, so both times
  move forward as the machine runs.
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
four memory windows, and one panel for each device the machine has. The View
menu also opens these, which stay closed until first opened:

- **Symbols**, each symbol table under a heading with its count and a check
  box that runs `SYM<table> ON` or `OFF`, and its symbols under it. A
  symbol's menu has **Go to** and **Remove**, and any row's has **Clear
  table** and **Save table...**; Find searches the symbols.
- **Bookmarks**, each bookmark's number, address and symbol, with **Go to**
  and **Delete**. Ctrl+K, K adds or removes a bookmark on the selected
  disassembly line, and Ctrl+K, N and Ctrl+K, P go to the next and the
  previous bookmark.
- **Profiler**, the profile as `PROFILE` lists it, by opcode or by address,
  read while the machine is paused; its menu starts, stops, resets and saves
  profiling.
- **Data ranges**, each range marked as data, with **Go to** and **Delete**.

**Debug > Step filters...** lists the routines a step goes over, each a
symbol, an address or a range as `SKIP` takes it; one is added on the last
row and deleted with Delete or its row's menu.

Under the registers, **Cycles** is the CPU's count since power-on and **Trip**
the count since the trip count was last reset, by **Reset trip count** on
either row's menu or by `RCC`; the total is never reset. **Last branch** is
the last instruction that sent the PC elsewhere, a taken branch, a jump, a
call, a return or BRK, and where it went, as `$8759 → $8751`; its menu has
**Go to branch** and **Go to target**. While the machine is stopped, the stack
pane's menu has **Push byte...**, **Pop byte** and **Pop word**, which run
`PUSH`, `POP` and `PPOP`.

A disassembly row's menu also has these, each on the row, or on the selected
rows when it is among them:

- **Mark as bytes**, **words**, **addresses**, **text** or **floats** and
  **Clear data marking**, which run `DB`, `DW`, `DA`, `ASC`, `DF` and `X`
  over the rows' bytes.
- **Add symbol here...**, which runs `SYM name = addr`.
- **Never step into this routine**, which runs `SKIP` on the routine a `JSR`
  row calls, or on the row's own address; the call stack's menu has it for a
  frame's routine.
- **Add bookmark** or **Remove bookmark**, which run `BMA` and `BMC`.
- **Start stopwatch here**, then **Stop stopwatch here** on another row,
  which runs `STOPWATCH start stop`.
- While the machine is stopped, **Call subroutine here**, which runs `JSR`;
  **Edit instruction...**, which assembles an instruction over the row's,
  padded with NOP when it is shorter and, when it is longer, written only
  after a question that lists the instructions it covers; and **Replace with
  NOPs**. Neither changes ROM, both write only over the bytes in the rows,
  and **Undo** and **Redo** on the menu put the bytes back and write them
  again.

A drag selects text across the columns; a Ctrl+drag selects only in the
column it began in.

The Breakpoints pane's **New** drop-down also has **Read of memory never
written...**, **Video line...**, **Beam position...** and **Execute and
watch...**, which run `BRKUNINIT ON`, `BPV`, `BPBEAM` and `BPA`. The first
three are stops of the session, not rows of the pane, so the console's reply
is the only sign of them; `BPA` adds a breakpoint and a watchpoint, each a row.

Each row of the Watches pane has a check box, which runs `WE` or `WD`. Under
**Zero-page pointers** the pane lists the zero-page pointers: each zero-page
address, then the address its two bytes form and the first four bytes there,
as `$0800: 00 00 00 00`, with a check box running `ZPE` or `ZPD` and Remove on
its menu running `ZPC`. **Add zero-page pointer...** on the pane's menu, and
**Add as pointer watch** on a Memory pane's menu over a byte in zero page,
run `ZP`.

### The heat map

The heat map is a tab beside **Memory 1** in the default layout, and **View >
Heat map** brings it forward. It shows the whole 64 KB address space as a
grid, one cell an address, filling the pane: each row holds a power of two
of addresses from 16 to 1,024, so every row starts at a round address and its
label down the left (such as $0400) reads as the first address in it. The
row holds the count whose cells come nearest the zoom's size, and the cells
are then widened or narrowed so the row spans the pane's whole width, every
column within a pixel of the others; a cell is between three quarters and one
and a half times the zoom's width (wider at the smallest zoom, which cannot
narrow a one-pixel cell, and in rows of 1,024), and always the zoom's height. Only when
even 16 cells at the zoom's size are wider than the pane does the map scroll
across instead. Each address is drawn in the color of
what touched it: code (the bytes of every instruction run) in the
disassembly's instruction color, reads in green and writes in red. An address
nothing touched is a dark gray (a light gray on a light theme), and every cell
is set apart from its neighbors by a one-pixel gap of the page. **All**,
**Code**, **Data** and **Changed** above the map choose what it shows:
everything, with code ahead of data where an address is both; executes
alone; reads and writes alone; or only the writes that changed a value (see
"Reads before written and changed values" below).

The pane's bar holds the rest. **Fading**, the default, shows how recently and
how often each address was touched. An address the program keeps busy is
bright, and its brightness follows how many times a second it is touched, on a
logarithmic scale, so a byte read a few times a second and a tight loop's
thousands both show. Once the program leaves an address it dims, and a single
access is gone after the fade time, 10 seconds of machine time unless the
**Fade** drop-down sets 2, 5, 10, 20, 30 or 60 seconds; a busy address takes a
few times longer to dim away. **Cumulative** keeps everything: nothing fades,
and each address's brightness is its total count on a logarithmic scale
against the busiest address, so a few hot loops do not wash out the rest.
**Reset counts** starts the totals over. The choice and the fade time are kept
between sessions.

The mouse wheel over the map scrolls it up and down, and Shift with the wheel
scrolls it across when the rows are wider than the pane; scrollbars show along
each side the map overflows, and dragging the map pans it. Ctrl with the wheel
over the map zooms it in and out; elsewhere in the debugger Ctrl with the
wheel still sizes the text. Zooming changes the cells' height at every step
and, about every third step, how many addresses a row holds; the row under the
mouse stays at the mouse's height, and when the row length does not change, or
the rows are wider than the pane, the address under the mouse stays under it
too.

The zoom shows in the map's bottom-right corner as a percentage of the
starting size, three pixels a cell at 100% scaling. Hovering over it gives a
tip about Ctrl with the wheel. Clicking it opens a slider above it, from one
pixel a cell at the left to the largest at the right, each step along it
scaling the cells by the same factor, and **Reset**, which goes back to the
starting size with $0000 at the top left. Clicking it again, or anywhere else
on the map, closes the slider.

The address the mouse is over shows at once in a tip beside it, with what
touched it and how much, and then its last writer and reader (see below): "$C65E  executed 120/s, read 3.4/s" while fading, or
"$C65E  executed 1,200 times, read once" while cumulative, the numbers to two
figures since the map keeps 255 levels; a frame is drawn around its cell.
While cells are small, a mouse over an untouched cell picks the busiest cell
within a few pixels instead, so a lone hot byte is easy to land on. Clicking a
cell shows its address in a memory window: the one last used if it is in
view, otherwise the first one that is, otherwise Memory 1, whose tab comes
forward.

Over the heat the map marks where the machine stands and what will stop it.
The cell at the PC is outlined in the disassembly's PC yellow, and the stack
pointer's byte on page $01 in cyan; in a bank's view each is outlined only
when that bank holds it, so aux RAM's view marks the stack only while the
zero page and stack are switched to aux. Every enabled breakpoint is
outlined: one on an address in the disassembly's breakpoint red, a read
watchpoint in green and a write watchpoint in orange, a watchpoint on both
with both rings. An address with a symbol is tinted toward the symbol color.
Each outline lies around the cell like the hover frame, and a second one on
the same cell sits inside the first. The tip lists what marks the cell: "PC",
"Stack pointer", "Breakpoint #2", "Read watchpoint #3", "Write watchpoint
#4", "Read and write watchpoint #5" and "Symbol COUT". Breakpoints and
symbols are on the CPU's addresses, so in a RAM bank's view a breakpoint at
$D100 marks both language card banks.

Code is drawn two ways. A byte fetched as an instruction's opcode takes the
full code color, and a byte only ever run as an operand is drawn part of the
way to the gray, so each instruction's start stands out and code entered at
an odd offset shows as a dim byte drawn bright. The tip says "executed as an
operand" for such a byte.

Zoomed in far enough, a cell shows what it holds. Once the text fits at a
size of 9 pixels or more at 100% scaling (cells about 15 pixels), the cell shows its
value in hex; once two lines fit with ten characters across (about 26
pixels tall and 58 wide), a second line shows the instruction's mnemonic
with its operand's form, such as "LDA (..),Y", for a byte fetched as an
opcode, and the byte's bits otherwise. The text is the theme's text color
or its page color, whichever stands out more from the cell. The values are
the bank's as they stand: the CPU's view reads what the CPU would, leaving
I/O unread; ROM's view only where a read reaches ROM now; a RAM bank's
straight from that bank. A set of a few small ranges is the easy way to see
this, since a set starts at the largest zoom that fits.

**Blend** on the bar mixes the colors of an address touched more than one
way, each weighted by how hot it is, instead of showing the hottest kind. An
address both run as code and written, self-modifying code, takes a magenta
of its own, which the key lists while Blend is on. Blend is off by default
and is kept between sessions. Whether or not Blend is on, the tip of such an
address says "Self-modifying: run as code and written".

#### Ranges

The map can be focused on the regions a program uses rather than all 64 KB.
The bar's set drop-down, **All memory** until a set is chosen, lists **All
memory** and every saved set; choosing a set shows only its ranges, stacked
in the set's order, each under a header that gives its name and span, such as
"Hi-res page 1  $2000-$3FFF". Each range's rows follow the same rule as the
whole map, starting at the range's own first address and stretched to the
pane, and a range smaller than a row is one row across the pane. A newly
chosen set starts at the largest zoom at which every range fits the pane
without scrolling, and fits again as the pane is resized until the map is
zoomed; one zoom applies to every range, and the zoom's **Reset** fits the
set again. Hovering, the tip, the pick of a busy cell nearby and a click to show
the address in memory all work in every range as on the whole map.

**Edit ranges...** on the bar, or **View > Heat map ranges**, opens the pane
that edits the sets; it stays closed until first opened. Its bar holds the
set being edited as a drop-down, **New set**, **Rename set** and **Delete
set**, then **Edit**, **Remove**, **Move up** and **Move down** for the set's
ranges. The list has each range's **Name**, **Start**, **End**, **Size** and
whether it is **Built-in**, with a check box in front to include the range in
the map or not.

Every set holds the built-in ranges: **Zero page**, **Stack**, **Text page
1**, **Text page 2**, **Hi-res page 1**, **Hi-res page 2**, **I/O**,
**Language Card**, and **My program**, which spans the symbols loaded from
files and is listed only while some are loaded. The first time the pane is
used there is a set called **Default** with all of them included; a new set
starts with all of them left out. A built-in range can be checked in or out
and moved, but not edited or removed.

The list's last row, **Add new range**, adds a range: a click opens its cell
for typing, Enter or Tab goes on along the row, and once it has a start the
range is added and a new last row follows it. A start accepts a whole span: `$6000-$95FF`, `ZP_VARS..+$20` (a symbol and a length), the console's
`$300:$3FF` and `$300,$100`, or a symbol alone, which takes the size its
cc65 debug file has for it. A click on a cell, **Edit** or F2 edits a
range's name, start, end or size in place; a size is stored as a length.
Every start and end is read by the console's own evaluator, so symbols,
decimal and arithmetic work as in any command. For a range that does not
read, the reason is under the bar and the box stays open; Escape cancels the
edit. The gripper at a row's left drags it to another place in the set.
Delete, or **Delete** on the row's menu, removes the selected range, and
Insert starts a new one. The sets, and the set on the map, are saved between
sessions.

The machine records for the heat map only while its pane is open and in
front. Closed, or behind another tab, it records nothing and costs nothing,
and the cumulative totals count only what it recorded.

The heat map shows the machine where it stands in reverse execution's
history, not where the host's clock has reached. After a step back, a seek, a
click or a drag on the timeline, a reverse run or a return to live, it shows
the map as it was at the instruction the machine landed on, and running or
stepping on from there counts on from there:

- **Cumulative** totals are the counts from **Reset counts**, or from when the
  pane was opened, up to the current position. History keeps the counts made
  between each keyframe and the next, packed small, beside the keyframe, so
  any position's totals follow from them. They count against the history
  budget and show in its meter: a few percent more history for an idle
  machine, and about two thirds more for a busy game, so the default 64 MB
  holds about 26 minutes of such a game rather than 43. A position before a
  reset, or before the pane was opened, shows nothing.
- **Fading** heat is rebuilt after each move by replaying three times the
  fade time before the landing, since a busy address stays warm well past
  the fade time, as far back as history reaches, on a second machine in the
  background, so neither the machine nor the debugger waits. The newest two
  seconds come first and the older seconds fill in after them, and the bar
  says **Rebuilding** in place of the color key until they are all in.
  During a drag of the timeline the rebuild waits until the line is let go.

A stretch of history recorded while the pane was closed keeps no counts, so
a seek across it adds nothing for it.

#### Banks

On a machine with banked memory (the //e, the //c, and a ][+ with its
Language Card), the **Bank** drop-down on the bar, after the ranges, chooses which
memory the map shows. **CPU**, the default, is the 64 KB the CPU addresses,
whatever is banked in at the time of each access. The others show where each
access really landed:

- **Main RAM** and **Aux RAM** (on the //e and //c) are 64 KB each, laid out
  as the //e's memory chips hold them: - is the RAM the CPU reaches
  there, and the top 16 KB holds the Language Card's RAM, bank 1 of  at
  -, bank 2 at - and the high RAM at -. A write
  with RAMWRT on shows in Aux RAM, a read with ALTZP on of - or of
  the Language Card shows in Aux RAM, and so on.
- **Language card** and **Aux language card** show that top 16 KB alone.
- **ROM** shows the reads that reached ROM, at the addresses they were made
  at, and any write to an address where ROM is banked in.

I/O (-) appears in the CPU's view only. Each view fades, counts,
resets and follows history just as the CPU's does, and the tip says where in
the bank the cell is: "Aux RAM ", "Main language card bank 1 ",
"High RAM " on a ][+, "ROM ". The choice is kept between sessions;
on a machine without that bank the map shows the CPU's view.

#### Last writer and last reader

For every address, in every bank, the heat map also keeps the instruction
that last wrote it and the one that last read it as data. Once the mouse has
been over a cell for a moment, the tip adds them on two more lines: "Last
written by  STA (),Y at cycle 1,234,567", with the symbol at that
address when one is loaded (" DRAWROW: STA (),Y"), and "Not read
since counting started" when nothing has read it. The instruction is
disassembled from the code at that address as it stands now. An interrupt's
pushes onto the stack count as the interrupted instruction's.

- **Ctrl+click** a cell to show its last writer in the disassembly, and
  **Ctrl+Shift+click** to show its last reader. A plain click still shows
  the address in a memory window.
- **Ctrl+Alt+click** a cell to go back through history to its last write, and
  **Ctrl+Alt+Shift+click** to its last read. The machine stops first if it is
  running, and lands just after the instruction that made the access: the
  byte holds the value it wrote, the instruction is the one just above the
  PC, and a step back runs to it. A write older than the history kept, or
  made while history was not being recorded, is reported in the console
  instead.
- Right-click a cell, or a byte in a memory window, for the same four actions
  as menu rows: **Show last writer**, **Show last reader**, **Go to last
  write** and **Go to last read**. A memory window's rows use the CPU's view
  of the byte.

The last writers and readers follow the machine through history as the
totals do: after a seek, a step or a run on from the past, they are what was
true at that position. They take no history of their own. The map keeps, for
each address, the latest access up to the furthest the machine has run;
where that is later than where the machine stands, the access before it is
found when the tip or a click asks for it: the counts history keeps say
which stretch between keyframes it was in, and that one stretch is replayed
on a second machine, which takes a few milliseconds. An access older than the history kept is reported as such.
Nothing from before the pane was opened, or from before **Reset counts**,
is shown.

#### Reads before written and changed values

For every byte of main and aux RAM, the Language Card's banks among them,
the heat map knows whether the CPU has written it since power-on. A read of
a byte nothing has written is a **read before written**: the program is
reading whatever power-on left there. In **All** and **Data** such reads
are drawn in violet over whatever else the cell shows, as hot as they
were, and the tip adds "read before written" with how often. An instruction
fetched from RAM nothing wrote is code, not a read, and is not counted. Only
a power cycle makes RAM unwritten again; a reset (Ctrl+Reset) keeps it
written, as the RAM keeps its bytes.

The debugger's own writes (a memory window's edit, a poke, a fill, a binary
loaded, the line assembler) are not the program's, so the heat map counts
them as nothing: no write, no change, no last writer, and not written for
the purpose above. Each byte the debugger wrote is marked as edited instead,
the tip over it ends with "Edited in the debugger", and a read of it is no
read before written, since the debugger put the value there. A power cycle
clears the marks with the RAM.

The map can know what was written only from power-on. Opened later, it
counts every byte as written and so finds no reads before written until
the next power cycle; a power cycle with the pane open (or with
`BRKUNINIT` on) starts the tracking. The ROM's own start-up reads some bytes
before anything writes them, such as the power-up byte at $03F4, and a
program may probe memory the same way.

`BRKUNINIT ON` stops the machine after such a read, as a watchpoint does:
"Read before written: $00 from $2000 by $0300" gives the value, the
address read and the instruction that read it. `BRKUNINIT OFF` turns it
off, and `BRKUNINIT` alone reports the setting, whether the map is tracking
yet, and how many reads before written it has counted at how many
addresses. The break keeps the heat map recording whether or not its pane
is open. `BPC *` clears it with the breakpoints, `BPSAVE` writes it, and
opening another machine or loading a machine state turns it off. A reverse
run does not stop on it.

To leave known reads out, put their ranges in a set (see "Ranges" above)
and choose the set from the bar's **Leave out** drop-down: reads before
written in its included ranges neither stop the machine nor show on the map
nor count in what `BRKUNINIT` reports, though the map still counts them. A
range given in a bank's view is matched at the address the CPU reaches the
byte at, so bank 1 of the Language Card is at $D000. A set renamed or
deleted leaves nothing out until it is chosen again.

A write that stores the value a byte already holds changes nothing. **Ignore
writes that don't change the value** on the bar leaves such writes out of the
writes shown, in every view, fading or cumulative; the **Changed** view
shows only the writes that changed a value, in rose. A write to I/O, or to
an address no memory takes, cannot be read back first, and always counts as
a change.

Both follow the machine through history as the totals do. Beside each
keyframe, history keeps which RAM had been written, which bytes the
debugger edited and which were fetched as opcodes (the marks that tell an
operand from code), packed, or a note that they are as at the keyframe
before; a seek, a step back or a run on from the
past puts it back, so a read counts as a read before written exactly as it
did the first time, and a rebuild of fading heat starts from it. **Reset
counts** zeroes their counts with the rest and leaves what was written
alone. Loading a machine state starts the counts over, and, as the map comes
on again away from power-on, every byte counts as written.

The map's tables, with what it keeps to follow history, take about 60 MB of
memory while the pane is open (or `BRKUNINIT` is on), and 35 MB more while
fading heat is rebuilt; none while it is closed. Which RAM was written adds
little to history: kept whole, with the edited and opcode marks, only at
the keyframes where any of them changed, it packs to about 64 bytes each.

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
window and Ctrl+Y redoes it. Neither writes over a byte that has changed since
the edit; that byte's address appears in the console instead.

**Edit > Find**, Ctrl+F or **Find...** on the window's menu searches all of
memory as `S` does: hex bytes, a word, text in quotes (`'text'` with the high
bit set), `??` for any byte and `?n` or `n?` for half of one. The window goes
to the first match and selects it, and F3 and Shift+F3 walk the matches, round
to the other end. On a selection, while the machine is stopped, **Fill
selection...** fills it with the bytes typed, repeated, as `F` does, and
**Copy selection to...** copies it to the address typed, as `M` does.
**Compare with...** compares the selection with the range at the address
typed, as `MC` does: the bytes that differ are drawn in the result color, and
each one's tip has the other range's value. **View the other range's values**
draws those values in their place, and **Clear comparison** ends it.

A window reads the bytes around its rows as it scrolls. Until a read reaches
rows scrolled into view, they show what an earlier read left there, in the
same colors; rows no read has reached yet show zeros in a muted color, and
their tip says "not read yet". Addresses are always the 64K's four digits,
wherever Go to starts the rows.

Each region of memory a window shows that is not main RAM has a box drawn
around its bytes: ROM, each slot's ROM (labeled with its slot, such as Slot 6
ROM), a card's expansion ROM, I/O, the Language Card's two banks and aux RAM,
as the machine's memory map stood when the window was read. The box follows
the values: where a region starts or ends partway along a row, its outline
steps around the bytes rather than taking in the whole row. Its label sits on
the box's top edge, centered and breaking the line; where that edge is too
short for it, as when a region starts near a row's end, the label moves down
to the edge along the next row. Rows move apart only where an edge runs
between them, so no line or label covers a byte. ROM and I/O boxes are drawn
in their bytes' colors, and the Language Card's and aux RAM's in the memory
map's. While the boxes are drawn, the gaps before and after the values are a
character wider, so a box's edge does not crowd the addresses or the text.

### Device panels

**Window > Device panels** lists the machine's devices: the Disk II controller, the //e MMU
and its memory map, the video switches, the keyboard, the Mockingboard, the
printer card, the cassette deck and the clock. Each opens as a pane showing its registers and
state, with the disk head position, the memory map, or level meters where the
device has them. `PANEL LIST`, `PANEL name` (or `PANEL OPEN name` or
`PANEL name OPEN`) and `PANEL CLOSE name` (or `PANEL name CLOSE`) do the same
from the command box.

### Colors

Every pane whose colors mean something has an info button (an i in a circle)
at the end of its toolbar; the call stack and a device panel with a memory
map or a disk head, which have no toolbar, have a band of their own for it.
Resting the pointer on the button shows that pane's color key under it: a
line for each color, its swatch beside its meaning, in the theme in force.
The key goes when the pointer leaves. Clicking the button, or tabbing to it and
pressing Space or Enter, holds the key open until the button is pressed
again, Escape is pressed, the focus moves on, or a click lands elsewhere.
Hovering over any of these colors in the window shows the same meaning as a
tip; where the element already has a tip, such as a symbol or the flags, the
meaning is added to it. The colors of syntax (mnemonics, directives, labels,
numbers, strings and addresses) mean what the text says and are not listed.
The debugger has no Help menu.

- **Disassembly**: the PC's yellow arrow and row, the next instruction to run,
  whose arrow can be dragged to set the next statement; a green row, the line
  brought into view by Go to, the call stack or a branch arrow; a row in the
  accent color, where the branch at the PC goes; the branch arrow in the PC's
  yellow when the flags as they stand take the branch, and gray when they do
  not, with a tip that gives the flag the branch tests and how it stands; a
  red dot for a breakpoint, a red ring for a disabled one, and a gray dot
  where a click sets one; bytes in the changed color, changed since the last
  stop; the operand in the comment green, what the instruction reads; and
  `Result: `, what it leaves behind, in the result color.
- **Source**: the PC's row, a red dot for a breakpoint and a red ring for a
  disabled one, and `Result: ` in the result color.
- **Registers, watch and stack**: values in the changed color, changed since
  the last stop; in the watch pane, a dimmed automatic watch, touched by the
  previous instruction, and a dimmed watch, disabled.
- **Call stack**: a dimmed frame, unverified, which may no longer be a live
  call; the dimmed last return, a call that has returned and left the stack.
- **Memory**: bytes in the changed color; ROM bytes and I/O bytes in their
  colors, with the boxes around them; bytes no read has reached yet in the
  muted color; and the boxes around the Language Card's banks and aux RAM.
- **Memory map**: main RAM, aux RAM, each Language Card bank, ROM, slot ROM and
  I/O, each in a color of the theme's, with a key below the map.
- **Heat map**: code in the disassembly's instruction color, an operand byte
  dimmer, reads in green and writes in red; with Blend on, magenta for
  self-modifying code; a cell tinted toward the symbol color for an address
  with a symbol; outlines in the PC's yellow for the PC, cyan for the stack
  pointer, the breakpoint red for a breakpoint, green for a read watchpoint
  and orange for a write watchpoint; violet for a read before written, and
  rose for a write that changed a value in the **Changed** view.
- **Disk head**: the head muted while the motor is off, flashing as it steps,
  and settling to the accent on its track; a lamp lit while its phase magnet
  or the motor is on.
- **Breakpoints**: a red dot for a breakpoint and a red ring for a disabled
  one.
- **Status bar**: the history meter, green while the buffer is empty, blending
  to blue as it fills; its own tip says so, and it has no info button.
