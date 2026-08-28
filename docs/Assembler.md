# Casso Assembler

`CassoCli` is a 6502 / 65C02 cross-assembler with an AS65-compatible command
line, plus a built-in runner that assembles and executes in one step.

It is deliberately drop-in for [AS65](http://www.kingswood-consulting.co.uk/assemblers/):
an option omitted behaves the way AS65's did, down to the `$FF` fill byte, the
`$8000` load address and the shape of the output: the assembled bytes, from the
lowest address the source used to the highest. Existing scripts keep working;
the modern conveniences are opt-in.

- [Invocation](#invocation)
- [Assembler flags](#assembler-flags)
- [Output formats](#output-formats)
- [Running code](#running-code)
- [One source, one image](#one-source-one-image)
- [Language reference](#language-reference)
- [Examples](#examples)
- [Dialects](#dialects)
  - [Merlin](#merlin)
  - [Where Merlin support ends](#where-merlin-support-ends)

---

## Invocation

```
CassoCli as65 <source> [flags]         assemble as65 syntax
CassoCli merlin <source> [flags]       assemble Merlin syntax
CassoCli run <binary> [opts]           run a binary
CassoCli run <source> --as65|--merlin [opts]
                                       assemble under the named dialect, then run
CassoCli --help | -?
CassoCli --version
```

**The dialect is named, not guessed.** `CassoCli as65 input.a65 -o out.bin`, the
form before dialect support, no longer works; insert `as65` after the
executable and nothing else changes. An unrecognized first argument used to be
assumed to be an AS65 source file, and a dialect the tool infers is a dialect
nobody stated.

The source file may be given without an extension, in which case `.a65`, `.asm`
and `.s` are tried in that order. Both dialects use that same list, so a Merlin
source saved as `PROG.S` is found by `merlin PROG`.

Flags accept either prefix: `-o` and `/o` are the same flag, and `--dos-bin` and
`/dos-bin` are the same long option. The prefix you type **first** is the one
remembered, so usage text and diagnostics come back written the way you invoked
the tool even if the rest of the command line mixes the two.

**An argument the grammar does not know is refused, not skipped.** The full
usage is printed, the line naming the argument comes last, and the process
exits 2 without assembling.

Short flags concatenate AS65-style, with a value-taking flag last:

```powershell
CassoCli as65 input.a65 -tlfile        # same as -t -l file
```

---

## Assembler flags

### Output

| Flag | Meaning |
|---|---|
| `-o <file>` | Output file. Default: the input with a `.bin` extension. |
| `--flat` | Write a full 64KB image with the bytes at their origin, padded with the fill byte. |
| `--dos-bin` | Write the assembled bytes behind a 4-byte DOS 3.3 header (load address + length), ready to `BLOAD`. |
| `-s` | Motorola S-record (`.s19`). |
| `-s2` | Intel HEX (`.hex`). |
| `-z` | Fill unused space with `$00`. Default is `$FF`. |

With no format flag the output is **the assembled bytes and nothing around
them**, running from the lowest address the source used to the highest. That is
AS65's behavior, and it has no flag of its own because it is what you get by
asking for nothing.

`--flat` is the one that pads, and it is the shape a ROM burner takes or a
byte-for-byte comparison against a reference image needs.

**The four format flags are mutually exclusive, and naming two is refused.**
`-s --flat` fails, naming both flags, rather than quietly writing one of them:
each flag is valid alone, so nothing in the output would look like a
mistake. The same flag repeated is not a conflict. The output file's extension
(`.s19`, `.hex`) is consulted only when no flag was given at all, so an
explicit flag always beats it.

### CPU target

| Flag | Meaning |
|---|---|
| *(omitted)* | Strict NMOS 6502. **Default.** |
| `-x` | The 65C02: `STZ`, `BRA`, `TSB`/`TRB`, `PHX`/`PHY`/`PLX`/`PLY`, `RMBn`/`SMBn`/`BBRn`/`BBSn`, and the `(zp)` and `(abs,X)` modes. |

Without `-x` a 65C02-only opcode is rejected as invalid rather than
silently assembled, so targeting the wrong CPU is a build error and not a
runtime surprise.

**Casso accepts more than AS65 does here.** AS65 is an assembler "for the 6502
and 65SC02 microprocessors", and `-x` enables that 65SC02 set. Casso's `-x`
enables the **Rockwell R65C02**, which is a superset: everything AS65 accepts,
plus the bit operations `RMBn`, `SMBn`, `BBRn` and `BBSn`, for which a 65SC02
has no opcodes.

The practical consequence runs one way. A source written for the 65SC02
assembles identically under both. A source using the Rockwell bit operations
assembles under Casso and **would not** under AS65, and Casso will not warn
that it has left the 65SC02 behind. The wider set is deliberate: Apple's //c
ROM 4 and the Enhanced //e firmware use those instructions, and the emulator
has to run them.

WDC's `WAI`/`STP` are excluded, not part of the Rockwell devices Apple
shipped, and their opcode slots behave as NOPs.

### Listing and symbols

| Flag | Meaning |
|---|---|
| `-l [<file>]` | Generate a listing. `-l` alone goes to stdout; `-l file` writes to a file. |
| `-c` | Include cycle counts in the listing. |
| `-m` | Show macro expansions in the listing. |
| `-p` | Generate a pass 1 listing. |
| `-t` | Print the symbol table to stdout: each symbol with its address in hex and decimal, and a `*` on a redefinable one. |
| `-w [<width>]` | Wrap the listing at `<width>` columns. Default `79`; `-w` alone means `133`; `0` disables wrapping. AS65 documents the range as 60 to 200; Casso does not enforce it. Continuations indent to the source column, so wrapped text lines up under the text rather than under the address and bytes. |
| `-g <file>` | Write symbol addresses as `NAME=$ADDR`, **twice**: once ordered by address under a `; by address` heading, then again ordered by symbol name, case-insensitively, under `; by symbol`. Reading a debug file is two questions: what is at an address, and where a name went, and each order answers one. Casso's own format; no standard is being followed. |

### Symbols and diagnostics

| Flag | Meaning |
|---|---|
| `-d <name>[=<value>]` | Define a symbol. Without a value it is defined as `1`. |
| `--warn` | Report warnings. **Default.** |
| `--no-warn` | Suppress warnings. |
| `--fatal-warnings` | Treat warnings as errors. |
| `-v` | Verbose: pass progress, assembly time, and a summary of output name, byte count, start and end address and symbol count. All on stderr, so it never mixes with a listing on stdout. |
| `-q` | Quiet, suppress progress output. |

> The three warning flags are accepted but are not yet listed in `--help`.

### Optimization

| Flag | Meaning |
|---|---|
| `-n` | Disable optimizations. Permanent: an `OPT` later in the source cannot turn them back on. |

Optimization is **on by default**, as it is in AS65, and there is one of them:
under `-x`, a `JMP` to an address the assembler has already seen is emitted as
a two-byte `BRA` when the target is within a branch's reach. The target must
already be defined — a forward reference stays a three-byte `JMP` — and the
displacement must fit in a signed byte, so `JMP` is still what you get across a
long file. Without `-x` there is no `BRA` to emit and nothing changes.

`NOOPT` turns the substitution off from that line on, `OPT` turns it back on,
and `-n` outranks both.

With `-c`, a substituted line reports the branch's timing rather than the
jump's: `BRA` is three cycles, and four when the branch crosses a page
boundary. `JMP` absolute is always three.

### Accepted but not yet implemented

Both are parsed and then read by no code, so passing them changes nothing.
They exist so an AS65 invocation is not refused outright. Tracked by
[#118](https://github.com/relmer/Casso/issues/118).

| Flag | AS65 behavior | Casso today |
|---|---|---|
| `-i` | Ignore case in **opcodes**, so `adc` and `ADC` are the same instruction. Labels stay case-sensitive. | Accepted, ignored: and it has nothing left to switch on, because opcodes are matched case-insensitively either way. That is now a deliberate rule rather than a coincidence; see [Case](#case). |
| `-h <lines>` | Listing page height; `0` disables pagination. | Accepted, ignored. The listing is not paginated at all. |

---

## Output formats

| Format | Flag | What is written |
|---|---|---|
| Assembled span | *(default)* | Only the bytes the source assembled, with no address. |
| Full image | `--flat` | 64KB, the bytes at their origin, padded with the fill byte. The only format that pads. |
| DOS 3.3 binary | `--dos-bin` | Load address (2 bytes, little-endian), length (2 bytes), then the span. `BLOAD`-ready. |
| S-record | `-s` | Only the assembled span, as Motorola S1 records that each carry their address. |
| Intel HEX | `-s2` | Only the assembled span, as Intel HEX records that each carry their address. |

Put another way: `--flat` answers "what is in memory", and the other four
answer "what was assembled". Three of those four know where it goes (the DOS
header, the S-record address field, the HEX address field) and the default is
the one that does not.

---

## Running code

`run` takes either a binary or a source file. Given source, it assembles first
and runs the result, no intermediate file.

**`run` names its assembler, and a source must name one.** `--as65` or
`--merlin` decides which dialect reads a source. There is no default: which
assembler reads a file decides what the file means, so a source given with
neither flag is refused rather than assembled under a guess, exactly as the
bare `CassoCli input.a65` form was. A binary is unaffected — it needs no
assembler, so there is nothing to name.

After the dialect, that assembler's own switches are accepted for the ones that
change what is assembled. The rest describe a file `run` never writes.

| Option | Meaning |
|---|---|
| `--as65` | Assemble the source as AS65. Takes `-x` and `-d` as well. |
| `--merlin` | Assemble the source as Merlin. Takes `-d` as well. Merlin selects its CPU in the source, with `XC`. |
| `-x` | The 65C02, as in AS65 mode. `--as65` only. |
| `-d <name>[=<value>]` | Define a symbol, as in the assembler's own mode. |
| `--load <addr>` | Load address. Default `$8000`. |
| `--entry <addr>` | Entry point. Defaults to the load address. |
| `--reset-vector` | Take the entry point from the reset vector at `$FFFC`/`$FFFD`. |
| `--stop <addr>` | Stop when the program counter reaches this address. |
| `--max-cycles <n>` | Stop after this many cycles. |
| `-v` | Verbose: what was assembled or loaded, the entry point, the stop reason, the cycle count, and the final register file. On stderr. |

Addresses accept `$8000` or `0x8000`.

---

## One source, one image

**Casso assembles a single source file into a single absolutely located image.
There is no linker, and no way to assemble several sources into one program.**

A source may pull in others (`include` under AS65, `PUT`/`USE` under Merlin)
but that is textual inclusion: the result is still one assembly producing one
image. What is absent is separate compilation: assembling several files
independently and resolving references between them afterward. That is why
Merlin's `REL`, `ENT` and `EXT` are refused, and why AS65's own relocatable
output has no equivalent here.

Nor does one assembly produce several outputs, which is why Merlin's `SAV` is
refused.

If your project needs either, please open an issue at
[github.com/relmer/Casso/issues](https://github.com/relmer/Casso/issues);
the linker is tracked as [#112](https://github.com/relmer/Casso/issues/112) and
would benefit from a concrete case to be designed against.

---

## Language reference

| Feature | Syntax |
|---|---|
| Mnemonics | All 56 standard, plus the 65C02 set under `-x` |
| Addressing modes | `#$42`, `$30`, `$30,X`, `$1234`, `$1234,X`, `($20,X)`, `($20),Y`, `A` |
| Rockwell bit ops | `RMB 0,$30` operand form, or the suffixed `RMB0 $30` / `BBR3 $30,target` form |
| Labels | `loop: DEX` … `BNE loop` |
| Constants | `value = $42`, `carry equ %00000001`, chains and forward references resolve |
| Origin and data | `.org $8000`, `.byte $FF`, `.word $1234`, `.text "hello"` |
| Sections | `code`, `data`, `bss` |
| Conditionals | `if` / `ifdef` / `ifndef` / `else` / `endif` |
| Macros | `name macro` … `endm`, with arguments and `\` line continuation |
| Includes | `include "file.a65"` |
| Comments | `; whole line`, or trailing: `LDA #$42 ; inline` |
| Numbers | `$FF` hex, `%10101010` binary, `255` decimal |
| Expressions | `+ - * / % & \| ^ ~ << >>`, `<label` low byte, `>label` high byte, `*` current PC |
| Listing control | `.page` is accepted and acts at listing time |
| Optimization control | `OPT` and `NOOPT` switch the `JMP`-to-`BRA` substitution on and off; see [Optimization](#optimization) |
| Case | Mnemonics, directives and instruction aliases are matched case-insensitively in **both** dialects; **labels are case-sensitive**. The asymmetry is deliberate: period sources write instructions in either case, but folding label case would silently merge `foo` and `FOO` into one symbol. A label written `lda` stays legal, and is warned about rather than refused. |

---

## Examples

Assemble to the default, the assembled bytes on their own:

```powershell
CassoCli as65 input.a65 -o output.bin
```

Listing to a file, with a symbol table and cycle counts:

```powershell
CassoCli as65 input.a65 -o output.bin -l listing.txt -t -c
```

A `BLOAD`-ready DOS 3.3 binary, or just the assembled bytes:

```powershell
CassoCli as65 input.a65 --dos-bin -o HELLO.BIN
CassoCli as65 input.a65           -o payload.bin
```

S-record or Intel HEX:

```powershell
CassoCli as65 input.a65 -s  -o output.s19
CassoCli as65 input.a65 -s2 -o output.hex
```

Pre-define a symbol, and make warnings fatal:

```powershell
CassoCli as65 input.a65 -d DEBUG=1 --fatal-warnings -o output.bin
```

65C02 source, CMOS opcodes are rejected without the flag:

```powershell
CassoCli as65 input.a65c -x -o output.bin
```

Assemble and run in one step, stopping at a known address:

```powershell
CassoCli run input.a65 --as65 --stop $8010
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

The assembler reads **two** source dialects, and the invocation names which:
`as65` or `merlin`. Everything above describes the as65 grammar; this section
is Merlin's.

`ca65` is planned next (spec 023) and depends on the same mechanism.

### Merlin

```
CassoCli merlin <source> [flags]
```

| Flag | Meaning |
|---|---|
| `-o <file>` | Rename output file. Default: `<source>.bin`, unless the source names one itself. |
| `-l [<file>]` | Generate a listing. `-l` alone goes to stdout. |
| `-d <symbol>[=<value>]` | Define a symbol the source expects. Without a value it is defined as `1`. |
| `-v` | Verbose: an assembly summary on stderr. |
| `--dos-bin` | Write the bytes behind a 4-byte DOS 3.3 header (origin + length), ready to `BLOAD`. |
| `--flat` | Write a full 64KB image with the bytes at their origin, padded with `$FF`. |

**The default output is the assembled bytes and nothing around them**, which is
what `as65` writes by default too. A Merlin source names its own origin, and
Merlin's
origin directive *relocates* rather than seeks, so one contiguous object can
carry sections destined for several addresses; padding it out to an
address-indexed image would scatter them.

`--dos-bin` is the one worth reaching for. The header carries the **origin**,
and the default output throws it away, so wrapping the bytes by hand afterward
means already knowing an address that usually comes from an `ORG` line rather
than from your command line. There is no `-z`: `--flat` always pads with `$FF`,
and neither of the other two formats pads at all.

**There is no CPU switch.** Merlin selects its processor in the source, with
`XC`, so `-x` is refused by name here rather than ignored. A switch accepted
here would assemble source the real assembler rejects.

### `KBD`, and how `-d` answers it

`KBD` stops a Merlin assembly and asks the operator for a value. A batch
assembler has nobody to ask, so in Casso the answer arrives **before** the
assembly rather than during it.

```
SAVOBJ KBD Save object code? (1=yes, 0=no)
```

The **label field** names the symbol; the rest of the line is the prompt. When
pass 2 reaches that line, Casso looks the name up among the symbols `-d`
defined:

- **Answered**: the symbol is defined there and then, as an ordinary
  immutable equate, so every `DO SAVOBJ` below it sees the value. `-d SAVOBJ=0`
  gives zero; a bare `-d SAVOBJ` gives 1.
- **Unanswered**: an error naming the symbol *and quoting the source's own
  prompt*, so the question is legible without opening the file:
  `No answer supplied for SAVOBJ (Save object code? (1=yes, 0=no)); define it
  on the command line, for example -d SAVOBJ=0`.

**An unanswered `KBD` is an error rather than a zero.** Both easier options are
wrong: blocking on a prompt hangs an unattended build, and defaulting quietly
assembles a program nobody asked for, since these symbols gate whole sections.

`CLOCK.S` is the case that proves it, one source, two objects Merlin shipped,
chosen entirely by the answers:

```powershell
CassoCli merlin CLOCK.S -d SAVOBJ=0 -d VERSION=24 --dos-bin -o CLOCK.24
CassoCli merlin CLOCK.S -d SAVOBJ=0 -d VERSION=12 --dos-bin -o CLOCK.12
```

### Where Merlin support ends

Six constructs are recognized and **refused by name**, so a refusal says which
construct, why, and what would widen it, where an unknown-directive error
would read as "Merlin support is broken". Crossing the boundary stops the
assembly before pass 2 and exits 2.

| Construct | What it is | Why it is refused | Widens with |
|---|---|---|---|
| `REL` | Relocatable-mode assembly | Produces a relocatable module for a linker to place; Casso emits one absolutely located image | A relocating linker ([#112](https://github.com/relmer/Casso/issues/112)) |
| `ENT` | An entry symbol declaration | Publishes a symbol for a linker to resolve from another module | A relocating linker ([#112](https://github.com/relmer/Casso/issues/112)) |
| `EXT` | An external symbol declaration | Names a symbol defined in another module, and resolving that is what a linker is for | A relocating linker ([#112](https://github.com/relmer/Casso/issues/112)) |
| `XC` (second one) | A second CPU-selection directive | One selects the 65C02; a second selects the 65802/65816, which Casso does not emulate | A 65802/65816 core |
| `TYP` | The output file-type directive | Sets the filesystem file type of the output, which means nothing without a filesystem that has types | Disk file-access support, where filesystem types belong |
| `SAV` | The save-object directive | Writes the object accumulated so far and carries on, so one assembly produces several outputs | A decision about multi-output assembly |

Three things about that list are worth reading twice.

- **`XC` is cumulative, not forbidden.** The *first* occurrence is carried out
  and selects the 65C02, which Casso emulates. Only a second is refused.
- **A relocatable module that imports nothing has a way forward**, and the
  refusal states it in full: remove `REL`, drop the `ENT` declarations, and give
  the source an origin with `ORG`. A module carrying even one `EXT` does *not*,
  it references a definition living in another file, and no edit to this one
  supplies it. That advice is a property of the whole module, so one `EXT`
  anywhere removes it from every refusal in the file.
- **Every offender is reported, not the first.** Deciding whether to port a file
  needs the size of the gap, and stopping at the first refusal turns one answer
  into as many assembly runs as there are constructs.

The authority for all six rows is one table in
`CassoCore/MerlinSubsetBoundary.cpp`; the refusals and this list are composed
from its fields, so they cannot describe two different sets of rules.

### The supported subset

[docs/merlin-subset.md](merlin-subset.md) covers what Merlin support *does*
include, the field-based line model, the directive vocabulary, symbols and
expressions, macros, and every place the implementation is documentation-led
rather than settled by vendor bytes.

Correctness is measured against Glen Bredon's own assembler: six objects shipped
on the Merlin Pro 2.23 disk in 1984, reproduced byte for byte from their vendor
sources. `scripts/RunMerlinOracles.ps1` runs that check through the executable.
