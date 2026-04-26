# Conformance Test Plan: AS65 Assembler Clone

**Feature**: 002-as65-assembler-compat  
**Date**: 2026-04-25  
**Reference**: AS65 v1.42 by Frank A. Kingswood (`as65.man`)

## Testing Strategy

Every test case is a `.a65` source file assembled by BOTH:
1. The real AS65 v1.42 binary (to produce reference output)
2. Casso65 (to produce test output)

Comparison modes:
- **Binary**: byte-for-byte comparison of output `.bin` files
- **Listing**: character-for-character comparison of `.lst` files
- **Errors**: expected error messages (for negative tests)

Reference files are committed to `specs/002-as65-assembler-compat/testdata/conformance/` and generated once by a PowerShell script that runs AS65 on all test inputs.

## Test Categories

### 1. Expression Evaluator (30+ tests)

#### 1.1 Arithmetic Operators
- `expr_add.a65`: `db 3+5` → `$08`
- `expr_sub.a65`: `db 10-3` → `$07`
- `expr_mul.a65`: `db 3*4` → `$0C`
- `expr_div.a65`: `db 15/3` → `$05`
- `expr_mod.a65`: `db 17%5` → `$02`
- `expr_neg.a65`: `db -1&$ff` → `$FF`
- `expr_unary_plus.a65`: `db +5` → `$05`

#### 1.2 Bitwise Operators
- `expr_and.a65`: `db $FF&$0F` → `$0F`
- `expr_or.a65`: `db $80|$01` → `$81`
- `expr_xor.a65`: `db $FF^$0F` → `$F0`
- `expr_not.a65`: `db ~$0F&$FF` → `$F0`
- `expr_shl.a65`: `db 1<<4` → `$10`
- `expr_shr.a65`: `db $80>>4` → `$08`

#### 1.3 Logical Operators
- `expr_logical_not.a65`: `db !0` → `$01`; `db !5` → `$00`
- `expr_logical_and.a65`: `db 1&&1` → `$01`; `db 1&&0` → `$00`
- `expr_logical_or.a65`: `db 0||1` → `$01`; `db 0||0` → `$00`

#### 1.4 Comparison Operators
- `expr_eq.a65`: `db 5=5` → `$01`; `db 5=6` → `$00`
- `expr_ne.a65`: `db 5!=6` → `$01`; `db 5!=5` → `$00`
- `expr_lt.a65`: `db 3<5` → `$01`; `db 5<3` → `$00`
- `expr_le.a65`: `db 3<=3` → `$01`; `db 4<=3` → `$00`
- `expr_gt.a65`: `db 5>3` → `$01`; `db 3>5` → `$00`
- `expr_ge.a65`: `db 3>=3` → `$01`; `db 2>=3` → `$00`

#### 1.5 Precedence & Grouping
- `expr_precedence.a65`: `db 2+3*4` → `$0E` (14, not 20)
- `expr_parens.a65`: `db (2+3)*4` → `$14` (20)
- `expr_brackets.a65`: `db [2+3]*4` → `$14` (brackets = parens)
- `expr_complex.a65`: `db ($FF&~$0F)|($80>>4)` → `$F8`
- `expr_nested.a65`: `db ((1+2)*(3+4))&$FF` → `$15` (21)

#### 1.6 Number Formats
- `num_hex.a65`: `db $FF` → `$FF`
- `num_bin.a65`: `db %11110000` → `$F0`
- `num_oct.a65`: `db @17` → `$0F`
- `num_dec.a65`: `db 255` → `$FF`
- `num_0x.a65`: `db 0xFF` → `$FF`
- `num_0b.a65`: `db 0b11110000` → `$F0`
- `num_base.a65`: `db 16#FF` → `$FF`; `db 2#1010` → `$0A`
- `num_char.a65`: `db 'A'` → `$41`; `db 'Z'-'A'` → `$19`

#### 1.7 Current-PC Operators
- `pc_star.a65`: `org $100` / `jmp *` → JMP to $100
- `pc_star_expr.a65`: `org $100` / `dw *+5` → `$05,$01`
- `pc_dollar.a65`: `org $100` / `jmp $` → JMP to $100 ($ alone = PC)
- `pc_dollar_hex.a65`: `lda #$FF` → hex literal, NOT current-PC
- `pc_star_mul.a65`: `db 5**` → 5 × current-PC
- `pc_mod_bin.a65`: `db 10%3` → modulo; `db %1010` → binary

#### 1.8 lo/hi Keywords
- `lohi_keyword.a65`: `org $1234` / `db lo(*)` → `$34`; `db hi(*)` → `$12`
- `lohi_noparen.a65`: `org $1234` / `db lo *` → `$34`; `db hi *` → `$12`
- `lohi_angle.a65`: `org $1234` / `db <*` → `$34`; `db >*` → `$12`

#### 1.9 Increment/Decrement
- `expr_inc.a65`: `A = 5` / `db ++A` (if AS65 supports this — verify)
- `expr_dec.a65`: `A = 5` / `db --A` (if AS65 supports this — verify)

### 2. Constants (20+ tests)

#### 2.1 Basic Definitions
- `const_eq.a65`: `val = $42` / `lda #val` → `$A9,$42`
- `const_equ.a65`: `val equ $42` / `lda #val` → `$A9,$42`
- `const_set.a65`: `val set $42` / `lda #val` → `$A9,$42`
- `const_expr.a65`: `a = 1` / `b = 2` / `c equ a+b` / `lda #c` → `$A9,$03`

#### 2.2 Reassignment
- `const_reassign_eq.a65`: `v = 1` / `v = 2` / `db v` → `$02`
- `const_reassign_set.a65`: `v set 1` / `v set 2` / `db v` → `$02`
- `const_reassign_incr.a65`: `v = 0` / `v = v+1` / `v = v+1` / `db v` → `$02`

#### 2.3 Immutability Errors
- `const_equ_redef.a65`: `v equ 1` / `v equ 2` → error (expected)
- `const_label_redef.a65`: `lbl:` / `lbl = 5` → error (expected)
- `const_mnemonic.a65`: `lda equ 5` → error (expected)

#### 2.4 Forward References
- `const_forward.a65`: `lda #val` / `val equ $42` → `$A9,$42`
- `const_pc.a65`: `addr = *+1` / `lda #0` → addr = address of LDA operand

#### 2.5 Zero-Page Optimization
- `const_zp.a65`: `zp = $10` / `sta zp` → zero-page STA (2 bytes, not 3)
- `const_abs.a65`: `abs = $1000` / `sta abs` → absolute STA (3 bytes)

### 3. Conditional Assembly (20+ tests)

#### 3.1 Basic If/Endif
- `cond_true.a65`: `flag = 1` / `if flag` / `nop` / `endif` → 1 byte
- `cond_false.a65`: `flag = 0` / `if flag` / `nop` / `endif` → 0 bytes
- `cond_expr.a65`: `if 3>2` / `nop` / `endif` → 1 byte

#### 3.2 If/Else/Endif
- `cond_else_true.a65`: `flag = 1` / `if flag` / `nop` / `else` / `brk` / `endif` → NOP only
- `cond_else_false.a65`: `flag = 0` / `if flag` / `nop` / `else` / `brk` / `endif` → BRK only

#### 3.3 Nesting
- `cond_nested.a65`: `if 1` / `if 0` / `nop` / `endif` / `brk` / `endif` → BRK only
- `cond_deep.a65`: 10 levels of nested if/endif
- `cond_nested_else.a65`: nested if/else/endif with various true/false combos

#### 3.4 Symbol Suppression
- `cond_no_labels.a65`: label inside skipped block → undefined when referenced
- `cond_no_constants.a65`: constant inside skipped block → undefined when referenced

#### 3.5 Error Cases
- `cond_unmatched_endif.a65`: `endif` without `if` → error
- `cond_unclosed_if.a65`: `if 1` without `endif` → error
- `cond_double_else.a65`: two `else` for one `if` → error

#### 3.6 Complex Guards (Dormann patterns)
- `cond_dormann_guard.a65`: `data_segment = $200` / `if (data_segment & $ff) != 0` / `nop` / `endif` → 0 bytes
- `cond_dormann_I_flag.a65`: nested `if I_flag = 3` patterns from Dormann suite

### 4. Macros (30+ tests)

#### 4.1 Basic Definition and Invocation
- `macro_basic.a65`: parameterless macro → correct expansion
- `macro_one_param.a65`: `\1` substitution → correct bytes
- `macro_two_params.a65`: `\1`, `\2` substitution
- `macro_nine_params.a65`: all `\1`–`\9`

#### 4.2 Unique Labels
- `macro_unique.a65`: `\?` suffix → unique per invocation (invoke 3 times, labels don't collide)
- `macro_local.a65`: `local` declaration → unique per invocation

#### 4.3 Named Parameters
- `macro_named.a65`: `NAME macro text, value` → `text`/`value` substitution
- `macro_named_in_string.a65`: named param substituted inside `db "..."` string

#### 4.4 Argument Count
- `macro_arg_count.a65`: `\0` = number of args passed
- `macro_zero_args.a65`: `\0` = 0 when no args

#### 4.5 Nested Macros
- `macro_nested.a65`: macro calls another macro → correct expansion
- `macro_self_recursive.a65`: macro calls itself with decreasing counter (bounded by exitm)
- `macro_depth_limit.a65`: 16-level nesting → error at level 16

#### 4.6 Exitm
- `macro_exitm.a65`: `exitm` terminates expansion early
- `macro_exitm_cond.a65`: `exitm` inside `if` → conditional early termination

#### 4.7 Macro + Conditional Interaction
- `macro_with_if.a65`: `if`/`endif` inside macro body
- `macro_in_if.a65`: macro invocation inside conditional block
- `macro_def_in_if.a65`: macro defined inside conditional block (taken vs skipped)

#### 4.8 Error Cases
- `macro_mnemonic_collision.a65`: macro named `lda` → error
- `macro_missing_arg.a65`: `\2` referenced but only 1 arg → error
- `macro_exitm_outside.a65`: `exitm` not in macro → error

#### 4.9 Dormann Patterns
- `macro_dormann_trap.a65`: `trap macro` / `jmp *` / `endm` → exact Dormann trap macros
- `macro_dormann_set_a.a65`: `set_a` macro with nested `load_flag` → correct expansion
- `macro_dormann_tst_a.a65`: `tst_a` macro with `cmp_flag` and `eor_flag` → correct expansion

### 5. Labels (10+ tests)

#### 5.1 Colon-less Labels
- `label_colonless.a65`: identifier at column 0, instruction on next line
- `label_colonless_sameline.a65`: identifier at column 0 with instruction on same line
- `label_colon.a65`: traditional `name:` form → backward compatible

#### 5.2 Forward/Backward References
- `label_forward.a65`: `jmp target` / ... / `target nop`
- `label_backward.a65`: `loop nop` / ... / `jmp loop`
- `label_branch_range.a65`: branch at ±127 byte boundary

#### 5.3 Error Cases
- `label_duplicate.a65`: same label twice → error
- `label_reserved.a65`: label named `lda` → error

### 6. Directives (25+ tests)

#### 6.1 Data Directives
- `dir_db.a65`: `db $42,$43,$44` → 3 bytes
- `dir_db_string.a65`: `db "Hello",0` → 6 bytes with escape sequences
- `dir_db_escape.a65`: `db "\a\b\n\r\t\\\"",0` → correct escape bytes
- `dir_dw.a65`: `dw $1234,$5678` → little-endian words
- `dir_dd.a65`: `dd $12345678` → 4 bytes little-endian
- `dir_ds.a65`: `ds 16` → 16 zero bytes
- `dir_ds_fill.a65`: `ds 8,$FF` → 8 bytes of $FF

#### 6.2 Directive Synonyms
- `dir_fcb.a65`: `fcb 1,2,3` → same as `db` but expressions only
- `dir_fcc.a65`: `fcc "Hello"` → same as `db` but strings only
- `dir_fcw.a65`: `fcw $1234` → same as `dw`
- `dir_fdb.a65`: `fdb $1234` → same as `dw`
- `dir_rmb.a65`: `rmb 10` → same as `ds 10`

#### 6.3 Origin and Alignment
- `dir_org.a65`: `org $400` / `nop` → NOP at $400
- `dir_org_nodot.a65`: `org $400` (no dot) → same as `.org`
- `dir_align_even.a65`: `org $401` / `align` → pad to $402
- `dir_align_expr.a65`: `org $401` / `align 256` → pad to $500

#### 6.4 Segments
- `dir_segments.a65`: `code`/`data`/`bss` switching with independent PCs
- `dir_segment_org.a65`: `org` sets PC for current segment only

#### 6.5 End Directive
- `dir_end.a65`: `end` stops assembly; lines after `end` ignored
- `dir_end_start.a65`: `end start` captures entry point

#### 6.6 Miscellaneous
- `dir_noopt.a65`: `noopt` / `opt` accepted as no-ops
- `dir_title.a65`: `title "Test"` → listing header (compare listing output)
- `dir_page.a65`: `page` → listing page break (compare listing output)
- `dir_error.a65`: `ERROR message` inside taken block → error; inside skipped block → no error

### 7. Instruction Synonyms (5+ tests)

- `syn_disable.a65`: `disable` → SEI opcode ($78)
- `syn_enable.a65`: `enable` → CLI opcode ($58)
- `syn_stc.a65`: `stc` → SEC opcode ($38)
- `syn_sti.a65`: `sti` → SEI opcode ($78)
- `syn_std.a65`: `std` → SED opcode ($F8)
- `syn_nop_multi.a65`: `nop 5` → 5 NOP opcodes ($EA × 5)

### 8. Include Files (5+ tests)

- `include_basic.a65` + `include_defs.i`: basic include → constants/labels available
- `include_nested.a65` + files: A includes B, B includes C
- `include_error.a65`: error in included file → file:line in error message
- `include_missing.a65`: missing include → clear error
- `include_depth.a65`: 17 levels of nesting → error at limit

### 9. Struct (5+ tests)

- `struct_basic.a65`: `struct`/`end struct` → offset constants
- `struct_offset.a65`: `struct Name, 4` → starting at offset 4
- `struct_align.a65`: `align` inside struct → even/modulus alignment
- `struct_label.a65`: `label` inside struct → zero-size offset
- `struct_ds.a65`: `ds Name, 10` → named 10-byte member

### 10. Character Mapping (5+ tests)

- `cmap_identity.a65`: `cmap` → reset; `db "AB"` → $41,$42
- `cmap_force.a65`: `cmap -1` → all chars map to $FF
- `cmap_range.a65`: `cmap "a","ABCDEFGHIJKLMNOPQRSTUVWXYZ"` → uppercase mapping
- `cmap_digits.a65`: `cmap "0",0,1,2,3,4,5,6,7,8,9` → ASCII digits to binary

### 11. Output Formats (5+ tests)

- `output_bin.a65`: binary output → flat file with $FF fill
- `output_bin_z.a65`: binary with `-z` → flat file with $00 fill
- `output_srec.a65`: S-record output → valid S19 structure
- `output_hex.a65`: Intel HEX output → valid HEX structure
- `output_srec_entry.a65`: S-record with `end start` → S9 record has entry point

### 12. Listing Format (10+ tests)

- `list_basic.a65`: compare listing column layout exactly against AS65 reference
- `list_macro_expand.a65`: `-m` flag → macro lines prefixed with `>`
- `list_nolist.a65`: `nolist`/`list` → suppressed lines
- `list_page.a65`: `page` → form feed in listing
- `list_title.a65`: `title "Test"` → header on each page
- `list_cycles.a65`: `-c` flag → cycle counts in `[]`
- `list_symtab.a65`: `-t` flag → symbol table with `*` for set labels
- `list_pass1.a65`: `-p` flag → pass 1 listing present
- `list_width.a65`: `-w133` → 133-column listing
- `list_height.a65`: `-h40` → 40-line pages

### 13. CLI Behavior (10+ tests)

- `cli_flags_concat.a65`: `-tlfile` parsed as `-t -lfile`
- `cli_flags_numeric.a65`: `-h80t` parsed as `-h80 -t`
- `cli_auto_ext`: source `test` → finds `test.a65`
- `cli_output_ext`: `-otest` → generates `test.bin`
- `cli_nul`: `-onul` → no output file created
- `cli_define`: `-dDEBUG` → `DEBUG` defined as 1
- `cli_quiet`: `-q` → no stderr progress
- `cli_exit_0`: successful assembly → exit code 0
- `cli_exit_1`: bad flag → exit code 1
- `cli_exit_2`: missing file → exit code 2
- `cli_exit_3`: assembly error → exit code 3

### 14. Integration Tests (5+ tests)

- `dormann_default.a65`: Full Dormann suite with default config → binary match
- `dormann_no_decimal.a65`: Dormann with `disable_decimal = 1` → binary match
- `dormann_report1.a65`: Dormann with `report = 1` (requires include) → binary match
- `as65_testcase.a65`: AS65's own testcase → binary + listing match
- `dormann_cpu_run`: Load assembled binary → CPU reaches success trap

## Reference Output Strategy

**Security policy**: The AS65 binary (`as65.exe`) is closed-source and MUST NOT be downloaded or executed. All expected outputs are hand-computed from the AS65 manual (`as65.man`) and verified by inspection.

### Conformance Test Expected Outputs

Each conformance test in `testdata/conformance/` has:
- `<name>.a65` — input source (our own work, committed)
- `<name>.expected.bin` — hand-computed expected binary output (committed)
- `<name>.expected.errors` — expected error messages for negative tests (committed)

Expected outputs are derived by:
1. Reading the AS65 manual for the exact behavior of each feature
2. Hand-computing the expected opcode/operand bytes using the 6502 opcode table
3. For complex cases, cross-referencing with known-good 6502 references

### Dormann Integration Test (on-demand only)

`scripts/RunDormannTest.ps1` — Self-contained script that:
1. Downloads `6502_functional_test.a65` from the Klaus2m5 GitHub repo (GPL-3.0)
2. Downloads the pre-built reference binary from `bin_files/` (data file, safe)
3. Assembles the source with Casso65
4. Compares Casso65 binary output against the reference binary byte-for-byte
5. Deletes all downloaded files on completion
6. No GPL code or external binaries persist in the repo

### What IS committed

- `testdata/conformance/*.a65` — our own test input files (original work)
- `testdata/conformance/*.expected.bin` — hand-computed expected outputs (original work)
- `scripts/RunDormannTest.ps1`

### What is NOT committed (gitignored)

- Any downloaded Dormann source files
- Any downloaded reference binaries
- NO executables of any kind

### Listing Format Validation

The listing format is validated against the AS65 manual documentation (column layout description, examples in `as65.man`) rather than against AS65 binary output. The manual provides sufficient detail to match the format exactly.

## Test Infrastructure

Each conformance test in `UnitTest/ConformanceTests.cpp`:
1. Reads the `.a65` source from `testdata/conformance/`
2. Assembles it with Casso65
3. Compares output against the hand-computed `.expected.bin` file
4. For negative tests, verifies expected error messages match
5. Reports any byte differences with exact offset

A helper function `AssembleAndCompare(testName, flags)` handles the boilerplate.

## Estimated Test Count

| Category | Count |
|----------|-------|
| Expression evaluator | 35 |
| Constants | 20 |
| Conditional assembly | 20 |
| Macros | 30 |
| Labels | 10 |
| Directives | 25 |
| Instruction synonyms | 6 |
| Include files | 5 |
| Struct | 5 |
| Cmap | 5 |
| Output formats | 5 |
| Listing format | 10 |
| CLI behavior | 11 |
| Integration | 5 |
| **Total** | **~192** |
