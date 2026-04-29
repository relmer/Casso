# Copilot Instructions for Casso65

## Project Overview

Casso65 is a 6502 CPU emulator in C++. The solution has three projects:

- **Casso65Core** — Static library containing all CPU logic (Cpu, CpuOperations, Microcode, instruction groups)
- **Casso65** — Console application (just `main()`, links Casso65Core)
- **UnitTest** — DynamicLibrary (Microsoft Native CppUnitTest, links Casso65Core)

## C++ Specific Guidelines

### Precompiled Headers
- Every `.cpp` file MUST include `"Pch.h"` as its **first** `#include`
- **NEVER** use angle-bracket includes (`<header>`) anywhere except `Pch.h`
- All system headers and STL headers belong in `Pch.h`
- Individual `.cpp` and `.h` files use only quoted includes (`"header.h"`) for project headers

### Code Style
- Use spaces for indentation (match existing code style)
- **NEVER** break existing column alignment in declarations
- **ALWAYS** preserve exact indentation when replacing code
- Keep functions focused and short — ideally under ~50 lines
- Each function should have a single clear purpose

### Code Formatting — CRITICAL RULES

#### **NEVER** Delete Blank Lines
- **NEVER** delete blank lines between file-level constructs (functions, classes, structs)
- **NEVER** delete blank lines between different groups (e.g., C++ includes vs C includes)
- **NEVER** delete blank lines between variable declaration blocks
- Preserve all existing vertical spacing in code

#### Top-Level Constructs (File Scope)
- **EXACTLY 5 blank lines** between all top-level file constructs:
  - Between preprocessor directives (#include, #define, etc.) and first function
  - Between include blocks and namespace declarations
  - Between namespace and struct/class definitions
  - Between structs/classes and global variables
  - Between global variables and first function
  - Between all function definitions
  - **After the last function in the file**
- **NEVER** add more than 5 blank lines
- **NEVER** delete blank lines if it would result in fewer than 5

#### Function/Block Internal Spacing
- **EXACTLY 3 blank lines** between variable definitions at the top of a function/block and the first real statement
- **1 blank line** for standard code separation within functions

#### Correct Spacing Example:
```cpp
#include "Pch.h"

#include "Header.h"
#include "Header2.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Function1
//
////////////////////////////////////////////////////////////////////////////////

void Function1()
{
    Type var1;
    Type var2;

    Type var3 = value;  // Different semantic group



    // Code section
    DoSomething();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Function2
//
////////////////////////////////////////////////////////////////////////////////

void Function2()
{
    // ...
}
```

#### **NEVER** Break Column Alignment
- **NEVER** break existing column alignment in variable declarations
- **NEVER** break alignment of:
  - Type names
  - Pointer/reference symbols (`*`, `&`)
  - Variable names
  - Assignment operators (`=`)
  - Initialization values
- **ALWAYS** preserve exact column positions when replacing lines
- When modifying a line, ensure replacement maintains same indentation as original

#### Indentation Rules
- **ALWAYS** preserve exact indentation when replacing code
- **NEVER** start code at column 1 unless original was at column 1
- Count spaces carefully — if original had 12 spaces, replacement must have 12 spaces
- Use spaces for indentation (match existing code style)

### Comment Blocks
- Function and class comment blocks use 80 `/` characters as delimiters
- One empty comment line before and after the actual comment text:
```cpp
////////////////////////////////////////////////////////////////////////////////
//
//  FunctionName
//
////////////////////////////////////////////////////////////////////////////////
```

### Type Definitions
- `Byte` = `unsigned char`, `SByte` = `signed char`, `Word` = `unsigned short`
- These are defined in `Pch.h`

## Unit Testing

### Test Infrastructure
- Tests use a `TestCpu` subclass (in `TestHelpers.h`) that exposes `Cpu`'s protected members
- No production code changes needed for testing
- Test files are organized per module: `CpuInitializationTests.cpp`, `CpuOperationTests.cpp`, `AddressingModeTests.cpp`

### Test Isolation
- Tests must be **deterministic** and **repeatable**
- Use `TestCpu::InitForTest()` for clean CPU state — never rely on `Cpu::Reset()`
- Use `TestCpu::WriteBytes()` to set up instruction sequences in memory
- Use `TestCpu::Step()` / `StepN()` to execute instructions
- Call `CpuOperations` static methods directly for unit-level tests
- No test may run the real `Casso65` binary

## Build System

### Building
- Use VS Code build tasks (Ctrl+Shift+B), not direct MSBuild calls
- Scripts are in `scripts/` — `Build.ps1`, `RunTests.ps1`, `VSTools.ps1`
- Supported platforms: x64, ARM64
- Toolset: v145 (VS 2026)

### Pre-Commit Gates
- **ALL** tests MUST pass before committing
- Build MUST succeed with no errors before committing
- Each commit must leave the codebase in a compilable, tests-passing state
- **ALWAYS** update `CHANGELOG.md` for user-visible changes (`feat`, `fix`, `perf`)
- **ALWAYS** update `README.md` when features, test counts, or roadmap items change

### Validation Suites for Significant Changes
- Any significant changes to the **assembler** or **CPU emulator** implementation
  require running both extended validation suites before committing:
  - **Dormann**: `scripts/RunDormannTest.ps1` — Klaus Dormann 6502 functional test
  - **Harte**: `scripts/RunHarteTests.ps1 -SkipGenerate` — Tom Harte SingleStepTests
- These suites validate end-to-end correctness beyond the unit test suite
- "Significant" includes: refactors, new instructions, addressing mode changes,
  assembler directive changes, expression evaluation changes, and binary output changes

## Commit Messages

- Use [Conventional Commits](https://www.conventionalcommits.org/) format: `type(scope): description`
- **Scope is always required** — never omit it
- Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`, `ci`, `build`
- Examples:
  - `feat(cpu): implement PHA/PLA stack operations`
  - `fix(ops): correct ShiftLeft dispatch to use ASL not ROL`
  - `test(adc): add signed overflow edge cases`

## Shell and Terminal Rules

- **ALL** terminal windows use PowerShell, not CMD
- **ALWAYS** format commands for PowerShell syntax

<!-- SPECKIT START -->
For additional context about technologies to be used, project structure,
shell commands, and other important information, read the current plan
at specs/003-apple2-platform-emulator/plan.md
<!-- SPECKIT END -->

## Security Rules

- **NEVER** download or execute external binaries — no `.exe`, `.dll`, `.com`, or other executables from any source
- **NEVER** run `Invoke-WebRequest` or `curl` to fetch executables
- If a tool is needed, it MUST be buildable from source within the repo
- GPL-licensed source files (e.g., Dormann test suite) may be downloaded for on-demand testing but MUST be deleted after use and MUST NOT be committed to the repository
