# The Merlin subset

Casso assembles Glen Bredon's Merlin under `CassoCli merlin <source>`. It
supports the **absolute** subset: everything an assembly needs to produce one
located image on its own, and nothing that needs a linker, a processor Casso
does not emulate, or a decision Casso has not made.

This page exists so you can tell whether a Merlin project is inside that subset
**before** pointing the tool at it. If your source uses none of the constructs
under [Where support ends](#where-support-ends), it is in.

The tool will also tell you: `CassoCli --help` prints the same boundary, and the
authority for both is one table in `CassoCore/MerlinSubsetBoundary.cpp`. Every
refusal, every help line and the list below are composed from that table's own
fields, so they cannot describe different rules.

## Where support ends

Six constructs are recognized and refused **by name**. That distinction is the
point: a refusal says which construct, why, and what would widen the boundary,
where an unknown-directive error would read as "Merlin support is broken".

The block below is the output of `CassoCli --help`, verbatim.

```
Where merlin support ends:
REL -- relocatable-mode assembly [needs a linker]. it produces a relocatable module for a linker to place, and Casso emits one absolutely located image. Widens with a relocating linker (GitHub issue #112).
ENT -- an entry symbol declaration [needs a linker]. it publishes a symbol for a linker to resolve from another module. Widens with a relocating linker (GitHub issue #112).
EXT -- an external symbol declaration [needs a linker]. it names a symbol defined in another module, and resolving that is what a linker is for. Widens with a relocating linker (GitHub issue #112).
XC -- a second CPU-selection directive [needs a CPU Casso does not emulate]. one selects the 65C02 and a second selects the 65802/65816, which Casso does not emulate. Widens with a 65802/65816 core.
TYP -- the output file-type directive [owned by another part of Casso]. it sets the filesystem file type of the output, which means nothing without a filesystem that has types. Widens with Casso's disk file-access support, which is where filesystem file types belong.
SAV -- the save-object directive [undecided]. it writes the object accumulated so far and carries on, so one assembly produces several outputs. Widens with a decision about multi-output assembly, which disk file access will not settle.
```

Three things about that list are worth reading twice.

- **`XC` is cumulative, not forbidden.** The *first* occurrence is carried out
  and selects the 65C02, which Casso emulates. Only a second one is refused,
  because in Merlin that selects the 65802/65816.
- **A relocatable module that imports nothing has a way forward, and the
  refusal states it in full**: remove `REL`, drop the `ENT` declarations, and
  give the source an origin with `ORG`. A module carrying even one `EXT`
  declaration does *not* — it references a definition living in another file,
  and no edit to this one supplies it. The advice is a property of the whole
  module, so one `EXT` anywhere removes the workaround from every refusal in
  the file, including those written above it.
- **Every offender is reported, not the first.** A developer deciding whether
  to port a file needs the size of the gap, and stopping at the first refusal
  turns one answer into as many assembly runs as there are constructs.

Crossing the boundary stops the assembly before pass 2 and exits 2.

## What is supported

### The line model

Fields, not columns. Runs of spaces or tabs separate the label, opcode, operand
and comment fields, and the only significant column is the first: a line
beginning with whitespace has no label. Tabs are never expanded — tab stops
affect display and nothing else, and the tidy columns in a Merlin listing are
the editor's doing.

A `*` in column 1 is a whole-line comment. So is a `;` in column 1, which is not
a separate rule: with no label present, column 1 is the first field boundary.

A semicolon is **not** a comment introducer anywhere else. Inside the operand it
is data, and it is how Merlin separates macro arguments — `ADD SUMSTR;DEFLEN;PL`
passes three.

### Directives

| Spelling | What it does |
|---|---|
| `ASC` `DCI` `INV` `FLS` `STR` `REV` | String data in six encodings. The delimiter is **any** character, taken from the text itself, and it selects high or low ASCII. A trailing hexadecimal run after the closing delimiter is part of the operand. |
| `DFB` `DB` | Bytes |
| `DA` `DW` | Words, low byte first |
| `DDB` | Words, **high** byte first |
| `HEX` | Raw hexadecimal digit pairs |
| `DS` | Reserve space |
| `ORG` | Origin. It **relocates**: output stays one contiguous stream and only the program counter moves. With no operand it resyncs the program counter to where output has actually reached. |
| `DSK` | Names the output file. A name supplied with `-o` beats it. |
| `END` | End of assembly |
| `PUT` `USE` | Include another file. The operand is a short name; Merlin prepends `T.` to reach the file on disk. |
| `DO` `ELSE` `FIN` | Conditional assembly |
| `MAC` `<<<` | Macro definition and its terminator |
| `LUP` `--^` | Repeat block and its terminator |
| `DUM` `DEND` | Dummy section — assigns addresses, emits nothing |
| `ERR` | Assembly-time assertion. `ERR expr` fails when the expression is non-zero; `ERR \expr` fails when the assembly has grown past `expr`. |
| `VAR` | Binds the positional parameters `]1`..`]9` with no macro call, so a fragment pulled in with `PUT` can be parameterized. Values are separated by `;`, the same character macro arguments use. |
| `KBD` | Binds the symbol in the label field to an answer supplied from outside. See below. |
| `PAG` `TR` `EXP` `AST` | Listing control; no object byte changes |
| `XC` | Selects the 65C02 (first occurrence only — see above) |

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
declaration to say so — Merlin's own sources expand one macro three times, each
redefining a bare label.

A parameter the body refers to with no argument to fill it is an **error**. It
is not substituted with nothing: the commonest cause is a call punctuated for
another assembler, which arrives as one argument however many were meant, and
silently assembling that would produce a different program.

### Answering `KBD`

Merlin stops the assembly and prompts the operator at the keyboard. A batch
assembler has nobody to ask, so the answer arrives as a predefined symbol, on
the same channel every other externally supplied value uses.

A missing answer is an error naming the symbol and the source's own prompt.
Neither easier outcome is acceptable — blocking on a prompt hangs an unattended
build, and defaulting to zero cleanly assembles code nobody asked for, because
these symbols gate whole sections.

The flag is `-d`, spelled as it is for `as65`:

```
CassoCli merlin CLOCK.S -d SAVOBJ=0 -d VERSION=24 -o CLOCK24
CassoCli merlin CLOCK.S -d SAVOBJ=0 -d VERSION=12 -o CLOCK12
```

Repeat it once per question. A bare `-d SAVOBJ` answers 1, for a source that
tests only whether a symbol was given; `-d SAVOBJ=0` answers zero, which is a
different answer and not an absent one.

## What comes out

The default output is the assembled bytes and nothing around them. A Merlin
source names its own origin, so "the object" is what the subcommand writes.

Two flags wrap them differently:

```
CassoCli merlin CLOCK.S -d SAVOBJ=0 -d VERSION=24 --dos-bin -o CLOCK.24
```

- `--dos-bin` writes the bytes behind a 4-byte DOS 3.3 header carrying the
  origin and the length — the form the file takes on an Apple II disk.
- `--flat` writes a full 64 KB image with the bytes at their origin.

**`--dos-bin` is the one that closes a gap rather than adding a convenience.**
The header carries the ORIGIN, and the default output throws it away — so
wrapping the bytes by hand afterward means already knowing an address that
usually comes from an `ORG` buried in the source. The assembler knows it;
nothing downstream reliably does.


## Deliberate divergences and unverified corners

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
  — which textual substitution inside a macro body would splice — resolves here
  as one symbol instead. No vendor line does it, so the corpus cannot say which
  reading Merlin takes outside a macro.
- **The inclusive-or character is also the local-label scope joiner.** The
  expression tokenizer reads an identifier greedily, so `LABEL.OTHER` lexes as
  one symbol where Merlin would read an operation. Every use on the vendor disk
  follows a digit, where no identifier is being scanned, so the corpus cannot
  force the other reading; a source that needs it would.

## Case

**Directives, mnemonics and the alternate branch names are taken in any case.**
`LDA`, `lda` and `Lda` are one instruction, and they emit the same byte.

Real Merlin ran on hardware with no lower case, so no vendor source can settle
this and the corpus never will. Casso is deliberately wider here: a dialect that
accepts more than the original cannot reject a source the original would have
assembled, and Merlin source written in a Windows editor arrives lower-case.

**Symbols are a different question and stay case-sensitive.** A label written
`lda` is legal — period sources do it — and is accepted with a warning that it
resembles an instruction, rather than being refused.

## Strictness

There is no lenient superset. A source is read under the dialect its invocation
names and no other, so an `as65` construct in a Merlin file is rejected — and
the diagnostic says which dialect defines it, rather than reporting an unknown
instruction.

The commonest first mistake has its own message: a Merlin label must begin in
column 1, and one written indented is read as the opcode field with the
instruction beside it as its operand. Casso says so instead of reporting that
the label is not an instruction.
