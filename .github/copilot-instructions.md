# Copilot Instructions for Casso

## Project Overview

Casso is a 6502 CPU emulator, assembler, and Apple II platform emulator in C++.
The solution has five projects:

- **CassoCore** — Static library containing CPU logic, assembler, parser, opcode table
- **CassoEmuCore** — Static library containing Apple II devices, video modes, audio generator
- **Casso** — Win32 GUI application (Apple II emulator, links CassoCore and CassoEmuCore)
- **CassoCli** — Console application (AS65-compatible assembler CLI, links CassoCore)
- **UnitTest** — DynamicLibrary (Microsoft Native CppUnitTest, links CassoCore and CassoEmuCore)

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
- Braces always required, even for single-statement `if`/`while`/`for`/`switch`
- No comma-separated variable declarations
- Prefer in-class member initialization (`.h`) over constructor initializer lists (`.cpp`)
- **Function-call/declaration spacing.** Space before non-empty parens
  (`fn (arg)`, `MyClass::Method (a, b)`); **NO** space before empty
  parens (`fn()`, `obj.GetThing()`). Never `fn ()`. Applies equally to
  declarations, definitions, calls, member access, and method calls in
  test bodies. Run `rg -n '\w \(\)' Casso/ CassoCore/ CassoEmuCore/ CassoCli/ UnitTest/`
  on any new or merged code before committing — should return zero hits
  in lines you authored or merged.
- **Cast spacing.** Space after a C-style cast:
  `(float) std::numbers::pi`, not `(float)std::numbers::pi`. Same for
  `(int) value`, `(Word) addr`, etc.
- File-scope statics use Hungarian: `s_<typePrefix><Name>`. Type prefixes:
  `k` = constant, `psz` = null-terminated string ptr (narrow OR wide),
  `ch` = char (narrow OR wide), no special wide marker. E.g.
  `s_kpszHost` (LPCWSTR), `s_kchBullet` (wchar_t),
  `s_kRomCatalog` (constant array).
- **No magic numbers** — all numeric literals must be named constants with clear intent.
  Exceptions: 0, 1, -1, nullptr, and sizeof expressions.

### EHM (Error Handling Macros)
- Every function that calls a failable API must use the EHM pattern:
  `HRESULT hr = S_OK;` at top, `Error:` label before cleanup, single exit via `return hr;`
- **NEVER** use bare `goto Error` — always use EHM macros (CHR, CBR, CWRA, CHRF, etc.)
- EHM macros must only contain **trivial expressions** — no function calls with side effects
  or out params. Store the result first, then pass it to the macro:
  ```cpp
  // WRONG:
  CHRF (root.GetString ("name", outConfig.name), outError = "...");

  // RIGHT:
  hr = root.GetString ("name", outConfig.name);
  CHRF (hr, outError = "...");
  ```
- The same rule applies to **all** macros (not just EHM): never call non-trivial functions
  inside macro arguments. Trivial: `.size()`, `.empty()`, `.load()`, member access.
  Non-trivial: anything with side effects, allocations, or out params.
- When intentionally ignoring an HRESULT return value, use the `IGNORE_RETURN_VALUE` macro:
  ```cpp
  IGNORE_RETURN_VALUE (hr, m_wasapiAudio.Initialize ());
  ```
- Use `CHRA`/`CWRA` (assert variant) for API failures that indicate bugs
- Use `CHR`/`CWR` for expected failures
- Use `CHRN`/`CBRN` for user-facing notification errors (auto-detects GUI/console)
- Use `CHRF`/`CBRF` for failures with a custom action (e.g., setting an error string)
- Use `BAIL_OUT_IF` for early-exit guard checks with a specific HRESULT

### Variable Declarations
- **ALL** local variables declared at the **top** of the function (or top of a necessary local block)
- Do **NOT** declare variables at point of first use
- Column-align sequential declarations: type, pointer/reference symbol, name, `=`, value
- If **any** line in a declaration block has a pointer `*` or reference `&`, **all** lines must include a column for that symbol — non-pointer lines use a space placeholder so subsequent columns stay aligned
- Remove unnecessary scoping braces — hoist the variable to function top instead

Example with pointer column:
```cpp
HRESULT          hr             = S_OK;
WAVEFORMATEX   * mixFormat      = nullptr;
WAVEFORMATEX     desiredFormat  = {};
REFERENCE_TIME   bufferDuration = 1000000;
BYTE           * buffer         = nullptr;
```

### Wrapped Function Parameters
- When a function call is too long for one line, wrap and align parameters to the opening `(`
```cpp
hr = D3D11CreateDeviceAndSwapChain (nullptr,
                                    D3D_DRIVER_TYPE_HARDWARE,
                                    nullptr,
                                    createFlags);
```
- When a function **definition** wraps its parameter list, the **first** parameter must wrap to the next line (indented one level), with one parameter per line, column-aligned like variable declarations (type, pointer/ref column, name)
```cpp
HRESULT EmulatorShell::Initialize (
    HINSTANCE              hInstance,
    const MachineConfig  & config,
    const std::string    & disk1Path,
    const std::string    & disk2Path)
```

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
- No test may run the real `CassoCli` binary

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
- **Code analysis MUST pass** before committing: run `scripts\Build.ps1 -RunCodeAnalysis` to verify
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

## Branching and Merging

- **NEVER squash on merge.** All branch merges to `master` must use
  `--no-ff` to preserve commit history.

## Workspace Hygiene

- **Always clean up diagnostic artifacts.** When debugging produces
  log files, trace dumps, stderr captures, or any other stray files
  in the working tree, remove them explicitly when the debugging
  session ends.
- **Do NOT add stray-file patterns to `.gitignore`.** The user
  prefers stray files to surface in `git status` as a visible
  reminder. Silencing them with `.gitignore` defeats that signal.
- The only `.gitignore`d disk image is the Apple-owned
  `Disks/Apple/dos33-master.dsk`. Disks we author belong in the repo.

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

## Tone & Personality

- **Default to dry, lightly snarky.** Concise quips, casual asides, and
  gentle ribbing of bad ideas (including my own) are encouraged.
- **Technical accuracy is non-negotiable.** Never sacrifice correctness,
  precision, or honest uncertainty for a joke. If the punchline conflicts
  with the truth, drop the punchline.
- **Brevity beats banter.** One well-placed remark beats five mediocre
  ones. Don't pad responses to make room for humor.
- **Punch up, not down.** Snark at machines, processes, flaky tools, and
  bad code — never at the user.
- **Chat only.** This applies to interactive replies. Commit messages,
  code comments, CHANGELOG entries, README content, and other artifacts
  stay neutral and professional.
