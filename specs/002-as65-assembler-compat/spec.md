# Feature Specification: Full AS65 Assembler Clone

**Feature Branch**: `002-as65-assembler-compat`  
**Created**: 2026-04-25  
**Status**: Draft  
**Input**: User description: "Full, exact clone of AS65 assembler by Frank A. Kingswood (6502 mode). All syntax, macro behaviors, directives, expression operators, output formats, and command-line options."

## User Scenarios & Testing

### User Story 1 - Full Expression Evaluator (Priority: P1)

A developer writes assembly source that uses arithmetic, bitwise, and logical operators in operands and constant definitions. The assembler evaluates these expressions correctly, respecting operator precedence and parenthetical grouping, and emits the right bytes.

**Why this priority**: Nearly every construct in the Dormann suite — constant definitions, conditional guards, macro arguments, and inline operands — depends on a full expression evaluator. Without it, none of the other features can consume real-world operands like `$ff & ~intdis` or `minus + zero + carry`.

**Independent Test**: Can be fully tested by assembling instructions with increasingly complex operand expressions and comparing emitted bytes against hand-computed values.

**Acceptance Scenarios**:

1. **Given** `LDA #$FF & $0F`, **When** assembled, **Then** the operand byte is `$0F`
2. **Given** `LDA #~$0F & $FF`, **When** assembled, **Then** the operand byte is `$F0` (bitwise NOT of `$0F` masked to a byte)
3. **Given** `LDA #(3 + 5) * 2`, **When** assembled, **Then** the operand byte is `$10` (16 decimal)
4. **Given** `LDA #$80 | $01`, **When** assembled, **Then** the operand byte is `$81`
5. **Given** `LDA #$FF ^ $0F`, **When** assembled, **Then** the operand byte is `$F0`
6. **Given** `LDA #1 << 4`, **When** assembled, **Then** the operand byte is `$10`
7. **Given** `LDA #$80 >> 4`, **When** assembled, **Then** the operand byte is `$08`
8. **Given** `LDA #-1 & $FF`, **When** assembled, **Then** the operand byte is `$FF`
9. **Given** existing programs using `<label`, `>label`, and `label+offset`, **When** assembled, **Then** they produce byte-identical output to spec 001 (regression check)

---

### User Story 2 - Constant Definitions (Priority: P1)

A developer defines named constants via `=` and `equ` and references them in operands exactly like labels. This allows readable programs that name magic numbers and compose constants from other constants.

**Why this priority**: The Dormann suite defines dozens of constants (`carry equ %00000001`, `zero_page = $a`, etc.) that are referenced throughout. Without constant support, no line that uses a named value can assemble.

**Independent Test**: Can be fully tested by defining constants and referencing them in instructions, then verifying the emitted operand bytes match the constant values.

**Acceptance Scenarios**:

1. **Given** `value = $42` then `LDA #value`, **When** assembled, **Then** the operand byte is `$42`
2. **Given** `carry equ %00000001` then `LDA #carry`, **When** assembled, **Then** the operand byte is `$01`
3. **Given** `a = 1` then `b = 2` then `c equ a + b` then `LDA #c`, **When** assembled, **Then** the operand byte is `$03`
4. **Given** `value = $42` then another `value = $10`, **When** assembled, **Then** the value is updated to `$10` (reassignable via `=`)
4a. **Given** `value equ $42` then another `value equ $10`, **When** assembled, **Then** a duplicate symbol error is reported (immutable via `equ`)
5. **Given** `LDA equ $42`, **When** assembled, **Then** a reserved-name error is reported (name collides with mnemonic)
6. **Given** a constant and a label with the same name, **When** assembled, **Then** a duplicate symbol error is reported
7. **Given** constants referenced in zero-page addressing (`STA zp` where `zp = $10`), **When** assembled, **Then** the assembler uses zero-page encoding

---

### User Story 3 - Current-PC Operator (`*`) (Priority: P1)

A developer uses `*` in expressions to refer to the current program counter value. This enables self-referencing constructs (`jmp *` for infinite loops, `beq *` for traps, and `name = *+1` for self-modifying code addresses).

**Why this priority**: The Dormann suite uses `*` extensively in trap macros (`jmp *`) and self-modifying code addresses (`range_adr = *+1`). This operator is fundamental to the suite's testing pattern.

**Independent Test**: Can be fully tested by assembling instructions containing `*` and verifying the emitted bytes compute the correct PC-relative values.

**Acceptance Scenarios**:

1. **Given** `.org $8000` then `jmp *`, **When** assembled, **Then** the JMP operand is `$8000` (the address of the JMP itself)
2. **Given** `.org $8000` then `beq *`, **When** assembled, **Then** the branch offset is `-2` (branches back to itself)
3. **Given** `.org $8000` then `addr = *+1` then `LDA #$00`, **When** assembled, **Then** `addr` resolves to `$8001` (the operand byte of the LDA)
4. **Given** `*` used in a complex expression like `LDA #(* + 5) & $FF`, **When** assembled, **Then** the expression evaluates with `*` equal to the LDA instruction address
5. **Given** `*` appears after a binary operator (e.g., `LDA #2 * 3`), **When** assembled, **Then** `*` is treated as multiplication (operand byte is `$06`), not the current-PC operator

---

### User Story 4 - Conditional Assembly (Priority: P1)

A developer uses `if` / `else` / `endif` directives to conditionally include or exclude blocks of source based on expression values. This enables assembling the same source with different configurations without editing the source file.

**Why this priority**: The Dormann suite uses conditional assembly to guard entire test sections (decimal mode tests, interrupt tests) behind configuration constants. Without this, the suite source cannot be assembled at all — the assembler would try to parse the `if`/`endif` lines and fail.

**Independent Test**: Can be fully tested by assembling source with conditional blocks and verifying that only the taken branch emits bytes, labels, and constants.

**Acceptance Scenarios**:

1. **Given** `flag = 1` then `if flag` / `NOP` / `endif`, **When** assembled, **Then** one NOP byte is emitted
2. **Given** `flag = 0` then `if flag` / `NOP` / `endif`, **When** assembled, **Then** zero bytes are emitted
3. **Given** `flag = 0` then `if flag` / `NOP` / `else` / `BRK` / `endif`, **When** assembled, **Then** only the BRK byte is emitted
4. **Given** nested conditionals `if a` / `if b` / `NOP` / `endif` / `endif` with `a = 1, b = 0`, **When** assembled, **Then** zero bytes are emitted (inner block is skipped)
5. **Given** a label defined inside a skipped `if` block, **When** that label is referenced elsewhere, **Then** an undefined label error is reported
6. **Given** an `endif` without a matching `if`, **When** assembled, **Then** a clear error is reported
7. **Given** an `if` without a matching `endif`, **When** assembled, **Then** a clear error is reported
8. **Given** a complex conditional guard like `if (data_segment & $ff) != 0`, **When** `data_segment = $200`, **Then** the block is skipped (expression evaluates to 0)
9. **Given** `.if` (dot-prefixed form), **When** assembled, **Then** it is treated identically to `if`

---

### User Story 5 - Macro Definitions and Expansion (Priority: P1)

A developer defines macros with positional parameters and invokes them to expand inline instruction sequences. This enables the suite's test patterns where hundreds of test points are generated from a handful of macro definitions.

**Why this priority**: The Dormann suite defines ~15 macros and invokes them hundreds of times. Without macro support, the suite cannot assemble. Macros depend on expressions (US1), constants (US2), and conditionals (US4).

**Independent Test**: Can be fully tested by defining macros, invoking them with various arguments, and verifying the emitted bytes match the expected expansion.

**Acceptance Scenarios**:

1. **Given** a parameterless macro `trap macro` / `jmp *` / `endm`, **When** `trap` is invoked, **Then** a `JMP` to the current PC is emitted
2. **Given** a macro `load macro` / `LDA #\1` / `endm`, **When** `load $42` is invoked, **Then** `LDA #$42` is emitted
3. **Given** a macro with two parameters `pair macro` / `LDA #\1` / `STA \2` / `endm`, **When** `pair $42, $10` is invoked, **Then** `LDA #$42` followed by `STA $10` is emitted
4. **Given** a macro using `\?` for unique labels, **When** the macro is invoked twice, **Then** each expansion generates distinct internal labels that do not collide
5. **Given** a macro that calls another macro, **When** invoked, **Then** the nested macro is expanded correctly
6. **Given** macro nesting exceeding the depth limit (15 levels), **When** expanded, **Then** a clear error is reported
7. **Given** a macro name that collides with a mnemonic, **When** defined, **Then** an error is reported
8. **Given** an undefined name in the mnemonic position, **When** assembled, **Then** the existing "unrecognized mnemonic" error is reported (not a macro error)
9. **Given** a macro with parameter substitution inside expressions like `LDA #\1 & $FF`, **When** invoked with `$1FF`, **Then** the expression evaluates correctly after substitution

---

### User Story 6 - Colon-less Labels (Priority: P2)

A developer writes labels as bare identifiers at column 0 without a trailing colon, following AS65/CA65 convention. The assembler recognizes these as label definitions equivalent to the colon form.

**Why this priority**: The Dormann suite uses colon-less labels exclusively. While not blocking expression/constant/macro work, this is needed to assemble the actual suite source verbatim.

**Independent Test**: Can be fully tested by assembling source with colon-less labels and verifying they resolve correctly in forward and backward references.

**Acceptance Scenarios**:

1. **Given** `myLabel` at column 0 on its own line, followed by `NOP` on the next line, **When** assembled, **Then** `myLabel` resolves to the address of the NOP
2. **Given** `myLabel NOP` (label and instruction on the same line, label at column 0), **When** assembled, **Then** `myLabel` resolves to the NOP address and the NOP is emitted
3. **Given** a label at column 0 with the same name as a constant, **When** assembled, **Then** a duplicate symbol error is reported
4. **Given** a label NOT at column 0 without a colon, **When** assembled, **Then** it is treated as a mnemonic (not a label) and produces an error if it is not a valid mnemonic
5. **Given** the existing colon form `myLabel:`, **When** assembled, **Then** it continues to work identically (backward compatibility)

---

### User Story 7 - Storage Directives (Priority: P2)

A developer uses `ds` / `dsb` / `.res` directives to reserve blocks of bytes, optionally pre-filled with a value. This is used in the Dormann suite to lay out data segment buffers.

**Why this priority**: The suite uses `ds` to reserve test data buffers. Without it, the data segment layout cannot be reproduced.

**Independent Test**: Can be fully tested by assembling programs with storage directives and verifying the output contains the expected number of bytes at the expected fill value.

**Acceptance Scenarios**:

1. **Given** `ds 16`, **When** assembled, **Then** 16 bytes of `$00` are emitted
2. **Given** `ds 8, $FF`, **When** assembled, **Then** 8 bytes of `$FF` are emitted
3. **Given** `dsb 4, $AA`, **When** assembled, **Then** 4 bytes of `$AA` are emitted
4. **Given** `.res 10`, **When** assembled, **Then** 10 bytes of `$00` are emitted
5. **Given** a label preceding `ds 16`, **When** referenced, **Then** the label resolves to the first reserved byte
6. **Given** `ds` with an expression operand (`ds end - start`), **When** assembled, **Then** the expression is evaluated and that many bytes are reserved

---

### User Story 8 - AS65 Directive Spellings (Priority: P2)

A developer uses AS65-style directive spellings without a leading dot (`org`, `byte`, `byt`, `db`, `word`, `dw`) and pragmas (`noopt`, `opt`). The assembler accepts these as synonyms for the existing dot-prefixed directives.

**Why this priority**: The Dormann suite uses `org` (not `.org`) and AS65-style data directives. Accepting these spellings is low effort but necessary for verbatim source compatibility.

**Independent Test**: Can be fully tested by assembling programs using each AS65 spelling and verifying byte-identical output to the dot-prefixed forms.

**Acceptance Scenarios**:

1. **Given** `org $8000` then `NOP`, **When** assembled, **Then** the NOP is placed at `$8000` (identical to `.org $8000`)
2. **Given** `db $42, $43`, **When** assembled, **Then** two bytes `$42, $43` are emitted (identical to `.byte $42, $43`)
3. **Given** `byt $42`, **When** assembled, **Then** one byte `$42` is emitted
4. **Given** `dw $1234`, **When** assembled, **Then** two bytes `$34, $12` are emitted (identical to `.word $1234`)
5. **Given** `noopt` on its own line, **When** assembled, **Then** no bytes are emitted and no error occurs
6. **Given** `opt` with arguments, **When** assembled, **Then** no bytes are emitted and no error occurs (treated as a no-op)
7. **Given** existing dot-prefixed directives (`.org`, `.byte`, `.word`), **When** assembled, **Then** they continue to work identically (backward compatibility)

---

### User Story 9 - Assertion Directive (Priority: P3)

A developer uses an `ERROR` directive inside a conditional block to produce a fatal assembly error if a configuration constraint is violated (e.g., data segment alignment). When the `if` block is skipped, the `ERROR` directive is silently ignored.

**Why this priority**: The Dormann suite uses `ERROR` to assert compile-time configuration constraints. This is only reached when conditional assembly (US4) evaluates a guard as true. It is low priority because the default Dormann configuration satisfies all constraints, so the `ERROR` lines are always inside skipped blocks.

**Independent Test**: Can be fully tested by assembling source where `ERROR` is inside taken and skipped conditional blocks and verifying the error is raised or suppressed accordingly.

**Acceptance Scenarios**:

1. **Given** `if 1` / `ERROR data segment must be aligned` / `endif`, **When** assembled, **Then** a fatal assembly error is reported with the message "data segment must be aligned"
2. **Given** `if 0` / `ERROR this should not fire` / `endif`, **When** assembled, **Then** no error is reported
3. **Given** `ERROR` with no message text, **When** inside a taken block, **Then** a fatal error is reported with an empty or default message

---

### User Story 10 - End-to-End Dormann Suite Assembly (Priority: P1)

A developer assembles the complete Klaus Dormann 6502 functional test suite (`6502_functional_test.a65`) using the Casso65 assembler and the output binary matches a reference binary produced by an external AS65/CA65 assembler. The binary can then be loaded and executed by the CPU emulator.

**Why this priority**: This is the ultimate validation goal — all individual features (US1–US9) exist to enable this. If this user story passes, the assembler is confirmed capable of handling a real-world, non-trivial 6502 program.

**Independent Test**: Can be tested by assembling the Dormann suite source, comparing the output binary against a reference binary byte-for-byte, and running the binary in the CPU emulator to verify all test sections pass.

**Acceptance Scenarios**:

1. **Given** the Dormann `6502_functional_test.a65` source (with `report = 0`, `disable_decimal = 0`), **When** assembled, **Then** assembly succeeds with zero errors
2. **Given** the assembled binary, **When** compared byte-for-byte against the pre-built reference binary from the Klaus2m5 `bin_files/` directory, **Then** the binaries are identical
3. **Given** the assembled binary loaded into the CPU emulator, **When** executed, **Then** the CPU reaches the "all tests passed" success trap address without hitting any failure trap
4. **Given** the Dormann source with `disable_decimal = 1`, **When** assembled, **Then** the decimal mode test section is excluded via conditional assembly and the remaining tests pass

---

### User Story 11 - Advanced Macro Features (Priority: P2)

A developer uses `local` declarations for macro-local labels and `exitm` for early exit from macro expansion, enabling more complex macro patterns with conditional early termination.

**Why this priority**: The Dormann suite does not use `local` or `exitm`, but these are core AS65 macro features documented in the manual. Full AS65 parity requires them.

**Independent Test**: Can be fully tested by defining macros with `local` labels and `exitm` guards, then verifying correct expansion and early termination.

**Acceptance Scenarios**:

1. **Given** a macro with `local MyLabel` and `MyLabel` used inside, **When** invoked twice, **Then** each expansion gets a unique label (no collision)
2. **Given** a macro with `exitm` inside an `if` block, **When** the condition is true, **Then** expansion stops at `exitm` and remaining macro lines are skipped
3. **Given** a macro with `exitm` inside an `if` block, **When** the condition is false, **Then** the full macro body is expanded
4. **Given** `\0` referenced in a macro body, **When** invoked with 3 arguments, **Then** `\0` evaluates to `3` (argument count)

---

### User Story 12 - File Inclusion (Priority: P2)

A developer uses the `include` directive to splice another source file into the assembly at that point, enabling modular source organization.

**Why this priority**: The Dormann suite uses `include "report.i65"` when `report = 1`. Full AS65 parity requires include support.

**Independent Test**: Can be fully tested by assembling a source that includes another file and verifying the included content is assembled correctly.

**Acceptance Scenarios**:

1. **Given** `include "defs.i"` in the source, **When** assembled, **Then** the contents of `defs.i` are assembled at that point
2. **Given** nested includes (A includes B, B includes C), **When** assembled, **Then** all files are processed correctly
3. **Given** an error in an included file, **When** assembled, **Then** the error message reports the included file's name and line number
4. **Given** a missing include file, **When** assembled, **Then** a clear error is reported

---

### User Story 13 - Structure Definitions (Priority: P3)

A developer uses `struct` / `end struct` to define data structure layouts, generating named offset constants automatically. This replaces manual `equ` chains for complex data layouts.

**Why this priority**: Structures are a convenience feature in AS65. Not required for the Dormann suite, but part of the complete AS65 feature set.

**Independent Test**: Can be fully tested by defining structs and verifying the generated offset constants match expected values.

**Acceptance Scenarios**:

1. **Given** `struct Node` / `dw Next` / `dw Prev` / `db Type` / `end struct`, **When** assembled, **Then** `Next = 0`, `Prev = 2`, `Type = 4`, `Node = 5`
2. **Given** `struct Foo, 4` (starting offset 4), **When** assembled, **Then** the first member offset starts at 4
3. **Given** `align` inside a struct, **When** assembled, **Then** the offset is padded to the next even boundary
4. **Given** `align 8` inside a struct, **When** assembled, **Then** the offset is padded to the next multiple of 8
5. **Given** `label Name` inside a struct, **When** assembled, **Then** `Name` equals the current offset without advancing it

---

### User Story 14 - Character Mapping (Priority: P3)

A developer uses `cmap` to remap character values in `db` strings, enabling custom character encodings (e.g., PETSCII, EBCDIC, or case conversion).

**Why this priority**: Character mapping is a specialized AS65 feature. Not required for the Dormann suite, but part of the complete AS65 feature set.

**Independent Test**: Can be fully tested by setting up a character map and verifying `db` strings emit remapped byte values.

**Acceptance Scenarios**:

1. **Given** `cmap "a","ABCDEFGHIJKLMNOPQRSTUVWXYZ"` then `db "abc"`, **When** assembled, **Then** bytes `$41, $42, $43` are emitted (uppercase mapping)
2. **Given** `cmap` (no args), **When** assembled, **Then** the mapping is reset to identity
3. **Given** `cmap -1` then `db "abc"`, **When** assembled, **Then** all bytes are `$FF` (forced value)

---

### User Story 15 - Output Formats (Priority: P2)

A developer selects between binary, Motorola S-record, or Intel HEX output formats for the assembled output, matching AS65's `-s` and `-s2` flags.

**Why this priority**: Full CLI parity with AS65 requires all three output formats. Binary is already implemented.

**Independent Test**: Can be fully tested by assembling the same source in each format and verifying the output file structure matches the format specification.

**Acceptance Scenarios**:

1. **Given** the `-s` flag, **When** assembled, **Then** a Motorola S-record file is written containing only the used memory regions
2. **Given** the `-s2` flag, **When** assembled, **Then** an Intel HEX file is written containing only the used memory regions
3. **Given** no format flag, **When** assembled, **Then** a flat binary file is written (existing behavior)
4. **Given** the `-z` flag with binary output, **When** assembled, **Then** unused bytes are filled with `$00` instead of `$FF`

---

### User Story 16 - Listing Enhancements (Priority: P2)

A developer controls listing output with `list`/`nolist` directives, page breaks with `page`, and page headers with `title`. Cycle counts can be shown per instruction with the `-c` flag.

**Why this priority**: Full AS65 listing parity. The basic listing from spec 001 is extended with these controls.

**Independent Test**: Can be fully tested by assembling with various listing options and verifying the listing output contains the expected formatting.

**Acceptance Scenarios**:

1. **Given** `nolist` in the source, **When** assembled with listing, **Then** lines after `nolist` are omitted from the listing until `list` re-enables them
2. **Given** `title "My Program"`, **When** assembled with listing, **Then** page headers show the title
3. **Given** `page`, **When** assembled with listing, **Then** a page break is inserted at that point
4. **Given** the `-c` flag, **When** assembled, **Then** instruction cycle counts appear in the listing
5. **Given** the `-p` flag, **When** assembled, **Then** a pass 1 listing is generated in addition to pass 2
6. **Given** the AS65 `testcase.a65` assembled with listing, **When** compared to hand-computed expected listing fragments derived from the AS65 manual, **Then** the listing format matches (column layout, address format, byte display, page headers, symbol table)

---

### User Story 17 - CLI Parity with AS65 (Priority: P2)

A developer uses all AS65 command-line options with the Casso65 executable, including symbol pre-definition, quiet mode, verbose mode, case insensitivity, and optimization control.

**Why this priority**: Full command-line parity ensures Casso65 can be used as a drop-in replacement for AS65.

**Independent Test**: Can be fully tested by running the executable with each flag combination and verifying the expected behavior.

**Acceptance Scenarios**:

1. **Given** `-dDEBUG`, **When** assembled, **Then** the symbol `DEBUG` is defined as `1` before the first source line
2. **Given** `-dMYVAR`, **When** assembled, **Then** the symbol `MYVAR` is defined as `1`
3. **Given** `-i`, **When** assembled, **Then** opcodes are case-insensitive (already default; flag accepted for compatibility)
4. **Given** `-q`, **When** assembled, **Then** no progress counter is displayed on stderr
5. **Given** `-v`, **When** assembled, **Then** verbose information is displayed on stdout
6. **Given** `-t`, **When** assembled, **Then** a symbol table is printed between passes
7. **Given** `-n`, **When** assembled, **Then** no optimizations are applied even if `opt` appears in source
8. **Given** `-g`, **When** assembled, **Then** a source-level debug information file is generated
9. **Given** `-tlmylist`, **When** assembled, **Then** both `-t` (symbol table) and `-l mylist` (listing to `mylist.lst`) are applied (flag concatenation)
10. **Given** `-h80t`, **When** assembled, **Then** page height is 80 lines and symbol table is generated (numeric parameter followed by flag)
11. **Given** `casso65 -l -s2 -w myfile`, **When** run, **Then** AS65 mode is assumed (no subcommand), with pass-2 listing, Intel HEX output, and 133-column width
12. **Given** a source file `test` (no extension), **When** `test` is not found but `test.a65` exists, **Then** `test.a65` is assembled
13. **Given** `-onul`, **When** assembled, **Then** assembly proceeds but no output file is written

---

### User Story 18 - Number Format Extensions (Priority: P2)

A developer uses octal (`@`), C-style (`0x`, `0b`), and arbitrary-base (`<base>#<value>`) number prefixes in expressions, matching the full AS65 number format support.

**Why this priority**: AS65 supports these formats and real-world 6502 sources may use them.

**Independent Test**: Can be fully tested by assembling instructions with each number format and verifying the correct numeric value.

**Acceptance Scenarios**:

1. **Given** `LDA #@17`, **When** assembled, **Then** the operand byte is `$0F` (octal 17 = decimal 15)
2. **Given** `LDA #0xFF`, **When** assembled, **Then** the operand byte is `$FF`
3. **Given** `LDA #0b11110000`, **When** assembled, **Then** the operand byte is `$F0`
4. **Given** `LDA #16#FF`, **When** assembled, **Then** the operand byte is `$FF`
5. **Given** `LDA #2#11110000`, **When** assembled, **Then** the operand byte is `$F0`

---

### User Story 19 - Instruction Synonyms and Multi-NOP (Priority: P3)

A developer uses AS65's instruction synonyms (`disable`=`sei`, `enable`=`cli`, `stc`=`sec`, `sti`=`sei`) and the extended `nop <expr>` form to emit multiple NOP instructions.

**Why this priority**: These are minor convenience features in AS65. Not commonly used but part of full parity.

**Independent Test**: Can be fully tested by assembling each synonym and verifying the correct opcode is emitted.

**Acceptance Scenarios**:

1. **Given** `disable`, **When** assembled, **Then** the `SEI` opcode is emitted
2. **Given** `enable`, **When** assembled, **Then** the `CLI` opcode is emitted
3. **Given** `stc`, **When** assembled, **Then** the `SEC` opcode is emitted
4. **Given** `sti`, **When** assembled, **Then** the `SEI` opcode is emitted
5. **Given** `nop 5`, **When** assembled, **Then** five NOP opcodes are emitted
6. **Given** `nop` (no operand), **When** assembled, **Then** one NOP opcode is emitted (existing behavior)

---

### Edge Cases

- What happens when a constant forward-references another constant that has not yet been defined? It is resolved in pass 2, the same as forward label references.
- What happens when `*` is the very first token in an expression (e.g., `* + 5`)? It is treated as the current-PC operator since there is no preceding operand.
- What happens when a macro parameter `\1` appears but no argument was passed? An error is reported indicating a missing macro argument.
- What happens when a macro is defined but never invoked? No error or warning (macros are inert until called).
- What happens when `if` / `endif` straddle a macro boundary (e.g., `if` inside a macro, `endif` outside)? An error is reported — conditional blocks must be self-contained within their definition context.
- What happens with `ds 0`? Zero bytes are emitted — this is not an error.
- What happens when `=` or `equ` appears without a name (e.g., `= 5`)? A parse error is reported.
- What happens when nested `if` depth exceeds a reasonable limit? A clear error is reported (limit of 32 nesting levels).
- What happens when an include file includes itself? A nesting depth limit (e.g., 16) prevents infinite recursion.
- What happens with `5**` (five times current-PC)? The assembler determines from context that the first `*` is multiplication and the second is current-PC.
- What happens with `2+*/2` (two plus current-PC divided by two)? The assembler determines from context that `*` is current-PC.
- What happens when a `struct` contains nested `struct`? This is not supported — structs cannot nest.
- What happens when `exitm` is used outside a macro? An error is reported.
- What happens when `\0` is used in a macro invoked with 0 arguments? `\0` evaluates to `0`.

## Requirements

### Functional Requirements

- **FR-001**: The assembler MUST support a full expression evaluator with the following operators (precedence high to low): unary (`-`, `+`, `!`, `~`, `<`, `>`), multiplicative (`*`, `/`, `%`), additive (`+`, `-`), shift (`<<`, `>>`), comparison (`<`, `>`, `<=`, `>=`), equality (`=`, `==`, `!=`), bitwise AND (`&`), bitwise XOR (`^`), bitwise OR (`|`), logical AND (`&&`), logical OR (`||`), and `hi`/`lo` keyword unary operators
- **FR-001a**: The expression evaluator MUST support `++` (increment) and `--` (decrement) as unary prefix operators, matching AS65
- **FR-001b**: `[` and `]` MUST be accepted as alternative grouping symbols equivalent to `(` and `)`, matching AS65
- **FR-002**: Expressions MUST support parenthetical grouping with `(` and `)` to override precedence
- **FR-003**: Expressions MUST evaluate internally using 32-bit signed integer arithmetic, with the final result range-checked and truncated to the width required by the consumer (1 byte for immediate/zero-page, 2 bytes for absolute, 1 signed byte for branch offset)
- **FR-004**: The `<` and `>` unary operators MUST continue to function as low-byte and high-byte extraction, equivalent to `(expr) & $FF` and `((expr) >> 8) & $FF` respectively
- **FR-005**: All existing expression forms (`<label`, `>label`, `label+offset`) MUST produce byte-identical output to spec 001 (backward compatibility)
- **FR-006**: The assembler MUST support constant definitions via `NAME = EXPR` syntax
- **FR-007**: The assembler MUST support constant definitions via `NAME equ EXPR` syntax (case-insensitive keyword)
- **FR-008**: Constants MUST participate in the symbol table identically to labels — they can be referenced anywhere a label can be referenced
- **FR-009**: Constants defined via `=` MUST be evaluated eagerly when encountered (in both passes). Constants defined via `equ` whose right-hand side contains forward references MUST be resolved in pass 2, like labels
- **FR-010**: Constants defined via `equ` are immutable — redefining an `equ` constant or a label MUST produce a duplicate symbol error. Constants defined via `=` are reassignable — re-assignment with `=` MUST update the value silently. A name first defined as `equ` or label MUST NOT be reassigned via `=`, and vice versa
- **FR-011**: A constant name that collides with a mnemonic or register name MUST be rejected (consistent with FR-024 from spec 001)
- **FR-012**: The assembler MUST recognize `*` as the current-PC operator when it appears as a primary (start of an expression, or immediately after an operator or open parenthesis). `$` alone (not followed by a hex digit) MUST also be treated as the current-PC operator, matching AS65. `$` followed by a hex digit is a hex literal as before
- **FR-013**: `*` MUST evaluate to the address that the current line's first byte will be emitted at (the PC before the line's bytes are written), in both pass 1 and pass 2
- **FR-014**: When `*` appears after an operand (e.g., `2 * 3`), it MUST be treated as the multiplication operator, not the current-PC operator
- **FR-015**: `jmp *` MUST assemble to a JMP whose target is its own address; `beq *` MUST assemble to a branch offset of `-2`
- **FR-016**: The assembler MUST support `if EXPR` / `else` / `endif` conditional assembly directives (case-insensitive, with or without leading dot)
- **FR-017**: When `EXPR` in an `if` directive evaluates to non-zero, the body MUST be assembled; when zero, the body MUST be skipped
- **FR-018**: Skipped conditional blocks MUST consume zero bytes and define zero symbols/constants
- **FR-019**: Conditional blocks MUST nest to at least 32 levels deep
- **FR-020**: An unmatched `else` or `endif` MUST produce a clear error; an unclosed `if` at end of source MUST produce a clear error
- **FR-021**: Comparison operators (`=`, `==`, `!=`, `<`, `<=`, `>`, `>=`) in `if` expressions MUST produce `1` (true) or `0` (false)
- **FR-022**: The assembler MUST support macro definitions via `NAME macro` / `endm` blocks (case-insensitive keywords)
- **FR-023**: Macro invocations MUST expand inline at the call site, with `\1` through `\9` replaced by the comma-separated arguments (textual substitution, whitespace trimmed)
- **FR-024**: `\?` within a macro body MUST be replaced by a per-expansion unique suffix to prevent label collisions across invocations
- **FR-025**: Macro bodies MAY contain invocations of other macros or themselves (self-recursion), bounded by a maximum depth of 15 levels (matching AS65)
- **FR-026**: Defining a macro whose name collides with a mnemonic, register, or existing macro MUST produce an error
- **FR-027**: An undefined name in the mnemonic position MUST fall through to the existing "unrecognized mnemonic" error
- **FR-028**: The assembler MUST accept labels at column 0 without a trailing colon as label definitions, identical to the colon form
- **FR-029**: An identifier at column 0 followed by an instruction on the same line MUST define the label and assemble the instruction (e.g., `loop NOP`)
- **FR-030**: The colon label form MUST continue to work identically (backward compatibility)
- **FR-031**: The assembler MUST support `ds`, `dsb`, and `.res` directives to reserve N bytes, optionally filled with a specified value (default `$00`)
- **FR-032**: The count operand of storage directives MUST accept a full expression
- **FR-033**: The assembler MUST accept the following AS65-style directive spellings as synonyms: `org` (= `.org`), `db` / `byt` / `byte` (= `.byte`), `dw` / `word` (= `.word`)
- **FR-034**: `noopt` and `opt` directives MUST be accepted as harmless no-ops (zero bytes emitted, no error)
- **FR-034a**: `data`, `bss`, and `code` segment directives MUST switch the current segment (see FR-077 for segment model)
- **FR-034b**: `end` directive MUST stop assembly (remaining lines are ignored); the optional operand (e.g., `end start`) sets the program entry point for S-record/Intel HEX output and MUST be displayed on stderr when the program is assembled
- **FR-034b2**: When no `org` directive is given, the default origin address MUST be 0 (matching AS65). Note: spec 001 used `$8000` as default — this is an intentional AS65-compatibility change
- **FR-034d**: The expression evaluator MUST support `lo` and `hi` as unary keyword operators equivalent to `<` and `>` (low-byte and high-byte extraction), usable both with and without parentheses: `lo(expr)`, `lo expr`, `hi(expr)`, `hi expr`
- **FR-034e**: The expression evaluator MUST support character constants in single quotes (e.g., `'A'` evaluates to `$41`)
- **FR-035**: The assembler MUST support an `ERROR` directive that, when inside a taken conditional block, produces a fatal assembly error with the rest of the source line as the message
- **FR-036**: An `ERROR` directive inside a skipped conditional block MUST produce no diagnostic
- **FR-037**: Macro expansion MUST be reflected in listing output (expanded lines shown individually, prefixed with `>`)
- **FR-037a**: The listing output format (column layout, address display, byte display, source text alignment, page headers, symbol table format, pass separators) MUST match the AS65 v1.42 format as documented in `as65.man`. Hand-computed expected listing fragments are used for validation.
- **FR-038**: Conditional assembly skipping MUST be reflected in listing output (skipped lines omitted or marked)
- **FR-039**: The assembler MUST support `local` declarations inside macros to declare macro-local labels, as an alternative to `\?`
- **FR-040**: The assembler MUST support `exitm` inside macro bodies to terminate expansion early; `exitm` outside a macro MUST produce an error
- **FR-041**: `\0` in a macro body MUST evaluate to the number of arguments passed to the macro invocation
- **FR-042**: Macros MUST support named parameters in the definition (`NAME macro text, value`) as an alternative to `\1`–`\9` positional references. Named parameter values MUST be substituted even inside quoted strings and character constants (e.g., `db "value=",'value',0` with `value` as a named param)
- **FR-043**: Macro nesting depth MUST be limited to 15 levels (matching AS65)
- **FR-044**: The assembler MUST support `include "filename"` to splice another source file at the include point; include nesting MUST be limited to 16 levels
- **FR-045**: Errors in included files MUST report the included file's name and line number
- **FR-046**: The assembler MUST support `struct NAME` / `end struct` for data structure layout; within structs, `db`, `dw`, `ds`, `label`, and `align` define member offsets; the struct name is set to the total structure size
- **FR-047**: `struct NAME, EXPR` MUST set the starting offset to `EXPR` instead of 0
- **FR-048**: The assembler MUST support `cmap` for character mapping: `cmap` (reset to identity), `cmap EXPR` (force all chars to value), `cmap "char", bytelist` (map range starting at char)
- **FR-049**: Character mapping MUST affect byte values emitted by `db` / `fcb` / `fcc` string arguments only
- **FR-050**: The assembler MUST support Motorola S-record output (CLI `-s` flag) containing only used memory regions
- **FR-051**: The assembler MUST support Intel HEX output (CLI `-s2` flag) containing only used memory regions
- **FR-052**: The assembler MUST support `list` and `nolist` directives to enable/disable listing generation; `nolist` is cumulative (two `nolist` calls require two `list` calls to re-enable). `list`/`nolist` MUST have no effect when no `-p` or `-l` option was specified on the command line
- **FR-053**: The assembler MUST support `page` (unconditional page break) and `page EXPR` (conditional page break — continue on next page only if EXPR lines do not fit on the current page) directives for listing
- **FR-054**: The assembler MUST support `title "string"` to set the listing page header title
- **FR-055**: The CLI MUST support `-c` to show instruction cycle counts in the listing
- **FR-056**: The CLI MUST support `-p` to generate a pass 1 listing
- **FR-057**: The CLI MUST support `-t` to generate a symbol table between passes; redefinable labels (defined via `=`/`set`) MUST be marked with an asterisk in the symbol table output
- **FR-058**: The CLI MUST support `-d<name>` to pre-define a symbol as `1` before assembly; `-d` with no name MUST define `DEBUG`
- **FR-059**: The CLI MUST support `-q` for quiet mode (no progress on stderr). By default (without `-q`), the assembler MUST display a running line counter on stderr during assembly, matching AS65 behavior
- **FR-059a**: The CLI MUST support `-v` for verbose mode, displaying additional information (pass progress, symbol resolution details, byte counts) on stdout
- **FR-060**: The CLI MUST support `-n` to globally disable optimizations regardless of `opt` in source
- **FR-061**: The CLI MUST support `-g` to generate a source-level debug information file
- **FR-062**: The CLI MUST support `-i` for case-insensitive opcodes (Casso65 already defaults to case-insensitive; flag accepted for AS65 compatibility)
- **FR-063**: The CLI MUST support `-o<filename>` to specify the output file name; the assembler MUST auto-add `.bin`, `.s19`, or `.hex` extension when no extension is given
- **FR-064**: The CLI MUST support `-h<lines>` to set listing page height; `-h0` means infinite page (no page breaks between code)
- **FR-065**: The CLI MUST support `-w<width>` to set listing column width (default 79, `-w` alone means 133)
- **FR-065a**: The CLI MUST support `-l` (generate pass 2 listing) and `-l<filename>` (set listing file name). When neither `-p` nor `-t` is specified and `-l<filename>` is given, a pass 2 listing is auto-generated. The filename `-` directs listing to stdout
- **FR-065b**: The CLI MUST support `-m` to show macro expansions in the listing (macro lines prefixed with `>`)
- **FR-065c**: The CLI MUST support AS65 flag concatenation rules: flags may be concatenated (e.g., `-tlfile` = `-t -lfile`, `-h80t` = `-h80 -t`), but no other flag may follow one that has a string parameter
- **FR-065d**: When the source file is not found, the CLI MUST try appending `.a65`, `.asm`, and `.s` extensions before reporting an error
- **FR-065e**: The CLI MUST accept `nul` as an output filename to discard the output file
- **FR-065f**: The CLI MUST support AS65 invocation syntax (`casso65 [-flags] file` with a single positional file argument) in addition to the existing Casso65 subcommand syntax (`casso65 assemble ...`, `casso65 run ...`). When invoked without a subcommand keyword, AS65 mode is assumed
- **FR-065g**: The CLI MUST return AS65-compatible exit codes: 0 (success), 1 (bad command-line parameter), 2 (unable to open file), 3 (assembly errors), 4 (memory allocation failure)
- **FR-066**: The expression evaluator MUST support `!` (logical NOT: evaluates to 0 if nonzero, 1 if zero) as distinct from `~` (binary NOT)
- **FR-067**: The expression evaluator MUST support `&&` (logical AND) and `||` (logical OR), both evaluating to 0 or 1
- **FR-068**: The expression evaluator MUST support `@` as an octal number prefix (e.g., `@17` = 15 decimal)
- **FR-069**: The expression evaluator MUST support `0x` and `0b` as hexadecimal and binary number prefixes (C-style)
- **FR-070**: The expression evaluator MUST support `<base>#<number>` format for arbitrary bases 2–36
- **FR-071**: `db` strings MUST support escape sequences: `\a` (bell), `\b` (backspace), `\n` (newline), `\r` (carriage return), `\t` (tab), `\\` (backslash), `\"` (double quote)
- **FR-072**: The assembler MUST accept `fcb`, `fcc`, `fcw`, `fdb`, and `rmb` as directive synonyms. `fcb` accepts only expressions (not strings), `fcc` accepts only strings (not expressions), `db` accepts both. `fcw`/`fdb` = `dw`, `rmb` = `ds`
- **FR-073**: The assembler MUST accept `set` as a synonym for `=` (reassignable constant)
- **FR-074**: The assembler MUST accept instruction synonyms: `disable` = `SEI`, `enable` = `CLI`, `stc` = `SEC`, `sti` = `SEI`, `std` = `SED`
- **FR-074a**: The assembler MUST accept `dd` as a define-double-word directive (4 bytes, little-endian)
- **FR-075**: `nop EXPR` MUST emit the specified number of NOP instructions; `nop` without an operand MUST emit one NOP
- **FR-076**: `align EXPR` MUST pad with fill bytes until (address % EXPR) == 0; `align` without an operand MUST pad to the next even address
- **FR-077**: The assembler MUST support three segments (`code`, `data`, `bss`) with independent location counters; for flat binary output, `code` and `data` segments are concatenated and `bss` reserves zero-filled space
- **FR-078**: The `-z` CLI flag MUST fill unused binary output bytes with `$00` instead of the default `$FF`; with S-records, `-z` forces an S9 start-address record
- **FR-079**: AS65 conformance MUST be validated by a comprehensive test suite with hand-computed expected outputs. For each test case, the expected binary and listing output are derived from the AS65 manual and verified by inspection. No external assembler binary is executed.

### Key Entities

- **Constant**: A named value defined via `=`, `set`, or `equ`, stored in the symbol table alongside labels. `=`/`set` constants are reassignable; `equ` constants are immutable.
- **Expression**: A combination of numeric literals, symbols (labels and constants), the current-PC operator (`*`), and operators, evaluated to produce a numeric value. Supports C-style operators, AS65 `lo`/`hi` keywords, and multiple number base formats.
- **Macro Definition**: A named template of source lines stored during pass 1, expanded inline each time the name appears in the mnemonic position. Supports positional (`\1`–`\9`), named parameters, `\0` (arg count), `\?` (unique suffix), `local` labels, and `exitm`.
- **Conditional Block**: A region of source delimited by `if`/`else`/`endif` that is assembled or skipped based on a compile-time expression value. Affects both byte emission and symbol/constant definition.
- **Segment**: One of three memory regions (`code`, `data`, `bss`) with independent location counters. Segments control where assembled output is placed in the final binary.
- **Structure**: A compile-time layout definition (`struct`/`end struct`) that generates named offset constants for data structure members.
- **Character Map**: A 256-entry byte-to-byte translation table applied to string arguments of `db`/`fcb`/`fcc` directives.

## Success Criteria

### Measurable Outcomes

- **SC-001**: The Klaus Dormann `6502_functional_test.a65` assembles with zero errors using the Casso65 assembler
- **SC-002**: The assembled Dormann binary is byte-identical to the pre-built reference binary from the Klaus2m5 `bin_files/` directory
- **SC-003**: The assembled Dormann binary executes in the CPU emulator and reaches the success trap without hitting any failure trap
- **SC-004**: All existing assembler unit tests from spec 001 continue to pass without modification (backward compatibility)
- **SC-005**: Expressions with 5+ operators and nested parentheses evaluate correctly in 100% of test cases
- **SC-006**: Macro expansion of the Dormann suite's ~15 macro definitions across hundreds of invocation sites produces correct output
- **SC-007**: Conditional assembly correctly includes or excludes test sections based on configuration constants (`disable_decimal`, `I_flag`, `report`)
- **SC-008**: The AS65 test case file (`testcase.a65` from `as65_142.zip`) assembles without errors
- **SC-009**: All AS65 command-line flags are accepted and produce the expected behavior
- **SC-010**: S-record and Intel HEX output files are valid and parseable by standard tools
- **SC-011**: Listing output format matches the AS65 v1.42 format as documented in `as65.man` (column layout, address format, byte display, page headers, symbol table format)
- **SC-012**: A comprehensive conformance test suite of 200+ individual test cases covers every operator, directive, macro feature, number format, addressing mode interaction, and edge case — each with hand-computed expected outputs

## Scope Boundary

### Out of Scope

- 65SC02 / 65C02 extensions (`-x` flag, `bra`, `phx`, `phy`, `plx`, `ply`, `stz`, `trb`, `tsb`, `(zp)` indirect) — deferred to a separate spec
- 65C02-specific instruction synonyms (`clr` = `stz`)
- Relocatable output or linking
- AS65 optimization mode (zero-page promotion via `opt`/`noopt`) — `opt`/`noopt` are accepted as no-ops but do not change assembly behavior

### Design Constraint: CPU Architecture Extensibility

The assembler MUST be designed so that adding support for a new CPU architecture (e.g., 65C02, 65816, Z80) requires minimal change to the core assembler engine. Specifically:

- The instruction set (mnemonics, addressing modes, opcodes, cycle counts) MUST be data-driven and injected into the assembler, not hardcoded
- The expression evaluator, macro engine, conditional assembly, directive processing, segment model, listing, and CLI MUST be CPU-independent
- Adding a new CPU MUST require only: (1) a new instruction table, (2) any new addressing mode patterns in the operand classifier, (3) a CPU-selection mechanism (e.g., CLI flag or source directive)
- The existing `Microcode` / `OpcodeTable` architecture from spec 001 already supports this — this spec MUST NOT break that extensibility

## Assumptions

- Spec 001 (assembler foundation) is fully implemented and all its tests pass — this spec extends that foundation
- The reference assembler documentation is AS65 v1.42 by Frank A. Kingswood (`as65.man`). The AS65 binary is NOT used — it is closed-source and cannot be built from source, so running it is a security risk. Conformance is validated by hand-computed expected outputs and the AS65 manual.
- The AS65 test case file (`testcase.a65`) from `as65_142.zip` will be used as an integration test — we verify it assembles without errors
- The Dormann `6502_functional_test.a65` (GPL-3.0) is NOT committed. A test script downloads it, assembles it, compares output against the pre-built reference binary from the Klaus2m5 `bin_files/` directory, and deletes all downloaded files.
- CPU emulator validation will use Tom Harte's SingleStepTests (MIT-licensed, JSON-based, per-opcode) — deferred to a separate spec
- The Dormann `6502_functional_test.a65` source will be used with default configuration (`report = 0`, `disable_decimal = 0`, `I_flag = 3`) as the primary validation target
- A reference binary for the Dormann suite is available in the Klaus2m5 `bin_files/` directory (data file, not executable) — downloaded on demand for comparison
- The existing two-pass architecture is sufficient — `=` constants are evaluated eagerly when encountered, `equ` constants and labels resolve in pass 2
- Macro expansion is textual substitution (AS65-style), not semantic — parameter values are spliced as raw text before parsing
- The `*` operator disambiguation (current-PC vs. multiplication) follows AS65 convention: `*` is current-PC when it appears as a primary, multiplication when it appears as a binary operator
- AS65 is case-sensitive by default (opcodes must be lowercase). Casso65 defaults to case-insensitive. The `-i` flag is accepted for compatibility but is effectively a no-op since Casso65 is already insensitive
- The debug information file format (`-g`) will match AS65's format as documented; the `READDBG` sample code from the AS65 distribution is the reference

## Clarifications

### Session 2026-04-25

- Q: Should constants defined via `=` be reassignable (AS65 semantics) or immutable (current FR-010)? → A: Follow AS65 conventions. `=` constants are reassignable (re-assignment updates the value); `equ` constants are immutable (redefinition is an error). The Dormann suite uses `test_num = test_num + 1` inside the `next_test` macro.
- Q: Which `I_flag` value should be the primary validation target? → A: `I_flag = 3` (the source file's actual default), which allows full SEI/CLI testing.
- Q: How should `=` vs `equ` constants be evaluated across passes? → A: Follow AS65 conventions. `=` assignments evaluated eagerly when encountered (in both passes); `equ` and labels deferred to pass 2. Two passes remain sufficient.
- Q: Should the spec cover the full AS65 feature set or only what's needed for Dormann? → A: Full, exact AS65 clone (6502 mode). All syntax, directives, macro features, expression operators, output formats, and CLI options. 65C02/65SC02 support is deferred to a separate spec.
- Q: Should the default case-sensitivity match AS65 (lowercase opcodes required)? → A: No. Keeping case-insensitive default is a non-breaking change — any valid AS65 source (lowercase) works fine. Users who write uppercase (which AS65 would reject) also work. `-i` flag accepted for compatibility.
- Q: Should the default origin address match AS65 (0) or keep Casso65's $8000? → A: Match AS65 — default origin is 0 when no `.org` is given.
- Q: How do we handle licensing and security for test assets? → A: Nothing GPL or proprietary is committed. The AS65 binary is closed-source and MUST NOT be downloaded or executed (security risk). Conformance is validated via hand-computed expected outputs derived from the AS65 manual. Dormann source is downloaded on demand and compared against pre-built reference binaries from `bin_files/` (data, not executables). CPU validation uses Tom Harte's MIT-licensed SingleStepTests (separate spec).- Q: Should the assembler be designed for CPU extensibility? → A: Yes. The core engine (expressions, macros, conditionals, directives, segments, listing, CLI) must be CPU-independent. Adding a new architecture (65C02, 65816, Z80, etc.) should require only a new instruction table and any new addressing mode patterns — no changes to the assembler core.