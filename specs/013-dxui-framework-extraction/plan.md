# Implementation Plan: Dxui — Reusable DirectX UI Framework Extracted from Casso

**Branch**: `013-dxui-framework-extraction` | **Date**: 2026-03-19 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/013-dxui-framework-extraction/spec.md`

## Summary

Extract Casso's in-house Direct3D 11 / Direct2D / DirectWrite UI layer (today wholly contained in `Casso/Ui/`) into a new sibling static-library project, **`Dxui`**, with the canonical `Dxui*` / `IDxui*` prefix discipline and a single umbrella header (`Dxui.h`) that owns the project's system-header surface. Migrate Casso's UI (`UiShell`, chrome, settings, debug panels, dialogs) to compose against the new framework so that user-visible behaviour is visually equivalent to the pre-migration build by manual sign-off. Three load-bearing capabilities ride along: a `DxuiHostWindow` that subsumes the four existing copies of `WM_NCCALCSIZE`/`WM_NCHITTEST` plumbing, a `DxuiPopupHost` that fixes the long-standing dropdown-clipping bug, and `IDxuiPainter` / `IDxuiTextRenderer` interfaces that let `UnitTest.dll` exercise widgets without a real D3D11 device.

The work is structured as an 11-phase mechanical migration; each phase ends with a green build and a passing test suite, and each phase is independently mergeable.

## Technical Context

**Language/Version**: C++ `stdcpplatest` (MSVC v145, Visual Studio 2026)
**Primary Dependencies**: Windows SDK only — Direct3D 11, Direct2D, DirectWrite, DXGI 1.3, DirectComposition, Windows Imaging Component, WRL (`Microsoft::WRL::ComPtr`). No third-party libraries are introduced; existing approved entries in the constitution allowlist are unaffected.
**Storage**: N/A (UI framework; consumers persist their own state)
**Testing**: Microsoft Native C++ Unit Test Framework (`UnitTest/`), augmented by mock `IDxuiPainter` / `IDxuiTextRenderer` recorders that let widget tests run without a D3D device. Tests must be deterministic and free of file system, registry, network, and real system-API calls per constitution §II.
**Target Platform**: Windows 10 (custom-NC fallback, no Mica / rounded corners) and Windows 11 (rounded corners, Mica, snap-layouts), x64 and ARM64, Debug and Release.
**Project Type**: Desktop application (Casso) consuming an internal C++ static library (Dxui).
**Performance Goals**: UI thread paint pass ≤ 16 ms (60 Hz). Popup open ≤ 1 frame after anchor click. Emulator viewport bounds-change propagation completes inside one layout/paint cycle so the next emulator frame is sized correctly (FR-093 + risk mitigation in spec).
**Constraints**: Single-threaded UI (FR-083 — no internal locking; `std::future` shared state set on the UI thread inside the message loop). Layouts public API in DIPs only; per-paint scaling via `DxuiDpiScaler` (FR-022, FR-082). All public string parameters `std::wstring` (FR-080). Public Dxui headers may include project headers only via quotes; angle-bracket includes live exclusively in `Pch.h` (consumers) and `Dxui.h` (the library umbrella) (FR-004, FR-007).
**Scale/Scope**: ~30 widget/page/dialog files migrate; `Casso/Ui/Settings/SettingsPanel.cpp` must shrink ≥ 40 % from 2,179 lines (SC-003); six per-page fan-out overrides must reach zero across Settings, debug panels, and dialogs (SC-004); four copies of `WM_NCCALCSIZE`/`WM_NCHITTEST` plumbing must collapse to one (SC-010).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Compliance | Notes |
|-----------|------------|-------|
| **I. Code Quality** | ✅ | All new Dxui source obeys EHM (`HRESULT hr = S_OK;` / `Error:` / single exit), 5-blank-line top-level spacing, 3-blank-line var-block separator, column-aligned declarations, `Dxui*` Hungarian-free type names with `s_k*` for file-scope statics, `Dip` suffix on DIP values, no magic numbers, no angle-bracket includes outside `Pch.h` and `Dxui.h` (FR-004/FR-007 — explicitly amended into copilot-instructions.md in Phase 1, see Complexity Tracking). Function-call spacing (`fn (arg)` / `fn()`) and cast spacing (`(float) x`) enforced on all new files. |
| **II. Testing Discipline** | ✅ | New `Dxui*Tests.cpp` files land in `UnitTest/`. SC-006 lists required coverage: `DxuiPanel` fan-out, `DxuiFocusManager` (reading-order + spatial), each layout policy, `DxuiPopupHost` placement/flip/dismiss, `DxuiHostWindow` NC classification. SC-007 mandates D3D-free tests via mock `IDxuiPainter` / `IDxuiTextRenderer` — directly satisfies the "no real system services" isolation rule. No file-system / registry / network / system-API access in the new tests. |
| **III. UX Consistency** | ✅ | Manual no-regression sign-off is an explicit acceptance gate (User Story 2, SC-011). No CLI surface change (Dxui has none). `CassoCli` deliberately does not reference Dxui (FR-003) so its CLI surface is unaffected. |
| **IV. Performance** | ✅ | No new allocations on the paint hot path beyond what the existing UI already does; `D3DRenderer` resizes only when `OnBoundsChanged` reports a real change (FR-030). Popup pool (initial 3, grow on demand — FR-055) prevents per-open HWND/swap-chain churn. |
| **V. Simplicity & Maintainability** | ✅ | Reduces complexity on net: four NC handlers → one; per-page fan-out boilerplate → zero; SettingsPanel.cpp shrinks ≥ 40 %. New surface is justified by user-visible bug fixes (popup clipping) and a testability gap (no headless widget tests today). No speculative APIs (`Adopt`-ownership, `Hidden` visibility, UIA provider, cross-platform) — all explicitly deferred. |

**Gate**: PASS. One amendment to non-binding guidance documentation (copilot-instructions.md angle-bracket rule) is recorded under Complexity Tracking; it does not modify the constitution.

## Project Structure

### Documentation (this feature)

```text
specs/013-dxui-framework-extraction/
├── plan.md              # This file
├── spec.md              # Feature spec (frozen post-clarify)
├── research.md          # Phase 0 — design decisions + alternatives
├── data-model.md        # Phase 1 — type relationships, ownership, lifecycles
├── quickstart.md        # Phase 1 — "build Dxui, write a widget, write a test"
├── contracts/           # /speckit.plan Phase 1 — public header sketches
│   ├── DxuiDialog.h.md
│   ├── IDxuiControl.h.md
│   ├── IDxuiLayout.h.md
│   ├── IDxuiTheme.h.md
│   ├── IDxuiPainter.h.md
│   ├── IDxuiTextRenderer.h.md
│   ├── IDxuiViewportInputSink.h.md
│   └── DxuiHostWindow.h.md
└── tasks.md             # Phase 2 output — generated by /speckit.tasks (NOT created here)
```

### Source Code (repository root)

```text
Casso.sln
├── CassoCore/                              (unchanged — does not link Dxui)
├── CassoEmuCore/                           (unchanged — does not link Dxui)
├── CassoCli/                               (unchanged — does NOT reference Dxui per FR-003)
├── Dxui/                                   (NEW — StaticLibrary, v145, x64/ARM64, Debug/Release)
│   ├── Dxui.vcxproj
│   ├── Pch.h / Pch.cpp                     # internal PCH; mirrors Dxui.h system-header set
│   ├── Dxui.h                              # ⭐ public umbrella — sole angle-bracket chokepoint
│   ├── Core/
│   │   ├── IDxuiControl.h
│   │   ├── DxuiPanel.h / .cpp              # container base, Add<T>/Remove/Clear, auto fan-out
│   │   ├── DxuiFocusManager.h / .cpp       # reading-order tab + spatial arrows + scopes
│   │   ├── DxuiEvents.h                    # DxuiMouseEvent, DxuiKeyEvent
│   │   ├── DxuiViewport.h / .cpp           # non-Dxui content placeholder (emulator host)
│   │   ├── IDxuiViewportInputSink.h
│   │   ├── DxuiHitTester.h / .cpp          # moved from Casso/Ui/HitTester
│   │   ├── DxuiInput.h / .cpp              # moved from Casso/Ui/UiInput
│   │   ├── DxuiAnimation.h / .cpp          # moved from Casso/Ui/Animation
│   │   ├── DxuiDpiScaler.h                 # moved from Casso/Ui/DpiScaler (header-only)
│   │   └── DxuiTitleBarHitTest.h / .cpp    # moved from Casso/Ui/TitleBarHitTest
│   ├── Layout/
│   │   ├── IDxuiLayout.h
│   │   ├── DxuiStackLayout.h / .cpp
│   │   ├── DxuiGridLayout.h / .cpp
│   │   ├── DxuiFormLayout.h / .cpp
│   │   ├── DxuiDockLayout.h / .cpp         # ⭐ includes ContainerSizeForFill (inverse fill)
│   │   └── DxuiAbsoluteLayout.h / .cpp
│   ├── Theme/
│   │   ├── IDxuiTheme.h
│   │   ├── DxuiWindowsThemeColors.h / .cpp # moved from Casso/Ui/WindowsThemeColors
│   │   └── DxuiDwm.h / .cpp                # moved from Casso/Ui/Win11DwmHelpers
│   ├── Render/
│   │   ├── IDxuiPainter.h
│   │   ├── DxuiPainter.h / .cpp / .hlsl    # renamed from Casso/Ui/DxUiPainter
│   │   ├── IDxuiTextRenderer.h
│   │   └── DxuiTextRenderer.h / .cpp       # renamed from Casso/Ui/DwriteTextRenderer
│   ├── Widgets/
│   │   ├── DxuiButton.h / .cpp
│   │   ├── DxuiCheckbox.h / .cpp
│   │   ├── DxuiRadio.h / .cpp
│   │   ├── DxuiToggle.h / .cpp
│   │   ├── DxuiSlider.h / .cpp
│   │   ├── DxuiDropdown.h / .cpp           # hosts content via DxuiPopupHost (FR-061)
│   │   ├── DxuiTabStrip.h / .cpp
│   │   ├── DxuiTextInput.h / .cpp
│   │   ├── DxuiLabel.h                     # header-only today; stays that way
│   │   ├── DxuiListView.h / .cpp
│   │   ├── DxuiTreeView.h / .cpp
│   │   ├── DxuiPopupMenu.h / .cpp          # hosts content via DxuiPopupHost
│   │   ├── DxuiTooltip.h / .cpp            # hosts content via DxuiPopupHost
│   │   └── DxuiModalScrim.h / .cpp
│   ├── Win32/
│   │   ├── DxuiHostWindow.h / .cpp         # ⭐ unified custom-NC top-level window
│   │   ├── DxuiCaptionBar.h / .cpp
│   │   ├── DxuiSystemButton.h / .cpp
│   │   ├── DxuiDragRegion.h / .cpp
│   │   ├── DxuiPopupHost.h / .cpp          # ⭐ WS_POPUP + own swap chain, shared device
│   │   └── DxuiDragDropTarget.h / .cpp     # moved from Casso/Ui/DragDropTarget
│   └── Dialog/
│       ├── DxuiDialog.h / .cpp
│       └── DxuiDialogManager.h / .cpp      # stack of modals, std::future<int> Show()
│
├── Casso/                                  (existing exe — references Dxui)
│   ├── Pch.h                               # ⭐ adds #include "Dxui.h" in Phase 1
│   ├── Casso.vcxproj                       # ⭐ Dxui project ref added; DX libs removed in Phase 1
│   └── Ui/                                 (Casso-specific composition retained)
│       ├── UiShell.{h,cpp}                 # becomes root DxuiPanel + DxuiDockLayout (Phase 9)
│       ├── ThemeManager.{h,cpp}
│       ├── ThemeLoader.{h,cpp}
│       ├── AutoMountResolver.{h,cpp}
│       ├── DriveWidgetController.{h,cpp}
│       ├── DriveWidgetState.h
│       ├── IDriveCommandSink.h
│       ├── Disk2DebugPanel*.{h,cpp}        # migrated to DxuiPanel in Phase 11
│       ├── InputDebugPanel*.{h,cpp}        # migrated to DxuiPanel in Phase 11
│       ├── FocusManager.{h,cpp}            # ⭐ DELETED at Phase 6 (replaced by DxuiFocusManager)
│       ├── Layout.{h,cpp}                  # ⭐ DELETED at Phase 6 (replaced by Dxui layouts)
│       ├── Chrome/                         # bands re-host on DxuiHostWindow in Phase 7
│       │   ├── ChromeTheme.h               # ⭐ implements IDxuiTheme from Phase 5
│       │   ├── ChromeMetrics.h
│       │   ├── DriveWidget.{h,cpp}
│       │   ├── DriveLabelTruncation.{h,cpp}
│       │   ├── JoystickToggleButton.{h,cpp}
│       │   ├── LedIndicator.{h,cpp}
│       │   ├── NavLayer.{h,cpp}
│       │   ├── TitleBar.{h,cpp}
│       │   ├── ChromedPanelWindow.{h,cpp}  # ⭐ DELETED at Phase 7
│       │   ├── IChromedPanelContent.h      # ⭐ DELETED at Phase 7
│       │   └── LayoutManager.{h,cpp},      # ⭐ DELETED at Phase 9 (with IEdgeContributor,
│       │     plus IEdgeContributor.h,      #   ICenterLayer, SimpleEdgeContributor)
│       │     ICenterLayer.h,
│       │     SimpleEdgeContributor.{h,cpp}
│       ├── Settings/
│       │   ├── SettingsWindow*.{h,cpp}     # ⭐ DELETED at Phase 7 (uses DxuiHostWindow)
│       │   ├── SettingsPanel.{h,cpp}       # ⭐ shrinks ≥ 40 % through Phases 10-11
│       │   ├── SettingsPanelState.{h,cpp}
│       │   ├── ThemePage.{h,cpp}           # ⭐ first page converted (Phase 10)
│       │   ├── MachinePage.{h,cpp}         # converted in Phase 11
│       │   ├── HardwarePage.{h,cpp}        # converted in Phase 11
│       │   └── DisplayPage.{h,cpp}         # converted in Phase 11
│       └── Dialogs/                        # ⭐ NEW directory created in Phase 11
│           ├── StartupDownloadDialog.{h,cpp}   # rewritten as DxuiDialog
│           └── StandaloneDialog.{h,cpp}        # rewritten as DxuiDialog
│       # ⭐ DELETED at Phase 11 end:
│       #   Casso/Ui/Dialog/DialogDefinition.h
│       #   Casso/Ui/Dialog/DialogLayout.{h,cpp}
│       #   Casso/Ui/Dialog/DialogPrimitive.{h,cpp}
│       #   Casso/Ui/Dialog/DialogPrimitiveRenderer.{h,cpp}
│       #   (old StartupDownloadDialog / StandaloneDialog under Casso/Ui/Dialog/)
│
└── UnitTest/                               (existing test DLL — references Dxui)
    ├── Pch.h                               # ⭐ adds #include "Dxui.h" in Phase 1
    ├── UnitTest.vcxproj                    # ⭐ Dxui project ref added in Phase 1
    ├── (existing CassoCore/CassoEmuCore tests — unchanged)
    └── Dxui/                               # ⭐ NEW — landed incrementally across phases
        ├── MockDxuiPainter.{h,cpp}         # recording IDxuiPainter (Phase 6)
        ├── MockDxuiTextRenderer.{h,cpp}    # recording IDxuiTextRenderer (Phase 6)
        ├── MockDxuiTheme.{h,cpp}           # canned colour/font accessors (Phase 5)
        ├── DxuiPanelTests.cpp              # fan-out, Add/Remove/Clear, visibility (Phase 6)
        ├── DxuiFocusManagerTests.cpp       # tab order, spatial arrows, scopes (Phase 6)
        ├── DxuiStackLayoutTests.cpp
        ├── DxuiGridLayoutTests.cpp
        ├── DxuiFormLayoutTests.cpp
        ├── DxuiDockLayoutTests.cpp         # incl. ContainerSizeForFill (Phase 9)
        ├── DxuiAbsoluteLayoutTests.cpp
        ├── DxuiPopupHostTests.cpp          # placement, flip, dismiss (Phase 8)
        ├── DxuiHostWindowTests.cpp         # NC classification (Phase 7)
        └── DxuiDialogManagerTests.cpp      # modal stack push/pop (Phase 11)
```

**Structure Decision**: Single new sibling static-library project (`Dxui/`) under the existing solution, no per-feature subprojects. Matches the existing `CassoCore` / `CassoEmuCore` factoring pattern. Internal subdirectories (`Core/`, `Layout/`, `Theme/`, `Render/`, `Widgets/`, `Win32/`, `Dialog/`) are organisational only — all public types live in a single library and reach consumers through the umbrella `Dxui.h`.

## /speckit.plan Phase 0 — Outline & Research

See [research.md](./research.md). All clarifications Q1–Q5 from the spec's Clarifications section are already resolved (FR-072, FR-011, FR-034, FR-004/FR-007, FR-083); research.md therefore focuses on **design** decisions, not unknowns:

- Project file shape (StaticLibrary vs DLL, PCH layout, AdditionalIncludeDirectories propagation).
- DX device sharing between `DxuiHostWindow` and `DxuiPopupHost`.
- Geometry-based focus traversal (row-epsilon, spatial nearest-in-direction).
- Mock painter recording strategy.
- Migration ordering rationale (why the 11 phases are in the order they are).

**NEEDS CLARIFICATION**: none. All five spec ambiguities were resolved in the clarify session 2026-03-19.

## /speckit.plan Phase 1 — Design & Contracts

**Outputs**: [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md), updated `.github/copilot-instructions.md` SPECKIT block (this plan's path).

### Contracts (public-header sketches)

Header-only Markdown sketches of the seven load-bearing public interfaces live in `contracts/`. They are not compiled; they exist so the migration phases can land header files mechanically and so reviewers have a single place to assert "yes, the public surface matches the spec." Concrete `.h` files land in `Dxui/Core/`, `Dxui/Layout/`, `Dxui/Render/`, `Dxui/Theme/`, `Dxui/Win32/` during the migration phases below.

### Agent-context update

In /speckit.plan Phase 1 of this command, update `.github/copilot-instructions.md` between the `<!-- SPECKIT START -->` / `<!-- SPECKIT END -->` markers to point at `specs/013-dxui-framework-extraction/plan.md`. The angle-bracket include carve-out described in FR-007 is **not** added here — it lands as a code change in Migration Phase 1 (Scaffolding) so the rule change and the project that needs it ship together.

## Migration Phases — Mapping Spec Phases 1–11 to Concrete Artifacts

Each phase ends with a green build (`scripts\Build.ps1` on x64+ARM64, Debug+Release), a passing test suite (`scripts\RunTests.ps1`), and a clean code-analysis run (`scripts\Build.ps1 -RunCodeAnalysis`). Phases are independently mergeable (Conventional Commits, scope `dxui`).

### Migration Phase 1 — Scaffold `Dxui.vcxproj`

**Goal**: A buildable empty `Dxui` static library that Casso and UnitTest reference, with the umbrella header chokepoint live.

**Create**:

- `Dxui/Dxui.vcxproj` — `ConfigurationType=StaticLibrary`, `PlatformToolset=v145`, `CharacterSet=Unicode`, configurations `Debug|x64`, `Release|x64`, `Debug|ARM64`, `Release|ARM64`. Mirror `CassoCore.vcxproj` layout for PCH, code-analysis, and warning level.
- `Dxui/Pch.h`, `Dxui/Pch.cpp` — internal precompiled header. Includes the same system-header set as `Dxui.h` so Dxui's own `.cpp` files compile against the umbrella.
- `Dxui/Dxui.h` — **the** public chokepoint. Angle-bracket includes only: `<windows.h>`, `<d3d11.h>`, `<d3dcompiler.h>`, `<d2d1.h>`, `<dwrite.h>`, `<dxgi1_3.h>`, `<dcomp.h>`, `<wincodec.h>`, `<wrl/client.h>`, `<future>`, `<functional>`, `<memory>`, `<string>`, `<vector>`, `<cstdint>`, `<cmath>`. No project headers — those reach consumers naturally through Casso's existing includes once the migration moves files into `Dxui/`.

**Modify**:

- `Casso/Casso.vcxproj` — add `ProjectReference` to `Dxui.vcxproj`; add `$(SolutionDir)Dxui` to `AdditionalIncludeDirectories` (all four configurations); **remove** `d3d11.lib;dxgi.lib` from `AdditionalDependencies` (Dxui owns them via FR-006); add `d3dcompiler.lib;d2d1.lib;dwrite.lib;dcomp.lib;windowscodecs.lib` to **Dxui**'s vcxproj (consumers inherit).
- `Casso/Pch.h` — add `#include "Dxui.h"` after the existing block of Casso-internal includes.
- `UnitTest/UnitTest.vcxproj` — add `ProjectReference` to `Dxui.vcxproj`; add `$(SolutionDir)Dxui` to `AdditionalIncludeDirectories`.
- `UnitTest/Pch.h` — add `#include "Dxui.h"`.
- `Casso.sln` — register the new project in both `Project(...)` declarations and `GlobalSection(ProjectConfigurationPlatforms) = postSolution`.
- `.github/copilot-instructions.md` — amend the "NEVER use angle-bracket includes anywhere except `Pch.h`" line to add: "or a library project's umbrella header (currently only `Dxui.h`)". Update the `<!-- SPECKIT START -->` block to point at this plan (also done by Phase 1 of the command).

**Delete**: none.

**Entry criterion**: Solution builds green on master.
**Exit criterion**: `Dxui.vcxproj` builds standalone on all four configurations; `Casso.exe` and `UnitTest.dll` link against Dxui and still build green; `CassoCli.exe` still builds and does **not** reference Dxui; all existing tests pass.
**Verification**: `scripts\Build.ps1 -Configuration Debug -Platform x64`; `scripts\Build.ps1 -Configuration Release -Platform ARM64`; `scripts\RunTests.ps1`; `rg -n 'd3d11\.lib|dxgi\.lib' Casso/Casso.vcxproj` → zero hits.
**Commit**: `build(dxui): scaffold Dxui static library and umbrella header`

### Migration Phase 2 — Move pure-generic files

**Goal**: Mechanically relocate eight files that have no Casso-specific dependencies into `Dxui/Core/` (and `Dxui/Theme/` for the DWM helpers) with the `Dxui` prefix.

**Moves**:

| From | To | Rename |
|------|----|----|
| `Casso/Ui/HitTester.{h,cpp}` | `Dxui/Core/DxuiHitTester.{h,cpp}` | `HitTester` → `DxuiHitTester` |
| `Casso/Ui/UiInput.{h,cpp}` | `Dxui/Core/DxuiInput.{h,cpp}` | `UiInput*` → `Dxui*` |
| `Casso/Ui/Animation.{h,cpp}` | `Dxui/Core/DxuiAnimation.{h,cpp}` | `Animation*` → `DxuiAnimation*` |
| `Casso/Ui/DpiScaler.h` | `Dxui/Core/DxuiDpiScaler.h` | `DpiScaler` → `DxuiDpiScaler` |
| `Casso/Ui/WindowsThemeColors.{h,cpp}` | `Dxui/Theme/DxuiWindowsThemeColors.{h,cpp}` | `WindowsThemeColors` → `DxuiWindowsThemeColors` |
| `Casso/Ui/Win11DwmHelpers.{h,cpp}` | `Dxui/Theme/DxuiDwm.{h,cpp}` | `Win11Dwm*` → `DxuiDwm*` |
| `Casso/Ui/TitleBarHitTest.{h,cpp}` | `Dxui/Core/DxuiTitleBarHitTest.{h,cpp}` | `TitleBarHitTest` → `DxuiTitleBarHitTest` |
| `Casso/Ui/DragDropTarget.{h,cpp}` | `Dxui/Win32/DxuiDragDropTarget.{h,cpp}` | `DragDropTarget` → `DxuiDragDropTarget` |

For each moved `.cpp` file: replace `#include "Pch.h"` first line, retain it; remove any angle-bracket includes (they're now satisfied by `Dxui.h` via `Pch.h`); replace project-header quoted includes (`#include "HitTester.h"`) with the renamed sibling header. Files moved into `Dxui/` use only `#include "Pch.h"` and `#include "<Sibling>.h"`.

**Modify in Casso**: mechanical include-path updates wherever the moved files are referenced; rename usages of moved types to the `Dxui*` form. Per FR-082 — opportunistically rename `Dp`-suffixed identifiers to `Dip` **only in moved files**, not in untouched Casso files.

**Delete**: original files at the old locations.

**Entry criterion**: Phase 1 complete and green.
**Exit criterion**: Build green; tests pass; `rg -n 'HitTester|UiInput|Win11DwmHelpers|WindowsThemeColors' Casso/Ui` returns zero hits.
**Commit**: `refactor(dxui): move generic utilities (HitTester/UiInput/...) into Dxui/Core`

### Migration Phase 3 — Rename + move render facades

**Moves**:

| From | To | Rename |
|------|----|----|
| `Casso/Ui/DxUiPainter.{h,cpp}` + the associated `.hlsl` | `Dxui/Render/DxuiPainter.{h,cpp,hlsl}` | `DxUiPainter` → `DxuiPainter` (single-word prefix) |
| `Casso/Ui/DwriteTextRenderer.{h,cpp}` | `Dxui/Render/DxuiTextRenderer.{h,cpp}` | `DwriteTextRenderer` → `DxuiTextRenderer` |

The `.hlsl` file moves with `DxuiPainter`; update its `vcxproj` reference (Casso's `Casso.vcxproj` loses the shader file; `Dxui.vcxproj` gains it).

`IDxuiPainter` and `IDxuiTextRenderer` interfaces are **not** introduced yet — that happens in Phase 6 alongside `DxuiPanel`. In Phase 3 the concretes are still used directly; the rename is mechanical.

**Entry**: Phase 2 green.
**Exit**: `rg -n 'DxUiPainter|DwriteTextRenderer' Casso UnitTest` returns zero hits in lines we authored (legacy comments excepted only if explicitly preserved). Build + tests green.
**Commit**: `refactor(dxui): rename render facades to DxuiPainter and DxuiTextRenderer`

### Migration Phase 4 — Move widgets

**Moves**: each `Casso/Ui/Widgets/<Foo>.{h,cpp}` → `Dxui/Widgets/Dxui<Foo>.{h,cpp}` with the `Dxui` prefix on every public type.

Widgets in scope (matches FR-060): `Button`, `Checkbox`, `Radio`, `Toggle`, `Slider`, `Dropdown`, `TabStrip`, `TextInput`, `Label` (header-only), `ListView`, `TreeView`, `PopupMenu`, `Tooltip`, `ModalScrim`.

Each widget's `Paint` signature still takes the concrete `DxuiPainter &` / `DxuiTextRenderer &` and Casso's concrete `ChromeTheme` — those abstractions come in Phases 5 and 6. The widget rename is purely mechanical.

**Entry**: Phase 3 green.
**Exit**: `Casso/Ui/Widgets/` is empty; the widget directory is removed; build + tests green; `rg -n 'class\s+(Button|Checkbox|Dropdown|...)\b' Dxui/Widgets` shows the `Dxui`-prefixed names.
**Commit**: `refactor(dxui): move widgets into Dxui/Widgets with Dxui prefix`

### Migration Phase 5 — Introduce `IDxuiTheme`

**Create**: `Dxui/Theme/IDxuiTheme.h` — pure-virtual interface with accessors widgets need (background, foreground, accent, focus ring, disabled foreground, caption colours, body / caption font handles, etc.). See `contracts/IDxuiTheme.h.md` for the candidate accessor list.

**Modify**:

- `Casso/Ui/Chrome/ChromeTheme.h` — derive from `IDxuiTheme`; existing accessors satisfy the interface.
- Every Dxui widget — change `Paint(... ChromeTheme const & ...)` parameter to `Paint(... IDxuiTheme const & ...)`. Concrete `ChromeTheme &` passed at call sites is implicitly upcast.

**Create in UnitTest**: `UnitTest/Dxui/MockDxuiTheme.{h,cpp}` — canned, deterministic accessor responses for headless widget tests.

**Entry**: Phase 4 green.
**Exit**: widgets compile against `IDxuiTheme const &`; `ChromeTheme` implements it; one smoke test in `UnitTest/Dxui/` constructs a `MockDxuiTheme` and feeds it to a `DxuiButton` (no D3D device required at *construction* — paint still needs the real concrete renderer until Phase 6).
**Commit**: `refactor(dxui): introduce IDxuiTheme; widgets paint against the interface`

### Migration Phase 6 — Add framework (`IDxuiControl`, `DxuiPanel`, layouts, focus, render interfaces)

**Goal**: The heart of the framework lands. After this phase the toolkit is consumable end-to-end, even though Casso's chrome / pages don't use it yet.

**Create** in `Dxui/Core/`:

- `IDxuiControl.h` — unified control interface (see `contracts/IDxuiControl.h.md`). Virtuals: `Layout`, `Paint(IDxuiPainter&, IDxuiTextRenderer&, const IDxuiTheme&)`, `OnMouse`, `OnKey`, `OnFocusChanged`, `OnThemeChanged` (default no-op), `Tick(int64_t nowMs)`, `ClassifyHit`, `AccessibleName`, `AccessibleRole`. Concrete-on-base accessors for bounds, visibility (FR-011 — Collapsed mode only), enabled, focusable, parent, child traversal (`ChildCount`/`Child` overridable by containers, defaulted for leaves).
- `DxuiPanel.{h,cpp}` — container base with `Add<T>(args…) → T&`, `Clear()`, `Remove(IDxuiControl *) → bool`, owning `std::vector<std::unique_ptr<IDxuiControl>>`. Auto fan-out for `Paint`, `OnMouse`, `OnKey`, focus collection, theme/DPI broadcast. Holds a `std::unique_ptr<IDxuiLayout> m_layout` (panel owns; per-instance layout state) and re-runs it on child mutation / bounds change.
- `DxuiEvents.h` — `DxuiMouseEvent`, `DxuiKeyEvent` POD-ish structs.
- `DxuiFocusManager.{h,cpp}` — attaches to a `DxuiPanel` root; builds tab order by sorting focusables on `(top / rowEpsilon, left)`; Tab / Shift+Tab / Esc / Enter / Space handling; per-control `tabIndex` override hints; **focus scopes** (push on popup open, pop on dismiss with focus restored); spatial arrow navigation choosing the nearest focusable in the requested direction using `Bounds()`. Replaces `Casso/Ui/FocusManager`.

**Create** in `Dxui/Layout/`:

- `IDxuiLayout.h` — `Arrange(parent, children)`; `Measure` defaults to `{0,0}`.
- `DxuiStackLayout.{h,cpp}`, `DxuiGridLayout.{h,cpp}`, `DxuiFormLayout.{h,cpp}`, `DxuiAbsoluteLayout.{h,cpp}`. (`DxuiDockLayout` lands in Phase 9 to keep this phase mergeable in isolation; Stack/Grid/Form/Absolute cover the proof-of-concept page in Phase 10.)

**Create** in `Dxui/Render/`:

- `IDxuiPainter.h` — pure-virtual HRESULT-returning methods `BeginFrame`, `EndFrame`, `FillRect`, `StrokeRect`, `FillRounded`, `StrokeRounded`, `FillGradient`, `OutlineRect`, `FillCircleApprox`, `DrawImage`, `PushClip`, and `PopClip` per the contract. (concrete `DxuiPainter` derives in the same phase).
- `IDxuiTextRenderer.h` — `Measure(...) -> HRESULT` and `DrawText(...) -> HRESULT`; fonts come from `IDxuiTheme` accessors such as `BodyFont()`, with no renderer `Font()` accessor.

**Modify**: `DxuiPainter` and `DxuiTextRenderer` derive from the new interfaces (existing method bodies unchanged); all widgets retype their parameters to the interface form.

**Delete**: `Casso/Ui/FocusManager.{h,cpp}`, `Casso/Ui/Layout.{h,cpp}` (after Casso pages briefly bridge to the new framework — see Phase 10).

**Create** in `UnitTest/Dxui/`:

- `MockDxuiPainter.{h,cpp}` — recorder that captures call sequences (op kind, rect, colour, etc.) into a `std::vector<RecordedCall>` for assertions. **No D3D device created.**
- `MockDxuiTextRenderer.{h,cpp}` — recorder + canned text metrics.
- `DxuiPanelTests.cpp` — exercises `Add<T>` / `Remove` / `Clear`, paint fan-out order, input fan-out front-to-back, visibility collapse behaviour.
- `DxuiFocusManagerTests.cpp` — reading-order tab, row-epsilon, spatial arrows, focus-scope push/pop with restore, `tabIndex` overrides.
- `DxuiStackLayoutTests.cpp`, `DxuiGridLayoutTests.cpp`, `DxuiFormLayoutTests.cpp`, `DxuiAbsoluteLayoutTests.cpp`.

**Entry**: Phase 5 green.
**Exit**: New tests pass; existing Casso code paths still compile (Casso continues to use the old `Layout` until Phase 9; `FocusManager` deletion blocks until Phase 10's ThemePage POC picks up `DxuiFocusManager`, so this phase's `Casso/Ui/FocusManager` deletion is deferred — verify by `rg -n FocusManager Casso/Ui` having zero hits at Phase 10 close).
**Commit**: `feat(dxui): add IDxuiControl, DxuiPanel, layouts, focus manager, render interfaces`

### Migration Phase 7 — Host window unification

**Goal**: Land `DxuiHostWindow` and migrate Casso's top-level windows to it.

**Create** in `Dxui/Win32/`:

- `DxuiHostWindow.{h,cpp}` — see `contracts/DxuiHostWindow.h.md`. Owns HWND + DXGI swap chain + root `DxuiPanel`. Handles `WM_NCCALCSIZE`, `WM_NCHITTEST` (eight resize edges + front-to-back tree walk via `ClassifyHit`), `WM_NCLBUTTONDOWN/UP`, `WM_NCMOUSEMOVE` / `WM_NCMOUSELEAVE`, `WM_DPICHANGED`, `WM_SETTINGCHANGE` / `WM_THEMECHANGED` / `WM_DWMCOLORIZATIONCOLORCHANGED`. `CreateParams` covers borderless / resizable / rounded / dark / backdrop / `resizeBorderDip`. Snap-layouts via correct `HTMAXBUTTON` return for `DxuiSystemButton` classified `MaxButton`.
- `DxuiCaptionBar.{h,cpp}` — returns `DxuiHitTestKind::Caption` for blank areas; children may override.
- `DxuiSystemButton.{h,cpp}` — `MinButton` / `MaxButton` / `CloseButton`, Win11-style glyphs, wires `SW_MINIMIZE` / `SC_MAXIMIZE` / `SC_RESTORE` / `WM_CLOSE`.
- `DxuiDragRegion.{h,cpp}` — invisible caption-filler.

**Migrate**:

1. Casso's main `Window` (`Casso\Window.cpp:277` dispatch and `Casso\EmulatorShell.cpp:4035` actual `OnNcCalcSize` implementation) → host on `DxuiHostWindow`.
2. `Casso\Ui\Chrome\ChromedPanelWindow.cpp:451` → host on `DxuiHostWindow`; **delete** the file and `IChromedPanelContent.h` once no consumers remain.
3. `Casso\Ui\Settings\SettingsWindow.cpp:408` → host on `DxuiHostWindow`; **delete** when the conversion completes.
4. (`DialogPrimitive` waits until Phase 11.)

**Create in UnitTest**: `UnitTest/Dxui/DxuiHostWindowTests.cpp` — NC classification only (no real HWND; uses a test harness that feeds synthetic `WM_NCHITTEST` coordinates and asserts the kind returned).

**Entry**: Phase 6 green.
**Exit**: Three of the four NC plumbing copies gone; the fourth (`DialogPrimitive`) remains for now. `grep WM_NCCALCSIZE` is allowed only in `Dxui/Win32/` and `Casso/Ui/Dialog/DialogPrimitive.cpp` until Phase 11. Win11 snap-layouts visibly works on main window, chromed panels, and Settings (SC-009 partial).
**Commit**: `feat(dxui): unify custom-NC top-level windows on DxuiHostWindow`

### Migration Phase 8 — Popup hosting

**Goal**: Land `DxuiPopupHost` + the pool; migrate `DxuiDropdown` / `DxuiTooltip` / `DxuiPopupMenu` onto it.

**Create**:

- `Dxui/Win32/DxuiPopupHost.{h,cpp}` — `WS_POPUP | WS_EX_NOACTIVATE` (`WS_EX_TRANSPARENT | WS_EX_LAYERED` for tooltips), own DXGI swap chain sharing the parent `ID3D11Device`. `ShowParams`: ownerHwnd, anchorRectScreen, placement (`Below`/`Above`/`Right`/`Left`/`AtCursor`), flip-if-offscreen flag, dismiss policy (`OnClickOutside`/`OnClickAnywhere`/`OnPointerLeave`/`Manual`), input policy (`Interactive`/`PassThrough`), shadow flag, `std::unique_ptr<DxuiPanel> content`. `Show()` returns `std::future<int>` (set on UI thread per FR-083). Owner-chain tracking for cascading submenus; `MonitorFromRect` for offscreen flipping; `WM_DPICHANGED_BEFOREPARENT` handling plus host forwarding from every active pooled popup; focus-scope push/pop via `DxuiFocusManager`.
- Pool in `DxuiHostWindow` — initial 3 instances, grow on demand, with debug-only `PopupHits()` / `PopupMisses()` counters (FR-055).

**Modify**: `DxuiDropdown`, `DxuiPopupMenu`, `DxuiTooltip` — replace whatever in-window clipping path they use today with `DxuiPopupHost`. (Inspect during the phase; FR-061.)

**Create in UnitTest**: `DxuiPopupHostTests.cpp` — placement / flip / dismiss policies, owner-chain (cascading submenus), focus-scope push/pop with restore.

**Entry**: Phase 7 green.
**Exit**: User Story 3 acceptance test passes — open a dropdown ~20 px from the bottom of the Settings window and confirm the menu opens upward or extends across the parent edge, no clipping (SC-008). Popup pool reuses instances (assert via debug-build instrumentation counter).
**Commit**: `feat(dxui): add DxuiPopupHost with pool; fix dropdown clipping`

### Migration Phase 9 — DxuiViewport + DxuiDockLayout; retire legacy edge layout

**Create**:

- `Dxui/Core/DxuiViewport.{h,cpp}` — see `contracts/IDxuiControl.h.md` plus FR-030 / FR-034. Configurable size policy (Fixed/Preferred/Fill), `SetConsumesInput(bool)` (default false), `SetInputSink(IDxuiViewportInputSink *)`, `OnBoundsChanged(RECT)` callback.
- `Dxui/Core/IDxuiViewportInputSink.h` — see `contracts/IDxuiViewportInputSink.h.md`.
- `Dxui/Layout/DxuiDockLayout.{h,cpp}` — `Top` / `Bottom` / `Left` / `Right` / `Fill` anchors plus the inverse `ContainerSizeForFill` used by the emulator to size the Apple ][ pixel grid from the inside out (FR-021, FR-093, SC-013).

**Modify**:

- `Casso/Ui/UiShell.{h,cpp}` — main shell becomes a root `DxuiPanel` with a `DxuiDockLayout` (chrome bands docked Top/Bottom + `DxuiViewport` filling middle).
- Whatever currently calls `ClientSizeForFramebuffer` — switch to `DxuiDockLayout::ContainerSizeForFill`. The `D3DRenderer` (Casso side) subscribes to `DxuiViewport::OnBoundsChanged` and resizes its render target only when bounds actually change.
- Casso implements `IDxuiViewportInputSink` (routes to `EmulatorShell` / Apple ][ keyboard controller).

**Delete**:

- `Casso/Ui/Chrome/LayoutManager.{h,cpp}`, `Casso/Ui/Chrome/IEdgeContributor.h`, `Casso/Ui/Chrome/ICenterLayer.h`, `Casso/Ui/Chrome/SimpleEdgeContributor.{h,cpp}` (FR-094).

**Create in UnitTest**: `DxuiDockLayoutTests.cpp` — anchors, fill, and `ContainerSizeForFill` (inverse). At least one test feeds a synthetic viewport-bounds change and asserts the renderer subscriber sees it exactly once.

**Entry**: Phase 8 green.
**Exit**: Viewport resize sizes the Apple ][ grid correctly at startup, after a window resize, and after a DPI change; `LayoutManager` family deleted; `D3DRenderer` no longer thrashes on bounds equal to the previous bounds. The Klaus Dormann and Tom Harte CPU validation suites are **not** required here (UI-only refactor — see SC-012, which we only re-verify at Phase 11 close as a belt-and-suspenders gate).
**Commit**: `refactor(dxui): replace edge-layout with DxuiDockLayout and DxuiViewport`

### Migration Phase 10 — Convert `ThemePage` (proof of concept)

**Goal**: Validate the declarative-layout + auto-fan-out + focus-manager story on the smallest page.

**Modify**: `Casso/Ui/Settings/ThemePage.{h,cpp}` — derives from `DxuiPanel`; uses `DxuiFormLayout`; deletes the per-page `OnLButtonDown` / `OnLButtonUp` / `OnMouseHover` / `OnKey` / `Paint` / `CollectFocusables` overrides (FR-097, SC-004).

**Modify** (bridge): `Casso/Ui/Settings/SettingsPanel.{h,cpp}` — accept a `DxuiPanel`-based page alongside the legacy-style pages until Phase 11 finishes. Document the bridge as temporary in code comments.

**Track**: LOC delta on `ThemePage` and the bridge cost in `SettingsPanel`. If the per-page reduction is materially below the 40 % target extrapolation, sound the alarm before doing the bulk conversion (spec risk: "`SettingsPanel.cpp` 40 % reduction is a target, not guaranteed").

**Entry**: Phase 9 green.
**Exit**: `ThemePage` works in the running app, no fan-out overrides remain in it, focus / tab order / arrow nav behave correctly, theme changes propagate.
**Commit**: `refactor(casso/ui): convert ThemePage to DxuiPanel + DxuiFormLayout`

### Migration Phase 11 — Convert remaining pages, debug panels, dialogs; delete DialogPrimitive

**Modify**:

- `Casso/Ui/Settings/MachinePage.{h,cpp}`, `HardwarePage.{h,cpp}`, `DisplayPage.{h,cpp}` — same treatment as `ThemePage`.
- `Casso/Ui/Disk2DebugPanel*.{h,cpp}`, `InputDebugPanel*.{h,cpp}` — derive from `DxuiPanel`; delete fan-out overrides.

**Create**:

- `Dxui/Dialog/DxuiDialog.{h,cpp}` — `DxuiPanel`-based dialog: `DxuiCaptionBar` (title + close), content panel (consumer-populated), optional `DxuiDockLayout`-bottom button row.
- `Dxui/Dialog/DxuiDialogManager.{h,cpp}` — `std::future<int> Show(std::unique_ptr<DxuiDialog> dialog, ShowParams params)`; **stack of modal dialogs**, `EnableWindow(FALSE)` on previous top, owner HWND set to previous top so Win32 handles activation/z-order; opt-in `DxuiModalScrim` flag default-off (FR-072).
- `Casso/Ui/Dialogs/StartupDownloadDialog.{h,cpp}` and `StandaloneDialog.{h,cpp}` — rewritten as `DxuiDialog`-based panels.

**Delete**:

- `Casso/Ui/Dialog/DialogDefinition.h`
- `Casso/Ui/Dialog/DialogLayout.{h,cpp}`
- `Casso/Ui/Dialog/DialogPrimitive.{h,cpp}`
- `Casso/Ui/Dialog/DialogPrimitiveRenderer.{h,cpp}`
- The old `Casso/Ui/Dialog/StartupDownloadDialog.{h,cpp}` and `StandaloneDialog.{h,cpp}` (replaced under `Casso/Ui/Dialogs/`).
- The `Casso/Ui/Dialog/` directory once empty.

**Create in UnitTest**: `DxuiDialogManagerTests.cpp` — stack push/pop with the download-then-error nested-modal scenario from the Q2 clarification (use `MockHwnd` / a `DxuiDialogManager` seam — see research.md for the test seam design).

**Entry**: Phase 10 green; `ThemePage` validated.
**Exit (release gate for this command's planning intent — release gate for the entire feature)**:

- `SettingsPanel.cpp` ≤ 1,307 lines (SC-003).
- `rg -n 'OnLButtonDown|OnLButtonUp|OnMouseHover|OnKey|^void\s+\w+::Paint\b|CollectFocusables' Casso/Ui/Settings Casso/Ui/Disk2DebugPanel Casso/Ui/InputDebugPanel Casso/Ui/Dialogs` → zero hits (SC-004).
- `rg -n 'WM_NCCALCSIZE' Casso/` → zero hits; matches exist **only** in `Dxui/Win32/` (SC-010).
- `rg -n 'DialogPrimitive|DialogDefinition|DialogLayout' Casso/` → zero hits.
- Klaus Dormann (`scripts/RunDormannTest.ps1`) and Tom Harte (`scripts/RunHarteTests.ps1 -SkipGenerate`) suites both pass (SC-012 — belt-and-suspenders even though this is UI-only).
- All user-visible behaviours (theme, settings persistence, drag-drop disk mount, emulator viewport resize, snap-layouts on all three top-level windows) match the pre-migration build under manual side-by-side comparison at 100 % / 150 % / 200 % DPI on Win10 + Win11 (SC-011).

**Commit**: `refactor(casso/ui): migrate remaining pages, debug panels, dialogs to Dxui; delete DialogPrimitive`

> **Phase numbering note**: Migration phases are numbered as integers 1–11 throughout this plan. The former host-window and popup-host insertion points are now Phase 7 and Phase 8 respectively; subsequent phases shift to 9, 10, and 11.

## Testing Strategy

### Test pyramid

1. **Headless widget unit tests** — the bulk of new coverage. Construct widgets with mock `IDxuiPainter` / `IDxuiTextRenderer` / `IDxuiTheme`, dispatch synthetic `DxuiMouseEvent` / `DxuiKeyEvent`, assert on recorded paint calls and on widget state. Zero D3D device, zero HWND. Land incrementally per phase per the file list above.
2. **Layout tests** — pure-function-style: build a panel with synthetic child rects, run `IDxuiLayout::Arrange`, assert resulting bounds. No painter required.
3. **NC classification tests** — `DxuiHostWindow` exposes a testable `ClassifyHitForTest(POINT)` seam (no real HWND) so `WM_NCHITTEST` mapping can be exercised in unit tests.
4. **Popup placement tests** — `DxuiPopupHost` exposes a testable `ComputePlacementForTest(anchorRectScreen, monitorRect, preferred)` seam.
5. **Dialog stack tests** — `DxuiDialogManager` exposes a `PushForTest` / `PopForTest` seam that drives the modal stack without real HWNDs; production code calls the real path.
6. **Existing Casso/CassoCore/CassoEmuCore tests** — must continue to pass at every phase gate (SC-005).
7. **Validation suites** — Klaus Dormann + Tom Harte at Phase 11 close (SC-012). Not run mid-migration because this is a UI-only refactor and the CPU/emulator code is untouched.
8. **Manual visual parity** — side-by-side screenshots at 100 % / 150 % / 200 % DPI on Win10 and Win11, gated at Phase 7 (chrome land) and Phase 11 (final) per SC-011.

### Mock infrastructure (lands in Phase 6)

`UnitTest/Dxui/MockDxuiPainter.{h,cpp}` records:

```cpp
struct RecordedPaintCall
{
    enum class Kind { Clear, FillRect, StrokeRect, RoundedRect, Line, Image, PushClip, PopClip };
    Kind     kind;
    RECT     rect;
    UINT32   colorArgb;
    float    strokeWidthDip;
    // …additional shape-specific fields kept POD-friendly for value comparisons
};
```

Tests assert on `painter.Calls()` shape and order — no fuzziness, no pixel comparison. `MockDxuiTextRenderer` returns canned `SIZE` metrics for measure calls so layouts are deterministic.

Per constitution §II: zero file-system / registry / network / system-API access in these tests. `DxuiHostWindow` and `DxuiPopupHost` tests must never call `CreateWindowEx` — they exercise the pure-function seams listed above.

## Risk Register

(Verbatim from spec §Risks, plus plan-level additions.)

| # | Risk | Likelihood | Impact | Phase | Mitigation |
|---|------|------------|--------|-------|------------|
| R1 | DX device sharing across HWNDs (`DxuiPopupHost` shares parent `ID3D11Device`, owns its own composition swap chain) | Med | High (crashes / invisible popups) | 8 | Create the device with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`; use `CreateSwapChainForComposition` + DirectComposition visual for popups; explicit owner-chain tracking; popup pool owned by `DxuiHostWindow`; dedicated `DxuiPopupHostTests` for lifecycle. |
| R2 | Custom-NC subtleties Win10 vs Win11 (rounded corners, Mica, snap-layouts) | Med | Med (visual gap) | 7 | `DxuiDwm` encapsulates version detection; eyeball-validate on both OSes at Phase 7 close and at Phase 11 close. |
| R3 | Reading-order tab differs subtly from hand-numbered `focusId` | High | Low (focus order shifts) | 8, 9 | Per-control `tabIndex` override; per-page focus-order review at conversion. |
| R4 | Manual chrome parity drifts during theme-interface port | Med | Med (visual regression) | 7, 10 | Side-by-side screenshots at 100/150/200 % DPI; `ChromeTheme` is amended-in-place rather than replaced, so colour values stay in one location. |
| R5 | `SettingsPanel.cpp` 40 % reduction is a target, not guaranteed | Med | Low (spec miss) | 10 | Measure LOC after Phase 10 (`ThemePage`); extrapolate; flag explicitly before Phase 11 bulk conversion if overrun likely. |
| R6 | A regression in Phase 7 (host window) blocks every later phase | Low | High | 7 | Each phase ends green and is independently mergeable; phases land as separate PRs, not one mega-PR. |
| R7 | `DxuiViewport::OnBoundsChanged` fires after the next emulator frame, producing one mis-sized frame | Low | Low (one-frame artefact) | 9 | Layout pass completes before paint pass; renderer subscribes and resizes lazily on first paint after bounds change. |
| R8 | Phase 1's angle-bracket carve-out breaks the discipline check or surprises reviewers | Low | Low | 1 | The amendment to `.github/copilot-instructions.md` lands in the same commit as the `Dxui.h` umbrella, with the carve-out scoped to "library project's umbrella header (currently only `Dxui.h`)". |
| R9 | `std::future<int>` returned by popup/dialog `Show` is awaited on a worker thread that then touches Dxui | Low | High (crash from UI-thread-only assumption — FR-083) | 8, 11 | Document FR-083 prominently in `Dxui.h` header doc comment; debug-build assertion in `DxuiHostWindow` message-pump entry that records the UI thread ID and re-asserts on every public Dxui API entry. |
| R10 | Mock painter call-set diverges from real painter API, hiding regressions | Med | Med | 6 → ongoing | `IDxuiPainter` interface methods are the **only** painter API widgets call; mock and concrete derive from the same interface so divergence is a compile error. |

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Amend `.github/copilot-instructions.md` angle-bracket rule to permit library umbrella header (`Dxui.h`) as a second chokepoint alongside `Pch.h` | Without an umbrella, every Dxui public header would have to angle-include its own system dependencies (`<d3d11.h>`, `<wrl/client.h>`, `<future>`, …), which violates the "system headers belong in `Pch.h`" rule in a different way and forces consumers to know Dxui's internal dependency set | (a) Consumer `Pch.h` enumerates Dxui's system deps directly — couples consumers to Dxui internals, breaks if Dxui adds a system dep, easy to forget on a new consumer. (b) Each Dxui public header angle-includes — same problem multiplied per-header, plus contradicts the existing rule even more loudly. The umbrella is the smallest viable carve-out. Documented in FR-004 and FR-007 with explicit consumer obligation (`#include "Dxui.h"` in their own `Pch.h`). |
| Two parallel focus-manager implementations briefly coexist (`Casso/Ui/FocusManager` until Phase 10 + `Dxui/Core/DxuiFocusManager` from Phase 6) | Phase 6 lands the framework; deleting `Casso/Ui/FocusManager` requires every Casso consumer of focus to migrate, which happens incrementally Phases 10–11 | Single-shot replacement = one giant PR = violates the "each phase ends green and is independently mergeable" gate (spec risk R6). Brief duplication is the cost of the staged migration. |

---

## Post-Phase-1 Status

- **Branch**: `013-dxui-framework-extraction`
- **Plan path**: `specs/013-dxui-framework-extraction/plan.md`
- **Artifacts generated by this command**:
  - `plan.md` (this file)
  - `research.md`
  - `data-model.md`
  - `quickstart.md`
  - `contracts/IDxuiControl.h.md`, `contracts/IDxuiLayout.h.md`, `contracts/IDxuiTheme.h.md`, `contracts/IDxuiPainter.h.md`, `contracts/IDxuiTextRenderer.h.md`, `contracts/IDxuiViewportInputSink.h.md`, `contracts/DxuiHostWindow.h.md`
  - `.github/copilot-instructions.md` — SPECKIT block updated to point at this plan
- **Constitution Check (re-evaluated post-design)**: PASS. No new violations surfaced during contract drafting. The two entries under Complexity Tracking are unchanged.
- **Next command**: `/speckit.tasks` to expand the 11 migration phases into per-file, per-test concrete tasks.
