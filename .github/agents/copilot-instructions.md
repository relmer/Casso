# TCDir Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-04-23

## Active Technologies
- C++ (stdcpplatest / MSVC v145+) + Windows SDK, STL only (no third-party libraries) (004-tree-view)
- N/A (filesystem enumeration, no persistent storage) (004-tree-view)
- C++ /std:c++latest (MSVC v145+) + Windows SDK only (Console API, Shell API, Process API, TlHelp32) (005-powershell-aliases)
- User's PowerShell profile files (text files on disk) (005-powershell-aliases)
- C++ (stdcpplatest, MSVC v145+) + Windows SDK, STL only — no third-party libraries (006-config-file-support)
- Single flat file (`%USERPROFILE%\.tcdirconfig`), UTF-8 with optional BOM (006-config-file-support)
- C++ (stdcpplatest, MSVC v145 / Visual Studio 2026) + STL only (no third-party libraries) (001-assembler)
- N/A (in-memory processing; CLI does file I/O for input/output) (001-assembler)

- stdcpplatest (MSVC v145+) + Windows SDK (+ `gdi32.lib` for font detection), STL only — no third-party libraries (003-file-icons)

## Project Structure

```text
src/
tests/
```

## Commands

# Add commands for stdcpplatest (MSVC v145+)

## Code Style

All coding conventions are defined in [/.github/copilot-instructions.md](/.github/copilot-instructions.md). Key rules:
- EHM macros (CHR, CBR, CWRA, etc.) for HRESULT functions with single `Error:` exit point
- NEVER delete blank lines between constructs, NEVER break column alignment
- Preserve exact indentation; match existing whitespace
- `pch.h` must be the first #include in every .cpp file
- Quoted includes only (no angle brackets except in pch.h)
- Prefer `unique_ptr` for exclusive ownership
- Use VS Code build tasks, not direct msbuild

## Recent Changes
- 001-assembler: Added C++ (stdcpplatest, MSVC v145 / Visual Studio 2026) + STL only (no third-party libraries)
- 009-variable-width-columns: Added C++ (stdcpplatest, MSVC v145+) + Windows SDK, STL only
- 008-ellipsize-targets: Added C++ (stdcpplatest, MSVC v145+) + Windows SDK, STL only


<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
