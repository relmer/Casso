# Feature Specification: 6502 Assembler

**Feature Branch**: `001-assembler`  
**Created**: 2026-04-23  
**Status**: Draft  
**Input**: User description: "A two-pass 6502 assembler class that converts assembly language source text into machine code bytes"

## User Scenarios & Testing

### User Story 1 - Assemble Simple Instructions (Priority: P1)

A developer writes a short assembly snippet containing basic instructions (e.g., LDA, STA, ADC) with immediate, zero-page, and absolute addressing modes. The assembler converts this into the correct machine code bytes, and the developer can load those bytes into the emulator's memory for execution.

**Why this priority**: Without correct opcode resolution for the core instruction set, no other assembler feature is useful. This is the foundation — the assembler must correctly turn mnemonics + addressing modes into opcode bytes using the existing instruction table.

**Independent Test**: Can be fully tested by assembling a handful of instructions, comparing emitted bytes against known-good opcodes, and verifying the output length matches expectations.

**Acceptance Scenarios**:

1. **Given** assembly text `LDA #$42`, **When** assembled, **Then** the output contains the correct opcode byte for LDA immediate followed by `$42`
2. **Given** assembly text with zero-page addressing `STA $10`, **When** assembled, **Then** the output contains the correct opcode byte for STA zero-page followed by `$10`
3. **Given** assembly text with absolute addressing `JMP $1234`, **When** assembled, **Then** the output contains the correct opcode for JMP absolute followed by `$34, $12` (little-endian)
4. **Given** assembly text with accumulator addressing `ROL A`, **When** assembled, **Then** the output contains only the single-byte ROL accumulator opcode
5. **Given** assembly text with all indexed modes (e.g., `LDA $10,X`, `LDA ($10,X)`, `LDA ($10),Y`, `LDA $1234,X`, `LDA $1234,Y`), **When** assembled, **Then** each produces the correct distinct opcode for that addressing mode

---

### User Story 2 - Labels and Branching (Priority: P1)

A developer writes assembly code with named labels and branch instructions that reference those labels. The assembler resolves label addresses during a second pass and produces correct relative offsets for branch instructions and absolute addresses for JMP/JSR targets.

**Why this priority**: Labels are essential for writing any non-trivial assembly program. Without them, the developer must manually compute addresses and offsets, making the assembler impractical for real use.

**Independent Test**: Can be fully tested by assembling code with forward and backward label references, then verifying that branch offsets and jump targets resolve to the correct addresses.

**Acceptance Scenarios**:

1. **Given** assembly with a forward label reference (`BEQ target` ... `target: NOP`), **When** assembled, **Then** the branch offset correctly points to the label's address
2. **Given** assembly with a backward label reference (`loop: INX` ... `BNE loop`), **When** assembled, **Then** the branch offset correctly points back to the label
3. **Given** assembly with `JMP label` where the label is an absolute address, **When** assembled, **Then** the two-byte operand contains the label's address in little-endian
4. **Given** assembly with `JSR subroutine` referencing a labeled subroutine, **When** assembled, **Then** the operand is the subroutine's absolute address
5. **Given** a duplicate label name, **When** assembled, **Then** an error is reported identifying the duplicate and its line number
6. **Given** a branch instruction referencing an undefined label, **When** assembled, **Then** an error is reported identifying the undefined label and its line number

---

### User Story 3 - Directives for Data and Origin (Priority: P2)

A developer uses assembler directives to control where code is placed in memory and to embed raw data (bytes, 16-bit words, ASCII strings) in the output. This allows setting up data tables, string constants, and placing code at specific addresses.

**Why this priority**: Directives are needed to write realistic test programs that place code at known addresses and include lookup tables or string data. Without `.org`, all code starts at a default address, limiting test flexibility.

**Independent Test**: Can be fully tested by assembling programs that use each directive, then verifying the output bytes appear at the expected offsets and contain the correct values.

**Acceptance Scenarios**:

1. **Given** `.org $C000` followed by `NOP`, **When** assembled, **Then** the NOP opcode is placed at address `$C000` in the output
2. **Given** `.byte $FF, $00, $42`, **When** assembled, **Then** three bytes `$FF, $00, $42` are emitted in order
3. **Given** `.word $1234, $ABCD`, **When** assembled, **Then** four bytes `$34, $12, $CD, $AB` are emitted (little-endian)
4. **Given** `.text "Hello"`, **When** assembled, **Then** five bytes corresponding to the ASCII values of `H`, `e`, `l`, `l`, `o` are emitted
5. **Given** a label followed by `.byte` data, **When** another instruction references that label, **Then** the label resolves to the address where the data begins

---

### User Story 4 - Number Formats and Simple Expressions (Priority: P2)

A developer uses hexadecimal, binary, and decimal number literals in operands, and uses simple label expressions (`label+offset`, `<label` for low byte, `>label` for high byte) to compute operand values from label addresses.

**Why this priority**: Multiple number formats are standard assembler syntax; low/high byte operators are essential for loading 16-bit addresses into 8-bit registers, a very common 6502 pattern.

**Independent Test**: Can be fully tested by assembling instructions that use each number format and expression type, then verifying the emitted operand bytes are correct.

**Acceptance Scenarios**:

1. **Given** `LDA #$FF`, **When** assembled, **Then** the operand byte is `$FF`
2. **Given** `LDA #%10101010`, **When** assembled, **Then** the operand byte is `$AA`
3. **Given** `LDA #255`, **When** assembled, **Then** the operand byte is `$FF`
4. **Given** a label `data` at address `$1234` and instruction `LDA #<data`, **When** assembled, **Then** the operand byte is `$34` (low byte of address)
5. **Given** a label `data` at address `$1234` and instruction `LDA #>data`, **When** assembled, **Then** the operand byte is `$12` (high byte of address)
6. **Given** a label `table` at address `$2000` and instruction `LDA table+3`, **When** assembled, **Then** the operand address is `$2003`

---

### User Story 5 - Comments and Whitespace Handling (Priority: P3)

A developer writes assembly code with semicolon-delimited comments (full-line and inline) and varied whitespace. The assembler ignores comments and handles whitespace gracefully.

**Why this priority**: Comments and flexible formatting are basic quality-of-life features. They don't affect output but are needed for readable test programs.

**Independent Test**: Can be fully tested by assembling commented code and verifying the output is identical to the same code without comments.

**Acceptance Scenarios**:

1. **Given** a full-line comment `; this is a comment`, **When** assembled, **Then** no bytes are emitted for that line
2. **Given** an inline comment `LDA #$42 ; load value`, **When** assembled, **Then** the instruction assembles correctly and the comment is ignored
3. **Given** blank lines interspersed in the source, **When** assembled, **Then** blank lines are ignored and surrounding instructions assemble correctly
4. **Given** varying indentation and spacing around operands, **When** assembled, **Then** the instructions assemble correctly

---

### User Story 6 - Assemble-and-Run Integration in Tests (Priority: P1)

A test developer uses a convenience method on TestCpu to assemble source text directly into emulator memory, execute it, and verify the CPU state afterward. This eliminates the need to hand-code raw opcode bytes in test cases.

**Why this priority**: This is the primary consumer of the assembler — making unit tests dramatically more readable and maintainable. Without this integration, the assembler provides value only in isolation.

**Independent Test**: Can be fully tested by writing a test that assembles a short program, runs it, and asserts register values, memory contents, and flag states.

**Acceptance Scenarios**:

1. **Given** assembly source text and a TestCpu instance, **When** `Assemble(source)` is called, **Then** the assembled bytes are written into the CPU's memory starting at the current PC
2. **Given** a successfully assembled program, **When** `RunUntil(address)` is called, **Then** the CPU executes instructions until PC reaches the target address
3. **Given** `RunUntil(address)` is called, **When** the CPU encounters an illegal opcode before reaching the target, **Then** execution stops and the illegal opcode condition is indicated
4. **Given** `RunUntil(address)` is called with an optional maximum cycle count, **When** the CPU exceeds that cycle count without reaching the target, **Then** execution stops and a timeout indication is available
5. **Given** a successfully assembled program with labels, **When** `LabelAddress(name)` is called, **Then** the address of the named label is returned
6. **Given** assembly source with errors, **When** `Assemble(source)` is called, **Then** the errors are accessible and no partial code is written to memory

---

### User Story 7 - Error Reporting (Priority: P2)

When assembly source contains errors (invalid mnemonics, bad addressing mode syntax, out-of-range values, undefined labels, duplicate labels), the assembler collects all errors with line numbers and makes them available to the caller rather than stopping at the first error.

**Why this priority**: Good error reporting is critical for usability. Collecting all errors (rather than halting on the first) lets the developer fix multiple issues in one pass.

**Independent Test**: Can be fully tested by feeding intentionally malformed assembly text and verifying each error message includes the correct line number and a meaningful description.

**Acceptance Scenarios**:

1. **Given** an invalid mnemonic `XYZ`, **When** assembled, **Then** an error is reported with the line number and an indication that the mnemonic is unrecognized
2. **Given** `LDA` with no operand, **When** assembled, **Then** an error is reported indicating a missing operand
3. **Given** `BEQ undefinedLabel`, **When** assembled, **Then** an error is reported identifying the undefined label
4. **Given** a branch instruction whose target is more than 127 bytes away, **When** assembled, **Then** an error is reported indicating the branch target is out of range
5. **Given** source with multiple errors on different lines, **When** assembled, **Then** all errors are collected (not just the first) and each includes its respective line number
6. **Given** `LDA #$1FF` (value exceeds byte range for immediate mode), **When** assembled, **Then** an error is reported indicating the value is out of range
7. **Given** a parse error on an instruction line during Pass 1, **When** Pass 2 runs, **Then** labels defined after the error line have addresses that are close to correct (best-effort size estimation keeps the PC advancing reasonably)

---

### Edge Cases

- What happens when source text is empty? The assembler succeeds with zero output bytes and no errors.
- What happens when `.org` sets an address lower than the current output position? The assembler reports an error (cannot move origin backward within a contiguous block).
- What happens when a label name collides with a mnemonic (e.g., a label named `LDA`)? The assembler reports an error — label names must not be reserved mnemonics.
- What happens when a branch offset is exactly at the boundary (±127/128 bytes)? Offsets of -128 to +127 are valid; ±128 on the positive side is out of range.
- What happens when `.text` contains an empty string (`""`)? Zero bytes are emitted; this is not an error.
- What happens when the CPU executes a BRK instruction during `run`? BRK is a software interrupt — it pushes PC+2 and the status register to the stack, then loads PC from the IRQ vector at $FFFE/$FFFF. If the vector points to uninitialized memory ($FF fill), the CPU will encounter an illegal opcode and stop.
- What happens when addressing mode syntax is ambiguous (e.g., `STA $10` could be zero-page or absolute)? The assembler prefers the shorter encoding (zero-page) when the address fits in one byte.

---

### User Story 8 - Command-Line Interface (Priority: P1)

A developer uses the My6502 executable from the command line to assemble source files to binary output and/or run programs directly. The CLI uses subcommands modeled after ACME (the popular 6502 assembler).

**Why this priority**: The CLI is the primary non-test entry point for the assembler. Without it, assembly is only available programmatically.

**Independent Test**: Can be verified by running the executable with various argument combinations and checking exit codes, output files, and stderr messages.

**Acceptance Scenarios**:

1. **Given** `My6502 assemble input.asm -o output.bin`, **When** run, **Then** the input file is assembled and the binary output is written to the specified file
2. **Given** `My6502 assemble input.asm -o output.bin -l labels.txt`, **When** run, **Then** a symbol table file is also written with label names and addresses
3. **Given** `My6502 run input.asm`, **When** run, **Then** the file is assembled and executed immediately
4. **Given** `My6502 run output.bin --load $8000`, **When** run, **Then** the binary is loaded at the specified address and executed
5. **Given** assembly errors in the input file, **When** `My6502 assemble` is run, **Then** errors are printed to stderr with line numbers and the process exits with a non-zero code
6. **Given** `My6502 assemble input.asm -o output.bin --fill $00`, **When** run, **Then** the output binary uses `$00` instead of the default `$FF` as the fill byte for unused addresses
7. **Given** `My6502` with no arguments, **When** run, **Then** a usage summary is displayed
8. **Given** `My6502 --version`, **When** run, **Then** the version string is displayed
9. **Given** `My6502 run input.asm --stop $FFFC`, **When** run, **Then** execution stops when the PC reaches `$FFFC` instead of relying solely on illegal opcode detection
10. **Given** `My6502 run input.asm --max-cycles 10000`, **When** run, **Then** execution stops after 10000 cycles if no other stop condition is met first
11. **Given** `My6502 run input.asm` with no entry-point flags, **When** run, **Then** execution begins at the lowest assembled address (or `$8000` if no `.org`)
12. **Given** `My6502 run input.asm --entry $C000`, **When** run, **Then** execution begins at `$C000` regardless of the assembled origin
13. **Given** `My6502 run input.asm --reset-vector`, **When** run, **Then** execution begins at the address stored in `$FFFC/$FFFD` (the 6502 reset vector)

---

### User Story 9 - Listing File Output (Priority: P1)

A developer requests a listing file that shows each source line alongside its assembled address and machine code bytes. This is essential for debugging assembly programs and verifying the assembler's output.

**Why this priority**: Listing files are a critical debugging tool — they show exactly what bytes were generated at what addresses, making it easy to spot encoding errors or incorrect label resolution.

**Independent Test**: Can be tested by assembling a program with listing enabled and verifying the listing contains the correct address, bytes, and source text for each line.

**Acceptance Scenarios**:

1. **Given** the `-a` flag on the CLI (or a listing option in the API), **When** a program is assembled, **Then** a listing is produced showing address, hex bytes, and original source per line
2. **Given** a line with an instruction, **Then** the listing shows the address (e.g., `$8000`), the hex bytes (e.g., `A9 42`), and the source (`LDA #$42`)
3. **Given** a comment-only or blank line, **Then** the listing shows the source text with no address or bytes
4. **Given** a `.byte` directive, **Then** the listing shows the address and the emitted data bytes
5. **Given** a label-only line, **Then** the listing shows the address the label resolves to
6. **Given** a `.org` directive, **Then** the listing shows the new origin address

---

### User Story 10 - Verbose Output and Warning Control (Priority: P2)

A developer uses verbose mode to see detailed assembly progress and controls whether warnings are displayed, elevated to errors, or suppressed.

**Why this priority**: Verbose output aids debugging during development. Warning control lets the developer decide how strictly to treat edge cases (e.g., unused labels, redundant `.org`).

**Independent Test**: Can be tested by assembling with different verbosity/warning flags and verifying the expected messages appear or are suppressed.

**Acceptance Scenarios**:

1. **Given** the `-v` (verbose) flag, **When** assembling, **Then** pass progress, symbol resolution details, and byte counts are printed to stderr
2. **Given** no `-v` flag, **When** assembling, **Then** only errors and warnings are printed (quiet by default)
3. **Given** `--warn` (default), **When** a warning condition occurs, **Then** the warning is printed to stderr but assembly succeeds
4. **Given** `--fatal-warnings`, **When** a warning condition occurs, **Then** it is treated as an error and assembly fails
5. **Given** `--no-warn`, **When** a warning condition occurs, **Then** the warning is suppressed

## Requirements

### Functional Requirements

- **FR-001**: The assembler MUST accept a string of assembly source text via an instance method and produce an assembly result containing machine code bytes, a symbol table, errors, warnings, and listing data
- **FR-002**: The assembler MUST recognize all 56 standard 6502 mnemonics with all valid addressing modes
- **FR-003**: The assembler MUST resolve opcodes by looking up the existing instruction table (mnemonic + addressing mode → opcode byte), not by maintaining a separate encoding table
- **FR-004**: The assembler MUST perform two passes — the first to determine label addresses, the second to emit final bytes with resolved references
- **FR-005**: The assembler MUST support labels defined by an identifier followed by a colon (e.g., `loop:`)
- **FR-006**: The assembler MUST support the `.org` directive to set the current assembly address
- **FR-007**: The assembler MUST support the `.byte` directive to emit one or more raw byte values
- **FR-008**: The assembler MUST support the `.word` directive to emit one or more 16-bit values in little-endian byte order
- **FR-009**: The assembler MUST support the `.text` directive to emit ASCII character bytes from a quoted string
- **FR-010**: The assembler MUST support semicolon-delimited comments, both full-line and inline
- **FR-011**: The assembler MUST support hexadecimal (`$FF`), binary (`%10101010`), and decimal (`255`) number literals
- **FR-012**: The assembler MUST support `label+offset` expressions to compute an address relative to a label
- **FR-013**: The assembler MUST support `<label` (low byte) and `>label` (high byte) operators to extract 8-bit components of a 16-bit label address
- **FR-014**: The assembler MUST collect all errors with line numbers rather than stopping at the first error
- **FR-015**: The assembler MUST report errors for: invalid mnemonics, invalid addressing mode syntax, undefined labels, duplicate labels, out-of-range values, and out-of-range branch offsets
- **FR-016**: The assembler MUST prefer shorter encodings when ambiguous (e.g., zero-page over absolute when the address fits in one byte)
- **FR-017**: The assembler MUST NOT depend on the CPU class — it produces bytes and a symbol table only
- **FR-018**: A test integration method MUST assemble source text and write the output into emulator memory at the current PC
- **FR-019**: A test integration method MUST execute instructions until the PC reaches a specified target address
- **FR-020**: The run-until method MUST stop execution when the CPU encounters an illegal opcode (the default stop condition)
- **FR-021a**: The run-until method MUST accept an optional maximum cycle count; when specified, execution stops after that many cycles if no other stop condition fires first. There is no default cycle limit — omitting the parameter means unlimited cycles
- **FR-021b**: The CLI MUST support `--stop $ADDR` to set an explicit stop address that halts execution when PC reaches it
- **FR-021c**: The CLI MUST support `--max-cycles N` to set an explicit cycle limit for the `run` subcommand
- **FR-021d**: BRK MUST be implemented as a software interrupt (push PC+2 and status register to stack, load PC from IRQ vector at $FFFE/$FFFF), NOT as a stop condition
- **FR-021e**: The default entry point for execution MUST be the lowest assembled address (or `$8000` if no `.org` directive is present)
- **FR-021f**: The CLI MUST support `--entry $ADDR` to override the default entry point with an explicit address
- **FR-021g**: The CLI MUST support `--reset-vector` to start execution at the address stored in the 6502 reset vector (`$FFFC/$FFFD`)
- **FR-022a**: A test integration method MUST allow querying the symbol table for a label's resolved address
- **FR-022**: Mnemonics and register names MUST be case-insensitive (e.g., `lda`, `LDA`, and `Lda` are all accepted)
- **FR-023**: Label names MUST be case-sensitive
- **FR-024**: Label names MUST NOT collide with reserved mnemonics or register names
- **FR-025**: The CLI MUST support an `assemble` subcommand that reads an input `.asm` file and writes a flat binary output file (`-o`)
- **FR-026**: The CLI `assemble` subcommand MUST support a `-l` flag to write a symbol table file listing label names and their resolved addresses
- **FR-027**: The CLI MUST support a `run` subcommand that assembles a `.asm` file and executes it immediately, or loads a `.bin` file at a specified address (`--load`) and executes it
- **FR-028**: The CLI MUST print errors to stderr with line numbers and exit with a non-zero code on failure
- **FR-029**: The CLI MUST display a usage summary when run with no arguments or `--help`
- **FR-030**: The assembler MUST support generating a listing file showing address, hex bytes, and original source text for each line
- **FR-031**: The listing MUST be available via a `-a` CLI flag and via an API option for programmatic use
- **FR-032**: The CLI MUST support a `-v` (verbose) flag that prints pass progress, symbol resolution details, and byte counts to stderr
- **FR-033**: The CLI MUST support `--warn` (default), `--no-warn`, and `--fatal-warnings` flags to control warning behavior
- **FR-034**: The CLI MUST support `--version` to display the version string
- **FR-035**: The assembled output MUST be a flat (contiguous) memory image covering from the lowest to highest address used, with unused bytes set to a configurable fill value
- **FR-036**: The default fill byte MUST be `$FF` (EEPROM erased state)
- **FR-037**: The CLI MUST support a `--fill N` flag to override the default fill byte
- **FR-038**: During Pass 1, when a line cannot be fully parsed, the assembler MUST use best-effort size estimation to advance the PC by a reasonable amount (based on partial mnemonic recognition), minimizing cascading label offset errors in Pass 2
- **FR-039**: The Assembler MUST be an instance-based class — configuration (instruction set reference, fill byte, options) is provided at construction time
- **FR-040**: The `Assemble()` method MUST return a result struct containing the assembled bytes (flat memory image), symbol table, error list, warning list, and listing data
- **FR-041**: An Assembler instance MUST be reusable across multiple `Assemble()` calls without re-creating the object

### Key Entities

- **Source Text**: The input string containing assembly language code, processed line by line
- **Symbol Table**: A mapping of label names to their resolved 16-bit addresses, built during the first pass and consumed during the second
- **Assembled Output**: A flat memory image (up to 64 KB) covering from the lowest to highest address used, with unused locations filled by a configurable fill byte (default `$FF`, matching EEPROM erased state); designed for EEPROM programmer compatibility
- **Error List**: A collection of error entries, each containing a line number and descriptive message
- **Assembler**: An instance-based class that holds configuration (instruction set reference, fill byte, listing/warning options) set at construction; exposes `Assemble(sourceText)` for repeated use without rebuilding config
- **Assembly Result**: A struct returned by `Assemble()` containing the assembled bytes (flat memory image), symbol table, error list, warning list, and listing data
- **Instruction Table**: The existing 256-entry opcode lookup table, used in reverse to map (mnemonic + addressing mode) to opcode byte

## Success Criteria

### Measurable Outcomes

- **SC-001**: All 56 standard 6502 mnemonics with every valid addressing mode combination assemble to the correct opcode byte
- **SC-002**: Programs with up to 100 labels assemble correctly with all references resolved
- **SC-003**: A test developer can write an assemble-and-run test in under 5 lines of test code (assemble, run, assert)
- **SC-004**: Assembly errors include the correct line number in 100% of error cases
- **SC-005**: Assembled programs execute identically to the same programs loaded as raw bytes via `WriteBytes()`
- **SC-006**: The assembler handles all edge cases (empty input, boundary branch offsets, mixed number formats) without crashes or undefined behavior
- **SC-007**: The CLI `assemble` subcommand produces byte-identical output to programmatic `Assembler::Assemble()` for the same input
- **SC-008**: Listing output correctly shows address, hex bytes, and source text for every line type (instructions, directives, labels, comments)

## Scope Boundary

### Out of Scope

- Macros and macro expansion
- Conditional assembly directives (`#if`, `.ifdef`, `.ifndef`)
- Relocatable output or linking multiple object files
- Include files or file I/O in the assembler core — the assembler takes a string; file reading is done by the CLI layer
- Illegal/undocumented 6502 opcodes
- Command-line symbol definition (`-Dlabel=value`) — future enhancement
- CPU variant selection (`--cpu 65c02`) — future enhancement when 65C02 extensions are added to the emulator

## Assumptions

- The existing 256-entry instruction table is complete and correct for all legal 6502 opcodes
- Mnemonic names in the instruction table are consistent uppercase strings (e.g., `"LDA"`, `"STA"`)
- The assembler will be used primarily in test code and the interactive console, not for assembling large programs (performance is not a primary concern)
- The default origin address when no `.org` directive is given is the TestCpu default start PC (`$8000`)
- The default execution entry point is the lowest assembled address; for test programs without `.org`, this is `$8000`
- Assembly source uses standard 6502 syntax conventions (operand follows mnemonic, separated by whitespace)
- The `label+offset` expression supports only addition of a positive integer constant; subtraction and complex arithmetic are not needed

## Clarifications

### Session 2026-04-23

- Q: What is the assembled output model (flat image vs. sparse/segmented)? → A: Flat memory image (up to 64 KB) with configurable fill byte defaulting to `$FF` (EEPROM erased state). Output covers lowest to highest address used. `--fill N` CLI flag allows overriding the fill byte. No sparse/segmented model needed — 64 KB max makes flat blob practical. Important for EEPROM programmer compatibility.
- Q: How should the assembler handle parse errors in Pass 1 (label address computation)? → A: Best-effort size estimation — when a line cannot be fully parsed, the assembler estimates the instruction size (e.g., 1–3 bytes based on partial mnemonic recognition) so the PC advances by a reasonable amount. This minimizes cascading label offset errors in Pass 2.
- Q: What should cause execution to stop when running assembled programs? → A: Stop on illegal opcode (current Run() behavior) with optional `--stop $ADDR` CLI override. Support `--max-cycles` but only when explicitly specified (no default limit). BRK is NOT a stop signal — it is a software interrupt that must be implemented correctly (push PC+2 and status, load PC from IRQ vector $FFFE/$FFFF). The `$FF` fill byte naturally halts execution since uninitialized memory is an illegal opcode.
- Q: Where does execution begin when running an assembled program? → A: Default entry point is the first instruction address (lowest assembled address, or `$8000` if no `.org`). Override with `--entry $ADDR` or `--reset-vector` to use `$FFFC/$FFFD`. This gives a simple default for test programs while supporting real 6502 reset-vector conventions when needed.
- Q: What is the assembler's public API shape (static function vs. instance-based)? → A: Instance-based API. Assembler object holds configuration (instruction set reference, fill byte, options) set at construction. `Assemble()` is called on the instance and returns a result struct with bytes, symbol table, errors, warnings, and listing. Allows reuse across multiple assemblies without rebuilding config.
