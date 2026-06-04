# Implementation Plan: Dxui — Reusable DirectX UI Framework Extracted from Casso

**Branch**: `013-dxui-framework-extraction` | **Date**: 2026-03-19 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/013-dxui-framework-extraction/spec.md`

## Summary

Extract Casso's in-house Direct3D 11 / Direct2D / DirectWrite UI layer (today wholly contained in `Casso/Ui/`) into a new sibling static-library project, **`Dxui`**, with the canonical `Dxui*` / `IDxui*` prefix discipline and a single umbrella header (`Dxui.h`) that owns the project's system-header surface. Migrate Casso's UI (`UiShell`, chrome, settings, debug panels, dialogs) to compose against the new framework so that user-visible behaviour is visually equivalent to the pre-migration build by manual sign-off. Three load-bearing capabilities ride along: a `DxuiHostWindow` that subsumes the four existing copies of `WM_NCCALCSIZE`/`WM_NCHITTEST` plumbing, a `DxuiPopupHost` that fixes the long-standing dropdown-clipping bug, and `IDxuiPainter` / `IDxuiTextRenderer` interfaces that let `UnitTest.dll` exercise widgets without a real D3D11 device.

The work is structured as a **14-phase** mechanical migration; each phase ends with a green build and a passing test suite, and each phase is independently mergeable. Phases 1–6 are complete; Phase 7 is in progress (scope reduced to framework primitives only — see Phase 7 below). Three additional phases were inserted during Phase 7 execution to absorb scope that surfaced when the chrome / `EmulatorShell` reality met the original plan: Phase 8 (main-window NC delegation via an adopt-HWND shim), Phase 10 (`DxuiMenuBar` + `MainMenu` conversion), and Phase 11 (chrome reshape + `EmulatorShell` restructure). Old Phases 8/9/10/11 shift to 9/12/13/14 respectively. See the "Migration Phases" section for the full enumeration and the spec for FR-098..FR-108 / SC-014..SC-020.

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
│   │   ├── DxuiMenuBar.h / .cpp            # ⭐ horizontal menu bar; submenus via DxuiPopupHost (Phase 10, FR-101/FR-102)
│   │   └── DxuiModalScrim.h / .cpp
│   ├── Win32/
│   │   ├── DxuiHostWindow.h / .cpp         # ⭐ unified custom-NC top-level window
│   │   │                                   #   full-ownership ctor (Phase 7) +
│   │   │                                   #   CreateInAdoptMode / HandleMessage /
│   │   │                                   #   SetHitTestDelegate (Phase 8, FR-098/FR-099)
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
│       ├── UiShell.{h,cpp}                 # becomes root DxuiPanel + DxuiDockLayout (Phase 12)
│       ├── ThemeManager.{h,cpp}
│       ├── ThemeLoader.{h,cpp}
│       ├── AutoMountResolver.{h,cpp}
│       ├── DriveWidgetController.{h,cpp}
│       ├── DriveWidgetState.h
│       ├── IDriveCommandSink.h
│       ├── Disk2DebugPanel*.{h,cpp}        # migrated to DxuiPanel in Phase 14
│       ├── InputDebugPanel*.{h,cpp}        # migrated to DxuiPanel in Phase 14
│       ├── FocusManager.{h,cpp}            # ⭐ DELETED at Phase 6 (replaced by DxuiFocusManager)
│       ├── Layout.{h,cpp}                  # ⭐ DELETED at Phase 6 (replaced by Dxui layouts)
│       ├── Chrome/                         # NC delegation via adopt shim in Phase 8;
│       │   │                               #   chrome controls reshape to IDxuiControl in Phase 11
│       │   ├── ChromeTheme.h               # ⭐ implements IDxuiTheme from Phase 5
│       │   ├── ChromeMetrics.h
│       │   ├── DriveWidget.{h,cpp}
│       │   ├── DriveLabelTruncation.{h,cpp}
│       │   ├── JoystickToggleButton.{h,cpp}
│       │   ├── LedIndicator.{h,cpp}
│       │   ├── NavLayer.{h,cpp}            # ⭐ RENAMED to MainMenu.{h,cpp} in Phase 10 (FR-103)
│       │   ├── TitleBar.{h,cpp}            # ⭐ reshaped to derive from DxuiCaptionBar in Phase 11 (FR-104)
│       │   ├── ChromedPanelWindow.{h,cpp}  # ⭐ reshaped to use DxuiHostWindow in Phase 11 (FR-106)
│       │   ├── IChromedPanelContent.h      # ⭐ DELETED at Phase 11
│       │   └── LayoutManager.{h,cpp},      # ⭐ DELETED at Phase 12 (with IEdgeContributor,
│       │     plus IEdgeContributor.h,      #   ICenterLayer, SimpleEdgeContributor)
│       │     ICenterLayer.h,
│       │     SimpleEdgeContributor.{h,cpp}
│       ├── Settings/
│       │   ├── SettingsWindow*.{h,cpp}     # ⭐ DELETED at Phase 14 (uses DxuiHostWindow)
│       │   ├── SettingsPanel.{h,cpp}       # ⭐ shrinks ≥ 40 % through Phases 10-11
│       │   ├── SettingsPanelState.{h,cpp}
│       │   ├── ThemePage.{h,cpp}           # ⭐ first page converted (Phase 13)
│       │   ├── MachinePage.{h,cpp}         # converted in Phase 14
│       │   ├── HardwarePage.{h,cpp}        # converted in Phase 14
│       │   └── DisplayPage.{h,cpp}         # converted in Phase 14
│       └── Dialogs/                        # ⭐ NEW directory created in Phase 14
│           ├── StartupDownloadDialog.{h,cpp}   # rewritten as DxuiDialog
│           └── StandaloneDialog.{h,cpp}        # rewritten as DxuiDialog
│       # ⭐ DELETED at Phase 14 end:
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
        ├── DxuiDockLayoutTests.cpp         # incl. ContainerSizeForFill (Phase 12)
        ├── DxuiAbsoluteLayoutTests.cpp
        ├── DxuiPopupHostTests.cpp          # placement, flip, dismiss (Phase 9)
        ├── DxuiHostWindowTests.cpp         # NC classification (Phase 7)
        ├── DxuiHostWindowAdoptModeTests.cpp # adopt mode + HandleMessage + hit-test delegate (Phase 8)
        ├── DxuiMenuBarTests.cpp            # alt-keys, hover-after-click, arrows, Escape (Phase 10)
        ├── ChromeControlAdaptersTests.cpp  # reshaped chrome bounds + ClassifyHit (Phase 11)
        └── DxuiDialogManagerTests.cpp      # modal stack push/pop (Phase 14)
```

**Structure Decision**: Single new sibling static-library project (`Dxui/`) under the existing solution, no per-feature subprojects. Matches the existing `CassoCore` / `CassoEmuCore` factoring pattern. Internal subdirectories (`Core/`, `Layout/`, `Theme/`, `Render/`, `Widgets/`, `Win32/`, `Dialog/`) are organisational only — all public types live in a single library and reach consumers through the umbrella `Dxui.h`.

## /speckit.plan Phase 0 — Outline & Research

See [research.md](./research.md). All clarifications Q1–Q5 from the spec's Clarifications section are already resolved (FR-072, FR-011, FR-034, FR-004/FR-007, FR-083); research.md therefore focuses on **design** decisions, not unknowns:

- Project file shape (StaticLibrary vs DLL, PCH layout, AdditionalIncludeDirectories propagation).
- DX device sharing between `DxuiHostWindow` and `DxuiPopupHost`.
- Geometry-based focus traversal (row-epsilon, spatial nearest-in-direction).
- Mock painter recording strategy.
- Migration ordering rationale (why the 14 phases are in the order they are; rationale for the three phases inserted during Phase 7 execution per spec Clarifications 2026-Q-Phase7+).

**NEEDS CLARIFICATION**: none. All five spec ambiguities were resolved in the clarify session 2026-03-19.

## /speckit.plan Phase 1 — Design & Contracts

**Outputs**: [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md), updated `.github/copilot-instructions.md` SPECKIT block (this plan's path).

### Contracts (public-header sketches)

Header-only Markdown sketches of the seven load-bearing public interfaces live in `contracts/`. They are not compiled; they exist so the migration phases can land header files mechanically and so reviewers have a single place to assert "yes, the public surface matches the spec." Concrete `.h` files land in `Dxui/Core/`, `Dxui/Layout/`, `Dxui/Render/`, `Dxui/Theme/`, `Dxui/Win32/` during the migration phases below.

### Agent-context update

In /speckit.plan Phase 1 of this command, update `.github/copilot-instructions.md` between the `<!-- SPECKIT START -->` / `<!-- SPECKIT END -->` markers to point at `specs/013-dxui-framework-extraction/plan.md`. The angle-bracket include carve-out described in FR-007 is **not** added here — it lands as a code change in Migration Phase 1 (Scaffolding) so the rule change and the project that needs it ship together.

## Migration Phases — Mapping Spec Phases 1–14 to Concrete Artifacts

**Status legend**: ✅ complete · 🚧 in progress · ⏳ not started.

Each phase ends with a green build (`scripts\Build.ps1` on x64+ARM64, Debug+Release), a passing test suite (`scripts\RunTests.ps1`), and a clean code-analysis run (`scripts\Build.ps1 -RunCodeAnalysis`). Phases are independently mergeable (Conventional Commits, scope `dxui`).

### Migration Phase 1 — Scaffold `Dxui.vcxproj` ✅

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

### Migration Phase 2 — Move pure-generic files ✅

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

### Migration Phase 3 — Rename + move render facades ✅

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

### Migration Phase 4 — Move widgets ✅

**Moves**: each `Casso/Ui/Widgets/<Foo>.{h,cpp}` → `Dxui/Widgets/Dxui<Foo>.{h,cpp}` with the `Dxui` prefix on every public type.

Widgets in scope (matches FR-060): `Button`, `Checkbox`, `Radio`, `Toggle`, `Slider`, `Dropdown`, `TabStrip`, `TextInput`, `Label` (header-only), `ListView`, `TreeView`, `PopupMenu`, `Tooltip`, `ModalScrim`.

Each widget's `Paint` signature still takes the concrete `DxuiPainter &` / `DxuiTextRenderer &` and Casso's concrete `ChromeTheme` — those abstractions come in Phases 5 and 6. The widget rename is purely mechanical.

**Entry**: Phase 3 green.
**Exit**: `Casso/Ui/Widgets/` is empty; the widget directory is removed; build + tests green; `rg -n 'class\s+(Button|Checkbox|Dropdown|...)\b' Dxui/Widgets` shows the `Dxui`-prefixed names.
**Commit**: `refactor(dxui): move widgets into Dxui/Widgets with Dxui prefix`

### Migration Phase 5 — Introduce `IDxuiTheme` ✅

**Create**: `Dxui/Theme/IDxuiTheme.h` — pure-virtual interface with accessors widgets need (background, foreground, accent, focus ring, disabled foreground, caption colours, body / caption font handles, etc.). See `contracts/IDxuiTheme.h.md` for the candidate accessor list.

**Modify**:

- `Casso/Ui/Chrome/ChromeTheme.h` — derive from `IDxuiTheme`; existing accessors satisfy the interface.
- Every Dxui widget — change `Paint(... ChromeTheme const & ...)` parameter to `Paint(... IDxuiTheme const & ...)`. Concrete `ChromeTheme &` passed at call sites is implicitly upcast.

**Create in UnitTest**: `UnitTest/Dxui/MockDxuiTheme.{h,cpp}` — canned, deterministic accessor responses for headless widget tests.

**Entry**: Phase 4 green.
**Exit**: widgets compile against `IDxuiTheme const &`; `ChromeTheme` implements it; one smoke test in `UnitTest/Dxui/` constructs a `MockDxuiTheme` and feeds it to a `DxuiButton` (no D3D device required at *construction* — paint still needs the real concrete renderer until Phase 6).
**Commit**: `refactor(dxui): introduce IDxuiTheme; widgets paint against the interface`

### Migration Phase 6 — Add framework (`IDxuiControl`, `DxuiPanel`, layouts, focus, render interfaces) ✅

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
**Exit**: New tests pass; existing Casso code paths still compile (Casso continues to use the old `Layout` until Phase 12; `FocusManager` deletion blocks until Phase 13's ThemePage POC picks up `DxuiFocusManager`, so this phase's `Casso/Ui/FocusManager` deletion is deferred — verify by `rg -n FocusManager Casso/Ui` having zero hits at Phase 13 close).
**Commit**: `feat(dxui): add IDxuiControl, DxuiPanel, layouts, focus manager, render interfaces`

### Migration Phase 7 — Host window framework primitives 🚧

**Goal**: Land `DxuiHostWindow` (full-ownership construction path) and its companion chrome primitives as headless framework code. **No Casso consumers migrate in this phase** — NC duplicate count stays at 4. Migration of existing windows is split across Phases 8 (main `Window` via adopt shim), 11 (`ChromedPanelWindow`), and 14 (`SettingsWindow` and `DialogPrimitive`).

**Create** in `Dxui/Win32/`:

- `DxuiHostWindow.{h,cpp}` — see `contracts/DxuiHostWindow.h.md`. Full-ownership mode owns HWND + DXGI swap chain + root `DxuiPanel`. Handles `WM_NCCALCSIZE`, `WM_NCHITTEST` (eight resize edges + front-to-back tree walk via `ClassifyHit`), `WM_NCLBUTTONDOWN/UP`, `WM_NCMOUSEMOVE` / `WM_NCMOUSELEAVE`, `WM_DPICHANGED`, `WM_SETTINGCHANGE` / `WM_THEMECHANGED` / `WM_DWMCOLORIZATIONCOLORCHANGED`. `CreateParams` covers borderless / resizable / rounded / dark / backdrop / `resizeBorderDip`. Snap-layouts via correct `HTMAXBUTTON` return for `DxuiSystemButton` classified `MaxButton`. (Adopt-mode constructor `CreateInAdoptMode` + `SetHitTestDelegate` + public `HandleMessage` forwarder ship in Phase 8 — FR-098 / FR-099.)
- `DxuiCaptionBar.{h,cpp}` — returns `DxuiHitTestKind::Caption` for blank areas; children may override.
- `DxuiSystemButton.{h,cpp}` — `MinButton` / `MaxButton` / `CloseButton`, Win11-style glyphs, wires `SW_MINIMIZE` / `SC_MAXIMIZE` / `SC_RESTORE` / `WM_CLOSE`.
- `DxuiDragRegion.{h,cpp}` — invisible caption-filler.

**Tasks covered**: T064–T069, T071–T073.

**Create in UnitTest**: `UnitTest/Dxui/DxuiHostWindowTests.cpp` — NC classification only (no real HWND; uses a test harness that feeds synthetic `WM_NCHITTEST` coordinates and asserts the kind returned).

**Migrate**: none. (Migration deferred to Phases 8 / 11 / 14.)

**Entry**: Phase 6 green.
**Exit**: `DxuiHostWindow` + companions build clean and pass NC-classification tests; **NC plumbing duplicate count remains 4** (no consumers migrated yet); existing main `Window` / `ChromedPanelWindow` / `SettingsWindow` / `DialogPrimitive` unchanged.
**Commit**: `feat(dxui): add DxuiHostWindow framework primitives (full-ownership path)`

### Migration Phase 8 — Main-window NC delegation via adopt-HWND shim ⏳

**Goal**: Migrate the main `Casso` window off its bespoke NC handling onto `DxuiHostWindow` **without** touching HWND / D3D11 device / swap-chain ownership and **without** forcing the existing `TitleBar` / `DriveWidget` / `NavLayer` chrome to become `IDxuiControl`-shaped yet. Achieved via a second `DxuiHostWindow` construction path that adopts a caller-owned HWND and a hit-test plug-in seam. Closing this phase drops the NC plumbing duplicate count **4 → 3**.

**Modify** in `Dxui/Win32/DxuiHostWindow.{h,cpp}`:

- Add `static std::unique_ptr<DxuiHostWindow> CreateInAdoptMode (HWND existing, const CreateParams & params, …)` (FR-098). In adopt mode `DxuiHostWindow` MUST NOT call `CreateWindowEx` / `DestroyWindow`, MUST NOT own the DXGI swap chain (the caller continues to own its own), and MUST NOT install a `WNDPROC` — the caller forwards messages.
- Add public message forwarder `bool HandleMessage (UINT msg, WPARAM wParam, LPARAM lParam, LRESULT & outResult)` (FR-098). Handles `WM_NCCALCSIZE`, `WM_NCHITTEST`, `WM_NCLBUTTONDOWN/UP`, `WM_NCMOUSEMOVE`, `WM_NCMOUSELEAVE`, `WM_DPICHANGED`, `WM_DPICHANGED_BEFOREPARENT`, `WM_SETTINGCHANGE`, `WM_THEMECHANGED`, `WM_DWMCOLORIZATIONCOLORCHANGED`. Returns `true` (handled, `outResult` populated) or `false` (caller continues).
- Add `void SetHitTestDelegate (std::function<LRESULT(POINT screenPt)> delegate)` (FR-099). `WM_NCHITTEST` consults the delegate first; if it returns `HTNOWHERE`, falls back to the standard tree-walk + resize-edge logic. Allows `EmulatorShell` to keep its bespoke `TitleBar` / `DriveWidget` / `NavLayer` hit classification through this phase.

**Migrate**:

1. `Casso\Window.cpp` (dispatch at `:277`) + `Casso\EmulatorShell.cpp` (`OnNcCalcSize` at `:4035` and adjacent NC handlers) — construct a `DxuiHostWindow` via `CreateInAdoptMode` over the existing HWND. Delete the bespoke `OnNcCalcSize` / `OnNcHitTest` / `OnNcMouseMove` / `OnNcLButton*` / `OnNcMouseLeave` / `OnDpiChanged` / `OnSettingChange` / `OnThemeChanged` / `OnDwmColorizationColorChanged` implementations and forward those `WM_*` messages to `DxuiHostWindow::HandleMessage` from `EmulatorShell`'s window-procedure dispatcher. Move the existing bespoke chrome hit-test classification into a single `std::function<LRESULT(POINT)>` and install via `SetHitTestDelegate`.
2. `EmulatorShell` retains ownership of HWND, D3D11 device, swap chain, viewport, and all chrome (FR-100). `EmulatorShell` is still a `Window` subclass after this phase — that restructure waits for Phase 11.

**Create in UnitTest**: `UnitTest/Dxui/DxuiHostWindowAdoptModeTests.cpp` — adopt-mode behaviours: (a) `CreateInAdoptMode` does not allocate a swap chain, does not call `CreateWindowEx` (assertion via test seam), (b) `HandleMessage` returns `false` for messages it does not handle and `true` with a populated `LRESULT` for messages it does, (c) `SetHitTestDelegate` is consulted first on `WM_NCHITTEST` and short-circuits the tree walk when it returns non-`HTNOWHERE`, (d) tree-walk fallback runs when the delegate returns `HTNOWHERE`.

**Entry**: Phase 7 green; `DxuiHostWindow` framework primitives shipped.
**Exit**:

- NC plumbing duplicate count drops **4 → 3** (main `Window` retired; `ChromedPanelWindow`, `SettingsWindow`, `DialogPrimitive` still bespoke).
- Main window drag, resize (all eight edges), minimise, maximise, restore, close, DPI change (drag between 100 % and 200 % monitors), Win11 snap-layouts flyout still all behave identically to the pre-phase build.
- `rg -n 'WM_NCCALCSIZE|WM_NCHITTEST' Casso\Window.cpp Casso\EmulatorShell.cpp` returns zero hits (forwarding to `DxuiHostWindow::HandleMessage` only).
- Adopt-mode unit tests pass.
- `EmulatorShell` still owns HWND / D3D / swap chain (verified by inspection — no behaviour change to viewport rendering).

**Commit**: `feat(dxui): adopt-HWND shim on DxuiHostWindow; main window delegates NC handling`

### Migration Phase 9 — Popup hosting ⏳ *(was Phase 8)*

**Goal**: Land `DxuiPopupHost` + the pool; migrate `DxuiDropdown` / `DxuiTooltip` / `DxuiPopupMenu` onto it.

**Create**:

- `Dxui/Win32/DxuiPopupHost.{h,cpp}` — `WS_POPUP | WS_EX_NOACTIVATE` (`WS_EX_TRANSPARENT | WS_EX_LAYERED` for tooltips), own DXGI swap chain sharing the parent `ID3D11Device`. `ShowParams`: ownerHwnd, anchorRectScreen, placement (`Below`/`Above`/`Right`/`Left`/`AtCursor`), flip-if-offscreen flag, dismiss policy (`OnClickOutside`/`OnClickAnywhere`/`OnPointerLeave`/`Manual`), input policy (`Interactive`/`PassThrough`), shadow flag, `std::unique_ptr<DxuiPanel> content`. `Show()` returns `std::future<int>` (set on UI thread per FR-083). Owner-chain tracking for cascading submenus; `MonitorFromRect` for offscreen flipping; `WM_DPICHANGED_BEFOREPARENT` handling plus host forwarding from every active pooled popup; focus-scope push/pop via `DxuiFocusManager`.
- Pool in `DxuiHostWindow` — initial 3 instances, grow on demand, with debug-only `PopupHits()` / `PopupMisses()` counters (FR-055).

**Modify**: `DxuiDropdown`, `DxuiPopupMenu`, `DxuiTooltip` — replace whatever in-window clipping path they use today with `DxuiPopupHost`. (Inspect during the phase; FR-061.)

**Create in UnitTest**: `DxuiPopupHostTests.cpp` — placement / flip / dismiss policies, owner-chain (cascading submenus), focus-scope push/pop with restore.

**Entry**: Phase 8 green.
**Exit**: User Story 3 acceptance test passes — open a dropdown ~20 px from the bottom of the Settings window and confirm the menu opens upward or extends across the parent edge, no clipping (SC-008). Popup pool reuses instances (assert via debug-build instrumentation counter).
**Commit**: `feat(dxui): add DxuiPopupHost with pool; fix dropdown clipping`

### Migration Phase 10 — `DxuiMenuBar` + `MainMenu` conversion ⏳ *(new)*

**Goal**: Ship a Win11-style menu bar widget in Dxui and rename Casso's misnamed `NavLayer` to `MainMenu`, rewired through the new widget. Depends on Phase 9 because submenus are rendered via `DxuiPopupHost`.

**Create** in `Dxui/Widgets/`:

- `DxuiMenuBar.{h,cpp}` — horizontal strip of menu items. Each item: display label, alt-letter accelerator character (the `&`-prefixed letter), dispatch callback (`std::function<void()>`), check-query callback (`std::function<bool()>`, optional), enabled-query callback (`std::function<bool()>`, optional), and a `bool checkable` flag. Derives from `IDxuiControl`; renders via `IDxuiPainter` + `IDxuiTextRenderer` + `IDxuiTheme` (FR-101).
- Submenus rendered via `DxuiPopupHost` (Phase 9 prerequisite) reusing the existing `DxuiPopupMenu` content surface (FR-102).
- Win11 UX requirements (FR-102): hover-per-item visual state; click-to-open; alt-letter accelerator routing (`Alt+F` opens File); arrow-key traversal between open submenus (Left / Right while a submenu is open closes it and opens its neighbour); Escape closes the open submenu and returns focus to the bar; once a submenu has been opened by click, hovering a sibling item switches the open submenu without a second click; checkable items reflect live state via the check-query callback.

**Modify / Rename** in `Casso/Ui/Chrome/`:

- Rename `NavLayer.{h,cpp}` → `MainMenu.{h,cpp}`; rename class `NavLayer` → `MainMenu`; remove any `NavMenu` typedef/alias (FR-103).
- `MainMenu` is either a thin wrapper over `DxuiMenuBar` or a populated `DxuiMenuBar` instance owned by `EmulatorShell`, configured with Casso's command set (File / Edit / Machine / Disk / View / Debug / Help). Commands wire dispatch / check-query / enabled-query callbacks straight into `EmulatorShell`'s existing command handlers.
- Update the ~15 `m_navLayer.X()` call sites in `EmulatorShell` (and any stragglers in `Window.cpp` / chrome) to use `m_mainMenu`. No call-site behavioural change beyond the rename + the menu-bar widget driving submenu open / dismiss through `DxuiPopupHost` instead of the legacy in-window popup.

**Create in UnitTest**: `UnitTest/Dxui/DxuiMenuBarTests.cpp` — alt-letter accelerator dispatch (synthetic `Alt+F` opens the File menu, `Alt+E` opens Edit, unmapped key does nothing); hover-after-click switches the open submenu without a second click; Left / Right arrow keys traverse between open submenus; Escape dismisses an open submenu and restores focus to the bar; check-query callback drives the rendered check-mark each frame; enabled-query disables dispatch and visually greys the item. Tests run headless against `MockDxuiPainter` / `MockDxuiTextRenderer` and a stub `DxuiPopupHost` seam.

**Entry**: Phase 9 green (`DxuiPopupHost` available for submenu hosting).
**Exit**:

- `Dxui/Widgets/DxuiMenuBar.{h,cpp}` ships; menu-bar tests pass.
- `rg -n 'NavLayer|NavMenu' Casso/` returns **zero hits** (SC-017).
- `MainMenu` matches pre-migration `NavLayer` UX with no user-perceptible regression: alt-letter accelerators dispatch, hover-to-open works after first click, arrow keys traverse, Escape dismisses, checkable items reflect live state (SC-016).
- File/Edit/Machine/Disk/View/Debug/Help menus open and dispatch all existing commands.

**Commit**: `feat(dxui): add DxuiMenuBar; rename NavLayer to MainMenu`

### Migration Phase 11 — Chrome reshape + `EmulatorShell` restructure ⏳ *(new)*

**Goal**: Reshape Casso's bespoke chrome controls (`TitleBar`, `DriveWidget`, `LedIndicator`, `JoystickToggleButton`) to derive from `IDxuiControl` (or `DxuiCaptionBar` for `TitleBar`); migrate `ChromedPanelWindow` to `DxuiHostWindow`; and restructure `EmulatorShell` so it stops being a `Window` subclass and instead owns a full-ownership `DxuiHostWindow`. After this phase the NC plumbing duplicate count drops **3 → 2** (remaining: `SettingsWindow`, `DialogPrimitive`, both retired in Phase 14) and `EmulatorShell.cpp` shrinks by ≥ 40 %.

**Reshape** in `Casso/Ui/Chrome/`:

- `TitleBar.{h,cpp}` — derive from `DxuiCaptionBar` (or compose one and forward), expose rendering via `IDxuiControl::Paint(IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &)`, report bounds + hit-test via `Bounds()` / `ClassifyHit(POINT)`. Skeuomorphic visual style preserved (FR-104).
- `DriveWidget.{h,cpp}` — derive from `IDxuiControl` with `Paint` / `OnMouse` / `ClassifyHit` on the standard interface; direct member access from `EmulatorShell` replaced with composition under a parent `DxuiPanel` (FR-105).
- `LedIndicator.{h,cpp}` — same treatment (FR-105).
- `JoystickToggleButton.{h,cpp}` — same treatment (FR-105).
- `ChromedPanelWindow.{h,cpp}` — adopt `DxuiHostWindow` (initially adopt-mode if necessary, full-ownership where practical). Collapses one NC plumbing duplicate (FR-106). NC count 3 → 2.

**Restructure** `Casso/EmulatorShell.{h,cpp}`:

- Remove `: public Window` from the `EmulatorShell` declaration (SC-019).
- `EmulatorShell` becomes a content controller that **owns** a full-ownership `DxuiHostWindow` (FR-107). HWND, D3D11 device, and DXGI swap chain ownership transfer to `DxuiHostWindow`.
- `EmulatorShell` wires the root `DxuiPanel` with: `MainMenu` (Phase 10), `TitleBar`, drive widgets, `LedIndicator`, `JoystickToggleButton`, the Apple ][ viewport, and the Apple ][ machine. Wiring is composition under `DxuiHostWindow`'s root panel — no more direct chrome member access from `EmulatorShell` to the controls.
- The hit-test delegate plumbed in Phase 8 is **removed**: now that all chrome derives from `IDxuiControl`, `DxuiHostWindow`'s standard tree-walk classification subsumes the delegate's role.
- Apple ][ viewport rendering reroutes through `DxuiHostWindow`'s swap chain via a minimal `DxuiViewport` surface (FR-107).

**Viewport scope split between Phases 11 and 12**: This phase introduces only the **minimal `DxuiViewport` surface** required to host the Apple ][ swap-chain reroute — i.e. a placeholder `IDxuiControl` that lets `EmulatorShell` install the Apple ][ renderer as the viewport's painter and reports bounds back to that renderer. The **full** `DxuiViewport` (configurable size policy Fixed / Preferred / Fill, `SetConsumesInput(bool)`, `SetInputSink(IDxuiViewportInputSink *)`, `OnBoundsChanged(RECT)` subscriber API), the `IDxuiViewportInputSink` interface, and `DxuiDockLayout` (with `ContainerSizeForFill`) land in Phase 12, which also retires the legacy `Chrome/LayoutManager` family. Rationale: keeping the full viewport + dock-layout work in Phase 12 keeps Phase 11 focused on the chrome / `EmulatorShell` restructure and preserves the original "retire `LayoutManager`" exit boundary of the old Phase 9.

**Create in UnitTest**:

- `UnitTest/Dxui/ChromeControlAdaptersTests.cpp` — bounds + `ClassifyHit` reporting for the reshaped `TitleBar` / `DriveWidget` / `LedIndicator` / `JoystickToggleButton`, plus paint-recording assertions via `MockDxuiPainter` (visual parity verified manually under SC-015; tests cover the contract surface only).

**Entry**: Phase 10 green (`MainMenu` rewired; `NavLayer` gone).
**Exit**:

- `rg -n 'class TitleBar|class DriveWidget|class LedIndicator|class JoystickToggleButton' Casso/Ui/Chrome/` shows each declaration includes `: public IDxuiControl` or `: public DxuiCaptionBar` (SC-014).
- `rg -n 'class EmulatorShell\s*:\s*public\s+Window' Casso/` returns zero hits (SC-019).
- `Casso/EmulatorShell.cpp` ≤ **2,502 lines** (40 % reduction from 4,170) (SC-018).
- NC plumbing duplicate count **3 → 2** (remaining: `SettingsWindow`, `DialogPrimitive`) (SC-020 interim).
- Apple ][ viewport renders correctly through `DxuiHostWindow`'s swap chain at 100 % / 150 % / 200 % DPI, on Win10 and Win11; viewport bounds change → renderer resize cycle works on window resize and DPI change.
- Skeuomorphic chrome visual parity preserved (manual side-by-side per SC-015 / SC-011).
- `ChromedPanelWindow` uses `DxuiHostWindow`.
- Adopt-mode hit-test delegate is no longer installed by `EmulatorShell` (verified by grep on `SetHitTestDelegate` in `Casso/` returning zero hits).

**Commit**: `refactor(casso): reshape chrome to IDxuiControl; EmulatorShell owns DxuiHostWindow`

### Migration Phase 12 — `DxuiViewport` + `DxuiDockLayout`; retire legacy edge layout ⏳ *(was Phase 9)*

**Create**:

- `Dxui/Core/DxuiViewport.{h,cpp}` — **finishes** the minimal viewport surface introduced in Phase 11. Adds full configurable size policy (Fixed/Preferred/Fill), `SetConsumesInput(bool)` (default false), `SetInputSink(IDxuiViewportInputSink *)`, `OnBoundsChanged(RECT)` subscriber API. See `contracts/IDxuiControl.h.md` plus FR-030 / FR-034.
- `Dxui/Core/IDxuiViewportInputSink.h` — see `contracts/IDxuiViewportInputSink.h.md`.
- `Dxui/Layout/DxuiDockLayout.{h,cpp}` — `Top` / `Bottom` / `Left` / `Right` / `Fill` anchors plus the inverse `ContainerSizeForFill` used by the emulator to size the Apple ][ pixel grid from the inside out (FR-021, FR-093, SC-013).

**Modify**:

- `Casso/Ui/UiShell.{h,cpp}` — main shell becomes a root `DxuiPanel` with a `DxuiDockLayout` (chrome bands docked Top/Bottom + `DxuiViewport` filling middle). (`EmulatorShell`'s `DxuiHostWindow` root panel — created in Phase 11 — picks up the full layout here.)
- Whatever currently calls `ClientSizeForFramebuffer` — switch to `DxuiDockLayout::ContainerSizeForFill`. The `D3DRenderer` (Casso side) subscribes to `DxuiViewport::OnBoundsChanged` and resizes its render target only when bounds actually change.
- Casso implements `IDxuiViewportInputSink` (routes to `EmulatorShell` / Apple ][ keyboard controller).

**Delete**:

- `Casso/Ui/Chrome/LayoutManager.{h,cpp}`, `Casso/Ui/Chrome/IEdgeContributor.h`, `Casso/Ui/Chrome/ICenterLayer.h`, `Casso/Ui/Chrome/SimpleEdgeContributor.{h,cpp}` (FR-094).

**Create in UnitTest**: `DxuiDockLayoutTests.cpp` — anchors, fill, and `ContainerSizeForFill` (inverse). At least one test feeds a synthetic viewport-bounds change and asserts the renderer subscriber sees it exactly once.

**Entry**: Phase 11 green.
**Exit**: Viewport resize sizes the Apple ][ grid correctly at startup, after a window resize, and after a DPI change; `LayoutManager` family deleted; `D3DRenderer` no longer thrashes on bounds equal to the previous bounds. The Klaus Dormann and Tom Harte CPU validation suites are **not** required here (UI-only refactor — see SC-012, which we only re-verify at Phase 14 close as a belt-and-suspenders gate).
**Commit**: `refactor(dxui): replace edge-layout with DxuiDockLayout and DxuiViewport`

### Migration Phase 13 — Convert `ThemePage` (proof of concept) ⏳ *(was Phase 10)*

**Goal**: Validate the declarative-layout + auto-fan-out + focus-manager story on the smallest page.

**Modify**: `Casso/Ui/Settings/ThemePage.{h,cpp}` — derives from `DxuiPanel`; uses `DxuiFormLayout`; deletes the per-page `OnLButtonDown` / `OnLButtonUp` / `OnMouseHover` / `OnKey` / `Paint` / `CollectFocusables` overrides (FR-097, SC-004).

**Modify** (bridge): `Casso/Ui/Settings/SettingsPanel.{h,cpp}` — accept a `DxuiPanel`-based page alongside the legacy-style pages until Phase 14 finishes. Document the bridge as temporary in code comments.

**Track**: LOC delta on `ThemePage` and the bridge cost in `SettingsPanel`. If the per-page reduction is materially below the 40 % target extrapolation, sound the alarm before doing the bulk conversion (spec risk: "`SettingsPanel.cpp` 40 % reduction is a target, not guaranteed").

**Entry**: Phase 12 green.
**Exit**: `ThemePage` works in the running app, no fan-out overrides remain in it, focus / tab order / arrow nav behave correctly, theme changes propagate.
**Commit**: `refactor(casso/ui): convert ThemePage to DxuiPanel + DxuiFormLayout`

### Migration Phase 14 — Convert remaining pages, debug panels, dialogs; delete DialogPrimitive; release gate ⏳ *(was Phase 11)*

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

**Entry**: Phase 13 green; `ThemePage` validated.
**Exit (release gate for the entire feature)**:

- `SettingsPanel.cpp` ≤ 1,307 lines (SC-003).
- `rg -n 'OnLButtonDown|OnLButtonUp|OnMouseHover|OnKey|^void\s+\w+::Paint\b|CollectFocusables' Casso/Ui/Settings Casso/Ui/Disk2DebugPanel Casso/Ui/InputDebugPanel Casso/Ui/Dialogs` → zero hits (SC-004).
- `rg -n 'WM_NCCALCSIZE' Casso/` → zero hits; matches exist **only** in `Dxui/Win32/` (SC-010, SC-020 final). NC plumbing duplicate count **2 → 1** (only `DxuiHostWindow`).
- `rg -n 'DialogPrimitive|DialogDefinition|DialogLayout' Casso/` → zero hits.
- `SettingsWindow` retired (migrated to `DxuiHostWindow` in this phase — replaces the original Phase 7 plan that deferred this migration).
- Klaus Dormann (`scripts/RunDormannTest.ps1`) and Tom Harte (`scripts/RunHarteTests.ps1 -SkipGenerate`) suites both pass (SC-012 — belt-and-suspenders even though this is UI-only).
- All user-visible behaviours (theme, settings persistence, drag-drop disk mount, emulator viewport resize, snap-layouts on all three top-level windows) match the pre-migration build under manual side-by-side comparison at 100 % / 150 % / 200 % DPI on Win10 + Win11 (SC-011, SC-015).
- All SC-001..SC-020 satisfied.

**Modify** (in addition to the original Phase 11 scope):

- `Casso\Ui\Settings\SettingsWindow.{h,cpp}` — host on `DxuiHostWindow` (full-ownership mode); **delete** the file when the conversion completes. (Originally planned for old Phase 7, deferred here.)

**Commit**: `refactor(casso/ui): migrate remaining pages, debug panels, dialogs to Dxui; delete DialogPrimitive`

> **Phase numbering note**: Migration phases are numbered as integers 1–14. The original 11-phase plan was expanded mid-execution (during Phase 7) when the chrome / `EmulatorShell` reality required additional phases — see spec Clarifications dated 2026-Q-Phase7+. Insertions: Phase 8 (adopt-HWND shim), Phase 10 (`DxuiMenuBar` + `MainMenu`), Phase 11 (chrome reshape + `EmulatorShell` restructure). Renumbering map for anyone reading older artefacts: old 8 → new 9, old 9 → new 12, old 10 → new 13, old 11 → new 14.

## Testing Strategy

### Test pyramid

1. **Headless widget unit tests** — the bulk of new coverage. Construct widgets with mock `IDxuiPainter` / `IDxuiTextRenderer` / `IDxuiTheme`, dispatch synthetic `DxuiMouseEvent` / `DxuiKeyEvent`, assert on recorded paint calls and on widget state. Zero D3D device, zero HWND. Land incrementally per phase per the file list above.
2. **Layout tests** — pure-function-style: build a panel with synthetic child rects, run `IDxuiLayout::Arrange`, assert resulting bounds. No painter required.
3. **NC classification tests** — `DxuiHostWindow` exposes a testable `ClassifyHitForTest(POINT)` seam (no real HWND) so `WM_NCHITTEST` mapping can be exercised in unit tests.
4. **Popup placement tests** — `DxuiPopupHost` exposes a testable `ComputePlacementForTest(anchorRectScreen, monitorRect, preferred)` seam.
5. **Dialog stack tests** — `DxuiDialogManager` exposes a `PushForTest` / `PopForTest` seam that drives the modal stack without real HWNDs; production code calls the real path.
6. **Existing Casso/CassoCore/CassoEmuCore tests** — must continue to pass at every phase gate (SC-005).
7. **Validation suites** — Klaus Dormann + Tom Harte at Phase 14 close (SC-012). Not run mid-migration because this is a UI-only refactor and the CPU/emulator code is untouched.
8. **Manual visual parity** — side-by-side screenshots at 100 % / 150 % / 200 % DPI on Win10 and Win11, gated at Phase 8 (main-window adopt-mode NC), Phase 11 (chrome reshape), and Phase 14 (final) per SC-011 / SC-015.

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

Per constitution §II: zero file-system / registry / network / system-API access in these tests. `DxuiHostWindow` and `DxuiPopupHost` tests must never call `CreateWindowEx` — they exercise the pure-function seams listed above. The Phase 8 adopt-mode tests likewise must not allocate a real HWND; the harness asserts `CreateInAdoptMode` does **not** call `CreateWindowEx` and feeds synthetic messages through `HandleMessage`.

## FR / SC → Phase Coverage Matrix

Phase-to-requirement mapping for the requirements added during the 2026-Q-Phase7+ clarification round. Original FR-001..FR-097 / SC-001..SC-013 coverage is documented inline in each phase's body and is unchanged.

| Requirement | Lands in Phase | Verified by |
|------------|----------------|-------------|
| FR-098 (adopt-mode `CreateInAdoptMode` + `HandleMessage`) | 8 | `DxuiHostWindowAdoptModeTests` |
| FR-099 (`SetHitTestDelegate`) | 8 | `DxuiHostWindowAdoptModeTests` |
| FR-100 (main window NC delegation; NC count 4 → 3) | 8 | grep `WM_NCCALCSIZE` in `Casso\Window.cpp` / `EmulatorShell.cpp` returns zero |
| FR-101 (`DxuiMenuBar` shape + props) | 10 | `DxuiMenuBarTests` |
| FR-102 (`DxuiMenuBar` Win11 UX) | 10 | `DxuiMenuBarTests` + manual menu UX walk |
| FR-103 (`NavLayer` → `MainMenu` rename + rewire) | 10 | SC-017 grep |
| FR-104 (`TitleBar` derives from `DxuiCaptionBar`) | 11 | SC-014 grep |
| FR-105 (`DriveWidget` / `LedIndicator` / `JoystickToggleButton` derive from `IDxuiControl`) | 11 | SC-014 grep |
| FR-106 (`ChromedPanelWindow` adopts `DxuiHostWindow`; NC 3 → 2) | 11 | NC-count audit |
| FR-107 (`EmulatorShell` stops inheriting `Window`; owns `DxuiHostWindow`; viewport reroutes) | 11 | SC-019 grep + manual viewport-rendering verify |
| FR-108 (NC count = 2 at Phase 11 close; = 1 at Phase 14 close) | 11, 14 | NC-count audit at each phase close |
| SC-014 (chrome derives from `IDxuiControl`/`DxuiCaptionBar`) | 11 | grep on class declarations |
| SC-015 (skeuomorphic chrome parity) | 11 | manual side-by-side |
| SC-016 (`MainMenu` UX parity) | 10 | manual menu UX walk |
| SC-017 (`NavLayer`/`NavMenu` grep = 0) | 10 | grep |
| SC-018 (`EmulatorShell.cpp` ≤ 2,502 lines) | 11 | `wc -l` |
| SC-019 (`EmulatorShell` not `Window` subclass) | 11 | grep |
| SC-020 (NC count = 2 at Phase 11; = 1 at Phase 14) | 11, 14 | NC-count audit |

## Risk Register

(Verbatim from spec §Risks, plus plan-level additions.)

| # | Risk | Likelihood | Impact | Phase | Mitigation |
|---|------|------------|--------|-------|------------|
| R1 | DX device sharing across HWNDs (`DxuiPopupHost` shares parent `ID3D11Device`, owns its own composition swap chain) | Med | High (crashes / invisible popups) | 9 | Create the device with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`; use `CreateSwapChainForComposition` + DirectComposition visual for popups; explicit owner-chain tracking; popup pool owned by `DxuiHostWindow`; dedicated `DxuiPopupHostTests` for lifecycle. |
| R2 | Custom-NC subtleties Win10 vs Win11 (rounded corners, Mica, snap-layouts) | Med | Med (visual gap) | 7, 8, 11 | `DxuiDwm` encapsulates version detection; eyeball-validate on both OSes at Phase 8 close (adopt-mode main window), Phase 11 close (chrome reshape), and Phase 14 close. |
| R3 | Reading-order tab differs subtly from hand-numbered `focusId` | High | Low (focus order shifts) | 13, 14 | Per-control `tabIndex` override; per-page focus-order review at conversion. |
| R4 | Manual chrome parity drifts during theme-interface port | Med | Med (visual regression) | 8, 11, 13 | Side-by-side screenshots at 100/150/200 % DPI; `ChromeTheme` is amended-in-place rather than replaced, so colour values stay in one location. |
| R5 | `SettingsPanel.cpp` 40 % reduction is a target, not guaranteed | Med | Low (spec miss) | 13 | Measure LOC after Phase 13 (`ThemePage`); extrapolate; flag explicitly before Phase 14 bulk conversion if overrun likely. |
| R6 | A regression in Phase 7 / 8 (host window + adopt shim) blocks every later phase | Low | High | 7, 8 | Each phase ends green and is independently mergeable; phases land as separate PRs, not one mega-PR. |
| R7 | `DxuiViewport::OnBoundsChanged` fires after the next emulator frame, producing one mis-sized frame | Low | Low (one-frame artefact) | 12 | Layout pass completes before paint pass; renderer subscribes and resizes lazily on first paint after bounds change. |
| R8 | Phase 1's angle-bracket carve-out breaks the discipline check or surprises reviewers | Low | Low | 1 | The amendment to `.github/copilot-instructions.md` lands in the same commit as the `Dxui.h` umbrella, with the carve-out scoped to "library project's umbrella header (currently only `Dxui.h`)". |
| R9 | `std::future<int>` returned by popup/dialog `Show` is awaited on a worker thread that then touches Dxui | Low | High (crash from UI-thread-only assumption — FR-083) | 9, 14 | Document FR-083 prominently in `Dxui.h` header doc comment; debug-build assertion in `DxuiHostWindow` message-pump entry that records the UI thread ID and re-asserts on every public Dxui API entry. |
| R10 | Mock painter call-set diverges from real painter API, hiding regressions | Med | Med | 6 → ongoing | `IDxuiPainter` interface methods are the **only** painter API widgets call; mock and concrete derive from the same interface so divergence is a compile error. |
| R11 | Phase 11 `EmulatorShell` restructure scope-creeps beyond the ≤ 2,502-line target | Med | Med (phase slips or partial migration ships) | 11 | Track LOC delta continuously; carve unrelated `EmulatorShell` cleanup out into follow-up commits; if 40 % proves unreachable, escalate to spec amendment rather than silently missing SC-018. The hit-test delegate seam from Phase 8 also means Phase 11 has a working fallback (keep delegate, defer full chrome reshape) if a sub-area refuses to fit. |
| R12 | Apple ][ viewport reroute through `DxuiHostWindow`'s swap chain regresses rendering fidelity (colour, scanlines, ratios, vsync) | Med | High (visible emulator artefacts) | 11, 12 | Per-mode (text40/text80/lores/hires/dhires) screenshot comparison against the pre-phase build at the same machine config; verify on both Win10 and Win11 and at 100 % / 150 % / 200 % DPI; ensure `D3DRenderer` resize path subscribed only to `DxuiViewport::OnBoundsChanged` (Phase 12) so no double-resize on DPI changes. |
| R13 | Chrome reshape (Phase 11) drifts pixel-parity for `TitleBar` / `DriveWidget` / `LedIndicator` / `JoystickToggleButton` | Med | Med (visible chrome regression) | 11 | Per-control screenshot comparison before/after the reshape; reshape one control at a time with a green-test gate between each; `ChromeTheme` colour values stay in one place to keep diff surface to layout / paint code rather than palette. |

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Amend `.github/copilot-instructions.md` angle-bracket rule to permit library umbrella header (`Dxui.h`) as a second chokepoint alongside `Pch.h` | Without an umbrella, every Dxui public header would have to angle-include its own system dependencies (`<d3d11.h>`, `<wrl/client.h>`, `<future>`, …), which violates the "system headers belong in `Pch.h`" rule in a different way and forces consumers to know Dxui's internal dependency set | (a) Consumer `Pch.h` enumerates Dxui's system deps directly — couples consumers to Dxui internals, breaks if Dxui adds a system dep, easy to forget on a new consumer. (b) Each Dxui public header angle-includes — same problem multiplied per-header, plus contradicts the existing rule even more loudly. The umbrella is the smallest viable carve-out. Documented in FR-004 and FR-007 with explicit consumer obligation (`#include "Dxui.h"` in their own `Pch.h`). |
| Two parallel focus-manager implementations briefly coexist (`Casso/Ui/FocusManager` until Phase 13 + `Dxui/Core/DxuiFocusManager` from Phase 6) | Phase 6 lands the framework; deleting `Casso/Ui/FocusManager` requires every Casso consumer of focus to migrate, which happens incrementally Phases 13–14 | Single-shot replacement = one giant PR = violates the "each phase ends green and is independently mergeable" gate (spec risk R6). Brief duplication is the cost of the staged migration. |
| Adopt-mode shim on `DxuiHostWindow` (`CreateInAdoptMode` + public `HandleMessage` + `SetHitTestDelegate`) coexists with the full-ownership path through Phases 8–10 | The single largest NC duplicate (`Casso\Window.cpp` + `Casso\EmulatorShell.cpp`) can be retired *without* the full chrome reshape, unlocking SC-009 / SC-014 work to proceed in parallel-friendly slices. Without it, Phase 7's exit would have to wait for the entire chrome reshape (the old Phase 7 → 11 ball of mud). | (a) Reshape main window's chrome at the same time as Phase 7 — multi-week mega-PR, violates "each phase mergeable" gate, blocks every later phase on one risky landing (spec risk R6 / R11). (b) Skip the main window in the NC consolidation — defeats SC-010 / FR-095 entirely. The shim is the smallest carve-out that preserves the staged-migration property. Removed in Phase 11 once chrome derives from `IDxuiControl`. |

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
- **Next command**: `/speckit.tasks` to expand the 14 migration phases into per-file, per-test concrete tasks. **tasks.md needs the matching renumbering + new tasks for Phases 8, 10, 11** (adopt-HWND shim, `DxuiMenuBar` + `MainMenu`, chrome reshape + `EmulatorShell` restructure). Existing T064–T069 / T071–T073 stay in Phase 7; later task ranges shift to align with the new phase boundaries.
