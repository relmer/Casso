# Feature Specification: Drag-Drop Blur and Target Glow

**Feature Branch**: `010-dragdrop-blur`
**Created**: 2026-05-26
**Status**: Draft (deferred — depends on Gaussian shader + textured-quad primitive in DxUiPainter)
**Input**: Planning draft for two related drag-drop visual upgrades requiring
shader + offscreen-render-target plumbing in `DxUiPainter`:

1. **Blur**: The drag-drop overlay currently dims non-drop regions with a flat
   alpha-tinted rectangle. Upgrade it by snapshotting the framebuffer to an
   offscreen render target, running a separable Gaussian blur (either by
   repurposing the existing `bloom_h` / `bloom_v` shaders in `CrtPostProcess` or
   by authoring dedicated blur shaders), then compositing the blurred copy over
   non-drop regions only. Drop targets sample from the un-blurred original so
   they stay in focus.
2. **Glow**: Replace the placeholder "drive widget is un-dimmed + hover tint"
   treatment with a real radial-gradient glow behind each drop target. The glow
   may come from a precomputed radial-falloff alpha texture shipped as
   `RT_RCDATA` and drawn as a textured quad with multiply / add blend, or from a
   fragment shader that computes attenuation in real time with each drive center
   treated as a point light.

Both items want the same new painter primitive: `DrawTexturedQuad` with an
explicit blend state. Shipping them together amortizes the plumbing cost, which
is the least glamorous kind of bargain but still a bargain.

## Overview

This is a planning draft, not a current implementation spec. It records the
shape of the desired drag-drop polish while deliberately deferring execution
until `DxUiPainter` can render textured quads with selectable blend states and
until the shader/offscreen-RT path for Gaussian blur is available.

When the user drags a disk image into the emulator window, the chrome should
feel like it recedes and softens. Non-drop regions blur and dim, while valid
drop targets remain crisp and invite the drop with a warm radial glow. The net
effect should be obvious without shouting, which is apparently legal UI design.

The feature has two coupled visual pieces:

1. **Blurred non-drop regions** — the current flat dim overlay becomes a blurred
   composite over the parts of the window that are not accepting the drop.
2. **Glowing drop targets** — drive widgets receive a radial glow whose intensity
   ramps with drag hover / active state.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Non-drop chrome softens during disk drag (Priority: P2)

A user drags a `.dsk`, `.po`, `.nib`, or `.woz` file over the Casso window. The
parts of the chrome that are not valid drop targets blur and dim, while drive
widgets remain sharp. The user can immediately tell where the disk can land.

**Why this priority**: This is visual guidance for the most common direct disk
interaction. It also proves the offscreen-RT blur path is usable by chrome, not
just CRT post-processing.

**Independent Test**: Start a drag with a valid disk image over the window;
confirm non-drop regions are blurred/dimmed, drive widgets remain unblurred, and
existing drag-drop hit testing still routes the eventual drop to the intended
drive.

**Acceptance Scenarios**:

1. **Given** the user drags a valid disk image over the emulator window,
   **When** the drag overlay paints,
   **Then** non-drop regions are composited from a blurred framebuffer snapshot
   with dimming applied.
2. **Given** drive widgets are valid drop targets,
   **When** the drag overlay paints,
   **Then** those widgets sample from the un-blurred original framebuffer and
   remain visually crisp.
3. **Given** the user leaves the window or cancels the drag,
   **When** the drag operation ends,
   **Then** all blur resources and overlay state are released or marked idle and
   the next frame is indistinguishable from the pre-drag frame.

---

### User Story 2 — Drop targets glow with hover intent (Priority: P2)

A user drags a disk over Drive 1. A warm radial glow behind Drive 1 ramps up as
the pointer enters its drop zone; Drive 2 remains available but less intense.
Moving to Drive 2 transfers the active glow. Dropping mounts the disk exactly as
it does today.

**Why this priority**: The glow replaces the current placeholder treatment with
a clear target affordance, while preserving the existing input behavior. The
paint gets nicer; the hit testing stays boring. Boring is how input should be.

**Independent Test**: Drag a valid disk over each drive widget in both flat and
skeuomorphic themes; confirm inactive, hover, and active intensities are visible
and that dropping still mounts the disk in the hovered drive.

**Acceptance Scenarios**:

1. **Given** a valid drag is inside the window but not over a drive,
   **When** the overlay paints,
   **Then** all valid drive targets show a low-intensity glow.
2. **Given** the pointer moves over Drive 1,
   **When** hover state changes,
   **Then** Drive 1's radial glow ramps to the hover intensity without a one-frame
   pop, and other drives remain at the inactive target intensity.
3. **Given** the user drops on a glowing drive target,
   **When** the drop is accepted,
   **Then** the existing mount behavior runs unchanged and the overlay fades or
   clears without altering drop routing.

## Requirements

### Functional Requirements

- **FR-001**: `DxUiPainter` provides a `DrawTexturedQuad`-style primitive that
  can draw a shader-resource view into an arbitrary destination rectangle with
  explicit blend-state selection.
- **FR-002**: Drag overlay rendering can snapshot the current framebuffer into an
  offscreen render target before overlay effects are applied.
- **FR-003**: The blur path performs separable horizontal and vertical Gaussian
  passes against the snapshot render target, using either repurposed
  `CrtPostProcess` bloom shaders or dedicated chrome blur shaders.
- **FR-004**: The final blur composite is masked so blurred pixels are applied
  only to non-drop regions; valid drop-target rectangles render from the
  un-blurred original framebuffer.
- **FR-005**: Drop targets can render a radial glow behind the drive widget using
  either a bundled radial-falloff texture or a shader-computed attenuation field.
- **FR-006**: Glow intensity supports at least inactive-target, hover, and active
  states, with frame-to-frame ramping rather than instantaneous jumps.
- **FR-007**: The overlay effect uses the existing drag-drop hit-test source of
  truth; no duplicate target geometry may be introduced solely for painting.
- **FR-008**: The effect is disabled cleanly when required GPU resources cannot
  be allocated, falling back to the current flat dim / hover tint treatment.

### Non-Functional / Out of Scope

- The effect must stay within the current 60 Hz frame budget on supported GPUs;
  a pretty drag overlay is not worth making the emulator feel like molasses.
- The visual treatment must work in both skeuomorphic and flat themes.
- Existing drag-drop hit testing, accepted file types, and mount behavior must
  not regress.
- Changing drag-drop semantics, adding new disk formats, or redesigning drive
  widget layout is out of scope.
- This draft does not promise an implementation date.

## Design Notes

- The blur should be fed from a pre-overlay framebuffer snapshot. Blurring after
  target glows are drawn would smear the affordance that is supposed to remain
  crisp. Computers are literal; they will absolutely do the wrong thing quickly.
- The blur mask can be represented as destination rectangles for the non-drop
  regions, as a stencil/mask pass, or as shader-side region tests. The plan phase
  should choose based on the final `DxUiPainter` primitive shape.
- The glow must sit behind drive widgets but above any monitor-frame center layer
  that visually surrounds the emulator viewport. Z-order matters here: inviting
  a drop behind the furniture is not useful.

## Dependencies

- **Hard**: spec 007 (`007-ui-overhaul`) must provide the `DxUiPainter` primitive
  set needed for textured-quad rendering, blend-state selection, and custom
  chrome composition.
- **Hard**: Gaussian shader plumbing and offscreen render-target management must
  exist in or be exposed through the chrome painter path before this spec can be
  implemented.
- **Soft**: spec 009 (`009-monitor-frames-and-dockable-chrome`) defines chrome
  layout center layers and monitor-frame z-order. The glow must compose in the
  correct layer relative to the monitor frame.

## Open Questions

1. Should the radial glow ship as a precomputed texture asset or be generated in
   a fragment shader from drive-center coordinates?
2. Should blur radius be fixed per theme, globally configurable, or selected by
   quality tier?
3. Do skeuomorphic and flat themes need distinct glow palettes / falloff curves,
   or is one neutral treatment enough?
4. What is the keyboard-equivalent / accessibility affordance for the same drop
   guidance, and should it expose any ARIA-style state if the UI layer grows an
   accessibility bridge later?
