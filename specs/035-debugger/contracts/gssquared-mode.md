# Contract: GSSquared Mode

The third command mode (FR-011, FR-022a). Its grammar is taken from
GSSquared's published debugger documentation; its output format is fixed by
fixtures captured from GSSquared (R-027). Every command maps onto the engine
operation the equivalent AppleWin command performs, so a breakpoint set here
is listed by AppleWin's `BPL` and the Monitor's `/bpl`.

## Selecting the mode

`MODE GSSQUARED` in AppleWin mode, `/mode gssquared` in Monitor mode. In
GSSquared mode Casso's engine commands are bare names, as in AppleWin mode,
so `mode applewin` returns; `/` is not a marker here because GSSquared uses
it as its bank separator (R-037).

## What GSSquared's source settled (2026-09-18)

- Its typed command table is exactly: `set`, `load`, `save`, `move`,
  `verify`, `watch`, `nowatch`, `help`, `bp`, `bpd`, `bpi`, `nobp`, `list`
  and `l`, `map`, `debug`, `nodebug`, `sload`, `sclear`, `slookup`, `m`,
  `x`, `video`, `novideo`. `verify` is parsed and does nothing.
- Stepping is by key in its window, not by command: Space and F10 step, `O`
  steps over, `R` steps out, Return resumes, `T` toggles the trace, `B`
  toggles a breakpoint. The GSSquared key scheme matches these. `o` and `r`
  as typed commands below are Casso's additions so a script can step.
- An address may carry a bank: `E1/0400`. Casso accepts `00/addr` as `addr`
  and refuses any other bank with "only bank 00 exists on this machine".
- Its tokenizer splits any token ending in `l` into `l` plus an address, so
  `300l` lists at $0300. Casso accepts `300l` but does not apply the split to
  command words.

## Commands

| GSSquared | Engine operation | AppleWin equivalent |
|---|---|---|
| `XXXX` | examine one byte | `D XXXX` (one byte) |
| `XXXX.YYYY` | dump range, 16 bytes per line with ASCII | `D` |
| `XXXX:YY YY ...` | deposit bytes | `MEB` |
| `set XXXX YY YY ...` | deposit bytes | `MEB` |
| `move XXXX.YYYY ZZZZ` | copy a range | `M` |
| `l XXXX` / `l` | disassemble from an address, or continue | `U` |
| `bp XXXX` / `bp XXXX.YYYY` | execution breakpoint on an address or range | `BP`, `BPX` with a range |
| `bp` | list breakpoints and watchpoints | `BPL` |
| `bpd XXXX r\|w\|rw` | data breakpoint | `BPMR`, `BPMW`, `BPM` |
| `bpi XXXX r\|w\|rw` | I/O breakpoint on `$C0xx`, from any bank | the same watchpoint; the bus mask already covers every bank |
| `nobp N` / `nobp XXXX` | clear by id or by address | `BPC` |
| `watch XXXX` / `watch XXXX.YYYY` / `watch` / `nowatch N` | display watches | `W`, `WL`, `WC` |
| `load "file" XXXX` | load a binary at an address | `BLOAD` |
| `save "file" XXXX.YYYY` | save a range | `BSAVE` |
| `sload "file"` / `slookup XXXX` / `sclear` | symbols | `SYM LOAD`, `SYM XXXX`, `SYM CLEAR` |
| `o` | step over | `P` |
| `r` | step out | `RTS` |
| Space (as a command line: `s`) | step into | `T` |
| Return (empty line) | resume | `G` |
| `debug "name"` / `debug` / `nodebug "name"` | open, list, close a device panel | window only |
| `m 8\|16`, `x 8\|16`, `map` | not available: no 65816, no IIgs MMU | reported with the reason |
| `bank/addr` | `00/addr` is `addr`; any other bank is refused with the reason | |
| `video ...` | not available: the emulator window shows the screen | reported with the reason |

- Addresses are hex without a prefix. A range is `first.last`, inclusive.
- Names are case-insensitive.
- Batch mode: Space and Return are not typeable in a script, so `s` steps and
  an empty line resumes only in the window; in a script an empty line is
  skipped as in the other modes, and `g` resumes.

## Output

Fixed by the captured fixtures under `UnitTest/Fixtures/Debugger/GSSquared/`.
From the documentation, the forms that are known:

- Dump: 16 bytes per line, the address, the bytes, then the ASCII.
- Breakpoint list: one line per entry with its id, kind, address and, for
  data and I/O breakpoints, `r`, `w` or `rw`.
- Watch list: one line per entry with its id and range.
- A stop: the reason, the address, and the registers.

The formatter tests compare against the fixtures line for line. When the
`OUTPUT` format is set to another mode, replies use that mode's formatter.

**As built:** the fixtures are written from GSSquared's source, not captured
(research R-027). GSSquared has no stop line, so a stop in this format reads
as in AppleWin's; a deposit prints nothing, as in GSSquared. `debug` and
`nodebug` report not available until `PANEL` exists. In the window, the
controls' lines are sent in GSSquared words in this mode; run to cursor has
none. `nobp N` clears id `N` when an entry has it, and otherwise the
execution breakpoint at address `$N`.