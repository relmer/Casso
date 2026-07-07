# Draft Stub — MatrixRain Dxui adoption (specs/014)

**Status**: DRAFT — captured mid-specs/013 to preserve design context. Run the
proper speckit workflow (`speckit.git.feature` → `speckit.specify` →
`speckit.clarify` → `speckit.plan` → `speckit.tasks` → `speckit.analyze` →
`speckit.implement`) when this feature is actually kicked off. The notes below
seed the eventual spec — they are NOT the spec itself.

**Prerequisite**: specs/013-dxui-framework-extraction must be complete (Dxui
v1.0 shipped as a stable static library; all 14 phases closed; release gate
passed).

---

## Purpose

Migrate **MatrixRain** (peer C++/Win32/DirectX project at `relmer/MatrixRain`)
off its current bespoke Win32 UI onto Dxui. MatrixRain is a Matrix-rain
screensaver + desktop app with a settings dialog currently being redesigned
as a Win32 property sheet — we redirect that redesign onto Dxui instead.

Out of scope:
- Casso changes (specs/013 already covers Casso's Dxui adoption)
- MatrixRain functional changes (rain rendering, density, colors, glow — all preserved)
- DlgProc support in Dxui (Dxui is WndProc-only by design; see specs/013
  Clarifications)

---

## What this feature adds to Dxui

Driven by MatrixRain's actual needs (not speculative). Each new widget should
be designed against MatrixRain's concrete settings UI surface rather than
guessed at.

### Likely needed (high confidence)

- **DxuiPropertySheet** — generic tabbed/nav'd settings container with
  OK/Apply/Cancel button row, dirty-state tracking, optional reset-required
  confirmation flow. Extract the pattern from Casso's existing `SettingsPanel`
  (specs/013 Phase 13). Two layout styles: `NavStyle::TopTabs` (matches
  Casso's UX) and `NavStyle::LeftNav` (Win11 Settings style).

  Sketch API:
  ```cpp
  class DxuiPropertySheet : public DxuiPanel
  {
  public:
      enum class NavStyle { TopTabs, LeftNav };

      void  AddPage         (const std::wstring & label,
                              std::unique_ptr<DxuiPanel> page,
                              wchar_t icon = 0);
      void  SetActivePage   (size_t index);
      void  SetNavStyle     (NavStyle);
      void  SetButtons      (bool showApply, bool showCancel, bool showOk);
      void  SetDirtyTracker (std::function<bool()> isDirty);
      void  SetOnApply      (std::function<void (DxuiApplyContext &)>);
  };
  ```

- **DxuiToggle visual restyle** — the widget exists (moved in specs/013
  Phase 4) but its current paint is Casso-flavored. Re-paint to Win11
  fluent-switch style, driven through `IDxuiTheme`. API-stable.

### Probably needed (verify against MatrixRain's settings UI when starting)

- **DxuiInfoBar** — banner-style notification strip with icon + message +
  optional dismiss button. Used for "Settings require restart" / "Drive
  write-protected" / etc. Casso currently does these via bespoke paint inside
  pages; an `InfoBar` widget formalizes it.
- **DxuiProgressBar** — formalize as a real widget. Casso's
  `StartupDownloadDialog` currently uses custom paint inside `DialogPrimitive`;
  MatrixRain's settings may need progress feedback during preview generation
  or similar.
- **DxuiNumberInput** — numeric entry with up/down spinner arrows + min/max
  validation. Casso uses sliders for numeric settings; MatrixRain may want
  numeric entry for density / glow / etc.
- **DxuiExpander** — collapsible section header. Useful for "Advanced settings"
  grouping in a property sheet page.
- **DxuiHyperlink** — clickable text (Label + click handler). For "Learn more"
  / "Restore defaults" / etc.

### Maybe needed (decide during migration)

- **DxuiNavigationView** — vertical nav rail alternative to TabStrip for
  property sheets with many pages. If MatrixRain has only 3-4 pages, TabStrip
  is fine and we skip this.
- **DxuiSegmentedControl** — horizontal radio-button-like selector (modern
  replacement for "Color: [Color] [Green] [Amber] [White]" radio groups).
- **DxuiImage** — display a bitmap. Painter already supports
  `DrawIconBitmap`; just needs a widget wrapper for use in panels.
- **DxuiSearchBox** — text input with search icon and clear button. Only if
  MatrixRain's settings get long enough to justify it.

### Explicitly DEFERRED unless MatrixRain proves it needs them

- **DxuiScrollViewer** — property sheets traditionally size to fit the
  largest page (matches every Windows property sheet ever). Casso doesn't
  need scrolling; MatrixRain probably doesn't either. Add only if a real
  use case appears.
- **DxuiPasswordBox** — neither Casso nor MatrixRain stores credentials.
- **DxuiColorPicker, DatePicker, TimePicker, AutoSuggestBox, RatingControl**
  — out of scope for both target apps.
- **DxuiCard** — pure paint pattern; can be expressed as a styled `DxuiPanel`
  subclass if needed. Not a separate widget.

---

## What this feature adds to MatrixRain

- **Migrate MatrixRain off `Window` (or whatever bespoke Win32 base it uses)
  onto `DxuiHostWindow`.** Mirrors specs/013 Phase 11d for Casso — drop
  inheritance, compose a `DxuiHostWindow` via `IDxuiHostClient`. May be
  smaller scope than Casso's EmulatorShell restructure depending on
  MatrixRain's complexity.
- **Replace MatrixRain's settings dialog with `DxuiPropertySheet`.** The
  in-progress Win32 property-sheet redesign is redirected here instead.
- **Replace any MatrixRain-side custom controls with Dxui equivalents.**
- **Verify the rain rendering pipeline keeps working** through the new host
  swap chain — same pattern as Casso's Apple ][ viewport reroute via
  `DxuiViewport`.

---

## Phase outline (preliminary)

1. **Audit MatrixRain's current Win32 UI surface.** Catalog every WndProc,
   every dialog, every custom control. Identify which Dxui widgets cover
   what; identify gaps. Surface gaps drive the widget-build phases below.
2. **Build needed Dxui widgets** (DxuiPropertySheet + the others MatrixRain
   actually needs, based on audit). Each widget = one phase with unit tests
   + a Casso integration check (no Casso regressions allowed since specs/013
   has already shipped Dxui v1.0).
3. **Restyle DxuiToggle** to Win11 fluent. Verify Casso's existing
   `m_driveAudio` toggle still looks acceptable (or update Casso's chrome
   theme accordingly).
4. **MatrixRain host migration.** Drop Window inheritance; adopt
   DxuiHostWindow + IDxuiHostClient. Preserve rain rendering through the
   host's swap chain.
5. **MatrixRain settings migration.** Build the new property sheet using
   DxuiPropertySheet + relevant widgets. Delete the old settings dialog.
6. **Release gate.** All MatrixRain functionality preserved (visual + behavioral).
   All Casso tests still pass (no Dxui regressions). Build all 4 configs in both
   solutions.

---

## Risks / open questions

- **Dxui v1.0 widget coverage gaps surface during migration.** Be willing to
  add widgets ad-hoc, with the discipline of phase boundaries — don't try to
  predict the full set up front.
- **MatrixRain's in-progress property-sheet redesign.** Coordinate with the
  existing work — either pause it and resume on Dxui, or finish the Win32
  redesign first and then port. Decide before starting.
- **Cross-solution dependency**: MatrixRain will need to reference Dxui from
  Casso's repo. Options: (a) Casso publishes Dxui as a versioned package
  (NuGet, vcpkg); (b) MatrixRain adds Casso as a git submodule; (c) extract
  Dxui to its own repo. Pick before Phase 4 of MatrixRain migration.
- **Naming clash potential**: MatrixRain may have its own classes named
  `Window`, `Settings`, etc. Mechanical conflicts during the Dxui adoption.
- **MatrixRain's render pipeline** uses Direct3D directly today; verify it
  can adopt DxuiHostWindow's shared device + swap chain pattern (specs/013
  Phase 11d already worked out the shape for Casso).

---

## Notes captured during specs/013 conversation

- User picked Path B (separate feature for MatrixRain) over Path A
  (extend specs/013) on 2026-06-05 — keeps Casso release scope tight.
- ScrollViewer explicitly out of scope — property sheets size to largest page.
- DlgProc support explicitly NOT shipping in Dxui — see specs/013
  Clarifications. MatrixRain dialogs become DxuiPropertySheet / DxuiDialog
  (WndProc-based), not Win32 dialogs.
- IDxuiHostClient interface stabilized in specs/013 (commit ab5b70f):
  DxuiMessageResult enum for trivial returns; LRESULT for rich-return
  messages (OnCreate, OnNcHitTest, OnCtlColorStatic, OnDrawItem). Polarity
  matches Win32/WPF/MFC (Handled = consumer took it; NotHandled = host
  calls DefWindowProc).
