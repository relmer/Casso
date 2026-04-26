# Data Model: 6502 Assembler

**Feature**: 001-assembler | **Date**: 2026-04-23

## Entities

### AssemblyError

A single error or warning encountered during assembly.

| Field | Type | Description |
|---|---|---|
| lineNumber | `int` | 1-based line number in the source text |
| message | `std::string` | Human-readable error description |

**Validation**: `lineNumber >= 1`. `message` is never empty.

### AssemblyLine

One line of listing output, correlating source text to emitted bytes.

| Field | Type | Description |
|---|---|---|
| lineNumber | `int` | 1-based source line number |
| address | `Word` | Address where bytes are emitted (valid only if `byteCount > 0` or line is a label) |
| bytes | `std::vector<Byte>` | Emitted machine code / data bytes for this line |
| sourceText | `std::string` | Original source text (for listing display) |
| hasAddress | `bool` | Whether `address` is meaningful (false for blank/comment-only lines) |

### AssemblyResult

The complete output of an `Assemble()` call.

| Field | Type | Description |
|---|---|---|
| success | `bool` | `true` if no errors were recorded |
| bytes | `std::vector<Byte>` | Flat memory image (lowest-to-highest address used), fill-byte padded |
| startAddress | `Word` | Address of first byte in the image (lowest `.org` or default) |
| endAddress | `Word` | Address past the last byte in the image |
| symbols | `std::unordered_map<std::string, Word>` | Label name → resolved address |
| errors | `std::vector<AssemblyError>` | All errors encountered (empty on success) |
| warnings | `std::vector<AssemblyError>` | All warnings encountered |
| listing | `std::vector<AssemblyLine>` | Per-line listing data (populated when listing is enabled) |

**Validation**: If `success == false`, `bytes` is empty. `startAddress <= endAddress`. `symbols` keys are non-empty, case-sensitive strings.

### AssemblerOptions

Configuration for an `Assembler` instance.

| Field | Type | Default | Description |
|---|---|---|---|
| fillByte | `Byte` | `0xFF` | Fill value for unused addresses in the output image |
| generateListing | `bool` | `false` | Whether to populate `AssemblyResult::listing` |
| warningMode | `WarningMode` | `Warn` | How to handle warnings (`Warn`, `NoWarn`, `FatalWarnings`) |

### WarningMode (enum)

| Value | Description |
|---|---|
| `Warn` | Display warnings but continue assembly (default) |
| `NoWarn` | Suppress all warnings |
| `FatalWarnings` | Treat warnings as errors |

### OpcodeEntry

An entry in the reverse opcode lookup table.

| Field | Type | Description |
|---|---|---|
| opcode | `Byte` | The opcode byte value (0x00–0xFF) |
| operandSize | `Byte` | Number of operand bytes (0, 1, or 2) |

### Assembler (class)

The main assembler engine. Instance-based for reuse (FR-039, FR-041).

| Member | Type | Description |
|---|---|---|
| m_opcodeTable | `OpcodeTable` | Reverse lookup: (mnemonic, addressing mode) → OpcodeEntry |
| m_options | `AssemblerOptions` | Configuration set at construction |

**Key methods**:
- Constructor: `Assembler(const Microcode instructionSet[256], AssemblerOptions options = {})`
- `AssemblyResult Assemble(const std::string& sourceText)`

**State transitions**: The `Assembler` is stateless between `Assemble()` calls. Each call creates fresh internal state (PC, symbol table, error list) and returns everything in `AssemblyResult`. The `m_opcodeTable` is built once at construction and reused.

### OpcodeTable (class)

Reverse lookup from (mnemonic, addressing mode) to opcode byte.

| Member | Type | Description |
|---|---|---|
| m_table | `std::unordered_map<std::string, std::unordered_map<int, OpcodeEntry>>` | mnemonic → (addressingMode → OpcodeEntry) |

**Key methods**:
- Constructor: `OpcodeTable(const Microcode instructionSet[256])` — iterates all 256 entries, records legal ones
- `bool Lookup(const std::string& mnemonic, GlobalAddressingMode::AddressingMode mode, OpcodeEntry& result) const`
- `bool IsMnemonic(const std::string& name) const` — for label validation
- `bool HasMode(const std::string& mnemonic, GlobalAddressingMode::AddressingMode mode) const`

### CommandLineOptions

CLI argument parsing result (Casso65 project only).

| Field | Type | Default | Description |
|---|---|---|---|
| subcommand | `Subcommand` | `None` | `Assemble`, `Run`, `None` |
| inputFile | `std::string` | `""` | Path to `.asm` or `.bin` input file |
| outputFile | `std::string` | `""` | Path to binary output file (`-o`) |
| symbolFile | `std::string` | `""` | Path to symbol table file (`-l`) |
| fillByte | `Byte` | `0xFF` | Fill byte override (`--fill`) |
| loadAddress | `Word` | `0x8000` | Address to load a `.bin` file (`--load`) |
| stopAddress | `Word` | `0` | Explicit stop address (`--stop`, 0 = not set) |
| entryAddress | `Word` | `0` | Explicit entry point (`--entry`, 0 = use default) |
| maxCycles | `uint32_t` | `0` | Cycle limit for `run` (`--max-cycles`, 0 = unlimited) |
| useResetVector | `bool` | `false` | Start from reset vector (`--reset-vector`) |
| verbose | `bool` | `false` | Verbose output (`-v`) |
| generateListing | `bool` | `false` | Generate listing file (`-a`) |
| warningMode | `WarningMode` | `Warn` | Warning behavior (`--warn/--no-warn/--fatal-warnings`) |
| showVersion | `bool` | `false` | Display version (`--version`) |
| showHelp | `bool` | `false` | Display usage (`--help`) |

## Relationships

```
Assembler
  ├── owns → OpcodeTable (built at construction from instructionSet[256])
  ├── owns → AssemblerOptions (configuration)
  └── produces → AssemblyResult (per Assemble() call)
                   ├── contains → vector<Byte> (flat memory image)
                   ├── contains → unordered_map<string, Word> (symbol table)
                   ├── contains → vector<AssemblyError> (errors)
                   ├── contains → vector<AssemblyError> (warnings)
                   └── contains → vector<AssemblyLine> (listing)

TestCpu (extends Cpu)
  ├── uses → Assembler (to assemble source text)
  └── uses → AssemblyResult (to write bytes into CPU memory, query labels)

CommandLineOptions
  └── drives → main() dispatch to DoAssemble() or DoRun()
```

## Key Design Constraints

1. **No dependency on Cpu**: The `Assembler` class takes a `const Microcode[]` reference at construction, not a `Cpu` reference. It never reads/writes CPU registers or memory (FR-017).

2. **Existing instruction table is the source of truth**: The `OpcodeTable` is built by iterating `instructionSet[256]` — no hardcoded opcode values in the assembler.

3. **Instance-based, reusable**: Configuration is set once at construction. `Assemble()` is stateless between calls — all per-assembly state is local to the call and returned in `AssemblyResult` (FR-039, FR-041).

4. **Flat memory image output**: The `bytes` vector covers `[startAddress, endAddress)` with unused gaps filled by `fillByte`. The vector index `i` corresponds to address `startAddress + i` (FR-035, FR-036).
