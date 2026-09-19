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
