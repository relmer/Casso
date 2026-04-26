# Implementation Guide: Spec 002 — Full AS65 Assembler Clone

**Purpose**: Continuation instructions for the speckit.implement agent. Read this file alongside the spec artifacts before resuming implementation.

## Current State

- **Branch**: `002-as65-assembler-compat`
- **Phase 1**: COMPLETE (committed cf1d70a) — skeleton files, project structure, types
- **Phase 2**: COMPLETE (committed 57abaa8) — full expression evaluator with 57 tests
- **Phase 3**: IN PROGRESS — integrating expression evaluator into assembler
- **Tests**: 290 passing (222 spec-001 + 57 expression evaluator + 11 placeholders)
- **Build**: All 4 configs (Debug/Release × x64/ARM64) clean, 0 warnings

## Critical Build/Commit Rules

Before EVERY commit:
1. Build Debug x64: `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts\Build.ps1 -Target Build -Configuration Debug -Platform x64`
2. Run tests: `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts\RunTests.ps1 -Configuration Debug -Platform x64`
3. Build Release x64: same with `-Configuration Release`
4. Build Debug ARM64: same with `-Platform ARM64`
5. Build Release ARM64: same with `-Configuration Release -Platform ARM64`
6. All builds must have 0 errors AND 0 warnings
7. All tests must pass at 100%
8. Commit at least once per phase

## Phase 3 Implementation Strategy (Next Up)

Phase 3 integrates the expression evaluator into the assembler (T011-T017). The key challenge is backward compatibility — all 222 existing spec-001 tests must continue to pass unchanged.

### Approach: Incremental Replacement

Do NOT rewrite the entire Assemble() method at once. Instead:

1. **Keep the existing `Parser::ClassifyOperand`** for addressing mode detection in Pass 1. It correctly identifies `#immediate`, `(indirect,x)`, `(indirect),y`, `zp,x`, `abs,y`, etc. The expression evaluator doesn't need to know about addressing modes.

2. **Replace value resolution in Pass 2** with `ExpressionEvaluator::Evaluate()`:
   - Where the old code does `symbols.find(labelName)` + `labelOffset` + `lowByteOp/highByteOp`, instead build a symbol table as `unordered_map<string, int32_t>` from the `symbols` map, set `ExprContext.currentPC`, and evaluate the operand expression.
   - The operand string (e.g., `label+3`, `<label`, `$FF&$0F`) is passed directly to the expression evaluator.
   - The expression evaluator already handles `<`/`>` as lo/hi byte, `+`/`-` arithmetic, and symbol lookup.

3. **Replace `.org`/`.byte`/`.word` value parsing** in both passes:
   - Use `ExpressionEvaluator::Evaluate()` instead of `Parser::ParseValue()` for `.org` addresses.
   - For `.byte`/`.word` value lists, split on commas (respecting parentheses) and evaluate each element.

4. **The `ClassifiedOperand` struct** needs a new field to store the raw operand expression string for Pass 2 evaluation. Currently it stores a pre-parsed `value` and `labelName` — the new approach stores the expression text and evaluates it in Pass 2 when all symbols are known.

5. **Run the existing tests after EVERY change** — if any of the 222 spec-001 tests break, fix immediately before proceeding.

### Files to Modify

- `Casso65Core/Assembler.cpp` — Pass 2 value resolution, directive handling
- `Casso65Core/Assembler.h` — may need `#include "ExpressionEvaluator.h"`
- `Casso65Core/Parser.h` — add `expression` field to `ClassifiedOperand` (or create a new operand struct)
- `Casso65Core/Parser.cpp` — store raw operand text for expression evaluation

### What NOT to Change

- Do NOT modify `Parser::ParseLine` — it correctly splits label/mnemonic/operand and handles directives
- Do NOT modify `Parser::SplitLines` — it works correctly
- Do NOT modify the test files in `UnitTest/AssemblerTests.cpp` or `UnitTest/ParserTests.cpp`
- Do NOT change the public API of `Assembler::Assemble()` or `AssemblyResult`

## Phases 4-8 (MVP Path) Quick Reference

After Phase 3, the remaining MVP phases are:

- **Phase 4** (T018-T024): Add `SymbolKind` tracking, implement `=`/`equ`/`set` constants, wire `*` as current-PC. Change default origin from `$8000` to `0`.
- **Phase 5** (T025-T031): Conditional assembly stack (`if`/`else`/`endif`). Stack of `ConditionalState` controls line processing.
- **Phase 6** (T032-T039): Macro definition collection and expansion. `\1`-`\9`, `\?`, 15-level nesting.
- **Phase 7** (T040-T048): Colon-less labels, `ds`/`align`/`end`/`ERROR`, AS65 directive synonyms, segments, escape sequences.
- **Phase 8** (T049-T051): Download Dormann source on demand, assemble, compare against bin_files/ reference. EXIT GATE.

## Formatting Rules (NON-NEGOTIABLE)

- **5 blank lines** between all top-level file constructs (includes→function, function→function, after last function)
- **3 blank lines** between variable declarations and first statement in a function
- **80-char `///...///` comment blocks** around every function/class
- `"Pch.h"` as first `#include` in every `.cpp` file
- **Never** use angle-bracket includes except in `Pch.h`
- **Never** break column alignment in declarations
- Functions under 50 lines

## Security Rules

- **NEVER** download or execute external binaries (as65.exe, etc.)
- Dormann source (GPL) downloaded on demand, compared, then deleted — never committed
- Conformance test inputs are our own work — committed
- Expected outputs are hand-computed — committed

## AS65 Reference

The AS65 manual (`as65.man`) was extracted from `as65_142.zip` in the Klaus2m5 GitHub repo. Key behavioral details:
- Default origin: 0 (not $8000)
- Case-sensitive by default; we keep case-insensitive (superset)
- `=`/`set` reassignable; `equ` immutable
- `*` and bare `$` = current-PC; `$` + hex digit = hex literal
- `%` + 0/1 = binary; `%` after operand = modulo
- Macro nesting limit: 15 levels
- Conditional nesting: at least 32 levels
- `lo`/`hi` keywords equivalent to `<`/`>`
- `fcb` = expressions only; `fcc` = strings only; `db` = both
