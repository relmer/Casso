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
| `-c` | Include cycle counts in the listing, in brackets between the bytes and the source text. |
| `-m` | Show macro expansions in the listing. |
| `-p` | Generate a pass 1 listing. |
| `-t` | Print the symbol table to stdout: each symbol with its address in hex and decimal, and a `*` on a redefinable one. |
| `-w [<width>]` | Wrap the listing at `<width>` columns. Default `79`; `-w` alone means `133`; `0` disables wrapping. AS65 documents the range as 60 to 200; Casso does not enforce it. Continuations indent to the source column, so wrapped text lines up under the text rather than under the address and bytes. |
| `-g <file>` | Write symbol addresses as `NAME=$ADDR`, **twice**: once ordered by address under a `; by address` heading, then again ordered by symbol name, case-insensitively, under `; by symbol`. Reading a debug file is two questions: what is at an address, and where a name went, and each order answers one. Casso's own format; no standard is being followed. |

#### What `-c` counts

The number is the **base** cost of the instruction: what every execution pays,
with nothing added for a condition the assembler cannot see. Three costs are
therefore excluded, because they depend on run-time state rather than on the
line:

- the extra cycle an indexed read pays when the address crosses a page — and,
  under `-x`, the same cycle for the `abs,X` shifts and rotates,
- the extra cycle a conditional branch pays when it is taken, and another when
  the branch crosses a page, and
- under `-x`, the extra cycle `ADC` and `SBC` pay while the decimal flag is set.

`BRA` is the one exception, listed at three rather than two: it has no
not-taken case, so the taken cycle is part of its base rather than a penalty.

Counts follow the CPU the source is assembled for, and they are the same numbers
the emulator bills when it executes the byte. `-x` selects the 65C02 set, where
two things cost differently from the NMOS part:

- `JMP (abs)` is six cycles rather than five. The page-wrap bug is fixed, at the
  price of a cycle.
- `ASL`, `LSR`, `ROL` and `ROR` in `abs,X` are six rather than seven. The NMOS
  part always spends the seventh; the 65C02 spends it only when the address
  crosses a page, so six is the base and the crossing is a penalty like any
  other. `INC` and `DEC` in `abs,X` are seven on both parts and are unaffected.

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

**`NOOPT` does not mean "assemble exactly as written."** It turns off the
jump substitution and nothing else. Zero-page selection is not affected: an
operand that resolves to `$00`–`$FF` still assembles to the two-byte zero-page
form, so `lda $0030` emits `A5 30` whether or not `NOOPT` is in force. AS65
behaves the same way — its manual lists zero-page substitution as an
optimization, but its `NOOPT` does not disable it either. Neither assembler
offers a way to force the absolute form.

**One deliberate divergence from AS65.** Under `NOOPT`, a forward reference to
a zero-page value makes AS65 emit a corrupt object: it sizes the instructions
as absolute in pass 1, emits the zero-page forms in pass 2, and writes the
original 7-byte span, leaving two stale bytes on the end. Its own listing shows
five bytes of code while it reports "Total size 7 bytes". Casso keeps the
absolute form, which is self-consistent and runs. This is the only case where
matching AS65 byte-for-byte would mean reproducing a defect.

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

One assembly *does* produce several outputs, which is what Merlin's `SAV` and a
second `DSK` do. That is not separate compilation: the outputs come from one
source read once, and nothing resolves a reference from one of them into
another.

If your project needs either, please open an issue at
[github.com/relmer/Casso/issues](https://github.com/relmer/Casso/issues);
the linker is tracked as [#112](https://github.com/relmer/Casso/issues/112) and
would benefit from a concrete case to be designed against.

---

## Building into a disk Casso already has open

A build loop that ends in a running emulator is one command:

```
CassoCli as65 prog.a65 --disk work.dsk --as PROG --type B --on-change reload
```

The same flag is on `disk put`, for a file the assembler did not produce:

```
CassoCli disk put work.dsk prog.bin --as PROG --type B --load $6000 --on-change reload
```

**There is no `--load` on the assembler**, and that is the point rather than an
omission: the address comes from the origin the source declared, so the two
cannot disagree.

Casso detects the change on its own, whichever tool wrote it. `--on-change`
specifies what happens next:

| | |
|---|---|
| `reload` | Insert the modified disk; leave the machine running |
| `reboot` | Insert the modified disk and reboot the machine |
| *(omitted)* | Casso prompts you |

Stating it when no emulator is running is not an error, so a build script does
not need to know whether you have Casso open.

**A pick-up is a disk swap, and swapping a disk under a running program cannot
be made safe.** The Apple keeps the disk's directory in its own memory, where
nothing on the host can see or correct it, so a program that was running before
the swap may not see the new disk correctly. Casso says so, and the reboot is on
the toolbar. `restart` is the answer that removes the question.

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
| `-l` | Generate a listing beside each object, named after it. Takes no filename. |
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

### The line model

Fields, not columns. Runs of spaces or tabs separate the label, opcode, operand
and comment fields, and the only significant column is the first: a line
beginning with whitespace has no label. Tabs are never expanded, tab stops
affect display and nothing else, and the tidy columns in a Merlin listing are
the editor's doing.

A `*` in column 1 is a whole-line comment. So is a `;` in column 1, which is not
a separate rule: with no label present, column 1 is the first field boundary.

A semicolon is **not** a comment introducer anywhere else. Inside the operand it
is data, and it is how Merlin separates macro arguments, `ADD SUMSTR;DEFLEN;PL`
passes three.

### Directives

| Directive | What it does |
|---|---|
| `ASC` `DCI` `INV` `FLS` `STR` `REV` | String data in six encodings. The delimiter is **any** character, taken from the text itself, and it selects high or low ASCII. A trailing hexadecimal run after the closing delimiter is part of the operand. |
| `DFB` `DB` | Bytes |
| `DA` `DW` | Words, low byte first |
| `DDB` | Words, **high** byte first |
| `HEX` | Raw hexadecimal digit pairs |
| `DS` | Reserve space |
| `ORG` | Origin. It **relocates**: output stays one contiguous stream and only the program counter moves. With no operand it resyncs the program counter to where output has actually reached. |
| `DSK` | Names the output. A name supplied with `-o` or `--as` beats it. **A second one closes the file the first opened and begins another**, so a source carrying two produces two files with no `SAV` anywhere. The name stays in effect until another replaces it. |
| `TYP` | Sets the filesystem type the output takes, as a ProDOS type byte. The accepted set is Merlin's own, from its manual: `$00` no type, `$06` binary, `$F0`–`$F7` command and user-defined, and `$FF` system. `$04` text and `$FC` Applesoft are accepted besides, which Merlin lists in neither direction; anything else is refused naming the byte, as Merlin's `ILLEGAL FILE TYPE` does. On DOS 3.3 only `$04`, `$06` and `$FC` have counterparts and the rest are refused by name, because DOS 3.3 has five types and none of them means a system program, a command file, or no type at all. `--type` beats it. **`TYP` is ProDOS-only in real Merlin** — Merlin Pro 2.23 under DOS 3.3 answers `Bad opcode` to it — so writing a typed output onto a DOS 3.3 volume is this tool going beyond the period assembler rather than matching it. |
| `SAV` | Writes the span accumulated since the previous save and carries on, so one source produces several files. **The accumulation is emptied**, so no byte appears in two outputs, and each output records the address its own first byte assembles to. A name is required. |
| `END` | End of assembly |
| `PUT` `USE` | Include another file. The operand is a short name; Merlin prepends `T.` to reach the file on disk. |
| `DO` `ELSE` `FIN` | Conditional assembly |
| `MAC` `<<<` | Macro definition and its terminator |
| `LUP` `--^` | Repeat block and its terminator |
| `DUM` `DEND` | Dummy section: assigns addresses, emits nothing |
| `ERR` | Assembly-time assertion. `ERR expr` fails when the expression is non-zero; `ERR \expr` fails when the assembly has grown past `expr`. |
| `VAR` | Binds the positional parameters `]1`..`]9` with no macro call, so a fragment pulled in with `PUT` can be parameterized. Values are separated by `;`, the same character macro arguments use. |
| `KBD` | Binds the symbol in the label field to an answer supplied from outside. See below. |
| `PAG` `TR` `EXP` `AST` | Listing control; no object byte changes |
| `XC` | Selects the 65C02 (first occurrence only, see above) |

#### One source, several outputs

`SAV` and a second `DSK` both cut a source into more than one file, and
everything the assembly writes follows the cut. Each object gets its own
listing, named after it: `SAV LOADER` produces `LOADER` and `LOADER.lst`.

The equates and macro definitions above the first output are repeated into every
listing rather than left in the first, because a listing a reader opens on its
own has to resolve the names its code refers to.

That is why `-l` takes no filename under Merlin. One name cannot serve several
listings, and the objects already supply the names. `as65 -l` is unchanged: it
keeps its filename and its standard-output default, and an as65 source has no
directive that could produce a second output.

### Symbols and expressions

- **Local labels.** A leading `:` scopes a label to the global label above it, so
  the same short name may be reused throughout a file.
- **Variable symbols.** `]NAME` is a reassignable symbol; `]NAME = expr` assigns
  it, and it may also stand as a label, taking the program counter, as many
  times as the source likes. Each reference resolves against the assignment most
  recently above it.
- **Positional parameters.** `]1` through `]9` are macro arguments inside a
  macro body, and are bound by `VAR` outside one.
- **`?` is legal inside a symbol name.**
- **No operator precedence.** Expressions fold strictly left to right, and
  parentheses are the only grouping.
- **Unsigned 16-bit arithmetic**, operands and intermediates alike.
- `!` is exclusive-or and `.` is inclusive-or.
- `"A"` is a high-ASCII character constant; `'A'` is the plain character.
- The byte selector written straight after `#` picks a byte out of the **whole**
  expression: `LDA #>HERE-1` takes the high byte of `HERE-1`.

### Instructions

- `BLT` and `BGE` are Merlin's names for `BCC` and `BCS`.
- A shift or rotate is written with no operand at all for accumulator mode.
- The default origin is `$8000` when the source names none.

### Macros

Invoked by bare name, or explicitly with `>>>`. Arguments are separated by `;`.
Every label a macro body defines is made unique per expansion, with no
declaration to say so, Merlin's own sources expand one macro three times, each
redefining a bare label.

A parameter the body refers to with no argument to fill it is an **error**. It
is not substituted with nothing: the commonest cause is a call punctuated for
another assembler, which arrives as one argument however many were meant, and
silently assembling that would produce a different program.

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

Four constructs are recognized and refused individually, so a refusal identifies
the construct and points at the issue tracking it, where an unknown-directive
error would read as "Merlin support is broken". Crossing the boundary stops the
assembly before pass 2 and exits 2. The why and the way forward are in the table
below rather than in the diagnostic.

| Construct | What it is | Why it is refused | Widens with |
|---|---|---|---|
| `REL` | Relocatable-mode assembly | Produces a relocatable module for a linker to place; Casso emits one absolutely located image | A relocating linker ([#112](https://github.com/relmer/Casso/issues/112)) |
| `ENT` | An entry symbol declaration | Publishes a symbol for a linker to resolve from another module | A relocating linker ([#112](https://github.com/relmer/Casso/issues/112)) |
| `EXT` | An external symbol declaration | The symbol is defined in another module, which would require linker support | A relocating linker ([#112](https://github.com/relmer/Casso/issues/112)) |
| `XC` (second one) | A second CPU-selection directive | One selects the 65C02; a second selects the 65802/65816, which Casso does not emulate | A 65802/65816 core |

Four things about that list are worth reading twice.

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
- **It was six, and `TYP` and `SAV` left.** The file-type directive set a
  filesystem type with no filesystem to set it on; the assembler writes onto a
  volume now, so the type has somewhere to land. The save-object directive was
  waiting on a decision about multi-output assembly rather than on a capability,
  and that decision was made: it writes the span accumulated since the previous
  save and carries on. Both are documented with the other supported directives
  above.

The four constructs and the issues they point at come from one table in
`CassoCore/MerlinSubsetBoundary.cpp`, which is what the refusals are composed
from. The why and widens-with columns above are maintained here, so a row added
to that table needs a row added here as well.

### Case

**Directives, mnemonics and the alternate branch names are taken in any case.**
`LDA`, `lda` and `Lda` are one instruction, and they emit the same byte.

Real Merlin ran on hardware with no lower case, so no vendor source can settle
this and the corpus never will. Casso is deliberately wider here: a dialect that
accepts more than the original cannot reject a source the original would have
assembled, and Merlin source written in a Windows editor arrives lower-case.

**Symbols are a different question and stay case-sensitive.** A label written
`lda` is legal, period sources do it, and is accepted with a warning that it
resembles an instruction, rather than being refused.

### Strictness

There is no lenient superset. A source is read under the dialect its invocation
names and no other, so an `as65` construct in a Merlin file is rejected, and
the diagnostic says which dialect defines it, rather than reporting an unknown
instruction.

The commonest first mistake has its own message: a Merlin label must begin in
column 1, and one written indented is read as the opcode field with the
instruction beside it as its operand. Casso says so instead of reporting that
the label is not an instruction.

### Deliberate divergences and unverified corners

Casso's Merlin support was built against vendor source and the object files
Merlin itself produced from it, so most of the above is settled by bytes. These
are the places where it is not, recorded so they are not mistaken for verified
behavior.

- **`INV`, `FLS` and `STR` follow documentation, not bytes.** `INV` appears once
  in the vendor corpus, in a file shipping no object; `FLS` and `STR` appear
  nowhere.
- **`DDB` appears nowhere in the corpus.** It is implemented because absence
  from one vendor's source is not absence from the language.
- **`ERR \expr`'s boundary may be exclusive or inclusive.** Merlin documents the
  check as firing when the address *exceeds* the ceiling; no vendor use lands on
  its own limit, so the corpus cannot tell `>` from `>=`.
- **The explicit `>>>` macro invocation is unverified.** The vendor library
  invokes every macro by bare name.
- **`VAR` binds values, not text.** A parameter pasted into a longer identifier
, which textual substitution inside a macro body would splice, resolves here
  as one symbol instead. No vendor line does it, so the corpus cannot say which
  reading Merlin takes outside a macro.
- **The inclusive-or character is also the local-label scope joiner.** The
  expression tokenizer reads an identifier greedily, so `LABEL.OTHER` lexes as
  one symbol where Merlin would read an operation. Every use on the vendor disk
  follows a digit, where no identifier is being scanned, so the corpus cannot
  force the other reading; a source that needs it would.

### How the subset is verified

Correctness is measured against Glen Bredon's own assembler: six objects shipped
on the Merlin Pro 2.23 disk in 1984, reproduced byte for byte from their vendor
sources. `scripts/RunMerlinOracles.ps1` runs that check through the executable.
