# Feature Specification: Dockable Drive Bar and Monitor Frames

**Feature Branch**: `009-monitor-frames-and-dockable-chrome` (proposed)
**Created**: 2026-05-25
**Status**: Draft (deferred — depends on the chrome layout manager shipped in 007)
**Input**: User-described future capabilities that build on the chrome layout
manager primitive (`ChromeLayout`, `IChromeContributor`, edge slots, center
layers) shipped in spec 007.

## Overview

Spec 007 ships a `ChromeLayout` core that resolves edge-slot reservations and
center-wrapping layer paddings into a canonical `{topPx, bottomPx, leftPx,
rightPx, centerRect}` snapshot. It ships with the drive bar nailed to the
bottom edge and zero registered center layers. This spec consumes that
foundation to deliver two long-promised capabilities:

1. **The drive bar becomes dockable** to any of the four window edges (top,
   bottom, left, right), driven by user config and — eventually — drag
   gestures.
2. **The Skeuomorphic theme grows a monitor-frame center layer** that wraps the
   emulator viewport in a realistic CRT bezel selected per-machine or
   per-user-override.

Neither capability requires re-architecting chrome sizing math — they slot into
`ChromeLayout` as additional contributors and inherit its single-source-of-truth
sizing discipline.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Drive Bar Docks to Any Edge (Priority: P1)

When the user opens Settings → Display, they see a "Drive bar position"
dropdown with the four cardinal directions (top, bottom, left, right). Picking
a different edge moves the drive bar to that edge immediately; the emulator
content area stays the same pixel size and the window resizes to accommodate
the new chrome layout.

**Why this priority**: Direct user-facing payoff and the simplest way to prove
the layout manager's slot-reassignment plumbing works end-to-end.

**Independent Test**: Launch with the active theme set to DarkModern; open
Settings → Display; pick each of the four edges in turn; confirm the drive bar
relocates, the emulator pixel area is preserved, the window resizes
appropriately, and Ctrl+0 still snaps to a clean integer scale relative to the
new chrome geometry.

**Acceptance Scenarios**:

1. **Given** the drive bar is docked at Bottom,
   **When** the user picks Left in Settings → Display → Drive bar position,
   **Then** the drive bar repositions to the left edge, drives stack
   vertically with their LEDs visible, the emulator content area shifts right
   without changing its pixel dimensions, and the window resizes by the
   delta between the old bottom inset and the new left inset.
2. **Given** the active theme is Skeuomorphic (which only allows Bottom),
   **When** the user opens Settings → Display → Drive bar position,
   **Then** the Left/Right/Top options are disabled with a tooltip explaining
   the active theme allows only Bottom.
3. **Given** the user has selected Left,
   **When** they close and relaunch Casso,
   **Then** the drive bar reappears at the left edge.

---

### User Story 2 — Drag-to-Dock the Drive Bar (Priority: P3)

When the user drags the drive bar's caption / handle region, drop zones light
up at each edge of the window. Releasing over a zone redocks the drive bar to
that edge.

**Why this priority**: Polish UX. P1 already exposes the capability through
Settings; drag is the discoverable surface.

---

### User Story 3 — Monitor Frame Wraps the Emulator (Priority: P2)

When the active theme is Skeuomorphic, the emulator image renders inside a
realistic CRT bezel that visually matches the active machine (Apple ][, ][+,
//e, //c, etc.). The bezel contributes uneven padding on all four sides of
the emulator viewport — typically heavier on the bottom for the brand logo
and power LED — and the window grows to accommodate the bezel while keeping
the emulator pixel grid unchanged.

**Why this priority**: Highest-impact visual identity payoff. The whole
skeuomorphic theme story benefits dramatically.

**Independent Test**: Switch the active machine between Apple ][ and Apple //c
under the Skeuomorphic theme; confirm the bezel art changes; confirm the
emulator pixel grid is unchanged across the switch; confirm the window
resizes to absorb the bezel padding without distorting the framebuffer.

**Acceptance Scenarios**:

1. **Given** Skeuomorphic theme is active and the machine is Apple ][,
   **When** the emulator window paints,
   **Then** the framebuffer is wrapped by an Apple ][-style monitor bezel
   with non-uniform padding (heavier bottom), the emulator pixel grid is
   unchanged, and the window's overall size reflects the bezel padding plus
   chrome insets.
2. **Given** the user picks a different monitor profile in Settings → Display
   → Monitor frame,
   **When** the panel closes,
   **Then** the bezel updates to the new profile without restarting the
   emulator.
3. **Given** the DarkModern or RetroTerminal theme is active,
   **When** the emulator window paints,
   **Then** no monitor frame is rendered (those themes register no monitor
   center layer); the chrome is flat around the emulator viewport.

---

### User Story 4 — Machine Picks a Default Theme (Priority: P3)

The active machine config carries an optional `defaultTheme` field. On first
load of a machine for a given user (or when the user has never explicitly
chosen a theme), Casso activates the machine's default theme instead of the
global default. Explicit user choices override and persist.

**Why this priority**: Polish. Decouples "which machine am I emulating" from
"which theme do I want to look at" while giving sensible defaults.

---

## Requirements

### Functional Requirements

- **FR-001**: `ChromeLayout` supports moving an edge-slot contributor from one
  edge to another without recreating the contributor.
- **FR-002**: `ChromeTheme` carries a `driveBar.allowedSlots` set; the Settings
  drive-bar-position dropdown grays out disallowed slots for the active theme.
  Skeuomorphic permits only Bottom; DarkModern and RetroTerminal permit all
  four edges.
- **FR-003**: Drive-bar slot assignment persists in `GlobalUserPrefs` under
  `chrome.dockSlots.driveBar`.
- **FR-004**: The compact `DriveWidget` paint path adapts to the perpendicular
  axis of its slot — drives stack horizontally on top/bottom edges, vertically
  on left/right edges. LED placement and label legibility are preserved.
- **FR-005**: `ChromeLayout` supports an `ICenterLayer` interface contributors
  can implement to reserve `{topPad, bottomPad, leftPad, rightPad}` around the
  center rect. Padding values are non-uniform per layer.
- **FR-006**: A `MonitorFrame` center layer paints a per-profile CRT bezel
  using image assets bundled in the theme.
- **FR-007**: Monitor frame profile is resolved with the precedence:
  user override → theme override → machine default → none.
- **FR-008**: Activating a theme or machine that changes the resolved monitor
  frame triggers a single `ChromeLayout::OnLayoutChanged` event; the host
  resizes the HWND by the inset/padding delta to preserve the emulator pixel
  grid.
- **FR-009**: Machine config gains an optional `defaultTheme` string field.
  When a user has never explicitly picked a theme for a machine, the machine's
  default is activated; explicit choices recorded in `GlobalUserPrefs` take
  precedence on subsequent launches.

### Non-Functional / Out of Scope

- Floating undocked drive bar (separate window) is explicitly out of scope.
- Tab-grouped chrome panels are out of scope.
- Splitter panes between chrome and emulator are out of scope.
- Custom user-authored monitor frames (vs picking from bundled profiles) are
  out of scope; a later spec may add an authoring path.

## Design Notes

- The 007 layout manager defines `IEdgeSlot` and `ICenterLayer` interfaces; the
  drive bar already implements `IEdgeSlot`, and this spec adds the first
  `ICenterLayer` implementation (`MonitorFrame`). No changes to `ChromeLayout`
  itself are required beyond a slot-reassignment helper for FR-001.
- Window-size math is unchanged in shape; it is already
  `window = NC overhead + edge insets + Σ center layer paddings + emulator
  pixel grid`. This spec adds non-zero `Σ center layer paddings` for the first
  time but the formula is the same.
- Drag-to-dock (US2) layers on top of the Settings-driven dock path. It
  reuses the same `ChromeLayout::ReassignSlot()` operation; only the gesture
  recognizer and drop-zone visualization are new.

## Dependencies

- **Hard**: the `ChromeLayout`, `IChromeContributor`, edge-slot registration,
  center-layer interface, and theme-listener resize orchestration shipped in
  spec 007 must be merged before this spec can begin.
- **Soft**: spec 008 (3D chrome rendering) is orthogonal but compatible — the
  monitor frame center layer could be rendered through the 3D pipeline once it
  lands, or via the existing 2D painter, at the implementer's discretion.
