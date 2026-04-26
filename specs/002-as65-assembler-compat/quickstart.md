# Quickstart: Full AS65 Assembler Clone

**Feature**: 002-as65-assembler-compat  
**Branch**: `002-as65-assembler-compat`

## Prerequisites

- Visual Studio 2026 (v145 toolset)
- VS Code with C++ extension
- PowerShell 7 (`pwsh`)
- All spec 001 tests passing

## Build

Use VS Code build tasks (Ctrl+Shift+B):
- `Build + Test Debug (current arch)` — builds and runs all tests
- `Build Debug (current arch)` — build only

## New Files to Create

1. `Casso65Core/ExpressionEvaluator.h` — expression evaluator header
2. `Casso65Core/ExpressionEvaluator.cpp` — recursive descent parser
3. `Casso65Core/OutputFormats.h` — S-record and Intel HEX writer header
4. `Casso65Core/OutputFormats.cpp` — output format implementations
5. `UnitTest/ExpressionEvaluatorTests.cpp` — expression tests
6. `UnitTest/ConditionalAssemblyTests.cpp` — if/else/endif tests
7. `UnitTest/MacroTests.cpp` — macro definition and expansion tests
8. `UnitTest/ConstantTests.cpp` — =/equ/set tests
9. `UnitTest/DirectiveTests.cpp` — ds/align/cmap/struct/etc. tests
10. `UnitTest/IncludeTests.cpp` — include file tests
11. `UnitTest/OutputFormatTests.cpp` — S-record/Intel HEX tests
12. `UnitTest/DormannIntegrationTests.cpp` — end-to-end Dormann assembly

## Files to Modify

1. `Casso65Core/Assembler.h` — add macro/conditional/include/struct/cmap state
2. `Casso65Core/Assembler.cpp` — main two-pass loop rewrite with all new features
3. `Casso65Core/AssemblerTypes.h` — new types (MacroDefinition, StructDefinition, etc.)
4. `Casso65Core/Parser.h/.cpp` — colon-less labels, AS65 directive recognition
5. `Casso65Core/OpcodeTable.h/.cpp` — instruction synonyms, cycle count data
6. `Casso65Core/Casso65Core.vcxproj` — add new .cpp/.h files
7. `Casso65/CommandLine.h/.cpp` — AS65-compatible flags
8. `Casso65/Casso65.cpp` — dispatch new flags
9. `UnitTest/UnitTest.vcxproj` — add new test files

## Validation Steps

1. **Build**: `Build + Test Debug (current arch)` — must pass with zero errors
2. **Existing tests**: All spec 001 tests in `AssemblerTests.cpp` and `ParserTests.cpp` must pass unchanged
3. **Dormann assembly**: Assemble `6502_functional_test.a65` → compare against reference binary
4. **AS65 testcase**: Assemble `testcase.a65` → compare against AS65 reference output

## Implementation Order

1. Expression evaluator (foundation for everything)
2. Constants (`=`/`equ`/`set`) and current-PC (`*`)
3. Conditional assembly (`if`/`else`/`endif`)
4. Macros (basic: `\1`–`\9`, `\?`, `endm`)
5. Colon-less labels + AS65 directive spellings + storage directives
6. Advanced macros (`local`, `exitm`, `\0`, named params)
7. Include files
8. Struct/cmap
9. Output formats (S-record, Intel HEX)
10. CLI flag parity
11. Listing enhancements
12. Instruction synonyms, multi-NOP, number format extensions
13. End-to-end Dormann validation
