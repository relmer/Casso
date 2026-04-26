# Research: Full AS65 Assembler Clone

**Feature**: 002-as65-assembler-compat  
**Date**: 2026-04-25

## 1. Expression Evaluator Architecture

**Decision**: Recursive descent parser with operator-precedence levels.

**Rationale**: The AS65 expression grammar has 11 precedence levels (from unary to logical OR). A recursive descent parser maps each level to a function, is easy to understand, requires no external tools, and handles the `*` disambiguation (current-PC vs multiplication) naturally via context. Alternatives considered: Pratt parser (slightly less readable, similar performance), shunting-yard (harder to handle unary operators and `*` disambiguation).

**AS65 Precedence (high to low)**:
1. `()` — parenthetical grouping
2. `* $` — current location counter (primary position only)
3. Unary `+ - ! ~` — unary plus, negation, logical NOT, binary NOT
4. `* / %` — multiplication, division, modulo
5. `+ -` — addition, subtraction
6. `<< >>` — shift left, shift right
7. `< > <= >=` — comparison for greater/less than
8. `= !=` — equality comparison (`==` is synonym for `=`)
9. `&` — binary AND
10. `^` — binary XOR
11. `|` — binary OR
12. `&&` — logical AND
13. `||` — logical OR
14. `hi lo` — high byte, low byte (keyword unary operators, same level as `< >` unary)

**Number formats**: `$hex`, `%binary`, `@octal`, `0xhex`, `0bbin`, `<base>#<value>`, decimal, `'char'`.

**`*` disambiguation**: `*` is current-PC when it appears as a primary (start of expression, after an operator, after `(`). It is multiplication when it appears after a number, identifier, or `)`. Same rule applies to `%` (binary prefix vs modulo) — `%` is binary only when followed by `0`/`1` and preceded by an operator or start-of-expression.

## 2. Macro Expansion Strategy

**Decision**: Text-based expansion during source line processing, before parsing.

**Rationale**: AS65 macros are textual substitution — `\1` through `\9` are replaced literally with argument text, `\?` with a unique suffix, `\0` with argument count. Named parameters are mapped to the same positional slots. This must happen before the line is parsed as an instruction, because macro arguments can contain partial expressions (e.g., `\1&m8i`).

**Implementation**: When a macro invocation is detected (name matches a defined macro), expand the macro body by:
1. Split invocation arguments on commas (respecting parentheses depth)
2. Replace `\1`–`\9` with argument text, `\0` with arg count string
3. Replace `\?` with a 4-digit unique counter (matching AS65 format)
4. Replace named parameter references with corresponding argument text
5. Process `local` labels by generating unique names
6. Feed expanded lines back into the line processing loop (allowing nested expansion)
7. Check `exitm` to terminate expansion early

**Nesting limit**: 15 levels (matching AS65).

## 3. Conditional Assembly Architecture

**Decision**: Stack-based state machine during line processing.

**Rationale**: `if`/`else`/`endif` blocks nest and control whether lines are processed or skipped. A stack of boolean states (assembling vs skipping) handles nesting naturally. When a parent block is skipped, all children are also skipped regardless of their own condition.

**Implementation**: Maintain a stack of `{assembling, seenElse}` states. On `if`: evaluate expression, push state. On `else`: flip the top state (only if parent is assembling). On `endif`: pop. Skip all line processing (labels, instructions, constants, macros) when the top state is "skipping". Exception: `if`/`else`/`endif` directives are always processed to maintain correct stack depth.

## 4. `=` vs `equ` Evaluation Timing

**Decision**: `=` evaluated eagerly (immediately when encountered); `equ` deferred to pass 2 like labels.

**Rationale**: AS65 convention. The Dormann suite uses `test_num = test_num + 1` inside macros, which requires immediate evaluation. `equ` can forward-reference labels because it's resolved in pass 2.

**Implementation**: `=`/`set` assignments evaluated immediately using the expression evaluator with current symbol table. If the expression contains an undefined symbol, emit an error. `equ` records the raw expression text in pass 1 (with the current PC for `*`), resolves in pass 2.

## 5. Segment Model

**Decision**: Three logical segments with independent PCs, flattened to a single binary output.

**Rationale**: AS65 has `code`, `data`, and `bss` segments with independent location counters. For flat binary output (the primary mode), `code` and `data` are interleaved in output order, and `bss` reserves zero-filled space. The Dormann suite uses `data`/`bss`/`code` directives.

**Implementation**: Maintain three PCs (`codePC`, `dataPC`, `bssPC`) and a current-segment selector. `org` sets the PC of the current segment. When switching segments, save/restore the appropriate PC. For binary output, flatten by writing each segment's output at its address range.

## 6. Include File Resolution

**Decision**: Injectable file reader interface for testability.

**Rationale**: Constitution principle II requires test isolation — no real filesystem access in unit tests. The include mechanism needs a seam where tests can provide synthetic file contents.

**Implementation**: `Assembler` accepts an optional `FileReader` interface (with a default implementation that reads from disk). Tests inject a mock that returns in-memory strings. Include nesting limit: 16 levels. Path resolution: relative to the including file's directory.

## 7. Output Format Architecture

**Decision**: Separate writer classes for binary, S-record, and Intel HEX.

**Rationale**: The three formats share input (assembled byte ranges) but differ in output encoding. Factoring into writer classes keeps the assembler core clean and each format testable independently.

**Implementation**: `OutputFormats.h/.cpp` with `WriteBinary()`, `WriteSRecord()`, `WriteIntelHex()` functions. Each takes the assembled output (byte vector, start address, end address, fill byte) and writes to a stream. S-record includes S0 (header), S1 (data), S9 (start address). Intel HEX includes data records and an EOF record.

## 8. Debug Information File Format

**Decision**: Match AS65's format using the `READDBG` sample as reference.

**Rationale**: The `-g` flag produces a debug file. The `as65_142.zip` includes `READDBG` sample code showing the format. This is a low-priority P3 feature but must match for full parity.

**Alternatives considered**: Inventing our own format (rejected — breaks AS65 compatibility goal).

## 9. Listing Format

**Decision**: Extend existing `FormatListingLine` with AS65-compatible features.

**Rationale**: The listing format from spec 001 is close to AS65's. Extensions needed: macro expansion lines prefixed with `>`, cycle counts in `[]`, page headers with title, `list`/`nolist` suppression, configurable width.

## 10. Backward Compatibility with Spec 001

**Decision**: All spec 001 behavior preserved; new features are additive only.

**Rationale**: SC-004 requires all existing tests to pass unchanged. The expression evaluator subsumes the old `ParseValue`/`ClassifyOperand` behavior. The old `<label`/`>label`/`label+offset` forms are a subset of the new expression syntax.

**Risk**: The parser rewrite (to support colon-less labels and AS65 directives) must be carefully tested against all existing `ParserTests.cpp` and `AssemblerTests.cpp` test cases.
