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
- Validate at 100/125/150/200 percent scaling.

Rationale:

- Current failures are mostly placement quality problems.
- DPI invariants need to be explicit and testable.

### R6 - Migration strategy

Decision:

- Stage cutover in four phases:

  1. Native scaffold behind gate
  2. Title ownership cutover
  3. Nav ownership cutover
  4. Dead-path cleanup

Rationale:

- Keeps rollback points.
- Prevents another large, opaque integration jump.

### R7 - Chrome/nav font policy

Decision:

- Use the same font family/weight/stretch as standard Windows 11 UI text
  for title bar and nav/menu text.
- Do not use theme-specific chrome font substitutions (e.g., Inter, VT323)
  for native title/nav surfaces.
- Differentiate themes via color, gradient, shading, depth, and geometry;
  not via chrome font family changes.

Rationale:

- Visual consistency with the OS is a hard product requirement.
- Removes font-style variability as a source of perceived text quality issues.

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
