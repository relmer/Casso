# Phase 0 - Research: 007 Native Chrome/Nav Pivot

## Metadata

- Date: 2026-05-22
- Branch: 007-ui-overhaul

## Scope

This research document covers only the chrome/nav pivot.

- Title bar rendering
- System button visuals (min/max/close)
- Nav strip and dropdown rendering
- Chrome input/hit-routing

It does not redefine non-chrome panel work.

## Decisions

### R1 - Rendering split

Decision:

- Use DirectWrite for title/nav text.
- Use D3D11 for rectangles, borders, fills, hover and active states.

Rationale:

- Text quality and baseline control must be deterministic.
- Geometry is straightforward and cheap in the existing render path.

### R2 - Single ownership

Decision:

- Use one native owner for title/nav rendering and input routing.
- Do not keep a parallel ownership path for chrome surfaces.

Rationale:

- Removes overlap/clipping conflicts.
- Eliminates "which layer is authoritative" bugs.

### R3 - Window behavior contract

Decision:

- Keep existing NC behavior contract:

  - caption drag
  - min/max/close hit zones
  - resize edges
  - snap behavior

- Change only how chrome rects are computed and rendered.

Rationale:

- Preserves expected UX while reducing migration risk.

### R4 - Command routing contract

Decision:

- Preserve current IDM command routing and dispatch entry points.
- Native nav uses the existing command registry/parity model.

Rationale:

- Avoids behavior drift and limits rewrite scope.

### R5 - DPI/layout policy

Decision:

- Compute all chrome metrics from per-window DPI.
- Validate at 100%, 125%, 150%, 200% scaling for chrome layout; SC-002 hot-swap evidence is captured at 100% and 150% per spec.

Rationale:

- Current failures are mostly placement quality problems.
- DPI invariants need to be explicit and testable.

### R6 - RmlUi excision strategy

Decision:

- Remove the `External/RmlUi/` project reference, include paths, preprocessor
  defines, vendored source tree, Rml-prefixed source files, Rml shaders, and
  remaining `Rml` / `RMLUI` symbol uses before adding the native pipeline.
- For non-Rml-prefixed UI files that still mention Rml symbols, delete files
  that are fully rewritten later and stub files whose state logic must keep the
  tree compiling until their owning phase rewrites them.

Rationale:

- The build must never compile both UI ownership paths at the same time.
- Stub/delete decisions keep P0 mechanically verifiable while preserving only
  the state logic needed by later phases.

### R7 - D2D-on-D3D11 text pipeline

Decision:

- Use Direct2D-on-D3D11 for native text: create `ID2D1Device` /
  `ID2D1DeviceContext` from the shared DXGI device and bind an
  `ID2D1Bitmap1` over the swap-chain back buffer via `IDXGISurface`.
- Render DirectWrite text after the D3D geometry pass and before `Present`,
  caching `IDWriteTextLayout` objects by family, weight, size, DPI, and text.

Rationale:

- DirectWrite text remains sharp across DPI changes without introducing a
  second swap chain or a parallel UI compositor.
- Sharing the existing D3D11 device keeps lifetime and device-loss handling in
  the renderer that already owns the back buffer.

### R8 - Hit-test architecture

Decision:

- The native UI owns a DPI-scaled rect tree for title bar, nav, modal, drive,
  LED, settings, and viewport regions.
- `WM_NCHITTEST` consults that tree for caption, system button, resize-edge,
  and client classifications; mouse and drag/drop events use the same tree.

Rationale:

- One hit-test source prevents drift between Win32 non-client behavior and
  in-canvas widget behavior.
- Shared routing makes drag/drop, click-to-browse, and chrome input testable
  with synthetic rects.

### R9 - Focus and keyboard policy

Decision:

- A native `FocusManager` owns tab order, Shift+Tab reverse traversal,
  Space/Enter activation, Escape dismissal, and visible focus cues.
- Settings controls register focusable entries with stable IDs so machine
  switches and theme changes can rebuild layout without losing intent.

Rationale:

- FR-044 requires keyboard-only operation, and that cannot be bolted onto
  pointer-only widgets at the end.
- Stable focus IDs make the Settings panel deterministic under machine/theme
  reloads.

### R10 - Modal and popup layer

Decision:

- Menus, dropdowns, tooltips, and confirmation prompts render in a native
  overlay layer above chrome and settings content.
- `ModalScrim` captures input while active; non-modal popups close on outside
  click, Escape, command dispatch, or focus transfer.

Rationale:

- A single overlay stack avoids Win32 dialog/message-box reintroduction.
- Explicit capture semantics make reset prompts, cancel prompts, and menu
  dropdowns predictable in tests.

### R11 - Drag-drop carve-out

Decision:

- Drag/drop remains a Win32 `IDropTarget` service registered on the HWND, but
  accepted targets and actions are decided by the native hit-test tree.
- Click-to-browse uses `IFileOpenDialog` for disk image selection and routes
  confirmed paths through the same mount command path as drag/drop.

Rationale:

- Shell drag/drop and common file dialogs are OS services, not UI ownership
  paths, so retaining them does not violate the native-rendered UI boundary.
- Reusing command routing keeps drag/drop and browse behavior identical after
  a file path is chosen.

### R12 - Debug-tool carve-out

Decision:

- `DiskIIDebugDialog` and `DebugConsole` remain Win32-implemented and are not
  migrated by this feature.
- The native nav layer only replaces the menu surface that launches those
  tools; their existing windows and debug-only behavior stay intact.

Rationale:

- Debug tools are outside the user-facing chrome/settings scope and do not
  justify migration risk in this feature.
- Keeping the carve-out explicit prevents the no-dialog rule from being read
  as applying to approved debug surfaces.

### R13 - Theme-token broadcast

Decision:

- `ThemeManager` publishes an active-theme change event containing validated
  tokens, drive visual profile, CRT defaults, and backdrop flags.
- Chrome, settings, drive widgets, LEDs, and CRT controls subscribe and update
  their cached draw state before the first post-switch frame is presented.

Rationale:

- First-frame hot-swap acceptance requires a single broadcast boundary rather
  than per-widget polling.
- Validated token payloads keep malformed themes from partially applying.

## Risks and mitigations

1. Risk: text still looks poor at specific DPI/font combinations.

   - Mitigation: direct DWrite path for chrome text and multi-DPI visual checks.

2. Risk: NC/client routing regressions.

   - Mitigation: retain existing hit-test semantics and unit coverage.

3. Risk: nav command regressions.

   - Mitigation: reuse existing command registry and parity tests.

## Exit criteria for research

Research is complete when:

- Architecture clearly defines a single title/nav owner.
- Render split (DWrite text, D3D geometry) is fixed.
- Migration phases and rollback points are documented.
- Acceptance metrics are measurable and tied to tests/visual checks.
