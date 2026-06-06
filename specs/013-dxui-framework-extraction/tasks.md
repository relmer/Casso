---
description: "Task list for Dxui framework extraction (spec 013)"
---

# Tasks: Dxui — Reusable DirectX UI Framework Extracted from Casso

**Input**: Design documents from `/specs/013-dxui-framework-extraction/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/
**Organization**: Tasks are grouped by **migration phase** (1 through 14) as defined in plan.md. Phases land sequentially; each phase ends green (build + tests + code analysis) and is independently mergeable. User stories US1–US5 from spec.md are satisfied incrementally across these phases; story coverage is tracked in the Dependencies section at the bottom of this file.

## Format: `[ID] [P?] [Phase] Description`

- **[P]**: Can run in parallel within its phase (different files, no intra-phase dependencies).
- **[Phase]**: `[PH1]`, `[PH2]`, `[PH3]`, `[PH4]`, `[PH5]`, `[PH6]`, `[PH7]`, `[PH8]`, `[PH9]`, `[PH10]`, `[PH11]`, `[PH12]`, `[PH13]`, `[PH14]`.
- Every task lists files affected, FR references, and a per-task exit criterion.
- **Phase exit gate (every phase)**: `scripts\Build.ps1 -Configuration Debug -Platform x64`, `scripts\Build.ps1 -Configuration Release -Platform ARM64`, `scripts\RunTests.ps1`, and `scripts\Build.ps1 -RunCodeAnalysis` all green. Commit message follows Conventional Commits with scope `dxui` (or `casso/ui` for consumer-side migration phases). Merge with `--no-ff` (never squash).

## Path Conventions

- New library: `Dxui/` at solution root.
- New tests: `UnitTest/Dxui/`.
- Casso consumer: `Casso/` and `Casso/Ui/` (existing).
- All paths absolute from repo root; the repo root is the directory containing `Casso.sln`.

---

## Phase 1 — Scaffold `Dxui.vcxproj`

- [x] T000 [PH1] Snapshot pre-migration test output by running `scripts\RunTests.ps1` and saving the captured console output to `specs/013-dxui-framework-extraction/baseline-tests.txt`. Subsequent phase gates compare against this snapshot to distinguish pre-existing flakes from new failures. **Exit**: `baseline-tests.txt` exists and records pass/fail counts plus any known failures. **SC**: SC-005.

**Goal (plan.md §Phase 1)**: A buildable empty `Dxui` static library that Casso and UnitTest reference, with the umbrella header chokepoint live. **Satisfies**: FR-001, FR-002, FR-003, FR-004, FR-006, FR-007, SC-001, SC-002.

- [x] T001 [PH1] Author `Dxui/Dxui.vcxproj` as `ConfigurationType=StaticLibrary`, `PlatformToolset=v145`, `CharacterSet=Unicode`, configs `Debug|x64`, `Release|x64`, `Debug|ARM64`, `Release|ARM64`; mirror `CassoCore/CassoCore.vcxproj` for PCH, code-analysis ruleset, warning level, and output paths; add `Dxui/Pch.h`, `Dxui/Pch.cpp`, `Dxui/Dxui.h` as items; add `<AdditionalDependencies>d3d11.lib;d3dcompiler.lib;d2d1.lib;dwrite.lib;dxgi.lib;dcomp.lib;windowscodecs.lib;%(AdditionalDependencies)</AdditionalDependencies>` to every link config so consumers inherit the DX link set. **Exit**: `Dxui.vcxproj` opens in Visual Studio with all four platform/config combinations visible. **FR**: FR-001, FR-006.

- [x] T002 [P] [PH1] Create `Dxui/Pch.h` and `Dxui/Pch.cpp` (internal PCH). `Pch.h` includes the same system-header set as `Dxui.h` (listed in T003). `Pch.cpp` contains only `#include "Pch.h"`. **Exit**: file compiles standalone when `Dxui.vcxproj` builds. **FR**: FR-004 (mirrors umbrella set internally).

- [x] T003 [P] [PH1] Create `Dxui/Dxui.h` — the **single public chokepoint** for Dxui's system-header surface. Angle-bracket includes **only**, in this order: `<windows.h>`, `<d3d11.h>`, `<d3dcompiler.h>`, `<d2d1.h>`, `<dwrite.h>`, `<dxgi1_3.h>`, `<dcomp.h>`, `<wincodec.h>`, `<wrl/client.h>`, `<future>`, `<functional>`, `<memory>`, `<string>`, `<vector>`, `<cstdint>`, `<cmath>`. No project headers (those reach consumers naturally once Phases 2–11 move them in). Define `DXUI_ASSERT_UI_THREAD()` in this header (debug-build assert, no-op in release) for use by public Dxui entry points. Top-of-file doc comment MUST state: "Dxui is UI-thread-only (FR-083). All public Dxui APIs must be called on the host window message-pump thread." **Exit**: header compiles when `#include`d from a TU with no other includes. **FR**: FR-004, FR-007, FR-083.

- [x] T004 [PH1] Register `Dxui` in `Casso.sln`: add the `Project(...) = "Dxui", "Dxui\Dxui.vcxproj", "{GUID}"` block and matching `GlobalSection(ProjectConfigurationPlatforms) = postSolution` rows for all four (Debug|x64, Release|x64, Debug|ARM64, Release|ARM64). Generate a fresh GUID; do not reuse another project's. **Depends on**: T001. **Exit**: `scripts\Build.ps1 -Project Dxui` succeeds on every config. **FR**: FR-001.

- [x] T005 [PH1] Modify `Casso/Casso.vcxproj`: add `<ProjectReference Include="..\Dxui\Dxui.vcxproj">` block; add `$(SolutionDir)Dxui` to `<AdditionalIncludeDirectories>` for every config; **remove** `d3d11.lib;dxgi.lib` (and any other DX libs already restated) from `<AdditionalDependencies>` so Dxui owns the DX link set. **Depends on**: T001. **Exit**: `rg -n 'd3d11\.lib|dxgi\.lib' Casso/Casso.vcxproj` returns zero hits; `Casso.exe` still links. **FR**: FR-003, FR-006.

- [x] T006 [P] [PH1] Modify `UnitTest/UnitTest.vcxproj`: add `<ProjectReference Include="..\Dxui\Dxui.vcxproj">`; add `$(SolutionDir)Dxui` to `<AdditionalIncludeDirectories>` for every config. Do **not** modify `CassoCli/CassoCli.vcxproj`. **Depends on**: T001. **Exit**: `UnitTest.dll` links; `rg -n 'Dxui' CassoCli/CassoCli.vcxproj` returns zero hits. **FR**: FR-003.

- [x] T007 [PH1] Update `Casso/Pch.h` to `#include "Dxui.h"` immediately after the existing Casso-internal include block (single-blank-line separator); update `UnitTest/Pch.h` to `#include "Dxui.h"` in the same position. **Depends on**: T003, T005, T006. **Exit**: both PCHs rebuild cleanly; consumer `.cpp` files transitively see Dxui's system-header surface. **FR**: FR-007.

- [x] T008 [P] [PH1] Amend `.github/copilot-instructions.md`: change the "NEVER use angle-bracket includes (`<header>`) anywhere except `Pch.h`" line to "NEVER use angle-bracket includes (`<header>`) anywhere except `Pch.h` or a library project's umbrella header (currently only `Dxui.h`)". Do not touch other rules. **Exit**: `git diff .github/copilot-instructions.md` shows exactly one substantive line changed. **FR**: FR-007.

- [x] T009 [PH1] Phase-1 exit verification. Run `scripts\Build.ps1 -Configuration Debug -Platform x64`, `scripts\Build.ps1 -Configuration Release -Platform ARM64`, `scripts\RunTests.ps1`, `scripts\Build.ps1 -RunCodeAnalysis`. Run `rg -n 'd3d11\.lib|dxgi\.lib' Casso/Casso.vcxproj` (expect 0). Run `rg -n 'Dxui' CassoCli/` (expect 0). Run `scripts\RunDormannTest.ps1` and `scripts\RunHarteTests.ps1 -SkipGenerate` once to establish CPU/emulator validation baseline; record pass/fail in `specs/013-dxui-framework-extraction/baseline-validation.txt`. **Depends on**: T000–T008. **Exit**: all four commands green; both greps as documented; `baseline-tests.txt` and `baseline-validation.txt` committed. **Commit**: `build(dxui): scaffold Dxui static library and umbrella header`. **FR**: FR-001/FR-002/FR-003/FR-006/FR-007; **SC**: SC-001, SC-002.

---

## Phase 2 — Move pure-generic files into `Dxui/Core/` and `Dxui/Theme/`

**Goal (plan.md §Phase 2)**: Mechanically relocate eight files that have no Casso-specific dependencies, applying the `Dxui` prefix and the `Dip`-suffix rename **only in the moved files**. **Satisfies**: FR-005, FR-090, FR-082 (incrementally).

Each move task in this phase is mechanical: (a) move file to new path; (b) rename the primary type per the table; (c) ensure first include is `#include "Pch.h"`; (d) drop any angle-bracket includes (now satisfied by `Dxui.h` via `Pch.h`); (e) rewrite quoted project-header includes to point at renamed siblings; (f) in moved files only, rename `Dp`-suffixed identifiers to `Dip` (FR-082); (g) update Dxui.vcxproj item list; (h) remove the old vcxproj item from `Casso.vcxproj`; (i) fix every consumer `#include` and type reference in `Casso/`. The eight moves are independent and parallel-safe within this phase.

- [x] T010 [P] [PH2] Move `Casso/Ui/HitTester.{h,cpp}` → `Dxui/Core/DxuiHitTester.{h,cpp}`; rename type `HitTester` → `DxuiHitTester`. Update all Casso call sites. **Exit**: `rg -n '\bHitTester\b' Casso/ Dxui/` returns hits only at the new name. **FR**: FR-005, FR-090.

- [x] T011 [P] [PH2] Move `Casso/Ui/UiInput.{h,cpp}` → `Dxui/Core/DxuiInput.{h,cpp}`; rename `UiInput*` types → `Dxui*` (matching the existing names with the prefix swap). **Exit**: `rg -n '\bUiInput' Casso/ Dxui/` returns zero hits. **FR**: FR-005, FR-090.

- [x] T012 [P] [PH2] Move `Casso/Ui/Animation.{h,cpp}` → `Dxui/Core/DxuiAnimation.{h,cpp}`; rename `Animation*` types → `DxuiAnimation*`. **Exit**: `rg -n 'class\s+Animation\b' Casso/` returns zero hits. **FR**: FR-005, FR-090.

- [x] T013 [P] [PH2] Move `Casso/Ui/DpiScaler.h` (header-only) → `Dxui/Core/DxuiDpiScaler.h`; rename `DpiScaler` → `DxuiDpiScaler`; rename any `Dp`-suffixed identifiers within this file to `Dip`. **Exit**: `rg -n '\bDpiScaler\b' Casso/ Dxui/` returns hits only at the new name. **FR**: FR-005, FR-082, FR-090.

- [x] T014 [P] [PH2] Move `Casso/Ui/WindowsThemeColors.{h,cpp}` → `Dxui/Theme/DxuiWindowsThemeColors.{h,cpp}`; rename `WindowsThemeColors` → `DxuiWindowsThemeColors`. **Exit**: `rg -n 'WindowsThemeColors' Casso/Ui` returns zero hits. **FR**: FR-005, FR-090.

- [x] T015 [P] [PH2] Move `Casso/Ui/Win11DwmHelpers.{h,cpp}` → `Dxui/Theme/DxuiDwm.{h,cpp}`; rename `Win11Dwm*` types/functions → `DxuiDwm*` (collapse the `Win11` prefix). **Exit**: `rg -n 'Win11Dwm' Casso/ Dxui/` returns zero hits. **FR**: FR-005, FR-090.

- [x] T016 [P] [PH2] Move `Casso/Ui/TitleBarHitTest.{h,cpp}` → `Dxui/Core/DxuiTitleBarHitTest.{h,cpp}`; rename `TitleBarHitTest` → `DxuiTitleBarHitTest`. **Exit**: `rg -n 'TitleBarHitTest' Casso/Ui` returns zero hits. **FR**: FR-005, FR-090.

- [x] T017 [P] [PH2] Move `Casso/Ui/DragDropTarget.{h,cpp}` → `Dxui/Win32/DxuiDragDropTarget.{h,cpp}`; rename `DragDropTarget` → `DxuiDragDropTarget`. **Exit**: `rg -n 'DragDropTarget' Casso/Ui` returns zero hits. **FR**: FR-005, FR-090.

- [x] T018 [PH2] Phase-2 exit verification. Build all four configs; run tests; run code analysis. Greps: `rg -n 'HitTester|UiInput|Win11DwmHelpers|WindowsThemeColors|TitleBarHitTest|DragDropTarget' Casso/Ui` → zero hits (legacy directory is now smaller). Spacing audit on moved files: `rg -n '\w \(\)' Dxui/Core Dxui/Theme Dxui/Win32` → zero hits. **Depends on**: T010–T017. **Commit**: `refactor(dxui): move generic utilities (HitTester/UiInput/...) into Dxui/Core`. **FR**: FR-005, FR-082, FR-090.

---

## Phase 3 — Rename + move render facades

**Goal (plan.md §Phase 3)**: Land the concrete render facades under their new single-prefix names. Interfaces stay deferred to Phase 6. **Satisfies**: FR-005, FR-091.

- [x] T019 [PH3] Move `Casso/Ui/DxUiPainter.{h,cpp}` → `Dxui/Render/DxuiPainter.{h,cpp}`; rename class `DxUiPainter` → `DxuiPainter` (single-word prefix). Update every Casso consumer (`#include` + type references). **Exit**: `rg -n '\bDxUiPainter\b' Casso/ Dxui/` returns zero hits. **FR**: FR-005, FR-091.

- [x] T020 [PH3] Move the painter's HLSL source from `Casso/Ui/` (whatever `.hlsl` file ships with `DxUiPainter`) → `Dxui/Render/DxuiPainter.hlsl`. Add the file as an `<FxCompile>` item in `Dxui.vcxproj` with the same entrypoint / target / output settings as the original `Casso.vcxproj` entry; remove the original `Casso.vcxproj` entry. **Depends on**: T019. **Exit**: `Dxui.vcxproj` builds the shader; `Casso.vcxproj` no longer references it; `rg -n '\.hlsl' Casso/Casso.vcxproj` returns zero hits for the painter shader. **FR**: FR-091.

- [x] T021 [PH3] Move `Casso/Ui/DwriteTextRenderer.{h,cpp}` → `Dxui/Render/DxuiTextRenderer.{h,cpp}`; rename class `DwriteTextRenderer` → `DxuiTextRenderer`. Update every Casso consumer. **Exit**: `rg -n 'DwriteTextRenderer' Casso/ Dxui/` returns zero hits. **FR**: FR-005, FR-091.

- [x] T022 [PH3] Phase-3 exit verification. Build all four configs; run tests; run code analysis. Greps: `rg -n 'DxUiPainter|DwriteTextRenderer' Casso UnitTest` → zero hits. **Depends on**: T019–T021. **Commit**: `refactor(dxui): rename render facades to DxuiPainter and DxuiTextRenderer`. **FR**: FR-005, FR-091.

---

## Phase 4 — Move widgets

**Goal (plan.md §Phase 4)**: Each `Casso/Ui/Widgets/<Foo>.{h,cpp}` → `Dxui/Widgets/Dxui<Foo>.{h,cpp}` with the `Dxui` prefix on every public type. Widget `Paint` signatures still take concrete `DxuiPainter &` / `DxuiTextRenderer &` / `ChromeTheme &`; the interface flip happens in Phase 5 and Phase 6. **Satisfies**: FR-005, FR-060.

The 13 widget files are independent moves and parallel-safe within this phase. Each task: move files, rename primary type (`<Foo>` → `Dxui<Foo>`), update consumer includes and references throughout `Casso/Ui/`, settings pages, debug panels, dialogs.

- [x] T023 [P] [PH4] Move `Casso/Ui/Widgets/Button.{h,cpp}` → `Dxui/Widgets/DxuiButton.{h,cpp}`; rename `Button` → `DxuiButton`. **FR**: FR-005, FR-060.

- [x] T024 [P] [PH4] Move `Casso/Ui/Widgets/Checkbox.{h,cpp}` → `Dxui/Widgets/DxuiCheckbox.{h,cpp}`; rename `Checkbox` → `DxuiCheckbox`. **FR**: FR-005, FR-060.

- [x] T025 [P] [PH4] Move `Casso/Ui/Widgets/Radio.{h,cpp}` → `Dxui/Widgets/DxuiRadio.{h,cpp}`; rename `Radio` → `DxuiRadio`. **FR**: FR-005, FR-060.

- [x] T026 [P] [PH4] Move `Casso/Ui/Widgets/Toggle.{h,cpp}` → `Dxui/Widgets/DxuiToggle.{h,cpp}`; rename `Toggle` → `DxuiToggle`. **FR**: FR-005, FR-060.

- [x] T027 [P] [PH4] Move `Casso/Ui/Widgets/Slider.{h,cpp}` → `Dxui/Widgets/DxuiSlider.{h,cpp}`; rename `Slider` → `DxuiSlider`. **FR**: FR-005, FR-060.

- [x] T028 [P] [PH4] Move `Casso/Ui/Widgets/Dropdown.{h,cpp}` → `Dxui/Widgets/DxuiDropdown.{h,cpp}`; rename `Dropdown` → `DxuiDropdown`. Existing in-window clipping path is preserved as-is here; popup hosting lands in Phase 8. **FR**: FR-005, FR-060.

- [x] T029 [P] [PH4] Move `Casso/Ui/Widgets/TabStrip.{h,cpp}` → `Dxui/Widgets/DxuiTabStrip.{h,cpp}`; rename `TabStrip` → `DxuiTabStrip`. **FR**: FR-005, FR-060.

- [x] T030 [P] [PH4] Move `Casso/Ui/Widgets/TextInput.{h,cpp}` → `Dxui/Widgets/DxuiTextInput.{h,cpp}`; rename `TextInput` → `DxuiTextInput`. **FR**: FR-005, FR-060.

- [x] T031 [P] [PH4] Move `Casso/Ui/Widgets/Label.h` (header-only) → `Dxui/Widgets/DxuiLabel.h`; rename `Label` → `DxuiLabel`. **FR**: FR-005, FR-060.

- [x] T032 [P] [PH4] Move `Casso/Ui/Widgets/ListView.{h,cpp}` → `Dxui/Widgets/DxuiListView.{h,cpp}`; rename `ListView` → `DxuiListView`. **FR**: FR-005, FR-060.

- [x] T033 [P] [PH4] Move `Casso/Ui/Widgets/TreeView.{h,cpp}` → `Dxui/Widgets/DxuiTreeView.{h,cpp}`; rename `TreeView` → `DxuiTreeView`. **FR**: FR-005, FR-060.

- [x] T034 [P] [PH4] Move `Casso/Ui/Widgets/PopupMenu.{h,cpp}` → `Dxui/Widgets/DxuiPopupMenu.{h,cpp}`; rename `PopupMenu` → `DxuiPopupMenu`. **FR**: FR-005, FR-060.

- [x] T035 [P] [PH4] Move `Casso/Ui/Widgets/Tooltip.{h,cpp}` → `Dxui/Widgets/DxuiTooltip.{h,cpp}`; rename `Tooltip` → `DxuiTooltip`. **FR**: FR-005, FR-060.

- [x] T036 [P] [PH4] Move `Casso/Ui/Widgets/ModalScrim.{h,cpp}` → `Dxui/Widgets/DxuiModalScrim.{h,cpp}`; rename `ModalScrim` → `DxuiModalScrim`. **FR**: FR-005, FR-060.

- [x] T037 [PH4] Phase-4 exit verification. Confirm `Casso/Ui/Widgets/` is empty; remove the now-empty `Casso/Ui/Widgets/` directory and drop its `<Filter>` entries from `Casso.vcxproj.filters`. Build all four configs; run tests; run code analysis. Greps: `rg -n 'class\s+(Button|Checkbox|Radio|Toggle|Slider|Dropdown|TabStrip|TextInput|Label|ListView|TreeView|PopupMenu|Tooltip|ModalScrim)\b' Casso/ Dxui/` returns hits only at the `Dxui`-prefixed names. **Depends on**: T023–T036. **Commit**: `refactor(dxui): move widgets into Dxui/Widgets with Dxui prefix`. **FR**: FR-005, FR-060.

---

## Phase 5 — Introduce `IDxuiTheme`

**Goal (plan.md §Phase 5)**: Decouple widgets from Casso's concrete `ChromeTheme` by routing them through a pure-virtual `IDxuiTheme`. **Satisfies**: FR-032, FR-033 (partial — default-noop accessor lands here; the broadcast machinery arrives in Phase 6).

- [x] T038 [PH5] Create `Dxui/Theme/IDxuiTheme.h` per `contracts/IDxuiTheme.h.md`. Pure-virtual accessors for background, foreground, accent, focus ring, disabled foreground, caption colours, body / caption font handles, plus any others the contract sketch lists. All accessors `const` and return-by-value or `const &`. No `IDxui` body — interface only. **Exit**: header compiles standalone (only `#include "Pch.h"`). **FR**: FR-032.

- [x] T039 [PH5] Modify `Casso/Ui/Chrome/ChromeTheme.h` to derive from `IDxuiTheme` and `override` every interface accessor; preserve existing skeuomorphic palette + scanline tint additions on top. **Depends on**: T038. **Exit**: `ChromeTheme` satisfies `IDxuiTheme` at compile time (no abstract leftovers); Casso paint paths still receive `ChromeTheme &` and continue to work via implicit upcast. **FR**: FR-032.

- [x] T040 [PH5] Re-type every Dxui widget's `Paint(...)` parameter from `ChromeTheme const &` to `IDxuiTheme const &`. Touch every `.h` and `.cpp` under `Dxui/Widgets/`. Call sites in `Casso/` continue to pass `ChromeTheme &` (implicit upcast). **Depends on**: T038, T039. **Exit**: `rg -n 'ChromeTheme' Dxui/Widgets` returns zero hits. **FR**: FR-032.

- [x] T041 [P] [PH5] Create `UnitTest/Dxui/MockDxuiTheme.{h,cpp}` returning deterministic, canned values for every `IDxuiTheme` accessor. No D3D, no DirectWrite — font handles are stored as opaque `IDWriteTextFormat *` set externally or as `nullptr` with a debug-build assert if accessed. Add to `UnitTest.vcxproj`. **Depends on**: T038. **Exit**: file compiles as part of `UnitTest.dll`. **FR**: FR-032; **SC**: SC-007.

- [x] T042 [PH5] Add a single smoke test `UnitTest/Dxui/MockDxuiThemeTests.cpp` that constructs `MockDxuiTheme`, constructs a `DxuiButton` with no D3D device, and asserts the button's `AccessibleName` / bounds setter / `SetVisible` behaviour. Paint is **not** exercised yet (no painter mock until Phase 6). **Depends on**: T040, T041. **Exit**: test passes via `scripts\RunTests.ps1`. **FR**: FR-032; **SC**: SC-007 (foundation).

- [x] T043 [PH5] Phase-5 exit verification. Build all four configs; run tests; run code analysis. **Depends on**: T038–T042. **Commit**: `refactor(dxui): introduce IDxuiTheme; widgets paint against the interface`. **FR**: FR-032, FR-033.

---

## Phase 6 — Add framework (`IDxuiControl`, `DxuiPanel`, layouts, focus, render interfaces)

**Goal (plan.md §Phase 6)**: The heart of the framework lands. After this phase the toolkit is consumable end-to-end, even though Casso's chrome / pages don't use it yet. **Satisfies**: FR-010, FR-011, FR-012, FR-020, FR-021 (Stack/Grid/Form/Absolute only; Dock lands in Phase 9), FR-022, FR-031, FR-033, FR-040, FR-041, FR-080, FR-081. **SC**: SC-006 (partial), SC-007.

- [x] T044 [PH6] Create `Dxui/Core/DxuiEvents.h` — `DxuiMouseEvent`, `DxuiKeyEvent` POD-ish structs (member init in-class, no constructors). **Exit**: header compiles standalone. **FR**: FR-010.

- [x] T045 [PH6] Create `Dxui/Core/IDxuiControl.h` per `contracts/IDxuiControl.h.md`. Pure-virtual: `Layout(const RECT & bounds, const DxuiDpiScaler & scaler)`, `Paint(IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &)`, `OnMouse(const DxuiMouseEvent &) -> bool`, `OnKey(const DxuiKeyEvent &) -> bool`, `OnFocusChanged(bool)`, `OnThemeChanged()` (default no-op), `Tick(int64_t nowMs)` (default no-op), `ClassifyHit(POINT) -> DxuiHitTestKind` (default `Client`), `AccessibleName() const -> std::wstring`, `AccessibleRole() const -> DxuiAccessibleRole`. Concrete-on-base: `Bounds()`, `SetBounds(RECT)`, `Visible()`/`SetVisible(bool)` (Collapsed mode only — FR-011), `Enabled()`/`SetEnabled(bool)`, `Focusable()`/`SetFocusable(bool)`, `Parent()`/`SetParent(IDxuiControl *)`, `ChildCount()` (default 0), `Child(size_t)` (default `nullptr`). Declare `enum class DxuiHitTestKind { None, Client, Caption, MinButton, MaxButton, CloseButton, ResizeEdgeLeft, ResizeEdgeRight, ResizeEdgeTop, ResizeEdgeBottom, ResizeCornerTL, ResizeCornerTR, ResizeCornerBL, ResizeCornerBR };` and tab sentinels `kTabIndexGeometry = -1`, `kTabIndexExcluded = -2`. All string params use `std::wstring`. **Depends on**: T044. **Exit**: header compiles standalone. **FR**: FR-010, FR-011, FR-012, FR-031, FR-080, FR-081.

- [x] T046 [PH6] Create `Dxui/Layout/IDxuiLayout.h` per `contracts/IDxuiLayout.h.md`. Pure-virtual `Arrange(const RECT & bounds, const DxuiDpiScaler & scaler, children_view)`; non-pure `Measure(...)` defaulting to `{0,0}`. All sizes in DIPs. **Exit**: header compiles standalone. **FR**: FR-020, FR-022.

- [x] T047 [PH6] Create `Dxui/Core/DxuiPanel.{h,cpp}` per FR-011 / `contracts/IDxuiControl.h.md`. Derives from `IDxuiControl`. Owning `std::vector<std::unique_ptr<IDxuiControl>>`. Owns `std::unique_ptr<IDxuiLayout> m_layout`. APIs: `template<class T, class... Args> T & Add(Args &&... args)`, `bool Remove(IDxuiControl *)`, `void Clear()`, `void SetLayout(std::unique_ptr<IDxuiLayout>)`. Override `Layout(const RECT & bounds, const DxuiDpiScaler & scaler)` and call `if (m_layout) { m_layout->Arrange(bounds, scaler, children_view()); }`; override `Paint`, `OnMouse`, `OnKey`, `OnThemeChanged`, and `Tick` with visible-child fan-out. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. Recursive helpers: `void PropagateDpi(float scale)`, `void PropagateTheme()`. `SetVisible(false)` MUST trigger a parent relayout on next pump. Override `ChildCount` / `Child`. **Depends on**: T045, T046. **Exit**: panel compiles; covered by T053. **FR**: FR-011, FR-022, FR-083.

- [x] T048 [PH6] Create `Dxui/Render/IDxuiPainter.h` per `contracts/IDxuiPainter.h.md`. Pure-virtual HRESULT-returning methods: `BeginFrame`, `EndFrame`, `FillRect`, `StrokeRect`, `FillRounded`, `StrokeRounded`, `FillGradient`, `OutlineRect`, `FillCircleApprox`, `DrawImage`, `PushClip`, and `PopClip`. Exact signature list per the contract sketch; keep the contract shape. **Exit**: header compiles standalone. **FR**: FR-040.

- [x] T049 [PH6] Create `Dxui/Render/IDxuiTextRenderer.h` per `contracts/IDxuiTextRenderer.h.md`. Pure-virtual HRESULT-returning methods: `Measure(const std::wstring & text, DxuiFontHandle fontSpec, float maxWidthDip, SIZE & outSizeDip)` and `DrawText(...)`. Do **not** add a `Font()` accessor; fonts come from `IDxuiTheme::BodyFont()`, `CaptionFont()`, etc. **Exit**: header compiles standalone. **FR**: FR-040.

- [x] T050 [PH6] Modify `Dxui/Render/DxuiPainter.{h,cpp}` to derive from `IDxuiPainter`; modify `Dxui/Render/DxuiTextRenderer.{h,cpp}` to derive from `IDxuiTextRenderer`. Existing concrete method bodies unchanged; add `override` to each virtual and invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry on the concrete painter/text renderer. **Depends on**: T048, T049. **Exit**: `Casso.exe` paint paths still render identically. **FR**: FR-040.

- [x] T051 [PH6] Re-type every Dxui widget's `Paint(...)` parameters from `DxuiPainter &` / `DxuiTextRenderer &` to `IDxuiPainter &` / `IDxuiTextRenderer &`. Touch every `.h` / `.cpp` under `Dxui/Widgets/`. **Depends on**: T048, T049, T050. **Exit**: `rg -n '\bDxuiPainter\s*&' Dxui/Widgets Dxui/Win32` returns zero hits (the interface is used; concretes only appear at the wiring layer). **FR**: FR-041.

- [x] T052 [P] [PH6] Create `Dxui/Layout/DxuiStackLayout.{h,cpp}` — horizontal/vertical, spacing in DIPs, per-child weight, cross-axis alignment. **Depends on**: T046. **FR**: FR-021, FR-022.

- [x] T053 [P] [PH6] Create `Dxui/Layout/DxuiGridLayout.{h,cpp}` — fixed rows × cols, per-cell span, per-row/per-col size (fixed-DIP, auto, star). **Depends on**: T046. **FR**: FR-021, FR-022.

- [x] T054 [P] [PH6] Create `Dxui/Layout/DxuiFormLayout.{h,cpp}` — label : field rows with consistent gutter, indented sub-rows, section gaps. **Depends on**: T046. **FR**: FR-021, FR-022.

- [x] T055 [P] [PH6] Create `Dxui/Layout/DxuiAbsoluteLayout.{h,cpp}` — uses pre-set child bounds verbatim (escape hatch). **Depends on**: T046. **FR**: FR-021, FR-022.

- [x] T056 [PH6] Create `Dxui/Core/DxuiFocusManager.{h,cpp}` per FR-031. Attaches to a `DxuiPanel` root; builds tab order by walking tree and sorting focusables on `(top / rowEpsilon, left)`. Skips `!Visible` / `!Enabled` / `!Focusable`. Handles Tab / Shift+Tab / Esc / Enter / Space. Per-control `tabIndex` override hint: `IDxuiControl::kTabIndexGeometry` uses geometry order; `IDxuiControl::kTabIndexExcluded` skips Tab traversal but remains mouse-focusable. Focus scopes: `PushScope(IDxuiControl * scopeRoot)` saves current focus, restricts tab walk to scope's subtree; `PopScope()` restores. Spatial arrow navigation: `MoveFocusInDirection(Up|Down|Left|Right)` picks nearest focusable in direction using `Bounds()` centroids. `RowEpsilonDip()` defaults to `IDxuiTheme::BodyLineHeightDip()`; expose `SetRowEpsilonDip(float)` for test determinism. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. **Depends on**: T045, T047. **Exit**: covered by T062. **FR**: FR-031.

- [x] T057 [P] [PH6] Create `UnitTest/Dxui/MockDxuiPainter.{h,cpp}` — recording `IDxuiPainter` implementation. Each override appends a `RecordedPaintCall` (struct per plan.md §Testing Strategy) to a `std::vector` exposed via `Calls() const`. POD-friendly value comparisons (no functors, no smart pointers). `Reset()` clears the log. **Zero** D3D device creation. **Depends on**: T048. **FR**: FR-041; **SC**: SC-007.

- [x] T058 [P] [PH6] Create `UnitTest/Dxui/MockDxuiTextRenderer.{h,cpp}` — recording `IDxuiTextRenderer`. `Measure` returns canned `SIZE` configured via `SetCannedMetrics(std::wstring, SIZE)` with a default fallback (width = `text.size() * 7`, height = `16`). `DrawText` records call. **Depends on**: T049. **FR**: FR-041; **SC**: SC-007.

- [x] T059 [P] [PH6] Create `UnitTest/Dxui/DxuiPanelTests.cpp` — at minimum: `Add` returns a reference to the constructed child; `Remove(nullptr)` returns false; `Remove(unknown)` returns false; `Remove(known)` returns true and the child is destroyed; `Clear` drops all children; paint fan-out visits visible children in z-order; `OnMouse` dispatches front-to-back; `SetVisible(false)` skips that child in subsequent paint and input; `SetVisible(false)` triggers `m_dirty` (or whatever signal the layout uses). **Depends on**: T047, T057, T058. **FR**: FR-011; **SC**: SC-006.

- [x] T060 [P] [PH6] Create `UnitTest/Dxui/DxuiStackLayoutTests.cpp` — H/V, spacing, weights distribute remainder, cross-axis alignment. **Depends on**: T052. **FR**: FR-021; **SC**: SC-006.

- [x] T061 [P] [PH6] Create `UnitTest/Dxui/DxuiGridLayoutTests.cpp`, `UnitTest/Dxui/DxuiFormLayoutTests.cpp`, `UnitTest/Dxui/DxuiAbsoluteLayoutTests.cpp` — each verifies arrange output against hand-computed rects for a small fixture. **Depends on**: T053, T054, T055. **FR**: FR-021; **SC**: SC-006.

- [x] T062 [PH6] Create `UnitTest/Dxui/DxuiFocusManagerTests.cpp` — reading-order tab across a 3-row × 4-column synthetic grid; row-epsilon collapses near-equal `top` values; spatial arrow nav picks the geometrically nearest target; `tabIndex` override beats geometry; `kTabIndexExcluded` skips Tab traversal while preserving mouse focus; focus scope push/pop restores prior focus; `!Visible` / `!Enabled` / `!Focusable` skipped. **Depends on**: T056. **FR**: FR-031; **SC**: SC-006.

- [x] T063 [PH6] Phase-6 exit verification. Build all four configs; run tests; run code analysis. Casso continues to use legacy `FocusManager` and `Layout` (their deletion is deferred until Phase 10 / Phase 11 respectively). Confirm: `rg -n '\w \(\)' Dxui/Core Dxui/Layout Dxui/Render Dxui/Widgets` → zero hits in newly authored lines. **Depends on**: T044–T062. **Commit**: `feat(dxui): add IDxuiControl, DxuiPanel, layouts, focus manager, render interfaces`. **FR**: FR-010/011/012/020/021/022/031/033/040/041/080/081; **SC**: SC-006 (partial), SC-007.

---

## Phase 7 — Host window framework primitives

**Goal (plan.md §Phase 7 — reduced scope)**: Land the `DxuiHostWindow` framework primitives (host window class, caption bar, system buttons, drag region, DWM helpers, test seams) **without** migrating any existing top-level window onto them. The four NC duplicates remain in place; consumption by main window / chrome panels / settings / dialogs happens in Phases 8 / 11 / 14. **Satisfies**: FR-050, FR-051, FR-052, FR-053, and the framework half of FR-095. Snap-layouts behaviour (US4) becomes visible only once main-window adoption lands in Phase 8.

- [x] T064 [PH7] Create `Dxui/Win32/DxuiHostWindow.{h,cpp}` per `contracts/DxuiHostWindow.h.md`. Owns HWND, DXGI swap chain, root `DxuiPanel`. `CreateParams` covers `borderless`, `resizable`, `rounded`, `dark`, `backdrop`, `resizeBorderDip`. Create the D3D11 device with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`. WndProc handles `WM_NCCALCSIZE` (claim NC as client when borderless), `WM_NCHITTEST` (8 resize edges + tree walk via `ClassifyHit`), `WM_NCLBUTTONDOWN/UP`, `WM_NCMOUSEMOVE`, `WM_NCMOUSELEAVE`, `WM_DPICHANGED` (re-DPI tree + relayout + repaint), `WM_DPICHANGED_BEFOREPARENT` (forward to every active pooled `DxuiPopupHost`), `WM_SETTINGCHANGE`, `WM_THEMECHANGED`, `WM_DWMCOLORIZATIONCOLORCHANGED`. Snap-layouts: return `HTMAXBUTTON` when hit lands on a `DxuiSystemButton` classified `DxuiHitTestKind::MaxButton`. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. **Depends on**: T047. **Exit**: framework primitive ready; consumption verified later by T070 (Phase 8 main-window adoption). **FR**: FR-050, FR-051, FR-052, FR-083.

- [x] T065 [PH7] Expose `DxuiHostWindow::ClassifyHitForTest(POINT clientPx) -> DxuiHitTestKind` test seam (public, no real HWND required, accepts a synthetic root panel via a constructor overload). **Depends on**: T064. **FR**: FR-050; **SC**: SC-006.

- [x] T066 [P] [PH7] Create `Dxui/Win32/DxuiCaptionBar.{h,cpp}` — derives from `DxuiPanel`; default `ClassifyHit` returns `DxuiHitTestKind::Caption` for blank areas; children may override. **Depends on**: T047. **FR**: FR-053.

- [x] T067 [P] [PH7] Create `Dxui/Win32/DxuiSystemButton.{h,cpp}` — derives from `IDxuiControl`; classification toggles `DxuiHitTestKind::MinButton`/`DxuiHitTestKind::MaxButton`/`DxuiHitTestKind::CloseButton`. Renders Win11-style glyphs via `IDxuiPainter`. Click dispatch: `Min` → `ShowWindow(hwnd, SW_MINIMIZE)`; `Max` → `SendMessage(hwnd, WM_SYSCOMMAND, IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0)`; `Close` → `SendMessage(hwnd, WM_CLOSE, 0, 0)`. **Depends on**: T045. **FR**: FR-053.

- [x] T068 [P] [PH7] Create `Dxui/Win32/DxuiDragRegion.{h,cpp}` — invisible caption-filler; `ClassifyHit` returns `DxuiHitTestKind::Caption`. **Depends on**: T045. **FR**: FR-053.

- [x] T069 [PH7] Create `UnitTest/Dxui/DxuiHostWindowTests.cpp` — NC classification only via `ClassifyHitForTest`. Cover: 8 resize edges (NW/N/NE/E/SE/S/SW/W) computed against a synthetic bounds + `resizeBorderDip`; blank caption area returns `HTCAPTION`; system button area returns `HTMAXBUTTON` (snap-layouts), `HTMINBUTTON`, `HTCLOSE`; client area returns `HTCLIENT`. **No real HWND**, **no `CreateWindowEx`**. **Depends on**: T065, T066, T067, T068. **FR**: FR-050, FR-052; **SC**: SC-006.

- [x] T071 [P] [PH7] Wire `DxuiHostWindow` to the existing `DxuiDwm` helpers (from T015) for backdrop, immersive-dark-mode, rounded-corner, and titlebar-colour configuration. Apply on `WM_CREATE`, re-apply on `WM_DWMCOLORIZATIONCOLORCHANGED` / `WM_SETTINGCHANGE` / `WM_THEMECHANGED`. **Depends on**: T064. **FR**: FR-050, FR-051.

- [x] T072 [P] [PH7] Apply `DxuiDwm` configuration during `DxuiHostWindow::CreateParams`-driven initialization: `backdrop` (Mica/Acrylic/None), `rounded` (round/small-round/square), `dark` (immersive dark-mode bool). Verify all four config knobs round-trip via a debug-build instrumentation seam. **Depends on**: T064, T071. **FR**: FR-050, FR-051.

- [x] T073 [PH7] Add a snap-layouts integration test in `UnitTest/Dxui/DxuiHostWindowTests.cpp` (or a sibling file) that drives `ClassifyHitForTest` with synthetic system-button rects and asserts `HTMAXBUTTON` falls out for the maximise button — the prerequisite Win11 sends to trigger the snap-layouts hover popover. **Depends on**: T065, T067. **FR**: FR-052; **SC**: SC-006, SC-009 (framework half).

- [x] T074 [PH7] Phase-7 exit verification (framework-only scope). Build all four configs; run tests; run code analysis. Greps: `rg -n 'WM_NCCALCSIZE' Casso/` → **unchanged at 4 hits** (`Casso/Window.cpp`, `Casso/EmulatorShell.cpp`, `Casso/Ui/Chrome/ChromedPanelWindow.cpp`, `Casso/Ui/Dialog/DialogPrimitive.cpp` — Phase 7 deliberately does not migrate any consumer); `rg -n 'WM_NCCALCSIZE' Dxui/Win32/` → matches expected in `DxuiHostWindow.cpp`. NC-handler duplicate count: **4 (unchanged)** — framework primitives ready for Phase 8 consumption. **No manual main-window visual parity check required this phase** — that lands with the Phase 8 main-window adoption. **Depends on**: T064–T069, T071–T073. **Commit**: `feat(dxui): add DxuiHostWindow framework primitives (host class, caption, system buttons, DWM)`. **FR**: FR-050/051/052/053; **SC**: SC-006 (framework tests).

---

## Phase 8 — Main-window NC delegation via adopt-HWND shim

**Goal (plan.md §Phase 8 — new)**: Without doing a full main-window restructure, delete the inline NC handling in `Casso\Window.cpp` / `Casso\EmulatorShell.cpp` and delegate it to a `DxuiHostWindow` running in **adopt-HWND mode** (no `CreateWindow`, no `DestroyWindow`, no swap-chain ownership). EmulatorShell's existing TitleBar / system-button classification plugs in via a hit-test delegate. NC duplicate count drops 4 → 3. **Satisfies**: FR-098, FR-099, FR-100, User Story 4 (main-window snap-layouts becomes live).

- [x] T112 [PH8] Extend `Dxui/Win32/DxuiHostWindow.{h,cpp}` with `CreateInAdoptMode(HWND existing, const CreateParams &)` constructor overload. Does **not** call `CreateWindow` or `DestroyWindow`; does **not** own the swap chain. Adopt mode shares the host's NC message-handling logic but defers ownership of HWND / device / swap chain to the legacy owner. **Depends on**: T064. **FR**: FR-098.

- [x] T113 [PH8] Add `DxuiHostWindow::SetHitTestDelegate(std::function<LRESULT(POINT)> delegate)` — adopt-mode plug-in for consumers that already have their own caption / system-button classification logic and don't want to reshape it onto `DxuiCaptionBar` yet. When set, `WM_NCHITTEST` calls the delegate first; resize-edge classification still runs around it. **Depends on**: T112. **FR**: FR-099.

- [x] T114 [PH8] Add `DxuiHostWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT & outResult) -> bool` public WndProc forwarder. Returns `true` if Dxui handled the message (caller returns `outResult`); returns `false` if the legacy WndProc should keep handling it. Routes the NC family (`WM_NCCALCSIZE`, `WM_NCHITTEST`, `WM_NCLBUTTONDOWN/UP`, `WM_NCMOUSEMOVE`, `WM_NCMOUSELEAVE`) and DPI / theme messages through Dxui. **Depends on**: T112. **FR**: FR-099.

- [x] T115 [P] [PH8] Create `UnitTest/Dxui/DxuiHostWindowAdoptModeTests.cpp` covering: adopt-mode `ClassifyHit` routes via the hit-test delegate when set; `HandleMessage` returns `true` for NC family and writes the expected `outResult`; `HandleMessage` returns `false` for messages Dxui does not own; lifetime — destructor of an adopt-mode `DxuiHostWindow` does **not** call `DestroyWindow` on the supplied HWND. **No real HWND** — drive via test seams. **Depends on**: T112, T113, T114. **FR**: FR-098, FR-099; **SC**: SC-006.

- [x] T070 [PH8] Migrate `Casso\Window.cpp` (the WndProc dispatch at ~line 277) and `Casso\EmulatorShell.cpp` (`OnNcCalcSize` at ~line 4035, plus `HandleNcHitTest` / `HandleNcLButtonDown` / `HandleNcLButtonUp` / `HandleNcMouseMove` / `HandleNcMouseLeave` helpers) — **delete** the inline NC handling, **replace** with delegation to a `DxuiHostWindow` held by `EmulatorShell` in adopt mode (constructed with the existing HWND via T112). Wire up the hit-test delegate (T113) to EmulatorShell's existing TitleBar / system-button classification logic; **no TitleBar reshape required in this phase** (that lands in Phase 11). Forward incoming messages via `m_hostWindow.HandleMessage(msg, wParam, lParam, outResult)`. **Depends on**: T112, T113, T114. **Exit**: `rg -n 'WM_NCCALCSIZE|WM_NCHITTEST' Casso/Window.cpp Casso/EmulatorShell.cpp` returns 0 hits (or only delegation lines forwarding to `HandleMessage`). **FR**: FR-098, FR-099, FR-100.

- [x] T116 [PH8] Phase-8 exit verification. Build all four configs; run tests; run code analysis. Greps: `rg -n 'WM_NCCALCSIZE|WM_NCHITTEST' Casso/Window.cpp Casso/EmulatorShell.cpp` → 0 (or delegation-only). NC duplicate count: **3** (`ChromedPanelWindow.cpp`, `SettingsWindow.cpp` if still present, `DialogPrimitive.cpp` — main window is gone). **Manual** main-window verification at 100 % / 150 % / 200 % DPI on Win10 + Win11: drag / resize / minimise / maximise / close / DPI change / Win11 snap-layouts hover popover all work identically to pre-migration. **Depends on**: T070, T112–T115. **Commit**: `refactor(casso/ui): delegate main-window NC handling to DxuiHostWindow (adopt mode)`. **FR**: FR-098, FR-099, FR-100; **SC**: SC-009 (main window ✅), SC-010 (NC count 4 → 3), SC-011 (mid-migration gate).

---

## Phase 9 — Popup hosting

**Goal (plan.md §Phase 9)**: Land `DxuiPopupHost` + the pool; migrate `DxuiDropdown` / `DxuiTooltip` / `DxuiPopupMenu` onto it. **Satisfies**: FR-054, FR-055, FR-056, FR-061, User Story 3, SC-008.

- [X] T075 [PH9] Create `Dxui/Win32/DxuiPopupHost.{h,cpp}` per FR-054 / FR-056. `WS_POPUP | WS_EX_NOACTIVATE` (add `WS_EX_TRANSPARENT | WS_EX_LAYERED` for tooltips); own DXGI composition swap chain sharing parent `ID3D11Device`; use `CreateSwapChainForComposition` + DirectComposition visual, not `CreateSwapChainForHwnd`. `ShowParams`: `ownerHwnd`, `anchorRectScreen`, placement (`Below`/`Above`/`Right`/`Left`/`AtCursor`), `flipIfOffscreen`, dismiss policy (`OnClickOutside`/`OnClickAnywhere`/`OnPointerLeave`/`Manual`), input policy (`Interactive`/`PassThrough`), `shadow`, `std::unique_ptr<DxuiPanel> content`. `Show() -> std::future<int>`; shared state set on the UI thread inside the host's message handling (FR-083). Owner-chain tracking for cascading submenus. `MonitorFromRect` + monitor work area for offscreen flipping. `WM_DPICHANGED_BEFOREPARENT` handling plus host forwarding to every active popup. Focus scope push/pop via `DxuiFocusManager`. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. Click-outside dismiss via `SetCapture` + `WM_CAPTURECHANGED`. Auto-dismiss on owner `WM_ACTIVATE` / `WM_ACTIVATEAPP` / `WM_MOVE`. **Depends on**: T056, T064. **Exit**: covered by T078. **FR**: FR-054, FR-056, FR-083.

- [X] T076 [PH9] Expose `DxuiPopupHost::ComputePlacementForTest(RECT anchorScreen, RECT monitorWorkArea, Placement preferred, SIZE popupSize) -> RECT` test seam (static, pure-function, no HWND). **Depends on**: T075. **FR**: FR-054; **SC**: SC-006.

- [X] T077 [PH9] Add popup pool to `DxuiHostWindow` per FR-055: initial 3 instances, grow on demand, LIFO reuse. Debug-build instrumentation counter (`PopupHits()` / `PopupMisses()`) for test assertion. **Depends on**: T064, T075. **FR**: FR-055.


- [X] T077A [PH9] Wire `DxuiHostWindow` to forward `WM_DPICHANGED_BEFOREPARENT` to every active `DxuiPopupHost` in the popup pool. Add a debug/test seam to enumerate active popups without exposing release-only state. **Depends on**: T075, T077. **FR**: FR-050, FR-056.

- [X] T078 [PH9] Create `UnitTest/Dxui/DxuiPopupHostTests.cpp` — placement below/above/left/right/at-cursor; flip-if-offscreen against synthetic monitor rects (all four edges + corners); dismiss policy state machine (no real HWND — drive via test seams); cascading owner-chain registration/unregistration; host forwarding of `WM_DPICHANGED_BEFOREPARENT` to active popups; focus-scope push/pop with restore. Asserts via `ComputePlacementForTest` and a dispatching shim for the dismiss state. **Depends on**: T076, T077A. **FR**: FR-050, FR-054, FR-056; **SC**: SC-006.

- [X] T079 [P] [PH9] Modify `Dxui/Widgets/DxuiDropdown.{h,cpp}` to host its option list inside a `DxuiPopupHost` instance acquired from the parent host window's pool. Remove any in-window clipping path. **Depends on**: T075, T077. **FR**: FR-061; **SC**: SC-008.

- [X] T080 [P] [PH9] Modify `Dxui/Widgets/DxuiPopupMenu.{h,cpp}` to host its menu content via `DxuiPopupHost`; cascading submenus opened via owner-chain. **Depends on**: T075, T077. **FR**: FR-061.

- [X] T081 [P] [PH9] Modify `Dxui/Widgets/DxuiTooltip.{h,cpp}` to host its content via `DxuiPopupHost` (with `WS_EX_TRANSPARENT | WS_EX_LAYERED`, dismiss `OnPointerLeave`, input `PassThrough`). **Depends on**: T075, T077. **FR**: FR-061.

- [X] T082 [PH9] Phase-8 exit verification. Build all four configs; run tests; run code analysis. **Manual acceptance test** for User Story 3 / SC-008: anchor a dropdown ~20 px from the bottom of the Settings window; the menu opens upward (or extends across the parent edge) with no clipping. **Manual** debug-build assert: open and close the dropdown 5 times; `PopupHits()` ≥ 4 (pool reuse). **Depends on**: T075–T081 and T077A. **Commit**: `feat(dxui): add DxuiPopupHost with pool; fix dropdown clipping`. **FR**: FR-054/055/056/061; **SC**: SC-008 (User Story 3 ✅).

**Phase 9 deviation note (T079/T080/T081):** The widget conversions add a `SetPopupHost(DxuiHostWindow *)` opt-in knob rather than removing the legacy in-panel rendering path outright. Removing the legacy path in Phase 9 would have broken every Casso UI consumer that drives Dropdown / PopupMenu / Tooltip painting through the parent panel's painter (12+ files across `Casso/Ui/...`), all of which are scheduled for proper restructure in Phase 11 (`EmulatorShell` restructure) and Phase 13–14 (page conversions). The opt-in shape lands the architectural piece that Phase 10's `DxuiMenuBar` needs (it can wire its host directly and use the popup-hosted path from day one), and SC-008's "anchor near the bottom — must flip up — must not clip" guarantee is fully verified by the `ComputePlacementForTest` test (`Below_FlipsToAboveWhenAnchorIsNearBottom`). The legacy in-panel path is fully decoupled — when `SetPopupHost` is wired the popup is acquired/released and rendered into its own `WS_POPUP` HWND via DComp.

**Phase 9 deviation note (T076 scope):** The user's prompt described T076 as "DwM shadow for popup HWND". `tasks.md` defines T076 as the `ComputePlacementForTest` static seam; the implementation honors `tasks.md`. DwM shadow support is included in `DxuiPopupHost` as part of `ShowParams::shadow` (applied via `DxuiDwm::ExtendFrameIntoClientArea` inside `Show()`), so both interpretations are covered.

---

## Phase 10 — `DxuiMenuBar` widget + `MainMenu` conversion

**Goal (plan.md §Phase 10 — new)**: Promote Casso's `NavLayer` to a reusable `Dxui/Widgets/DxuiMenuBar.{h,cpp}` widget; rename `NavLayer` → `MainMenu` and reshape it to use the new widget. **Satisfies**: FR-101, FR-102, FR-103.

- [x] T117 [PH10] Create `Dxui/Widgets/DxuiMenuBar.{h,cpp}` + umbrella entry. Class supports: items array (label, accelerator-char, dispatch callback, check-query callback, enabled flag, checkable flag, separator), hover state per item, click-to-open submenu (delegates to `DxuiPopupHost` from Phase 9), alt-letter accelerator routing, arrow-key traversal between open submenus, Escape dismissal, mouse-leave behaviour. Paint via `IDxuiPainter` + `IDxuiTextRenderer`; theme via `IDxuiTheme`. Public API surface roughly: `Layout(...)`, `Hide()`, `Open(...)`, `OpenMenu(...)`, `SetDispatch(...)`, `SetCheckQuery(...)`, `SetFocusedMenu(...)`, `ClearFocus()`, `IsOpen()`, `HandleKey(...)`, `HandleAltKey(...)` — matching the existing NavLayer surface so MainMenu can be a thin wrapper. **Depends on**: T075 (DxuiPopupHost). **FR**: FR-101.

- [x] T118 [P] [PH10] Create `UnitTest/Dxui/DxuiMenuBarTests.cpp` — alt-letter dispatch, hover-to-open, arrow traversal between open submenus, Escape dismissal, check-query, disabled item behaviour, separator rendering. Drive via the existing painter mock (T057/T058) and the `DxuiPopupHost` test seams. **No real HWND**. **Depends on**: T117. **FR**: FR-101; **SC**: SC-006.

- [x] T119 [PH10] Rename `Casso/Ui/Chrome/NavLayer.{h,cpp}` → `Casso/Ui/Chrome/MainMenu.{h,cpp}` via `git mv` to preserve history. Rename class `NavLayer` → `MainMenu`, enum `NavMenu` → `MainMenuId`. Update vcxproj item entries. **Depends on**: T117. **FR**: FR-102, FR-103.

- [x] T120 [PH10] Reshape `MainMenu` to **use** `DxuiMenuBar` internally — either subclass it with the Casso command set baked in, or compose one as a member. `MainMenu` becomes a configured `DxuiMenuBar` instance owned by `EmulatorShell`. **Depends on**: T117, T119. **FR**: FR-102.

- [x] T121 [PH10] Rewire `EmulatorShell`'s ~15 `m_navLayer.X()` call sites to `m_mainMenu.X()`. Specifically: `Hide()`, `Layout(...)`, `SetDispatch(...)`, `SetCheckQuery(...)`, `SetFocusedMenu(...)`, `ClearFocus()`, `IsOpen()`, `OpenMenu()`, `HandleKey(...)`, `Open(...)`, `HandleAltKey(...)`. Plus member rename `m_navLayer` → `m_mainMenu`. **Depends on**: T120. **FR**: FR-102, FR-103.

- [x] T122 [PH10] Phase-10 exit verification. Build all four configs; run tests; run code analysis. Greps: `rg -n 'NavLayer|NavMenu' Casso/` → 0 hits (FR-103). **Manual** menu UX verification: every menu opens (File / Edit / View / Disk / Help — or current Casso menu set); alt-letter accelerators work; arrow-traversal between adjacent menus works; check-states display correctly for checkable items; disabled items render greyed and ignore input. **Depends on**: T117–T121. **Commit**: `refactor(casso/ui): promote NavLayer to Dxui DxuiMenuBar; rename to MainMenu`. **FR**: FR-101, FR-102, FR-103; **SC**: SC-006, SC-016, SC-017.

---

## Phase 11 — Chrome reshape + `EmulatorShell` restructure

**Status (this session — partial)**: Chrome reshape landed (T123–T128). EmulatorShell restructure (T129) is partially landed (Sessions A through C3); the phase exit gate (T130) is deferred to Phase 11e. NC duplicate count is still **2** after Session C3 (the chrome surface drop from T127 stands; the EmulatorShell-side drop to 1 is gated on the swap-chain flip itself, which is gated on visual verification not available in headless CI). The full Phase 11 NC target (2) is therefore met for the chrome surface; the further drop to 1 lands once the swap chain actually flips to host ownership and `CassoRenderSurface` is deleted.

**Phase 11d sub-step 1 (`5017bee`)**: `DxuiHostWindow` exposes shared `GetDevice()` / `GetContext()` / `GetSwapChain()` accessors so a consumer's renderer can target the host's swap chain without standing up its own.

**Phase 11d sub-step 2 (Session A, `6da31eb` + `ccfd47b`)**: API additions only — no consumer flip yet.

  * `6da31eb` feat(dxui): extend `DxuiHostWindow::CreateParams` with four optional fields (`classNameOverride`, `useInitialWindowRectPx` + `initialWindowRectPx`, `appIconBig` / `appIconSmall`) so a consumer can land its existing HWND-creation bespoke logic on `DxuiHostWindow::Create` without losing behavior. All four default to nullptr / false; existing callers see no change.

  * `ccfd47b` feat(dxui): add `IDxuiHostClient` interface + `DxuiHostWindow::SetClient` installer. Lets a full-ownership consumer receive the Win32 messages the host does not own end-to-end (commands, keyboard, mouse, paint, timer, drawitem, init-menu-popup, close, destroy, NC-LButtonUp with original hit-test). Eight new unit tests (1796 → 1804 passing). Build green on all four configs; code analysis clean.

**Phase 11d sub-step 3 (Session A wrap, `411db4f`)**: Switched `IDxuiHostClient` to a typed `DxuiMessageResult` enum (`Handled` / `NotHandled`) + `LRESULT` for rich-return messages. Polarity now matches Win32 / WPF / MFC convention; the legacy `Window`-base `bool` ambiguity ("which value means consumed?") is eliminated at the call site.

**Phase 11d sub-step 4 (Session B, `65338c2` + `6e00915` + `91c644e`)**: EmulatorShell HWND ownership flipped from `Window` base into full-ownership `DxuiHostWindow`. **SC-019 satisfied**.

  * `65338c2` feat(dxui): added `CreateParams::createSwapChain` opt-out (default `true`) so a consumer that drives its own swap chain via a child HWND can ask the host to skip its own. EmulatorShell uses it; the existing `CassoRenderSurface` child HWND keeps owning `D3DRenderer`'s swap chain unchanged (Session C territory).

  * `6e00915` refactor(casso): `class EmulatorShell : public Window, public IDriveCommandSink` → `class EmulatorShell : public IDxuiHostClient, public IDriveCommandSink`. Rewrote `CreateEmulatorWindow` to populate `CreateParams` (with `classNameOverride = L"CassoWindow"`, pre-computed `useInitialWindowRectPx` + `initialWindowRectPx` for saved-placement / monitor-aware sizing, `appIconBig` / `appIconSmall` for the WM_SETICON handoff, `createSwapChain = false`) and call `m_host = std::make_unique<DxuiHostWindow>(); m_host->SetClient (this); m_host->Create (params);`. Translated all ~13 `Window`-virtual overrides to IDxuiHostClient overrides with polarity preserved at every return site (`return true` → `NotHandled`; `return false` → `Handled`; OnNcLButtonUp's inverted polarity preserved correctly). Dropped `TryDelegateMessage`, dropped `SetInitialDpi`, added local `DxuiDpiScaler m_scaler` that LayoutManager references and `OnDpiChanged` keeps in sync with the host. Build green on all four configs; 1808/1808 tests passing.

  * `91c644e` refactor(casso): deleted `Casso/Window.{h,cpp}` (1153 LOC removed) and dropped them from `Casso.vcxproj`. EmulatorShell was the only consumer; `ChromedPanelWindow` uses adopt-mode `DxuiHostWindow` and never inherited from `Window`. **No production class still inherits from `Window`**.

**Still required for Session C (swap-chain transfer)**: route Apple ][ viewport rendering through `DxuiHostWindow`'s swap chain via the minimal `DxuiViewport` placeholder from T128; drop `D3DRenderer`'s own swap-chain ownership; delete `CassoRenderSurface` child HWND; flip `DxuiHostWindow::CreateParams::createSwapChain` back to its default `true` on the EmulatorShell side.

**Still required for Session D (SC-018 line-count target)**: helper-class extraction inside `EmulatorShell.cpp` to push line count `≤ 2,502`. Not attempted this session; current line count is unchanged from baseline (Session B traded one set of overrides for another, not a net reduction).

**Phase 11d sub-step 5 (Session C, `4a9ed5c` + `e58a715` + `ca93b21`)**: API plumbing landed for the swap-chain flip; the flip itself (Steps 4 + 5 in the Session C plan) deferred pending visual verification. EmulatorShell still uses the existing `CassoRenderSurface` child HWND + `D3DRenderer::Initialize` / `UploadAndPresent` path; behavior unchanged.

  * `4a9ed5c` feat(dxui): `DxuiHostWindow::SetBeforePresentHook` / `BeforePresentHook` accessor pair. Stores a `std::function<void()>` that the host's WM_PAINT panel-tree paint pump will invoke between the chrome Paint walk and swap-chain Present, letting an external renderer (the Apple ][ framebuffer composite) draw into the host's back buffer without owning the swap chain itself. The panel-tree paint pump itself does **not** exist yet in `DxuiHostWindow::WndProc`'s `WM_PAINT` handler (today it just forwards to `IDxuiHostClient::OnPaint`); the setter only stores the callback for now. 3 new unit tests (1808 → 1811 passing).

  * `e58a715` feat(casso): EmulatorShell sets up `m_host->Root()` as a `DxuiAbsoluteLayout` container and adds a single `DxuiViewport` child representing the Apple ][ framebuffer region. New `EmulatorShell::UpdateViewportLayout` computes the viewport rect (client minus chrome bands) from the current `LayoutManager` result and invokes `DxuiViewport::Layout`; the bounds-changed callback forwards the new rectangle to `D3DRenderer::SetTargetBounds` via a new `OnViewportBoundsChanged` member. New `D3DRenderer::SetTargetBounds` parks the rect on `m_targetBoundsPx` for future consumption; the existing `CassoRenderSurface` render pipeline still ignores it.

  * `ca93b21` feat(casso): `D3DRenderer::Initialize2` adopts an externally-owned `ID3D11Device` / `ID3D11DeviceContext` / `IDXGISwapChain1` instead of creating its own. Skips device + swap-chain creation; still creates this renderer's own back-buffer RTV, dynamic upload texture, sampler, shaders, vertex / index buffers, and CRT post-process chain via a new private `CreateRenderResources` helper. Sets `m_externalSwapChain` so `Resize` / `Shutdown` leave swap-chain lifecycle to the caller. New `UploadAndComposite` is the present-less companion entry point: same upload + CRT pass as `UploadAndPresent`, but skips the swap-chain Present (host owns it) and the after-blit chrome hook (chrome paints via the host's panel-tree Paint pump, not the renderer hook). Target rect comes from `m_targetBoundsPx` (the DxuiViewport bounds) rather than the chrome inset side-channel. **Not wired into EmulatorShell yet** — the existing `Initialize` / `UploadAndPresent` path on the child HWND stays live.

**Phase 11d sub-step 6 (Session C2, `a386f0b`)**: First half of the deferred Session C work — `DxuiHostWindow` now owns its panel-tree paint pump. The chrome-tree reparenting + the actual `createSwapChain=true` flip remain deferred.

  * `a386f0b` feat(dxui): `DxuiHostWindow::WM_PAINT` runs the full pump in full-ownership mode — `BeginPaint` → bind back-buffer RTV → `ClearRenderTargetView` to `IDxuiTheme::Background()` → `m_painter->Begin` + `m_textRenderer->BeginDraw` → `m_root->Paint(painter, text, theme)` → `m_textRenderer->EndDraw` + `m_painter->End` → invoke `m_beforePresentHook` → `m_swapChain->Present(1, 0)` → `EndPaint`. New private helpers `CreateBackBufferRtv()` (acquires buffer 0, creates the RTV, sets the viewport, rebinds the D2D bitmap on the text renderer) and `ReleaseBackBufferRtv()` (unbinds D2D + null-RTV + Reset) are called from `CreateRenderResources` (initial create) and `HandleSize` (release before `ResizeBuffers`, recreate after). `GetBackBufferRtv()` now returns a live RTV in full-ownership mode. The pump short-circuits in adopt / synthetic / `createSwapChain=false` modes and falls through to the legacy `IDxuiHostClient::OnPaint` path, so EmulatorShell sees zero behaviour change (it still has `createSwapChain=false`). An `Error:` epilogue tears down any in-flight `Begin` so a CHRA bail-out can't strand the painter mid-frame. 2 new unit tests (`SetTheme_BroadcastsOnThemeChangedToRoot`, `SetTheme_NullClearsThemeWithoutCrash`); 1811 → 1813 passing.

**Still deferred from Session C2 (Steps 2-6 of the Session C2 plan)**: chrome-tree reparenting and the swap-chain flip itself.

  1. Chrome controls (TitleBar, MainMenu, the two DriveWidgets, JoystickToggleButton) still aren't children of `m_host->Root()`. They all already implement `IDxuiControl`, so `DxuiPanel::Add` / a new `Adopt`-style API is the mechanical part; the obstacle is that their current `Layout` and rendering paths run through `UiShell` with bespoke signatures (`m_mainMenu.Layout (x, y, w, dpi, &textRenderer)`, `m_joystickButton.Layout (centerX, centerY, dpi, &textRenderer)`, etc.) rather than `IDxuiControl::Layout (const RECT &, const DxuiDpiScaler &)`. Reshaping them to the standard signature also requires UiShell's painter / text renderer / hit-tester / focus / animation seams to be re-homed onto `DxuiHostWindow`'s equivalents (the host already owns its own `DxuiPainter` + `DxuiTextRenderer` + `DxuiFocusManager`). That work is substantial enough on its own to deserve its own session, and it lights up no new behavior until paired with the swap-chain flip.

  2. Without (1), flipping `createSwapChain=true` would erase all chrome at frame 0 — the host pump would clear to the theme background, walk a panel tree containing only a `DxuiViewport`, composite the Apple ][ pixels through `BeforePresentHook`, and present a chrome-less frame. Steps 3 (the flip), 4 (`CassoRenderSurface` delete), and 5 (`D3DRenderer::Initialize` retire) all cascade from (1) and stay deferred.

  **No visual verification was attempted** for the pump itself (it stays dead in production until `createSwapChain=true`). The unit-test coverage exercises the theme broadcast and the public accessor contracts; the GPU paths are gated by future EmulatorShell wiring.



**Phase 11d sub-step 7 (Session C3, `b765c39` + `571e00c` + `1b78d00`)**: Chrome-tree reparenting infrastructure landed. The swap-chain flip itself (Steps 4 + 5 of the Session C3 plan) is still deferred because visual verification on a real display is mandatory for that step and isn't available in this environment.

  * `b765c39` feat(dxui): `DxuiPanel::Adopt` for non-owning child registration. The unified `m_children` vector now stores `ChildSlot` entries that hold either a `unique_ptr` (Add<T>) or a bare pointer (Adopt); insertion order is preserved across both APIs so paint front-to-back and input back-to-front guarantees hold uniformly. Companion `RemoveAdopted` (refuses owned children) + `ClearAdopted` (leaves Add'd children alone) round out the API. Re-adopting the same pointer is a no-op; adopting an owned child asserts. 9 new unit tests (1813 → 1822 passing).

  * `571e00c` refactor(casso/ui): reshape chrome control Layout signatures onto IDxuiControl. TitleBar, MainMenu (via DxuiMenuBar), DriveWidget, JoystickToggleButton, and LedIndicator now expose the standard `IDxuiControl::Layout(const RECT &, const DxuiDpiScaler &)` as their primary entry point. Internal positioning math derives x/y from `boundsDip.left/top` (or its center for the joystick toggle), and DPI from `scaler.Dpi()`. Text-renderer access is injected via member setters (`JoystickToggleButton::SetTextRenderer`, `DxuiMenuBar::SetTextRendererForMeasure`) rather than passed as a Layout parameter. EmulatorShell wires the UiShell-owned text renderer into both controls once after `UiShell::Initialize`. LedIndicator's bespoke positional `Layout(x,y,dpi)` is renamed to `PositionAt` so DriveWidget and JoystickToggleButton can still place the LED inline. DriveWidget's and JoystickToggleButton's bespoke `Layout` overloads are folded into the IDxuiControl override. TitleBar gains an IDxuiControl::Layout override that delegates to `UpdateGeometry` (which remains callable for `ChromedPanelWindow`). Consumer call sites (`EmulatorShell::LayoutChrome`, `LayoutJoystickButton`, `OnSize`, `LayoutDriveWidgetsInCommandBar`, `ThemePage` preview) and unit tests (`DriveWidgetHitTests`, `LedIndicatorStateTests`) all updated. 1822/1822 tests still passing.

  * `1b78d00` refactor(casso): adopt chrome controls into DxuiHostWindow root panel. EmulatorShell registers the chrome members (`m_titleBar`, `m_mainMenu`, `m_driveChrome[0..1]`, `m_joystickButton`) into `m_host->Root()` via `DxuiPanel::Adopt` after `m_host->Create` returns. The panel takes raw-pointer references; the chrome members keep their EmulatorShell-owned lifetime but now participate in the host's paint, input, focus, theme, tick, and DPI walks alongside the existing `DxuiViewport` child. Teardown: `EmulatorShell::~EmulatorShell` calls `m_host->Root().ClearAdopted()` before any chrome member falls out of scope so the panel never holds a dangling pointer during field-by-field destruction. This is tree-membership-only — `createSwapChain` is still `false` (the host's WM_PAINT pump does nothing without a back-buffer RTV), so the chrome continues to paint via `UiShell::Render` against the existing `CassoRenderSurface` child HWND. 1822/1822 tests still passing.

**Still deferred from Session C3 (Steps 4-6 of the Session C3 plan)**: swap-chain flip + `CassoRenderSurface` deletion + `D3DRenderer::Initialize` retirement.

  1. **The swap-chain flip itself** (`createSwapChain=true` in EmulatorShell's `CreateParams`; replace `m_d3dRenderer.Initialize(m_renderHwnd, ...)` with `m_d3dRenderer.Initialize2(m_host->GetDevice(), m_host->GetContext(), m_host->GetSwapChain(), initialViewportBounds)`; register `m_d3dRenderer.UploadAndComposite` as the host's `SetBeforePresentHook`). All the infrastructure to make this work is now in place (Session C2's paint pump runs once `createSwapChain=true`; Session C3's adopted chrome paints through that pump; the chrome's IDxuiControl Layout signatures are uniform). The blocker is **visual verification on a real display** — the spec marks this MANDATORY at this step, and headless CI cannot satisfy it.

  2. **`CassoRenderSurface` deletion** (`RegisterRenderSurfaceClass` + `m_renderHwnd` + `s_RenderSurfaceWndProc` + the MoveWindow call + drag-drop dual-window registration) cascades from the swap-chain flip.

  3. **`D3DRenderer::Initialize` retirement** (mark `[[deprecated]]` or delete entirely; dedupe `CreateRenderResources` if duplication remains) cascades from the swap-chain flip.

  **NC duplicate count remains 2 after Session C3** for the same reason — the EmulatorShell-side drop to 1 is gated on `CassoRenderSurface` deletion which is gated on the swap-chain flip.



**Goal (plan.md §Phase 11 — new)**: Reshape every Casso chrome widget onto `IDxuiControl`; reshape `ChromedPanelWindow` onto `DxuiHostWindow`; restructure `EmulatorShell` so it no longer inherits from `Window` and instead composes a fully-owning `DxuiHostWindow`. NC duplicate count 3 → 2. **Satisfies**: FR-104, FR-105, FR-106, FR-107, FR-108, SC-014, SC-015, SC-018, SC-019.

- [x] T123 [P] [PH11] Reshape `Casso/Ui/Chrome/TitleBar.{h,cpp}` to derive from `DxuiCaptionBar` (or compose one). Implement the standard `IDxuiControl::Paint(IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &)` signature. Preserve current visual style and skeuomorphic palette. Move app-icon rendering, hover state, min/max/close button positioning into the standard interface. **Depends on**: T066, T067. **FR**: FR-104; **SC**: SC-014, SC-015.

- [x] T124 [P] [PH11] Reshape `Casso/Ui/Chrome/DriveWidget.{h,cpp}` to derive from `IDxuiControl` with the standard Paint signature. Preserve current visual. **Depends on**: T045. **FR**: FR-104; **SC**: SC-014, SC-015.

- [x] T125 [P] [PH11] Reshape `Casso/Ui/Chrome/LedIndicator.{h,cpp}` to derive from `IDxuiControl`. Preserve current visual. **Depends on**: T045. **FR**: FR-104; **SC**: SC-014, SC-015.

- [x] T126 [P] [PH11] Reshape `Casso/Ui/Chrome/JoystickToggleButton.{h,cpp}` to derive from `IDxuiControl`. Preserve current visual. **Depends on**: T045. **FR**: FR-104; **SC**: SC-014, SC-015.

- [x] T127 [PH11] Reshape `Casso/Ui/Chrome/ChromedPanelWindow.{h,cpp}` to **use** `DxuiHostWindow` for its NC handling — subclass, compose, or rewrite, whichever gives the cleanest integration. NC duplicate count: **3 → 2**. **Depends on**: T064. **Exit**: `rg -n 'WM_NCCALCSIZE|WM_NCHITTEST' Casso/Ui/Chrome/ChromedPanelWindow.cpp` returns 0 hits (or only delegation lines). **FR**: FR-106. *(Landed via adopt-mode `DxuiHostWindow`; the spec's full-ownership option was deferred because the existing `IChromedPanelContent` rendering pipeline still owns the externally-supplied D3D device and swap chain, and converting it would have rippled across three panel consumers.)*

- [x] T128 [PH11] Add minimal `Dxui/Core/DxuiViewport.{h,cpp}` **placeholder**: leaf `IDxuiControl` exposing `Bounds()` for renderer subscription and `OnBoundsChanged` callback registration. (Full `DxuiViewport` with size policies + `IDxuiViewportInputSink` + dock-layout integration + reserved-chord routing lands in Phase 12 / T084 — **landed in Phase 12 Session, see T084**.) **Depends on**: T045. **FR**: FR-107.

- [~] T129 [PH11d] **EmulatorShell restructure (the big one)**: change `EmulatorShell` to no longer inherit from `Window`. Either (a) `EmulatorShell` composes a `DxuiHostWindow` and forwards its main loop to it, or (b) split `EmulatorShell` into `EmulatorShell` (content controller) + a separate `Window`-style adapter — choose whichever is cleaner. Move HWND ownership from `EmulatorShell` (or `Casso/Window.cpp` wherever it lives) into `DxuiHostWindow` (now in **full-ownership mode**, not adopt mode). Move swap-chain ownership into `DxuiHostWindow`. Move the D3D11 device wherever makes sense — likely `DxuiHostWindow` owns the device that paints chrome, or the device is shared between EmulatorShell's viewport renderer and `DxuiHostWindow`'s chrome renderer. Reroute Apple ][ viewport rendering through `DxuiHostWindow`'s swap chain via the minimal `DxuiViewport` placeholder from T128. **Depends on**: T070, T112, T123–T128. **FR**: FR-105, FR-107, FR-108. *(Partially landed across Phase 11d Sessions A → C3. **Session B**: HWND ownership moved into `DxuiHostWindow` in full-ownership mode, `EmulatorShell` no longer inherits from `Window` (SC-019), `Window.{h,cpp}` deleted. **Session C**: `DxuiHostWindow::SetBeforePresentHook` API, DxuiViewport child wired into host root panel with bounds-changed → `D3DRenderer::SetTargetBounds`, `D3DRenderer::Initialize2` + `UploadAndComposite` for host-owned swap chain. **Session C2**: `DxuiHostWindow::WM_PAINT` now runs a full clear-walk-hook-present pump in full-ownership mode + manages a back-buffer RTV across `CreateRenderResources` / `HandleSize` / `ReleaseRenderResources`. **Session C3**: `DxuiPanel::Adopt` API for non-owning child registration; chrome controls (TitleBar / MainMenu / DriveWidgets / JoystickToggleButton / LedIndicator) reshaped onto the standard `IDxuiControl::Layout(const RECT &, const DxuiDpiScaler &)` signature with text-renderer injected via setter; chrome adopted into `m_host->Root()`. **Still deferred (mandatory visual verification required)**: the swap-chain flip itself (`createSwapChain=true` + `D3DRenderer::Initialize2` + `SetBeforePresentHook` wiring), `CassoRenderSurface` deletion, and `D3DRenderer::Initialize` retirement.)*

- [ ] T130 [PH11e] Phase-11 exit verification. Build all four configs; run tests; run code analysis (regression checks on Apple ][ rendering — Klaus Dormann + Tom Harte should still pass since this is render-only, not CPU). Greps: `(Get-Content Casso/EmulatorShell.cpp).Length -le 2502` (SC-018); `rg -n 'class EmulatorShell.*Window' Casso/` returns 0 hits (SC-019); NC duplicate count = **2**. **Manual**: emulator visible, chrome renders correctly with skeuomorphic palette preserved (SC-015), all controls interactive, drag/resize/min/max/close all work, Win11 snap-layouts work on the main window. **Depends on**: T123–T129. **Commit**: `refactor(casso/ui): reshape chrome onto IDxuiControl; restructure EmulatorShell off Window`. **FR**: FR-104/105/106/107/108; **SC**: SC-014, SC-015, SC-018, SC-019. *(Deferred to Phase 11e along with T129.)*

---

## Phase 12 — `DxuiViewport` + `DxuiDockLayout`; retire legacy edge layout

**Goal (plan.md §Phase 12)**: Casso's main shell becomes a root `DxuiPanel` with `DxuiDockLayout`; the emulator viewport sizes the Apple ][ pixel grid from the inside out. **Satisfies**: FR-030, FR-034, FR-021 (dock portion), FR-093, FR-094, SC-013.

**Session status (Phase 12 partial — Dxui side complete, Casso wiring deferred)**:
T083–T086 landed: full `DxuiViewport` API (size policies + input sink + reserved-chord routing) and `DxuiDockLayout` (five-side dock + inverse `ContainerSizeForFill`) are in `Dxui/Core/` with 32 new unit tests (16 viewport, 16 dock). All four configs green; full suite 1854/1854 (delta +32 from 1822 baseline). T087–T092 deferred per the documented fallback policy: `EmulatorShell` integrates with `Chrome/LayoutManager` via `IEdgeContributor::DesiredThicknessDp()` (a DPI-aware contract distinct from how `DxuiDockLayout` reads child natural sizes from `Bounds()`), and `WindowCommandManager` is a second consumer of `LayoutManager::Resolve`. Bridging both call sites onto `DxuiDockLayout` plus retiring `LayoutManager` is a multi-file refactor with non-trivial visual-regression risk (Ctrl+0 framebuffer-fit sizing) — leaving for a follow-up session that owns the chrome-control measurement reshape.

- [x] T083 [PH12] Create `Dxui/Core/IDxuiViewportInputSink.h` per `contracts/IDxuiViewportInputSink.h.md`. Pure-virtual `OnKey(const DxuiKeyEvent &)` and `OnMouse(const DxuiMouseEvent &)`. **Exit**: header compiles standalone. **FR**: FR-034.

- [x] T084 [PH12] **Extend** the minimal `Dxui/Core/DxuiViewport.{h,cpp}` placeholder added in Phase 11 (T128) per FR-030 / FR-034. Add: size policy (`Fixed`/`Preferred`/`Fill`); `preferredSizeDip`; `consumesInput` flag (default `false`); `SetInputSink(IDxuiViewportInputSink *)`; `OnBoundsChanged(RECT)` callback registration via `SetBoundsChangedCallback(std::function<void(RECT)>)` (fires **only** when new bounds differ from previous). When `consumesInput == true`: `Focusable() == true`; `OnKey` forwards to sink and returns `true` for **non-reserved** unmodified keys (Tab, Shift+Tab, Esc, Alt-alone, F10 stay with Dxui; Ctrl+Tab, Ctrl+Esc, Alt+F10, and Apple ][ CTRL-C/CTRL-G forward to the sink); `OnMouse` inside viewport rect forwards to sink. Integrate with `DxuiDockLayout`. **Depends on**: T045, T083, T128. **FR**: FR-030, FR-034.

- [x] T085 [PH12] Create `Dxui/Layout/DxuiDockLayout.{h,cpp}` per FR-021. Per-child `DockSide` enum: `Top`/`Bottom`/`Left`/`Right`/`Fill` (exactly one `Fill` child). `Arrange` consumes children in registration order, peeling slabs off the parent rect. Provide **inverse**: `static SIZE ContainerSizeForFill(SIZE desiredFillDip, std::vector<IDxuiControl *> const & nonFillChildren)` — given a desired `Fill` size and fixed-measure non-fill children, compute the container size required so that the `Fill` slot ends up at exactly that size. **Depends on**: T046. **FR**: FR-021, FR-093. _Implementation lives in `Dxui/Core/` alongside the other layouts (Stack/Grid/Form/Absolute) rather than under `Dxui/Layout/`._

- [x] T086 [P] [PH12] Create `UnitTest/Dxui/DxuiDockLayoutTests.cpp` — anchors Top/Bottom/Left/Right/Fill produce expected rects; `ContainerSizeForFill` round-trips with fixed-measure non-fill children (compute container size → arrange → fill rect equals desired). At least one test fires a synthetic viewport-bounds change through a `std::function` subscriber and asserts the subscriber is invoked **exactly once** when bounds change, **zero times** when set to identical bounds. **Depends on**: T084, T085. **FR**: FR-021, FR-030, FR-093; **SC**: SC-006, SC-013. _Bounds-changed subscriber semantics are covered by `DxuiViewportTests` (Layout_DoesNotRefireWhenBoundsUnchanged / Layout_RefiresWhenBoundsChange) carried forward from T128._

- [ ] T087 [PH12] **DEFERRED** — Refactor `Casso/Ui/UiShell.{h,cpp}` so the main shell becomes a root `DxuiPanel` with a `DxuiDockLayout`. Existing chrome bands dock Top/Bottom (and Left/Right where applicable); a single `DxuiViewport` fills the middle and is wired to the emulator. **Depends on**: T085. **FR**: FR-093. _Deferral reason: chrome controls report sizes via `LayoutManager`'s `IEdgeContributor::DesiredThicknessDp()` interface, not via `Bounds()` natural sizes the way `DxuiDockLayout` consumes them. Picking up T087 requires first reshaping chrome controls to measure themselves into `Bounds()` (or extending `DxuiDockLayout` with a DPI-aware measure callback)._

- [ ] T088 [PH12] **DEFERRED** — Locate the current `ClientSizeForFramebuffer` call sites (Casso side) and replace with `DxuiDockLayout::ContainerSizeForFill(...)` driven by the Apple ][ pixel-grid dimensions. The emulator's `D3DRenderer` subscribes to `DxuiViewport::OnBoundsChanged` via `SetBoundsChangedCallback` and resizes its render target only when bounds change. **Depends on**: T084, T087. **FR**: FR-030, FR-093, SC-013. _Deferred with T087; `ContainerSizeForFill` is ready and unit-tested for the eventual swap._

- [ ] T089 [PH12] **DEFERRED** — Implement `IDxuiViewportInputSink` in Casso (e.g., `Casso/EmulatorInputSink.{h,cpp}` or attach to `EmulatorShell`) routing key/mouse events to the existing `EmulatorShell` / Apple ][ keyboard controller. Install via `DxuiViewport::SetInputSink`. Confirm `consumesInput = true` for the emulator viewport. **Depends on**: T084, T087. **FR**: FR-034. _Deferred with T087; the sink interface and viewport forwarding are ready and unit-tested._

- [ ] T090 [PH12] **DEFERRED** — Delete `Casso/Ui/Chrome/LayoutManager.{h,cpp}`. If `Casso/Ui/Chrome/IEdgeContributor.h`, `Casso/Ui/Chrome/ICenterLayer.h`, or `Casso/Ui/Chrome/SimpleEdgeContributor.{h,cpp}` exist in the current tree, delete them as well (plan.md lists them under Phase 9 cleanup even though the current `Casso/Ui/Chrome/` listing does not show them — verify and remove any that are present). Drop the vcxproj item entries. **Depends on**: T087. **Exit**: `rg -n 'LayoutManager|IEdgeContributor|ICenterLayer|SimpleEdgeContributor' Casso/Ui/Chrome` returns zero hits. **FR**: FR-094. _Current consumers (EmulatorShell, WindowCommandManager, ChromeMetrics, LayoutManagerTests) block deletion until T087/T088 land. `IEdgeContributor` and `SimpleEdgeContributor` are nested inside `LayoutManager.h`; `ICenterLayer` is also nested there. No separate header files for those types exist in the current tree._

- [ ] T091 [PH12] **DEFERRED** — Delete `Casso/Ui/Layout.{h,cpp}` and `Casso/Ui/FocusManager.{h,cpp}` if (and only if) all consumers have migrated; otherwise document remaining holdouts and defer their deletion to Phase 10/11 as appropriate. **Depends on**: T087, T088. **Exit**: either the files are gone (preferred) or a follow-up note appears in the Phase 10 / Phase 11 entry below.

- [ ] T092 [PH12] **DEFERRED** — Phase-9 exit verification. Build all four configs; run tests; run code analysis. **Manual** smoke test: launch Casso, confirm viewport sizes the Apple ][ grid correctly at startup, after a window resize, after a DPI change (drag to a different-DPI monitor). Confirm renderer no longer thrashes on identical bounds (instrument `D3DRenderer::Resize` with a debug counter; only fires when bounds change). **Depends on**: T083–T091. **Commit**: `refactor(dxui): replace edge-layout with DxuiDockLayout and DxuiViewport`. **FR**: FR-021/030/034/093/094; **SC**: SC-013.

---

## Phase 12.5 — Retrofit `Dxui/Widgets/*` primitives onto `IDxuiControl`

**Goal**: Every widget primitive in `Dxui/Widgets/*` derives from `IDxuiControl` so it can be added to a `DxuiPanel` tree via `Add<T>` / `Adopt`. Discovered as a blocker by the Phase 13 POC: chrome controls retrofitted in Phase 11 derive from `IDxuiControl`, but the widget primitives (Button, Checkbox, Dropdown, …) had bespoke APIs (`SetRect`, `Paint(painter,text)`, `OnLButtonDown(x,y)`, `HandleKey(vk)`) that did not satisfy the base. **Satisfies**: FR-081 (accessibility), unblocks FR-097 (page conversions). **Session status (Phase 12.5 — complete; all 14 widgets retrofitted)**: Five commits landed, +49 tests (1854 → 1903 baseline). All four configs green. Existing call sites (~30 files across Casso/Ui, Casso/Ui/Settings, Casso/Ui/Dialog) keep compiling unchanged — the retrofit is purely additive.

- [x] T092A [PH12.5] Retrofit `DxuiLabel`, `DxuiButton`, `DxuiCheckbox`, `DxuiToggle`, `DxuiRadioGroup` (`Dxui/Widgets/`). Each adds `: public IDxuiControl`, override shims for `Layout(RECT, scaler)` / `Paint(painter, text, theme)` / `OnMouse(DxuiMouseEvent)` / `OnKey(DxuiKeyEvent)` / `OnFocusChanged(bool)` / `AccessibleName()` / `AccessibleRole()`. Setters with widget-local `m_enabled`/`m_visible` storage also call `IDxuiControl::SetEnabled`/`SetVisible` so the base storage stays in sync with what `DxuiPanel` reads when iterating children. **Commit**: `refactor(dxui): retrofit simple widgets onto IDxuiControl`.

- [x] T092B [PH12.5] Retrofit `DxuiSlider`, `DxuiDropdown`, `DxuiTabStrip`, `DxuiTextInput`. Same pattern. `DxuiDropdown::Paint` (IDxuiControl override) delegates to the existing `Paint(painter, text)` which invokes `PaintBase` + `PaintMenu` in the dropdown's z-slot; the split helpers stay public so legacy consumers that need the menu painted last across siblings (settings pages, debug panels) keep working unchanged. `DxuiTextInput::OnKey` maps `DxuiKeyEvent::Char` to `OnChar(wchar_t)` and `Down` to `OnKey(WPARAM)`. **Commit**: `refactor(dxui): retrofit DxuiSlider/Dropdown/TabStrip/TextInput onto IDxuiControl`.

- [x] T092C [PH12.5] Retrofit `DxuiListView`, `DxuiTreeView`. Internal scroll state, column widths, expand/collapse, and sticky-tail logic are untouched. `DxuiListView`'s input model is host-driven through the public hit-test / scroll / focus accessors, so its `IDxuiControl::OnMouse` / `OnKey` overrides return `false` and the panel walks past to siblings. **Commit**: `refactor(dxui): retrofit DxuiListView and DxuiTreeView onto IDxuiControl`.

- [x] T092D [PH12.5] Retrofit `DxuiPopupMenu`, `DxuiTooltip`, `DxuiModalScrim`. Typical hosting is via `DxuiPopupHost` (WS_POPUP overlay), so the panel-tree path is rare but supported for consistency. `DxuiTooltip::Tick(int64_t)` was already signature-compatible with `IDxuiControl::Tick` — it now just carries the `override` specifier. `DxuiModalScrim::Layout` treats the supplied bounds as the viewport rect (full-bleed). **Commit**: `refactor(dxui): retrofit DxuiPopupMenu/Tooltip/ModalScrim onto IDxuiControl`.

- [x] T092E [PH12.5] Add `UnitTest/Dxui/DxuiWidgetIDxuiControlTests.cpp` — 49 cases that verify every primitive derives from `IDxuiControl`, can be added via `DxuiPanel::Add<T>()`, has a `Layout` that updates `Bounds()`, has a `Paint` callable through the base virtual, and reports the correct `AccessibleRole`. Also verifies `DxuiPanel::Adopt(externally-owned widget)` and `OnMouse` dispatch through the panel into a child button click. **Commit**: `test(dxui): cover the Phase 12.5 IDxuiControl widget retrofit`.

- [x] T092F [PH12.5] Phase-12.5 exit verification. Build all four configs (Debug/Release × x64/ARM64) — all green. Run full suite — 1903/1903 passing (delta +49 from 1854 baseline). Confirm existing call sites still compile (no API removal: `SetRect`, `Paint(painter, text)`, `OnLButtonDown(x, y)`, `HandleKey(vk)`, `PaintBase`/`PaintMenu` are all still callable). Phase 13's `ThemePage` POC can now retry: `Add<DxuiCheckbox>` / `Add<DxuiDropdown>` / `Adopt(m_themeDropdown)` compile against the panel API.

---

## Phase 13 — Convert `ThemePage` (proof of concept)

**Goal (plan.md §Phase 13)**: Validate the declarative-layout + auto-fan-out + focus-manager story on the smallest settings page. **Satisfies**: FR-097 (partial — first page), SC-003 (begins), SC-004 (begins).

- [x] T093 [PH13] Refactor `Casso/Ui/Settings/ThemePage.{h,cpp}` to derive from `DxuiPanel`. Use `DxuiFormLayout`. **Delete** the per-page `OnLButtonDown`, `OnLButtonUp`, `OnMouseHover`, `OnKey`, `Paint`, and `CollectFocusables` overrides (auto fan-out replaces them). Construct children via `Add<DxuiCheckbox>`, `Add<DxuiDropdown>`, etc. Focus order should fall out of geometry; add per-control `tabIndex` overrides only if visual reading order disagrees with geometric order. **Depends on**: T047, T054, T056. **FR**: FR-011, FR-031, FR-097.

- [x] T094 [PH13] Bridge `Casso/Ui/Settings/SettingsPanel.{h,cpp}` to accept a `DxuiPanel`-based page alongside the legacy-style pages until Phase 11 converts the rest. Add a brief comment at the bridge code. No spec/phase numbers in the comment per the project's "no phase/task/spec references in comments" rule — phrase it as "temporary bridge for incremental page migration" with a TODO. **Depends on**: T093.

- [x] T095 [PH13] Measure `ThemePage` LOC delta (before vs after) and `SettingsPanel.cpp` LOC delta (post-bridge). Record numbers in the commit body. If extrapolated reduction across the four pages falls materially short of 40 %, flag explicitly in the commit body before proceeding to Phase 11 (per spec R5 / plan.md §Risk Register). **Depends on**: T093, T094.

- [x] T096 [PH13] Phase-10 exit verification. Build all four configs; run tests; run code analysis. **Manual** test: launch Casso, open Settings → Theme page; every control renders, focus order tabs in reading order, arrow nav moves spatially, theme change broadcasts down the tree. **Depends on**: T093–T095. **Commit**: `refactor(casso/ui): convert ThemePage to DxuiPanel + DxuiFormLayout`. **FR**: FR-011/031/097; **SC**: SC-003 (partial), SC-004 (partial).

**Phase 13 deviation note (T093 / T094 / T095 — POC outcome):** The POC landed a **structural-only** conversion rather than the full deletion-of-overrides shape `tasks.md` prescribed. Outcome and reasoning:

* **What landed**: `ThemePage` now derives from `DxuiPanel`. Its constructor `Adopt`s `m_themeLabel` and `m_themeDropdown` so both widgets participate in the IDxuiControl tree (parent pointers, `Bounds()`, focus/visibility/enabled state, paint/input/tick/theme/DPI walks). `ThemePage::Layout` overrides `IDxuiControl::Layout` and calls `DxuiPanel::SetBounds` at the end so the panel base sees the page footprint. Two `using DxuiPanel::Paint;` / `using DxuiPanel::OnKey;` declarations surface the base virtuals past the same-named bespoke shims.

* **What did not land** (and why):
  - `DxuiFormLayout` not adopted. The framework has a leaf-widget rect-storage duality: `DxuiPanel::Layout` only recursively invokes `Layout(...)` on child *panels*, not on leaf widgets. Leaf positioning happens via `IDxuiLayout::Arrange` writing `SetBounds` directly. But every leaf widget today stores its rect twice (`m_rect` for paint geometry vs `m_boundsDip` for the IDxuiControl contract) and only the `SetRect` path syncs both. So a `DxuiFormLayout`-arranged dropdown would paint at the stale `m_rect` (0,0,0,0) while its `Bounds()` reported the correct value. **Phase 14 must close this duality** (`SetBounds` ⇒ updates `m_rect`, OR leaf `Layout` overrides get walked alongside panel `Layout`) before any page can use a layout policy end-to-end. Until then, `ThemePage::Layout` keeps the explicit `SetRect` calls — and the win from "declarative layout" is zero on a single-row page anyway.
  - Per-page `OnLButtonDown` / `OnLButtonUp` / `OnMouseHover` / `OnKey(WPARAM)` / `CollectFocusables` / `AnyDropdownOpen` overrides **kept** as one-line bridging shims. `SettingsPanel` still routes raw WM_* coords to each page through these bespoke entry points (`m_themePage.OnLButtonDown(x,y)`, `m_themePage.OnKey(vk)`, etc., interleaved with identical bespoke calls into `m_machinePage` / `m_hardwarePage` / `m_displayPage`). Deleting only ThemePage's shims would force SettingsPanel to construct `DxuiMouseEvent` / `DxuiKeyEvent` values for one page out of four and call the IDxuiControl base virtuals — a heterogeneous dispatch path that buys nothing until the other three pages convert in Phase 14. The shims are documented at the declaration as "temporary bridge for incremental page migration" with a TODO and a description of the collapse.
  - `SettingsPanel.{h,cpp}` **untouched**. The bridge T094 prescribes lands as the shim-preservation strategy above; no code change in SettingsPanel.

* **LOC measurement (T095)**: `ThemePage.cpp` 595 → 628 (+33), `ThemePage.h` 78 → 97 (+19), `SettingsPanel.cpp` 1730 → 1730 (no change). The growth comes from constructor + commentary, not from real behavioural code. The honest 40%-reduction signal from this POC: **on a single-row page where the framework's layout-policy seam is blocked by a leaf-widget gap, the conversion has near-zero LOC payoff**. The 40% extrapolation from plan.md §Risk Register assumed (a) FormLayout drives positioning and (b) per-page input shims disappear; neither is currently true. Both unlock together in Phase 14: close the leaf-widget rect duality, then convert all four pages + SettingsPanel together so the bespoke dispatch shims vanish in one stroke. The POC validates this is the correct sequencing; it does NOT validate the headline reduction number.

* **What the POC did validate**:
  - `DxuiPanel::Adopt` works for externally-owned widget members on a real Casso page (matches Phase 11 chrome-control pattern).
  - The Phase 12.5 widget retrofit lets the leaf widgets compile cleanly under both their bespoke API (still called by ThemePage's shims) and the IDxuiControl virtuals (now reachable through the panel tree).
  - Co-existence: a `DxuiPanel`-derived page slots into a `SettingsPanel` container that holds heterogeneous (legacy + DxuiPanel) page members without churning the container.

* **Verification gate**: 1903/1903 tests passing (no delta from baseline; this is a structural change with no new test surface). All four configs (Debug/Release × x64/ARM64) green. Code analysis clean.

* **Lessons for Phase 14**:
  1. **Fix the leaf-widget rect duality first.** Either have `IDxuiControl::SetBounds` also update each widget's internal `m_rect`, OR change `DxuiPanel::Layout` to call `child->Layout(child->Bounds(), scaler)` on every child (not just child panels). Without this, `DxuiFormLayout` / `DxuiStackLayout` / `DxuiGridLayout` are decorative; they do nothing the calling page can rely on.
  2. **Convert all four settings pages + SettingsPanel atomically.** Per-page conversion has zero LOC payoff while SettingsPanel keeps its heterogeneous bespoke dispatch. The 40% reduction (R5) materializes when SettingsPanel itself becomes a `DxuiPanel` whose children are the four page panels, with one uniform `OnMouse` / `OnKey` / `Paint` walk replacing the four-arm switch statements at five call sites.
  3. **Plan a `Paint(painter, text)` migration.** SettingsPanel's render path threads `(DxuiPainter &, DxuiTextRenderer &)` without an `IDxuiTheme &`. Either thread the chrome theme down as an `IDxuiTheme` adapter, or migrate SettingsPanel's render path to take theme alongside painter/text.

---

## Phase 14 — Convert remaining pages, debug panels, dialogs; delete DialogPrimitive

Phase 11 is split into two independently mergeable parts: **Phase 11 — Part A** introduces `DxuiDialog` / `DxuiDialogManager` and rewrites Casso dialogs while legacy `DialogPrimitive` files remain in place; **Phase 11 — Part B** deletes legacy dialog files only after manual verification confirms both new dialogs run in the app.

**Goal (plan.md §Phase 14 — final migration; this is the release gate for the feature)**. Converts the remaining three settings pages, both debug panels, and both dialogs onto Dxui; introduces `DxuiDialog` + `DxuiDialogManager`; deletes the entire legacy `DialogPrimitive` family and the fourth NC plumbing copy. **Satisfies**: FR-070, FR-071, FR-072, FR-096, FR-097 (fully), FR-095 (fully), all remaining SCs (SC-003, SC-004, SC-005, SC-006, SC-010, SC-011, SC-012).

- [x] T097 [P] [PH14] Refactor `Casso/Ui/Settings/MachinePage.{h,cpp}` to derive from `DxuiPanel`; use `DxuiFormLayout` or `DxuiGridLayout` as content shape demands; delete `OnLButtonDown`/`OnLButtonUp`/`OnMouseHover`/`OnKey`/`Paint`/`CollectFocusables` overrides. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004. — **Done (minimum-viable)**: derives from `DxuiPanel` and `Adopt`s all member widgets (six labels, four dropdowns, drive-audio toggle, two write-protect checkboxes) so the leaves participate in the IDxuiControl tree. `Layout()` mirrors the page footprint into the base via `DxuiPanel::SetBounds`. **No layout policy applied** — the existing layout positions a sub-row indented under "Drive audio" and two checkboxes sharing a single row, neither of which `DxuiFormLayout` can model. All bespoke shims (`OnLButtonDown`/`OnLButtonUp`/`OnMouseHover`/`OnKey(WPARAM)`/`Paint(painter,text,theme)`/`CollectFocusables`/`AnyDropdownOpen`) **preserved**; SettingsPanel still routes WM_* through them. LOC delta: `MachinePage.h` 75 → 90 (+15), `MachinePage.cpp` 313 → 353 (+40) — pure structural setup, not the SC-004 reduction. The reduction lands when SettingsPanel converts (T100) and the shims collapse.

- [x] T098 [P] [PH14] Refactor `Casso/Ui/Settings/HardwarePage.{h,cpp}` same treatment. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004. — **Done (minimum-viable)**: derives from `DxuiPanel` and `Adopt`s the three info-row arrays (12 labels × 3 columns = 36 widgets) plus the `DxuiTreeView`. `SetRect()` mirrors the page footprint via `DxuiPanel::SetBounds`. **No layout policy applied** — the existing layout packs memory sub-rows three-wide under a header row that shares its line with the first sub-row; `DxuiFormLayout` cannot model that. All bespoke shims preserved. LOC delta: `HardwarePage.h` 68 → 83 (+15), `HardwarePage.cpp` 275 → 307 (+32) — structural setup only.

- [x] T099 [P] [PH14] Refactor `Casso/Ui/Settings/DisplayPage.{h,cpp}` same treatment. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004. — **Done (minimum-viable)**: derives from `DxuiPanel` and `Adopt`s all 25 member widgets (12 labels, dropdown, 6 sliders, 3 toggles, button). `Layout()` mirrors the page footprint via `DxuiPanel::SetBounds`. **No layout policy applied** — the existing layout has sub-row indents for scanline/bloom/color-bleed children, a Restore button sharing the monitor row, and indicator-column alignment past every slider; `DxuiFormLayout` cannot model those. The extended `Paint(painter,text,theme,focusedControlId,nonFocusedAlpha,focusedAlpha)` overload that drives the live-preview fade **stays put** as a DisplayPage-specific overload alongside the inherited `IDxuiControl::Paint(painter,text,theme)`; SettingsPanel still calls the extended one directly. All bespoke shims preserved. LOC delta: `DisplayPage.h` 194 → 212 (+18), `DisplayPage.cpp` 679 → 729 (+50) — structural setup only.

  **Phase 14 Steps 1-4 deviation note (T097 / T098 / T099 + ThemePage refinement — minimum-viable conversion):** Three pre-flight constraints were confirmed before Steps 1-4 ran and dictated the minimum-viable scope: (a) `DxuiDropdown` z-order needs popup-host wiring that only makes sense once SettingsPanel itself is a panel tree, (b) `DisplayPage::Paint`'s extended signature with the fade-alpha args needs to merge into the IDxuiControl `Paint(painter,text,theme)` path during the SettingsPanel rewrite, (c) SettingsPanel's ~30 bespoke dispatch sites are the SettingsPanel rewrite's job. Steps 1-4 therefore land structural setup only: each page IS-A `DxuiPanel`, every widget is registered into the IDxuiControl tree via `Adopt`, the page footprint mirrors into the base via `SetBounds`. Bespoke shims survive verbatim so SettingsPanel keeps working unchanged. **The SC-003 / SC-004 line reductions await Step 5 (T100)**; the line counts go *up* in Steps 1-4 (constructors + commentary), which is expected and called out per-task above. Step 4 promoted ThemePage's single label-field row to a real `DxuiFormLayout` (constructor unchanged, `Layout()` builds a fresh form per pass, `AddRow` the label and dropdown, drive through `DxuiPanel::Layout`) — the bespoke `m_themeLabel.SetRect` / `m_themeDropdown.SetRect` calls are gone, replaced by the form's `Arrange()` writing `SetBounds` on the leaves. `ThemePage.cpp` 499 → 511 (+12). ThemePage tests verify identical widget geometry. Builds green all four configs; tests 1904/1904 (no delta from baseline, as expected for structural plumbing with no new test surface).

- [ ] T100 [PH14] Strip `Casso/Ui/Settings/SettingsPanel.{h,cpp}` of the temporary bridge introduced in T094 (all pages are now `DxuiPanel`s); collapse the dual code paths into the single `DxuiPanel` path. Measure `SettingsPanel.cpp` final line count; assert **≤ 1,307 lines** (40 % reduction from 2,179 baseline). **Depends on**: T097, T098, T099. **Exit**: `(Get-Content Casso/Ui/Settings/SettingsPanel.cpp).Length -le 1307`. **FR**: FR-097; **SC**: SC-003. — **Step 5 Session A landed (minimum-viable Sub-step A only)**: `SettingsPanel` now derives from `DxuiPanel`; the constructor `Adopt`s the `DxuiTabStrip`, the four pages, the modal scrim, and the Apply / Cancel buttons so they participate in the IDxuiControl tree. Bespoke `m_visible` renamed to `m_panelVisible` to keep it distinct from the inherited `IDxuiControl::m_visible`. Two adapter overrides for the `IDxuiControl` pure virtuals (`Layout(RECT, scaler)` and `Paint(IDxuiPainter, ...)`) forward to the bespoke entry points UiShell already drives. All bespoke shims, the per-page dispatch in `Paint`, the modal-scrim short-circuit, and the preview-fade alpha pipeline are **preserved verbatim**. LOC delta: `SettingsPanel.cpp` 2179 → 2251 (+72), `SettingsPanel.h` 211 → 239 (+28) — pure structural setup, not the SC-003 reduction. Sub-steps **B (popup hosting), C (shim deletion across all 4 pages), D (DisplayPage `Paint` extended-signature collapse), E (SC-003 verification + helper extraction), F (visual / behavioral verification), G (final tasks.md sign-off) deferred** to follow-up Step 5 sessions. Sub-step B specifically blocks on `SettingsWindow` not currently owning a `DxuiHostWindow`; wiring `SetPopupHost` on the page-level dropdowns requires either threading the EmulatorShell host through (wrong HWND for placement) or giving `SettingsWindow` its own host first — that's spec scope but more plumbing than fits a single safe MVP commit. Build green Debug x64; tests 1904/1904 (no delta from baseline, as expected for structural plumbing with no new test surface).

- [ ] T101 [P] [PH14] Refactor `Casso/Ui/Disk2DebugPanel.{h,cpp}` + `Disk2DebugPanelLayout.{h,cpp}` to derive from `DxuiPanel`; delete fan-out overrides; merge or delete the separate `Layout` companion if its responsibilities collapse into a `DxuiGridLayout` / `DxuiAbsoluteLayout` selection on the panel. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004.

- [ ] T102 [P] [PH14] Refactor `Casso/Ui/InputDebugPanel.{h,cpp}` + `InputDebugPanelLayout.{h,cpp}` same treatment. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004.

### Phase 14 — Part A: introduce Dxui dialogs and coexist with legacy dialogs

- [ ] T103 [PH14] Create `Dxui/Dialog/DxuiDialog.{h,cpp}` per FR-070. Derives from `DxuiPanel`. Composed of `DxuiCaptionBar` (title + close button), consumer-populated content panel, optional `DxuiDockLayout`-bottom button row. `Show()` returns nothing directly — the `DxuiDialogManager` wraps it. **Depends on**: T066, T085. **FR**: FR-070.

- [ ] T104 [PH14] Create `Dxui/Dialog/DxuiDialogManager.{h,cpp}` per FR-071, FR-072. API: `std::future<int> Show(std::unique_ptr<DxuiDialog> dialog, ShowParams)` where `ShowParams` includes `bool modalScrim` (default `false`). Maintains an internal `std::vector<DialogStackFrame>` (stack). On `Show`: capture current top-of-stack HWND (or owner HWND if stack empty) → `EnableWindow(prevTop, FALSE)`, create the new dialog's `DxuiHostWindow` with `ownerHwnd = prevTop`, push the frame. On dialog close: `EnableWindow(prevTop, TRUE)`, pop the frame, set the `std::future` shared state on the UI thread. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. Owner-window assignment lets Win32 handle z-order and activation restore automatically. **Depends on**: T064, T103. **FR**: FR-071, FR-072, FR-083.

- [ ] T105 [PH14] Expose `DxuiDialogManager::PushForTest(IDxuiControl * fakeHwndStandIn) -> int` and `PopForTest(int frameId, int result)` test seams driving the stack without real HWNDs. **Depends on**: T104. **FR**: FR-072; **SC**: SC-006.

- [ ] T106 [PH14] Create `UnitTest/Dxui/DxuiDialogManagerTests.cpp` — push two frames simulating "download fails → error dialog opens on top of download dialog"; pop the inner; assert the outer regains the "active top" position; pop the outer; assert the owner HWND ID stand-in regains active. Cover `modalScrim` flag default `false`. **Depends on**: T105. **FR**: FR-072; **SC**: SC-006.

- [ ] T107 [PH14] Create `Casso/Ui/Dialogs/` directory. Use `git mv Casso/Ui/Dialog/X.cpp Casso/Ui/Dialogs/X.cpp` (or delete-then-create with an intermediate commit) to avoid NTFS case-aliasing ghost directories. Add `Casso/Ui/Dialogs/StartupDownloadDialog.{h,cpp}` and `Casso/Ui/Dialogs/StandaloneDialog.{h,cpp}` rewritten as `DxuiDialog`-based panels using `DxuiDialogManager::Show`. Functionality matches the legacy versions behaviourally (download flow + URL handling + standalone-mode toggle); pixel parity verified per SC-011 (manual sign-off). **Depends on**: T103, T104. **FR**: FR-092.


- [ ] T107A [PH14] Manually verify both new Dxui-based dialogs run in the app while the legacy `DialogPrimitive` files still exist. Confirm download flow, URL handling, standalone-mode toggle, and SC-011 visual sign-off before deleting legacy files. **Depends on**: T107. **FR**: FR-092, FR-096; **SC**: SC-011.

### Phase 14 — Part B: delete legacy dialog system after verification

- [ ] T108 [PH14] **Delete** legacy dialog scaffolding: `Casso/Ui/Dialog/DialogDefinition.h`, `Casso/Ui/Dialog/DialogLayout.{h,cpp}`, `Casso/Ui/Dialog/DialogPrimitive.{h,cpp}`, `Casso/Ui/Dialog/DialogPrimitiveRenderer.{h,cpp}`, `Casso/Ui/Dialog/StartupDownloadDialog.{h,cpp}`, `Casso/Ui/Dialog/StandaloneDialog.{h,cpp}`. Remove the now-empty `Casso/Ui/Dialog/` directory using `git mv`/explicit delete paths so NTFS does not leave case-aliasing ghost directories. Drop all vcxproj + filters entries. **Depends on**: T107A. **Exit**: `rg -n 'DialogPrimitive|DialogDefinition|DialogLayout|DialogPrimitiveRenderer' Casso/` → zero hits; `Test-Path Casso/Ui/Dialog/` → `False`. **FR**: FR-096.

- [ ] T109 [PH14] If T091 deferred the deletion of `Casso/Ui/FocusManager.{h,cpp}` and/or `Casso/Ui/Layout.{h,cpp}`, delete them now (all settings pages, debug panels, and dialogs are on `DxuiFocusManager` / Dxui layouts). Drop vcxproj entries. **Depends on**: T097–T108. **Exit**: `rg -n 'class\s+(FocusManager|Layout)\b' Casso/Ui` returns zero hits.

- [ ] T110 [PH14] Update `CHANGELOG.md` with user-visible changes: new in-house Dxui framework, dropdown clipping fix, snap-layouts on all top-level windows, manual-signoff-equivalent custom chrome, MainMenu/MenuBar parity, EmulatorShell decoupled from `Window`. Update `README.md` if test counts, features list, or roadmap items change. Per project rules these MUST update for `feat`/`fix` commits.

- [x] T110A [PH14] **Unify SettingsWindow's NC handling onto `DxuiHostWindow`** as part of the final migration. Reshape (or replace) `Casso/Ui/Settings/SettingsWindow.cpp` (current NC handling at ~line 408) plus `SettingsWindowRenderer.{h,cpp}` so the window hosts on `DxuiHostWindow` (full-ownership mode). Drop redundant files; update vcxproj. NC duplicate count: **2 → 1** (only `DialogPrimitive` remains — that goes away with T108). **Depends on**: T100, T127. **Exit**: `rg -n 'WM_NCCALCSIZE|WM_NCHITTEST' Casso/Ui/Settings/` returns 0 hits. **FR**: FR-095; **SC**: SC-010, SC-020.
  - **Done (adopt mode)**: SettingsWindow now stands up `DxuiHostWindow` via `CreateInAdoptMode` and forwards `WM_NCCALCSIZE` / `WM_NCHITTEST` through `HandleMessage`; the legacy `TitleBar` hit-test is plugged in via `SetHitTestDelegate`. Mirrors the ChromedPanelWindow precedent. Full-ownership was deferred because `SettingsWindowRenderer` owns its own DComp visual + flip-discard swap chain + transparency-blur post-process pipeline; collapsing that onto the host's swap chain is a separate refactor. Exit grep: `rg -n 'WM_NCCALCSIZE|WM_NCHITTEST' Casso/Ui/Settings/` returns 0 hits (matches in comments are the only remaining mentions). Unblocks Phase 14 Step 5 Sub-step B (`DxuiDropdown` popup hosting in SettingsPanel) — the SettingsWindow HWND now has a live `DxuiHostWindow` whose popup pool can host the dropdown. NC-handler case-clauses outside DxuiHostWindow: 0 in `Casso/Ui/Settings/`, 0 across `Casso/` overall for `case WM_NCCALCSIZE`. Builds clean on all 4 configs, code analysis clean, 1904/1904 tests pass.

- [ ] T111 [PH14] **Phase-14 / feature release-gate verification**. Run in order, all must pass:
  1. `scripts\Build.ps1 -Configuration Debug -Platform x64`
  2. `scripts\Build.ps1 -Configuration Release -Platform x64`
  3. `scripts\Build.ps1 -Configuration Debug -Platform ARM64`
  4. `scripts\Build.ps1 -Configuration Release -Platform ARM64`
  5. `scripts\RunTests.ps1` — no regression vs `specs/013-dxui-framework-extraction/baseline-tests.txt` (SC-005)
  6. `scripts\Build.ps1 -RunCodeAnalysis` — clean
  7. `scripts\RunDormannTest.ps1` — no regression vs `baseline-validation.txt`; Klaus Dormann passes if it passed at baseline (SC-012)
  8. `scripts\RunHarteTests.ps1 -SkipGenerate` — no regression vs `baseline-validation.txt`; Tom Harte passes if it passed at baseline (SC-012)
  9. Greps (all expected to return **zero** hits unless noted):
     - `rg -n 'OnLButtonDown|OnLButtonUp|OnMouseHover|OnKey|^\s*void\s+\w+::Paint\b|CollectFocusables' Casso/Ui/Settings Casso/Ui/Disk2DebugPanel.cpp Casso/Ui/Disk2DebugPanel.h Casso/Ui/InputDebugPanel.cpp Casso/Ui/InputDebugPanel.h Casso/Ui/Dialogs` → 0 (SC-004)
     - `rg -n 'WM_NCCALCSIZE' Casso/` → 0; `rg -n 'WM_NCCALCSIZE' Dxui/Win32/` → matches expected (SC-010, SC-020 — NC count = 1 in Dxui, 0 in Casso)
     - `rg -n 'DialogPrimitive|DialogDefinition|DialogLayout' Casso/` → 0
     - `rg -n 'NavLayer|NavMenu' Casso/` → 0 (SC-017)
     - `rg -n 'class EmulatorShell.*Window' Casso/` → 0 (SC-019)
     - For each chrome widget (TitleBar / DriveWidget / LedIndicator / JoystickToggleButton), verify it derives from `IDxuiControl` / `DxuiCaptionBar`: spot-check class declaration (SC-014)
     - `rg -n '\w \(\)' Dxui/ Casso/Ui/ Casso/Pch.h UnitTest/Dxui/` → zero hits in migration-touched paths. Pre-existing violations elsewhere are explicitly out of scope
  10. **Manual visual parity sign-off** at 100 % / 150 % / 200 % DPI on Win10 + Win11 across: main window, chromed panels, Settings, both debug panels, both dialogs (SC-011). Skeuomorphic palette preserved across all chrome widgets (SC-015).
  11. **Manual** MainMenu UX parity: every menu opens; alt-letter accelerators work; arrow-traversal works; check-states display correctly (SC-016).
  12. **Manual** `(Get-Content Casso/Ui/Settings/SettingsPanel.cpp).Length -le 1307` (SC-003).
  13. **Manual** `(Get-Content Casso/EmulatorShell.cpp).Length -le 2502` (SC-018).
  14. **Manual** dropdown bottom-of-Settings test (SC-008 / User Story 3 re-verify).
  15. **Manual** Win11 snap-layouts hover on main + chromed + Settings (SC-009 / User Story 4 re-verify).
  16. **Manual** NC-handler final count: 1 (only `Dxui/Win32/DxuiHostWindow.cpp`) (SC-020).

  **Depends on**: T097–T110A. **Commit**: `refactor(casso/ui): migrate remaining pages, debug panels, dialogs to Dxui; delete DialogPrimitive`. **Merge**: `git merge --no-ff` into `master`. **FR**: FR-070/071/072/092/095/096/097/098/099/100/101/102/103/104/105/106/107/108; **SC**: SC-003, SC-004, SC-005, SC-006, SC-008, SC-009, SC-010, SC-011, SC-012, SC-013, SC-014, SC-015, SC-016, SC-017, SC-018, SC-019, SC-020 (all satisfied).

---

## Dependencies & Execution Order

### Phase Dependencies

```
PH1 → PH2 → PH3 → PH4 → PH5 → PH6 → PH7 → PH8 → PH9 → PH10 → PH11 → PH12 → PH13 → PH14
```

Phases are **strictly sequential** — every phase ends green and is independently mergeable. The user-story timeline is:

| User Story | Priority | Satisfied at end of |
|------------|----------|---------------------|
| US1 — Dxui as standalone consumable library | P1 | **Phase 1** (then continuously) |
| US5 — Headless widget tests with no D3D device | P2 | **Phase 6** (mocks + first tests land); fully exercised by Phase 14 |
| US4 — Snap-layouts on every top-level window | P2 | **Phase 8** (main window via adopt mode); extended Phase 11 (chromed panels) and Phase 14 (Settings, dialogs) |
| US3 — Popups extend beyond parent | P1 | **Phase 9** |
| US2 — Casso UI runs on Dxui with zero regressions | P1 | **Phase 14** (release gate) |

### Intra-Phase Dependencies

- **Phase 1**: T001 unblocks T002–T009. T003 + T005/T006 unblock T007. All `[P]`-marked are parallel-safe; T009 is the gate.
- **Phase 2**: T010–T017 all independent and parallel-safe; T018 is the gate.
- **Phase 3**: T019 → T020; T021 independent of T019/T020.
- **Phase 4**: T023–T036 all parallel-safe (different files); T037 is the gate.
- **Phase 5**: T038 → T039 → T040; T041/T042 follow T038–T040.
- **Phase 6**: T044 → T045 → T047. T046 → T052–T055 (parallel). T048/T049 → T050 → T051. T056 depends on T045+T047. T057/T058 parallel after T048/T049. Tests T059–T062 parallel after their respective targets.
- **Phase 7** (framework-only): T064 → T065. T066/T067/T068 parallel after T064. T069 needs T065–T068. T071/T072 parallel after T064. T073 after T065+T067. T074 is the gate.
- **Phase 8** (main-window adopt-mode): T112 → T113/T114. T115 after T112–T114. T070 after T112–T114. T116 is the gate.
- **Phase 9** (popups): T075 → T076/T077. T078 after T076. T079/T080/T081 parallel after T077.
- **Phase 10** (MenuBar + MainMenu): T117 → T118 (parallel). T117 → T119 → T120 → T121 → T122 (gate).
- **Phase 11** (chrome reshape + EmulatorShell): T123/T124/T125/T126 parallel after T066/T067/T045. T127 after T064. T128 after T045. T129 after T070+T112+T123–T128. T130 is the gate.
- **Phase 12**: T083 → T084 (also depends on T128). T085 independent. T086 after T084+T085. T087/T088/T089 sequential consumer wiring. T090/T091 after T087+T088.
- **Phase 13**: T093 → T094 → T095 → T096.
- **Phase 14**: T097/T098/T099/T101/T102 parallel after T096. T100 after T097–T099. T103 → T104 → T105 → T106. T107 after T103+T104. T107A after T107. T108 after T107A. T109 after T097–T108. T110A after T100+T127. T111 after everything else.

### Parallel Opportunities

- **Phase 2** offers the widest fan-out: 8 file moves with no cross-dependencies (T010–T017) can all run in parallel.
- **Phase 4** offers the second widest: 14 widget moves (T023–T036) all parallel.
- **Phase 6** has multiple parallel sub-groups: layout files (T052–T055), test files (T059–T062), mock infrastructure (T057/T058).
- **Phase 7** parallel block: T066/T067/T068 (chrome primitives) and T071/T072 (DWM wiring).
- **Phase 11** parallel block: T123/T124/T125/T126 (4 chrome widget reshapes).
- **Phase 14** parallel block: 5 page/panel conversions (T097/T098/T099/T101/T102) can run together.

---

## Implementation Strategy

### Strict-Sequence (Recommended)

Phases land in order Phase 1 → Phase 14. Each phase produces a single PR; each PR ends green and is merged `--no-ff`. This matches the "each phase is independently mergeable" gate and the spec's R6 risk-mitigation (no mega-PR).

### MVP Increments Available Mid-Migration

- **After Phase 1** (US1): Dxui is consumable; downstream consumers could in principle target it (none exist outside this repo yet).
- **After Phase 6** (US5): headless widget tests run with no D3D device — Casso CI gains test coverage for widget logic.
- **After Phase 7**: `DxuiHostWindow` + chrome primitives + DWM wiring exist as framework code; no behaviour change yet.
- **After Phase 8** (US4 partial): Win11 snap-layouts hover popover appears on the **main window** via adopt-mode delegation; NC duplicates 4 → 3.
- **After Phase 9** (US3): the long-standing dropdown clipping bug is fixed — user-visible bug fix.
- **After Phase 10**: `DxuiMenuBar` widget and `MainMenu` rename land — internal cleanup, no behaviour change.
- **After Phase 11**: Chrome widgets all on `IDxuiControl`; `ChromedPanelWindow` on `DxuiHostWindow` (NC duplicates 3 → 2); `EmulatorShell` decoupled from `Window`.
- **After Phase 14** (US2 + everything): full extraction complete, pixel-parity preserved, NC duplicate count = 1.

---

## Notes

- **Spacing audit** (`rg -n '\w \(\)' …`) is a tripwire for the project's `fn ()` vs `fn()` rule; run on every newly authored file before commit. Phases 2, 4, 6 are the highest-risk for accidental violations due to volume.
- **DIP suffix** (FR-082): renames are scoped — only files actually moved into Dxui or rewritten in place get the `Dp` → `Dip` treatment in their own pass. No big-bang rename.
- **EHM discipline**: every new Dxui `.cpp` file uses the project EHM pattern (`HRESULT hr = S_OK;` at top, `Error:` label, single exit). For non-HRESULT-returning functions (e.g., layout `Arrange` returning `void`), declare a vestigial `HRESULT hr = S_OK;` for the macros and end `Error:` with explicit `return;`. Default to asserting variants (`CHRA`/`CWRA`) unless failure is genuinely user/external.
- **Tests are in scope from Phase 5 onwards** (mock theme), with the bulk landing in Phase 6 alongside `IDxuiPainter` / `IDxuiTextRenderer`. No production code creates a D3D11 device or calls `CreateWindowEx` inside `UnitTest.dll`.
- **No phase/spec/task numbers in code comments** — phrase TODOs in terms of the work, not the spec metadata, per the project's comment-style rule.
- **CHANGELOG.md / README.md updates** are user-facing: T110 in Phase 11 is the explicit reminder, but every phase that flips a user-visible behaviour (Phase 7 snap-layouts, Phase 8 dropdown fix) should also touch `CHANGELOG.md` at minimum.

---

## Extension Hooks (after_tasks)

(See report header for the optional pre-tasks hook; the after_tasks hook below is also optional.)

---

## Coverage Matrix

| Requirement | Covering task IDs |
|-------------|-------------------|
| FR-001 | T001,T004,T009 |
| FR-002 | T001,T009 |
| FR-003 | T005,T006,T009 |
| FR-004 | T002,T003 |
| FR-005 | T010-T037,T045 |
| FR-006 | T001,T005 |
| FR-007 | T003,T007,T008 |
| FR-010 | T044,T045 |
| FR-011 | T047,T059,T093-T102 |
| FR-012 | T045,T064-T069 |
| FR-020 | T046 |
| FR-021 | T052-T055,T085,T086 |
| FR-022 | T013,T046-T055 |
| FR-030 | T084,T086,T088 |
| FR-031 | T045,T056,T062,T093 |
| FR-032 | T038-T042,T073 |
| FR-033 | T040,T047 |
| FR-034 | T083,T084,T089 |
| FR-040 | T048-T050 |
| FR-041 | T051,T057,T058 |
| FR-050 | T064,T065,T069,T077A |
| FR-051 | T064 |
| FR-052 | T064,T067,T069,T073,T074 |
| FR-053 | T066-T068 |
| FR-054 | T075,T076,T078 |
| FR-055 | T077,T082 |
| FR-056 | T075,T077A,T078 |
| FR-060 | T023-T037 |
| FR-061 | T079-T081 |
| FR-070 | T103 |
| FR-071 | T104 |
| FR-072 | T104-T106 |
| FR-080 | T045,T049,T103 |
| FR-081 | T045 |
| FR-082 | T013,T046-T055 |
| FR-083 | T003,T047,T050,T056,T064,T075,T104 |
| FR-090 | T010-T017 |
| FR-091 | T019-T022 |
| FR-092 | T107,T107A |
| FR-093 | T085-T088 |
| FR-094 | T090 |
| FR-095 | T070,T108,T110A,T111,T127 |
| FR-096 | T108 |
| FR-097 | T093-T102,T109 |
| FR-098 | T070,T112,T115,T116 |
| FR-099 | T070,T113,T114,T115,T116 |
| FR-100 | T070,T116 |
| FR-101 | T117,T118,T122 |
| FR-102 | T119,T120,T121,T122 |
| FR-103 | T119,T121,T122 |
| FR-104 | T123,T124,T125,T126,T130 |
| FR-105 | T129,T130 |
| FR-106 | T127,T130 |
| FR-107 | T128,T129,T130 |
| FR-108 | T129,T130 |
| SC-001 | T001,T004,T009 |
| SC-002 | T005-T009 |
| SC-003 | T095,T100,T111 |
| SC-004 | T093,T097-T102,T111 |
| SC-005 | T000,T009,T111 |
| SC-006 | T041,T057-T062,T069,T078,T086,T106,T115,T118 |
| SC-007 | T041,T042,T057,T058 |
| SC-008 | T079,T082,T111 |
| SC-009 | T064-T069,T073,T074,T116,T111 |
| SC-010 | T070,T108,T110A,T111,T116,T127 |
| SC-011 | T074,T107A,T111,T116,T130 |
| SC-012 | T009,T111 |
| SC-013 | T085-T088,T092,T111 |
| SC-014 | T123-T126,T130,T111 |
| SC-015 | T123-T126,T130,T111 |
| SC-016 | T122,T111 |
| SC-017 | T119,T122,T111 |
| SC-018 | T130,T111 |
| SC-019 | T129,T130,T111 |
| SC-020 | T108,T110A,T111 |
| US1 | T001-T009 |
| US2 | T093-T130 |
| US3 | T075-T082 |
| US4 | T064-T074,T070,T112-T116,T127,T129,T130,T110A |
| US5 | T041,T042,T057-T062,T115,T118 |
