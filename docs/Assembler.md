# Casso Assembler

`CassoCli` is a 6502 / 65C02 cross-assembler with an as65-compatible command
line, plus a built-in runner that assembles and executes in one step.

It is deliberately drop-in for [as65](http://www.kingswood-consulting.co.uk/assemblers/):
an option omitted behaves the way as65's did, down to the `$FF` fill byte, the
`$8000` load address and the full 64 KB output image. Existing scripts keep
working; the modern conveniences are opt-in.

- [Invocation](#invocation)
- [Assembler flags](#assembler-flags)
- [Output formats](#output-formats)
- [Running code](#running-code)
- [Language reference](#language-reference)
- [Examples](#examples)
- [Dialects](#dialects)

---

## Invocation

```
CassoCli <source> [flags]              assemble
CassoCli run <binary | source> [opts]  assemble-and-run, or run a binary
CassoCli --help | -?
CassoCli --version
```

The source file may be given without an extension, in which case `.a65`, `.asm`
and `.s` are tried in that order.

Flags accept either prefix: `-o` and `/o` are the same flag. Whichever you type
is remembered, so usage text and diagnostics come back spelled the way you
invoked the tool.

Short flags concatenate as65-style, with a value-taking flag last:

```powershell
CassoCli input.a65 -tlfile        # same as -t -l file
```

---

## Assembler flags

### Output

| Flag | Meaning |
|---|---|
| `-o <file>` | Output file. Default: the input with a `.bin` extension. |
| `--raw` | Write only the assembled bytes, unpadded. |
| `--dos-bin` | Write the assembled bytes behind a 4-byte DOS 3.3 header (load address + length), ready to `BLOAD`. |
| `-s` | Motorola S-record (`.s19`). |
| `-s2` | Intel HEX (`.hex`). |
| `-z` | Fill unused space with `$00`. Default is `$FF`. |

With no format flag the output is a **full 64 KB image** padded with the fill
byte — as65's behavior, and the right shape for ROM burning or reference
comparison. An explicit format flag wins over the output file's extension; the
extension is consulted only when no flag is given.

### CPU target

| Flag | Meaning |
|---|---|
| `--cpu 6502` | Strict NMOS 6502. **Default.** |
| `--cpu 65c02` | CMOS 65C02: `STZ`, `BRA`, `TSB`/`TRB`, `PHX`/`PHY`/`PLX`/`PLY`, `RMBn`/`SMBn`/`BBRn`/`BBSn`, and the `(zp)` and `(abs,X)` modes. |

Under `--cpu 6502` a 65C02-only opcode is rejected as invalid rather than
silently assembled, so targeting the wrong CPU is a build error and not a
runtime surprise.

### Listing and symbols

| Flag | Meaning |
|---|---|
| `-l [<file>]` | Generate a listing. `-l` alone goes to stdout; `-l file` writes to a file. |
| `-c` | Include cycle counts in the listing. |
| `-m` | Show macro expansions in the listing. |
| `-p` | Generate a pass 1 listing. |
| `-t` | Generate a symbol table. |
| `-g [<file>]` | Generate a debug information file. |

### Symbols and diagnostics

| Flag | Meaning |
|---|---|
| `-d <name>[=<value>]` | Pre-define a symbol. Without a value it is defined as `1`. |
| `--warn` | Report warnings. **Default.** |
| `--no-warn` | Suppress warnings. |
| `--fatal-warnings` | Treat warnings as errors. |
| `-v` | Verbose. |
| `-q` | Quiet — suppress progress output. |

> The three warning flags are accepted but are not yet listed in `--help`.

### Already the behavior

| Flag | Meaning |
|---|---|
| `-i` | Ignore case in **opcodes**, so `adc` and `ADC` are the same instruction. Labels stay case-sensitive. |

Casso is at parity with as65 here without the flag doing anything: opcode and
directive lookups always ignore case, and labels are always case-sensitive —
which is exactly the behavior `-i` asks for. Passing it or omitting it gives
the same, correct result.

### Accepted and not yet implemented

Parsed so an as65 invocation is not refused, then read by no code. Tracked by
[#118](https://github.com/relmer/Casso/issues/118).

| Flag | as65 behavior |
|---|---|
| `-h <lines>` | Listing page height. |
| `-n` | Disable optimizations, overriding the `OPT` pseudo-instruction. |
| `-w [<width>]` | Listing column width. |

`OPT` and `NOOPT` are likewise accepted and ignored as directives, so there is
currently nothing for `-n` to switch off.

---

## Output formats

| Format | Flag | Shape |
|---|---|---|
| Full image | *(default)* | 64 KB, padded with the fill byte. |
| Raw | `--raw` | Only the assembled span. |
| DOS 3.3 binary | `--dos-bin` | Load address (2 bytes, little-endian), length (2 bytes), then the span. `BLOAD`-ready. |
| S-record | `-s` | Motorola S19 text. |
| Intel HEX | `-s2` | Intel HEX text. |

---

## Running code

`run` takes either a binary or a source file. Given source, it assembles first
and runs the result — no intermediate file.

| Option | Meaning |
|---|---|
| `--load <addr>` | Load address. Default `$8000`. |
| `--entry <addr>` | Entry point. Defaults to the load address. |
| `--reset-vector` | Take the entry point from the reset vector at `$FFFC`/`$FFFD`. |
| `--stop <addr>` | Stop when the program counter reaches this address. |
| `--max-cycles <n>` | Stop after this many cycles. |
| `-v` | Verbose. |

Addresses accept `$8000` or `0x8000`.

---

## Language reference

| Feature | Syntax |
|---|---|
| Mnemonics | All 56 standard, plus the 65C02 set under `--cpu 65c02` |
| Addressing modes | `#$42`, `$30`, `$30,X`, `$1234`, `$1234,X`, `($20,X)`, `($20),Y`, `A` |
| Rockwell bit ops | `RMB 0,$30` operand form, or the suffixed `RMB0 $30` / `BBR3 $30,target` form |
| Labels | `loop: DEX` … `BNE loop` |
| Constants | `value = $42`, `carry equ %00000001` — chains and forward references resolve |
| Origin and data | `.org $8000`, `.byte $FF`, `.word $1234`, `.text "hello"` |
| Sections | `code`, `data`, `bss` |
| Conditionals | `if` / `ifdef` / `ifndef` / `else` / `endif` |
| Macros | `name macro` … `endm`, with arguments and `\` line continuation |
| Includes | `include "file.a65"` |
| Comments | `; whole line`, or trailing: `LDA #$42 ; inline` |
| Numbers | `$FF` hex, `%10101010` binary, `255` decimal |
| Expressions | `+ - * / % & \| ^ ~ << >>`, `<label` low byte, `>label` high byte, `*` current PC |
| Listing control | `.page` is accepted and acts at listing time |
| Case | Mnemonics and directives are matched case-insensitively; **labels are case-sensitive**. The asymmetry is deliberate — period sources write instructions in either case, but folding label case would silently merge `foo` and `FOO` into one symbol. |

---

## Examples

Assemble to the default full 64 KB image:

```powershell
CassoCli input.a65 -o output.bin
```

Listing to a file, with a symbol table and cycle counts:

```powershell
CassoCli input.a65 -o output.bin -l listing.txt -t -c
```

A `BLOAD`-ready DOS 3.3 binary, or just the assembled bytes:

```powershell
CassoCli input.a65 --dos-bin -o HELLO.BIN
CassoCli input.a65 --raw     -o payload.bin
```

S-record or Intel HEX:

```powershell
CassoCli input.a65 -s  -o output.s19
CassoCli input.a65 -s2 -o output.hex
```

Pre-define a symbol, and make warnings fatal:

```powershell
CassoCli input.a65 -d DEBUG=1 --fatal-warnings -o output.bin
```

65C02 source — CMOS opcodes are rejected without the flag:

```powershell
CassoCli input.a65c --cpu 65c02 -o output.bin
```

Assemble and run in one step, stopping at a known address:

```powershell
CassoCli run input.a65 --stop $8010
```

Run a pre-assembled binary at a chosen address, bounded by a cycle budget:

```powershell
CassoCli run output.bin --load $8000 --max-cycles 100000
```

Take the entry point from the reset vector, as a ROM image would:

```powershell
CassoCli run rom.bin --reset-vector
```

---

## Dialects

The assembler currently accepts **one** source dialect: as65 syntax, as
documented above.

**Merlin support is in development** on the `019-assembler-dialects` branch and
is *not* present in this build. That work introduces a dialect mechanism —
selecting a syntax rather than hard-coding one — with Merlin as its first
additional dialect. Until it merges, Merlin sources are not accepted and there
is no dialect flag to pass.

`ca65` is planned after that (spec 023), and depends on the same mechanism.
