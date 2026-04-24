<!--
================================================================================
SYNC IMPACT REPORT
================================================================================
Version change: N/A → 1.0.0 (initial ratification)
Modified principles: N/A (new document)
Added sections:
  - Core Principles (5 principles)
  - Technology Constraints
  - Development Workflow
  - Governance
Removed sections: N/A
Templates requiring updates:
  ✅ plan-template.md - Constitution Check section aligns with new principles
  ✅ spec-template.md - Requirements/edge cases align with UX consistency principle
  ✅ tasks-template.md - Test phases align with Testing Discipline principle
Follow-up TODOs: None
================================================================================
-->

# My6502 Constitution

## Core Principles

### I. Code Quality (NON-NEGOTIABLE)

All code MUST adhere to established formatting and structural standards:

- **Formatting Preservation**: NEVER delete blank lines between file-level constructs, NEVER break column alignment in declarations
- **Indentation Exactness**: Preserve exact indentation when modifying code; match existing whitespace precisely
- **Error Handling Macros (EHM)**: Use project EHM patterns (`CHR`, `CBR`, `CWRA`, `BAIL_OUT_IF`, etc.) for all HRESULT-returning functions; use `BAIL_OUT_IF` for success-path early exits
- **Single Exit Point**: Functions returning HRESULT MUST have exactly one exit point via the `Error:` label; NEVER use direct `goto Error`; NEVER use early returns — always use EHM macros
- **Avoid Nesting**: Use EHM macros to flatten deeply nested conditional logic instead of stacking `if`/`else` blocks
- **Smart Pointers**: Prefer `unique_ptr` for exclusive ownership, `shared_ptr` when shared ownership is required

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

- **CLI Syntax**: Use standard `--flag` long options and `-f` short options; subcommand style (`My6502 assemble`, `My6502 run`)
- **Error Messages**: Errors go to stderr; user-facing messages MUST be clear, actionable, and consistent in tone
- **Help System**: All features MUST be documented in `--help` output
- **Backward Compatibility**: Existing command-line behavior MUST NOT change without explicit user notification

**Rationale**: Users rely on consistent behavior; breaking established patterns creates confusion and reduces trust.

### IV. Performance Requirements

Performance considerations apply where relevant:

- **Avoid Waste**: Minimize unnecessary memory allocations; prefer stack allocation and move semantics in hot paths
- **Reasonable Scale**: Assembler and emulator should handle typical 6502 programs (< 10K lines, 64 KB address space) without noticeable delay
- **Resource Efficiency**: Prefer simple, direct implementations over over-engineered abstractions

**Rationale**: My6502 is a development tool; responsiveness matters for developer experience.

### V. Simplicity & Maintainability

Complexity MUST be justified:

- **YAGNI**: Do not implement features "just in case"; implement when needed
- **Single Responsibility**: Each class/module SHOULD have one clear purpose
- **Self-Documenting Code**: Prefer clear naming over comments; add comments only for non-obvious "why" explanations
- **Minimal Dependencies**: Avoid external libraries unless they provide substantial value
- **File Scope**: Modify only files explicitly required; ask before making "helpful" changes to unrelated files
- **Function Size & Structure (NON-NEGOTIABLE)**: Functions MUST be kept short — ideally under 50 lines, 100 lines at absolute maximum. Aggressively factor out helper functions that do just one thing. Avoid excessive nesting by extracting inner logic into separate helper functions rather than adding more indentation levels. If a function requires more than 2-3 levels of indentation beyond the EHM pattern, extract that logic into its own function.

**Rationale**: Simple code is easier to understand, test, and maintain over time.

## Technology Constraints

**Language/Version**: stdcpplatest (MSVC v145+)
**Build System**: Visual Studio 2026 / MSBuild; VS Code tasks wrap PowerShell scripts
**Target Platforms**: Windows 10/11, x64 and ARM64 architectures
**Testing Framework**: Microsoft C++ Unit Test Framework
**Dependencies**: Windows SDK, STL only; no third-party libraries
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

**Version**: 1.3.0 | **Ratified**: 2026-01-24 | **Last Amended**: 2026-04-19
