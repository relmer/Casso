# Klaus Dormann Functional Test Suite — Assembler Gap Analysis

**Status**: Investigation report for issue [#26](https://github.com/relmer/Casso65/issues/26) (parent [#7](https://github.com/relmer/Casso65/issues/7))
**Date**: 2026-04-25
**Target source**: [`6502_functional_test.a65`](https://github.com/Klaus2m5/6502_65C02_functional_tests/blob/master/6502_functional_test.a65)
**Target assembler dialect**: AS65 by Frank A. Kingswood (the suite was authored against AS65)

## Goal

Determine whether Casso65's built-in two-pass assembler can assemble Klaus Dormann's
6502 functional test suite as a precondition for issue #7. If gaps exist, list each
missing assembler feature as a separate, file-ready GitHub issue with a clear scope,
evidence drawn from the suite source, and acceptance criteria.

## Method

1. Reviewed currently implemented features in `My6502Core/Assembler.cpp`,
   `My6502Core/Parser.cpp`, and the feature spec at `specs/001-assembler/spec.md`.
2. Read `6502_functional_test.a65` from the upstream repository and catalogued every
   distinct syntactic construct that Casso65 does not currently accept.
3. Cross-checked the "Out of Scope" section of the existing feature spec — items
   listed there (macros, conditional assembly, includes) are deliberately deferred
   and need to be re-scoped if we want to run Dormann's suite directly.

## Summary of current vs. required syntax

| Capability                              | Casso65 today                                          | Dormann's suite needs                                                  | Gap |
|-----------------------------------------|--------------------------------------------------------|------------------------------------------------------------------------|:---:|
| Mnemonics + addressing modes (NMOS)     | All 56 standard mnemonics, all modes                   | Same                                                                   |  —  |
| Number literals                         | `$hex`, `%bin`, decimal                                | Same                                                                   |  —  |
| Labels                                  | `name:` (colon required)                               | `name` at column 0, no colon (`range_fw`, `trap`, …)                   |  ✗  |
| Constant / symbol definitions           | None                                                   | `name = expr` and `name equ expr` (heavily used: `carry equ %0001`, `zero_page = $a`, `range_adr = *+1`) | ✗ |
| Current-PC operator (`*`)               | None                                                   | `*+1`, `beq *`, `jmp *`                                                |  ✗  |
| Expression operators                    | `<label`, `>label`, `label+positiveConst`              | `+ - * / & \| ^ ~ << >>`, parentheses, unary `-`, `~`, comparisons (`=`, `!=`, `<`, `>`) inside `if` | ✗ |
| Label arithmetic (subtraction, label−label) | Not supported                                      | `*+1`, `m8i = $ff & ~intdis`, `data_segment & $ff`                     |  ✗  |
| Conditional assembly                    | None                                                   | `if expr` / `else` / `endif`, nested; `if (data_segment & $ff) != 0`   |  ✗  |
| Macros                                  | None                                                   | `name macro` / `endm` with positional params (`\1`, `\2`) and unique-label suffix `\?` | ✗ |
| Storage / fill directives               | None                                                   | `dsb`/`ds` (reserve N bytes, optional fill) used to align data buffers |  ✗  |
| `.byte` synonyms                        | `.BYTE` only                                           | `db`/`byt` and `dw`/`word` are common AS65 spellings; suite uses `.byte`/`byte`/`dw` mix | ✗ |
| `org` without leading dot               | `.ORG` required                                        | `org $400` (no dot) is the AS65 form; suite uses `org`                 |  ✗  |
| `.include`                              | None                                                   | Suite is single-file but pulls in `report.i65` when `report=1`         | ✗* |
| Assembler assertions / `ERROR` directive| None                                                   | `ERROR ERROR ERROR low byte of data_segment MUST be $00 !!`            | ✗* |
| `noopt` and similar passthrough pragmas | None                                                   | `noopt` is harmless for a one-pass-style emitter but must not error    |  ✗  |
| Case sensitivity                        | Mnemonics insensitive, labels sensitive (FR-022/023)   | Same — OK                                                              |  —  |
| Output format                           | Flat binary, default fill `$FF`                        | Suite is built with `-h0` (intel hex) but a flat binary at `$0000..$FFFF` is acceptable for our purposes | — |

`✗*` = nice-to-have but the default Dormann config (`report = 0`) makes it skippable.

## Evidence excerpts

All quotes are verbatim from `6502_functional_test.a65`:

- Constant via `=`:
  `ROM_vectors = 1`, `zero_page = $a`, `data_segment = $200`, `code_segment = $400`,
  `disable_decimal = 0`.
- Constant via `equ`:
  `carry   equ %00000001`, `m8      equ $ff`, `m8i     equ $ff&~intdis`,
  `fnzc    equ minus+zero+carry`.
- Current-PC operator and label arithmetic:
  `range_adr   = *+1`, `jmp *`, `beq *`, `bne *`.
- Conditional assembly (nested):
  ```
  if (data_segment & $ff) != 0
      ERROR ERROR ERROR low byte of data_segment MUST be $00 !!
  endif
  …
  if disable_decimal < 2
      if I_flag = 0
          …
      endif
      …
  endif
  ```
- Macros with parameters and unique-label suffix:
  ```
  trap_eq macro
          beq *           ;failed equal (zero)
          endm

  set_a   macro       ;precharging accu & status
          load_flag \2
          pha
          lda #\1
          plp
          endm

  trap_eq macro
          bne skip\?
          trap
  skip\?
          endm
  ```
- Colon-less labels (column-0 identifier, instruction follows on next line):
  `range_fw` then `nop` on the next line; `range_op` then `range_adr = *+1`.
- Bitwise expressions in immediate operands:
  `lda #\1&m8i`, `cmp #(\1|fao)&m8i`, `eor #(\1&m8i|fao)`.

## Conclusion

**Casso65 cannot assemble Klaus Dormann's 6502 functional test suite as-is.** The
core mnemonic/addressing-mode coverage is sufficient, but the surrounding meta-syntax
(constant definitions, expressions, conditionals, and macros) is not. Closing #7 by
running the suite from source therefore requires either:

1. **Path A (preferred long-term)** — extend the assembler with the enhancements
   listed below so the suite builds with our toolchain, or
2. **Path B (interim)** — assemble the suite externally with AS65/CA65 once and
   commit the binary artifact for #7. This unblocks CPU validation but leaves the
   assembler unable to consume any non-trivial real-world source.

The proposed enhancement issues below describe the work required for Path A. They
are listed in the recommended implementation order — each is independently shippable
and unlocks at least one additional concrete construct used in the suite.

---

## Proposed enhancement issues

Each subsection below is written so it can be filed verbatim as a separate GitHub
issue. Suggested labels are listed at the top of each entry. All issues are children
of #7 (test suite execution) and unblock it.

### 1. `feat(assembler): support constant assignment via `=` and `equ``

**Labels**: `enhancement`, `assembler`
**Priority**: P1 — required for #7
**Blocks**: #7

**Summary.** Allow lines of the form `NAME = EXPR` and `NAME equ EXPR` to define a
named constant that participates in the symbol table for the rest of the program.
Symbols defined this way are referenced exactly like labels (`lda #zero_page`, but
also `sta zero_page,x`).

**Evidence (Dormann).**
```
ROM_vectors = 1
zero_page   = $a
carry       equ %00000001
m8          equ $ff
fnzc        equ minus+zero+carry
range_adr   = *+1
```

**Acceptance criteria.**
- Both `=` and `equ` (case-insensitive) are accepted.
- The right-hand side is a full expression (see issue 3) and may forward-reference
  other symbols/labels (resolved in pass 2 like normal label references).
- A constant defined via `=` or `equ` is queryable through `AssemblyResult::symbols`
  exactly like a label.
- Redefinition produces an error in the same style as duplicate labels.
- A constant whose name collides with a mnemonic or register is rejected (FR-024).
- `name = *+N` resolves to the current PC at the point of definition (depends on
  the `*` operator — see issue 2).

---

### 2. `feat(assembler): support `*` (current-PC) operator in expressions`

**Labels**: `enhancement`, `assembler`
**Priority**: P1 — required for #7
**Blocks**: #7

**Summary.** Recognize a bare `*` token in any expression context as "the address of
the first byte of the current line / current PC during emission." Common usages are
`jmp *` (infinite loop trap), `beq *+64` (skip forward), and `name = *+1`
(self-modifying code address).

**Evidence (Dormann).**
```
trap    macro
        jmp *
        endm
beq     *+64
range_adr = *+1
```

**Acceptance criteria.**
- `*` evaluates to the address that the current instruction/directive will be
  emitted at (i.e., the PC *before* this line's bytes are written), in both pass 1
  size estimation and pass 2 emission.
- `*` is valid anywhere a numeric expression is valid (operand, RHS of `=`/`equ`,
  inside `if`).
- Branch targets like `bne *` correctly compute a relative offset of `-2`.
- Multiplication in expressions (issue 3) must not be ambiguous with `*` — `*` is
  only the current-PC operator when it appears as a primary (start of an expression
  or after an operator/`(`); otherwise it is the multiplication operator.

---

### 3. `feat(assembler): full numeric expression evaluator`

**Labels**: `enhancement`, `assembler`
**Priority**: P1 — required for #7
**Blocks**: #7, issue 1, issue 4

**Summary.** Replace the current restricted operand classifier (which understands
only `<label`, `>label`, and `label+positiveConst`) with a real expression parser
that supports the operators routinely used by 6502 source.

**Required operators (precedence high→low, mirroring AS65/CA65).**
1. Primary: numeric literal, symbol, label, `*`, `(expr)`
2. Unary: `-`, `+`, `~` (bitwise NOT), `<` (low byte), `>` (high byte)
3. `*`, `/`, `%` (modulo)
4. `+`, `-`
5. `<<`, `>>`
6. `&`
7. `^`
8. `|`
9. Comparisons (only valid inside `if`): `=`, `==`, `!=`, `<`, `<=`, `>`, `>=`
   producing `0` (false) or `1` (true).

Internal evaluation is done in 32-bit signed integer; the final result is range-
checked and truncated to the width required by the consumer (1 byte for immediate
or zero-page, 2 bytes for absolute, 1 signed byte for branch offset).

**Evidence (Dormann).**
```
m8i      equ $ff & ~intdis
fnzc     equ minus + zero + carry
lda #\1 & m8i
cmp #(\1 | fao) & m8i
eor #(\1 & m8i | fao)
if (data_segment & $ff) != 0
```

**Acceptance criteria.**
- All operators above parse with correct associativity and precedence.
- Forward references to labels/constants are resolved in pass 2.
- `<expr` and `>expr` continue to work and are equivalent to `(expr) & $ff` and
  `((expr) >> 8) & $ff` respectively.
- `label - label` and `label + N - M` evaluate correctly.
- Out-of-range results for the consumer width produce the existing
  "value out of range" error (FR-015).
- The previous narrow forms (`<label`, `>label`, `label+offset`) continue to
  assemble byte-identical output (regression check against existing UnitTests).

---

### 4. `feat(assembler): conditional assembly (`if` / `else` / `endif`)`

**Labels**: `enhancement`, `assembler`
**Priority**: P1 — required for #7
**Blocks**: #7
**Depends on**: issues 1, 3

**Summary.** Add `if EXPR`, `else`, and `endif` directives (with and without leading
dot, case-insensitive). When `EXPR` evaluates to non-zero the body is assembled,
otherwise it is skipped — including label and constant definitions inside it.
Conditionals nest arbitrarily.

**Evidence (Dormann).**
```
if (data_segment & $ff) != 0
    ERROR ERROR ERROR low byte of data_segment MUST be $00 !!
endif

if disable_decimal < 2
    if I_flag = 0
        …
    endif
    if I_flag = 1
        …
    endif
endif

if report = 0
trap macro
    jmp *
    endm
…
endif
```

**Acceptance criteria.**
- `if` / `else` / `endif` are recognized in pass 1 *before* macro expansion of the
  enclosed lines, so skipped blocks do not contribute to label addresses.
- Skipped blocks consume zero bytes and define zero symbols.
- An unmatched `else` or `endif` produces a clear error.
- Both `if expr` and `.if expr` are accepted (Dormann uses `if`).
- The expression is evaluated using the issue-3 evaluator including comparison
  operators returning 0/1.
- Add `ifdef NAME` / `ifndef NAME` as a follow-up only if needed; Dormann's primary
  test does not require them.

---

### 5. `feat(assembler): macro definitions with positional parameters`

**Labels**: `enhancement`, `assembler`
**Priority**: P1 — required for #7
**Blocks**: #7
**Depends on**: issues 1, 3, 4

**Summary.** Add `NAME macro` / `endm` blocks. Calls of the form
`NAME arg1, arg2, …` expand inline at the call site, with `\1`, `\2`, … replaced
literally by the call arguments and `\?` replaced by a per-expansion unique suffix
(used to keep internal labels collision-free across expansions). Parameter-less
macros are valid (`trap macro` / `endm`).

**Evidence (Dormann).** Every `trap_*`, `set_*`, `tst_*`, `load_flag`, `cmp_flag`,
and `eor_flag` is a macro; macros are invoked hundreds of times throughout the
suite. Examples:
```
trap_eq macro
        bne skip\?
        trap
skip\?
        endm

set_a   macro
        load_flag \2
        pha
        lda #\1
        plp
        endm
```
Note that `trap_eq` itself contains a macro call (`trap`) — macro bodies can call
other macros (recursive expansion is bounded by the source not declaring infinite
recursion).

**Acceptance criteria.**
- `name macro` / `endm` registers a macro in pass 1; subsequent occurrences of
  `name` as the mnemonic position trigger expansion.
- `\1`…`\9` substitute the comma-separated arguments verbatim (textual
  substitution, like AS65/CA65). Whitespace around arguments is trimmed.
- `\?` substitutes a unique token (e.g., `__m17`) per expansion site, so that
  labels defined inside a macro do not collide across calls.
- Macro expansions show up in the listing output expanded on separate lines (this
  matches AS65's `-m` switch and the expectations of FR-030).
- A maximum expansion-depth cap (e.g., 32) prevents pathological recursion.
- Defining a macro whose name collides with a mnemonic, register, or another
  macro produces an error.
- An undefined macro invocation falls through to the normal "unrecognized
  mnemonic" error (FR-015).

---

### 6. `feat(assembler): accept colon-less labels`

**Labels**: `enhancement`, `assembler`
**Priority**: P2 — required for #7
**Blocks**: #7

**Summary.** Currently a label requires a trailing `:`. Dormann (and AS65/CA65 by
default) writes labels as bare identifiers in column 1 with the instruction either
on the same line or the next line.

**Evidence (Dormann).**
```
range_fw
        nop
        nop

range_op                ;test target with zero flag=0…
range_adr   = *+1
        beq *+64
```

**Acceptance criteria.**
- An identifier that appears at column 0 *and* is not followed by a recognized
  mnemonic, directive, or `=`/`equ` on the same line is treated as a label
  declaration that resolves to the current PC, exactly as `name:` does today.
- An identifier at column 0 followed by an instruction on the same line still
  works (existing behaviour preserved): e.g. `loop dex` is equivalent to
  `loop: dex`.
- The colon form continues to assemble identically.
- Label-vs-mnemonic disambiguation: identifiers that match a mnemonic/register
  are rejected as labels (FR-024 already covers this).

---

### 7. `feat(assembler): storage directives `dsb` / `ds` / `.res``

**Labels**: `enhancement`, `assembler`
**Priority**: P2 — required for #7
**Blocks**: #7

**Summary.** Add a directive to reserve N bytes, optionally pre-filled with a value.
Dormann uses these to lay out the data segment.

**Acceptance criteria.**
- `.dsb N [, fill]` and `.res N [, fill]` (and `ds` without dot, AS65-style) emit
  exactly N bytes; `fill` defaults to `$00`.
- A label preceding the directive resolves to the first byte reserved.
- N may be any expression (issue 3).

---

### 8. `feat(assembler): accept AS65 directive spellings (`org`, `db`, `dw`, `byt`, `noopt`)`

**Labels**: `enhancement`, `assembler`
**Priority**: P3 — required for #7
**Blocks**: #7

**Summary.** AS65 (the assembler Dormann targeted) writes directives without a
leading dot. To keep the suite source verbatim, accept `org`, `db`/`byt`/`byte`,
`dw`/`word`, `dsb`/`ds`, and treat `noopt`/`opt` as harmless no-ops.

**Acceptance criteria.**
- Both `.org`/`org`, `.byte`/`byte`/`byt`/`db`, `.word`/`word`/`dw` produce
  identical output to today's `.org`/`.byte`/`.word` paths.
- `noopt` (and `opt …`) parses and emits zero bytes with no error and at most an
  informational warning under `--warn`.

---

### 9. `feat(assembler): assertion directive (`ERROR`) and improved diagnostics`

**Labels**: `enhancement`, `assembler`
**Priority**: P3 — nice-to-have for #7
**Blocks**: —

**Summary.** Inside a (presumably failed) `if` block Dormann emits a hard error:

```
if (data_segment & $ff) != 0
    ERROR ERROR ERROR low byte of data_segment MUST be $00 !!
endif
```

Recognize an `ERROR` (case-insensitive) directive that, when assembled, produces
an `AssemblyError` with the rest of the line as the message.

**Acceptance criteria.**
- An `ERROR` line inside a *taken* `if` branch produces a fatal assembly error
  with the remainder of the source line as the message and the correct line
  number.
- An `ERROR` line inside a *skipped* `if` branch produces no diagnostic.

---

### 10. `feat(assembler): `.include` directive`

**Labels**: `enhancement`, `assembler`
**Priority**: P3 — only required if `report = 1` mode of the suite is used
**Blocks**: —

**Summary.** Allow the assembler to splice another source file in at the include
site. The assembler core currently takes a string; the CLI layer would need to
resolve the path relative to the including file and pass an expanded source. The
default Dormann config (`report = 0`) does not exercise this, so it is only needed
to assemble the optional `report.i65` companion.

**Acceptance criteria.**
- `.include "path"` (and AS65-style `include "path"`) splices the contents of the
  named file at that point.
- Errors in the included file report the included file's name and line number.
- Missing/unreadable include files produce a clear error.

---

## Cross-cutting work (not separate issues, called out for planning)

- **Spec update**: `specs/001-assembler/spec.md` currently lists macros, conditional
  assembly, and includes under "Out of Scope". Implementing issues 1–10 makes that
  scope statement obsolete and the spec should be amended in the PR that lands
  the first of those features (or in a small docs-only PR up front).
- **Test plan**: each enhancement issue should land with focused UnitTest cases
  (matching the existing `Assembler*Tests.cpp` style) plus, once issues 1–7 are
  in, a single integration test that assembles a representative excerpt of
  `6502_functional_test.a65` and compares against a reference binary.
- **Listing output (FR-030)**: macro expansion (issue 5) and conditional skipping
  (issue 4) interact with the listing. Listing format changes should be reviewed
  alongside those two issues, not as a separate item.

## Recommendation

File issues 1–8 immediately as children of #7. Issues 9 and 10 can wait until a
contributor wants the optional `report = 1` mode of the suite. Once issues 1–7
are merged, parent issue #7 can pick up the binary produced from
`6502_functional_test.a65` and exercise the CPU end-to-end.
