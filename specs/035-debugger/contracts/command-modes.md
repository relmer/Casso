# Contract: Command Modes

Both modes execute against one session (FR-012). This contract fixes the
syntax each mode reads and the text each mode writes. Per-command argument
forms for AppleWin mode follow AppleWin's help pages (research R-014).

## Casso engine commands

These commands have no AppleWin or Monitor original. They are reachable in
AppleWin mode directly and in Monitor mode through `/`.

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

AppleWin has no `MODE`, `PAUSE`, `BUDGET`, `SWITCHES`, `STACK`, `PATCH` or `SRC` command, so
these names collide with nothing in its table.

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
- **Argument forms** follow AppleWin's own behavior, recorded in research
  R-014: `M dest range`, `F range value` (also `F start end value`), the
  `S`/`SH` item syntax with `?` wildcards and `@n` results, `PRINT` and
  `PRINTF` formats, `CALC`'s four-column line, `LOG` as a console verbosity
  setting, the `[name] [range]` data directives with `Z` as `DB` and `B` as
  the block list, and `SYM<table> CLEAR | LOAD "file" | ON | OFF`.

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
