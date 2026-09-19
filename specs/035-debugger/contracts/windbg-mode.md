# Contract: WinDbg Mode

The fourth command mode (FR-011, FR-022c, FR-022d). It is WinDbg-flavored:
the commands a 6502 session uses, with WinDbg's arguments and output layouts,
mapped onto the same engine operations as their AppleWin equivalents. It is
not parity with WinDbg, and the documentation says so.

## Selecting the mode

`MODE WINDBG` in AppleWin mode, `/mode windbg` in Monitor mode, `mode windbg`
in GSSquared mode. In WinDbg mode Casso's engine commands are `!` commands
(`!switches`, `!mode applewin`, `!calls`, `!skip cout`), which is WinDbg's
own marker for commands a debugger adds (R-037).

## Numbers and addresses

- A bare number is hex, as in WinDbg: `300` is $0300. `0x300` and `$300` mean
  the same.
- A length is `l<count>`: `db 2000 l20` shows 32 bytes.
- A symbol resolves through the enabled symbol tables; `x` lists matches for
  a pattern with `*`.
- A source line is `file:line`, with or without WinDbg's backquotes.

## Commands

| WinDbg | Engine operation | AppleWin equivalent |
|---|---|---|
| `t` | step into | `T` |
| `p` | step over | `P` |
| `gu` | step out | `RTS` |
| `g` / `g addr` | resume / run to | `G` / `G addr` |
| `pa addr`, `ta addr` | step over / step into until PC reaches addr | `G addr`, in steps |
| `bp addr` / `bp file:line` | execution breakpoint | `BP` |
| `bl` | list breakpoints and access breakpoints | `BPL` |
| `bc n\|*`, `bd n\|*`, `be n\|*` | clear, disable, enable | `BPC`, `BPD`, `BPE` |
| `ba r1\|w1\|e1 addr` | access breakpoint on one byte: read, write, execute | `BPMR`, `BPMW`, `BP` |
| `db addr [l n]` | bytes with ASCII, 16 per line | `D` |
| `dw addr [l n]`, `dd addr [l n]` | words, double words, little-endian | `D` grouped |
| `da addr` | ASCII string to a zero | `D` |
| `eb addr b b ...`, `ew addr w ...` | deposit bytes, words | `MEB`, `MEW` |
| `ea addr "text"` | deposit text | `MEB` |
| `f addr l n b` | fill | `F` |
| `s addr l n b b ...` | search | `S` |
| `m src l n dest` | move | `M` |
| `r` / `r a=41` | show / set registers | `R` / `A=41` |
| `u [addr]` | disassemble | `U` |
| `x pattern` | symbol lookup | `SYM` |
| `k` | call stack, one frame per line with provenance | `CALLS` |
| `? expr` | evaluate; hex and decimal | `CALC` |
| `.formats value` | the value in hex, decimal, binary and as a character | none |
| `l+s`, `l-s` | step by source line on, off | `SRC ON`, `SRC OFF` |
| `lsa [file:line]` | show source around a line, or around PC | `SRC` |
| `!name ...` | any Casso engine command | the command itself |

## Excluded, with a defined reply

A command in these families replies `Error: no meaning on this machine`,
followed by which family it belongs to, and changes nothing. It is never
reported as unknown (FR-022d).

| Family | Examples |
|---|---|
| threads and processes | `~`, `\|`, `.process`, `.thread`, `.attach`, `.kill` |
| modules and symbol paths | `lm`, `.reload`, `.sympath`, `.srcpath`, `ld` |
| exceptions and events | `sx`, `sxe`, `sxd`, `.lastevent`, `!analyze` |
| kernel | `!pte`, `!pool`, `!irql`, `.trap` |
| dump files | `.dump`, `.opendump` |
| types and locals | `dt`, `dv`, `dx`, `??`, `dq`, `dp`, `.frame` |
| extensions and scripting | `.load`, `.chain`, `!ext.*`, `.scriptload`, `.foreach`, `.if`, `.while`, `as`, `$t0` |

Deferred (an error naming the feature until it exists): `wt`, which maps
onto the trace and the profile; `s -a` and `s -b`.

## Output

Follows WinDbg's documented layouts:

- `r`: `a=41 x=00 y=00 sp=f9 pc=0300 nv-bdizc` on one line, flags lowercase
  where clear and uppercase where set.
- `db`: `0300  a9 41 8d 00 04 60 ...  .A....` with two spaces between the
  address and the bytes and between the bytes and the ASCII.
- `bl`: one line per breakpoint, `0 e 0300 0001 (0001)  start`, with `e` or
  `d` for enabled or disabled, and `ba` entries marked `w1`/`r1`/`e1`.
- `u`: `0300 a941  lda #$41`, address, bytes, then the instruction, and a
  symbol line (`start:`) before a labeled address.
- `k`: `# call site  target        how` header, then one frame per line;
  a break prints as a line beginning `--` naming it (R-035).
- A stop: `Break instruction exception` is not printed; the stop reason and
  `r`'s line are, as AppleWin mode prints them.

The formatter tests fix each layout from WinDbg's command reference examples.