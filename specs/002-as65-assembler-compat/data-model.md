# Data Model: Full AS65 Assembler Clone

**Feature**: 002-as65-assembler-compat  
**Date**: 2026-04-25

## Entities

### Symbol

A named value in the symbol table. Three kinds:

| Field | Type | Description |
|-------|------|-------------|
| name | string | Identifier (case-sensitive for labels/constants) |
| value | int32_t | Resolved numeric value |
| kind | enum | `Label`, `Equ`, `Set` |
| lineNumber | int | Source line where first defined |
| isResolved | bool | True if value is finalized (pass 2) |
| fileName | string | Source file where defined (for includes) |

**Rules**:
- `Label`: immutable, set to PC at point of definition
- `Equ`: immutable, expression resolved in pass 2
- `Set` (= or set): reassignable, expression resolved eagerly

### MacroDefinition

| Field | Type | Description |
|-------|------|-------------|
| name | string | Macro identifier |
| paramNames | vector\<string\> | Named parameters (optional) |
| body | vector\<string\> | Raw source lines between `macro` and `endm` |
| localLabels | vector\<string\> | Labels declared with `local` |
| lineNumber | int | Source line of `macro` keyword |

### ConditionalState

Stack element for `if`/`else`/`endif` tracking.

| Field | Type | Description |
|-------|------|-------------|
| assembling | bool | True if current block is being assembled |
| seenElse | bool | True if `else` has been encountered for this level |
| parentAssembling | bool | True if enclosing block is assembling |

### Segment

One of three memory regions with independent location counters.

| Field | Type | Description |
|-------|------|-------------|
| name | enum | `Code`, `Data`, `Bss` |
| pc | Word | Current program counter for this segment |
| output | vector\<Byte\> | Emitted bytes (empty for Bss) |
| startAddress | Word | Lowest address used |
| endAddress | Word | Highest address + 1 |

### StructDefinition

Compile-time layout definition.

| Field | Type | Description |
|-------|------|-------------|
| name | string | Structure name (set to total size on `end struct`) |
| startOffset | int32_t | Starting offset (default 0) |
| currentOffset | int32_t | Current offset during definition |
| members | vector\<StructMember\> | Member definitions |

### StructMember

| Field | Type | Description |
|-------|------|-------------|
| name | string | Member name |
| offset | int32_t | Byte offset within structure |
| size | int32_t | Size in bytes (0 for `label`) |

### CharacterMap

| Field | Type | Description |
|-------|------|-------------|
| table | Byte[256] | Mapping from input character to output byte |

**Rules**: Initialized to identity mapping. `cmap` modifies entries. Applied only to string arguments of `db`/`fcb`/`fcc`.

### AssemblyLine (extended from spec 001)

| Field | Type | Description |
|-------|------|-------------|
| lineNumber | int | Source line number |
| address | Word | Address of first byte |
| bytes | vector\<Byte\> | Emitted bytes |
| sourceText | string | Original source text |
| hasAddress | bool | True if line emits or references an address |
| isMacroExpansion | bool | True if line came from macro expansion |
| fileName | string | Source file name (for includes) |
| cycleCounts | int | Instruction cycle count (0 if not an instruction) |

### ExprContext

Context passed to the expression evaluator.

| Field | Type | Description |
|-------|------|-------------|
| symbols | map\<string, int32_t\> | Current symbol table |
| currentPC | int32_t | PC of the current line |

### ExprResult

| Field | Type | Description |
|-------|------|-------------|
| success | bool | True if evaluation succeeded |
| value | int32_t | Evaluated value |
| error | string | Error message if failed |
| hasUnresolved | bool | True if failed due to undefined symbol |

## Relationships

```
Assembler
├── OpcodeTable              (1:1, existing)
├── SymbolTable              (1:1, map<string, Symbol>)
├── MacroTable               (1:1, map<string, MacroDefinition>)
├── ConditionalStack         (1:1, stack<ConditionalState>)
├── Segments[3]              (1:3, Code/Data/Bss)
├── CharacterMap             (1:1)
├── StructDefinitions        (1:N)
├── FileReader               (1:1, injectable interface)
└── ExpressionEvaluator      (static, stateless)

AssemblyResult (returned by Assemble)
├── bytes                    (flat binary image)
├── symbols                  (map<string, Word>)
├── errors                   (vector<AssemblyError>)
├── warnings                 (vector<AssemblyError>)
└── listing                  (vector<AssemblyLine>)
```

## State Transitions

### Assembler Processing Pipeline

```
Source Text
    │
    ├─► Split into lines
    │
    ├─► For each line:
    │   ├─► Check conditional stack (skip if not assembling)
    │   ├─► Check for macro definition (collect body lines)
    │   ├─► Check for macro invocation (expand, recurse)
    │   ├─► Check for constant definition (= / equ / set)
    │   ├─► Check for directive (org, db, dw, ds, if, else, endif, include, struct, cmap, etc.)
    │   ├─► Check for label (colon or column-0)
    │   └─► Parse instruction (mnemonic + operand)
    │
    ├─► Pass 1: Compute PC values and collect symbols
    │
    └─► Pass 2: Resolve forward references and emit bytes
```

### Conditional Assembly State Machine

```
if (expr != 0)     →  push {assembling=true, seenElse=false}
if (expr == 0)     →  push {assembling=false, seenElse=false}
else (top.asm)     →  top = {assembling=false, seenElse=true}
else (!top.asm)    →  top = {assembling=parentAssembling, seenElse=true}
endif              →  pop
```
