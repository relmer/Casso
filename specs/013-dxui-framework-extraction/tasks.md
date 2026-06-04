---
description: "Task list for Dxui framework extraction (spec 013)"
---

# Tasks: Dxui — Reusable DirectX UI Framework Extracted from Casso

**Input**: Design documents from `/specs/013-dxui-framework-extraction/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/
**Organization**: Tasks are grouped by **migration phase** (1 through 11) as defined in plan.md. Phases land sequentially; each phase ends green (build + tests + code analysis) and is independently mergeable. User stories US1–US5 from spec.md are satisfied incrementally across these phases; story coverage is tracked in the Dependencies section at the bottom of this file.

## Format: `[ID] [P?] [Phase] Description`

- **[P]**: Can run in parallel within its phase (different files, no intra-phase dependencies).
- **[Phase]**: `[PH1]`, `[PH2]`, `[PH3]`, `[PH4]`, `[PH5]`, `[PH6]`, `[PH7]`, `[PH8]`, `[PH9]`, `[PH10]`, `[PH11]`.
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

- [ ] T019 [PH3] Move `Casso/Ui/DxUiPainter.{h,cpp}` → `Dxui/Render/DxuiPainter.{h,cpp}`; rename class `DxUiPainter` → `DxuiPainter` (single-word prefix). Update every Casso consumer (`#include` + type references). **Exit**: `rg -n '\bDxUiPainter\b' Casso/ Dxui/` returns zero hits. **FR**: FR-005, FR-091.

- [ ] T020 [PH3] Move the painter's HLSL source from `Casso/Ui/` (whatever `.hlsl` file ships with `DxUiPainter`) → `Dxui/Render/DxuiPainter.hlsl`. Add the file as an `<FxCompile>` item in `Dxui.vcxproj` with the same entrypoint / target / output settings as the original `Casso.vcxproj` entry; remove the original `Casso.vcxproj` entry. **Depends on**: T019. **Exit**: `Dxui.vcxproj` builds the shader; `Casso.vcxproj` no longer references it; `rg -n '\.hlsl' Casso/Casso.vcxproj` returns zero hits for the painter shader. **FR**: FR-091.

- [ ] T021 [PH3] Move `Casso/Ui/DwriteTextRenderer.{h,cpp}` → `Dxui/Render/DxuiTextRenderer.{h,cpp}`; rename class `DwriteTextRenderer` → `DxuiTextRenderer`. Update every Casso consumer. **Exit**: `rg -n 'DwriteTextRenderer' Casso/ Dxui/` returns zero hits. **FR**: FR-005, FR-091.

- [ ] T022 [PH3] Phase-3 exit verification. Build all four configs; run tests; run code analysis. Greps: `rg -n 'DxUiPainter|DwriteTextRenderer' Casso UnitTest` → zero hits. **Depends on**: T019–T021. **Commit**: `refactor(dxui): rename render facades to DxuiPainter and DxuiTextRenderer`. **FR**: FR-005, FR-091.

---

## Phase 4 — Move widgets

**Goal (plan.md §Phase 4)**: Each `Casso/Ui/Widgets/<Foo>.{h,cpp}` → `Dxui/Widgets/Dxui<Foo>.{h,cpp}` with the `Dxui` prefix on every public type. Widget `Paint` signatures still take concrete `DxuiPainter &` / `DxuiTextRenderer &` / `ChromeTheme &`; the interface flip happens in Phase 5 and Phase 6. **Satisfies**: FR-005, FR-060.

The 13 widget files are independent moves and parallel-safe within this phase. Each task: move files, rename primary type (`<Foo>` → `Dxui<Foo>`), update consumer includes and references throughout `Casso/Ui/`, settings pages, debug panels, dialogs.

- [ ] T023 [P] [PH4] Move `Casso/Ui/Widgets/Button.{h,cpp}` → `Dxui/Widgets/DxuiButton.{h,cpp}`; rename `Button` → `DxuiButton`. **FR**: FR-005, FR-060.

- [ ] T024 [P] [PH4] Move `Casso/Ui/Widgets/Checkbox.{h,cpp}` → `Dxui/Widgets/DxuiCheckbox.{h,cpp}`; rename `Checkbox` → `DxuiCheckbox`. **FR**: FR-005, FR-060.

- [ ] T025 [P] [PH4] Move `Casso/Ui/Widgets/Radio.{h,cpp}` → `Dxui/Widgets/DxuiRadio.{h,cpp}`; rename `Radio` → `DxuiRadio`. **FR**: FR-005, FR-060.

- [ ] T026 [P] [PH4] Move `Casso/Ui/Widgets/Toggle.{h,cpp}` → `Dxui/Widgets/DxuiToggle.{h,cpp}`; rename `Toggle` → `DxuiToggle`. **FR**: FR-005, FR-060.

- [ ] T027 [P] [PH4] Move `Casso/Ui/Widgets/Slider.{h,cpp}` → `Dxui/Widgets/DxuiSlider.{h,cpp}`; rename `Slider` → `DxuiSlider`. **FR**: FR-005, FR-060.

- [ ] T028 [P] [PH4] Move `Casso/Ui/Widgets/Dropdown.{h,cpp}` → `Dxui/Widgets/DxuiDropdown.{h,cpp}`; rename `Dropdown` → `DxuiDropdown`. Existing in-window clipping path is preserved as-is here; popup hosting lands in Phase 8. **FR**: FR-005, FR-060.

- [ ] T029 [P] [PH4] Move `Casso/Ui/Widgets/TabStrip.{h,cpp}` → `Dxui/Widgets/DxuiTabStrip.{h,cpp}`; rename `TabStrip` → `DxuiTabStrip`. **FR**: FR-005, FR-060.

- [ ] T030 [P] [PH4] Move `Casso/Ui/Widgets/TextInput.{h,cpp}` → `Dxui/Widgets/DxuiTextInput.{h,cpp}`; rename `TextInput` → `DxuiTextInput`. **FR**: FR-005, FR-060.

- [ ] T031 [P] [PH4] Move `Casso/Ui/Widgets/Label.h` (header-only) → `Dxui/Widgets/DxuiLabel.h`; rename `Label` → `DxuiLabel`. **FR**: FR-005, FR-060.

- [ ] T032 [P] [PH4] Move `Casso/Ui/Widgets/ListView.{h,cpp}` → `Dxui/Widgets/DxuiListView.{h,cpp}`; rename `ListView` → `DxuiListView`. **FR**: FR-005, FR-060.

- [ ] T033 [P] [PH4] Move `Casso/Ui/Widgets/TreeView.{h,cpp}` → `Dxui/Widgets/DxuiTreeView.{h,cpp}`; rename `TreeView` → `DxuiTreeView`. **FR**: FR-005, FR-060.

- [ ] T034 [P] [PH4] Move `Casso/Ui/Widgets/PopupMenu.{h,cpp}` → `Dxui/Widgets/DxuiPopupMenu.{h,cpp}`; rename `PopupMenu` → `DxuiPopupMenu`. **FR**: FR-005, FR-060.

- [ ] T035 [P] [PH4] Move `Casso/Ui/Widgets/Tooltip.{h,cpp}` → `Dxui/Widgets/DxuiTooltip.{h,cpp}`; rename `Tooltip` → `DxuiTooltip`. **FR**: FR-005, FR-060.

- [ ] T036 [P] [PH4] Move `Casso/Ui/Widgets/ModalScrim.{h,cpp}` → `Dxui/Widgets/DxuiModalScrim.{h,cpp}`; rename `ModalScrim` → `DxuiModalScrim`. **FR**: FR-005, FR-060.

- [ ] T037 [PH4] Phase-4 exit verification. Confirm `Casso/Ui/Widgets/` is empty; remove the now-empty `Casso/Ui/Widgets/` directory and drop its `<Filter>` entries from `Casso.vcxproj.filters`. Build all four configs; run tests; run code analysis. Greps: `rg -n 'class\s+(Button|Checkbox|Radio|Toggle|Slider|Dropdown|TabStrip|TextInput|Label|ListView|TreeView|PopupMenu|Tooltip|ModalScrim)\b' Casso/ Dxui/` returns hits only at the `Dxui`-prefixed names. **Depends on**: T023–T036. **Commit**: `refactor(dxui): move widgets into Dxui/Widgets with Dxui prefix`. **FR**: FR-005, FR-060.

---

## Phase 5 — Introduce `IDxuiTheme`

**Goal (plan.md §Phase 5)**: Decouple widgets from Casso's concrete `ChromeTheme` by routing them through a pure-virtual `IDxuiTheme`. **Satisfies**: FR-032, FR-033 (partial — default-noop accessor lands here; the broadcast machinery arrives in Phase 6).

- [ ] T038 [PH5] Create `Dxui/Theme/IDxuiTheme.h` per `contracts/IDxuiTheme.h.md`. Pure-virtual accessors for background, foreground, accent, focus ring, disabled foreground, caption colours, body / caption font handles, plus any others the contract sketch lists. All accessors `const` and return-by-value or `const &`. No `IDxui` body — interface only. **Exit**: header compiles standalone (only `#include "Pch.h"`). **FR**: FR-032.

- [ ] T039 [PH5] Modify `Casso/Ui/Chrome/ChromeTheme.h` to derive from `IDxuiTheme` and `override` every interface accessor; preserve existing skeuomorphic palette + scanline tint additions on top. **Depends on**: T038. **Exit**: `ChromeTheme` satisfies `IDxuiTheme` at compile time (no abstract leftovers); Casso paint paths still receive `ChromeTheme &` and continue to work via implicit upcast. **FR**: FR-032.

- [ ] T040 [PH5] Re-type every Dxui widget's `Paint(...)` parameter from `ChromeTheme const &` to `IDxuiTheme const &`. Touch every `.h` and `.cpp` under `Dxui/Widgets/`. Call sites in `Casso/` continue to pass `ChromeTheme &` (implicit upcast). **Depends on**: T038, T039. **Exit**: `rg -n 'ChromeTheme' Dxui/Widgets` returns zero hits. **FR**: FR-032.

- [ ] T041 [P] [PH5] Create `UnitTest/Dxui/MockDxuiTheme.{h,cpp}` returning deterministic, canned values for every `IDxuiTheme` accessor. No D3D, no DirectWrite — font handles are stored as opaque `IDWriteTextFormat *` set externally or as `nullptr` with a debug-build assert if accessed. Add to `UnitTest.vcxproj`. **Depends on**: T038. **Exit**: file compiles as part of `UnitTest.dll`. **FR**: FR-032; **SC**: SC-007.

- [ ] T042 [PH5] Add a single smoke test `UnitTest/Dxui/MockDxuiThemeTests.cpp` that constructs `MockDxuiTheme`, constructs a `DxuiButton` with no D3D device, and asserts the button's `AccessibleName` / bounds setter / `SetVisible` behaviour. Paint is **not** exercised yet (no painter mock until Phase 6). **Depends on**: T040, T041. **Exit**: test passes via `scripts\RunTests.ps1`. **FR**: FR-032; **SC**: SC-007 (foundation).

- [ ] T043 [PH5] Phase-5 exit verification. Build all four configs; run tests; run code analysis. **Depends on**: T038–T042. **Commit**: `refactor(dxui): introduce IDxuiTheme; widgets paint against the interface`. **FR**: FR-032, FR-033.

---

## Phase 6 — Add framework (`IDxuiControl`, `DxuiPanel`, layouts, focus, render interfaces)

**Goal (plan.md §Phase 6)**: The heart of the framework lands. After this phase the toolkit is consumable end-to-end, even though Casso's chrome / pages don't use it yet. **Satisfies**: FR-010, FR-011, FR-012, FR-020, FR-021 (Stack/Grid/Form/Absolute only; Dock lands in Phase 9), FR-022, FR-031, FR-033, FR-040, FR-041, FR-080, FR-081. **SC**: SC-006 (partial), SC-007.

- [ ] T044 [PH6] Create `Dxui/Core/DxuiEvents.h` — `DxuiMouseEvent`, `DxuiKeyEvent` POD-ish structs (member init in-class, no constructors). **Exit**: header compiles standalone. **FR**: FR-010.

- [ ] T045 [PH6] Create `Dxui/Core/IDxuiControl.h` per `contracts/IDxuiControl.h.md`. Pure-virtual: `Layout(const RECT & bounds, const DxuiDpiScaler & scaler)`, `Paint(IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &)`, `OnMouse(const DxuiMouseEvent &) -> bool`, `OnKey(const DxuiKeyEvent &) -> bool`, `OnFocusChanged(bool)`, `OnThemeChanged()` (default no-op), `Tick(int64_t nowMs)` (default no-op), `ClassifyHit(POINT) -> DxuiHitTestKind` (default `Client`), `AccessibleName() const -> std::wstring`, `AccessibleRole() const -> DxuiAccessibleRole`. Concrete-on-base: `Bounds()`, `SetBounds(RECT)`, `Visible()`/`SetVisible(bool)` (Collapsed mode only — FR-011), `Enabled()`/`SetEnabled(bool)`, `Focusable()`/`SetFocusable(bool)`, `Parent()`/`SetParent(IDxuiControl *)`, `ChildCount()` (default 0), `Child(size_t)` (default `nullptr`). Declare `enum class DxuiHitTestKind { None, Client, Caption, MinButton, MaxButton, CloseButton, ResizeEdgeLeft, ResizeEdgeRight, ResizeEdgeTop, ResizeEdgeBottom, ResizeCornerTL, ResizeCornerTR, ResizeCornerBL, ResizeCornerBR };` and tab sentinels `kTabIndexGeometry = -1`, `kTabIndexExcluded = -2`. All string params use `std::wstring`. **Depends on**: T044. **Exit**: header compiles standalone. **FR**: FR-010, FR-011, FR-012, FR-031, FR-080, FR-081.

- [ ] T046 [PH6] Create `Dxui/Layout/IDxuiLayout.h` per `contracts/IDxuiLayout.h.md`. Pure-virtual `Arrange(const RECT & bounds, const DxuiDpiScaler & scaler, children_view)`; non-pure `Measure(...)` defaulting to `{0,0}`. All sizes in DIPs. **Exit**: header compiles standalone. **FR**: FR-020, FR-022.

- [ ] T047 [PH6] Create `Dxui/Core/DxuiPanel.{h,cpp}` per FR-011 / `contracts/IDxuiControl.h.md`. Derives from `IDxuiControl`. Owning `std::vector<std::unique_ptr<IDxuiControl>>`. Owns `std::unique_ptr<IDxuiLayout> m_layout`. APIs: `template<class T, class... Args> T & Add(Args &&... args)`, `bool Remove(IDxuiControl *)`, `void Clear()`, `void SetLayout(std::unique_ptr<IDxuiLayout>)`. Override `Layout(const RECT & bounds, const DxuiDpiScaler & scaler)` and call `if (m_layout) { m_layout->Arrange(bounds, scaler, children_view()); }`; override `Paint`, `OnMouse`, `OnKey`, `OnThemeChanged`, and `Tick` with visible-child fan-out. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. Recursive helpers: `void PropagateDpi(float scale)`, `void PropagateTheme()`. `SetVisible(false)` MUST trigger a parent relayout on next pump. Override `ChildCount` / `Child`. **Depends on**: T045, T046. **Exit**: panel compiles; covered by T053. **FR**: FR-011, FR-022, FR-083.

- [ ] T048 [PH6] Create `Dxui/Render/IDxuiPainter.h` per `contracts/IDxuiPainter.h.md`. Pure-virtual HRESULT-returning methods: `BeginFrame`, `EndFrame`, `FillRect`, `StrokeRect`, `FillRounded`, `StrokeRounded`, `FillGradient`, `OutlineRect`, `FillCircleApprox`, `DrawImage`, `PushClip`, and `PopClip`. Exact signature list per the contract sketch; keep the contract shape. **Exit**: header compiles standalone. **FR**: FR-040.

- [ ] T049 [PH6] Create `Dxui/Render/IDxuiTextRenderer.h` per `contracts/IDxuiTextRenderer.h.md`. Pure-virtual HRESULT-returning methods: `Measure(const std::wstring & text, DxuiFontHandle fontSpec, float maxWidthDip, SIZE & outSizeDip)` and `DrawText(...)`. Do **not** add a `Font()` accessor; fonts come from `IDxuiTheme::BodyFont()`, `CaptionFont()`, etc. **Exit**: header compiles standalone. **FR**: FR-040.

- [ ] T050 [PH6] Modify `Dxui/Render/DxuiPainter.{h,cpp}` to derive from `IDxuiPainter`; modify `Dxui/Render/DxuiTextRenderer.{h,cpp}` to derive from `IDxuiTextRenderer`. Existing concrete method bodies unchanged; add `override` to each virtual and invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry on the concrete painter/text renderer. **Depends on**: T048, T049. **Exit**: `Casso.exe` paint paths still render identically. **FR**: FR-040.

- [ ] T051 [PH6] Re-type every Dxui widget's `Paint(...)` parameters from `DxuiPainter &` / `DxuiTextRenderer &` to `IDxuiPainter &` / `IDxuiTextRenderer &`. Touch every `.h` / `.cpp` under `Dxui/Widgets/`. **Depends on**: T048, T049, T050. **Exit**: `rg -n '\bDxuiPainter\s*&' Dxui/Widgets Dxui/Win32` returns zero hits (the interface is used; concretes only appear at the wiring layer). **FR**: FR-041.

- [ ] T052 [P] [PH6] Create `Dxui/Layout/DxuiStackLayout.{h,cpp}` — horizontal/vertical, spacing in DIPs, per-child weight, cross-axis alignment. **Depends on**: T046. **FR**: FR-021, FR-022.

- [ ] T053 [P] [PH6] Create `Dxui/Layout/DxuiGridLayout.{h,cpp}` — fixed rows × cols, per-cell span, per-row/per-col size (fixed-DIP, auto, star). **Depends on**: T046. **FR**: FR-021, FR-022.

- [ ] T054 [P] [PH6] Create `Dxui/Layout/DxuiFormLayout.{h,cpp}` — label : field rows with consistent gutter, indented sub-rows, section gaps. **Depends on**: T046. **FR**: FR-021, FR-022.

- [ ] T055 [P] [PH6] Create `Dxui/Layout/DxuiAbsoluteLayout.{h,cpp}` — uses pre-set child bounds verbatim (escape hatch). **Depends on**: T046. **FR**: FR-021, FR-022.

- [ ] T056 [PH6] Create `Dxui/Core/DxuiFocusManager.{h,cpp}` per FR-031. Attaches to a `DxuiPanel` root; builds tab order by walking tree and sorting focusables on `(top / rowEpsilon, left)`. Skips `!Visible` / `!Enabled` / `!Focusable`. Handles Tab / Shift+Tab / Esc / Enter / Space. Per-control `tabIndex` override hint: `IDxuiControl::kTabIndexGeometry` uses geometry order; `IDxuiControl::kTabIndexExcluded` skips Tab traversal but remains mouse-focusable. Focus scopes: `PushScope(IDxuiControl * scopeRoot)` saves current focus, restricts tab walk to scope's subtree; `PopScope()` restores. Spatial arrow navigation: `MoveFocusInDirection(Up|Down|Left|Right)` picks nearest focusable in direction using `Bounds()` centroids. `RowEpsilonDip()` defaults to `IDxuiTheme::BodyLineHeightDip()`; expose `SetRowEpsilonDip(float)` for test determinism. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. **Depends on**: T045, T047. **Exit**: covered by T062. **FR**: FR-031.

- [ ] T057 [P] [PH6] Create `UnitTest/Dxui/MockDxuiPainter.{h,cpp}` — recording `IDxuiPainter` implementation. Each override appends a `RecordedPaintCall` (struct per plan.md §Testing Strategy) to a `std::vector` exposed via `Calls() const`. POD-friendly value comparisons (no functors, no smart pointers). `Reset()` clears the log. **Zero** D3D device creation. **Depends on**: T048. **FR**: FR-041; **SC**: SC-007.

- [ ] T058 [P] [PH6] Create `UnitTest/Dxui/MockDxuiTextRenderer.{h,cpp}` — recording `IDxuiTextRenderer`. `Measure` returns canned `SIZE` configured via `SetCannedMetrics(std::wstring, SIZE)` with a default fallback (width = `text.size() * 7`, height = `16`). `DrawText` records call. **Depends on**: T049. **FR**: FR-041; **SC**: SC-007.

- [ ] T059 [P] [PH6] Create `UnitTest/Dxui/DxuiPanelTests.cpp` — at minimum: `Add` returns a reference to the constructed child; `Remove(nullptr)` returns false; `Remove(unknown)` returns false; `Remove(known)` returns true and the child is destroyed; `Clear` drops all children; paint fan-out visits visible children in z-order; `OnMouse` dispatches front-to-back; `SetVisible(false)` skips that child in subsequent paint and input; `SetVisible(false)` triggers `m_dirty` (or whatever signal the layout uses). **Depends on**: T047, T057, T058. **FR**: FR-011; **SC**: SC-006.

- [ ] T060 [P] [PH6] Create `UnitTest/Dxui/DxuiStackLayoutTests.cpp` — H/V, spacing, weights distribute remainder, cross-axis alignment. **Depends on**: T052. **FR**: FR-021; **SC**: SC-006.

- [ ] T061 [P] [PH6] Create `UnitTest/Dxui/DxuiGridLayoutTests.cpp`, `UnitTest/Dxui/DxuiFormLayoutTests.cpp`, `UnitTest/Dxui/DxuiAbsoluteLayoutTests.cpp` — each verifies arrange output against hand-computed rects for a small fixture. **Depends on**: T053, T054, T055. **FR**: FR-021; **SC**: SC-006.

- [ ] T062 [PH6] Create `UnitTest/Dxui/DxuiFocusManagerTests.cpp` — reading-order tab across a 3-row × 4-column synthetic grid; row-epsilon collapses near-equal `top` values; spatial arrow nav picks the geometrically nearest target; `tabIndex` override beats geometry; `kTabIndexExcluded` skips Tab traversal while preserving mouse focus; focus scope push/pop restores prior focus; `!Visible` / `!Enabled` / `!Focusable` skipped. **Depends on**: T056. **FR**: FR-031; **SC**: SC-006.

- [ ] T063 [PH6] Phase-6 exit verification. Build all four configs; run tests; run code analysis. Casso continues to use legacy `FocusManager` and `Layout` (their deletion is deferred until Phase 10 / Phase 11 respectively). Confirm: `rg -n '\w \(\)' Dxui/Core Dxui/Layout Dxui/Render Dxui/Widgets` → zero hits in newly authored lines. **Depends on**: T044–T062. **Commit**: `feat(dxui): add IDxuiControl, DxuiPanel, layouts, focus manager, render interfaces`. **FR**: FR-010/011/012/020/021/022/031/033/040/041/080/081; **SC**: SC-006 (partial), SC-007.

---

## Phase 7 — Host window unification

**Goal (plan.md §Phase 7)**: Land `DxuiHostWindow` and migrate three of the four NC plumbing copies (main `Window`, `ChromedPanelWindow`, `SettingsWindow`) onto it. `DialogPrimitive` is the fourth and waits for Phase 11. **Satisfies**: FR-050, FR-051, FR-052, FR-053, FR-095 (partial), and User Story 4 (snap-layouts).

- [ ] T064 [PH7] Create `Dxui/Win32/DxuiHostWindow.{h,cpp}` per `contracts/DxuiHostWindow.h.md`. Owns HWND, DXGI swap chain, root `DxuiPanel`. `CreateParams` covers `borderless`, `resizable`, `rounded`, `dark`, `backdrop`, `resizeBorderDip`. Create the D3D11 device with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`. WndProc handles `WM_NCCALCSIZE` (claim NC as client when borderless), `WM_NCHITTEST` (8 resize edges + tree walk via `ClassifyHit`), `WM_NCLBUTTONDOWN/UP`, `WM_NCMOUSEMOVE`, `WM_NCMOUSELEAVE`, `WM_DPICHANGED` (re-DPI tree + relayout + repaint), `WM_DPICHANGED_BEFOREPARENT` (forward to every active pooled `DxuiPopupHost`), `WM_SETTINGCHANGE`, `WM_THEMECHANGED`, `WM_DWMCOLORIZATIONCOLORCHANGED`. Snap-layouts: return `HTMAXBUTTON` when hit lands on a `DxuiSystemButton` classified `DxuiHitTestKind::MaxButton`. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. **Depends on**: T047. **Exit**: covered by T070. **FR**: FR-050, FR-051, FR-052, FR-083.

- [ ] T065 [PH7] Expose `DxuiHostWindow::ClassifyHitForTest(POINT clientPx) -> DxuiHitTestKind` test seam (public, no real HWND required, accepts a synthetic root panel via a constructor overload). **Depends on**: T064. **FR**: FR-050; **SC**: SC-006.

- [ ] T066 [P] [PH7] Create `Dxui/Win32/DxuiCaptionBar.{h,cpp}` — derives from `DxuiPanel`; default `ClassifyHit` returns `DxuiHitTestKind::Caption` for blank areas; children may override. **Depends on**: T047. **FR**: FR-053.

- [ ] T067 [P] [PH7] Create `Dxui/Win32/DxuiSystemButton.{h,cpp}` — derives from `IDxuiControl`; classification toggles `DxuiHitTestKind::MinButton`/`DxuiHitTestKind::MaxButton`/`DxuiHitTestKind::CloseButton`. Renders Win11-style glyphs via `IDxuiPainter`. Click dispatch: `Min` → `ShowWindow(hwnd, SW_MINIMIZE)`; `Max` → `SendMessage(hwnd, WM_SYSCOMMAND, IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0)`; `Close` → `SendMessage(hwnd, WM_CLOSE, 0, 0)`. **Depends on**: T045. **FR**: FR-053.

- [ ] T068 [P] [PH7] Create `Dxui/Win32/DxuiDragRegion.{h,cpp}` — invisible caption-filler; `ClassifyHit` returns `DxuiHitTestKind::Caption`. **Depends on**: T045. **FR**: FR-053.

- [ ] T069 [PH7] Create `UnitTest/Dxui/DxuiHostWindowTests.cpp` — NC classification only via `ClassifyHitForTest`. Cover: 8 resize edges (NW/N/NE/E/SE/S/SW/W) computed against a synthetic bounds + `resizeBorderDip`; blank caption area returns `HTCAPTION`; system button area returns `HTMAXBUTTON` (snap-layouts), `HTMINBUTTON`, `HTCLOSE`; client area returns `HTCLIENT`. **No real HWND**, **no `CreateWindowEx`**. **Depends on**: T065, T066, T067, T068. **FR**: FR-050, FR-052; **SC**: SC-006.

- [ ] T070 [PH7] Migrate Casso's main top-level `Window` to host on `DxuiHostWindow`: replace the legacy WndProc plumbing; the main window's content panel becomes the host's root `DxuiPanel`. Delete/unify the WM_NCCALCSIZE / WM_NCHITTEST handling code at `Casso\Window.cpp:277` and `Casso\EmulatorShell.cpp:4035`. Existing chrome bands re-host as children of the new root panel. **Depends on**: T064, T066, T067, T068. **Exit**: `rg -n 'WM_NCCALCSIZE|WM_NCHITTEST' Casso/Window.cpp Casso/Window.h` returns zero hits. **FR**: FR-095.

- [ ] T071 [PH7] Migrate `Casso\Ui\Chrome\ChromedPanelWindow.cpp:451` consumers to host on `DxuiHostWindow`. **Delete** `Casso/Ui/Chrome/ChromedPanelWindow.{h,cpp}` and `Casso/Ui/Chrome/IChromedPanelContent.h`. Drop the vcxproj item entries. **Depends on**: T064. **Exit**: `rg -n 'ChromedPanelWindow|IChromedPanelContent' Casso/` returns zero hits. **FR**: FR-095.

- [ ] T072 [PH7] Migrate `Casso\Ui\Settings\SettingsWindow.cpp:408` + `SettingsWindowRenderer.{h,cpp}` to host on `DxuiHostWindow`. **Delete** all four files. Drop the vcxproj item entries. **Depends on**: T064. **Exit**: `rg -n 'SettingsWindow|SettingsWindowRenderer' Casso/` returns zero hits. **FR**: FR-095.

- [ ] T073 [PH7] Modify `Casso/Ui/Chrome/ChromeTheme.h` — if not already done in T039, ensure it derives from `IDxuiTheme` cleanly so the migrated host windows can hand it down. **Depends on**: T070, T071, T072. **FR**: FR-032.

- [ ] T074 [PH7] Phase-7 exit verification. Build all four configs; run tests; run code analysis. Greps: `rg -n 'WM_NCCALCSIZE' Casso/` → matches only in `Casso/Ui/Dialog/DialogPrimitive.cpp` (waits for Phase 11); `rg -n 'WM_NCCALCSIZE' Dxui/` → matches only in `Dxui/Win32/`. **Manual visual parity check (per SC-011 partial)** on main window, chromed panels, and Settings window at 100 % / 150 % / 200 % DPI on Win10 + Win11; Win11 snap-layouts works on all three. **Depends on**: T064–T073. **Commit**: `feat(dxui): unify custom-NC top-level windows on DxuiHostWindow`. **FR**: FR-050/051/052/053/095; **SC**: SC-009 (partial), SC-010 (3/4), SC-011 (mid-migration gate).

---

## Phase 8 — Popup hosting

**Goal (plan.md §Phase 8)**: Land `DxuiPopupHost` + the pool; migrate `DxuiDropdown` / `DxuiTooltip` / `DxuiPopupMenu` onto it. **Satisfies**: FR-054, FR-055, FR-056, FR-061, User Story 3, SC-008.

- [ ] T075 [PH8] Create `Dxui/Win32/DxuiPopupHost.{h,cpp}` per FR-054 / FR-056. `WS_POPUP | WS_EX_NOACTIVATE` (add `WS_EX_TRANSPARENT | WS_EX_LAYERED` for tooltips); own DXGI composition swap chain sharing parent `ID3D11Device`; use `CreateSwapChainForComposition` + DirectComposition visual, not `CreateSwapChainForHwnd`. `ShowParams`: `ownerHwnd`, `anchorRectScreen`, placement (`Below`/`Above`/`Right`/`Left`/`AtCursor`), `flipIfOffscreen`, dismiss policy (`OnClickOutside`/`OnClickAnywhere`/`OnPointerLeave`/`Manual`), input policy (`Interactive`/`PassThrough`), `shadow`, `std::unique_ptr<DxuiPanel> content`. `Show() -> std::future<int>`; shared state set on the UI thread inside the host's message handling (FR-083). Owner-chain tracking for cascading submenus. `MonitorFromRect` + monitor work area for offscreen flipping. `WM_DPICHANGED_BEFOREPARENT` handling plus host forwarding to every active popup. Focus scope push/pop via `DxuiFocusManager`. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. Click-outside dismiss via `SetCapture` + `WM_CAPTURECHANGED`. Auto-dismiss on owner `WM_ACTIVATE` / `WM_ACTIVATEAPP` / `WM_MOVE`. **Depends on**: T056, T064. **Exit**: covered by T078. **FR**: FR-054, FR-056, FR-083.

- [ ] T076 [PH8] Expose `DxuiPopupHost::ComputePlacementForTest(RECT anchorScreen, RECT monitorWorkArea, Placement preferred, SIZE popupSize) -> RECT` test seam (static, pure-function, no HWND). **Depends on**: T075. **FR**: FR-054; **SC**: SC-006.

- [ ] T077 [PH8] Add popup pool to `DxuiHostWindow` per FR-055: initial 3 instances, grow on demand, LIFO reuse. Debug-build instrumentation counter (`PopupHits()` / `PopupMisses()`) for test assertion. **Depends on**: T064, T075. **FR**: FR-055.


- [ ] T077A [PH8] Wire `DxuiHostWindow` to forward `WM_DPICHANGED_BEFOREPARENT` to every active `DxuiPopupHost` in the popup pool. Add a debug/test seam to enumerate active popups without exposing release-only state. **Depends on**: T075, T077. **FR**: FR-050, FR-056.

- [ ] T078 [PH8] Create `UnitTest/Dxui/DxuiPopupHostTests.cpp` — placement below/above/left/right/at-cursor; flip-if-offscreen against synthetic monitor rects (all four edges + corners); dismiss policy state machine (no real HWND — drive via test seams); cascading owner-chain registration/unregistration; host forwarding of `WM_DPICHANGED_BEFOREPARENT` to active popups; focus-scope push/pop with restore. Asserts via `ComputePlacementForTest` and a dispatching shim for the dismiss state. **Depends on**: T076, T077A. **FR**: FR-050, FR-054, FR-056; **SC**: SC-006.

- [ ] T079 [P] [PH8] Modify `Dxui/Widgets/DxuiDropdown.{h,cpp}` to host its option list inside a `DxuiPopupHost` instance acquired from the parent host window's pool. Remove any in-window clipping path. **Depends on**: T075, T077. **FR**: FR-061; **SC**: SC-008.

- [ ] T080 [P] [PH8] Modify `Dxui/Widgets/DxuiPopupMenu.{h,cpp}` to host its menu content via `DxuiPopupHost`; cascading submenus opened via owner-chain. **Depends on**: T075, T077. **FR**: FR-061.

- [ ] T081 [P] [PH8] Modify `Dxui/Widgets/DxuiTooltip.{h,cpp}` to host its content via `DxuiPopupHost` (with `WS_EX_TRANSPARENT | WS_EX_LAYERED`, dismiss `OnPointerLeave`, input `PassThrough`). **Depends on**: T075, T077. **FR**: FR-061.

- [ ] T082 [PH8] Phase-8 exit verification. Build all four configs; run tests; run code analysis. **Manual acceptance test** for User Story 3 / SC-008: anchor a dropdown ~20 px from the bottom of the Settings window; the menu opens upward (or extends across the parent edge) with no clipping. **Manual** debug-build assert: open and close the dropdown 5 times; `PopupHits()` ≥ 4 (pool reuse). **Depends on**: T075–T081 and T077A. **Commit**: `feat(dxui): add DxuiPopupHost with pool; fix dropdown clipping`. **FR**: FR-054/055/056/061; **SC**: SC-008 (User Story 3 ✅).

---

## Phase 9 — `DxuiViewport` + `DxuiDockLayout`; retire legacy edge layout

**Goal (plan.md §Phase 9)**: Casso's main shell becomes a root `DxuiPanel` with `DxuiDockLayout`; the emulator viewport sizes the Apple ][ pixel grid from the inside out. **Satisfies**: FR-030, FR-034, FR-021 (dock portion), FR-093, FR-094, SC-013.

- [ ] T083 [PH9] Create `Dxui/Core/IDxuiViewportInputSink.h` per `contracts/IDxuiViewportInputSink.h.md`. Pure-virtual `OnKey(const DxuiKeyEvent &)` and `OnMouse(const DxuiMouseEvent &)`. **Exit**: header compiles standalone. **FR**: FR-034.

- [ ] T084 [PH9] Create `Dxui/Core/DxuiViewport.{h,cpp}` per FR-030 / FR-034. Leaf `IDxuiControl`; size policy (`Fixed`/`Preferred`/`Fill`); `preferredSizeDip`; `consumesInput` flag (default `false`); `SetInputSink(IDxuiViewportInputSink *)`; `OnBoundsChanged(RECT)` callback registration (`SetBoundsChangedCallback(std::function<void(RECT)>)`); fires the callback **only** when new bounds differ from previous. When `consumesInput == true`: `Focusable() == true`; `OnKey` forwards to sink and returns `true` for **non-reserved** unmodified keys (Tab, Shift+Tab, Esc, Alt-alone, F10 stay with Dxui; Ctrl+Tab, Ctrl+Esc, Alt+F10, and Apple ][ CTRL-C/CTRL-G forward to the sink); `OnMouse` inside viewport rect forwards to sink. **Depends on**: T045, T083. **FR**: FR-030, FR-034.

- [ ] T085 [PH9] Create `Dxui/Layout/DxuiDockLayout.{h,cpp}` per FR-021. Per-child `DockSide` enum: `Top`/`Bottom`/`Left`/`Right`/`Fill` (exactly one `Fill` child). `Arrange` consumes children in registration order, peeling slabs off the parent rect. Provide **inverse**: `static SIZE ContainerSizeForFill(SIZE desiredFillDip, std::vector<IDxuiControl *> const & nonFillChildren)` — given a desired `Fill` size and fixed-measure non-fill children, compute the container size required so that the `Fill` slot ends up at exactly that size. **Depends on**: T046. **FR**: FR-021, FR-093.

- [ ] T086 [P] [PH9] Create `UnitTest/Dxui/DxuiDockLayoutTests.cpp` — anchors Top/Bottom/Left/Right/Fill produce expected rects; `ContainerSizeForFill` round-trips with fixed-measure non-fill children (compute container size → arrange → fill rect equals desired). At least one test fires a synthetic viewport-bounds change through a `std::function` subscriber and asserts the subscriber is invoked **exactly once** when bounds change, **zero times** when set to identical bounds. **Depends on**: T084, T085. **FR**: FR-021, FR-030, FR-093; **SC**: SC-006, SC-013.

- [ ] T087 [PH9] Refactor `Casso/Ui/UiShell.{h,cpp}` so the main shell becomes a root `DxuiPanel` with a `DxuiDockLayout`. Existing chrome bands dock Top/Bottom (and Left/Right where applicable); a single `DxuiViewport` fills the middle and is wired to the emulator. **Depends on**: T085. **FR**: FR-093.

- [ ] T088 [PH9] Locate the current `ClientSizeForFramebuffer` call sites (Casso side) and replace with `DxuiDockLayout::ContainerSizeForFill(...)` driven by the Apple ][ pixel-grid dimensions. The emulator's `D3DRenderer` subscribes to `DxuiViewport::OnBoundsChanged` via `SetBoundsChangedCallback` and resizes its render target only when bounds change. **Depends on**: T084, T087. **FR**: FR-030, FR-093, SC-013.

- [ ] T089 [PH9] Implement `IDxuiViewportInputSink` in Casso (e.g., `Casso/EmulatorInputSink.{h,cpp}` or attach to `EmulatorShell`) routing key/mouse events to the existing `EmulatorShell` / Apple ][ keyboard controller. Install via `DxuiViewport::SetInputSink`. Confirm `consumesInput = true` for the emulator viewport. **Depends on**: T084, T087. **FR**: FR-034.

- [ ] T090 [PH9] Delete `Casso/Ui/Chrome/LayoutManager.{h,cpp}`. If `Casso/Ui/Chrome/IEdgeContributor.h`, `Casso/Ui/Chrome/ICenterLayer.h`, or `Casso/Ui/Chrome/SimpleEdgeContributor.{h,cpp}` exist in the current tree, delete them as well (plan.md lists them under Phase 9 cleanup even though the current `Casso/Ui/Chrome/` listing does not show them — verify and remove any that are present). Drop the vcxproj item entries. **Depends on**: T087. **Exit**: `rg -n 'LayoutManager|IEdgeContributor|ICenterLayer|SimpleEdgeContributor' Casso/Ui/Chrome` returns zero hits. **FR**: FR-094.

- [ ] T091 [PH9] Delete `Casso/Ui/Layout.{h,cpp}` and `Casso/Ui/FocusManager.{h,cpp}` if (and only if) all consumers have migrated; otherwise document remaining holdouts and defer their deletion to Phase 10/11 as appropriate. **Depends on**: T087, T088. **Exit**: either the files are gone (preferred) or a follow-up note appears in the Phase 10 / Phase 11 entry below.

- [ ] T092 [PH9] Phase-9 exit verification. Build all four configs; run tests; run code analysis. **Manual** smoke test: launch Casso, confirm viewport sizes the Apple ][ grid correctly at startup, after a window resize, after a DPI change (drag to a different-DPI monitor). Confirm renderer no longer thrashes on identical bounds (instrument `D3DRenderer::Resize` with a debug counter; only fires when bounds change). **Depends on**: T083–T091. **Commit**: `refactor(dxui): replace edge-layout with DxuiDockLayout and DxuiViewport`. **FR**: FR-021/030/034/093/094; **SC**: SC-013.

---

## Phase 10 — Convert `ThemePage` (proof of concept)

**Goal (plan.md §Phase 10)**: Validate the declarative-layout + auto-fan-out + focus-manager story on the smallest settings page. **Satisfies**: FR-097 (partial — first page), SC-003 (begins), SC-004 (begins).

- [ ] T093 [PH10] Refactor `Casso/Ui/Settings/ThemePage.{h,cpp}` to derive from `DxuiPanel`. Use `DxuiFormLayout`. **Delete** the per-page `OnLButtonDown`, `OnLButtonUp`, `OnMouseHover`, `OnKey`, `Paint`, and `CollectFocusables` overrides (auto fan-out replaces them). Construct children via `Add<DxuiCheckbox>`, `Add<DxuiDropdown>`, etc. Focus order should fall out of geometry; add per-control `tabIndex` overrides only if visual reading order disagrees with geometric order. **Depends on**: T047, T054, T056. **FR**: FR-011, FR-031, FR-097.

- [ ] T094 [PH10] Bridge `Casso/Ui/Settings/SettingsPanel.{h,cpp}` to accept a `DxuiPanel`-based page alongside the legacy-style pages until Phase 11 converts the rest. Add a brief comment at the bridge code. No spec/phase numbers in the comment per the project's "no phase/task/spec references in comments" rule — phrase it as "temporary bridge for incremental page migration" with a TODO. **Depends on**: T093.

- [ ] T095 [PH10] Measure `ThemePage` LOC delta (before vs after) and `SettingsPanel.cpp` LOC delta (post-bridge). Record numbers in the commit body. If extrapolated reduction across the four pages falls materially short of 40 %, flag explicitly in the commit body before proceeding to Phase 11 (per spec R5 / plan.md §Risk Register). **Depends on**: T093, T094.

- [ ] T096 [PH10] Phase-10 exit verification. Build all four configs; run tests; run code analysis. **Manual** test: launch Casso, open Settings → Theme page; every control renders, focus order tabs in reading order, arrow nav moves spatially, theme change broadcasts down the tree. **Depends on**: T093–T095. **Commit**: `refactor(casso/ui): convert ThemePage to DxuiPanel + DxuiFormLayout`. **FR**: FR-011/031/097; **SC**: SC-003 (partial), SC-004 (partial).

---

## Phase 11 — Convert remaining pages, debug panels, dialogs; delete DialogPrimitive

Phase 11 is split into two independently mergeable parts: **Phase 11 — Part A** introduces `DxuiDialog` / `DxuiDialogManager` and rewrites Casso dialogs while legacy `DialogPrimitive` files remain in place; **Phase 11 — Part B** deletes legacy dialog files only after manual verification confirms both new dialogs run in the app.

**Goal (plan.md §Phase 11 — final migration; this is the release gate for the feature)**. Converts the remaining three settings pages, both debug panels, and both dialogs onto Dxui; introduces `DxuiDialog` + `DxuiDialogManager`; deletes the entire legacy `DialogPrimitive` family and the fourth NC plumbing copy. **Satisfies**: FR-070, FR-071, FR-072, FR-096, FR-097 (fully), FR-095 (fully), all remaining SCs (SC-003, SC-004, SC-005, SC-006, SC-010, SC-011, SC-012).

- [ ] T097 [P] [PH11] Refactor `Casso/Ui/Settings/MachinePage.{h,cpp}` to derive from `DxuiPanel`; use `DxuiFormLayout` or `DxuiGridLayout` as content shape demands; delete `OnLButtonDown`/`OnLButtonUp`/`OnMouseHover`/`OnKey`/`Paint`/`CollectFocusables` overrides. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004.

- [ ] T098 [P] [PH11] Refactor `Casso/Ui/Settings/HardwarePage.{h,cpp}` same treatment. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004.

- [ ] T099 [P] [PH11] Refactor `Casso/Ui/Settings/DisplayPage.{h,cpp}` same treatment. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004.

- [ ] T100 [PH11] Strip `Casso/Ui/Settings/SettingsPanel.{h,cpp}` of the temporary bridge introduced in T094 (all pages are now `DxuiPanel`s); collapse the dual code paths into the single `DxuiPanel` path. Measure `SettingsPanel.cpp` final line count; assert **≤ 1,307 lines** (40 % reduction from 2,179 baseline). **Depends on**: T097, T098, T099. **Exit**: `(Get-Content Casso/Ui/Settings/SettingsPanel.cpp).Length -le 1307`. **FR**: FR-097; **SC**: SC-003.

- [ ] T101 [P] [PH11] Refactor `Casso/Ui/Disk2DebugPanel.{h,cpp}` + `Disk2DebugPanelLayout.{h,cpp}` to derive from `DxuiPanel`; delete fan-out overrides; merge or delete the separate `Layout` companion if its responsibilities collapse into a `DxuiGridLayout` / `DxuiAbsoluteLayout` selection on the panel. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004.

- [ ] T102 [P] [PH11] Refactor `Casso/Ui/InputDebugPanel.{h,cpp}` + `InputDebugPanelLayout.{h,cpp}` same treatment. **Depends on**: T096. **FR**: FR-011, FR-097; **SC**: SC-004.

### Phase 11 — Part A: introduce Dxui dialogs and coexist with legacy dialogs

- [ ] T103 [PH11] Create `Dxui/Dialog/DxuiDialog.{h,cpp}` per FR-070. Derives from `DxuiPanel`. Composed of `DxuiCaptionBar` (title + close button), consumer-populated content panel, optional `DxuiDockLayout`-bottom button row. `Show()` returns nothing directly — the `DxuiDialogManager` wraps it. **Depends on**: T066, T085. **FR**: FR-070.

- [ ] T104 [PH11] Create `Dxui/Dialog/DxuiDialogManager.{h,cpp}` per FR-071, FR-072. API: `std::future<int> Show(std::unique_ptr<DxuiDialog> dialog, ShowParams)` where `ShowParams` includes `bool modalScrim` (default `false`). Maintains an internal `std::vector<DialogStackFrame>` (stack). On `Show`: capture current top-of-stack HWND (or owner HWND if stack empty) → `EnableWindow(prevTop, FALSE)`, create the new dialog's `DxuiHostWindow` with `ownerHwnd = prevTop`, push the frame. On dialog close: `EnableWindow(prevTop, TRUE)`, pop the frame, set the `std::future` shared state on the UI thread. Invoke `DXUI_ASSERT_UI_THREAD()` at every public method entry. Owner-window assignment lets Win32 handle z-order and activation restore automatically. **Depends on**: T064, T103. **FR**: FR-071, FR-072, FR-083.

- [ ] T105 [PH11] Expose `DxuiDialogManager::PushForTest(IDxuiControl * fakeHwndStandIn) -> int` and `PopForTest(int frameId, int result)` test seams driving the stack without real HWNDs. **Depends on**: T104. **FR**: FR-072; **SC**: SC-006.

- [ ] T106 [PH11] Create `UnitTest/Dxui/DxuiDialogManagerTests.cpp` — push two frames simulating "download fails → error dialog opens on top of download dialog"; pop the inner; assert the outer regains the "active top" position; pop the outer; assert the owner HWND ID stand-in regains active. Cover `modalScrim` flag default `false`. **Depends on**: T105. **FR**: FR-072; **SC**: SC-006.

- [ ] T107 [PH11] Create `Casso/Ui/Dialogs/` directory. Use `git mv Casso/Ui/Dialog/X.cpp Casso/Ui/Dialogs/X.cpp` (or delete-then-create with an intermediate commit) to avoid NTFS case-aliasing ghost directories. Add `Casso/Ui/Dialogs/StartupDownloadDialog.{h,cpp}` and `Casso/Ui/Dialogs/StandaloneDialog.{h,cpp}` rewritten as `DxuiDialog`-based panels using `DxuiDialogManager::Show`. Functionality matches the legacy versions behaviourally (download flow + URL handling + standalone-mode toggle); pixel parity verified per SC-011 (manual sign-off). **Depends on**: T103, T104. **FR**: FR-092.


- [ ] T107A [PH11] Manually verify both new Dxui-based dialogs run in the app while the legacy `DialogPrimitive` files still exist. Confirm download flow, URL handling, standalone-mode toggle, and SC-011 visual sign-off before deleting legacy files. **Depends on**: T107. **FR**: FR-092, FR-096; **SC**: SC-011.

### Phase 11 — Part B: delete legacy dialog system after verification

- [ ] T108 [PH11] **Delete** legacy dialog scaffolding: `Casso/Ui/Dialog/DialogDefinition.h`, `Casso/Ui/Dialog/DialogLayout.{h,cpp}`, `Casso/Ui/Dialog/DialogPrimitive.{h,cpp}`, `Casso/Ui/Dialog/DialogPrimitiveRenderer.{h,cpp}`, `Casso/Ui/Dialog/StartupDownloadDialog.{h,cpp}`, `Casso/Ui/Dialog/StandaloneDialog.{h,cpp}`. Remove the now-empty `Casso/Ui/Dialog/` directory using `git mv`/explicit delete paths so NTFS does not leave case-aliasing ghost directories. Drop all vcxproj + filters entries. **Depends on**: T107A. **Exit**: `rg -n 'DialogPrimitive|DialogDefinition|DialogLayout|DialogPrimitiveRenderer' Casso/` → zero hits; `Test-Path Casso/Ui/Dialog/` → `False`. **FR**: FR-096.

- [ ] T109 [PH11] If T091 deferred the deletion of `Casso/Ui/FocusManager.{h,cpp}` and/or `Casso/Ui/Layout.{h,cpp}`, delete them now (all settings pages, debug panels, and dialogs are on `DxuiFocusManager` / Dxui layouts). Drop vcxproj entries. **Depends on**: T097–T108. **Exit**: `rg -n 'class\s+(FocusManager|Layout)\b' Casso/Ui` returns zero hits.

- [ ] T110 [PH11] Update `CHANGELOG.md` with user-visible changes: new in-house Dxui framework, dropdown clipping fix, snap-layouts on all top-level windows, manual-signoff-equivalent custom chrome. Update `README.md` if test counts, features list, or roadmap items change. Per project rules these MUST update for `feat`/`fix` commits.

- [ ] T111 [PH11] **Phase-11 / feature release-gate verification**. Run in order, all must pass:
  1. `scripts\Build.ps1 -Configuration Debug -Platform x64`
  2. `scripts\Build.ps1 -Configuration Release -Platform x64`
  3. `scripts\Build.ps1 -Configuration Debug -Platform ARM64`
  4. `scripts\Build.ps1 -Configuration Release -Platform ARM64`
  5. `scripts\RunTests.ps1` — no regression vs `specs/013-dxui-framework-extraction/baseline-tests.txt` (SC-005)
  6. `scripts\Build.ps1 -RunCodeAnalysis` — clean
  7. `scripts\RunDormannTest.ps1` — no regression vs `baseline-validation.txt`; Klaus Dormann passes if it passed at baseline (SC-012)
  8. `scripts\RunHarteTests.ps1 -SkipGenerate` — no regression vs `baseline-validation.txt`; Tom Harte passes if it passed at baseline (SC-012)
  9. Greps (all expected to return **zero** hits):
     - `rg -n 'OnLButtonDown|OnLButtonUp|OnMouseHover|OnKey|^\s*void\s+\w+::Paint\b|CollectFocusables' Casso/Ui/Settings Casso/Ui/Disk2DebugPanel.cpp Casso/Ui/Disk2DebugPanel.h Casso/Ui/InputDebugPanel.cpp Casso/Ui/InputDebugPanel.h Casso/Ui/Dialogs` → 0 (SC-004)
     - `rg -n 'WM_NCCALCSIZE' Casso/` → 0; `rg -n 'WM_NCCALCSIZE' Dxui/Win32/` → matches expected (SC-010)
     - `rg -n 'DialogPrimitive|DialogDefinition|DialogLayout' Casso/` → 0
     - `rg -n '\w \(\)' Dxui/ Casso/Ui/ Casso/Pch.h UnitTest/Dxui/` → zero hits in migration-touched paths. Pre-existing violations elsewhere are explicitly out of scope
  10. **Manual visual parity sign-off** at 100 % / 150 % / 200 % DPI on Win10 + Win11 across: main window, chromed panels, Settings, both debug panels, both dialogs (SC-011).
  11. **Manual** SettingsPanel.cpp line count check: `(Get-Content Casso/Ui/Settings/SettingsPanel.cpp).Length -le 1307` (SC-003).
  12. **Manual** dropdown bottom-of-Settings test (SC-008 / User Story 3 re-verify).
  13. **Manual** Win11 snap-layouts hover on main + chromed + Settings (SC-009 / User Story 4 re-verify).

  **Depends on**: T097–T110. **Commit**: `refactor(casso/ui): migrate remaining pages, debug panels, dialogs to Dxui; delete DialogPrimitive`. **Merge**: `git merge --no-ff` into `master`. **FR**: FR-070/071/072/092/095/096/097; **SC**: SC-003, SC-004, SC-005, SC-006, SC-008, SC-009, SC-010, SC-011, SC-012, SC-013 (all satisfied).

---

## Dependencies & Execution Order

### Phase Dependencies

```
PH1 → PH2 → PH3 → PH4 → PH5 → PH6 → PH7 → PH8 → PH9 → PH10 → PH11
```

Phases are **strictly sequential** — every phase ends green and is independently mergeable. The user-story timeline is:

| User Story | Priority | Satisfied at end of |
|------------|----------|---------------------|
| US1 — Dxui as standalone consumable library | P1 | **Phase 1** (then continuously) |
| US5 — Headless widget tests with no D3D device | P2 | **Phase 6** (mocks + first tests land); fully exercised by Phase 11 |
| US4 — Snap-layouts on every top-level window | P2 | **Phase 7** (3 of 3 migrated host windows) |
| US3 — Popups extend beyond parent | P1 | **Phase 8** |
| US2 — Casso UI runs on Dxui with zero regressions | P1 | **Phase 11** (release gate) |

### Intra-Phase Dependencies

- **Phase 1**: T001 unblocks T002–T009. T003 + T005/T006 unblock T007. All `[P]`-marked are parallel-safe; T009 is the gate.
- **Phase 2**: T010–T017 all independent and parallel-safe; T018 is the gate.
- **Phase 3**: T019 → T020; T021 independent of T019/T020.
- **Phase 4**: T023–T036 all parallel-safe (different files); T037 is the gate.
- **Phase 5**: T038 → T039 → T040; T041/T042 follow T038–T040.
- **Phase 6**: T044 → T045 → T047. T046 → T052–T055 (parallel). T048/T049 → T050 → T051. T056 depends on T045+T047. T057/T058 parallel after T048/T049. Tests T059–T062 parallel after their respective targets.
- **Phase 7**: T064 → T065. T066/T067/T068 parallel after T064 (well, T064 itself just needs panel from T047). T069 needs T065–T068. T070/T071/T072 parallel after T064 + the chrome primitives.
- **Phase 8**: T075 → T076/T077. T078 after T076. T079/T080/T081 parallel after T077.
- **Phase 9**: T083 → T084. T085 independent. T086 after T084+T085. T087/T088/T089 sequential consumer wiring. T090/T091 after T087+T088.
- **Phase 10**: T093 → T094 → T095 → T096.
- **Phase 11**: T097/T098/T099/T101/T102 parallel after T096. T100 after T097–T099. T103 → T104 → T105 → T106. T107 after T103+T104. T107A after T107. T108 after T107A. T109 after T097–T108. T111 after everything else.

### Parallel Opportunities

- **Phase 2** offers the widest fan-out: 8 file moves with no cross-dependencies (T010–T017) can all run in parallel.
- **Phase 4** offers the second widest: 14 widget moves (T023–T036) all parallel.
- **Phase 6** has multiple parallel sub-groups: layout files (T052–T055), test files (T059–T062), mock infrastructure (T057/T058).
- **Phase 11** parallel block: 5 page/panel conversions (T097/T098/T099/T101/T102) can run together.

---

## Implementation Strategy

### Strict-Sequence (Recommended)

Phases land in order Phase 1 → Phase 11. Each phase produces a single PR; each PR ends green and is merged `--no-ff`. This matches the "each phase is independently mergeable" gate and the spec's R6 risk-mitigation (no mega-PR).

### MVP Increments Available Mid-Migration

- **After Phase 1** (US1): Dxui is consumable; downstream consumers could in principle target it (none exist outside this repo yet).
- **After Phase 6** (US5): headless widget tests run with no D3D device — Casso CI gains test coverage for widget logic.
- **After Phase 7** (US4): Win11 snap-layouts hover popover appears on the main window + chromed panels + Settings — visible win.
- **After Phase 8** (US3): the long-standing dropdown clipping bug is fixed — user-visible bug fix.
- **After Phase 11** (US2 + everything): full extraction complete, pixel-parity preserved.

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
| FR-052 | T064,T067,T069,T074 |
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
| FR-095 | T070-T072,T108,T111 |
| FR-096 | T108 |
| FR-097 | T093-T102,T109 |
| SC-001 | T001,T004,T009 |
| SC-002 | T005-T009 |
| SC-003 | T095,T100,T111 |
| SC-004 | T093,T097-T102,T111 |
| SC-005 | T000,T009,T111 |
| SC-006 | T041,T057-T062,T069,T078,T086,T106 |
| SC-007 | T041,T042,T057,T058 |
| SC-008 | T079,T082,T111 |
| SC-009 | T064-T069,T074,T111 |
| SC-010 | T070-T074,T108,T111 |
| SC-011 | T074,T107A,T111 |
| SC-012 | T009,T111 |
| SC-013 | T085-T088,T092,T111 |
| US1 | T001-T009 |
| US2 | T093-T111 |
| US3 | T075-T082 |
| US4 | T064-T074 |
| US5 | T041,T042,T057-T062 |
