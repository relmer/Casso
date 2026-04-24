# Research: 6502 Assembler

**Feature**: 001-assembler | **Date**: 2026-04-23

## R1: Reverse Opcode Lookup Strategy

**Decision**: Build a `std::unordered_map<string, std::unordered_map<GlobalAddressingMode::AddressingMode, Byte>>` at Assembler construction time by iterating over the existing `instructionSet[256]` array.

**Rationale**: The emulator already has a complete 256-entry `Microcode instructionSet[]` that maps opcode byte → (name, addressing mode, operation). The assembler needs the reverse: (name, addressing mode) → opcode byte. Building from the existing table guarantees consistency — if the emulator supports an instruction, the assembler will too, with zero risk of table drift.

**Alternatives considered**:
- **Separate static table**: Hardcode a second table of (mnemonic, mode) → opcode. Rejected because it duplicates data and risks drift when instructions are added/modified.
- **Compute from Instruction bitfield encoding**: Use the Group/Opcode/AddressingMode bit layout to compute opcodes. Rejected because the Misc group doesn't follow the regular encoding, making this approach incomplete.

**Implementation detail**: The key string is the uppercase mnemonic (e.g., `"LDA"`). The inner map key is the `GlobalAddressingMode::AddressingMode` enum. Mnemonic names are already stored as `const char*` in the `Microcode` entries. Construction iterates `instructionSet[0..255]`, skips entries where `isLegal == false`, and populates the map.

## R2: Two-Pass Architecture

**Decision**: Standard two-pass assembler design.

**Rationale**: This is the established approach for 6502 assemblers. Pass 1 parses all lines, builds the symbol table with label addresses, and advances the PC by the instruction size. Pass 2 re-parses and emits final bytes with all label references resolved.

**Alternatives considered**:
- **Single-pass with backpatching**: Track unresolved forward references and patch them after the pass. Rejected because it adds complexity (managing a backpatch list) and doesn't handle the case where a forward reference determines the instruction size (zero-page vs. absolute).
- **Three-pass**: Add a pre-pass for directives. Rejected — unnecessary complexity; `.org` and `.byte` work fine in two passes.

**Key detail**: On Pass 1, when a line has a parse error, the assembler uses best-effort size estimation (e.g., assume 2 bytes for unknown mnemonics, 1 byte for implied) to advance the PC. This minimizes cascading label offset errors.

## R3: Addressing Mode Detection from Operand Syntax

**Decision**: Regex-free pattern matching using character inspection.

**Rationale**: 6502 operand syntax has a small, well-defined grammar. Each addressing mode has a unique syntactic signature:

| Syntax Pattern | Addressing Mode |
|---|---|
| (none) | Implied / SingleByteNoOperand |
| `A` | Accumulator |
| `#value` | Immediate |
| `$ZZ` (1 byte value) | ZeroPage |
| `$HHHH` (2 byte value) | Absolute |
| `$ZZ,X` | ZeroPageX |
| `$ZZ,Y` | ZeroPageY |
| `$HHHH,X` | AbsoluteX |
| `$HHHH,Y` | AbsoluteY |
| `($ZZ,X)` | ZeroPageXIndirect |
| `($ZZ),Y` | ZeroPageIndirectY |
| `($HHHH)` | JumpIndirect |
| `label` (branch mnemonic) | Relative |

The parser first strips comments, splits into label/mnemonic/operand, then classifies the operand by checking for `#`, `(`, `,X`, `,Y`, `)` markers and value range.

**Alternatives considered**:
- **Regex-based parsing**: Use `std::regex` for pattern matching. Rejected due to performance overhead and complexity of regex patterns for this grammar. Simple character checks are faster and more readable.
- **Full tokenizer/lexer**: Build a formal lexer with token types. Rejected as over-engineering for this grammar size.

**Key detail**: When the operand value fits in a byte and the mnemonic supports both zero-page and absolute modes, prefer zero-page (FR-016). When the operand is a label (forward reference on Pass 1), assume absolute addressing (2-byte operand) to be safe, then on Pass 2 if the label resolves to zero-page, downgrade if the mnemonic supports it.

## R4: Label Handling and Forward References

**Decision**: Pass 1 records label addresses in a `std::unordered_map<std::string, Word>`. Pass 2 resolves all references.

**Rationale**: Simple and correct. Labels are case-sensitive (per FR-023). Duplicate detection happens immediately on Pass 1.

**Key details**:
- Label names are validated: must start with letter or underscore, contain only alphanumerics and underscores
- Label names must not collide with mnemonics (checked against the reverse opcode table keys) or register names (`A`, `X`, `Y`, `S`)
- Forward references on Pass 1: PC advances by the assumed instruction size (typically 3 bytes for absolute addressing). On Pass 2, the actual label value is known.

## R5: CLI Architecture

**Decision**: Simple subcommand dispatch in `main()` using hand-rolled argument parsing. No argument parsing library.

**Rationale**: The CLI has only two subcommands (`assemble`, `run`) with a small number of flags. A hand-rolled parser is simpler than adding a dependency. The My6502 project currently has no CLI infrastructure at all.

**Alternatives considered**:
- **Argument parsing library (CLI11, cxxopts)**: Rejected per constitution principle V (no external dependencies).
- **Windows-style `/flag` syntax**: Rejected — the spec defines Unix-style `--flag` and `-f` syntax, which is more standard for cross-platform tools and matches ACME conventions.

**Architecture**:
- `CommandLine` class in My6502 project parses `argc/argv` into a typed options struct
- `main()` dispatches to `DoAssemble()` or `DoRun()` based on subcommand
- Assembler core is instantiated with options, fed source text, result examined
- File I/O (reading `.asm`, writing `.bin`, `.lst`, `.sym`) happens only in the CLI layer

## R6: Listing File Format

**Decision**: Standard assembler listing format with columns for address, hex bytes, and source text.

**Rationale**: This is the conventional format used by ACME, ca65, and other 6502 assemblers. Developers familiar with any assembler will recognize it.

**Format**:
```
$8000  A9 42     LDA #$42
$8002  85 10     STA $10
$8004             ; comment line
$8004  EA        NOP
```

- Address column: 4-digit hex, prefixed with `$`
- Bytes column: up to 3 hex bytes (most instructions), more for `.byte` data
- Source column: original source text as written
- Comment-only and blank lines show no address/bytes
- Label-only lines show the address the label resolves to

## R7: Error Collection Strategy

**Decision**: Errors are collected in a `std::vector<AssemblyError>` where each entry has a line number (1-based) and a message string. Assembly continues after each error.

**Rationale**: FR-014 requires collecting all errors rather than stopping at the first. This is standard practice for assemblers and compilers.

**Key details**:
- Parse errors on Pass 1: record error, use best-effort size estimation to advance PC
- Reference errors on Pass 2: record error, emit zero bytes as placeholder
- Duplicate label errors: detected on Pass 1
- Undefined label errors: detected on Pass 2
- Out-of-range branch: detected on Pass 2 when the offset is computed
- Invalid mnemonic: detected on Pass 1
- Value out of range: detected on Pass 2

## R8: TestCpu Integration Design

**Decision**: Add `Assemble()`, `RunUntil()`, and `LabelAddress()` methods directly to the `TestCpu` class in `TestHelpers.h`.

**Rationale**: `TestCpu` already extends `Cpu` to expose protected members. Adding assembly integration here keeps test infrastructure in one place and avoids modifying production code. The `Assembler` class is in My6502Core (which UnitTest already links), so `TestCpu` can instantiate it directly.

**Methods**:
- `AssemblyResult Assemble(const char* source, Word startAddress = 0x8000)` — assembles source, writes bytes to `memory[]`, returns result
- `void RunUntil(Word targetAddress, uint32_t maxCycles = 0)` — executes instructions until PC == target, illegal opcode, or cycle limit
- `Word LabelAddress(const AssemblyResult& result, const char* name)` — looks up a label's address from the result's symbol table
