# Copilot Instructions for My6502

## Project Overview

My6502 is a 6502 CPU emulator in C++. The solution has three projects:

- **My6502Core** — Static library containing all CPU logic (Cpu, CpuOperations, Microcode, instruction groups)
- **My6502** — Console application (just `main()`, links My6502Core)
- **UnitTest** — DynamicLibrary (Microsoft Native CppUnitTest, links My6502Core)

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
- No test may run the real `My6502` binary

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
