# Contract: Merlin Directive Vocabulary and Subset Boundary

**Feature**: `019-assembler-dialects`

What Merlin source may contain and expect to work, and what is refused by name.
The refusal list is the human-readable face of the single table FR-019 requires;
the table in code is the source of truth, and the tool's help text is generated
from it.

## Supported

### Data

| Spelling | Operation | Notes |
|---|---|---|
| `DFB` / `DB` | Byte data | Maps to the existing byte token |
| `DA` / `DW` | Word data, low byte first | Maps to the existing word token |
| `DDB` | Word data, **high byte first** | New token — the assembler has no such operation today |
| `HEX` | Raw hexadecimal bytes | New token |
| `DS` | Storage reservation | Existing token |

### Strings

One token carrying an encoding mode, not five tokens. The five spellings differ
only in high-bit handling, inversion, and terminator convention — parameters, not
operations.

| Spelling | High bit | Inverse | Flashing | Terminator | Uses on disk |
|---|---|---|---|---|---|
| `ASC` | from delimiter | no | no | none | 35 |
| `DCI` | from delimiter | no | no | last character inverted in high bit | **130** |
| `INV` | — | yes | no | none | 1 |
| `FLS` | — | no | yes | none | 0 |
| `STR` | from delimiter | no | no | leading length byte | 0 |
| `REV` | from delimiter | no | no | text emitted **reversed** | 3 |

**`REV` was missing from this list** and was found in the vendor sources, which is
why the family is six spellings and not five. It is the argument for deriving the
table from the corpus rather than transcribing it: a spelling nobody wrote down
still has to assemble. Its behavior is verified — `REV "CALL-151"` in
`MAKE DUMP.S` emits `151-LLAC` in high ASCII, which is why searching that object
for the forward spelling finds nothing.

**Half of this table has no oracle**, counting only the five sources that ship an
object to compare against:

| Mode | Lines with an object | Status |
|---|---|---|
| `ASC` | 24 | **Verified** |
| `DCI` | 130 | **Verified** — and all 984 of `LABELS`'s bytes, whole-file through the assembler |
| `REV` | 1 | **Verified** |
| `INV` | 0 | **Unverified** — its one use is in `PI.START.S`, a linker demo shipping no object |
| `FLS` | 0 | **Unverified** — appears nowhere in the corpus |
| `STR` | 0 | **Unverified** — appears nowhere in the corpus |

This is why the corpus floor demands one captured entry **per spelling** rather
than one for the family. For `INV`, `FLS` and `STR` the implementation follows
documentation, not bytes, and only capture can settle them. Each is marked
`UNVERIFIED` at its own line in `StringEncoding.cpp`.

**The delimiter rule is not an ordering.** `"` (`$22`) and `!` (`$21`) both
produce high ASCII in the vendor objects, which rules out any rule comparing the
delimiter's ASCII value against `'` (`$27`) — `!` sorts below it and would have to
give low ASCII. The rule implemented is that every delimiter sets the high bit
except the apostrophe. **The apostrophe half is unverified**: no `'`-delimited
string appears anywhere in the corpus. It is the conservative choice, because it
changes nothing about any string the corpus does contain.

The counts are from the nine committed sources. `DCI` at 130 uses is the
workhorse and the corpus's sharpest probe, which is consistent with its being the
encoding whose terminator convention is easiest to get subtly wrong.

Merlin infers high-bit or low-bit from *which delimiter quotes the string*, so
delimiter handling is part of resolving the mode rather than a separate concern.

This is the highest-risk area in the dialect: a high-bit or terminator error
produces output that still looks plausible on inspection. The corpus floor
therefore requires **one entry per spelling**, not one for the family.

### Symbols, structure, and flow

| Spelling | Operation |
|---|---|
| bare word in column 1 | Label — no terminator character |
| `:name` | Local label, scoped to the enclosing global |
| `]name` | Variable symbol, reassignable. **No oracle** — not one appears in the nine committed sources, where the sigil occurs only as `]1`..`]3`. The form standing as a program-counter label is refused rather than accepted; see tasks.md T031 |
| `LUP` / `--^` | Loop and its terminator |
| `DUM` / `DEND` | Dummy section — assigns addresses, emits no bytes |
| `NAME MAC` … `<<<` | Macro definition. **`<<<` is the terminator, not the invocation** — every macro in the disk's own library ends with it |
| `NAME arg;arg` | Macro invocation: bare name, arguments separated by `;` |
| `>>> NAME;arg` | Explicit macro invocation, name first in the operand. **UNVERIFIED** — every macro on the disk is invoked by bare name, so the corpus cannot report whether the prefix is accepted. Implemented anyway; `PMC`, its documented word synonym, is not |
| `]1`, `]2`, `]3` | Positional parameters inside a macro body. Substituted **textually, ignoring identifier boundaries** — `LDX #A]1-ADRTBL` and `LDX #]1END-]1-1` both splice a parameter into the middle of a symbol |
| `IF <char>=]n` | Merlin's first-character conditional, used throughout the vendor macro library to switch on how a parameter was written (`IF #=]1`, `IF (=]2`, `IF "=]1`) |
| `PUT` / `USE` | File inclusion, resolved relative to the including source |
| `XC` (first) | Enables the 65C02 instruction set for the rest of the assembly |
| `DSK` | Names the assembly's output — **the command line takes precedence** |
| `NAME KBD` / `NAME KBD "prompt"` | Binds `NAME` to an answer supplied to the assembly. Merlin prompts the operator; Casso takes the answer from `AssemblerOptions::predefinedSymbols` and **refuses, naming the symbol and the prompt, when none was supplied**. Both operand forms parse; the quoted text is a prompt, not data |

### Comments and line structure

`*` in column 1 introduces a whole-line comment. So does `;` in column 1 — 8 such
lines appear across `CLOCK.S`, `KEYMAC.S`, and `MAKE DUMP.S`, all of them
continuations of a preceding comment. That is **not a second rule**: with no label
present, column 1 is the first field boundary, so a semicolon there is simply a
semicolon beginning a field, and the general rule below already covers it.

A comment otherwise occupies the trailing comment field.

**A semicolon is not a comment introducer "anywhere."** Inside the operand field
it is data — Merlin uses it to separate macro arguments. Verified on the
**Merlin Pro 2.23** disk, in `PI.ADD.S` and `PI.START.S`:

```
 ADD SUMSTR;DEFLEN;PL      macro call, three arguments
 VAR MSGPNT;OUTPUT         macro call, two arguments
 BCS SKIP ;Do another page if nec        instruction, then a comment
```

The difference is the field boundary, not the character: a semicolon within the
whitespace-delimited operand token belongs to the operand; one beginning the field
after it starts a comment. A parser that stripped from the first `;` would silently
truncate every macro call on the disk to its first argument — which is the class of
bug that produces plausible-looking wrong bytes.

The line model is **field-based, not literal columns**: runs of whitespace
separate the label, opcode, operand, and comment fields, and the only significant
column is the first — a line beginning with whitespace has no label. Tabs are
whitespace and separate fields exactly as spaces do; no tab-stop expansion is
performed, because tab stops affect only display.

**The field scanner MUST respect quoting.** Whitespace ends the operand field only
outside a quoted string, because Merlin source is full of strings containing
spaces. From `PI.START.S` on the distribution disk:

```
 INV "APPLE PI"
 ASC "       This program computes pi to many "
 ASC "Number of 11-char. columns for printout "
 ASC "(1-10) "
```

Note the leading and trailing spaces *inside* the quotes — they are payload
bytes, so a naive whitespace split would both truncate the operand and silently
change the emitted data. The string family is already the highest-risk area for
its encodings; quoting makes it the highest-risk area for field scanning too.

**And the delimiter is ANY character, not a quote.** Merlin takes the first
character after the directive as the delimiter and runs to its next occurrence.
`KEYMAC.S` depends on this, in the one place where a fixed quote set is
guaranteed to fail:

```
 ASC !7" "&$9F!
 ASC !" ASC ""!
```

Both choose `!` *because* their text contains `"`. Of the 166 string-directive
lines on the disk, 164 use `"` and these 2 do not — which is exactly the
distribution that lets a `"`-only scanner look correct on almost every line while
shredding these two. The second one is the sharper case: a `"` scanner ends the
operand after two characters.

This has a structural consequence. **The operand scanner must know the
mnemonic**, because which rule applies depends on the directive. That is not a
layering violation — the mnemonic field is read before the operand field, so it
is in hand by the time the operand is scanned.

The fixed columns visible in Merlin listings are the **editor's** formatting, not
an assembler requirement. A parser demanding an opcode at a specific column would
be wrong.

### The stored encoding marks field structure. Do not use it.

On disk, Merlin distinguishes the two kinds of space: a space **separating
fields** carries the high bit (`$A0`), a space **inside comment text** does not
(`$20`). `LABELS.S` holds 214 of the first and 81 of the second, and across all
nine committed sources spaces are the only bytes below `$80` — not one non-space
low byte in any of them.

This looks like a free lexer and it is a trap. **The parser must not depend on
it.** Source reaching Casso by any other route carries no such distinction: a
host editor, a read off a disk image, a file Casso itself writes. A parser
leaning on the encoding would work only on files authored on a Merlin disk and
fail on everything else — including files it had just produced.

`T.SENDMSG` settles it independently: all 26 of its spaces are `$A0` and none are
`$20`, so the distinction is not even reliably present in vendor source. It is an
observation about these bytes, never grammar. The field model above is the
grammar, and it is defined on ordinary spaces.

The fixture decoder collapses both forms to one space before any parser sees
them, so this is enforced by construction rather than by discipline.

## Refused, by name

Each refusal names the construct and gives the reason. None surfaces as an
unknown-directive error, which would read as "Merlin support is broken" rather
than "this construct is outside the subset." All offenders in a source file are
reported in one pass.

| Construct | Reason | Widens with |
|---|---|---|
| Relocatable-mode assembly | Needs a linker | GitHub issue #112 |
| Entry symbol declaration | Needs a linker | GitHub issue #112 |
| External symbol declaration | Needs a linker | GitHub issue #112 |

Two of those refusals are not equally final, and the message must reflect it:

| Source shape | What the refusal says |
|---|---|
| Relocatable mode and entry symbols, **no** external symbols | The module exports without importing, so it assembles on its own once relocatable mode is removed and an origin supplied. Say that. |
| Any external symbol declared | No workaround — it references symbols defined in other modules, and resolving those needs a linker Casso does not have. Say that instead; offering the fix above would send the developer down a path that cannot work. |

The distinction is not academic, and the vendor disk supplies **both** cases:

- `PI.ADD.S` — `REL` plus `ENT` on **six** labels, **no `EXT`**. The export-only
  case, which assembles once relocatable mode is removed and an origin supplied.
- `PI.START.S` — `REL` plus `EXT` three times and one `ENT`. The case with **no**
  workaround.

So a developer meeting this boundary will hit both messages from the same
project, which is precisely why offering the export-only fix indiscriminately
would send them down a path that cannot work for half the files.

Note also that the whole APPLE PI group is the **linker demo** — its own source
header says "This is just a test source for the linker." All five `PI.*.S` files
belong to the relocatable mode this spec puts out of scope, so they are
**negative** corpus material only. Feeding them in as positive byte-comparison
cases would generate confusing failures against a boundary the spec has already
decided.
| `XC` (second occurrence) | Selects the 65802/65816, which Casso does not emulate | A 65816 core |
| `TYP` | Sets a filesystem file type, meaningless without a filesystem that has types | `020-disk-file-access` |
| `SAV` | Saves the object so far and continues — multi-output segmentation | Its own decision. **Not** a 020 dependency; 020 landing will not make the right behavior obvious. |

## Found in the sources, missing from this contract

Extracting every opcode-field word from the nine committed sources and removing
the 6502/65C02 mnemonics left 43 distinct words. Most are macro invocations from
the vendor library (`STADR` 21, `TR` 5, `MOV` 5 …). The rest exposed
gaps in this document, all now in the table:

| Word | Uses | What it is | Status |
|---|---|---|---|
| `REV` | 3 | Sixth string spelling | **Added** to the string family above |
| `ERR` | 17 | Assembly-time assertion — fails when its expression is non-zero | **Added and implemented**; evaluated in pass 2, so its expression may name a forward label |
| `KBD` | 7 | Binds a symbol to an answer supplied to the assembly | **Added and implemented.** This row was originally read as a macro invocation from the vendor library and the count filed under that heading; it is a directive, and it gated three of the five oracle sources |
| `PAG` | 1 | Page eject in the listing | **Added**, reusing the existing `Page` token |
| `AST` | 4 | Asterisk line in the listing | **Open** — listing cosmetics, no byte effect |
| `EXP` | 4 | Macro-expansion listing control | **Open** — listing cosmetics, no byte effect |
| `VAR` | 1 | Macro variable substitution | **Open** — semantics unconfirmed, do not guess |

**`BLT` (9) and `BGE` (7) are not directives.** They are Merlin's aliases for
`BCC` and `BCS`, so they belong to the instruction layer rather than to this
table. Recorded here because this is where they were found, not because this is
where they are fixed.

The use count understates the impact, because what matters is which *oracles*
they gate:

| Source | `BLT`/`BGE` | `KBD` | Macros | Status |
|---|---|---|---|---|
| `LABELS.S` | — | — | — | **byte-identical**, 984 @ `$8000` |
| `MAKE DUMP.S` | 2 | — | 9 | **byte-identical**, 589 @ `$9000` |
| `KEYMAC.S` | — | 1 | yes | **byte-identical**, 674 @ `$9000` |
| `PRINTFILER.S` | 1 | 2 | yes | **byte-identical**, 286 @ `$02A0` |
| `CLOCK.S` | 6 | 2 | yes | **byte-identical twice**, 365 @ `$0240` each |

Three of five sources — and **four of the six objects**, since `CLOCK.S` yields
two — could not assemble until the mnemonic layer knew the aliases.

**`KBD` was never interactive input in the sense that sentence implied.** It is
a directive that binds a symbol from an answer given to the assembly, and the
answer path — `AssemblerOptions::predefinedSymbols` — already existed. The
sequencing note that said only two oracles were reachable "without ever solving
interactive input" was reading a prompt as a barrier. All six objects now
reproduce byte for byte, from all five sources.

What the three `KBD` sources cost beyond the directive itself was four other
things, every one of them settled from the shipped bytes: exclusive-or and
inclusive-or spelled `!` and `.`, unsigned 16-bit arithmetic, a variable symbol
standing as a program-counter label, and `?` inside a symbol.

`AST`, `EXP` and `VAR` are left open deliberately. The first two affect only the
listing and cannot change a byte; `VAR`'s semantics are not confirmed by anything
observed, and guessing at a macro-substitution rule is exactly the way to produce
plausible wrong bytes.

## Unsettled, to be answered by capture

Recorded here so they are answered with evidence rather than reasoning. The
corpus is the arbiter; the manual is not.

- Is a semicolon required to start the comment field, or is everything after the
  operand field a comment regardless? **Still open — the corpus cannot settle
  it.** Measured directly: of the 26 lines where an implied-mode opcode (`RTS`,
  `SEI`, `PHA`…) takes no operand and so is followed by the comment field, **all
  26** begin that field with `;`. Zero counterexamples. But that is absence of a
  counterexample in source a human wrote by convention, not evidence about what
  the assembler *accepts*, and the two differ precisely here. The parser
  implements the conservative reading (`;` introduces the comment), which is safe
  under either answer, since it accepts everything the disk contains.
  **Experiment**: assemble a line whose fourth field begins with an ordinary word
  and would be a syntax error if parsed as anything but a comment. Acceptance
  confirms comment-by-position; an error proves `;` is required. One line, one
  assembly, definitive — and the entry is worth keeping either way to pin the
  answer against regression.
- Does an unterminated `MAC` fall through into the next one? The vendor library
  has `ADDX MAC` / `TXA` immediately followed by `ADDA MAC` with no intervening
  `<<<`, sharing a single terminator — so either `ADDX` is a fall-through into
  `ADDA`'s body, or it is a one-instruction macro and the idiom means something
  else. This matters twice: for expansion, and because the unterminated-macro
  diagnostic must not fire on legitimate vendor source.
- ~~How are labels inside a macro body scoped?~~ **ANSWERED, from bytes.** They
  are made **unique per expansion**, with no declaration and nothing in the
  source to ask for it. `MAKE DUMP.S` expands `INCD` twice and `STORE` three
  times; each expansion redefines `NI` or `LP`, and `DECD` redefines `ND`. The
  vendor shipped a working 589-byte object, so the labels cannot have collided —
  and an assembler that merely permitted the redefinition would have pointed both
  branches at the first copy, which is a broken program rather than a shipped one.

  Two consequences the question did not anticipate, both from the same sources.
  A label an expansion produces must **not** open a local-label scope: `MAKE
  DUMP.S` calls those macros in the middle of routines whose locals belong to a
  global label further up, so a macro label becoming the enclosing global strands
  every local after the call site. And the label may sit on the **terminator
  line** — `KEYMAC.S` writes `NI <<<`, putting the body's branch target on the
  one line that closing a body discards.
- Does Merlin accept a form of `XC` that resets the target to 6502? If so it is in
  scope and cheap; if not, nothing to do.
- Is Merlin's symbol matching case sensitive?
- What is Merlin's symbol **length limit**? Still open. Its legal character set
  is not: `?` is legal and is now **implemented**. `CMD?`, `CORR?`, `ISY?` and
  `RNGOK?` are labels in the vendor sources, and `KEYMAC.S`'s `CMD?` is the one
  of the four sitting in a file that ships an object — so it fails a byte
  comparison rather than a matter of taste. Both halves were needed: the label
  rule and the expression tokenizer, or the name binds and then resolves
  nowhere. Supplied as profile DATA (`GetExtraSymbolCharacters`), so as65 goes
  on rejecting it.
- ~~Does Merlin's arithmetic match the evaluator's?~~ **ANSWERED, from bytes,
  and nothing in this document had thought to ask.** Merlin computes in
  **unsigned 16-bit quantities**, operands and intermediates alike. `CLOCK.S`
  turns that into a build switch: `HOURS = VERSION-25/-1*12+12` followed by
  `ERR HOURS-VERSION` is an equality test written as arithmetic, and it works
  only because `$FFF3` divided by `$FFFF` is 0 while `$FFFF` divided by `$FFFF`
  is 1. Read as signed 32-bit the same line is -13 / -1 = 13 and the assertion
  fires on a file the vendor shipped an object for.
- ~~May a variable symbol stand as a program-counter label?~~ **ANSWERED, from
  bytes.** Yes, and repeatedly: `CLOCK.S` names eight separate loop targets
  `]LOOP`, each branch meaning the definition immediately above it. It opens no
  local-label scope. The earlier refusal (tasks.md T031) is lifted; what made it
  safe is that pass 2 rebinds the symbol as it walks, which the reassignable
  constant already needed for the same reason.
- ~~Do Merlin's expression operators and precedence match the shared
  evaluator?~~ **ANSWERED, from bytes — in two instalments, and the first was
  half wrong.**

  **The binding does not match.** Merlin folds an expression strictly left to
  right and parentheses are the only grouping. `LABELS.S` proves it: it ends with

  ```
   ERR END-LABTBL-1/$700
  ```

  bounding its own table at seven pages, and its header comment says so in
  words. Under ordinary precedence the division binds first, the expression
  collapses to `END-LABTBL` — the table's own length, 983 — and the assertion
  fires on a file the vendor shipped a working object for. Folded left to right
  it is `(END-LABTBL-1)/$700`, which is 0 for any table inside seven pages.

  Implemented as the contract predicted — **in the evaluator, not by the profile
  forking it**. `ExprContext` carries an `OperatorBinding`, the profile answers
  `GetOperatorBinding`, and `TryParseBinary` flattens every operator to the
  loosest level when the answer is left-to-right. One branch; the operator set,
  the folds and the diagnostics are shared.

  Note the shape of this find. It was invisible to every string-level test and
  to the 983-byte encoder comparison, and surfaced only the first time a whole
  file went through the assembler — because the only line that depends on it is
  the last one, and it is an assertion rather than data.

  **The operator SET does not match either**, which the sentence "the operators
  do" above got wrong when it was written on `LABELS.S` alone. `!` is
  exclusive-or and `.` is inclusive-or, where the shared punctuation reads the
  first as logical-not and does not treat the second as punctuation at all.
  `CLOCK.S` settles both against its two shipped objects: `LDX #HOURS/24!1`
  places the time editor's cursor and must be exclusive-or, while
  `CMP #HOURS/24+3."0"` must be inclusive-or for the rollover comparison to read
  "24" and "13". Carried into the shared tokenizer as a table on `ExprContext`,
  the same way the binding is — one lookup on the way in, no second set of folds.

  One consequence to know before writing Merlin source for Casso: the
  inclusive-or character is also what joins a local label to its scope, and the
  tokenizer reads an identifier greedily, so `LABEL.OTHER` lexes as one symbol
  where Merlin would read an operation. Every use on the disk follows a digit,
  where no identifier is being scanned, so the corpus cannot force the other
  reading; a source that needs it would.
