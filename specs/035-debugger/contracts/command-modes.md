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
| `BUDGET <n>` | set the default cycle budget for later runs in this session |

AppleWin has no `MODE`, `PAUSE` or `BUDGET` command, so these names collide
with nothing in its table.

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
| `a+b`, `a-b` | hex arithmetic, printed as `=result` |
| `I`, `N` | inverse, normal |
| `n^K`, `n^P` | input and output hooks to slot n |
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

- Examine: `0300- A9 00 8D 00 03 60 ...`, eight bytes per row on 40-column
  machines. There is no 80-column variant, so batch output is identical on
  every machine.
- List: `0300-   A9 00       LDA   #$00`.
- Registers: `A=00 X=00 Y=00 P=30 S=FF`.
- Arithmetic: `=1234`.
- Errors print as the Monitor's `ERR` line, followed by the Casso two-line
  error in `text`, so scripts can tell which error occurred.
