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
| `MAC` / `EOM`, `<<<`, `]1`… | Macro definition, invocation, positional parameters |
| `PUT` / `USE` | File inclusion, resolved relative to the including source |
| `XC` (first) | Enables the 65C02 instruction set for the rest of the assembly |
| `DSK` | Names the assembly's output — **the command line takes precedence** |

### Comments and line structure

`*` in column 1 introduces a whole-line comment. A comment otherwise occupies the
trailing comment field.

**A semicolon is not a comment introducer "anywhere."** Inside the operand field
it is data — Merlin uses it to separate macro arguments. Observed on the Merlin 8
v2.47 disk, in `PI.ADD.S`:

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
| `XC` (second occurrence) | Selects the 65802/65816, which Casso does not emulate | A 65816 core |
| `TYP` | Sets a filesystem file type, meaningless without a filesystem that has types | `020-disk-file-access` |
| `SAV` | Saves the object so far and continues — multi-output segmentation | Its own decision. **Not** a 020 dependency; 020 landing will not make the right behavior obvious. |

## Unsettled, to be answered by capture

Recorded here so they are answered with evidence rather than reasoning. The
corpus is the arbiter; the manual is not.

- Is a semicolon required to start the comment field, or is everything after the
  operand field a comment regardless? Both fit the source observed so far.
- Does Merlin accept a form of `XC` that resets the target to 6502? If so it is in
  scope and cheap; if not, nothing to do.
- Is Merlin's symbol matching case sensitive?
- What is Merlin's symbol length limit, and its legal label character set?
- Do Merlin's expression operators and precedence match the shared evaluator? If
  they diverge, the evaluator gains a dialect-scoped operator table rather than
  the profile forking it.
