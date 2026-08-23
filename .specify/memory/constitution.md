<!--
================================================================================
SYNC IMPACT REPORT
================================================================================
Version change: 1.9.0 -> 1.10.0 (MINOR -- Principle VI now states that the
  exe/lib split is a testability line and NOT a platform boundary)
Modified principles:
  - VI. Thin Executable, Testable Core: replaced the "Irreducible Platform
    Edge Only" bullet, which listed file/registry I/O, clipboard and dialogs
    as things that stay in the executable. That reading let any code touching
    a platform API claim exemption from the Testability Litmus in the bullet
    directly above it, which was already NON-NEGOTIABLE. The two bullets
    contradicted each other and the weaker one was winning.
    Now: the criterion is whether UnitTest can drive it, full stop. Calling
    Win32 is not a reason to live in the exe. What stays is only what cannot
    exist without the process -- for a console app, an entry point that does
    nothing but call a core entry function and return what it returns; for a
    GUI, that plus the HWND, its message pump, and the device objects.
    Added an Evidence paragraph recording what the old reading cost.
Modified sections: N/A
Added sections: N/A
Removed sections: N/A
Templates requiring updates:
  ✅ plan-template.md - Constitution Check still aligned
  ✅ spec-template.md - No template change required
  ✅ tasks-template.md - No template change required
Follow-up TODOs: GitHub issue #85 tracks extracting the executables that
  predate this. Casso.exe is the remaining offender; CassoCli.exe was
  reduced from 3,639 lines to 57 under this reading and is the worked
  example.

WHY MINOR AND NOT MAJOR: this makes previously-compliant code non-compliant,
  which reads as a backward-incompatible redefinition. It is filed as MINOR
  because the Testability Litmus already said this and already carried
  NON-NEGOTIABLE; the removed bullet was an exemption that contradicted it.
  Resolving a contradiction in favor of the stronger existing rule is a
  clarification of what the principle always meant. Reclassify if the owner
  reads it the other way.
================================================================================

================================================================================
SYNC IMPACT REPORT (PRIOR)
================================================================================
Version change: 1.8.0 -> 1.9.0 (MINOR -- scoped the dependency allowlist to
  shipped material and added a lighter rule for test fixtures)
Modified principles: N/A
Modified sections:
  - Technology Constraints / Dependencies: stated that the Approved
    Third-Party Dependencies allowlist governs material that SHIPS
    (compiled into, linked into, or distributed alongside a released
    binary), and that test fixtures are not dependencies. Fixtures need
    no allowlist entry, no per-file accounting, and no amendment; a
    non-permissively licensed fixture needs a sidecar LICENSE file
    covering its directory, and a fixture whose license forbids
    modification must be read-only to its tests.
Added sections: N/A
Removed sections: N/A
Templates requiring updates:
  ✅ plan-template.md - Constitution Check still aligned
  ✅ spec-template.md - No template change required
  ✅ tasks-template.md - No template change required
Follow-up TODOs: None. Resolves the deferred D1 finding in
  specs/019-assembler-dialects: UnitTest/Fixtures/Merlin and
  UnitTest/Fixtures/Disks are compliant under this rule, and each now
  carries a LICENSE file.
================================================================================

================================================================================
SYNC IMPACT REPORT (PRIOR)
================================================================================
Version change: 1.7.0 -> 1.8.0 (MINOR -- added Core Principle VI: Thin
  Executable, Testable Core)
Modified principles: N/A
Modified sections:
  - Core Principles: added "VI. Thin Executable, Testable Core
    (NON-NEGOTIABLE)" -- the application executable MUST be a trivially thin
    shim over a rich core library that both the exe and the UnitTest project
    link, so essentially all logic is unit-testable. Reinforces the existing
    II. Testing Discipline / Test Isolation rule (mock all system state).
Added sections: Core Principles / VI. Thin Executable, Testable Core
Removed sections: N/A
Templates requiring updates:
  ✅ plan-template.md - Constitution Check still aligned
  ✅ spec-template.md - No template change required
  ✅ tasks-template.md - No template change required
Follow-up TODOs: None
================================================================================

================================================================================
SYNC IMPACT REPORT (PRIOR)
================================================================================
Version change: 1.6.0 -> 1.7.0 (MINOR -- materially expanded Code Quality
  formatting/structure guidance with two new normative rules)
Modified principles: I. Code Quality (NON-NEGOTIABLE) -- added two bullets
Modified sections:
  - Core Principles / I. Code Quality: added "No Anonymous Namespaces"
    (file-local constants are file-scope `static constexpr`; helpers are
    class `static` members, not free functions) and "Blank Line After
    Closing Brace" (a `}` is followed by a blank line except a do-while
    tail, a following `else`, or another `}`).
Added sections: N/A
Removed sections: N/A
Templates requiring updates:
  ✅ plan-template.md - Constitution Check still aligned
  ✅ spec-template.md - No template change required
  ✅ tasks-template.md - No template change required
Follow-up TODOs: None
================================================================================

================================================================================
SYNC IMPACT REPORT (PRIOR)
================================================================================
Version change: 1.5.0 -> 1.6.0 (MINOR -- materially changed Tech Constraints
  by removing an approved third-party dependency)
Modified principles: N/A
Modified sections:
  - Technology Constraints / Approved Third-Party Dependencies: removed the
    HTML/CSS-style UI framework row that was added in 1.5.0. The chrome
    surface is being rebuilt against the native Direct3D 11 / Direct2D +
    DirectWrite stack, so the framework is no longer needed and its
    vendored copy under `External/` has been deleted. Reverses the
    spec-007 allowlist addition recorded in the 1.5.0 sync impact report.
Added sections: N/A
Removed sections: N/A
Templates requiring updates:
  ✅ plan-template.md - Constitution Check still aligned
  ✅ spec-template.md - No template change required
  ✅ tasks-template.md - No template change required
Follow-up TODOs: None
================================================================================

================================================================================
SYNC IMPACT REPORT (PRIOR)
================================================================================
Version change: 1.4.0 -> 1.5.0 (MINOR -- materially expanded Tech Constraints
  with an Approved Third-Party Dependencies allowlist)
Modified principles: N/A
Modified sections:
  - Technology Constraints: replaced "Dependencies: Windows SDK, STL only;
    no third-party libraries" with a baseline + explicit allowlist of
    approved third-party dependencies. Each entry is MIT/BSD/Apache/PD,
    source-vendored, with provenance tracking required.
Added sections: Approved Third-Party Dependencies table (under Tech
  Constraints). Initial entries: stb_vorbis.c (already in tree),
  an HTML/CSS-style UI framework, crt-pi, libretro bloom, libretro
  ntsc-adaptive chroma stage (the last four for spec 007-ui-overhaul).
Removed sections: N/A
Templates requiring updates:
  ✅ plan-template.md - Constitution Check still aligned
  ✅ spec-template.md - No template change required
  ✅ tasks-template.md - No template change required
Follow-up TODOs: None
================================================================================
-->

# Casso Constitution

## Core Principles

### I. Code Quality (NON-NEGOTIABLE)

All code MUST adhere to established formatting and structural standards:

- **Formatting Preservation**: NEVER delete blank lines between file-level constructs, NEVER break column alignment in declarations
- **Indentation Exactness**: Preserve exact indentation when modifying code; match existing whitespace precisely
- **Error Handling Macros (EHM)**: Use project EHM patterns (`CHR`, `CBR`, `CWRA`, `BAIL_OUT_IF`, etc.) for all HRESULT-returning functions; use `BAIL_OUT_IF` for success-path early exits. ANY function that contains calls (or other operations) that can fail MUST follow the EHM pattern internally — even if the function does not itself return HRESULT to its caller.
- **No Calls Inside Macro Arguments**: NEVER pass a non-trivial function call as an argument to a macro (including EHM macros, assertions, logging macros). Trivial accessors only — e.g. `.size()`, `.empty()`, `.data()`, `.c_str()`, getter accessors. For anything that can fail or has side effects, capture the result into a local variable first, then pass the variable to the macro.
- **Single Exit Point**: Functions returning HRESULT MUST have exactly one exit point via the `Error:` label; NEVER use direct `goto Error`; NEVER use early returns — always use EHM macros
- **Avoid Nesting**: Use EHM macros to flatten deeply nested conditional logic instead of stacking `if`/`else` blocks. Functions SHOULD have at most 1-2 levels of indentation beyond the EHM pattern; 3 is the absolute maximum. When indentation grows, extract inner logic into a helper function.
- **Variable Declarations at Top of Scope**: Declare ALL variables at the top of their enclosing scope block. Do NOT define variables in the middle of code. (This is independent of and supplemental to the C++ language allowing mid-block declarations.)
- **No Unnecessary Scope Blocks**: Do NOT introduce `{ ... }` blocks that are not required by control flow or lifetime semantics. Scope blocks must serve a purpose (loop body, conditional body, RAII lifetime, etc.).
- **Function Comments in .cpp Only**: Every function MUST have a function-level comment block in the .cpp implementation file. Function-level comments MUST NOT appear in the .h header file. Header files document only declarations and types, not implementation behavior.
- **Function Spacing**: NEVER insert a space between a function name and an empty argument list — write `func()`, not `func ()`. ALWAYS insert a space (or more, for column alignment) between a function name and a non-empty argument list, and between any keyword that takes parens (`if`, `for`, `while`, `switch`, `return`, `sizeof`, etc.) and the opening paren — write `func (a, b)`, `if (x)`, `return (value)`.
- **Cast Spacing**: ALWAYS insert a space between a C-style cast and its operand — write `(float) std::numbers::pi`, `(int) value`, `(Word) addr`; NEVER write `(float)value`.
- **Smart Pointers**: Prefer `unique_ptr` for exclusive ownership, `shared_ptr` when shared ownership is required
- **No Anonymous Namespaces**: NEVER use `namespace {}`. Declare file-local constants as file-scope `static constexpr` (`s_k` Hungarian naming); put file-local helpers as class `static` members. Strongly prefer class members over free/global functions in general — a free function requires a very convincing justification.
- **Blank Line After Closing Brace**: A closing `}` MUST be followed by a blank line, EXCEPT when it ends a do-while (`} while (...)`), is followed by `else`, or is immediately followed by another closing `}`. Guard clauses and `switch`/`case` blocks are NOT exceptions.

**Rationale**: Consistent formatting enables efficient code review and reduces merge conflicts. EHM patterns ensure predictable error handling, resource cleanup, and flat readable code.

### II. Testing Discipline

All production code MUST have corresponding unit tests:

- **Unit Test Framework**: Use Microsoft C++ Unit Test Framework (CppUnitTestFramework)
- **Test Coverage**: Every public function and significant code path MUST be covered by tests
- **Test Independence**: Each test MUST be independently runnable and MUST NOT depend on execution order
- **Test Isolation (NON-NEGOTIABLE)**: Unit tests MUST NEVER read, write, or depend on any actual system state. ALL system services MUST be mocked or abstracted behind interfaces:
  - **File system**: No reading or writing actual files on disk — use in-memory data, synthetic byte buffers, or mock I/O interfaces
  - **Registry**: No accessing the Windows registry — mock all registry calls
  - **Network**: No real HTTP/socket calls — mock network layers
  - **Process/environment**: No inspecting real processes, environment variables, or console handles — inject mock providers
  - **System APIs**: No calling `SHGetKnownFolderPath`, `CreateToolhelp32Snapshot`, `OpenProcessToken`, `CreateFileW`, `DeviceIoControl`, etc. directly in unit tests — inject dependencies through interfaces so tests can substitute mocks or use synthetic data
  
  If a module uses system APIs, the testable logic MUST be factored into pure functions that accept data (not handles or OS resources) so tests can supply synthetic inputs. Tests MUST be deterministic and repeatable regardless of the machine or user running them.
- **Build Verification**: Tests MUST pass before any merge or release; use VS Code tasks (`Build + Test Debug/Release`)
- **Test Organization**: Tests reside in the `UnitTest/` project, grouped by component (e.g., `CommandLineTests.cpp`, `ConfigTests.cpp`)

**Rationale**: Automated tests catch regressions early and serve as living documentation of expected behavior.

### III. User Experience Consistency

All user-facing output MUST follow established patterns:

- **CLI Syntax**: Use standard `--flag` long options and `-f` short options; subcommand style (`CassoCli assemble`, `CassoCli run`)
- **Error Messages**: Errors go to stderr; user-facing messages MUST be clear, actionable, and consistent in tone
- **Help System**: All features MUST be documented in `--help` output
- **Backward Compatibility**: Existing command-line behavior MUST NOT change without explicit user notification

**Rationale**: Users rely on consistent behavior; breaking established patterns creates confusion and reduces trust.

### IV. Performance Requirements

Performance considerations apply where relevant:

- **Avoid Waste**: Minimize unnecessary memory allocations; prefer stack allocation and move semantics in hot paths
- **Reasonable Scale**: Assembler and emulator should handle typical 6502 programs (< 10K lines, 64 KB address space) without noticeable delay
- **Resource Efficiency**: Prefer simple, direct implementations over over-engineered abstractions

**Rationale**: Casso is a development tool; responsiveness matters for developer experience.

### V. Simplicity & Maintainability

Complexity MUST be justified:

- **YAGNI**: Do not implement features "just in case"; implement when needed
- **Single Responsibility**: Each class/module SHOULD have one clear purpose
- **Self-Documenting Code**: Prefer clear naming over comments; add comments only for non-obvious "why" explanations
- **Minimal Dependencies**: Avoid external libraries unless they provide substantial value
- **File Scope**: Modify only files explicitly required; ask before making "helpful" changes to unrelated files
- **Function Size & Structure (NON-NEGOTIABLE)**: Functions MUST be kept short — ideally under 50 lines, 100 lines at absolute maximum. Aggressively factor out helper functions that do just one thing. Avoid excessive nesting by extracting inner logic into separate helper functions rather than adding more indentation levels. If a function requires more than 2-3 levels of indentation beyond the EHM pattern, extract that logic into its own function.

**Rationale**: Simple code is easier to understand, test, and maintain over time.

### VI. Thin Executable, Testable Core (NON-NEGOTIABLE)

Essentially all logic MUST live in a linked core library, not the application executable:

- **Trivially Thin Shell**: The application `.exe` MUST be an empty shell over a core entry function — `CliMain` for the console tool, its GUI equivalent for the emulator. Emulation, parsing, rendering, device models, persistence, and lifecycle/orchestration MUST live in a core static library that BOTH the executable AND the `UnitTest` project link. An executable that contains a decision worth asserting has already failed this.
- **Testability Litmus (NON-NEGOTIABLE)**: Any new logic MUST be reachable and exercised from the `UnitTest` project. If a piece of logic can only be tested by running the `.exe`, it is in the wrong place or the wrong shape (entangled with an `HWND`, device context, COM apartment, or menu id). Factor it into core as data-in/data-out functions or interface seams. The exe carries no test coverage by design, so it MUST carry no logic worth testing.
- **THE CRITERION IS TESTABILITY, NOT A PLATFORM BOUNDARY (NON-NEGOTIABLE)**: The exe/lib line is NOT where the operating system begins. It is where testability ends. "Does this call a platform API?" is the wrong question and MUST NOT be used to justify placement; the only question is "can the `UnitTest` project drive this?" Calling Win32 is not a reason to live in the exe. File I/O behind an interface seam, a registry read, a clipboard round-trip, an image codec over WIC: all are drivable by a test, therefore all belong in core.
- **What Actually Stays**: Only what cannot exist without the process itself. For a console application that is the entry point and nothing else — `main` MUST do no more than call a core entry function and return what it returns. For a GUI application it is the entry point, the `HWND` and its message pump, and the graphics/audio device objects. Everything else, including the code that decides what to draw, what to persist, what to load and what status to exit with, MUST live in core.
- **Do Not Imitate Existing Divergence**: Where an executable has already accreted logic that belongs in core, that is debt to be extracted — NEVER a template for new code. New code follows this principle regardless of the surrounding exe's current state.

**Rationale**: A thin shell over a rich, linked core is the structural precondition for Principle II — thorough unit testing and mocking are only possible when the logic lives where tests can reach it. Untestable code is, most often, merely code placed where tests cannot link to it.

**Evidence**: `CassoCli.exe` held 3,639 lines under the old platform-boundary reading: parsing adapters, every page of help text, artifact writing, the mode runners, the Win32 file layer, and a `main` whose ten-arm dispatch chose which page to print and what to exit with. All of it looked defensible as "the platform edge and the printing around it." None of it was reachable by a test, and two defects lived there undisturbed through a release cycle: the exit statuses every help page documented were never the ones the tool returned, and a bare invocation exited 0 while the comment directly above the code said 1. Both were caught within minutes of the code moving into core. The executable is now 57 lines and its `main` calls `CliMain`. The lesson is that code placed by platform reasoning is not merely untested, it is unobservable — and unobservable code drifts from its own documentation with nothing failing.

## Technology Constraints

**Language/Version**: stdcpplatest (MSVC v145+)
**Build System**: Visual Studio 2026 / MSBuild; VS Code tasks wrap PowerShell scripts
**Target Platforms**: Windows 10/11, x64 and ARM64 architectures
**Testing Framework**: Microsoft C++ Unit Test Framework
**Dependencies**: Windows SDK and C++ STL form the baseline. Additional third-party dependencies are permitted ONLY when explicitly listed in the **Approved Third-Party Dependencies** allowlist below. Each entry MUST be MIT/BSD/Apache/PD-licensed (no copyleft), source-vendored in-tree under `External/` (no package manager, no binary downloads), and accompanied by upstream attribution (tag/SHA, license file, `README.casso.md` recording provenance).

**Scope**: this allowlist governs material that ships — anything compiled into, linked into, or distributed alongside a released binary. That is where a license grant has to be established in detail, because we are redistributing someone else's work to end users under our own release.

**Test fixtures are NOT dependencies** and do not require an allowlist entry, a per-file accounting, or a constitution amendment. They are inputs consumed by the test suite, ship in nothing, and reach no end user. A non-permissive license is acceptable for a fixture where it would not be acceptable for a dependency, and the bar is correspondingly lighter:

- Non-permissively licensed fixtures MUST carry a sidecar license note. One `LICENSE` file per directory covers every file in that directory; it MUST name the license, the attribution the license requires, and where the material came from.
- Where several files share a license, group them in their own subdirectory rather than annotating each file. The directory is the unit.
- Permissively licensed and repo-original fixtures need nothing.
- A fixture whose license forbids modification MUST be treated as read-only by the tests that consume it.

The distinction is deliberate. Establishing detailed permission for every shipped artifact is a real obligation; extending that ceremony to test data buys nothing and discourages using the realistic inputs that make a test suite worth having.

**Approved Third-Party Dependencies**:

| Dependency           | License           | Used By        | Location                          | Purpose                                   |
|----------------------|-------------------|----------------|-----------------------------------|-------------------------------------------|
| `stb_vorbis.c`       | MIT / Public Dom. | CassoCore      | `CassoCore/External/`             | Ogg Vorbis decoding for Disk II audio     |
| `crt-pi`             | MIT               | Casso          | `Casso/Shaders/`                  | CRT scanline post-process (spec 007)      |
| libretro `bloom`     | MIT / Public Dom. | Casso          | `Casso/Shaders/`                  | Phosphor bloom post-process (spec 007)    |
| libretro `ntsc-adaptive` chroma stage | MIT | Casso | `Casso/Shaders/`              | NTSC color bleed post-process (spec 007)  |

Adding a new entry to the allowlist is a constitution amendment (MINOR version bump).
**Build Configurations**: Debug and Release for both x64 and ARM64
**Scripts**: PowerShell 7 (`pwsh`) for build/test automation (`scripts/`)

## Development Workflow

### Tool Preference

When automation tooling exists, prefer it over raw terminal commands:

- **Build/Test**: Use VS Code tasks (`Build + Test Debug/Release`) instead of invoking MSBuild directly
- **Errors**: Use `get_errors` tool instead of parsing compiler output manually
- **File Operations**: Use provided tools (read_file, replace_string_in_file, etc.) over terminal commands when appropriate
- **MCP Servers**: When an MCP server provides relevant functionality, use it instead of scripting equivalents

**Rationale**: Established tooling is tested, consistent, and integrates with the development environment. Raw commands bypass safeguards and create inconsistent workflows.

### Quality Gates

1. **Pre-Commit**: Code MUST compile without errors or warnings in both Debug and Release
2. **Build Verification**: Run `Build + Test` task to ensure all tests pass before considering work complete
3. **Error Checking**: Use `get_errors` tool to verify specific files after modifications
4. **Architecture Coverage**: Verify changes work on both x64 and ARM64 when touching platform-sensitive code
5. **Code Analysis**: Run Code Analysis (`process: Run Code Analysis (current arch)`) before pushing; MUST pass with zero warnings

### Commit Discipline

- **Commit per phase**: During speckit implementation, commit after each completed phase (Setup, Foundational, each User Story, Polish). Do NOT accumulate all phases into a single commit.
- **Conventional Commits**: Use `type(scope): description` format with required scope. See CONTRIBUTING.md for type list.
- **Version.h**: If a version header exists, include it in commits after building.
- **Refs/Closes**: Reference the GitHub issue in commit messages (`Refs #N` or `Closes #N`).

### Change Process

1. Make minimal, surgical edits; show only changed lines with context
2. Preserve all formatting (indentation, alignment, blank lines)
3. Run build task after changes
4. Verify tests pass
5. Check for both compilation errors (C-codes) and warnings

## Governance

This constitution supersedes all ad-hoc practices. All code changes MUST verify compliance with these principles.

**Amendment Process**:
1. Propose change with rationale
2. Document impact on existing code/practices
3. Update constitution version following semantic versioning:
   - MAJOR: Backward-incompatible principle removal or redefinition
   - MINOR: New principle or materially expanded guidance
   - PATCH: Clarifications, wording, non-semantic refinements
4. Update dependent templates if affected

**Compliance Review**: Periodically review codebase against constitution principles; document exceptions with justification.

**Guidance Reference**: See `.github/copilot-instructions.md` for detailed runtime development guidance and code style rules.

**Version**: 1.10.0 | **Ratified**: 2026-01-24 | **Last Amended**: 2026-08-22
