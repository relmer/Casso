# Implementation Plan: Full UI Overhaul (Native DX UI Reset)

**Branch**: `007-ui-overhaul` | **Date**: 2026-05-23 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/007-ui-overhaul/spec.md`

## Summary

Replace Casso's chrome, navigation, settings surfaces, drive widgets, LEDs, and
modal confirms with a **from-scratch native Direct3D 11 + DirectWrite UI
runtime**, owned entirely by the `Casso` project. This pivot follows a failed
prior attempt that introduced **RmlUi** as the chrome/settings runtime; per
spec **FR-053/FR-054** and **SC-011**, all RmlUi runtime ownership, build
wiring, source files, shaders, headers, and the vendored `External/RmlUi/`
dependency MUST be excised before the new pipeline lands. There is no
"legacy mode" fallback and no Win32 UI-control fallback (no menu bar, no
status bar, no `BUTTON`/`COMBOBOX`/`EDIT` host controls, no `DialogBox*`
modal). The only Win32 surfaces that remain are OS services that are not UI
controls: the `HWND` itself, `WM_NCHITTEST` integration, accelerator tables,
`IFileOpenDialog` (for click-to-browse mount per FR-022b), and `IDropTarget`
(for FR-022 drag-drop). `DiskIIDebugDialog` and `DebugConsole` are
out-of-scope for this spec and remain on their current Win32 implementation
pending a future spec.

Approach: build a thin painter on top of the existing `D3DRenderer` swap
chain (geometry, 9-slice, textured quads, gradients, soft-glow), share its
back-buffer with Direct2D for high-quality text via DirectWrite (D2D-on-D3D11
through `IDXGISurface`), and implement widget primitives (button, checkbox,
treeview row, dropdown, slider, list, scrollable panel, modal scrim) directly
against that painter. Hit-testing, focus, keyboard nav, and DPI live in a
single `UiShell` owner that consumes translated `WM_*` events.

## Technical Context

**Language/Version**: C++ (stdcpplatest, MSVC v145, VS 2026)
**Primary Dependencies**:
  - Windows SDK (D3D11, DXGI, D2D1, DirectWrite, DWM, Shell, Ole32)
  - C++ STL
  - Existing in-tree: `CassoCore`, `CassoEmuCore`
  - Approved 3rd-party shaders in `Casso/Shaders/CRT/` (crt-pi, libretro
    bloom, libretro ntsc-adaptive chroma stage — unchanged by this work)
  - **REMOVED**: RmlUi (`External/RmlUi/`) — see Constitution Check
**Storage**:
  - Per-machine: `<assetBaseDir>/Machines/<Name>/<Name>_user.json`
  - Global: `<assetBaseDir>/GlobalUserPrefs.json`
  - Themes: `<assetBaseDir>/Themes/<ThemeName>/` (`theme.json` + assets)
  - Window placement: `HKCU\Software\relmer\Casso\WindowPlacement\v1\<hash>`
**Testing**: Microsoft C++ Unit Test Framework (`UnitTest/`), with all
  filesystem/registry/system services mocked behind interfaces (FR-057,
  SC-013). Runtime validation: screenshot matrix M1–M7 captured under
  `TestResults/007-ui-overhaul/`.
**Target Platform**: Windows 10 + Windows 11, x64 and ARM64. Windows 11-only
  DWM APIs are runtime-gated via `IsWindows11OrGreater()` (FR-042).
**Project Type**: Desktop application (Win32 + D3D11).
**Performance Goals**:
  - Chrome + viewport composite at the swap chain's present cadence with no
    measurable regression versus the existing `D3DRenderer::UploadAndPresent`
    path on integrated GPUs (Intel/AMD iGPU) at 1280×960 and 1920×1080,
    100% and 150% DPI (SC-002).
  - Theme switch: first post-switch frame is fully themed (zero mixed-theme
    regions) — SC-002.
  - Drive door animation and floppy sound within one rendered frame
    (FR-050, SC-010).
**Constraints**:
  - No Win32 UI controls anywhere in the new runtime.
  - No RmlUi anywhere in active build targets, source, shaders, or vendored
    dependencies (FR-053, FR-054, SC-011).
  - Emulated viewport pixels and Apple II 4:3 aspect policy (FR-043) must be
    pixel-identical to today.
  - Device-lost recovery must rebuild all chrome geometry + textures
    (spec Edge Case).
**Scale/Scope**:
  - One application window, one swap chain, one UI ownership path.
  - ≈12 widget primitive types, ≈8 chrome surfaces (title bar, nav strip,
    dropdown, settings panel with 4 pages, drive widgets ×2, LED indicators
    ×2, modal confirm scrim).
  - 3 built-in themes shipped (Skeuomorphic, Dark Modern, Retro Terminal)
    with per-variant Disk ][ treatment for II/II+/IIe/IIc.

## Constitution Check

*GATE: Must pass before Phase 0. Re-checked after Phase 1.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Code Quality (formatting, EHM, scoping) | PASS | New `Casso/Ui/` files follow existing `Pch.h`-first, EHM, single-exit, top-of-scope-decl, column-alignment rules. |
| II. Testing Discipline (mocking, isolation) | PASS | Painter, layout, hit-test, focus, animation, theme loader, and config merge are factored into pure/data-driven units. System APIs (filesystem, registry, dialogs, drag-drop) live behind interfaces. FR-057/SC-013 honored. |
| III. UX Consistency | PASS | Custom chrome preserves the existing IDM command map and accelerator table; nav layer exposes every command currently in the Win32 menu (SC-006). |
| IV. Performance | PASS | Single swap chain; geometry batched per surface; text glyph runs cached per font/size; no per-frame allocations on the render path. |
| V. Simplicity & Maintainability | PASS *(after RmlUi excision)* | Removing RmlUi reduces External/-tree dependencies by one large library. New runtime is purpose-built and scoped to what Casso actually needs. |

**Constitution amendment required** (tracked in Complexity Tracking): the
**Approved Third-Party Dependencies** allowlist (constitution v1.5.0) lists
`RmlUi` as approved for spec 007. This plan reverses that decision. The
implementation MUST include a constitution amendment commit (MINOR bump per
governance rules — "materially expanded guidance / removed entry") that
deletes the `RmlUi` row from the allowlist and adds a sync-impact note
recording the reversal. This is the only declared violation; no others.

## Project Structure

### Documentation (this feature)

```text
specs/007-ui-overhaul/
├── plan.md                  # This file
├── research.md              # Phase 0 (existing — extended in this revision)
├── data-model.md            # Phase 1 (existing — re-checked)
├── quickstart.md            # Phase 1 (existing)
├── WindowArchitecture.md    # Existing companion doc
├── menu-command-parity.md   # Existing companion doc
├── contracts/
│   ├── theme-metadata.schema.json
│   ├── machine-user-config.schema.json
│   ├── global-user-prefs.schema.json
│   └── theme-manager.h
├── checklists/
│   └── requirements.md
└── tasks.md                 # Phase 2 output (`/speckit.tasks`, not this command)
```

### Source Code (repository root)

```text
Casso/                              # Win32 + D3D11 application
├── Pch.h / Pch.cpp                 # Remove all <RmlUi/*> includes
├── Main.cpp                        # Window class, message pump
├── Window.cpp / .h                 # Borderless window, WM_NCHITTEST routing
├── EmulatorShell.cpp / .h          # Owns UiShell; drops Rml init/shutdown
├── D3DRenderer.cpp / .h            # Adds shared D2D/DirectWrite device,
│                                   # drops Rml* render bindings
├── CrtPostProcess.cpp              # Unchanged
├── AssetBootstrap.cpp              # Drops Rml asset bootstrap; keeps Themes/
├── MenuSystem.cpp                  # Retired (no Win32 menu bar)
├── DiskIIDebugDialog.*             # Out of scope (kept as Win32 for now)
├── DebugConsole.*                  # Out of scope (kept as Win32 for now)
├── Config/
│   ├── IFileSystem.h
│   ├── Win32FileSystem.cpp / .h
│   ├── UserConfigStore.cpp / .h
│   └── GlobalUserPrefs.cpp / .h
├── Shaders/
│   ├── CRT/                        # Unchanged
│   └── Ui/                         # REPLACED — Rml shaders deleted, new
│                                   # ui_solid.hlsl, ui_textured.hlsl,
│                                   # ui_glyph.hlsl, ui_glow.hlsl
└── Ui/                             # Rewritten from scratch
    ├── UiShell.cpp / .h            # Top-level UI owner, message router,
    │                               # focus mgr, frame composer
    ├── DxUiPainter.cpp / .h        # D3D11 geometry: rects, 9-slice,
    │                               # gradients, textured quads, soft-glow
    ├── DwriteTextRenderer.cpp / .h # D2D-on-D3D11 (IDXGISurface) text +
    │                               # glyph-run cache
    ├── UiInput.cpp / .h            # WM_* -> UiEvent translation, modifiers
    ├── HitTester.cpp / .h          # Rect tree + WM_NCHITTEST mapping
    ├── FocusManager.cpp / .h       # Tab order, keyboard nav, focus cue
    ├── Layout.cpp / .h              # Stack/grid primitives, DPI scaling
    ├── Animation.cpp / .h          # Tween + DriveSyncEvent broker
    ├── Theme.cpp / .h               # ThemeManager + ThemeLoader (JSON)
    ├── Widgets/
    │   ├── Button.cpp / .h
    │   ├── Checkbox.cpp / .h
    │   ├── Slider.cpp / .h
    │   ├── Dropdown.cpp / .h
    │   ├── ListView.cpp / .h
    │   ├── TreeView.cpp / .h
    │   ├── TextField.cpp / .h       # Read-only labels + numeric editor
    │   ├── TabStrip.cpp / .h
    │   ├── ModalScrim.cpp / .h
    │   └── Tooltip.cpp / .h
    ├── Chrome/
    │   ├── TitleBar.cpp / .h        # NC drag, min/max/close, system menu
    │   ├── NavLayer.cpp / .h        # Top-level nav + dropdown panels
    │   ├── DriveWidget.cpp / .h     # Drive face, eject, spin, door anim
    │   └── LedIndicator.cpp / .h    # Soft-glow LED states
    ├── Settings/
    │   ├── SettingsPanel.cpp / .h           # Modal panel host
    │   ├── SettingsPanelState.cpp / .h      # Transient unapplied state
    │   ├── MachinePage.cpp / .h             # Machine + speed + video + WP
    │   ├── HardwarePage.cpp / .h            # Hardware component tree
    │   ├── ThemePage.cpp / .h               # Theme picker + CRT controls
    │   └── DisplayPage.cpp / .h             # CRT brightness/scanlines/etc.
    ├── DragDropTarget.cpp / .h     # IDropTarget impl (Win32 OS service)
    ├── AutoMountResolver.cpp / .h  # Existing — kept
    ├── DriveWidgetController.cpp / .h       # Existing logic — adapted
    ├── DriveWidgetState.h                   # Existing — adapted
    ├── IDriveCommandSink.h                  # Existing — kept
    ├── Win11DwmHelpers.cpp / .h    # Existing — kept, runtime-gated
    └── TitleBarHitTest.cpp / .h    # Existing — kept

External/                           # Vendored 3rd-party
└── RmlUi/                          # DELETED (FR-054, SC-011)

CassoCore/                          # Unchanged
CassoEmuCore/                       # Unchanged
CassoCli/                           # Unchanged

UnitTest/                           # Microsoft CppUnitTestFramework
├── Ui/                              # New test files (no real I/O)
│   ├── DxUiPainterTests.cpp        # Geometry batching, 9-slice math
│   ├── LayoutTests.cpp             # Stack/grid + DPI scaling
│   ├── HitTesterTests.cpp          # Rect routing + NC mapping
│   ├── FocusManagerTests.cpp       # Tab order + Esc dismiss
│   ├── ThemeLoaderTests.cpp        # JSON parse, version migration,
│   │                               # malformed-theme exclusion
│   ├── SettingsPanelStateTests.cpp # Machine switch reload, cancel
│   │                               # discard, apply commit semantics
│   ├── DriveWidgetStateTests.cpp   # Door state machine + sync events
│   ├── AnimationSyncTests.cpp      # Anim/sound within one frame
│   ├── ChromeCommandRoutingTests.cpp        # Nav -> IDM dispatch parity
│   └── NcHitTestTests.cpp          # Borderless hit-test classification
└── Existing test files             # Kept

specs/007-ui-overhaul/...           # Documentation (see above)
```

**Structure Decision**: Single-project Win32 desktop application. All new
code lives under `Casso/Ui/`. Tests live in `UnitTest/Ui/`. No new
solution-level projects; the `External/RmlUi/` project is removed from the
solution and the `Casso` project's `ProjectReference`, include path
(`..\External\RmlUi\Include`), and preprocessor defines
(`RMLUI_STATIC_LIB`, `RMLUI_NO_THIRDPARTY_CONTAINERS`) are stripped from
all six configuration blocks (Debug/Release × x64/ARM64 + Analyze rows).

## Phase 0 — Outline & Research

Existing `research.md` R1–R5 covers chrome rendering split, single
ownership, NC behavior contract, command routing, and DPI policy. The existing
R6/R7 entries are obsolete pre-pivot decisions and are replaced in-place with
the following decisions; R8–R13 are appended in the same edit:

- **R6 — RmlUi excision strategy**. Remove the `External/RmlUi/` project
  reference, includes, defines, vendored sources, Rml-prefixed files, shaders,
  and remaining `Rml` / `RMLUI` symbols before introducing the new painter.
  Non-Rml-prefixed UI files that still reference Rml are either deleted (when
  a later phase rewrites them) or stubbed to compile with native-owned state.
- **R7 — D2D-on-D3D11 text pipeline**. Use Direct2D-on-D3D11: create
  `ID2D1Device`/`ID2D1DeviceContext` from the shared DXGI device; bind a
  `ID2D1Bitmap1` over the swap chain back-buffer via `IDXGISurface`. Render
  DirectWrite text after the D3D geometry pass and before `Present`, with
  glyph-layout caching by font family, weight, size, DPI, and text.
- **R8 — Hit-test architecture**. A native DPI-scaled rect tree owns client
  hit-testing and `WM_NCHITTEST` classification; mouse and drag/drop events
  consult the same tree.
- **R9 — Focus and keyboard policy**. `FocusManager` owns tab order,
  activation keys, Escape dismissal, and visible focus cues for Settings.
- **R10 — Modal and popup layer**. Menus, dropdowns, tooltips, and confirms
  render in a native overlay stack; modal scrims capture input while active.
- **R11 — Drag-drop carve-out**. `IDropTarget` and `IFileOpenDialog` remain
  OS services, with accepted targets/actions routed through native hit tests
  and existing mount commands.
- **R12 — Debug-tool carve-out**. `DiskIIDebugDialog` and `DebugConsole` stay
  Win32-implemented; only the nav entry points that launch them move.
- **R13 — Theme-token broadcast**. `ThemeManager` broadcasts validated tokens,
  drive visual profile, CRT defaults, and backdrop flags so chrome/settings
  update before the first post-switch frame.

**Output**: All Phase 0 NEEDS-CLARIFICATION items are resolved (the prior
revision already resolved the major open questions in `Clarifications`
session 2026-05-23). The R6–R13 additions above are the implementer's
brief for the RmlUi-removal pivot.

## Phase 1 — Design & Contracts

**Prerequisites**: `research.md` (existing) plus R6–R13 above.

1. **Data model** (`data-model.md`). Ensure the existing document explicitly
   covers `Theme`, `ChromeMetrics`, `ChromeRects`, `ChromeVisualState`,
   `DriveWidgetState`, `DriveSyncEvent`, `GlobalUserPrefs`,
   `MachineUserConfig`, `HardwareComponentEntry`, `SettingsPanelState`,
   and `WindowPlacementProfile`, then add `UiDrawList` (per-frame batched
   geometry/texture/text command stream consumed by `DxUiPainter` and
   `DwriteTextRenderer`).
2. **Contracts** (`contracts/`). Existing JSON schemas
   (`theme-metadata.schema.json`, `machine-user-config.schema.json`,
   `global-user-prefs.schema.json`) and the `theme-manager.h` header
   remain authoritative. No new external contracts are exposed by this
   spec. The Win32 surfaces that remain (HWND messages, `IDropTarget`,
   `IFileOpenDialog`, accelerators) are OS contracts, not Casso contracts.
3. **Agent context update**. `.github/copilot-instructions.md` already
   points its SPECKIT marker block at `specs/007-ui-overhaul/plan.md` —
   no edit required.

**Output**: `data-model.md` (explicit entity coverage + `UiDrawList` addition
during implementation), `contracts/*` (existing), `quickstart.md` (existing).

## Implementation Phases

Implementation is staged so the tree is **always** compilable and
testable. RmlUi removal happens in P0 **before** any new UI code is
introduced, so the two pipelines never coexist.

### P0 — Excise RmlUi (must land first, single commit series)

1. Remove the RmlUi project entry and all associated `{GUID}` configuration
   rows from `Casso.sln` after looking up the actual project GUID.
2. Remove the `<ProjectReference Include="..\External\RmlUi\RmlUi.vcxproj">`
   from `Casso/Casso.vcxproj`. Strip `..\External\RmlUi\Include` from every
   `AdditionalIncludeDirectories`. Strip `RMLUI_STATIC_LIB` and
   `RMLUI_NO_THIRDPARTY_CONTAINERS` from every `PreprocessorDefinitions`.
3. Delete all `Casso/Ui/Rml*.{cpp,h}` files (8 files) and remove their
   `<ClCompile>` / `<ClInclude>` entries from `Casso.vcxproj`.
4. Delete `UiShell.{cpp,h}`, `LedElement.{cpp,h}`, and
   `DriveWidgetElement.{cpp,h}` outright; P1/P2 rewrites them from native
   contracts.
5. Stub non-Rml-prefixed UI files that still contain Rml symbols so P0 remains
   compilable: `SettingsPanel` becomes an empty compiling skeleton returning
   `S_OK`, `ThemeManager` drops Rml document/context members while retaining
   `Discover()` and an in-memory active theme name, `TitleBar`/`NavLayer`
   `Show`/`Hide` become no-ops while preserving pure layout/parity logic, and
   `DriveWidgetController` stubs UI-facing document/hit-test methods while
   preserving the state-pump path. `SettingsPanelState` and `ThemeLoader` keep
   Rml-free logic and only lose Rml includes/references if present.
6. Delete `Casso/Shaders/Ui/rml_*.hlsl` and remove their build entries.
7. Delete `External/RmlUi/` directory in its entirety.
8. Remove `#include <RmlUi/...>` from `Pch.h`, `EmulatorShell.{cpp,h}`,
   `D3DRenderer.{cpp,h}`, `Main.cpp`, `Window.cpp`, `AssetBootstrap.cpp`,
   `resource.h`, and any other call sites surfaced by build.
9. Remove RmlUi init/shutdown calls from `EmulatorShell`, after-blit hooks,
   settings panel listeners, drive widget instancers, and the per-frame Rml
   render/input hooks from `D3DRenderer` and `Main.cpp`'s message pump.
   Replace each with clean no-op compile paths (`// UiShell takes over in P1`)
   that still allow the app to launch a black/bare viewport window.
10. Drop Rml-related entries from `AssetBootstrap` (the `Themes/` bootstrap
    itself stays — only the Rml-document/font bootstrap goes).
11. Amend the constitution: delete the `RmlUi` row from the Approved
    Third-Party Dependencies table; add a sync-impact note; bump to
    v1.6.0 (MINOR — materially changed Tech Constraints by removing an
    approved dependency).
12. Verify: `scripts\Build.ps1` succeeds for all four configurations;
    `scripts\Build.ps1 -RunCodeAnalysis` is clean; `scripts\RunTests.ps1`
    passes. Application launches with **no chrome** beyond the bare
    borderless window + viewport (intentional regression — P1 reintroduces
    chrome). `rg -n "Rml|RMLUI" Casso CassoCore CassoEmuCore CassoCli
    UnitTest External` returns zero hits.

**Acceptance gate for P0**: SC-011 (source/build audit shows no active
RmlUi runtime symbols or dependency wiring) is provable by `rg`. No P1
work begins until P0 is merged.

### P1 — Foundational native UI runtime (no user-visible features yet)

Builds the painter/text/input/layout/focus/animation/theme primitives that
every subsequent phase depends on. **Maps to spec FR-046 and US5
prerequisites.**

- `DxUiPainter` with `ui_solid`/`ui_textured`/`ui_glow` shaders.
- `DwriteTextRenderer` (R7) sharing the swap-chain back buffer via
  `IDXGISurface`; glyph-run cache.
- `UiInput` translates `WM_MOUSE*`, `WM_KEY*`, `WM_CHAR`, `WM_*BUTTON*`,
  `WM_MOUSEWHEEL`, `WM_SETFOCUS`/`WM_KILLFOCUS` into `UiEvent`.
- `HitTester` (R8) + `WM_NCHITTEST` integration in `Window.cpp`.
- `FocusManager` with Tab/Shift-Tab/Enter/Space/Escape (FR-044).
- `Layout` stack/grid + per-window DPI helpers (R5).
- `Animation` tween + `DriveSyncEvent` broker (FR-050, SC-010).
- `Theme` loader (`ThemeManager` + JSON loader) honoring
  `contracts/theme-metadata.schema.json` and version migration
  (FR-045); malformed themes excluded with warning (FR-036).
- `AssetBootstrap` extracts built-in themes (FR-030, FR-037) — embedded
  resources need re-authoring as plain image/json bundles (no Rml docs).
- Device-lost recovery: painter, text renderer, and theme texture cache
  all rebuild on `D3DRenderer` device restore (spec Edge Case).
- Tests: `DxUiPainterTests`, `LayoutTests`, `HitTesterTests`,
  `FocusManagerTests`, `ThemeLoaderTests`, `AnimationSyncTests`,
  `NcHitTestTests`. All with mocked filesystem.

### P2 — Chrome surfaces (P1 stories)

Implements the visible chrome that closes US3 + US5 and most of US1/US2.

- `Chrome/TitleBar` — drag, double-click fullscreen, min/max/close,
  right-click system menu (FR-018, FR-019, FR-020, FR-028).
- `Chrome/NavLayer` — top-level nav + dropdown panels covering every
  former Win32 menu command via the existing IDM map (FR-026, SC-006).
- `Chrome/DriveWidget` — 9-slice drive face with door, eject affordance,
  spinning disk during motor-on, sized per FR-021 (FR-021 through FR-024,
  FR-049, FR-050).
- `Chrome/LedIndicator` — soft-glow LED meeting FR-025 dimensional minima.
- `DragDropTarget` (R11) wires drop coordinates through `HitTester` for
  per-widget acceptance (FR-022).
- Click-to-browse via `IFileOpenDialog` (FR-022b).
- Variant treatment for II/II+/IIe/IIc (FR-051, SC-009).
- Tests: `ChromeCommandRoutingTests`, `DriveWidgetStateTests`.
- Runtime validation: capture screenshot matrix M1, M2, M3, M5, M6, M7.

### P3 — Settings panel (P1 stories)

Closes US1 + US2 and folds in P2 theme picker entry point.

- `Settings/SettingsPanel` modal panel (no Win32 dialog — FR-027).
- `MachinePage` — machine selector (outermost control), speed, video,
  write protect, drive audio toggle/mechanism (FR-002, FR-003, FR-011).
- `HardwarePage` — `TreeView` of `HardwareComponentEntry` with capability
  flag rendering rules (FR-004 through FR-008).
- `SettingsPanelState` transient buffer; Cancel discards; Apply commits
  via existing command queue + reset prompt for reset-required changes
  (FR-009, FR-010, edge cases).
- Per-machine `_user.json` read/write through `UserConfigStore` with
  schema merge + `MachineConfigUpgrade` invocation (FR-012 through
  FR-017, US6 / FR-013 canonical-field rules).
- Tests: `SettingsPanelStateTests`, plus extension of existing
  `MachineConfigUpgrade` tests for `capabilityFlag` + canonical-field
  rewrite.
- Runtime validation: M4.

### P4 — Themes + CRT controls (P2 stories)

Closes US4 and finalizes the theme system.

- `ThemePage` — theme picker, hot-swap (FR-033), discovery refresh
  (FR-035), persistence in `GlobalUserPrefs` (FR-034).
- `DisplayPage` — CRT brightness slider (FR-038), per-effect toggles +
  per-effect parameters (FR-039, FR-040). All overrides global, not
  machine-specific.
- Ship 3 built-in themes per FR-031 with full Apple II-family variant
  coverage (FR-051, FR-052).
- Runtime validation: SC-002 two-frame capture during theme switch.

### P5 — Polish & validation

- US6 silent upgrade validation across simulated v→v+3 transitions
  (SC-003, SC-007).
- Drive widget drag-drop reliability run: 200 attempts × 4 formats
  (SC-004).
- Code analysis clean (`scripts\Build.ps1 -RunCodeAnalysis`).
- Full screenshot matrix M1–M7 captured under
  `TestResults/007-ui-overhaul/` per `quickstart.md`.
- `CHANGELOG.md` + `README.md` updates.
- Final RmlUi-residue audit (`rg` sweep) re-run on the merged branch
  (SC-011, SC-012, FR-055, FR-056).

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| Constitution amendment (v1.5.0 → v1.6.0): delete `RmlUi` from Approved Third-Party Dependencies allowlist. | Spec FR-053/FR-054/SC-011 mandate full RmlUi excision; leaving the row in the allowlist would falsely signal RmlUi is an approved active dependency. | Keeping the row "for documentation" rejected — the allowlist is an authoritative gate, not a history log. Removal + sync-impact note is the cleaner contract. |
| New `Casso/Ui/` runtime is purpose-built (≈12 widget primitives, painter, text, focus, layout, animation, theme, hit-test) rather than adopting a 3rd-party UI library. | The prior attempt with RmlUi failed and is being removed by spec mandate. The Approved Third-Party Dependencies allowlist (post-amendment) contains no UI library, and adding one would itself require a constitution amendment justifying ongoing cost vs. a custom runtime. Casso's UI surface is small and stable (chrome + one settings panel + drive widgets), so the build/test/maintenance cost of a 3rd-party UI runtime exceeds the cost of a focused custom one. | Adopting another 3rd-party UI library (e.g., Dear ImGui, Nuklear) rejected because (a) requires a fresh constitution amendment with the same governance overhead, (b) reintroduces the integration-mismatch class of failure that ended the RmlUi attempt, (c) Casso's UI scope does not benefit from a general-purpose UI runtime's feature surface. |

No other gate violations.

## Stop & Report

This document is the Phase-2 planning artifact only. `/speckit.tasks`
will translate the P0–P5 phases above into the actionable `tasks.md`
backlog. The next executable step for an engineer picking this up is
**P0 step 1**.

- **Branch**: `007-ui-overhaul`
- **Plan**: `specs/007-ui-overhaul/plan.md` (this file)
- **Generated/refreshed artifacts**:
  - `plan.md` — regenerated (RmlUi-removal pivot)
  - `research.md` — existing; will be extended in-place by R6–R13 during
    implementation kick-off
  - `data-model.md` — existing; `UiDrawList` entity to be appended in P1
  - `contracts/` — existing; unchanged
  - `quickstart.md` — existing; unchanged
