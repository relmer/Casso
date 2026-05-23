# Tasks: 007 UI Overhaul (Native DX UI Reset)

**Input**: `spec.md`, `plan.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`
**Feature Dir**: `C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul`
**Reset Note**: Regenerated against the post-pivot plan that excises RmlUi from the
runtime entirely and replaces it with a from-scratch native D3D11 + DirectWrite UI
runtime under `Casso/Ui/`. The prior tasks.md predated the RmlUi-removal pivot and
is fully superseded. Every checkbox is intentionally unchecked; nothing is assumed
done until re-verified in code, tests, and a clean `rg` sweep.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Maps the task to a spec user story (US1–US6) where applicable
- Absolute file paths included in every task

## Phase Dependencies (at a glance)

```text
P0 (RmlUi excision)   ── HARD GATE ──▶ P1 (foundational runtime) ──▶ P2 (chrome)
                                                                  └─▶ P3.0 (EmulatorShell decomposition) ──▶ P3 (settings)
                                                                                                                  └─▶ P3.1 (smoke-test polish) ──▶ P4 (themes/CRT) ──▶ P5 (polish/validation)
```

No P1+ task may begin until **every** P0 task is complete, the build is green
across all four configurations (Debug/Release × x64/ARM64), `scripts\Build.ps1
-RunCodeAnalysis` is clean, `scripts\RunTests.ps1` passes, and a repo-wide
`rg -n "Rml|RMLUI" Casso CassoCore CassoEmuCore CassoCli UnitTest External`
returns zero hits.

---

## Phase P0: Excise RmlUi (HARD GATE — must land first, single commit series)

**Purpose**: Remove every trace of the RmlUi runtime, dependency, source, shader,
and test before any new pipeline lands. The two pipelines must never coexist in
a single commit. Covers **FR-053**, **FR-054**, **SC-011**.

### P0a — Source file deletions (Rml-prefixed UI sources)

- [X] T001 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlBackend_D3D11.cpp`
- [X] T002 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlBackend_D3D11.h`
- [X] T003 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlFontEngine_DWrite.cpp`
- [X] T004 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlFontEngine_DWrite.h`
- [X] T005 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlInputBridge.cpp`
- [X] T006 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlInputBridge.h`
- [X] T007 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlSystemInterface.cpp`
- [X] T008 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlSystemInterface.h`

### P0b — Shader and resource deletions

- [X] T009 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Shaders\Ui\rml_textured.hlsl`
- [X] T010 [P] Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Shaders\Ui\rml_untextured.hlsl`
- [X] T011 [P] Sweep `C:\Users\relmer\repos\relmer\Casso\Casso\Resources\` for any `*.rml`, `*.rcss`, and Rml-bundled fonts/images and delete them; record removed files in the commit body
- [X] T012 [P] Sweep `C:\Users\relmer\repos\relmer\Casso\Assets\` for any `*.rml`, `*.rcss`, and Rml-bundled fonts/images and delete them; record removed files in the commit body

### P0c — Vendored dependency tree

- [X] T013 Recursively delete the entire `C:\Users\relmer\repos\relmer\Casso\External\RmlUi\` directory (project, source, headers, samples, license — everything)

### P0d — Build wiring scrub

- [X] T014 Remove the RmlUi project entry and **ALL** of its associated `{GUID}` configuration rows from `C:\Users\relmer\repos\relmer\Casso\Casso.sln` (look up the actual GUID in `Casso.sln`)
- [X] T015 In `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj`, strip `RMLUI_STATIC_LIB` and `RMLUI_NO_THIRDPARTY_CONTAINERS` from every `<PreprocessorDefinitions>` element across all six configuration blocks (Debug/Release × x64/ARM64, plus the two Analyze rows)
- [X] T016 In `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj`, strip `..\External\RmlUi\Include` from every `<AdditionalIncludeDirectories>` element across all six configuration blocks
- [X] T017 In `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj`, remove the four `<ClCompile Include="Ui\Rml*.cpp" />` entries and the four `<ClInclude Include="Ui\Rml*.h" />` entries
- [X] T018 In `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj`, remove the two `<None Include="Shaders\Ui\rml_*.hlsl" />` entries
- [X] T019 In `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj`, remove the `<ProjectReference Include="..\External\RmlUi\RmlUi.vcxproj"> … </ProjectReference>` block in its entirety
- [X] T020 In `C:\Users\relmer\repos\relmer\Casso\UnitTest\UnitTest.vcxproj`, remove every Rml-related `<ClCompile>` entry (`RmlBackendSmokeTests.cpp`, `RmlInputBridgeTests.cpp`) and any Rml include-path / preprocessor-define inherited from the Casso project

### P0e — PCH and call-site scrub

- [X] T021 Remove `#include <RmlUi/Core.h>` and `#include <RmlUi/Debugger.h>` (and any other `<RmlUi/...>` lines) from `C:\Users\relmer\repos\relmer\Casso\Casso\Pch.h`
- [X] T022 Strip every `#include "Rml*.h"` and `#include <RmlUi/...>` reference, plus all `Rml::*` symbol uses, from `C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.h`; replace removed init/shutdown calls with `// UiShell takes over in P1` TODO stubs that do nothing
- [X] T022a Drop `EmulatorShell` wiring calls to `UiShell`, after-blit hooks, settings panel listeners, drive widget instancers, and related Rml UI callbacks behind clean no-op compile paths so the app still launches after P0 (black/bare window OK)
- [X] T023 Strip Rml render bindings, includes, and per-frame Rml render/update hooks from `C:\Users\relmer\repos\relmer\Casso\Casso\D3DRenderer.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\D3DRenderer.h`; leave a TODO stub for the new painter integration in P1
- [X] T024 Strip Rml input-pump hooks and includes from `C:\Users\relmer\repos\relmer\Casso\Casso\Main.cpp`
- [X] T025 Strip Rml hit-test / chrome-routing references from `C:\Users\relmer\repos\relmer\Casso\Casso\Window.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Window.h`
- [X] T026 Strip the Rml-document/font bootstrap (but **keep** the `Themes/` directory bootstrap) from `C:\Users\relmer\repos\relmer\Casso\Casso\AssetBootstrap.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\AssetBootstrap.h`
- [X] T027 Strip any `IDR_RML_*` / Rml-asset resource IDs from `C:\Users\relmer\repos\relmer\Casso\Casso\resource.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.rc`
- [X] T027a Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\UiShell.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\UiShell.h` outright; P1 rewrites them from native contracts
- [X] T027b Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\LedElement.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\LedElement.h` outright; P2 rewrites as `Chrome\LedIndicator`
- [X] T027c Delete `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetElement.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetElement.h` outright; P2 rewrites as `Chrome\DriveWidget`
- [X] T027d Stub `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanel.cpp` and `.h`: empty class skeleton in `.h` that compiles standalone; `.cpp` returns `S_OK` from every method (P3 rewrites)
- [X] T027e Verify `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanelState.cpp` and `.h` are Rml-free; if any Rml header is included, remove it while keeping the state logic (P3 keeps/extends)
- [X] T027f Stub `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\ThemeManager.cpp` and `.h`: drop `Rml::Context*` / `Rml::ElementDocument*`; keep `Discover()` returning `S_OK` and an in-memory active-theme name (P1/P5 fleshes out)
- [X] T027g Remove any Rml references from `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\ThemeLoader.cpp` and `.h` while keeping JSON parsing
- [X] T027h Stub `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\TitleBar.cpp` and `.h`: `Show`/`Hide` no-op; keep `TitleBarLayout` pure logic untouched (P2 rewrites)
- [X] T027i Stub `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\NavLayer.cpp` and `.h`: `Show`/`Hide` no-op; keep parity table + `EmitParityMarkdown()` (P2 rewrites)
- [X] T027j Stub UI-facing methods in `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetController.cpp` and `.h` (`LoadDocument` / `UnloadDocument` no-op; `HitTest` returns `nullptr`); keep state-pump path (P2 rewrites)
- [X] T028 Sweep the rest of `C:\Users\relmer\repos\relmer\Casso\Casso\` for any remaining `Rml`/`RMLUI` references (`rg -n "Rml|RMLUI" C:\Users\relmer\repos\relmer\Casso\Casso`) and strip each; record the swept file list in the commit body

### P0f — Test deletions tied to the retired pipeline

- [X] T029 [P] Delete `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\RmlBackendSmokeTests.cpp`
- [X] T030 [P] Delete `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\RmlInputBridgeTests.cpp`
- [X] T031 Sweep `C:\Users\relmer\repos\relmer\Casso\UnitTest\` for any other `Rml`/`RMLUI` references (`rg -n "Rml|RMLUI" C:\Users\relmer\repos\relmer\Casso\UnitTest`) and strip each; record the swept file list in the commit body

### P0g — Constitution amendment

- [X] T032 Amend `C:\Users\relmer\repos\relmer\Casso\.specify\memory\constitution.md`: delete the `RmlUi | MIT | Casso | External/RmlUi/ | HTML/CSS-style UI framework (spec 007)` row from the **Approved Third-Party Dependencies** table; add a sync-impact note at the top of the file recording the reversal of spec 007's prior allowlist addition; bump the version footer from `1.5.0` to `1.6.0` (MINOR — materially changed Tech Constraints by removing an approved dependency); update the `Last Amended` date

### P0h — Gate verification

- [ ] T033 Run `scripts\Build.ps1` and confirm all four configurations (Debug/Release × x64/ARM64) build clean with zero errors and zero new warnings; capture output in the commit body
- [ ] T034 Run `scripts\Build.ps1 -RunCodeAnalysis` and confirm zero analysis findings on the Casso, CassoCore, CassoEmuCore, CassoCli, and UnitTest projects
- [X] T035 Run `scripts\RunTests.ps1` and confirm all surviving tests pass
- [X] T036 Run `rg -n "Rml|RMLUI" C:\Users\relmer\repos\relmer\Casso\Casso C:\Users\relmer\repos\relmer\Casso\CassoCore C:\Users\relmer\repos\relmer\Casso\CassoEmuCore C:\Users\relmer\repos\relmer\Casso\CassoCli C:\Users\relmer\repos\relmer\Casso\UnitTest C:\Users\relmer\repos\relmer\Casso\External` and confirm zero hits (SC-011 audit)
- [X] T037 Launch `Casso.exe` and confirm the borderless window + emulated viewport still come up (no chrome, no settings, no nav — intentional regression; P1 reintroduces them)

**🛑 HARD GATE (P0 → P1)**: SC-011 must be provable by T036's zero-hit `rg` sweep
**and** T033/T034/T035/T037 must all pass before any P1 task begins.

---

## Phase P1: Foundational native UI runtime (Blocking Prerequisites for US1–US5)

**Purpose**: Build the painter, text renderer, input translator, hit-tester,
focus manager, layout, animation broker, and theme loader that every later
phase depends on. No user-visible chrome or settings yet. Covers **FR-046**
prerequisites for US3 + US5.

### P1a — Documentation refresh (in-place, single small commit)

- [X] T038a Replace `research.md` R6 with the RmlUi excision strategy: delete/stub decisions for non-Rml-prefixed files, remove vendored/build/PCH references, and keep P0 compiling without the old runtime
- [X] T038b Replace `research.md` R7 with the D2D-on-D3D11 text pipeline decision: shared DXGI device, `ID2D1Bitmap1` over the swap-chain back buffer, DirectWrite layout cache, render after geometry before `Present`
- [X] T038c Append `research.md` R8 for hit-test architecture: shared DPI rect tree for client input, `WM_NCHITTEST`, drag/drop, and chrome routing
- [X] T038d Append `research.md` R9 and R10 for focus/keyboard policy plus modal/popup layer ownership (Tab/Shift-Tab, activation keys, Escape, focus cues, `ModalScrim`, dropdown/tooltips)
- [X] T038e Append `research.md` R11 and R12 for drag-drop/file-open carve-out and debug-tool carve-out (`IDropTarget`, `IFileOpenDialog`, `DiskIIDebugDialog`, `DebugConsole`)
- [X] T038f Append `research.md` R13 for theme-token broadcast so validated tokens, drive profile, CRT defaults, and backdrop flags update all surfaces before the first post-switch frame
- [X] T039 Append `HardwareComponentEntry`, `SettingsPanelState`, `WindowPlacementProfile`, and transient `UiDrawList` entities to `C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\data-model.md`

### P1b — New shaders

- [X] T040 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Shaders\Ui\ui_solid.hlsl` (solid + linear-gradient rect with corner radius, alpha)
- [X] T041 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Shaders\Ui\ui_textured.hlsl` (textured quad with 9-slice insets and uv transform)
- [X] T042 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Shaders\Ui\ui_glyph.hlsl` (cleartype-friendly glyph sampling path used by D2D handoff debug + LED labels; doc the D2D-on-D3D11 dependency)
- [X] T043 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Shaders\Ui\ui_glow.hlsl` (additive radial soft-glow sample for LED active state per FR-025)
- [X] T044 In `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj`, add `<None Include="Shaders\Ui\ui_solid.hlsl" />`, `<None Include="Shaders\Ui\ui_textured.hlsl" />`, `<None Include="Shaders\Ui\ui_glyph.hlsl" />`, `<None Include="Shaders\Ui\ui_glow.hlsl" />` and the matching HLSL build steps to embed bytecode (mirror the existing CRT shader pattern)

### P1c — New native UI runtime files

- [X] T045 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DxUiPainter.h` (painter API: rects, 9-slice, gradients, textured quads, soft-glow draw commands; UiDrawList consumer)
- [X] T046 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DxUiPainter.cpp` (D3D11 geometry batching, single dynamic vertex buffer, per-draw constants, device-lost rebuild hook) for the native geometry pipeline
- [X] T047 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DwriteTextRenderer.h` (text/layout API + glyph-run cache key `(family,weight,size,dpi,text)`)
- [X] T048 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DwriteTextRenderer.cpp` (D2D-on-D3D11 via `IDXGISurface` + `ID2D1Bitmap1` over the swap chain back buffer; renders after geometry pass, before `Present`; cache + device-lost rebuild) per R7
- [X] T049 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\UiInput.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\UiInput.cpp` (translate `WM_MOUSE*`, `WM_KEY*`, `WM_CHAR`, `WM_*BUTTON*`, `WM_MOUSEWHEEL`, `WM_SETFOCUS`/`WM_KILLFOCUS` into a typed `UiEvent` stream with modifier state)
- [X] T050 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\HitTester.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\HitTester.cpp` (rect tree + `WM_NCHITTEST` → `HTCAPTION`/`HTMINBUTTON`/`HTMAXBUTTON`/`HTCLOSE`/`HTCLIENT` + fixed DPI-scaled resize-edge margin) per R8
- [X] T051 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\FocusManager.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\FocusManager.cpp` (Tab / Shift-Tab / Enter / Space / Escape per FR-044, focus cue rendering callback)
- [X] T052 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Layout.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Layout.cpp` (stack/grid primitives, per-window DPI helpers per R5)
- [X] T053 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Animation.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Animation.cpp` (tween + `DriveSyncEvent` broker satisfying FR-050 / SC-010 one-frame skew)
- [X] T054 [P] Rewrite `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\ThemeManager.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\ThemeManager.cpp` as the native, non-Rml form (owns `Theme*` selection, per-asset GPU texture cache, glyph fonts via `IDWriteFontSetBuilder1`, hot-swap entry point) per R13
- [X] T055 [P] Rewrite `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\ThemeLoader.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\ThemeLoader.cpp` as the native JSON loader honoring `contracts/theme-metadata.schema.json` + `$cassoThemeVersion` migration (FR-045) and malformed-theme exclusion (FR-036)
- [X] T056 Rewrite `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\UiShell.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\UiShell.cpp` from scratch as the top-level UI owner, message router, focus manager owner, and frame composer (consumes `UiInput` → routes through `HitTester` → dispatches to widgets → emits `UiDrawList` → flushes `DxUiPainter` then `DwriteTextRenderer`)

### P1d — Integration with existing renderer / window / shell / bootstrap

- [X] T057 Extend `C:\Users\relmer\repos\relmer\Casso\Casso\D3DRenderer.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\D3DRenderer.h` to create a shared `ID2D1Device` / `ID2D1DeviceContext` from the existing DXGI device, expose the back-buffer `IDXGISurface` to `DwriteTextRenderer`, and call `UiShell::OnDeviceLost` / `OnDeviceRestored` on the existing device-lost recovery path (spec Edge Case)
- [X] T058 Wire `WM_NCHITTEST` in `C:\Users\relmer\repos\relmer\Casso\Casso\Window.cpp` to consult `HitTester` (replacing the P0 stub) per R8 / FR-028
- [X] T059 Replace the P0 init/shutdown TODO stubs in `C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.h` with `UiShell` construction, teardown, per-frame composition tick, and input event routing
- [X] T060 Wire the `UiInput` pump into `C:\Users\relmer\repos\relmer\Casso\Casso\Main.cpp`'s message loop (replacing the P0 stub)
- [X] T061 Rework `C:\Users\relmer\repos\relmer\Casso\Casso\AssetBootstrap.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\AssetBootstrap.h` to extract built-in **native** theme bundles (plain image + json, no Rml documents) per FR-030 / FR-037

### P1e — Foundational tests (all isolated; mocked filesystem per FR-057 / SC-013)

- [X] T062 [P] Create `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\DxUiPainterTests.cpp` (geometry batching invariants, 9-slice math, vertex-buffer growth)
- [X] T063 [P] Create `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\LayoutTests.cpp` (stack/grid measure + arrange, DPI scaling)
- [X] T064 [P] Create `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\HitTesterTests.cpp` (rect-tree routing + NC mapping coverage)
- [X] T065 [P] Create `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\FocusManagerTests.cpp` (Tab order, Esc dismiss, focus cue trigger)
- [X] T066 [P] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\ThemeLoaderTests.cpp` against the native loader (JSON parse, `$cassoThemeVersion` migration, malformed-theme exclusion FR-036)
- [X] T067 [P] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\ThemeManagerTests.cpp` against the native ThemeManager (hot-swap, GPU asset retention semantics — no real GPU calls, mocked device)
- [X] T068 [P] Create `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\AnimationSyncTests.cpp` (FR-050 / SC-010 within-one-frame skew)
- [X] T069 [P] Create `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\NcHitTestTests.cpp` (borderless WM_NCHITTEST classification per FR-028)
- [X] T070a In `C:\Users\relmer\repos\relmer\Casso\UnitTest\UnitTest.vcxproj`, add `<ClCompile Include="…">` entries for T062–T069's new/rewritten test files and any production `.cpp` files linked into the test project for direct unit coverage
- [X] T070b In `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj`, add production entries for `DxUiPainter.cpp`, `DwriteTextRenderer.cpp`, `UiInput.cpp`, `HitTester.cpp`, `FocusManager.cpp`, `Layout.cpp`, `Animation.cpp`, `UiShell.cpp`, `ThemeManager.cpp`, and `ThemeLoader.cpp` plus matching headers

### P1f — Phase gate

- [X] T071 Run `scripts\Build.ps1` clean across all four configurations
- [ ] T072 Run `scripts\Build.ps1 -RunCodeAnalysis` clean
- [X] T073 Run `scripts\RunTests.ps1` — all P1 tests pass, no regressions in surviving Phase-2/CPU/Emu tests

**Checkpoint**: Native runtime primitives exist and are independently tested.
User-story phases (P2/P3 in parallel, then P4) may start.

---

## Phase P2: Chrome surfaces (Priority: P1 / P2 user stories US3, US5; partial US1/US2)

**Goal**: Replace the bare borderless window with the visible native chrome:
title bar, nav layer, drive widgets, LED indicators, drop target, click-to-browse.

**Independent Test**: Launch Casso; observe custom title bar with min/max/close
working, nav layer reaching every former Win32 menu command (SC-006), drive
widgets responding to drag-drop **and** click-to-browse with door animation
synced to mount sound (FR-050 / SC-010), LEDs glowing at FR-025 dimensional
minima.

### P2a — Widget primitives (T074–T093: 10 widgets × {.h, .cpp})

- [X] T074 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Button.h`
- [X] T075 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Button.cpp`
- [X] T076 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Checkbox.h`
- [X] T077 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Checkbox.cpp`
- [X] T078 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Slider.h`
- [X] T079 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Slider.cpp`
- [X] T080 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Dropdown.h`
- [X] T081 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Dropdown.cpp`
- [ ] T082 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\ListView.h`
- [ ] T083 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\ListView.cpp`
- [X] T084 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\TreeView.h`
- [X] T085 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\TreeView.cpp`
- [ ] T086 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\TextField.h` (read-only labels + numeric editor)
- [ ] T087 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\TextField.cpp`
- [X] T088 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\TabStrip.h`
- [X] T089 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\TabStrip.cpp`
- [X] T090 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\ModalScrim.h` (in-canvas modal overlay per R10)
- [X] T091 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\ModalScrim.cpp`
- [X] T092 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Tooltip.h` (FR-008 platform-lock tooltip target)
- [X] T093 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Widgets\Tooltip.cpp`

### P2b — Chrome surfaces (move existing files into `Chrome/` subdir + new files)

- [X] T094 [US5] Move `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\TitleBar.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\TitleBar.h` to `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Chrome\TitleBar.cpp` / `.h`, rewrite contents against the native runtime (no Rml), implement drag, double-click toggle fullscreen, min/max/close, right-click system menu (FR-018, FR-019, FR-020, FR-028); update `Casso.vcxproj` paths
- [X] T095 [US5] Move `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\NavLayer.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\NavLayer.h` to `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Chrome\NavLayer.cpp` / `.h`, rewrite as native dropdown panels covering every former Win32 menu command via the existing IDM map (FR-026, SC-006); update `Casso.vcxproj` paths
- [X] T096 [US3] Move `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetElement.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetElement.h` to `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Chrome\DriveWidget.cpp` / `.h`, rewrite as a native 9-slice drive face with door, eject affordance, spinning disk during motor-on, ≥96×64 logical px (FR-021, FR-023, FR-024, FR-049, FR-050); update `Casso.vcxproj` paths
- [X] T097 [US3] Move `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\LedElement.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\LedElement.h` to `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Chrome\LedIndicator.cpp` / `.h`, rewrite as a native soft-glow LED honoring FR-025 dimensional minima (≥12 px lit core + ≥4 px halo in active); update `Casso.vcxproj` paths

### P2c — Drag-drop, file-open, variant treatment

- [X] T098 [US3] Rework `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DragDropTarget.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DragDropTarget.h` so drop coordinates route through the new `HitTester` for per-widget acceptance (FR-022) per R11
- [X] T099 [US3] Wire click-to-browse on the drive-widget body (not the eject affordance) to `IFileOpenDialog` with the supported-extension filter list in `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Chrome\DriveWidget.cpp` (FR-022b)
- [ ] T100 [US3] Implement Apple II / II+ / IIe / IIc variant treatment driven by `Theme::driveVisualProfile` + `variantId` in `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Chrome\DriveWidget.cpp` (FR-051, SC-009)
- [X] T101 Retire `C:\Users\relmer\repos\relmer\Casso\Casso\MenuSystem.cpp` (and `.h` if present): delete the file(s) and remove their `<ClCompile>`/`<ClInclude>` entries from `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj`; nav layer now owns command surfaces

### P2d — Tests + vcxproj updates

- [X] T102 [P] [US5] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\NavLayerTraceabilityTests.cpp` as `ChromeCommandRoutingTests.cpp` (rename file) — Nav → IDM dispatch parity across the full menu map (SC-006)
- [X] T103 [P] [US5] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\TitleBarHitTestTests.cpp` against the native chrome (drag band, system buttons, resize edges)
- [X] T104 [P] [US5] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\TitleBarLayoutTests.cpp` against the native title bar layout (DPI-scaled bands, min/max/close positions)
- [X] T105 [P] [US3] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\DriveWidgetStateTests.cpp` against the rewritten drive widget (door state machine + sync events FR-050)
- [X] T106 [P] [US3] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\CrtLetterboxLayoutTests.cpp` against the native chrome viewport calculation (4:3 letterbox/pillarbox FR-043)
- [ ] T106a [P] [US5] Create or extend `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\Win11DwmHelpersTests.cpp` to verify every DWM call site is gated by `IsWindows11OrGreater()` (FR-042)
- [X] T106b [P] [US3] Verify `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\AutoMountTests.cpp` still compiles after `Chrome\DriveWidget` moves; rewrite includes/namespaces if not
- [X] T107 [US5] Update `C:\Users\relmer\repos\relmer\Casso\UnitTest\UnitTest.vcxproj` to reflect renamed/relocated test files and add new chrome-bound tests
- [X] T108 [US5] Update `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj` to add every new widget + chrome `.cpp`/`.h` from T074–T100 under the appropriate `<ClCompile>` / `<ClInclude>` `<ItemGroup>`

### P2e — Runtime validation

- [ ] T109 [US5] Capture screenshot matrix entries M1 (startup chrome), M2 (nav-layer dropdown visible), M3 (NC controls hover states), M5 (drive door open), M6 (drive door closed + spin), M7 (LED active glow) under `C:\Users\relmer\repos\relmer\Casso\TestResults\007-ui-overhaul\` per `quickstart.md`

**Checkpoint**: Custom chrome is visible, interactive, theme-driven, and tested.

---

## Phase P3.0: EmulatorShell decomposition (refactor; safety prerequisite for settings panel + machine-switch wiring)

**Purpose**: `EmulatorShell.cpp` started P3 as a ~142 KB / 4726-line god-class
that owned clipboard, window lifecycle, CPU thread, disk controllers, machine
switching, and command dispatch in a single translation unit. The P3 settings
panel apply path (T122) and the machine-switch wiring (FR-001, FR-002) cannot
land safely on top of that shape — too many overlapping responsibilities, too
much hidden state, no clean seam to plug a typed apply path into. P3.0 cuts
the shell into focused managers under `C:\Users\relmer\repos\relmer\Casso\Casso\Shell\`
so the P3 work has a sane surface to hook into.

**Scope constraints (apply uniformly to every extraction T108b..T108e)**:

- New files live under `C:\Users\relmer\repos\relmer\Casso\Casso\Shell\<ManagerName>.cpp` / `.h`.
- Each `.cpp` includes `"Pch.h"` first; quoted includes only for project headers.
- Repo conventions on EHM, function-call/declaration spacing, 5-blank-line
  top-level separation, 3-blank-line declaration/statement separation, column
  alignment, and header-style comment blocks apply.
- Managers hold back-references to shared state by reference or pointer; **no
  new global state, no new singletons** are introduced.
- `EmulatorShell.cpp` shrinks on every extraction; the constructor wires the
  manager via `std::make_unique` or a direct member; observable behavior is
  identical before and after.
- Per-extraction audit: `rg "Rml|RMLUI" Casso CassoCore CassoEmuCore CassoCli UnitTest External`
  returns zero hits; **no new** `BUTTON` / `COMBOBOX` / `EDIT` / `STATUSBAR` /
  `LISTVIEW` / `TREEVIEW` / `TOOLTIPS` window-class creations; **no new**
  `DialogBox*` / `DialogBoxIndirectParam*` calls.
- Build green Debug|x64 after each extraction; existing test suite (1447+
  tests) stays green with zero new failures.

**Commit discipline**:

- Each extraction (T108b..T108e) is its own commit titled
  `refactor(shell): extract <Manager> (007 P3.0X)` where `X` matches the task
  letter (b/c/d/e).
- T108a is already shipped (commit `33537b4`).
- T108f is a verification gate (tests + build + audit), **not** a separate
  commit — it gates entry to P3.

**Independent Test**: After each extraction the full test suite passes, the
build is green Debug|x64, the audit `rg` calls return zero hits, and
`EmulatorShell.cpp` shrinks measurably. After T108f the file is well under
100 KB and every responsibility listed above lives in its dedicated manager.

### P3.0a — Extractions (one commit per task, in order)

- [X] T108a [P3.0] **DONE** — extract `ClipboardManager` and `WindowManager` into `C:\Users\relmer\repos\relmer\Casso\Casso\Shell\ClipboardManager.cpp` / `.h` and `C:\Users\relmer\repos\relmer\Casso\Casso\Shell\WindowManager.cpp` / `.h`; reduced `EmulatorShell.cpp` from 4726 lines to ~3250 lines; 1447 tests still pass; build green (commit `33537b4`)
- [X] T108b [P3.0] **DONE** — extract `CpuManager` into `C:\Users\relmer\repos\relmer\Casso\Casso\Shell\CpuManager.cpp` / `.h` (commit `2330211`; 128305 → 126504 B)
- [X] T108c [P3.0] **DONE** — extract `DiskManager` into `C:\Users\relmer\repos\relmer\Casso\Casso\Shell\DiskManager.cpp` / `.h` (commit `d439366`; 126504 → 112944 B)
- [X] T108d [P3.0] **DONE** — extract `MachineManager` into `C:\Users\relmer\repos\relmer\Casso\Casso\Shell\MachineManager.cpp` / `.h` (commit `be02bb6`; 112944 → 78919 B)
- [X] T108e [P3.0] **DONE** — extract `WindowCommandManager` into `C:\Users\relmer\repos\relmer\Casso\Casso\Shell\WindowCommandManager.cpp` / `.h` (commit `e20722f`; 78919 → 66649 B; total P3.0 shrinkage 128305 → 66649 B, −1609 lines)

### P3.0b — Acceptance gate

- [X] T108f [P3.0] **VERIFIED** — build green Debug|x64 (0/0); 1447/1447 tests pass; `rg "Rml|RMLUI"` zero hits; no new Win32 UI control creations; no new `DialogBox*`; `EmulatorShell.cpp` at 66649 B (well under 100 KB).

**Checkpoint**: `EmulatorShell` is a thin coordinator over five focused
managers (`ClipboardManager`, `WindowManager`, `CpuManager`, `DiskManager`,
`MachineManager`, `WindowCommandManager`). P3 may now wire its apply path,
reset prompt, and machine-switch flow into the clean seams these managers
expose.

---

## Phase P3: Settings panel (Priority: P1 user stories US1, US2)

**Begins after P3.0 acceptance (T108f).** All T110–T129 work executes against
the decomposed shell so the apply path, machine-switch wiring, and command
dispatch land on focused managers rather than the original god-class.

**Goal**: Replace the current Win32 menu-command settings and machine-switch entry points with the native
in-canvas Settings panel: machine selector outermost, hardware tree, transient
state, Cancel/Apply semantics, reset-prompt for reset-required changes,
per-machine `_user.json` persistence with schema merge and version migration.

**Independent Test**: Open Settings, switch between two machines, observe every
control update within one frame (US1 acceptance). Toggle an optional hardware
component, Apply, reset, observe component absent (US2 acceptance). Cancel
with unapplied changes — nothing persisted.

### P3a — Settings panel surfaces (move + new)

- [X] T110 [US1] Move and rewrite `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanel.cpp` / `.h` into `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\SettingsPanel.cpp` / `.h` as a native `ModalScrim`-hosted panel (FR-027 retires Win32 dialog)
- [X] T111 [US1] Move and rewrite `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanelState.cpp` / `.h` into `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\SettingsPanelState.cpp` / `.h` (transient unapplied state, FR-009 Cancel discard, FR-010 Apply commit + reset prompt)
- [X] T112 [P] [US1] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\MachinePage.h`
- [X] T113 [P] [US1] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\MachinePage.cpp` (machine selector outermost; speed; video; write-protect; drive-audio toggle + mechanism per FR-002, FR-003, FR-011)
- [X] T114 [P] [US2] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\HardwarePage.h`
- [X] T115 [P] [US2] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\HardwarePage.cpp` (TreeView of `HardwareComponentEntry` rendering optional/required/platform-locked checkbox states + lock tooltip per FR-004 through FR-008)
- [X] T116 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\ThemePage.h` (used by P4; stub in P3 so SettingsPanel can wire the tab)
- [X] T117 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\ThemePage.cpp` (stub body in P3; filled in P4)
- [X] T118 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\DisplayPage.h` (used by P4; stub in P3)
- [X] T119 [P] Create `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\DisplayPage.cpp` (stub body in P3; filled in P4)

### P3b — Persistence + migration

- [X] T120 [US1] Extend `C:\Users\relmer\repos\relmer\Casso\Casso\Config\UserConfigStore.cpp` to read/write per-machine `_user.json` with shadow/fallthrough merge (FR-012, FR-014, FR-017)
- [X] T120a [US1] Extend `C:\Users\relmer\repos\relmer\Casso\Casso\Config\UserConfigStore.cpp` to run a one-shot migration that reads `RegistrySettings` machine-specific keys and writes them to `<Machine>_user.json` on first load when the user JSON is absent (FR-016)
- [X] T120b [US1] Apply persisted `lastMountedImages` from `_user.json` on machine load / application launch; missing paths start empty with a warning, eject clears the entry, and mount overwrites it (FR-047)
- [X] T120c [US1] Extend `C:\Users\relmer\repos\relmer\Casso\Casso\Config\GlobalUserPrefs.cpp` and `.h` with `WindowPlacementProfile` read/write keyed by monitor topology + active monitor under `HKCU\Software\relmer\Casso\WindowPlacement\v1\<hash>` (FR-048)
- [X] T120d [US1] Wire `C:\Users\relmer\repos\relmer\Casso\Casso\Window.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp` to apply saved window bounds on startup and save profile updates on `WM_MOVE` / `WM_SIZE` (FR-048)
- [X] T121 [US1] Extend `C:\Users\relmer\repos\relmer\Casso\CassoEmuCore\Core\MachineConfigUpgrade.cpp` and `.h` to add a `capabilityFlag` upgrade step and the canonical-field rewrite rule: accept legacy `$cassoDefault` as a read-alias **only** when `$cassoMachineVersion` is absent; when both are present, treat `$cassoMachineVersion` as authoritative and immediately rewrite the file to canonical form containing only `$cassoMachineVersion` (FR-013, FR-015)
- [X] T122 [US1] Wire `EmulatorShell` apply path so reset-required changes prompt via `ModalScrim` confirm and immediate changes commit without reset (FR-010, FR-011) in `C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp`
- [X] T123 Verify the legacy Win32 dialogs remain absent: confirm no `C:\Users\relmer\repos\relmer\Casso\Casso\OptionsDialog.*`, no `C:\Users\relmer\repos\relmer\Casso\Casso\MachinePickerDialog.*`, and zero `DialogBox*` / `DialogBoxIndirectParam*` calls under `C:\Users\relmer\repos\relmer\Casso\Casso\`; update `MenuSystem.cpp` / `EmulatorShell.cpp` command entry points to open the native Settings panel instead (FR-027, SC-005)

### P3c — Tests

- [X] T124 [P] [US1] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\SettingsPanelStateTests.cpp` (machine-switch reload, Cancel discard, Apply commit semantics, and open/close does not call a pause API per FR-041; mocked filesystem per FR-057)
- [X] T125 [P] [US1] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\UserConfigStoreTests.cpp` (shadow/fallthrough merge, write-back round-trip, one-shot registry-to-user-JSON migration, and `lastMountedImages` auto-mount persistence; mocked filesystem/registry per FR-016, FR-047, FR-057)
- [X] T125a [P] [US2] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\HardwareTreeTests.cpp` for FR-004 through FR-008 hardware tree coverage
- [X] T126 [P] [US1] Extend `C:\Users\relmer\repos\relmer\Casso\UnitTest\EmuTests\MachineConfigUpgradeTests.cpp` (create if absent) with `capabilityFlag` upgrade + `$cassoMachineVersion` canonical-rewrite cases (FR-013, FR-015)
- [X] T126a [P] [US1] Create `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\WindowPlacementProfileTests.cpp` with mocked registry coverage for per-monitor restore, default-centered fallback, and `WM_MOVE` / `WM_SIZE` save triggers (FR-048, FR-057)
- [X] T127 [US2] Add `HardwarePage` rendering-rule coverage (optional / required / platform-locked checkbox + lockReason tooltip) inside `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\SettingsPanelStateTests.cpp`
- [X] T128 Update `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj` and `C:\Users\relmer\repos\relmer\Casso\UnitTest\UnitTest.vcxproj` to reference every new Settings/config file from T110–T127, T120a–T120d, and T126a

### P3d — Runtime validation

- [ ] T129 [US1] Capture screenshot matrix entry M4 (settings panel open over running emulation, two machines visible in selector, hardware tree expanded) under `C:\Users\relmer\repos\relmer\Casso\TestResults\007-ui-overhaul\`

**Checkpoint**: US1 and US2 are independently functional and tested.

---

## Phase P3.1: Smoke-test polish (UX corrections)

**Goal**: Fix the four issues uncovered by the post-P3 visual smoke test
(back-buffer capture of commit `c8faa57` + predecessors) before P4 begins.
This sub-phase also lands the agreed architectural change of moving the drive
widgets off the emulator surface into a new bottom command bar.

**Out of scope**: The Disk II visual treatment (FR-049 / SC-009 — drive widgets
that read as Disk II hardware rather than generic boxes) is intentionally NOT
a P3.1 task. It remains in P4 as part of theme variant work. P3.1 only
relocates the widgets and fixes their state pump; their look stays generic.

**Independent Test**: Launch `Casso.exe` at the default startup window size.
Top chrome, the emulator viewport at 100% (560×384) with no overlap, and a
bottom command bar containing both Disk II drive widgets must all be visible
simultaneously. Booting from a mounted disk drives the active drive's LED into
the Active glow state while the motor is on. Opening the settings panel
(Ctrl+,) shows a populated machine selector with the active machine
pre-selected.

### P3.1a — Bottom command bar + drive-widget relocation

- [X] T109a [P3.1] Add a bottom command bar to the host window: reserve a new bottom-inset region in `C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp` (mirror of the existing top chrome inset), draw a chrome strip via `UiShell` at the bottom of the back buffer, and expand the default startup window size so 100% emulator UI + top chrome (title + nav) + bottom command bar all fit without scaling the emulator. **Acceptance**: emulator viewport renders at 100% (560×384) with no overlap, top chrome visible above it, command bar visible below it, at the default startup window size.
- [X] T109b [P3.1] Relocate the drive widgets from their current floating-on-top-of-emulator position into the new bottom command bar. Drive widgets render in `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Chrome\DriveWidget.cpp` and are positioned by `C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp`; reroute their layout into bottom-command-bar coordinates and update the `HitTest` registry so drag-drop and click-to-browse hit-test against the new location. **Acceptance**: drive widgets visually live in the bottom command bar, never overlap the emulator content area, and drag-drop + click-to-browse still mount disks correctly.

### P3.1b — State + selector fixes

- [X] T109c [P3.1] Fix the `DriveWidget` LED state pump in `C:\Users\relmer\repos\relmer\Casso\Casso\Shell\DiskManager.cpp` and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Chrome\DriveWidget.cpp`. The LED currently displays the Idle/Present orange even while the disk controller has its motor on during boot. Confirm the motor-on signal is read from the disk controller into `DriveWidgetState` every UI frame and that `DriveWidget::Paint` maps `DriveWidgetState::motorOn` / `diskActive` to the Active glow color. **Acceptance**: booting from a mounted disk drives the LED into the Active state (matching the FR-025 glow contract) while the motor is on, and back to Present when the motor stops.
- [X] T109d [P3.1] Fix the Machine selector population in `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\MachinePage.cpp` (and `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\SettingsPanel.cpp` if needed). The selector currently displays an empty value when the panel opens. Walk `MachineScanner` / `UserConfigStore` to populate the dropdown with installed machines and select the currently active one. **Acceptance**: opening the settings panel shows the dropdown populated with installed machines, the active machine pre-selected, and changing the selection refreshes the other controls within one frame per FR-002.

### P3.1c — Acceptance gate

- [X] T109e [P3.1] P3.1 acceptance gate: full smoke-test pass. `Casso.exe` launches at default window size; chrome bar visible at top with min/max/close working; nav strip visible with dropdowns; emulator viewport renders at 100% (560×384) with NO overlap from any chrome element; bottom command bar visible with two Disk II drive widgets (no overlap with emulator); LEDs respond to motor on/off during boot; `Ctrl+,` opens the native settings panel with machine selector populated and all controls interactive; `rg "Rml|RMLUI" Casso CassoCore CassoEmuCore CassoCli UnitTest External` returns zero hits; full test suite passes. Gate-only — no separate commit.

**Checkpoint**: P3.1 acceptance gate (T109e) is green. P4 may now begin.

---

## Phase P4: Themes + CRT controls (Priority: P2 user story US4)

> **Dependency note**: P3.1 acceptance (T109e) must pass before any P4 task starts.

**Goal**: Theme picker hot-swap, CRT brightness + per-effect toggles + per-effect
parameters, three shipped built-in themes with full II / II+ / IIe / IIc variant
coverage, global persistence in `GlobalUserPrefs`.

**Independent Test**: Switch theme in Settings → ThemePage; the first post-switch
frame is fully themed (SC-002). Adjust CRT brightness slider; viewport
luminance updates live. Toggle scanlines / bloom / color-bleed; each toggle
takes effect immediately and persists across app restart.

### P4a — ThemePage / DisplayPage implementations

- [ ] T130 [US4] Fill `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\ThemePage.cpp` with theme picker, hot-swap dispatch (FR-033), discovery refresh on panel open (FR-035), persistence in `GlobalUserPrefs.json` (FR-034)
- [ ] T131 [US4] Fill `C:\Users\relmer\repos\relmer\Casso\Casso\Ui\Settings\DisplayPage.cpp` with CRT brightness slider (FR-038), per-effect toggles (scanlines, bloom, color bleed FR-039), and per-effect parameter sliders (FR-040). All values persisted globally, never per-machine.

### P4b — `GlobalUserPrefs` plumbing

- [ ] T132 [US4] Extend `C:\Users\relmer\repos\relmer\Casso\Casso\Config\GlobalUserPrefs.cpp` and `.h` to load/save active theme id + CRT brightness + per-effect toggles + per-effect parameters, schema-migrated like machine configs

### P4c — Ship built-in themes (FR-031)

- [ ] T133 [P] [US4] Author `C:\Users\relmer\repos\relmer\Casso\Resources\Themes\Skeuomorphic\theme.json` plus image / font assets (II, II+, IIe, IIc variants per FR-051, FR-052)
- [ ] T134 [P] [US4] Author `C:\Users\relmer\repos\relmer\Casso\Resources\Themes\DarkModern\theme.json` plus assets (4 variants)
- [ ] T135 [P] [US4] Author `C:\Users\relmer\repos\relmer\Casso\Resources\Themes\RetroTerminal\theme.json` plus assets (4 variants)
- [ ] T136 [US4] Register the three theme directories as embedded resources in `C:\Users\relmer\repos\relmer\Casso\Casso\Casso.rc` and `C:\Users\relmer\repos\relmer\Casso\Casso\resource.h` so `AssetBootstrap` can extract them on first launch (FR-030, FR-037)

### P4d — Tests

- [ ] T137 [P] [US4] Extend `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\ThemeManagerTests.cpp` with hot-swap acceptance (every chrome surface re-rendered in the first post-switch frame, no mixed-theme regions — SC-002)
- [ ] T138 [P] [US4] Rewrite `C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\GlobalUserPrefsTests.cpp` (round-trip, schema migration, malformed-file recovery; mocked filesystem)

### P4e — Runtime validation

- [ ] T139 [US4] Capture consecutive-frame screenshots during theme switch under `C:\Users\relmer\repos\relmer\Casso\TestResults\007-ui-overhaul\` proving SC-002 (first post-switch frame contains zero mixed-theme chrome regions) at 1280×960 and 1920×1080, 100% and 150% DPI, on integrated GPU

**Checkpoint**: US4 is functional, themes hot-swap, CRT controls persist globally.

---

## Phase P5: Polish, cross-cutting concerns, and SC-* validation

**Purpose**: US6 silent-upgrade validation, drag-drop reliability run, code
analysis sweep, full screenshot matrix capture, residue audit, documentation.

### P5a — US6 silent upgrade validation

- [ ] T140 [US6] Construct simulated v→v+3 `_user.json` fixtures under `C:\Users\relmer\repos\relmer\Casso\UnitTest\TestData\UserConfigUpgrade\` (each version adds at least one field) and extend `C:\Users\relmer\repos\relmer\Casso\UnitTest\EmuTests\MachineConfigUpgradeTests.cpp` with chain-upgrade cases proving SC-003 / SC-007

### P5b — SC-004 drag-drop reliability run

- [ ] T141 Author and run a 200-attempt × 4-format (`.dsk`, `.nib`, `.woz`, `.po`) drag-drop reliability harness against the running app; record results under `C:\Users\relmer\repos\relmer\Casso\TestResults\007-ui-overhaul\sc004-dragdrop.md` (overall ≥99%, per-format failure ≤2%)

### P5c — Final code analysis + residue audit

- [ ] T142 Run `scripts\Build.ps1 -RunCodeAnalysis` on the fully integrated branch and confirm zero findings
- [ ] T143 Re-run `rg -n "Rml|RMLUI" C:\Users\relmer\repos\relmer\Casso\Casso C:\Users\relmer\repos\relmer\Casso\CassoCore C:\Users\relmer\repos\relmer\Casso\CassoEmuCore C:\Users\relmer\repos\relmer\Casso\CassoCli C:\Users\relmer\repos\relmer\Casso\UnitTest C:\Users\relmer\repos\relmer\Casso\External` on the merged branch HEAD and confirm zero hits (SC-011, SC-012, FR-055, FR-056)
- [ ] T144 Run `scripts\RunTests.ps1` on the merged branch HEAD and confirm a green run with deterministic, side-effect-free results (SC-013)

### P5d — Full screenshot matrix

- [ ] T145 Capture the complete screenshot matrix M1–M7 under `C:\Users\relmer\repos\relmer\Casso\TestResults\007-ui-overhaul\` per `quickstart.md` and verify against acceptance behavior (SC-012)

### P5e — Documentation

- [ ] T146 Update `C:\Users\relmer\repos\relmer\Casso\CHANGELOG.md` with the user-visible UI overhaul entry (chrome, settings panel, theme system, RmlUi removal)
- [ ] T147 Update `C:\Users\relmer\repos\relmer\Casso\README.md` to reflect the native UI runtime, the three shipped themes, the per-machine user-JSON model, and updated test counts
- [ ] T148 Update `C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\quickstart.md` with any matrix or harness changes discovered during P5
- [ ] T149 Retire `C:\Users\relmer\repos\relmer\Casso\Casso\RegistrySettings.cpp` and `.h` once the one-shot migration in `UserConfigStore` is proven and no shipping code depends on registry-backed machine settings (FR-016)
- [ ] T150 Refresh `C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\menu-command-parity.md` after T095/T101 so task references and generated command coverage remain synchronized

---

## Dependencies & Execution Order

### Phase dependencies

- **P0** has no dependencies — start immediately. P0 is a HARD GATE.
- **P1** depends on P0 completion (constitution amended, RmlUi gone, build green, `rg` clean).
- **P2** depends on P1 completion.
- **P3.0** depends on P1 completion and may run in parallel with P2. It is the safety prerequisite for P3 — the settings panel apply path, reset prompt, and machine-switch wiring all hook into the managers extracted here.
- **P3** depends on P3.0 acceptance (T108f). It may run in parallel with the tail of P2 once the shell decomposition is in.
- **P3.1** depends on P3 completion (post-`c8faa57` smoke test surfaced the four issues this sub-phase fixes). T109e is its acceptance gate.
- **P4** depends on P3.1 acceptance (T109e). ThemePage / DisplayPage stubs from P3c are filled in P4a.
- **P5** depends on P2 + P3 + P3.1 + P4 completion.

### Within P0

- T001–T012 (file deletions) are all `[P]` and run in parallel.
- T013 (External/RmlUi delete) is independent and `[P]`-eligible with the file deletions.
- T014–T020 (build wiring scrub) must follow the file deletions or run in the same commit; they edit the same `Casso.vcxproj` / `Casso.sln` / `UnitTest.vcxproj` files and must be serialized.
- T021–T028 plus T022a and T027a–T027j (PCH + call-site scrub + targeted stubs/deletes) must follow T001–T020 because the source edits depend on the deleted symbols no longer being referenced.
- T029–T031 (test deletions) may run in parallel with T021–T028.
- T032 (constitution amendment) is independent — may run in parallel with anything in P0.
- T033–T037 (gate verification) must run last in P0, in order.

### Within P1

- T038a–T039 (docs) are independent and may run in parallel with everything else in P1.
- T040–T043 (shaders) are `[P]`. T044 (vcxproj wiring) depends on T040–T043.
- T045–T055 (new runtime sources) are `[P]` across distinct files.
- T056 (UiShell rewrite) depends on T045–T055.
- T057–T061 (integration into existing files) must follow T045–T056 because they call into the new APIs.
- T062–T069 (tests) are `[P]` across distinct files but depend on the sources they exercise existing first.
- T070a (UnitTest.vcxproj update) depends on T062–T069; T070b (Casso.vcxproj update) depends on T045–T056.
- T071–T073 (gate) last.

### Within P2

- T074–T093 (widget primitives) are all `[P]`.
- T094–T097 (chrome surface moves+rewrites) are `[P]` across distinct files and depend on the widgets they compose.
- T098–T100 (drag-drop, file-open, variants) depend on T096 (DriveWidget).
- T101 (MenuSystem retirement) is independent.
- T102–T106b (tests) `[P]` across distinct files; depend on the production files they cover.
- T107–T108 (vcxproj updates) depend on T074–T106b.
- T109 (screenshots) depends on T094–T108.

### Within P3.0

- T108a is already shipped (commit `33537b4`) — no further action.
- T108b → T108c → T108d → T108e run **serially**, each its own commit. They all edit `EmulatorShell.cpp` (and its header) and add new files under `Casso\Shell\`, so they cannot parallelize without merge churn on the shell.
- T108f is a gate after T108e: build + full test suite + `rg` audit + size check on `EmulatorShell.cpp`. No commit. Failure blocks entry to P3.

### Within P3

- **P3.0 (T108a–T108f) must be complete and T108f green before any P3 task starts.**
- T110–T111 (panel move+rewrite) must precede T112–T119 because the page files compose into `SettingsPanel`.
- T112–T119 (pages) are `[P]` across distinct files (the P4 stubs T116–T119 are intentionally light to keep `SettingsPanel` compilable).
- T120–T120b (UserConfigStore) and T121 (MachineConfigUpgrade) are `[P]` across distinct files; T120c–T120d (window placement) are serialized with `GlobalUserPrefs` / shell wiring.
- T122 (EmulatorShell apply path) depends on T120.
- T123 (legacy Win32 dialog verification / command entry point rewiring) is independent after T110 exists.
- T124–T127 plus T125a and T126a (tests) `[P]` across distinct files; depend on the production files.
- T128 (vcxproj) depends on T110–T127, T120a–T120d, and T126a.
- T129 (M4 screenshot) depends on T110–T128.

### Within P3.1

- T109a (bottom command bar + window resize) must land before T109b — the relocation target has to exist before drive widgets can move into it.
- T109b (drive-widget relocation + hit-test rewire) follows T109a.
- T109c (LED state pump) and T109d (machine selector population) are `[P]` across distinct files and independent of T109a/T109b, but in practice should land after T109b so the smoke-test acceptance gate sees the final layout.
- T109e is the acceptance gate and must run last. No separate commit; gate-only.

### Within P4

- **P3.1 (T109a–T109e) must be complete and T109e green before any P4 task starts.**
- T130 depends on T117 (ThemePage stub) and T132 (GlobalUserPrefs).
- T131 depends on T119 (DisplayPage stub) and T132.
- T132 is independent within P4.
- T133–T135 (theme authoring) are `[P]` across distinct directories.
- T136 (resource registration) depends on T133–T135.
- T137–T138 (tests) `[P]`.
- T139 (SC-002 screenshots) depends on T130–T138.

### Within P5

- T140 (US6 fixtures + tests), T141 (SC-004 harness), T146/T147/T150 (docs) may run in parallel.
- T142 (code analysis), T143 (rg residue), T144 (test run), T145 (matrix capture) must run last and in that order on the merged branch HEAD.
- T148 (quickstart) follows T140–T145; T149 follows proven FR-016 migration coverage.

---

## Parallel execution examples

### P0 — parallel file deletions

```text
Run T001–T013 + T029–T032 in parallel; serialize T014–T028 (same project files);
finish with T033–T037 in order.
```

### P1 — parallel runtime authoring after shaders land

```text
Run T040–T043 in parallel (shaders), then T044.
Run T045–T055 in parallel (one file each), then T056 (UiShell).
Run T062–T069 in parallel (one test file each), then T070a/T070b.
```

### P2 — widget swarm

```text
Run T074–T093 in parallel (20 distinct files: 10 widgets × 2).
Once widgets land, run T094–T097 in parallel (4 chrome surfaces, distinct files).
```

---

## Implementation strategy

### MVP (US1 only, the spec's named MVP)

1. Land P0 in a tight commit series. Verify the HARD GATE.
2. Land P1 to restore a paintable, hit-testable, focusable runtime.
3. Skip most of P2 — implement only `Chrome/NavLayer` minimal entry for "open Settings", `Chrome/TitleBar`, and `Widgets/Button`/`Checkbox`/`Dropdown`/`Slider`/`TextField`/`ModalScrim` (the subset US1 needs).
4. Land P3.0 (EmulatorShell decomposition) — the apply path needs clean seams.
5. Land P3 in full.
6. Validate US1 acceptance scenarios; ship the MVP.

### Incremental delivery (recommended)

1. P0 → green gate.
2. P1 → restore foundational runtime.
3. P2 → full chrome surface (US3, US5 ship).
4. P3.0 → EmulatorShell decomposition (refactor; no user-visible change, gates P3).
5. P3 → settings panel (US1, US2 ship).
6. P3.1 → smoke-test polish (bottom command bar, drive-widget relocation, LED pump, machine selector).
7. P4 → themes + CRT (US4 ships).
8. P5 → polish + validation (US6 + SC-* satisfied).

### Parallel team strategy

- **Phase P0**: one engineer, single commit series, no parallelism (the scrub is the work).
- **Phase P1**: 2–3 engineers — one owns painter/text/device-lost, one owns input/hit-test/focus/layout, one owns animation/theme + integration.
- **Phase P2 + P3.0**: can run in parallel once P1 lands. P3.0 is one engineer working serially on `EmulatorShell.cpp` (the extractions cannot parallelize without merge churn).
- **Phase P3**: starts after P3.0 acceptance (T108f). May run in parallel with the tail of P2.
- **Phase P3.1**: serial after P3 — one engineer, smoke-test driven. T109a → T109b serialize on `EmulatorShell.cpp`; T109c and T109d can fan out in parallel; T109e is the gate. P4 waits on T109e.
- **Phase P5**: single engineer per validation thread; T140, T141, T146, T147 can run in parallel; T142–T145 run last on a frozen merge.

---

## Notes

- Every task in this list maps to one or more spec FR-* or SC-* IDs; if a task feels orphaned, flag it for review against `spec.md` before executing.
- `[P]` strictly means "different files, no incomplete-task dependencies"; if two `[P]` tasks edit the same `.vcxproj` or `.sln`, they must be serialized regardless of the marker.
- All tests must mock filesystem, registry, environment, and Win32 system APIs per FR-057 / SC-013. No test introduced by this spec may read or write real user-state paths.
- Commit per task or per logically-coherent group; do not bundle P0 deletions with P1 introductions in a single commit — the two pipelines must never appear together in history.
- All new `.cpp` files follow the project conventions (Pch.h first, EHM, single-exit on HRESULT functions, top-of-scope variable declarations, column alignment, 5 blank lines between top-level constructs, 3 blank lines after variable declaration blocks).

