# Contract: GSSquared Mode

The third command mode (FR-011, FR-022a). Its grammar is taken from
GSSquared's published debugger documentation; its output format is fixed by
fixtures captured from GSSquared (R-027). Every command maps onto the engine
operation the equivalent AppleWin command performs, so a breakpoint set here
is listed by AppleWin's `BPL` and the Monitor's `/bpl`.

## Selecting the mode

`MODE GSSQUARED` in AppleWin mode, `/mode gssquared` in Monitor mode. In
GSSquared mode the AppleWin engine commands are reached with the `/` prefix,
as in Monitor mode, so `/mode applewin` returns.

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
