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

| Spelling | High bit | Inverse | Flashing | Terminator |
|---|---|---|---|---|
| `ASC` | from delimiter | no | no | none |
| `DCI` | from delimiter | no | no | last character inverted in high bit |
| `INV` | — | yes | no | none |
| `FLS` | — | no | yes | none |
| `STR` | from delimiter | no | no | leading length byte |

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
| `]name` | Variable symbol, reassignable |
| `LUP` / `--^` | Loop and its terminator |
| `DUM` / `DEND` | Dummy section — assigns addresses, emits no bytes |
| `NAME MAC` … `<<<` | Macro definition. **`<<<` is the terminator, not the invocation** — every macro in the disk's own library ends with it |
| `NAME arg;arg` | Macro invocation: bare name, arguments separated by `;` |
| `]1`, `]2`, `]3` | Positional parameters inside a macro body |
| `IF <char>=]n` | Merlin's first-character conditional, used throughout the vendor macro library to switch on how a parameter was written (`IF #=]1`, `IF (=]2`, `IF "=]1`) |
| `PUT` / `USE` | File inclusion, resolved relative to the including source |
| `XC` (first) | Enables the 65C02 instruction set for the rest of the assembly |
| `DSK` | Names the assembly's output — **the command line takes precedence** |

### Comments and line structure

`*` in column 1 introduces a whole-line comment. A comment otherwise occupies the
trailing comment field.

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

The fixed columns visible in Merlin listings are the **editor's** formatting, not
an assembler requirement. A parser demanding an opcode at a specific column would
be wrong.

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

- `PI.ADD.S` — `REL` plus `ENT` on four labels, **no `EXT`**. The export-only
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

## Unsettled, to be answered by capture

Recorded here so they are answered with evidence rather than reasoning. The
corpus is the arbiter; the manual is not.

- Is a semicolon required to start the comment field, or is everything after the
  operand field a comment regardless? Both fit the source observed so far — every
  trailing comment on the disk happens to begin with `;`. **Experiment**: assemble
  a line whose fourth field begins with an ordinary word and would be a syntax
  error if parsed as anything but a comment. Acceptance confirms
  comment-by-position; an error proves `;` is required. One line, one assembly,
  definitive — and the entry is worth keeping either way to pin the answer against
  regression.
- Does an unterminated `MAC` fall through into the next one? The vendor library
  has `ADDX MAC` / `TXA` immediately followed by `ADDA MAC` with no intervening
  `<<<`, sharing a single terminator — so either `ADDX` is a fall-through into
  `ADDA`'s body, or it is a one-instruction macro and the idiom means something
  else. This matters twice: for expansion, and because the unterminated-macro
  diagnostic must not fire on legitimate vendor source.
- How are labels inside a macro body scoped? The vendor library reuses the bare
  label `NC` in three separate macros, so expanding two of them into one assembly
  would collide unless Merlin scopes or renames them.
- Does Merlin accept a form of `XC` that resets the target to 6502? If so it is in
  scope and cheap; if not, nothing to do.
- Is Merlin's symbol matching case sensitive?
- What is Merlin's symbol length limit, and its legal label character set?
- Do Merlin's expression operators and precedence match the shared evaluator? If
  they diverge, the evaluator gains a dialect-scoped operator table rather than
  the profile forking it.
