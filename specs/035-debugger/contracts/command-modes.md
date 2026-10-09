# Contract: Command Modes

Every mode executes against one session (FR-012). This contract fixes the
syntax each mode reads and the text each mode writes. Per-command argument
forms for AppleWin mode follow AppleWin's help pages (research R-014).

## Casso engine commands

These commands have no AppleWin, Monitor, GSSquared or WinDbg original. Each
mode reaches them through its own marker (FR-014, R-037), and every one is
reachable in every mode:

| Mode | Marker | Example |
|---|---|---|
| AppleWin | bare name | `SWITCHES` |
| Monitor | `/` | `/switches` |
| GSSquared | bare name (`/` is its bank separator) | `switches` |
| WinDbg | `!` | `!switches` |

| Command | Effect |
|---|---|
| `MODE APPLEWIN` / `MODE MONITOR` | switch the session's mode |
| `MODE` | report the current mode |
| `PAUSE` | stop a running machine (reason `pause`) |
| `BUDGET <n>` | set the cycle budget for later runs in this session; `BUDGET 0` makes them unbounded, which is the default outside batch and `--attach` |
| `SWITCHES` | list the soft switches and memory banking (RAMRD, RAMWRT, ALTZP, 80STORE, INTCXROM, SLOTC3ROM, language-card read, write and bank, video switches) as name/value pairs |
| `STACK` | show SP and the stack page from $01FF down to SP+1 |
| `PATCH addr value...` | write bytes as a memory window edit does: RAM as a write, ROM into the image the CPU reads from (on the //c, into the current bank's image, so it survives a `$C028` flip), an I/O address refused with a pointer to `OUT`; values above $FF are two bytes, low first, as with `MEB` |
| `SRC` | the source file and line that produced PC, from the loaded debug file, and whether steps go by source line or by instruction |
| `SRC ON` / `SRC OFF` | step by source line or by instruction. With `SRC ON` and a debug file loaded, every step command in every mode (`T`, `P`, `RTS`; the Monitor's `S`) steps by source line: into stops at the first instruction of another line, the innermost one inside a macro; over runs calls and whole macro expansions and stops on the next line; out is unchanged. The window sets this from which of its source and disassembly panes has focus |
| `BP file:line` | a breakpoint on the first instruction of a source line, at each place a macro body line was expanded. A line that produced no code moves to the next one that did, and the reply says so. Told apart from AppleWin's `BP addr:addr` range by its left side, which is not an address, and its decimal right side |
| `CALLS` | the call chain to PC, innermost first: one line per frame with its call site, target symbol and provenance (`recorded` or `guessed`), and a break line (`-- TXS at $0812 --`) wherever the chain is broken (FR-067 to FR-069) |
| `CALLS MODE RECORDED\|WALK\|HYBRID` | choose the mechanism; `CALLS MODE` reports it; hybrid is the default |
| `SKIP name\|addr\|first.last` | add a routine to the step filter; `SKIP` lists it; `SKIP - name` removes one; `SKIP CLEAR` empties it (FR-070) |
| `HISTORY ON` / `HISTORY OFF` | start the instruction trace, which keeps the newest 100,000 instructions with their cycle count, registers before execution and last memory access, or stop it; stopping keeps the retained entries until the trace starts again or the machine changes (FR-045, FR-046) |
| `HISTORY [first [count]]` | the state, the number of entries retained, and `count` entries (20 by default) from entry `first`, oldest first; a bare `HISTORY` shows the newest. Both numbers are decimal. Each line: entry, cycles, address and its symbol, instruction, registers, and `R` or `W` with the accessed address, byte and symbol (FR-047) |
| `HISTORY SAVE file` | write every retained entry, one `HISTORY` line each, oldest first (FR-048) |
| `PANEL` / `PANEL LIST` / `PANEL name` / `PANEL CLOSE name` | list the current machine's device panels, open one, or close one, by provider id (`disk`, `mmu`) or title, either case (FR-049 to FR-053). Carried out by the window, which runs it from its command box and its panel menu; batch and the pipe return `notAvailable` with `PANEL needs the debugger window.` A name the machine lacks is an error that points at `PANEL LIST` |
| `OUTPUT APPLEWIN\|MONITOR\|GSSQUARED` | set the output format alone (FR-013); `OUTPUT` reports it; `MODE x` sets both mode and format |
| `MODE GSSQUARED` | switch to GSSquared mode ([gssquared-mode.md](gssquared-mode.md)) |

The engine commands are the `Engine` family of `AppleWinCommandTable`
(`MODE`, `PAUSE`, `BUDGET`, `SWITCHES`, `STACK`, `PATCH`, `SRC`, `SKIP`,
`OUTPUT`, `PROFILE`). Each mode's parser strips its marker and hands the line
to `AppleWinParser`, so a command added to the family is reachable in every
mode with no parser change; `EngineMarkerTests` walks the family. `PANEL`,
`HISTORY` and `CALLS` join it when they are built, and WinDbg's `!` row when
that mode is.
AppleWin has no `MODE`, `PAUSE`, `BUDGET`, `SWITCHES`, `STACK`, `PATCH`, `SRC`,
`CALLS`, `SKIP`, `HISTORY` or `PANEL` command, so these names collide with nothing in its table;
nor does GSSquared's.

## AppleWin mode

- **Input**: `NAME [args]`, with the name case-insensitive. Hex numbers are
  written without a prefix (`C019`), or with `$` (`$C019`). Expressions follow
  AppleWin's operators. Symbols resolve through the enabled symbol tables.
- **Command names**: every name in spec Assumptions, "AppleWin command
  coverage".
  - A phase-3 name before the window exists returns `notAvailable` with
    `Error: command not available`, `NAME needs the debugger window.`
  - A name listed as not available returns `notAvailable` with the reason from
    the spec (`SHR needs a machine with Super Hi-Res.`).
  - A name that is in neither list returns `unknown`.
- **Output** follows AppleWin's documented console formats: `R` prints
  registers as `A:00 X:00 Y:00 P:30 S:FF PC:0300` plus a flag string, and `D`
  prints eight bytes per row with ASCII. Exact formats are fixed per command in
  the formatter tests from the help pages.
- **`A addr`** enters line-assembly mode, the same one Monitor `!` uses: each
  following line is assembled at the current address, and a blank line ends
  the mode. AppleWin's `A` is interactive in its window; this is how batch and
  the channel feed it lines.
- **`GG`** runs at full speed in the emulator and restores the previous speed
  when the run stops.
- **Watchpoint mode**: `BPM`, `BPMR` and `BPMW` take an optional trailing
  `BEFORE` or `AFTER`, defaulting to `AFTER`.
  - `BPMW 0400 BEFORE` stops before the instruction that would write, with
    memory unchanged.
  - `BPM C030` stops after the access and reports the value, the value a write
    replaced where it is known, and the instruction that made the access.
  - The keyword is a Casso addition; AppleWin has only the after-stop form.
- **`IF <expression>`** (FR-061), a Casso addition: `BP`, `BPX`, `BPM`,
  `BPMR`, `BPMW`, `BPMV` and `BP file:line` take a trailing `IF` and an
  expression over registers, symbols and memory (`*addr` reads a byte), such
  as `BP Loop IF X == 3 & *PTR != 0` or `BPMW 400 BEFORE IF Y = 2`.
  - The expression is evaluated only when the address or the access hits. A
    false one neither stops the machine nor counts a hit; a true one stops,
    and the stop reports the expression and its value
    (`Breakpoint #0 at $0300, IF A=41 is $1`; JSON `condition` and
    `conditionValue`).
  - It is evaluated once as it is set. An unknown symbol, or a read of an I/O
    address ($C000-$C0FF), is an `invalid condition` error and creates
    nothing. At a hit, an I/O read makes the expression false.
  - On a watchpoint or `BPMV` hit the pseudo-symbols `ACCESS` and `VALUE` give
    the accessed address and the byte read or written; a `BEFORE` watchpoint
    has `ACCESS` only. Elsewhere they are unknown symbols.
- **`BPMV addr value [IF expr]`** (FR-062), a Casso addition: a value
  breakpoint (kind `memoryValue`) that stops after a write leaves `addr`
  holding the byte `value`, and on no other write. The stop is reported as a
  watchpoint stop under the breakpoint's id, with the value and the byte it
  replaced.
- **`BPR`** takes its register, comparison and value with or without spaces
  (FR-015a): `BPR A=0`, `BPR A = 0` and `BPR A 0` set the same breakpoint;
  `BPR A=` is still an error.
- **Argument forms** follow AppleWin's own behavior, recorded in research
  R-014: `M dest range`, `F range value` (also `F start end value`), the
  `S`/`SH` item syntax with `?` wildcards and `@n` results, `PRINT` and
  `PRINTF` formats, `CALC`'s four-column line, `LOG` as a console verbosity
  setting, the `[name] [range]` data directives with `Z` as `DB` and `B` as
  the block list, and `SYM<table> CLEAR | LOAD "file" | ON | OFF`.
- **`PROFILE ON | OFF | RESET | LIST [ADDR] | SAVE [file]`**: while on, each
  instruction a debugger-driven run executes is counted; off, nothing is
  counted and no hook is added. A bare `PROFILE` lists. `LIST` prints count,
  base cycles and share per mnemonic and addressing mode, then the penalty
  cycles apart by kind: page crossings on indexed reads, taken branches, and
  branches crossing a page. `LIST ADDR` prints the twenty hottest addresses
  with their symbols. `SAVE` writes both tables to the file, `Profile.txt` by
  default. The penalty rows are a Casso addition.

## Apple II Monitor mode

**Input** is scanned the way the Monitor scans a line (R-013), with these
Casso rules on top:

| Form | Meaning |
|---|---|
| `addr` | examine one byte |
| `addr.addr`, `.addr` | examine range; `.addr` continues from the last address |
| Return (empty line), space | continue examining from the last address |
| `addr: b b b`, `: b b b` | deposit |
| `addrL`, `L` | list 20 instructions |
| `dest<start.endM` / `dest<start.endV` | move / verify |
| `value<start.endS` | search: `value` is one or two bytes |
| `S`, `addrS` | step one instruction |
| `T`, `addrT` | trace until a stop or the budget |
| `addrG` | go; `F666G` opens the line assembler instead |
| `!` | line assembler: `addr:MNE operand`, ` MNE operand` continues, `$cmd` runs a Monitor command, empty line exits |
| `a+b`, `a-b` | 8-bit hex arithmetic on the low bytes, as the Monitor's is; `FF+FF` prints `=FE` |
| `I`, `N` | inverse, normal |
| `n^K`, `n^P` | input and output hooks to slot 1-7; `0^K` and `0^P` restore the keyboard and screen |
| `^B`, `^C`, `^Y` | BASIC cold, BASIC warm, user vector |
| `^E` then `: b b b b b` | show registers, then set A X Y P S |
| `start.endR [file]`, `start.endW [file]` | read / write a host file |
| `/rest-of-line` | AppleWin-mode command line (`/bpl`, `/mode applewin`) |

- **Control characters** are accepted as the character itself (Ctrl+E) and as
  `^` plus the letter (`^E`), in either case.
- **Case**: all input is case-insensitive.
- **Several commands on one line** separated by spaces, as the Monitor allows
  (`300.30F 400.40F`).

**Output** matches the Monitor:

- Examine: `0300- A9 00 8D 00 03 60 ...`, with rows aligned to 8-byte
  boundaries: `303.30F` prints `0303-` with five bytes, then `0308-` with
  eight. The layout is the same on every machine, so batch output is
  identical on all of them.
- List: `0300-   A9 00       LDA   #$00`.
- Verify: one line per difference, `0303-41 (42)`, the source byte in
  parentheses, as the Monitor prints it.
- Registers: `A=00 X=00 Y=00 P=30 S=FF`.
- Arithmetic: `=FE`.
- Errors print as the Monitor's `ERR` line, followed by the Casso two-line
  error in `text`, so scripts can tell which error occurred.

## Disk breakpoints (`BPDISK`)

User Story 20, FR-142 to FR-165, research R-041. `BPDISK` is a Casso command,
listed in help's breakpoints section beside `BPBEAM` and in the command
table's Breakpoints family, reached as every Casso command is:

| Mode | Form |
|---|---|
| AppleWin, Casso | `BPDISK ...` |
| GSSquared | `bpdisk ...` (no GSSquared word collides) |
| Monitor | `/bpdisk ...` |
| WinDbg | `!bpdisk ...` |

```text
BPDISK [D1|D2] <event> [arguments] [options] [IF <expression>]
```

Numbers follow the mode's syntax: hex unless marked, `#` or `0n` for decimal,
`0x` too in WinDbg mode. `D1` or `D2` limits the breakpoint to a drive. There
is no slot: the breakpoint acts on the machine's one Disk II controller, and
the report gives its slot. Names are case-insensitive. `*` in place of a
number matches any.

| Event | Arguments and options | Stops |
|---|---|---|
| `READ track sector` | `PASSED`, `ADDR p p p`, `DATA p p p`, `SECTORS 13\|16` | after the instruction that read the checksum nibble of a data field following an address field with that track and sector; a field passed over during a search does not count without `PASSED` (FR-145) |
| `ADDRESS [track [sector [volume]]]` | `BADSUM`, `WRONGTRACK`, `ADDR p p p`, `SECTORS 13\|16` | after the instruction that read the checksum nibble of a matching address field (FR-146) |
| `HEAD [QT first[:last] \| T first[:last] \| HALF]` | `BUMP` | after the instruction that accessed a phase switch and moved the head onto the position, or, with `BUMP`, drove it against a stop at a matching position (FR-148) |
| `WRITE [QT first[:last] \| T first[:last]]` | | after the instruction that turned on write mode with the motor on and the head there (FR-149) |
| `WRITE track sector` | `DATA p p p`, `ADDR p p p` | after the instruction that wrote the third data prologue nibble following the read of that address field |
| `WRITE BLOCKED` | | after the instruction whose latch load write protection dropped, the first in a stretch of write mode (FR-161's rule: a load in write mode while the motor runs) |
| `DOS track sector` | `PASSED` | as `READ`, for DOS 3.3 logical sector `sector` (FR-150) |
| `BLOCK n` | `PASSED` | after the second of ProDOS block n's two sectors is read |
| `RWTS [track [sector]] [READ\|WRITE\|SEEK\|FORMAT]` | | before the first instruction of the routine the JMP at $03D9 holds, when the IOB matches (FR-151) |
| `DRIVER [BLOCK n] [READ\|WRITE\|STATUS\|FORMAT]` | | before the first instruction of the routine ProDOS's device vector holds for the slot and drive, when $42-$47 match |
| `MOTOR [ON\|OFF\|STOPPED]` | | after the instruction that started the motor, that began its spindown, or during which the spindown ended (FR-152) |
| `SELECT` | `EVERY` | after the instruction that changed the selected drive to this one, or, with `EVERY`, after each drive-select access |
| `INSERT`, `EJECT` | | at the next instruction boundary after the disk changed |

`FORMAT` after `RWTS` or `DRIVER` is the request's command, the call that
formats a track or a volume; the data-field length option is `SECTORS`.

- **Marks**: `ADDR` replaces the address prologues matched (D5 AA 96 and D5 AA
  B5) and `DATA` the data prologue (D5 AA AD), for this breakpoint only. Each
  `p` is a nibble, two hex digits, or `??` for any. `SECTORS 13` makes a data
  field after a custom address prologue 410 nibbles and a checksum; the
  default, `SECTORS 16`, is 342 and a checksum.
- **Head positions**: `QT` takes quarter tracks, 0 to 8B hex. `T` takes
  tracks and matches only their whole-track positions, so `HEAD T 11` is
  quarter track $44. `HALF` matches quarter tracks 4n+2. Casso's stepper moves
  the head in half-track steps, so there is no option for an odd quarter
  track; a `QT` range matches whatever positions in it the head reaches.
- **`BUMP`** with a position stops on a bump only when the head's position
  after it is one the position matches: `HEAD QT 0 BUMP` stops on the boot
  ROM's recalibration against track 0, not on a bump at the outer stop.
  `HEAD BUMP` stops on any bump and any move.
- **Clearing and changing**: a disk breakpoint is set enabled and stopping;
  `BPC`, `BPCHANGE`, `BPD` and `BPE`, their dialect equivalents (`nobp`,
  `bc`, `bd`, `be`) and the Breakpoints pane clear, change, disable and
  enable it as they do any breakpoint. `BPEDIT # BPDISK ...` replaces one,
  keeping its id.
- **`BPSAVE`** writes each as the `BPDISK` line that sets it again, with its
  marks and the user's condition; an RWTS or driver breakpoint's request is
  part of its definition, not of its condition, so a saved line read back
  sets the same breakpoint.

**Disk values.** In any expression -- an `IF` on any breakpoint or
watchpoint, `CALC`, an address argument -- these read the controller's state
and nothing else, ahead of any program symbol with the same name (FR-156):
`DISK.DRIVE`, `DISK.QTRACK`, `DISK.QTRACK1`, `DISK.QTRACK2`, `DISK.MOTOR`,
`DISK.WRITING`, `DISK.PROTECTED`, `DISK.INSERTED`, and `DISK.ATRACK`,
`DISK.ASECTOR`, `DISK.AVOLUME` (the selected drive's last good address field,
$FFFF while none is recorded). On a machine with no Disk II controller they
are unknown symbols, an `invalid condition` error when set in an `IF`.

**Replies**, AppleWin's text (Casso and GSSquared output too):

```text
BPDISK D1 READ 11 0
Set disk breakpoint #3 (sector read, track $11 sector $00, drive 1)
BPL
#3  E  on disk sector read, track $11 sector $00, drive 1
BPDISK RWTS 11 0 READ
Set disk breakpoint #4 (RWTS call, track $11 sector $00, read), unresolved: $03D9 holds no JMP yet
```

A stop:

```text
Disk breakpoint #3: sector read, track $11 (17) sector $00 volume $FE, drive 1, 16-sector
  address checksum $EF, expected $EF; data checksum $2C, expected $2C; epilogue DE AA EB
  head on track 17 (quarter track $44), motor on; read by $B925, address field found by $B94F, data field by $B8E1
```

The instruction addresses are those of DOS 3.3's RWTS as R-041 records them
from the System Master: RDADR16's search at $B94F, READ16's data prologue
search at $B8E1 and its checksum nibble read at $B925. The data checksum
values are illustrative; tests take theirs from the fixture they build.

In GSSquared's `bp` listing a disk breakpoint shows `disk` as its kind and the
event where the address goes; in WinDbg's `bl`, `disk` stands where the
address does, then the hit counts, then the event. Every listing gives the
same event text, and shows `unresolved` for an RWTS or driver breakpoint
whose vector holds no target.

**Errors**:

```text
Error: no Disk II controller
       This machine has no Disk II controller.

Error: invalid arguments
       BPDISK [D1|D2] READ track sector [PASSED] [ADDR p p p] [DATA p p p] [SECTORS 13|16] [IF expr]
```

A wrong argument gives the syntax line of the event typed, as FR-127 has every
command do.
