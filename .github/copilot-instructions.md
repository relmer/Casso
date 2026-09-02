# Copilot Instructions for Casso

## Project Overview

Casso is a 6502 CPU emulator, assembler, and Apple II platform emulator in C++.
The solution has five projects:

- **CassoCore**: Static library containing CPU logic, assembler, parser, opcode table
- **CassoEmuCore**: Static library containing Apple II devices, video modes, audio generator
- **Casso**: Win32 GUI application (Apple II emulator, links CassoCore and CassoEmuCore)
- **CassoCli**: Console application (AS65-compatible assembler CLI, links CassoCore)
- **UnitTest**: DynamicLibrary (Microsoft Native CppUnitTest, links CassoCore and CassoEmuCore)

### Architecture: Thin Exe, Rich Testable Core (NON-NEGOTIABLE)

New code goes in the **core libraries** (`CassoCore` / `CassoEmuCore`), NOT an exe. An exe is an empty shell over a core entry function, `CliMain` for `CassoCli`, its GUI equivalent for `Casso`. All emulation, parsing, rendering, device models, persistence, and lifecycle/orchestration logic lives in the core static libs, which both the exe **and** the `UnitTest` project link. This is the whole reason the split exists: so essentially everything is unit-testable and mockable.

- **UT-reachable litmus**: before placing code, ask "can the `UnitTest` project link and exercise this?" If a piece of logic can only be tested by running the exe, it is in the wrong place or the wrong shape (entangled with an `HWND`, device context, COM apartment, or menu id). Factor the logic into core behind data-in/data-out functions or interface seams.
- **The split is testability, NOT a platform boundary.** "Does this call a platform API?" is the wrong question and never justifies putting code in an exe. Calling Win32 is not a reason to live there: file I/O behind an interface seam, a registry read, a clipboard round-trip, a WIC image codec are all drivable by a test, so all belong in core.
- **What actually stays in an exe** is only what cannot exist without the process. For a console app that is `main` and nothing else, doing no more than calling a core entry function and returning what it returns. For a GUI app it is that plus the `HWND`, its message pump, and the device objects. `CassoCli.exe` is the worked example: 3,639 lines to 57.
- **The existing `Casso` exe has accreted logic that belongs in core** (e.g. `EmulatorShell`). That is debt to be extracted, NEVER a template; do not imitate it. Read exe files only to find wiring points (where objects are constructed/owned, machine build/teardown hooks, menu dispatch), never as a structural template for new logic.

See the Constitution's Principle VI (Thin Executable, Testable Core) and Principle II (Testing Discipline).

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
- Keep functions focused and short: ideally under ~50 lines
- Each function should have a single clear purpose
- Braces always required, even for single-statement `if`/`while`/`for`/`switch`
- No comma-separated variable declarations
- Prefer in-class member initialization (`.h`) over constructor initializer lists (`.cpp`)
- **Function-call/declaration spacing.** Space before non-empty parens
  (`fn (arg)`, `MyClass::Method (a, b)`); **NO** space before empty
  parens (`fn()`, `obj.GetThing()`). Never `fn ()`. Applies equally to
  declarations, definitions, calls, member access, and method calls in
  test bodies. Run `rg -n '\w \(\)' Casso/ CassoCore/ CassoEmuCore/ CassoCli/ UnitTest/`
  on any new or merged code before committing, should return zero hits
  in lines you authored or merged.
- **Cast spacing.** Space after a C-style cast:
  `(float) std::numbers::pi`, not `(float)std::numbers::pi`. Same for
  `(int) value`, `(Word) addr`, etc.
- File-scope statics use Hungarian: `s_<typePrefix><Name>`. Type prefixes:
  `k` = constant, `psz` = null-terminated string ptr (narrow OR wide),
  `ch` = char (narrow OR wide), no special wide marker. E.g.
  `s_kpszHost` (LPCWSTR), `s_kchBullet` (wchar_t),
  `s_kRomCatalog` (constant array).
  The leading `s_` says **file-scope static** and nothing else, a class member
  or a function-local drops it and keeps the rest (`kPadDip`, `kpszTitle`), so
  the prefix alone tells you which you are reading. Whether a constant should
  be file-scope at all is decided by "Where a file-local constant goes" below;
  being constant is not the test.
- **No anonymous namespaces.** NEVER use `namespace {}`. Put file-local
  helpers as class `static` members, not free functions. More broadly,
  strongly prefer class members over free/global functions; a free function
  needs a very convincing justification.
- **Where a file-local *type* goes**, by what it is:
  1. **A class, anything with methods** → its own `.h` / `.cpp` pair, one
     class per pair, named for the class. Behavior deserves a translation
     unit and a name it owns; burying it inside another class's files hides
     it from everyone who might reuse it. A *null* implementation of an
     interface is the exception to the pair rule only; it belongs in the
     interface's own header, since it is a property of the contract.
  2. **A plain-data struct with no methods, used only as a member or a
     parameter** → nest it in the class that uses it and let it ride along
     in that header. It has nothing to define out of line.
  3. **A type that is part of the API**: what callers pass in or get back,
     stays a free type in the header, not a member. Nesting it only adds
     `Owner::` to every use, and API types are often testable on their own.

  Note what is *not* a reason to nest: a bare `struct` or `class` in a `.cpp`
  has external linkage and no keyword can change that (`static` applies to
  functions and objects, not types). That makes duplicate type names across
  translation units an ODR violation the linker never reports, but the fix
  is to give the type a proper home per the order above, not to hide it.
- **Declare in the header, define in the `.cpp`.** A nested type declared
  (`struct Foo;`) rather than defined needs none of its own dependencies in
  the header, including base classes. Defining it inline instead drags those
  includes along for every file that touches the header.
- **Where a file-local constant goes**, in order:
  1. **Used by one function, declaration fits on 1-2 lines** → move it *into*
     that function as a local `constexpr`. Nothing leaves the function, and
     nothing reaches the header. Drop the `s_` prefix: it marks a file-scope
     static, and this is not one.
  2. **Used by several functions** → private `static constexpr` member of the
     class the `.cpp` implements. Also drop the `s_`.
  3. **Declaration spans 3+ lines** → leave it as a file-scope
     `static constexpr`, keeping the `s_k` prefix. This is the deliberate
     exception. At that size it is a *table or payload* (an opcode table, a
     palette, a ROM catalog, shader source) not a parameter the logic hinges
     on. Moving it into the function buries the logic under a wall of data,
     and hoisting it to the class forces a header declaration plus a
     declaration/definition split, which is how `DxuiPainter` briefly ended up
     passing `sizeof (ptr) - 1 == 7` as a shader length.

  The 3-line cut is measured, not guessed: of 1,466 constant declarations in
  the tree, 1,411 (96%) are a single line, and essentially everything at 3+
  lines is a table or blob.
- **No magic numbers**: all numeric literals must be named constants with clear intent.
  Exceptions: 0, 1, -1, nullptr, and sizeof expressions.
- **American spelling, ALWAYS.** Use American English everywhere, identifiers,
  comments, log/UI strings, commit messages, CHANGELOG, README, and docs.
  `color` / `center` / `behavior` / `gray` / `initialize` / `optimize` /
  `analyze`, NEVER `colour` / `centre` / `behaviour` / `grey` /
  `initialise` / `optimise` / `analyse`. No exceptions; this
  applies even when the surrounding pre-existing text uses British spelling
  (fix your own added/modified lines regardless). Quick check on new/merged
  code: `rg -in 'colour|behaviour|centre|grey|initialise|optimise|analyse'`
  should return zero hits in lines you authored.

  **British IDIOM is banned too, not only British spelling.** These are spelled
  the American way letter for letter, so no spelling check will ever catch them;
  they read like a BBC voice rather than a programmer's. Use the replacement:
  `straight away` -> right away / immediately, `whilst` -> while,
  `amongst` -> among, `in future` -> in the future, `different to` ->
  different from, `have got` -> have, `sort out` -> fix, `at the weekend` ->
  on the weekend.

  `cancelled` is NOT on that list: the doubled L is standard American usage
  too, merely less common than `canceled` in US style guides. Both are
  accepted here, so the checker does not flag either, which also means the
  Win32 `ERROR_CANCELLED` family needs no special handling.

  **Commit messages are checked too (CS0008), and there is no opt-out.** A
  message about a spelling fix must not quote what it removed: say what
  changed and where ("11 hits across 8 files, all comments and test names")
  rather than listing the words. A message is prose you write, so unlike source
  (which can be stuck with a name like `ERROR_CANCELLED`) it can always be
  phrased around them. If the gate rejects your push, rephrase; do not reach
  for `--no-verify`, which switches off every rule rather than one.

### Function Names

A function name begins with a **verb**. `GetPrimaryExtension`, not
`ExtensionFor`; `HasReachedCap`, not `CapReached`. A noun-first name reads as a
value rather than an action.

Approved prefixes: `Get` / `Set` for accessors and lookups, `Is` / `Has` /
`Are` / `Can` / `Should` / `Does` / `Did` for bool queries, `Try` for fallible
attempts, and any plain imperative verb otherwise.

| Shape | Example | Becomes |
|---|---|---|
| lookup or accessor | `ExtensionFor`, `TrackOf`, `CellAt` | `Get...` |
| noun-first bool query | `CapReached`, `EverTouched` | `HasReachedCap`, `HasBeenTouched` |
| named constructor | `CassoTheme::Skeuomorphic` | `MakeSkeuomorphic` |

`Make` returns a plain value that cannot fail (`MakeRect`, `MakeCrtParams`).
`Create` hands back an owned resource, an `HRESULT`, or mutated owned state
(`CreateCpu`, `CreateShaders`). Pick on that basis, not on taste.

**Any tense of a verb already satisfies the rule.** `SawCycle`, `HitBound`,
`ExceededLength`, `Matches` and `WritesTheImage` are verb-first and correct as
they stand. Renaming them to `HasSeenCycle` and the like is churn, and has been
reverted once already.

**Exempt, decided rather than overlooked:**

| Category | Examples |
|---|---|
| `OnXxx` handlers | `OnAddressMark`, `OnActivateApp` |
| `XToY` / `XFromY` conversions | `ArgbToHsv`, `CrtModeFromJson` |
| CPU instruction methods | `And`, `Or`, `Xor`, `NoOperation`, `RotateLeft` |
| printer control operations | `FormFeed`, the operation's own name |
| theme color members | `themeColor.ButtonPressed`, which reads as its value |
| platform and library conventions | `ThreadProc`, `CliMain`, `Instance`, `Stat` |
| matrix helpers mirroring DirectXMath | `Mul44`, `PerspectiveFovRH`, `LookAtRH` |
| EHM framework hooks | `EhmBreakpoint`, `EhmNotifyUser` |

Anything whose signature is fixed from outside is exempt for the same reason:
Win32 callbacks, virtual overrides, and `operator` functions.

**This rule is not machine-checked.** The exemptions above are why: a leading
word is a verb or a noun depending on the name it starts, so a verb allowlist
large enough to clear `RenderScene` also clears `FreeSpace` and `SweepLtr`,
which are accessors. Writing new code is therefore the only place the rule gets
applied, and skipping it here is not caught later. When auditing, run two passes
and take the union: the leading word against a verb list, and the declaration's
**shape**, since a `const` member function returning a value is an accessor
whatever it is called. `docs/coding-standards-backlog.md` item 6 carries the
method, the measured counts, and the renames still outstanding.

### EHM (Error Handling Macros)
- Every function that calls a failable API must use the EHM pattern:
  `HRESULT hr = S_OK;` at top, `Error:` label before cleanup, single exit via `return hr;`
- EHM is for **all functions with failable operations**, regardless of return
  type (HRESULT/enum/int/struct/void). For non-HRESULT returns, keep a local
  vestigial `HRESULT hr = S_OK;` for macros and return the normal result at
  `Error:`; for `void`, `Error:` must end with explicit `return;`.
- Functions returning `HRESULT` MUST have exactly one exit point (`Error:` -> `return hr;`).
  Do not use early `return` statements in those functions.
- **NEVER** use bare `goto Error`; always use EHM macros (CHR, CBR, CWRA, CHRF, etc.)
- An EHM macro's **condition may not contain a call of any kind**, not even
  `.size()`, `.empty()`, `.good()` or `.is_open()`. Hoist it to a local **named for
  what is being tested**, then test the local. Gated by `CS0011`.
  ```cpp
  // WRONG:
  CHRF (root.GetString ("name", outConfig.name), outError = "...");
  CBR  (out.is_open());
  CBR  (!bytes.empty());

  // RIGHT:
  hr = root.GetString ("name", outConfig.name);
  CHRF (hr, outError = "...");

  isOpen = out.is_open();
  CBR (isOpen);

  hasBytes = !bytes.empty();
  CBR (hasBytes);
  ```
  Two reasons the ban is absolute rather than "no calls that do work". The macro
  hides what failed, so the bail point should name the condition. And the
  narrower rule cannot be checked, a pattern cannot tell "does work" from "asks
  a question", so it sat in this document unenforced and drifted to 88 sites
  before anyone counted.
- The **comparison stays in the macro**; only the call moves out. Hoist the value,
  not the predicate: `rawSize = raw.size();` then `CBRAEx (rawSize == kFoo, E_INVALIDARG)`.
- The name must say what is tested. `ok` is not a name; it tells you nothing at
  the bail, which is the whole point of hoisting. Same principle as the
  `bool`-returning function rule.
- **Action arguments are exempt** and normally are calls:
  `CBRF (isComma, SetError ("Expected ',' or ']'"))`. Only the condition (the
  first argument, up to the first top-level comma) is covered.
- `SUCCEEDED (hr)` / `FAILED (hr)` inside a `CBR` is wrong for a second reason:
  testing an HRESULT means the macro should be `CHR`, which keeps the actual
  error code instead of substituting a generic failure.
- The same rule applies to **all** macros (not just EHM): never call non-trivial functions
  inside macro arguments. Non-trivial: anything with side effects, allocations, or out params.
- When intentionally ignoring an HRESULT return value, use the `IGNORE_RETURN_VALUE`
  macro. Its second argument is ALWAYS a plain reset value (`S_OK`, `false`, `0`, …),
  NEVER a call, not even a trivial one. Capture the result into a variable FIRST,
  then pass that variable plus the reset value. (Other EHM macros tolerate trivial
  calls in their arguments, if not ideal; `IGNORE_RETURN_VALUE` does not.)
  This is compiler-enforced: the macro routes the reset value through a
  `constexpr` local, so anything but a compile-time constant fails with C2131
  at the use site. CheckStyle's CS0018 catches the same misuse pre-build.
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
  Use only for `new`/`malloc`, APIs that don't call `SetLastError`.
  For other pointer checks:
  - **Parameter pointer validation**: `CBRAEx (ptr, E_INVALIDARG)`,
    null param passed by caller is an argument error, not OOM.
  - **Member-state precondition** (`m_foo` must have been initialized):
    `CBRA (m_foo)`, null member = Casso bug, default `E_FAIL`.
  - **Win32 API that returns a handle/pointer** (HWND from
    `CreateWindowEx`, HDC from `GetDC`, HGLOBAL from `GlobalAlloc`,
    HMENU from `CreatePopupMenu`, etc.): `CWRA (ptr)`,
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
  column-align their arguments, same rule as variable declarations.
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
- If **any** line in a declaration block has a pointer `*` or reference `&`, **all** lines must include a column for that symbol, non-pointer lines use a space placeholder so subsequent columns stay aligned
- Remove unnecessary scoping braces: hoist the variable to function top instead

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

### Code Formatting: CRITICAL RULES

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
- Count spaces carefully: if original had 12 spaces, replacement must have 12 spaces
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
  the `////` header block, NOT in the `.h` declaration. Headers should
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
- Use `TestCpu::InitForTest()` for clean CPU state; never rely on `Cpu::Reset()`
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
- If code cannot be tested this way, it usually lives in the wrong project; see **Architecture: Thin Exe, Rich Testable Core** above; move the logic into a core lib rather than leaving it untested in the exe.

### Degraded Operation Must Be Observable

A component that cannot do its job MUST NOT be indistinguishable from one that
did. This class has bitten five times, in five different layers:

- `NibblizationLayer::Denibblize` returned `S_OK` over sectors it had
  zero-filled, on the flush path (GH #115)
- `RunTests.ps1` reported a full, confident pass against a stale test assembly
- Dormann integration tests passed without their data present, having done no
  work
- A `DialectId` enumerator with no profile behind it silently answered with a
  different dialect, support that looked present and was not
- A corpus harness looping over an empty entry list passes by comparing nothing

Concretely:

- **Tests**: a test that cannot reach its data FAILS. It does not skip quietly
  and it does not pass. "N passed" must mean N things were checked.
- **Fixtures and corpora**: assert a non-zero item count *before* asserting over
  the items. A loop over an empty set is a passing test that tests nothing, and
  it is indistinguishable from a full one in the output.
- **Production code**: a function that could not do what it was asked MUST NOT
  return success. That is what EHM exists for; producing `S_OK` after a partial
  failure defeats the whole pattern.
- **Identifier / implementation pairs**: where an enum is **total** over its
  implementations, sweep the *enum*, not only the table, a table sweep visits
  only rows that exist by construction and structurally cannot find a missing
  one. `DialectId` and `Directive` are total; `CommandLineOptions::Subcommand`
  is deliberately partial (`None` / `Help` / `Version` / `As65` are not
  bare-word subcommands), so the same sweep there would fail correct code.
  `UnitTest/DirectiveTokenTests.cpp` sweeps both directions and is the
  exemplar.

The common shape is a **degraded state that reads as a healthy one**: zeros that
look like a blank track, a stale binary that looks like a run, a missing profile
that looks like support. Ask of any success path: could this have reported
success while doing nothing?

Two specific practices fall out of this:

- **Verify a new test fails without the fix.** A test written after the code it
  covers frequently passes for the wrong reason, the setup happens to satisfy it
  regardless of whether the fix is present. Revert the fix, confirm the test
  fails, restore. The assertion message is the proof:
  `Expected:<opener.a65> Actual:<>` shows what the broken path actually produced,
  where a green run shows nothing at all. A real case: tests for include-file
  diagnostic attribution passed immediately, because the include happened to be
  the last thing processed, so the ambient state was coincidentally correct. They
  only discriminated once a trailing top-level line was added.

  The general form is **mutate what the test covers and confirm the test
  notices.** Reverting a fix is the instance that applies when the fix changed
  existing behavior. A test covering newly-added API has nothing to revert to, 
  "without the fix" does not compile, so the mutation is to stub the
  implementation instead: make the classifier return a constant and confirm the
  test goes red. A test that stays green under a reverted fix is therefore not
  automatically weak; check first whether there was prior behavior to
  discriminate against. Two of the three damage tests for GH #115 stayed green
  correctly, because the mechanisms they cover reported nothing at all before
  that work.
- **A `Copy-Item` restore defeats build staleness detection.** `Copy-Item`
  preserves `LastWriteTime`, so restoring a backup makes the source look **older**
  than the object built from the edited version. MSBuild then skips the rebuild, 
  the tell is a sub-second "Build succeeded", and the suite runs against the code
  you just reverted. `RunTests.ps1`'s staleness guard **cannot** catch this: it
  detects source *newer* than the assembly, and this is the reverse, so the guard
  sees a fresh assembly and passes. After any restore, stamp the file before
  rebuilding: `(Get-Item path).LastWriteTime = Get-Date`

## Build System

### Building
- Use VS Code build tasks (Ctrl+Shift+B), not direct MSBuild calls
- Scripts are in `scripts/`, `Build.ps1`, `RunTests.ps1`, `VSTools.ps1`
- Supported platforms: x64, ARM64
- Toolset: v145 (VS 2026)
- **`RunTests.ps1` does not build unless you pass `-Build`.** Every VS Code
  task that calls it is labeled "(no build)" and the composite `Build + Test`
  tasks chain a build in front; from a terminal, pass `-Build`. A staleness
  guard refuses to run when the test assembly is older than the newest source
  that compiles into it, because a stale run reports a full, confident pass
  against code that is not on disk, and a new test file that never compiled
  in is simply absent from the count.
- **`RunTests.ps1 -Filter <word>`** runs only matching tests, for the edit-test
  loop. A bare word matches as a substring of the fully qualified name
  (`-Filter Merlin`); anything containing filter grammar passes through to
  vstest's `/TestCaseFilter:` verbatim. A filtered run prints a loud banner
  saying it is not the suite; never report a filtered pass as a suite pass.
- Debug and Release differ enormously: the Debug suite runs ~15 minutes, Release
  ~2. They also run **different test sets**, assertion-behavior tests verify
  nothing in Release, so Release is not a drop-in substitute for the pre-merge
  gate.
- **Restoring a file from a backup copy defeats the incremental build.**
  `Copy-Item` stamps the restored file with the *backup's* mtime, which is older
  than the object built from the version you are replacing, so MSBuild skips the
  compile and the binary still contains what you thought you reverted. The
  timestamp check passes, because it only detects staleness pointing forward.
  The tell is a build that finishes in under a second when it should take
  several; treat a suspiciously fast build after a restore as a skipped one.
  Either touch the file (`(Get-Item p).LastWriteTime = Get-Date`) and rebuild, or
  make the restore an editor write rather than a filesystem copy. This matters
  most when deliberately breaking code to check that a test fails without its
  fix, which is exactly when a silently-unbuilt revert is most misleading.

### Style Gate (pre-push)

`scripts/CheckStyle.ps1` mechanically enforces the subset of the style rules
above that reduce to a mechanical test: empty-paren spacing, anonymous
namespaces, American spelling, angle-bracket includes, `Pch.h`-first, bare
`goto Error`, cast spacing, producing `S_FALSE`, Claude attribution in commit
messages, lookup tables in an executable (CS0021), the banner/blank-line
structure rules (CS0014–CS0017), and
declaration-run column alignment (CS0019, flags only runs that
`scripts/FixDeclAlign.ps1 -Apply` can mechanically repair; late declarations
have a companion fixer in `scripts/FixLateDecls.ps1`). Commit subjects on
unpushed commits must be `action(scope): description`; the scope is not
optional (CS0020; merge/revert subjects keep git's conventions, and
already-pushed history is never re-judged). Rules that need judgment (magic
numbers, EHM single-exit) are **not** covered and remain review's job.

**`bool` returns must be self-describing.** Return `bool` only when the
function's name makes `true` / `false` obvious: `IsXxx`, `HasXxx`, `TryXxx`,
`CanXxx`. `ExtractFirstHDropPath` returning `false` could mean no data object,
no path, or a failed read; `TryExtractFirstHDropPath` says which. When
converting a function away from `HRESULT`, rename it to suit. Not gated yet;
see `docs/coding-standards-backlog.md`.

**Lookup tables in an executable (CS0021).** A `case X::Y: return "..."` under
`Casso/` or `CassoCli/` is a mapping, and a mapping is a decision, which
Principle VI puts in a core library where `UnitTest` can reach it. The rule is
narrow on purpose: only a switch arm returning a string literal, so dispatching
and computing switches are untouched.

It exists because the failure is not cosmetic. `CreateDiskDialog` held three of
these -- format to extension, format to caption, filling to caption -- and each
ended in a `default:` arm answering with another entry's name, so a value added
without an arm was silently rendered as something else rather than refused. One
of them would have shipped a create dialog naming a nibble image `.woz`. Living
in the exe, which the test assembly does not link, nothing could reach them to
notice; the fix was to move them, after which the tests wrote themselves.

`Include` in the check table is the inverse of `Exclude` and matches a path
PREFIX, where `Exclude` matches a suffix. Do not reuse one for the other: the
first version of CS0021 did, matched nothing, and reported a clean tree while
checking no files at all.

**`S_FALSE` (CS0009).** Do not *produce* `S_FALSE` without explicit
approval. Returning it overloads the result with a second, private meaning (
"succeeded, but not the way you'd assume") that a caller can only decode by
reading the callee. Model the second outcome explicitly instead, with a
status enum or an out-param, and leave `hr` meaning only success or failure.
*Testing* for `S_FALSE` is not flagged: when an external API returns it you
have no choice. Where producing it is genuinely unavoidable, mark the line
`// EHM-ALLOW-SFALSE: <reason>`.

**`Build.ps1` enables the hook for you.** It points the clone at `.githooks`
on every run, announcing itself the one time it changes anything, so a fresh
clone or a new worktree acquires the gate without anyone remembering to.

Git refuses to let a repository configure its own clones, `core.hooksPath`
arriving with a checkout would make cloning an arbitrary-code-execution
hazard, so `.git/config` never syncs and this cannot be committed once and
inherited. Building is the closest thing to a step everyone already takes.

To set it by hand (or to check):

```powershell
git config core.hooksPath .githooks
git config --get core.hooksPath
```

- The pre-push hook is **diff-scoped**: it inspects only the lines a push
  *adds*, so a push is judged on its own contribution.
- Every rule is swept to zero tree-wide, and CI's `style` job runs
  `scripts/CheckStyle.ps1 -Mode Tree` as the backstop; a violation anywhere
  fails the build, so the backlog can never regrow.
- Check your branch by hand any time with `scripts/CheckStyle.ps1`.
- **Adding a NEW file? Run `scripts/CheckStyle.ps1 -Mode Staged` before you
  commit.** Diff mode compares two commits, so a file that has never been
  committed contributes no added lines to that comparison and is invisible to
  every rule; the run reports `0 file(s) checked … OK` and means it. The
  first violation then surfaces only once the commit exists, and fixing it
  costs an amend. Staged mode diffs the index against `HEAD`, so it sees the
  new file while it is still staged. It skips the commit-message check, since
  there is no commit yet, and says `SKIPPED` out loud when nothing is staged
  rather than passing on an empty inspection.

### Merge-to-Master Gates
These gates apply to **`master`**, i.e. every commit that lands on `master`
(directly or via merge/PR) MUST satisfy them. They do **NOT** apply to every
intermediate commit on a feature branch: feature branches routinely carry many
work-in-progress commits, and running the full build/test/analysis suite on each
one wastes time. Validate the full gates **once, just before merging/PRing the
branch to `master`** (and re-run after resolving merge conflicts).
- **ALL** tests MUST pass before merging to master
- Build MUST succeed with no errors before merging to master
- Every commit that lands on `master` must leave the codebase compilable and tests-passing
- **Code analysis MUST pass** before merging to master: run `scripts\Build.ps1 -RunCodeAnalysis` to verify
- **ALWAYS** update `CHANGELOG.md` for user-visible changes (`feat`, `fix`, `perf`)
- **NEVER** add a changelog entry for a `docs` commit. The changelog records
  code changes -- what the software now does differently. Checking in a spec,
  plan, or tasks file changes no behavior and gets no entry, however
  significant the spec. Same for `chore`, `build`, `test` and `ci` unless a
  user would notice the result.
- `[Unreleased]` means "on master, not yet released", not one feature's staging
  area. It will hold work from several features at once, so do not retitle it
  after any one of them.
- **ALWAYS** update `README.md` when features, test counts, or roadmap items change

On a feature branch, prefer cheap, targeted validation per commit (compile the
touched project, run the narrowest relevant tests) and defer the full suite to
the pre-merge gate.

### Validation Suites for Significant Changes
- Any significant changes to the **assembler** or **CPU emulator** implementation
  require running both extended validation suites before committing:
  - **Dormann**: `scripts/RunDormannTest.ps1`, Klaus Dormann 6502 functional test
  - **Harte**: `scripts/RunHarteTests.ps1 -SkipGenerate`, Tom Harte SingleStepTests
- These suites validate end-to-end correctness beyond the unit test suite
- "Significant" includes: refactors, new instructions, addressing mode changes,
  assembler directive changes, expression evaluation changes, and binary output changes
- **CPU or instruction-set changes must run Harte at FULL depth**, 10,000
  vectors per opcode, not the 200-vector set checked into the repo. The full
  set lives in `%LOCALAPPDATA%\Casso\HarteTests` and is often renamed to
  `HarteTests.off` to keep the ordinary suite fast, so rename it back before
  starting CPU work. The runner prints which depth it ran. See
  [docs/testing.md](../docs/testing.md)
- **Changes to what a guest reads require the scenario suite**:
  `scripts/RunTests.ps1 -Build -Scenario`. It boots real DOS 3.3 and ProDOS
  guests over images the code under test produced, which is the only check
  that answers whether a real operating system agrees with us. A tokenizer
  compared against its own detokenizer, or a disk read back through the
  writer's own understanding, agrees with itself perfectly while being wrong.
  Trigger areas, all of which the suite exercises directly:
  - `CassoEmuCore/Devices/Disk/` -- the nibblization layer, the DOS 3.3 and
    ProDOS volumes and skeletons, `BlankDiskBuilder`, `DirectBootBuilder`,
    `DiskCommandRunner`, `DiskImageStore`, `VolumeImage`
  - `CassoEmuCore/Devices/Disk2Controller` -- the drive and its sequencer
  - `CassoCore/ApplesoftTokenizer` -- the bytes Applesoft itself would store
- **The scenario suite is not run by CI and nothing else triggers it**, which
  is the whole reason this rule exists. CI runs `UnitTest.dll` only, and no
  task or hook invokes the scenario binary. Six cases once sat in the unit
  suite logging SKIPPED and returning, which the runner reported as passes:
  they announced success while checking nothing, on every machine without the
  disks. Running it is one command and about nine seconds in Release.
- **The disks it needs are fetched for you.** `-Scenario` runs
  `scripts/FetchStockDisks.ps1` first, which downloads the DOS 3.3 System
  Master and ProDOS Users Disk only when this machine has neither a copy under
  `Disks/Apple/` nor one in the emulator's cache. Both are gitignored. A fetch
  that cannot supply a missing disk stops the run rather than letting it
  proceed half-armed.
- **If a required suite could not run, state that in the summary rather than
  omitting it.** An unrun suite reported as nothing is indistinguishable from
  a suite that passed, which is the failure "Degraded Operation Must Be
  Observable" exists to prevent. Give the suite and the reason.

## User-Facing Prose

Applies to every word a user can read. That includes string literals in the
source: help text, error messages, verdicts, dialog and menu text, tooltips,
and anything else that reaches a screen. It also covers `CHANGELOG.md`,
`README.md`, docs and commit messages. Sitting inside a `.cpp` file exempts
nothing; most of the offenders below were string literals. The style gate
checks none of this, so it is on you.

It applies to what you write in chat as well. These are habits, not a filter
applied to artifacts at the end, and the ones below are corrected most often
in conversation.

- **Write the way a programmer speaks.** Plain, ordinary developer English.
  No literary or precious constructions, no aphoristic fragments after a
  colon or dash, no rhetorical inversions, and no addressing the reader as
  "you" in a CHANGELOG.
- **Never use "name", "named" or "naming" as the verb for specifying
  something.** This is the most persistent offender by a wide margin. Not
  "a source now names `--as65` or `--merlin`", "refused by name", "the
  dialect is named, not guessed", "an image you name". Use **specify, pass,
  give, list, report, include**, or recast the sentence. Ordinary uses are
  fine and always were: a file name, a volume name, naming a variable.
- **No conversational or anthropomorphic verbs.** Flags, tools and disks do
  not say, ask, speak, want, know, wonder, spell or shape anything. Not "or
  say which with `--type`", "`--bootable` and `--boot` ask for different
  disks", "`--logical` speaks the numbering catalogs use". Also out:
  "answered" and "is answered with", and "grammar" where a reader wants
  "command-line parsing".

  **This covers HEADINGS and chat, which is where it keeps surviving.** A
  write, a change, an option or a build has no wants, intentions or knowledge.
  Not "the change stated what it wanted" (2026-08-31, a section heading in
  chat), "what the write means", "the flag knows". Describe the mechanism:
  "the write specified reload or restart", "run with --on-change".
- **Do not state the obvious as though it were profound.** "There is no
  default to guess with", "blocks have only one numbering, so there is
  nothing to state". Cut the sentence.
- **Dashes abut the text they join** (`word--word`), never spaced. Most
  dashes are better rewritten away.
- **Error messages take a fixed shape.** Line 1 is a short categorical
  label, not a sentence about the input. Line 2 onward states the rule as
  complete, capitalized, punctuated sentences with serial commas.

      Error: illegal volume name
             ProDOS volume names are 1-15 characters, starting with a
             letter, and can include letters, digits, and periods.

  Not `Error: 1BAD is not a name ProDOS can put on a volume` followed by a
  lowercase fragment.
- **CHANGELOG entries are terse and stand alone.** One or two lines giving
  the user-visible effect, then stop; mechanism, numbers and the
  wrong-then-right arc belong in the commit message. Group aggressively
  instead of enumerating, put GitHub references first (`GH #115: ...`), and
  never cross-reference another entry, because entries get skimmed and
  reordered. Version headings are the one place wordplay is wanted.
- **When I rewrite your prose, that version is the model.** Match it and
  stop proposing improvements to it.

## Commit Messages

- Use [Conventional Commits](https://www.conventionalcommits.org/) format: `type(scope): description`
- **Scope is always required**: never omit it
- Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`, `ci`, `build`
- Examples:
  - `feat(cpu): implement PHA/PLA stack operations`
  - `fix(ops): correct ShiftLeft dispatch to use ASL not ROL`
  - `test(adc): add signed overflow edge cases`

## Branching and Merging

- **NEVER squash on merge.** All branch merges to `master` must use
  `--no-ff` to preserve commit history.
- **Worktree branch names must match the spec-kit pattern `NNN-name`.**
  `EnterWorktree` prefixes the branch it creates with `worktree-`, and
  `.specify/scripts/powershell/check-prerequisites.ps1` rejects anything that
  does not start with three digits, so a worktree entered the obvious way
  leaves every speckit workflow refusing to run. Rename the branch to bare
  `NNN-name` after creating the worktree.
- **`.specify/feature.json` is per-checkout state, not a deliverable.** It names
  the active spec, so two concurrent sessions each want it pointing somewhere
  different. Repoint it in your worktree, keep it out of the merge to `master`,
  and beware `git add -A`, which sweeps it into a commit silently. The
  `CLAUDE.md` active-spec block has the same hazard: do not flip it from a
  feature branch while another session owns it.

## Workspace Hygiene

- **Always clean up diagnostic artifacts.** When debugging produces
  log files, trace dumps, stderr captures, or any other stray files
  in the working tree, remove them explicitly when the debugging
  session ends.
- **Do NOT add stray-file patterns to `.gitignore`.** The user
  prefers stray files to surface in `git status` as a visible
  reminder. Silencing them with `.gitignore` defeats that signal.
- The only `.gitignore`d disk images are the two Apple-owned masters the
  scenario suite reads, `Disks/Apple/dos33-master.dsk` and
  `Disks/Apple/prodos-users.dsk`. Disks we author belong in the repo.

## Shell and Terminal Rules

- **ALL** terminal windows use PowerShell, not CMD
- **ALWAYS** format commands for PowerShell syntax
- **When a count or a listing IS the claim, print the total separately from the
  sample.** `.Count` the unsliced collection first, then show the first N.
  Truncation and exhaustion look identical in tool output, and the default shape
  of most tooling is to truncate, so a number read off a cut-off listing becomes
  a confident false claim. This has produced several: a `Select-Object -First 5`
  turned six symbols into "four", a `head` on a grep made a file look absent when
  the real hits were below the cut, and a byte-order bug printed the same wrong
  header for every file and was reported as nonsense data rather than recognized
  as a bug in the reader. Before a number goes into a commit message, a spec, or
  a claim to the user, re-derive it from the whole collection. If output is
  genuinely truncated, say the total is unknown rather than reporting what is
  visible.

## Security Rules

- **NEVER** download or execute external binaries, no `.exe`, `.dll`, `.com`, or other executables from any source
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
  bad code; never at the user.
- **Chat only.** This applies to interactive replies. Commit messages,
  code comments, CHANGELOG entries, README content, and other artifacts
  stay neutral and professional.
