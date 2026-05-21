# Implementation Plan: Full UI Overhaul (RmlUi + CSS Themes + Custom D3D Chrome)

**Branch**: `007-ui-overhaul` | **Date**: 2026-05-20 | **Spec**: [spec.md](./spec.md)
**Input**: `specs/007-ui-overhaul/spec.md` (48 functional requirements: FR-001..FR-047 plus FR-022b)

## Summary

Replace Casso's Win32 chrome (title bar, menus, OptionsDialog, MachinePickerDialog,
status-bar drive indicators) with a fully custom Direct3D 11–rendered shell built
on **RmlUi** (MIT, vendored in-tree). One consolidated machine-aware Settings panel
supersedes every existing per-feature dialog. Per-machine user overrides become
`<MachineName>_user.json` shadow files merged at load time; non-machine preferences
(active theme, CRT effect parameters, global brightness) go to a single
`GlobalUserPrefs.json`. Themes ship as directories of `.rml` + `.rcss` + assets,
hot-swappable at runtime via RmlUi document reload, with three built-ins
(Skeuomorphic, Dark Modern, Retro Terminal). The emulated viewport keeps its
existing D3D11 path and gains optional CRT post-processing (scanlines, phosphor
bloom, color bleed) implemented as HLSL ports of MIT/public-domain community
shaders.

The plan is **explicitly anti–DIY-UI**. Two approved third-party dependencies
(RmlUi source + MIT shader source) replace what would otherwise be many thousand
lines of hand-rolled focus management, layout, text shaping, and shader work.

## Technical Context

**Language/Version**: C++ `stdcpplatest` (MSVC v145+, VS 2026)
**Primary Dependencies**:
  - **Windows SDK / DirectX 11** (existing)
  - **RmlUi** (MIT, vendored under `External/RmlUi/`, built as
    `External/RmlUi/RmlUi.vcxproj` static lib in the solution)
  - **In-tree HLSL CRT shaders** (MIT/public-domain ports under
    `Casso/Shaders/CRT/`); GPL shaders (CRT-Royale et al.) **excluded**
**Storage**:
  - Per-machine read-only defaults: `Machines/<Name>/<Name>.json` (embedded then
    extracted by `AssetBootstrap`)
  - Per-machine user overrides: `Machines/<Name>/<Name>_user.json` (NEW)
  - Global preferences: `GlobalUserPrefs.json` at asset base dir (NEW)
  - Themes: `Themes/<ThemeName>/{theme.json,*.rml,*.rcss,assets/...}` (NEW;
    embedded built-ins extracted via the AssetBootstrap pattern)
**Testing**: Microsoft C++ Unit Test Framework (`UnitTest/` project). New test
  groups: `UserConfigStoreTests`, `GlobalUserPrefsTests`, `ThemeLoaderTests`,
  `SettingsPanelStateTests`, `RmlBackendSmokeTests` (logic only; no real GPU).
**Target Platform**: Windows 10/11, x64 / ARM64. FR-042-style Win11-only
  effects (Mica, `DWMWA_WINDOW_CORNER_PREFERENCE`, modern WM_NCHITTEST
  niceties) are gated at runtime via `IsWindows11OrGreater()`; the chrome
  must remain functional on Win10 (sharp corners + solid backgrounds).
**Project Type**: Desktop GUI application (Casso) plus two static-library
  dependencies (CassoCore, CassoEmuCore) — no new project shape, but a new
  `External/RmlUi` static lib project is added to the solution.
**Performance Goals**:
  - SC-005: chrome + theme + RmlUi render pass ≤ **1 ms / frame** on mid-range
    GPU at native res.
  - SC-002: theme switch visible within one rendered frame (≤ ~16 ms).
  - 60 fps steady-state with CRT post-processing enabled on the same target.
**Constraints**:
  - MIT license preservation (no GPL deps; shaders audited).
  - Existing `D3DRenderer` swap chain remains the only swap chain — RmlUi
    composites into it, not a sibling HWND.
  - Win32 `HWND` is retained as the input/composition anchor.
  - Emulation keeps running while Settings panel is open (FR-041).
  - Strict adherence to constitution: EHM, single exit, top-of-scope vars,
    function-spacing rules, no Pch-bypass for system headers — RmlUi headers
    go through `Pch.h` like everything else.
**Scale/Scope**:
  - ~48 functional requirements across 3 areas.
  - 3 built-in themes + open extensibility for user themes.
  - 5–7 new modules in `Casso/`; 1 schema extension in `CassoEmuCore`;
    1 vendored third-party lib; 3–5 HLSL files in `Casso/Shaders/CRT/`.

## Constitution Check

*Gate evaluated against `.specify/memory/constitution.md` v1.4.0.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Code Quality (EHM, spacing, top-of-scope, function size, comments in .cpp) | **PASS** (with discipline required) | RmlUi callbacks invoked from our code must still follow EHM; we wrap RmlUi calls in our own helpers so the EHM pattern surrounds them. |
| II. Testing Discipline (no real I/O, mocks for FS/registry) | **PASS** | `UserConfigStore` and `ThemeLoader` accept an `IFileSystem`-style abstraction; existing `AssetBootstrap` mocking pattern is reused. RmlUi rendering is not exercised in unit tests; only loader/validator logic is. |
| III. UX Consistency | **PASS** | Custom chrome unifies all entry points; CLI unaffected. |
| IV. Performance | **PASS** at design level | 1 ms chrome budget (SC-005), 60 fps CRT pass. Validated by frame-time logging during implementation. |
| V. Simplicity & Minimal Dependencies | **VIOLATION — JUSTIFIED** | See *Complexity Tracking* below: RmlUi + community CRT shaders are explicit, scoped exceptions to the "Windows SDK + STL only" clause. |

### ⚠️ Constitution amendment required (Phase 0, blocking)

The constitution currently states (`§ Technology Constraints`):

> **Dependencies**: Windows SDK, STL only; no third-party libraries

This plan introduces two third-party dependencies (RmlUi source + MIT CRT
shader sources). A constitution amendment is **mandatory before implementation
work begins** (see Phase 0 task **P0-T1**). The amendment should:

1. Bump constitution to **v1.5.0** (MINOR — materially expanded guidance: adds
   an approved-dependency allowlist mechanism).
2. Replace the absolute clause with: *"Windows SDK + STL by default. Any
   additional third-party dependency MUST be (a) MIT/BSD/Apache/PD-licensed,
   (b) source-vendored in-tree under `External/`, (c) buildable as a project
   in the solution with no external package manager, and (d) explicitly listed
   in the `Approved Dependencies` table."*
3. Add an **Approved Dependencies** table listing RmlUi (UI framework) and the
   in-tree CRT shader ports (visual post-processing).
4. Update the SYNC IMPACT REPORT block.

If the amendment is not approved as written, **the plan stops** — there is no
non-RmlUi fallback in scope (per the binding directive in the planning input).

## Project Structure

### Documentation (this feature)

```text
specs/007-ui-overhaul/
├── plan.md              # This file
├── research.md          # Phase 0 — RmlUi/shader/Win11 chrome research + decisions
├── data-model.md        # Phase 1 — entity definitions (UserConfig, Theme, etc.)
├── quickstart.md        # Phase 1 — dev/run/iteration loop
├── contracts/
│   ├── machine-user-config.schema.json
│   ├── global-user-prefs.schema.json
│   ├── theme-metadata.schema.json
│   ├── rml-backend.h            # C++ interface contract (header-shaped doc)
│   └── theme-manager.h          # C++ interface contract (header-shaped doc)
└── tasks.md             # Phase 2 — produced by /speckit.tasks (not here)
```

### Source code (repository root, after this feature)

```text
External/
└── RmlUi/                        # NEW — vendored source (git subtree or copy)
    ├── RmlUi.vcxproj             # NEW — static lib, added to Casso.sln
    ├── Source/                   # upstream
    ├── Include/                  # upstream
    └── README.casso.md           # what version is vendored, how to re-vendor

Casso/                            # GUI app (existing project)
├── Pch.h                         # adds: <RmlUi/Core.h>, RmlUi C headers
├── Main.cpp                      # bootstraps RmlUi after AssetBootstrap
├── EmulatorShell.{h,cpp}         # owns UiShell + ThemeManager + UserConfigStore
├── D3DRenderer.{h,cpp}           # MODIFIED — exposes shared device + per-frame
│                                 #   hook for RmlUi backend + CRT post-pass
├── AssetBootstrap.{h,cpp}        # MODIFIED — adds EnsureThemes(), EnsureGlobalUserPrefs()
├── RegistrySettings.{h,cpp}      # MODIFIED — one-shot migration source only
├── OptionsDialog.{h,cpp}         # DELETED in final commit
├── MachinePickerDialog.{h,cpp}   # DELETED in final commit
│
├── Ui/                           # NEW directory — RmlUi integration layer
│   ├── UiShell.{h,cpp}           # boots RmlUi Context, owns top-level docs
│   ├── RmlBackend_D3D11.{h,cpp}  # custom Rml::RenderInterface implementation
│   ├── RmlSystemInterface.{h,cpp}# clock, log, clipboard, cursor
│   ├── RmlInputBridge.{h,cpp}    # Win32 WM_* → Rml::Input dispatch
│   ├── ThemeManager.{h,cpp}      # load/activate/hot-swap themes
│   ├── ThemeLoader.{h,cpp}       # parse theme.json + validate RML/RCSS
│   ├── DriveWidgetElement.{h,cpp}# custom Rml::Element for drive widgets
│   ├── LedElement.{h,cpp}        # custom Rml::Element for LED indicators
│   ├── SettingsPanel.{h,cpp}     # binds RML doc + SettingsPanelState
│   ├── SettingsPanelState.{h,cpp}# transient snapshot for cancel/apply
│   ├── TitleBarController.{h,cpp}# drag/min/max/close/WM_NCHITTEST glue
│   └── NavLayerController.{h,cpp}# top-level menu dispatcher
│
├── Config/                       # NEW directory — user-side config
│   ├── UserConfigStore.{h,cpp}   # load/save/merge _user.json files
│   ├── GlobalUserPrefs.{h,cpp}   # load/save GlobalUserPrefs.json
│   └── IFileSystem.h             # injectable FS abstraction (for tests)
│
├── Shaders/CRT/                  # NEW — HLSL CRT post-process shaders
│   ├── crt_common.hlsli          # shared utilities + license header
│   ├── crt_scanlines.hlsl        # MIT port (attributed in header)
│   ├── crt_bloom.hlsl            # MIT/PD port (attributed in header)
│   ├── crt_color_bleed.hlsl      # MIT/PD port (attributed in header)
│   ├── crt_composite.hlsl        # final compose
│   └── LICENSES.md               # per-shader source/author/license
│
└── Resources/                    # MODIFIED — embedded built-in themes
    └── Themes/...                # Skeuomorphic, DarkModern, RetroTerminal

CassoEmuCore/Core/
├── MachineConfig.{h,cpp}         # MODIFIED — adds capabilityFlag + lockReason
│                                 #   on hardware entries; renames $cassoDefault
│                                 #   → $cassoMachineVersion (with legacy alias)
└── MachineConfigUpgrade.{h,cpp}  # MODIFIED — new upgrade step(s) for the
                                  #   rename + capabilityFlag defaulting

UnitTest/
├── EmuTests/MachineConfigUpgradeTests.cpp   # MODIFIED — adds rename + capFlag cases
├── UiTests/                                 # NEW group
│   ├── UserConfigStoreTests.cpp
│   ├── GlobalUserPrefsTests.cpp
│   ├── ThemeLoaderTests.cpp
│   ├── SettingsPanelStateTests.cpp
│   ├── TitleBarHitTestTests.cpp
│   └── RmlBackendSmokeTests.cpp             # logic-only; no real D3D device
```

**Structure Decision**: Single GUI project (`Casso/`) gains two sub-namespaces
(`Casso/Ui/`, `Casso/Config/`) so the new modules don't drown the root. RmlUi
sits in `External/RmlUi/` as a sibling project added to `Casso.sln`. CassoCore
remains untouched; CassoEmuCore gains only the schema extension. No new exe.

## Phased Plan

Phases are linear at the gate level (each gate must pass before the next phase
starts) but tasks within a phase are largely parallelizable. The full task
breakdown belongs to `/speckit.tasks`; this section lists the work units the
task generator should expand.

### Phase 0 — Foundations (research + governance)

Gate to exit: research.md complete, constitution amended, RmlUi version pinned.

- **P0-T1 [BLOCKING]** Constitution amendment v1.4.0 → v1.5.0 (see above).
  Must be merged before any third-party source lands in the repo.
- **P0-T2** Pin RmlUi version (current upstream stable; verify MIT license file
  is present; document the exact tag/commit in `External/RmlUi/README.casso.md`).
- **P0-T3** Audit shader sources: for each candidate (CRT-Lottes, CRT-Geom-Mod,
  libretro common, crt-pi), record original author, license, and SHA of the
  upstream file we ported from. Record decisions in `Casso/Shaders/CRT/LICENSES.md`.
- **P0-T4** RmlUi D3D11 backend research: review upstream Backends/RmlUi_Backend_*.
  Decide: write our own backend from scratch (recommended — small, ~600 LOC) vs.
  fork upstream `RmlUi_Renderer_DX11`. Decision and rationale → research.md.
- **P0-T5** Windows 11 borderless-window research: verify the WM_NCHITTEST +
  `DwmExtendFrameIntoClientArea` + `DWMWA_WINDOW_CORNER_PREFERENCE` recipe works
  on both x64 and ARM64 Win11 (no behavioral differences). Document in research.md.
- **P0-T6** Font strategy: choose one default font shipped with each built-in
  theme (must be SIL OFL or PD). Document in research.md.

### Phase 1 — Vendoring & schema (no UI yet)

Gate to exit: solution still builds; all existing tests still pass; new schema
fields round-trip through MachineConfigUpgrade.

- **P1-T1** Vendor RmlUi source under `External/RmlUi/`. Create
  `External/RmlUi.vcxproj` as a Static Library, x64+ARM64, Debug+Release,
  toolset v145. Add to `Casso.sln`. Wire `Casso.vcxproj` to reference it.
  Verify clean build on all four configurations.
- **P1-T2** Add RmlUi headers to `Pch.h` (per the angle-bracket include rule:
  ALL system/3rd-party headers go through Pch). Confirm no compile/lint hits.
- **P1-T3** Extend `MachineConfig` schema:
  - Add `capabilityFlag` (`optional`|`required`|`platform-locked`) on internal
    device entries and slot entries.
  - Add optional `lockReason` string.
  - Rename `$cassoDefault` → `$cassoMachineVersion`. Keep `$cassoDefault` as a
    read alias for one upgrade cycle.
- **P1-T4** Add new upgrade step(s) to `MachineConfigUpgrade`:
  - Rename `$cassoDefault` → `$cassoMachineVersion`.
  - Default missing `capabilityFlag` to `"required"` on internal devices and
    `"optional"` on slots (per FR-015).
- **P1-T5** Update all default machine JSONs under `Resources/Machines/` for the
  rename and to set explicit `capabilityFlag` + `lockReason` where appropriate
  (e.g., Apple //c integrated 80-column card → `platform-locked` with reason).
- **P1-T6** New tests for the rename + capabilityFlag defaulting in
  `MachineConfigUpgradeTests.cpp`.

### Phase 2 — User-config infrastructure

Gate to exit: per-machine `_user.json` round-trips; `GlobalUserPrefs.json`
round-trips; both go through an injectable FS for tests.

- **P2-T1** `IFileSystem` abstraction in `Casso/Config/IFileSystem.h`
  (`ReadAllText`, `WriteAllText` (atomic), `Exists`, `Delete`, `EnumerateFiles`).
  Default impl wraps Win32; tests use in-memory.
- **P2-T2** `UserConfigStore`:
  - `Load (machineName, defaultConfig) → MachineConfig` performing the
    shadow/fallthrough merge (FR-014, FR-017).
  - On version mismatch, invoke `MachineConfigUpgrade` and write migrated
    file back (FR-013).
  - `SaveDelta (machineName, current, default)` writes only the user-changed
    fields (no full snapshot).
- **P2-T3** `GlobalUserPrefs`:
  - Fields: `activeTheme` (string), `crt.brightness` (float 0..1),
    `crt.scanlines.enabled/intensity`, `crt.bloom.enabled/radius/strength`,
    `crt.colorBleed.enabled/width`, `window.lastBounds` (optional),
    `$cassoGlobalPrefsVersion` (int).
  - Load + Save through `IFileSystem`; default-construct if file absent.
- **P2-T4** `AssetBootstrap`:
  - `EnsureThemes()` mirroring `EnsureMachineConfigs()` — extracts built-in
    themes from embedded resources; preserves user themes (FR-030, FR-037).
  - `EnsureGlobalUserPrefs()` — writes a default file if missing.
- **P2-T5** `RegistrySettings` migration shim — on first launch with no
  `GlobalUserPrefs.json` or no `<Name>_user.json`, read legacy values from the
  registry, write to the new JSONs, and stop touching the registry afterward
  (FR-016).
- **P2-T6** Tests for all of the above, with an in-memory FS.

### Phase 3 — RmlUi D3D11 backend + shell boot

Gate to exit: empty RmlUi Context boots inside the existing window, presents
a transparent overlay on top of the emulator, and dismisses cleanly on exit.
No theme yet; no chrome content yet.

- **P3-T1** `RmlBackend_D3D11`:
  - Implements `Rml::RenderInterface`: `RenderGeometry`, `CompileGeometry`,
    `RenderCompiledGeometry`, `ReleaseCompiledGeometry`, `EnableScissorRegion`,
    `SetScissorRegion`, `LoadTexture`, `GenerateTexture`, `ReleaseTexture`,
    `SetTransform`.
  - Uses **shared** ID3D11Device/Context from `D3DRenderer` (no second device).
  - Two HLSL shaders: textured + untextured triangle list, premultiplied alpha.
  - Pre-multiplied alpha blend state cached; scissor rect via RSSetScissorRects.
- **P3-T2** `RmlSystemInterface`: routes `GetElapsedTime`, logging
  (through our existing logger), clipboard (Win32), cursor (LoadCursor).
- **P3-T3** `RmlInputBridge`: maps `WM_MOUSE*`, `WM_KEY*`, `WM_CHAR`,
  `WM_MOUSEWHEEL`, `WM_SETFOCUS`, IME messages into Rml input events.
- **P3-T4** `UiShell::Initialize` / `Render` / `Shutdown`:
  - Create Rml `Context` sized to client rect.
  - Call `context->Update()` once per frame, then `context->Render()` after
    the emulator's `UploadAndPresent` draws the framebuffer but before
    `Present()`.
  - Handle device-lost: rebuild backend GPU resources (textures, buffers).
- **P3-T5** Wire into `EmulatorShell` lifecycle. Keep current chrome alive
  during this phase (parallel-mode); the old Win32 menus and dialogs still
  work — this phase only proves the overlay composes correctly.

### Phase 4 — Borderless Win11 chrome + title bar + nav layer

Gate to exit: window is borderless, title bar drag/min/max/close/double-click
fullscreen all work as well as native; Aero Snap and Snap Layouts still work.

- **P4-T1** Strip `WS_OVERLAPPEDWINDOW` chrome from the Casso main window
  registration; switch to `WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX |
  WS_MAXIMIZEBOX` (or the standard borderless recipe — finalized in P0-T5).
- **P4-T2** WM_NCCALCSIZE custom client-area extension; WM_NCHITTEST returning
  HTCAPTION over the title bar element rect from RmlUi, HTCLIENT elsewhere,
  HTLEFT/HTRIGHT/HTTOP/HTBOTTOM for resize edges, HTMINBUTTON/HTMAXBUTTON/
  HTCLOSE over their respective RML element rects (so OS-level Snap Layouts
  appear on hover over the maximize button — Win11 only).
- **P4-T3** `DwmExtendFrameIntoClientArea` + `DWMWA_WINDOW_CORNER_PREFERENCE
  = DWMWCP_ROUND` + optional Mica backdrop on the non-emulated chrome region.
- **P4-T4** `TitleBarController`: drives the RML title-bar document, exposes
  drag region, dispatches min/max/close/double-click-fullscreen, delegates
  fullscreen toggle to existing `D3DRenderer::ToggleFullscreen`.
- **P4-T5** `NavLayerController`: top-level RML document with one panel per
  legacy menu (File, Machine, View, Disk, Edit, Help). Each item dispatches
  to the same command IDs the old menus used (FR-026, SC-006).

### Phase 5 — Themes (ThemeManager + 3 built-ins)

Gate to exit: hot-swap works across the three built-ins within one frame
(SC-002); malformed themes excluded gracefully (FR-036); user-authored theme
discovery without restart (FR-035).

- **P5-T1** `ThemeLoader`: parses `theme.json`, validates `$cassoThemeVersion`,
  resolves asset paths, returns a `LoadedTheme` struct or a structured error.
- **P5-T2** `ThemeManager`:
  - `Discover()` — scan `Themes/` for candidate dirs.
  - `Activate(name)` — unload current Rml documents/stylesheets, clear
    style sheet cache, load new `.rml` + `.rcss`, apply `theme.json` CRT
    defaults if user has not overridden them, raise an `OnThemeChanged` event.
  - `ReloadCurrent()` — for asset-hot-iteration in dev builds.
- **P5-T3** Author **Skeuomorphic** built-in theme: layout + RCSS for title bar,
  drive widgets, LEDs, nav layer, settings panel; beige/cream/anodized palette;
  CRT defaults (scanlines off, mild bloom).
- **P5-T4** Author **Dark Modern** built-in theme: dark palette with glowing
  LED accents; CRT defaults (subtle scanlines, neon bloom).
- **P5-T5** Author **Retro Terminal** built-in theme: phosphor green/amber,
  scanlines on by default at high intensity, color bleed on.
- **P5-T6** Embed built-in themes into the Casso resource script; verify
  `EnsureThemes()` extracts them on first launch.

### Phase 6 — Custom RmlUi elements (drive widgets + LEDs)

Gate to exit: drag-drop and click-to-browse mount disks via existing
EmulatorShell command path; spinning + door animations driven by existing
motor/disk-active signals; LEDs reflect three states.

- **P6-T1** Decide per element: RCSS-only (preferred for LEDs) vs. custom
  `Rml::Element` subclass (required for drive widget because of drag-drop +
  animation state machine). Decision lives in research.md.
- **P6-T2** `LedElement`: RCSS-only — three CSS classes
  (`.led--idle`, `.led--present`, `.led--active`) toggled from C++ on the
  parent drive widget. Glow via box-shadow / radial gradient in RCSS.
- **P6-T3** `DriveWidgetElement` custom element:
  - `OnDragDrop` from Win32 (`WM_DROPFILES`) routed through RmlInputBridge
    into the element under cursor.
  - Click-to-browse opens `IFileDialog` filtered to `.dsk`, `.nib`, `.woz`,
    `.po` (FR-022b).
  - Eject affordance: child element with its own hit region.
  - Spinning animation: RCSS `@keyframes` toggled by an `is-spinning` class
    driven from the existing CPU-thread motor-on signal observed by the
    UI thread via `atomic<bool>` (matches the audio system's existing read
    pattern).
  - Door open/close animation: RCSS transition triggered on state change.
- **P6-T4** `DriveWidgetState` in `EmulatorShell` populated from disk insert/
  eject and motor-on/off events; read by RmlInputBridge each frame to update
  element classes.

### Phase 7 — Settings panel

Gate to exit: every requirement in FR-001 … FR-011 satisfied; SC-001 (60s) and
SC-008 (90% findability) achievable in a UX dry-run.

- **P7-T1** `SettingsPanelState`: in-memory snapshot of current selections.
  `LoadFromMachine(name)`, `Apply()`, `Cancel()`, `IsDirty()`.
- **P7-T2** RML layout `settings_panel.rml`: sections
  1. **Machine** selector at top (FR-002).
  2. **Emulation** (speed, write protect, floppy sound + mechanism).
  3. **Video** (color mode, CRT effects panel: brightness, scanlines,
     bloom, color bleed).
  4. **Theme** (list of installed themes, preview, refresh button).
  5. **Hardware** (tree view bound to the selected machine's component list,
     with checkbox states driven by `capabilityFlag`).
  6. Footer: Apply / Cancel / Reset-to-defaults.
- **P7-T3** Machine-selector change → reload `SettingsPanelState` from that
  machine's merged config without closing the dialog (FR-002 acceptance).
- **P7-T4** Hardware tree (FR-004 .. FR-008) — `optional` interactive,
  `required` checked+disabled, `platform-locked` checked+disabled+tooltip.
- **P7-T5** Apply path:
  - Immediate-effect fields (speed, video mode, floppy sound) push through
    existing `EmulatorShell` atomics + command queue (FR-011).
  - Reset-required fields (hardware tree changes) show a confirmation, then
    schedule a machine reset (FR-010).
  - All applied changes flush through `UserConfigStore::SaveDelta`.
- **P7-T6** Keyboard navigation (FR-044): verify Tab cycles all controls in
  visual order; Space/Enter activate; Escape cancels; RCSS `:focus` styles
  present in all three built-in themes.
- **P7-T7** Open-while-running (FR-041): no pause; the panel is purely a
  view over a transient state object; emulation thread is unaffected.

### Phase 8 — CRT post-processing

Gate to exit: 60 fps maintained with all CRT effects on; SC-005 chrome budget
still met; per-theme defaults respected; user override path works.

- **P8-T1** Port + integrate `crt_scanlines.hlsl` (MIT source attributed).
- **P8-T2** Port + integrate `crt_bloom.hlsl` (separable Gaussian or KFM).
- **P8-T3** Port + integrate `crt_color_bleed.hlsl` (NTSC-style chroma lateral
  spread).
- **P8-T4** `crt_composite.hlsl`: takes the emulated framebuffer texture +
  applies enabled effects in order; final output blits to the viewport
  region (letterboxed/pillarboxed per FR-043).
- **P8-T5** `D3DRenderer` exposes a post-process pass run **before** the
  RmlUi composite pass. Parameters fed from `GlobalUserPrefs.crt.*`.
- **P8-T6** Brightness control wired live as the user adjusts it (FR-038).

### Phase 9 — Retirement + polish

Gate to exit: legacy code deleted; CHANGELOG + README updated; full test
suite green; code analysis green; all acceptance scenarios pass.

- **P9-T1** Delete `OptionsDialog.{h,cpp}` and `MachinePickerDialog.{h,cpp}`
  and their resource entries (FR-027).
- **P9-T2** Remove menu bar registration; verify the existing menu command
  IDs are still served by `NavLayerController`.
- **P9-T3** CHANGELOG entry: `feat(ui): full RmlUi-based chrome + theme system`.
- **P9-T4** README updates: dependency table, screenshots of built-in themes,
  user-theme authoring quickstart.
- **P9-T5** Final pass with `scripts\Build.ps1 -RunCodeAnalysis` on Debug
  and Release, x64 and ARM64.

## Open Technical Questions (need user input before relevant phase)

> **Status:** All resolved 2026-05-20. Decisions recorded inline.

These are flagged here so they're answered before they block work:

1. **RmlUi vendoring mechanism** — git subtree vs. plain copy under
   `External/RmlUi/`? Subtree preserves upstream history and makes re-vendor
   trivial; plain copy is dead simple but loses provenance. *Recommend: plain
   copy with `README.casso.md` recording tag + SHA. The "never download
   external binaries" security rule supports keeping it offline.* — **DECIDED:
   plain copy with `External/RmlUi/README.casso.md` capturing upstream tag +
   commit SHA.**
2. **RmlUi version pin** — latest upstream stable tag at the time of P1-T1.
   Need user OK on the exact tag. — **DECIDED: pin to the latest stable
   upstream tag at the moment P1-T1 begins; record exact tag + SHA in
   `External/RmlUi/README.casso.md`.**
3. **Custom backend vs. fork RmlUi's DX11 backend** — recommended: write our
   own (smaller, fits our EHM/code-style, shares the existing device). —
   **DECIDED: write our own backend.**
4. **Specific shader source authors to port** — recommended set:
   - Scanlines: `crt-pi` (MIT) or the libretro `image-adjustment` chain.
   - Bloom: any of the libretro `bloom` shaders (MIT/PD).
   - Color bleed: libretro `ntsc-adaptive` chroma stage (MIT).
   *User should confirm or substitute.* — **DECIDED: adopt the recommended
   set — `crt-pi` (scanlines), libretro `bloom` (bloom), `ntsc-adaptive`
   chroma stage (color bleed). All MIT. Full attribution in
   `Casso/Shaders/README.md`.**
5. **Font** — default theme font. Candidates: Inter (SIL OFL), JetBrains Mono
   (SIL OFL), B612 (SIL OFL). Retro Terminal needs a phosphor/bitmap font
   (e.g., VT323 — SIL OFL). — **DECIDED: Inter (SIL OFL) for default chrome /
   Skeuomorphic / Dark Modern themes; VT323 (SIL OFL) for Retro Terminal.
   Both vendored under `Themes/<name>/fonts/` with license files alongside.**
6. **Mica backdrop on/off by default** — Mica looks great but only renders
   over the desktop wallpaper, which can clash with skeuomorphic themes. Per
   built-in theme? Per `theme.json` flag? *Recommend: `theme.json` field
   `useMicaBackdrop: bool` default false; built-in Dark Modern sets it true.*
   — **DECIDED: add `useMicaBackdrop` boolean to `theme.json` schema, default
   false. Built-in Dark Modern sets it true; Skeuomorphic and Retro Terminal
   leave it false.**
7. **Settings panel "Reset to defaults" semantics** — does it delete the
   `_user.json` file entirely or write an empty `{ "$cassoMachineVersion": N }`?
   *Recommend: delete file — minimal storage, clean state.* — **DECIDED:
   delete the file. `UserConfigStore::Reset` performs the deletion and forces
   a reload of the read-only default.**
8. **GPL shader handling** — confirmed excluded. Need decision on whether to
   add a CI guard (e.g., a `scripts/CheckShaderLicenses.ps1`) that fails if
   GPL identifiers appear under `Casso/Shaders/`. — **DECIDED: add
   `scripts/CheckShaderLicenses.ps1` invoked from the existing build pipeline.
   Fails the build if any file under `Casso/Shaders/` contains
   case-insensitive "GPL", "GNU General Public", or "copyleft" tokens outside
   designated attribution-comment markers.**
9. **ARM64 RmlUi build** — RmlUi upstream should build cleanly with v145 +
   ARM64 but needs verification before P1-T1 is marked done. — **DECIDED:
   ARM64 build verification is a hard done-criterion for P1-T1; the vcxproj
   must produce a working static lib for x64 Debug, x64 Release, ARM64 Debug,
   and ARM64 Release.**
10. **Drag-drop registration** — currently a single `RegisterDragDrop` on the
    main window? Or per–drive-widget element? *Recommend: keep main-window
    registration, route hit-tested drop to the element under cursor via
    RmlInputBridge.* — **DECIDED: single main-window `RegisterDragDrop`; the
    `RmlInputBridge` hit-tests the drop point and routes the event to the
    drive-widget element under the cursor (or rejects the drop if none).**

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| Third-party dependency: **RmlUi** (vendored) | FR-018, FR-026, FR-027, FR-029, FR-032, FR-033, FR-044, FR-046 collectively require a real layout engine, RCSS parser, focus/tab manager, text shaping, animation system, custom-element framework, and hot-reload. Hand-rolling this in D3D primitives would be many thousands of LOC of high-risk UI code outside this project's competence and unrelated to its emulator domain. | "Build it ourselves with D3D11 + DWrite" was the prior plan; the explicit binding directive in the planning input rejects it. |
| Third-party source: **MIT/PD CRT shader ports** | FR-039, FR-040 require scanlines, phosphor bloom, and color bleed with credible visual fidelity. CRT shader authoring is its own specialty. | Hand-writing these from scratch was the prior plan; rejected by the same binding directive. We adopt only MIT/PD sources, with full attribution, to preserve Casso's MIT license. |
| **Two separate user-config files** (per-machine `_user.json` + one `GlobalUserPrefs.json`) | See `research.md` R6 for the canonical rationale: machine-specific settings (speed, video mode, hardware tree, last-mounted disks) must follow the machine on import/export; UI preferences (theme, CRT params, window) span machines. | "Single config file for everything" — rejected because per-machine settings must follow the machine on import/export. |
| **Constitution amendment** (Tech Constraints) | Required to legalize the two dependencies above. | "Just do it without amending" — rejected because the constitution is binding and silently violating it sets a corrosive precedent. |

## Progress Tracking

- [x] Phase 0 plan written (research.md generated alongside this file)
- [x] Phase 1 design written (data-model.md, contracts/, quickstart.md)
- [x] Constitution Check evaluated (PASS w/ documented justified violation)
- [x] Agent context updated (`.github/copilot-instructions.md` already references
      `specs/007-ui-overhaul/plan.md`)
- [x] Constitution amendment v1.5.0 merged (Approved Third-Party Dependencies
      allowlist added 2026-05-20; RmlUi + 3 shaders listed)
- [x] Phase 2 task generation (`/speckit.tasks`) — tasks.md committed
