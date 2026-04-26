# Assembler API Contract

**Feature**: 001-assembler | **Date**: 2026-04-23

## Public API — Casso65Core (Assembler)

### Construction

```cpp
// Create an assembler instance with the CPU's instruction set
Assembler assembler (cpu.GetInstructionSet ());

// Create with custom options
AssemblerOptions options;
options.fillByte        = 0x00;
options.generateListing = true;
options.warningMode     = WarningMode::FatalWarnings;

Assembler assembler (cpu.GetInstructionSet (), options);
```

**Preconditions**:
- `instructionSet` must be a valid 256-entry array (initialized via `Cpu::InitializeInstructionSet`)

**Postconditions**:
- Reverse opcode table is built and ready
- Instance is reusable for multiple `Assemble()` calls

---

### Assemble

```cpp
AssemblyResult result = assembler.Assemble (sourceText);
```

**Parameters**:
- `sourceText` — `const std::string&` — the full assembly source as a single string (lines separated by `\n`)

**Returns**: `AssemblyResult` struct containing all outputs

**Preconditions**:
- `sourceText` may be empty (produces empty successful result)

**Postconditions**:
- If `result.success == true`: `result.bytes` contains the flat memory image, `result.symbols` is populated, `result.errors` is empty
- If `result.success == false`: `result.bytes` is empty, `result.errors` contains at least one error with line number and message
- `result.warnings` may be populated regardless of success
- `result.listing` is populated only if `options.generateListing == true`

**Error conditions** (non-exhaustive):
- Invalid mnemonic → error with line number
- Invalid addressing mode syntax → error with line number
- Undefined label → error with line number
- Duplicate label → error with line number
- Value out of range → error with line number
- Branch target out of range → error with line number
- `.org` backward → error with line number

---

### OpcodeTable::Lookup

```cpp
OpcodeEntry entry;
bool found = opcodeTable.Lookup ("LDA", GlobalAddressingMode::Immediate, entry);
// found == true, entry.opcode == 0xA9, entry.operandSize == 1
```

**Parameters**:
- `mnemonic` — uppercase mnemonic string
- `mode` — `GlobalAddressingMode::AddressingMode` enum value
- `result` — output `OpcodeEntry`

**Returns**: `true` if the combination is valid, `false` otherwise

---

## Public API — TestCpu Integration (UnitTest)

### TestCpu::Assemble

```cpp
TestCpu cpu;
cpu.InitForTest ();
AssemblyResult result = cpu.Assemble ("LDA #$42\nSTA $10");
// result.success == true
// CPU memory at 0x8000 contains: A9 42 85 10
```

**Parameters**:
- `source` — `const char*` — assembly source text
- `startAddress` — `Word` — where to place assembled bytes in memory (default `0x8000`)

**Postconditions**:
- On success: bytes written to `memory[startAddress..]`, PC set to `startAddress`
- On failure: memory unchanged, result contains errors

### TestCpu::RunUntil

```cpp
cpu.RunUntil (0x8004);            // Run until PC reaches 0x8004
cpu.RunUntil (0x8004, 10000);     // Run with 10000-cycle limit
```

**Parameters**:
- `targetAddress` — `Word` — stop when `PC == targetAddress`
- `maxCycles` — `uint32_t` — optional cycle limit (0 = unlimited)

**Stop conditions** (in priority order):
1. `PC == targetAddress` → normal completion
2. Illegal opcode encountered → illegal opcode stop
3. `cycles >= maxCycles` (if maxCycles > 0) → timeout stop

### TestCpu::LabelAddress

```cpp
Word addr = cpu.LabelAddress (result, "loop");
```

**Parameters**:
- `result` — `const AssemblyResult&` — result from a prior `Assemble()` call
- `name` — `const char*` — label name (case-sensitive)

**Returns**: The resolved address of the label

---

## CLI Contract — Casso65 Executable

### Subcommand: `assemble`

```
Casso65 assemble <input.asm> -o <output.bin> [options]
```

| Flag | Description |
|---|---|
| `-o <file>` | Output binary file (required) |
| `-l <file>` | Output symbol table file |
| `-a` | Generate listing to stdout |
| `--fill <byte>` | Fill byte for unused addresses (default: `$FF`) |
| `-v` | Verbose output to stderr |
| `--warn` | Show warnings (default) |
| `--no-warn` | Suppress warnings |
| `--fatal-warnings` | Treat warnings as errors |

**Exit codes**: 0 = success, 1 = assembly errors, 2 = I/O or usage error

### Subcommand: `run`

```
Casso65 run <input.asm|input.bin> [options]
```

| Flag | Description |
|---|---|
| `--load <addr>` | Load address for `.bin` files (default: `$8000`) |
| `--entry <addr>` | Override entry point address |
| `--reset-vector` | Start at address in reset vector (`$FFFC/$FFFD`) |
| `--stop <addr>` | Stop when PC reaches this address |
| `--max-cycles <n>` | Maximum cycle count |
| `-v` | Verbose output |

**Exit codes**: 0 = normal completion, 1 = assembly errors, 2 = I/O or usage error, 3 = illegal opcode encountered

### Global flags

| Flag | Description |
|---|---|
| `--help` | Display usage summary |
| `--version` | Display version string |

### Assembly Source Syntax

```asm
; Full-line comment
label:              ; Label definition (identifier followed by colon)
    LDA #$42        ; Immediate: hex
    LDA #%10101010  ; Immediate: binary
    LDA #255        ; Immediate: decimal
    STA $10         ; Zero page (or absolute if address > $FF)
    JMP $1234       ; Absolute
    LDA $10,X       ; Zero page,X
    LDA ($10,X)     ; Zero page X indirect
    LDA ($10),Y     ; Zero page indirect Y
    BEQ label       ; Relative branch (to label)
    LDA #<label     ; Low byte of label address
    LDA #>label     ; High byte of label address
    LDA table+3     ; Label + offset expression
    .org $C000      ; Set origin address
    .byte $FF,$00   ; Emit raw bytes
    .word $1234     ; Emit 16-bit value (little-endian)
    .text "Hello"   ; Emit ASCII string bytes
    ROL A           ; Accumulator addressing
```

**Mnemonics**: Case-insensitive (FR-022)
**Labels**: Case-sensitive (FR-023), must not collide with mnemonics or register names (FR-024)
**Comments**: Semicolon to end-of-line (FR-010)
