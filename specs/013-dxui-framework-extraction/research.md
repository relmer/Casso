# Phase 0 — Research: Dxui Framework Extraction

All five spec ambiguities (Q1–Q5) were resolved in the 2026-03-19 clarify session and are encoded as FR-072 (modal stack), FR-011 (Collapsed-only visibility), FR-034 (viewport input sink), FR-004 + FR-007 (umbrella header chokepoint), and FR-083 (UI-thread-only). This document captures the **design** decisions taken in planning that aren't direct clarifications — the things a reviewer would otherwise ask about.

---

## 1. Project file shape — StaticLibrary, mirrors CassoCore

**Decision**: `Dxui.vcxproj` is `ConfigurationType=StaticLibrary`, toolset `v145`, character set `Unicode`, four configurations (`Debug|x64`, `Release|x64`, `Debug|ARM64`, `Release|ARM64`), warning level `Level4` with `WarningAsError`, code analysis enabled, internal PCH.

**Rationale**: Mirrors the existing `CassoCore.vcxproj` and `CassoEmuCore.vcxproj` exactly. Consumers (`Casso`, `UnitTest`) pick up the library via `ProjectReference`, which gives MSBuild the dependency edge and propagates `AdditionalIncludeDirectories` if we set them at the library level. DX `.lib` link directives travel from Dxui to consumers via `Link.AdditionalDependencies` inherited through `ProjectReference` (FR-006).

**Alternatives considered**:

- **DLL** — would create a runtime dependency `Dxui.dll`, complicate the install layout (one .exe + one .dll on disk), require dllexport/dllimport plumbing across every Dxui type. Zero upside for an in-tree, single-consumer library.
- **Header-only** — the render facades and `DxuiHostWindow` carry substantial implementation; header-only would balloon every Casso TU's compile time.
- **Merge into Casso** (i.e., no extraction) — defeats the whole spec.

---

## 2. Umbrella-header chokepoint mechanics

**Decision**: `Dxui/Dxui.h` is the *only* angle-bracket include site in the Dxui project's public surface. `Dxui/Pch.h` (internal) duplicates that include set so Dxui's own `.cpp` files compile against the same system headers. Consumers add `#include "Dxui.h"` to their `Pch.h`; consumer `.cpp` files include only `Pch.h` and get the entire Dxui surface plus all its system deps.

**Why duplicate the include set in `Dxui/Pch.h`?** PCH content is a property of *the project doing the compilation*, not of headers it consumes. Dxui's `.cpp` files use `Dxui/Pch.h`; Casso's use `Casso/Pch.h`. Both include `Dxui.h` directly or indirectly, so the system-header set must appear in both PCHs. The duplication is one block of `#include`s in two files; the alternative (`Dxui/Pch.h` itself includes `Dxui.h`) introduces a circular-ish flow where the project's PCH depends on its own public header, which makes incremental rebuilds noisier when `Dxui.h` changes. We accept the minor duplication.

**Constitution implication**: `.github/copilot-instructions.md`'s "NEVER use angle-bracket includes anywhere except `Pch.h`" needs the carve-out for library umbrella headers. Recorded under Complexity Tracking in plan.md; the amendment ships in Phase 1's scaffolding commit.

**Alternative considered**: Per-header self-include of system deps (each `DxuiButton.h` includes `<d3d11.h>` etc.). Rejected — same set of angle-brackets multiplied by ~40 headers, and every header would need precise minimum-include hygiene to avoid pulling in too much. Single chokepoint is one file to maintain.

---

## 3. DX device sharing between `DxuiHostWindow` and `DxuiPopupHost`

**Decision**: `DxuiHostWindow` creates the `ID3D11Device` with `D3D11_CREATE_DEVICE_BGRA_SUPPORT` (via `D3D11CreateDeviceAndSwapChain` or factory equivalent) and owns it as the canonical device. `DxuiPopupHost` does **not** create a device — it gets a non-owning `ID3D11Device *` from its `DxuiHostWindow` owner via `ShowParams` (or implicitly via the pool's owner). The popup creates only its own `IDXGISwapChain1` (composition swap chain via `IDXGIFactory2::CreateSwapChainForComposition` and a DirectComposition visual), its own `ID3D11RenderTargetView`, and a `IDCompositionVisual` if rounded-corner / Mica composition is in play.

**Rationale**: Sharing the device avoids the substantial cost of a second device (driver memory + the inability to share GPU resources). Per-popup swap chain is required because each popup is a separate HWND with a separate present surface. Owner-chain tracking (FR-056) keeps the popup's owner HWND alive long enough to satisfy the shared-device lifetime; the popup's `OnClose` callback runs before the popup releases its swap chain references.

**Threading**: All device + swap-chain calls run on the UI thread (FR-083). No multi-thread protection of `ID3D11Device`.

**Alternative considered**: Per-popup device. Rejected — wastes driver memory, prevents resource sharing (text formats, glyph runs from DirectWrite), no upside.


---

## 3a. Popup HWND swap-chain flavor — DirectComposition, not HWND swap chains

**Decision**: The shared D3D11 device MUST be created with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`. Popup surfaces MUST use `IDXGIFactory2::CreateSwapChainForComposition` connected to a DirectComposition visual for the `WS_POPUP` HWND. Do **not** use `CreateSwapChainForHwnd` for popups.

**Rationale**: Popup HWNDs need correct z-ordering and transparency while sharing the parent device. DirectComposition is the supported path for a transparent composition surface over a popup HWND; HWND swap chains are a poor fit for layered/no-activate popups and tend to fight z-order/transparency.

**Task references**: T064 owns BGRA-capable device creation; T075 owns popup composition swap-chain creation.

---

## 4. Geometry-based focus traversal (reading-order tab + spatial arrows)

**Decision**: Tab order = sort focusables by `(top / rowEpsilon, left)`, where `rowEpsilon` is chosen from `DxuiFocusManager::RowEpsilonDip()`, defaulting to `IDxuiTheme::BodyLineHeightDip()` (a single value, no per-widget tuning). Spatial arrow nav picks the nearest focusable whose centre lies inside a 90° cone in the requested direction, breaking ties by Euclidean distance. Per-control `tabIndex` overrides preempt geometry sort entirely. `IDxuiControl::kTabIndexGeometry = -1` is the default and uses geometry-based ordering; `IDxuiControl::kTabIndexExcluded = -2` skips Tab traversal but remains focusable by mouse.

**Rationale**: The row-bucketing approach is the same trick web browsers use for "horizontal lines of focusable elements." Empirically it produces stable, expected order for forms (label : field rows) and grids. The 90° cone + Euclidean tie-break is the standard treatment for spatial nav in XAML / Cocoa.

**Risk** (R3 in plan): Existing Casso pages currently hand-number `focusId`; some pages may shift focus order under geometry sort. Mitigation: per-page focus-order review during conversion in Phases 8–9; `tabIndex` override available for any page that needs a non-geometric order.

**Alternatives considered**:

- **Explicit `tabIndex` on every control** (current approach) — works but is exactly the boilerplate this spec is trying to eliminate.
- **DOM-style document order** — doesn't apply; `DxuiPanel::Add<T>` order is not necessarily the visual order with non-stack layouts.

---

## 5. Mock painter recording strategy

**Decision**: `MockDxuiPainter` records each `IDxuiPainter` method invocation into a `std::vector<RecordedPaintCall>` (POD struct, value-comparable). Tests assert on the **sequence** of recorded calls (count, kind, key parameters). No pixel comparison.

**Rationale**: We're testing widget *logic* — "does the button paint a focus ring when focused?" — not the rendering pipeline. A pixel-level test would require a real D3D device, which violates the testing-discipline rule and would be brittle across driver versions. Recording at the interface boundary catches every relevant regression (forgot to paint the focus ring → zero recorded `StrokeRect` calls → test fails) without coupling to GPU output.

**What we deliberately don't mock**: text-shaping deep internals. `MockDxuiTextRenderer::Measure` returns a canned `SIZE` derived from `text.length() * canonicalGlyphWidthDip` so layouts are deterministic; we don't try to model DirectWrite's actual shaping. Layout tests that need true metrics are out of scope for unit tests.

---

## 6. Migration phase ordering rationale

The spec lists 11 phases. The order is not arbitrary — each phase establishes a base the next phase needs:

| Phase | Establishes | Why this order |
|-------|-------------|----------------|
| 1 — Scaffold | Project + umbrella + include carve-out | Without the project, nothing else compiles. |
| 2 — Generic files | Shared utilities (HitTester, UiInput, Animation, DpiScaler, DwmHelpers) | Widgets and the framework need these; they have no dependencies of their own. |
| 3 — Render facades rename | `DxuiPainter` / `DxuiTextRenderer` concrete names | Phase 4 widgets reference these; doing the rename first avoids per-widget churn. |
| 4 — Widgets | Concrete widget types under `Dxui` names | Phase 5 changes their `Paint` signatures; needs to be after the move so the rename is mechanical. |
| 5 — `IDxuiTheme` | Theme abstraction | Phase 6's `IDxuiControl::Paint` takes `const IDxuiTheme &`; widgets need this interface available. |
| 6 — Framework | `IDxuiControl`, `DxuiPanel`, layouts, focus, render interfaces | The framework lands as one unit because all of its types reference each other. |
| 7 — Host window | `DxuiHostWindow` + chrome primitives | Later phases collapse Casso's main shell onto a `DxuiPanel` root; needs the host that owns the root. |
| 8 — Popup host | `DxuiPopupHost` | Later settings-page phases use dropdowns; clipping fix needs to be in place before pages migrate so we do not ship regressions. Also delivers SC-008 independently. |
| 9 — Viewport + Dock | `DxuiViewport`, `DxuiDockLayout`, retire `LayoutManager` | Casso's main shell composition needs both. |
| 10 — `ThemePage` proof | One page works end-to-end | Validates the SC-003 LOC-reduction target before committing to the bulk conversion. |
| 11 — Remaining migration + `DialogPrimitive` deletion | Everything else | Last step; meets all "delete" exit criteria (DialogPrimitive, fan-out overrides, NC duplication). |

**Why not land `DxuiHostWindow` before `DxuiPanel`?** `DxuiHostWindow` owns a root `DxuiPanel`. The dependency edge requires `DxuiPanel` first.

**Why not delete `Casso/Ui/FocusManager` in Phase 6?** Settings pages still use it until Phase 10/11. Brief coexistence is the cost of staged migration.

**Why is `CassoCli` deliberately excluded from referencing Dxui?** `CassoCli` is the AS65-compatible assembler CLI — no UI, no graphics, console-only. Linking Dxui (and transitively DX libs) would bloat the CLI binary and create a meaningless dependency.

## 2026-07 design decisions (post-reshape)

- **Decision**: Ship a generic `DxuiPropertySheet` / `DxuiPropertyPage` (tab strip + OK/Cancel/Apply + per-page dirty flag + `OnApply` / `ApplyAllDirtyPages`). **Rationale**: the hand-rolled `SettingsPanel` proved the paged-with-Apply pattern; extracting it into the framework lets future dialogs reuse dirty-tracking + live-preview. **Alternatives**: keep it bespoke per dialog (rejected — duplication); a single flat dialog (rejected — no multi-page / dirty model).
- **Decision**: `DxuiHwndSource` gains a composited-transparent window mode + `SetAfterPaintHook`. **Rationale**: the emulator viewport must composite through the host-owned swap chain (the T129 flip); an after-paint hook lets the consumer blit its frame into the composited surface between the panel-tree paint and `Present` without the host knowing about the emulator. **Alternatives**: a second child HWND / swap chain (the old `CassoRenderSurface`, now deleted — extra HWND + DWM seams); a before-present hook only (insufficient — the consumer needed the composited surface).
- **Decision**: The no-`DxuiDialog` model shipped as a **modal / modeless split** (`ShowModalDialog` + `ShowModelessDialog`) rather than a single `ShowDialog`. **Rationale**: a settings / preview sheet must run *modeless* so input still reaches the emulator for live preview, while message boxes / ROM pickers want a true modal pump. **Alternatives**: one `ShowDialog` with a modal flag (rejected — the two loops differ enough to warrant separate entry points).
- **Decision**: Settings keeps an **OK-only commit** model (button row is `[OK][Cancel]`, no Apply). Machine / theme / hardware commit on OK; live settings (CRT, colours, drive-audio audition) apply immediately and revert on Cancel. A machine or ROM change is destructive (power cycle), so FR-131 surfaces an inline "takes effect after OK; OK restarts the machine" notice, and FR-132 adds a Theme "Apply now" for the one cheap, reversible live-apply. **Rationale**: avoids power-cycling the emulator on a casual Apply and keeps Cancel coherent (nothing irreversible commits until OK-and-close). **Alternatives**: a full property-sheet Apply (rejected for now — Apply would power-cycle mid-edit); an immediate machine switch on selection (rejected — breaks staging + Cancel).
