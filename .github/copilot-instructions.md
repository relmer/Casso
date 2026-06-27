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
- **NEVER** use angle-bracket includes (`<header>`) anywhere except `Pch.h` or a library project's umbrella header (currently only `Dxui.h`)
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
- **No anonymous namespaces.** NEVER use `namespace {}`. Declare file-local
  constants as file-scope `static constexpr` (with the `s_k` Hungarian naming
  above); put file-local helpers as class `static` members, not free functions.
  More broadly, strongly prefer class members over free/global functions — a
  free function needs a very convincing justification.
- **No magic numbers** — all numeric literals must be named constants with clear intent.
  Exceptions: 0, 1, -1, nullptr, and sizeof expressions.

### EHM (Error Handling Macros)
- Every function that calls a failable API must use the EHM pattern:
  `HRESULT hr = S_OK;` at top, `Error:` label before cleanup, single exit via `return hr;`
- EHM is for **all functions with failable operations**, regardless of return
  type (HRESULT/enum/int/struct/void). For non-HRESULT returns, keep a local
  vestigial `HRESULT hr = S_OK;` for macros and return the normal result at
  `Error:`; for `void`, `Error:` must end with explicit `return;`.
- Functions returning `HRESULT` MUST have exactly one exit point (`Error:` -> `return hr;`).
  Do not use early `return` statements in those functions.
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
  inside macro arguments. Trivial: `.size()`, `.count()`, `.bad()`, `.empty()`, `.load()`,
  and member access.
  Non-trivial: anything with side effects, allocations, or out params.
- When intentionally ignoring an HRESULT return value, use the `IGNORE_RETURN_VALUE`
  macro. Its second argument is ALWAYS a plain reset value (`S_OK`, `false`, `0`, …),
  NEVER a call — not even a trivial one. Capture the result into a variable FIRST,
  then pass that variable plus the reset value. (Other EHM macros tolerate trivial
  calls in their arguments, if not ideal; `IGNORE_RETURN_VALUE` does not.)
  ```cpp
  // WRONG — a call inside the macro (even a trivial one is wrong here):
  IGNORE_RETURN_VALUE (hr, m_wasapiAudio.Initialize ());

  // RIGHT — store first, then reset:
  hr = m_wasapiAudio.Initialize ();
  IGNORE_RETURN_VALUE (hr, S_OK);

  // RIGHT — non-HRESULT result, reset to a neutral value:
  consumed = m_uiShell.OnLButtonDown (x, y);
  IGNORE_RETURN_VALUE (consumed, false);
  ```
- Use `CHRA`/`CWRA` (assert variant) for API failures that indicate bugs
- Use `CHR`/`CWR` for expected failures
- Use `CHRN`/`CBRN` for user-facing notification errors (auto-detects GUI/console)
- Use `CHRF`/`CBRF` for failures with a custom action (e.g., setting an error string)
- Use `BAIL_OUT_IF` for early-exit guard checks with a specific HRESULT
- **Default to asserting variants** (`CHRA`/`CWRA`/`CBRA`/`CPRA`). Only use
  non-asserting (`CHR`/`CWR`/`CBR`/`CPR`) when failure is legitimately
  possible due to user input or external state (e.g., user-provided file
  path, network). Failure of internal API calls indicates a Casso bug and
  SHOULD assert.
- **CPR/CPRA test C++ allocation results only** (sets `hr = E_OUTOFMEMORY`).
  Use only for `new`/`malloc` — APIs that don't call `SetLastError`.
  For other pointer checks:
  - **Parameter pointer validation**: `CBRAEx (ptr, E_INVALIDARG)` —
    null param passed by caller is an argument error, not OOM.
  - **Member-state precondition** (`m_foo` must have been initialized):
    `CBRA (m_foo)` — null member = Casso bug, default `E_FAIL`.
  - **Win32 API that returns a handle/pointer** (HWND from
    `CreateWindowEx`, HDC from `GetDC`, HGLOBAL from `GlobalAlloc`,
    HMENU from `CreatePopupMenu`, etc.): `CWRA (ptr)` —
    these APIs document `GetLastError` on failure, so CWRA captures
    the real error code rather than blindly reporting `E_OUTOFMEMORY`.
- For **non-HRESULT-returning** functions (returning enum/int/struct/void/etc.)
  that still want flat EHM control flow, declare a vestigial
  `HRESULT hr = S_OK;` at the top of the function purely to satisfy the
  macros (`__EHM_Base` writes to `hr` and `goto`s `ErrorLabel`). The
  `Error:` label simply precedes `return <result>;`. The dead store
  optimizes away in release. Example: `MachineConfigUpgrade::Plan`
  in `CassoEmuCore/Core/MachineConfigUpgrade.cpp` uses this to flatten
  a decision tree returning an enum.
- For **`void` functions** using EHM, the `Error:` label must be followed
  by explicit `return;`, not a lonely `;`. The dangling semicolon reads
  like a typo.
- **Prefer EHM bail-out over body-wrapping.** Use `CBR`/`CHR`/`CHRF` at
  the top with a jump to `Error:` rather than wrapping the function body
  in `if (precondition) { … }`. EHM flattens indentation; body-wrap
  increases it.
- Use EHM bail-outs aggressively to reduce indentation; inside loops prefer
  guard-style `continue`/`break` patterns rather than adding nested `if` blocks.
- When multiple EHM macro calls (`CBR`/`CBRF`/`CHR`/`CHRF`/etc.) appear
  on **consecutive lines** (no blank lines or comments between them),
  column-align their arguments — same rule as variable declarations.
- Macro-selection guidance:
  - `*A` variants (`CHRA`/`CWRA`/`CBRA`/`CPRA`) for bug-indicating/internal failures.
  - Non-`*A` variants (`CHR`/`CWR`/`CBR`/`CPR`) for expected user/external failures.
  - `*F` variants (`CHRF`/`CBRF`) when you must run custom failure action.
  - `*N` variants (`CHRN`/`CBRN`) for user-facing notification failures.
  - `CWR/CWRA` for Win32 APIs that set `GetLastError`; `CBR/CBRA` for boolean checks.
  - `CPR/CPRA` only for allocation results (`new`/`malloc`-style OOM checks).

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
- **Function calls** and **declarations in `.h` files**: wrap and align
  parameters to the opening `(`. The first argument stays on the same
  line as the opening paren; continuation lines align under it.
```cpp
hr = D3D11CreateDeviceAndSwapChain (nullptr,
                                    D3D_DRIVER_TYPE_HARDWARE,
                                    nullptr,
                                    createFlags);
```
- **In `.h` header declarations**, the opening-paren columns of
  successive declarations (without interceding comments) must align.
  Pad return-type + name with spaces so the `(` columns line up:
```cpp
    HRESULT Initialize (ID3D11Device         * pDevice,
                        ID3D11DeviceContext  * pContext,
                        UINT                   viewportWidthPx,
                        UINT                   viewportHeightPx);
    void    Shutdown   ();
    HRESULT Resize     (UINT widthPx, UINT heightPx);
```
- **Function definitions in `.cpp` files**: the first parameter wraps
  to the next line (indented one level), with one parameter per line,
  column-aligned like variable declarations (type, pointer/ref column, name):
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
- **Blank line after a closing brace.** A closing `}` MUST be followed by a blank line, EXCEPT when it ends a do-while (`} while (...)`), is followed by `else`, or is immediately followed by another closing `}`. Guard clauses and `switch`/`case` blocks are **not** exceptions.

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
- **Function documentation comments belong in the `.cpp` file** inside
  the `////` header block — NOT in the `.h` declaration. Headers should
  have only terse one-liner comments (or none) on member functions.
- **No phase/task/spec references in comments.** Never include spec
  numbers, phase IDs, task numbers, or "Per spec …" / "Open Question N"
  in code comments. These have no context without the spec and become
  meaningless noise. Write comments that stand alone.

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
- Unit tests **MUST NEVER** rely on or alter real system state
- **ALL** system services **MUST** be mocked or abstracted behind interfaces:
  - **File system**: no reading/writing real files on disk in unit tests
  - **Registry**: no access to the real Windows registry
  - **Network**: no real HTTP/socket calls
  - **Process/environment**: no inspection of real processes, env vars, or console handles
  - **System APIs**: no direct calls to APIs like `SHGetKnownFolderPath`, `CreateFileW`,
    `CreateToolhelp32Snapshot`, `OpenProcessToken`, `DeviceIoControl`, etc.
- If a module uses system APIs, inject dependencies via interfaces and test
  pure/data-driven logic with mocks or synthetic inputs.
- Temp files are acceptable only in integration tests, never in unit tests.

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
at specs/013-dxui-framework-extraction/plan.md
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
