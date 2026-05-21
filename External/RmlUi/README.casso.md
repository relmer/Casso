# RmlUi (vendored)

This directory contains a vendored copy of [RmlUi](https://github.com/mikke89/RmlUi),
an HTML/CSS-style UI library, used by the Casso GUI application for all custom chrome
and the Settings panel (spec 007-ui-overhaul, FR-046).

## Provenance

| Field             | Value                                                                                  |
|-------------------|----------------------------------------------------------------------------------------|
| Upstream          | https://github.com/mikke89/RmlUi                                                       |
| Tag               | `6.2`                                                                                  |
| Commit SHA        | `2230d1a6e8e0848ed87a5761e2a5160b2a175ba4`                                             |
| License           | MIT (see `LICENSE.txt`)                                                                |
| Vendored          | 2026-05-21 (per spec 007-ui-overhaul P1-T1)                                            |
| Approved by       | Casso Constitution v1.5.0, Approved Third-Party Dependencies allowlist                 |

## What was kept vs. trimmed

**Kept** (required to build the static lib used by Casso):

- `Include/Core/` — public Rml headers
- `Include/Debugger/` — debugger panel headers
- `Include/RmlUi/` — convenience umbrella headers
- `Source/Core/` — implementation
- `Source/Debugger/` — debugger panel implementation
- `LICENSE.txt`, `readme.md`, `changelog.md` — upstream documentation and license

**Trimmed** (not needed for the Casso build):

- `Samples/` — upstream demo applications
- `Tests/` — upstream test harness
- `Backends/` — Casso ships its own custom D3D11 backend (`Casso/Ui/RmlBackend_D3D11`)
- `Utilities/`, `CMake/`, `CMakeLists.txt`, `CMakePresets.json` — Casso builds via MSBuild
- `Source/{Lottie,Lua,SVG}` and `Include/{Lottie,Lua,SVG}` — optional language/format
  bindings; Casso uses none of them
- `Dependencies/` (was empty in upstream tag anyway)
- `.github/`, `.appveyor.yml`, `.clang-format`, `.editorconfig`, `.gitignore`,
  `contributing.md` — upstream project plumbing

## Font engine

RmlUi 6.2's default font engine (`Source/Core/FontEngineDefault/`) depends on FreeType.
Casso does **not** vendor FreeType; instead, a custom `Rml::FontEngineInterface`
implementation backed by Windows DirectWrite (DWrite) is provided in
`Casso/Ui/RmlFontEngine_DWrite.{h,cpp}` (spec 007 P3-T2 ancillary).

The bundled `FontEngineDefault/` source files are **excluded** from the static
library compilation via the project's `<ClCompile Exclude=...>` rule.

## How to upgrade

1. `git clone --depth 1 --branch <new-tag> https://github.com/mikke89/RmlUi.git External/RmlUi-new`
2. Inside the new directory, remove `.git` and apply the same trim list above.
3. Replace this directory's contents.
4. Update this file's Provenance table with the new tag + SHA.
5. Bump the dependency entry in `.specify/memory/constitution.md` if a new version
   alters the API surface meaningfully.
6. Rebuild and run the full test suite.

## Constitution compliance

This vendoring complies with the Casso Constitution v1.5.0 Approved Third-Party
Dependencies allowlist:

- MIT-licensed ✅
- Source-vendored in-tree under `External/` ✅
- No package manager (no vcpkg / NuGet / Conan) ✅
- Provenance recorded (tag + SHA, this file) ✅
- Listed by name in `.specify/memory/constitution.md` ✅
