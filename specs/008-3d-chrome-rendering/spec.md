# Feature Specification: 3D Chrome Rendering

**Feature Branch**: `008-3d-chrome-rendering`
**Created**: 2025-11-25
**Status**: Draft
**Input**: User description: "Introduce a real 3D rendering path for UI chrome elements (starting with the Apple Disk II drive widgets), replacing the current 2D scanline-approximation approach."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Drive Widgets Render as Real 3D Objects (Priority: P1)

When a user runs Casso and looks at the command bar, the two Apple Disk II drive
widgets appear as actual three-dimensional objects — beige cases with subtly
beveled edges, an inset faceplate carrying the drive's label, slot recess, and
door tab — drawn under a single shared perspective. Both drives share the same
camera, so the perspective foreshortening is visually consistent between them
rather than fighting hand-tuned per-widget skews.

**Why this priority**: This is the whole point of the feature. Without it, the
3D pipeline is dead code and the chrome regresses to a worse version of what we
already had. Everything else (animation, lighting polish, hit-test refinement)
layers on top of this baseline.

**Independent Test**: Launch the emulator, observe both drive widgets in the
command bar, confirm they are rendered as actual meshes (visible perspective
foreshortening on the case top, consistent vanishing point shared between the
two drives, faceplate visibly inset into the case). Drive labels ("DRIVE 1",
"DRIVE 2", "IN USE" text, cassowary mark, LED dot) remain legible and correctly
positioned on each faceplate.

**Acceptance Scenarios**:

1. **Given** the emulator is running with two disk drives configured,
   **When** the command bar is visible,
   **Then** both drive widgets are rendered as 3D meshes with a shared camera,
   each occupying the same screen footprint as the previous 2D widget within
   the command bar layout.
2. **Given** the user resizes the main window,
   **When** the command bar reflows,
   **Then** the drive widgets re-fit to the new command bar dimensions without
   distorting the case proportions or breaking the shared-camera perspective.
3. **Given** a drive is empty vs. has a disk inserted,
   **When** the widget is drawn,
   **Then** the correct faceplate label state ("DRIVE N" vs. "IN USE") is
   visible on the 3D faceplate surface and remains crisp at the current DPI.

---

### User Story 2 - Door Opens and Closes as a Real Hinge Animation (Priority: P2)

When the user inserts or ejects a disk, the drive's door tab rotates about its
top hinge edge rather than translating vertically. The animation is driven by
the existing `DriveWidgetState` finite state machine, so the open/close timing
and triggers are unchanged; only the visual interpretation moves from a 2D
translation hack to a real 3D rotation.

**Why this priority**: This is the most visible win after the baseline meshes
are in place, and it's the original motivating example for the refactor. It
also exercises the sub-node transform path of the new renderer, which other
future chrome (knobs, monitor bezel) will rely on.

**Independent Test**: Trigger an eject on an inserted drive and observe the
door tab swinging open about its top edge. Trigger a load and observe it
swinging closed. The animation timing matches the existing state machine's
durations; no vertical pop.

**Acceptance Scenarios**:

1. **Given** a drive in the `Closed` state with a disk loaded,
   **When** the user clicks the eject hit-region,
   **Then** the door sub-node rotates open around its hinge edge over the
   state machine's existing open duration, with the rotation visible as a
   true 3D pivot (the bottom edge of the door swings outward toward the
   camera).
2. **Given** a drive in the `Open` state,
   **When** the user triggers a load,
   **Then** the door rotates closed around the same hinge, ending flush
   with the faceplate.
3. **Given** the door is mid-animation,
   **When** the user clicks the body of the drive,
   **Then** the eject hit-region tracks the door's currently-projected
   screen position rather than its closed-state position.

---

### User Story 3 - Case Surfaces Show Simple Directional Lighting (Priority: P3)

The case top, faceplate, and bevels respond to a single directional light plus
ambient term, producing a visible highlight on the top face and slight shading
on the front bevel. The LED on the faceplate becomes an emissive sub-mesh so
that "IN USE" actually glows rather than just being a brighter swatch.

**Why this priority**: Cosmetic polish that makes the meshes read as solid
objects rather than flat-shaded geometry. Genuinely nice but not required for
the feature to be considered working; the baseline meshes plus animation
already deliver most of the value.

**Independent Test**: Visually inspect a drive widget: the top of the case is
brighter than the front, the front bevel reads as a distinct surface, and the
LED visibly glows when the drive is in the `InUse` state regardless of the
ambient light direction.

**Acceptance Scenarios**:

1. **Given** the default scene lighting,
   **When** a drive widget is rendered,
   **Then** the case top shows a visible Lambert highlight from the directional
   light and the front faceplate is darker but still readable.
2. **Given** a drive transitions into the `InUse` state,
   **When** the faceplate is rendered,
   **Then** the LED sub-mesh visibly emits light (does not depend on the
   directional term) and matches the existing "IN USE" color signal.

---

### Edge Cases

- Command bar is sized too small or too narrow to fit two drive widgets at the
  designed mesh aspect ratio — the renderer must fall back gracefully (clip
  rather than distort proportions; widgets must not bleed outside the command
  bar's clip rect).
- Window is minimized or has zero client area — the 3D pass must skip cleanly
  without device-context errors or depth-buffer allocation churn.
- Display scaling changes at runtime (DPI change) — faceplate render-to-texture
  must be re-rasterized at the new DPI so the labels stay crisp.
- D3D device is lost and recreated — the mesh registry, shaders, depth target,
  and faceplate render-textures must all be re-created without leaking
  resources or leaving widgets invisible.
- Camera placement places a drive partly behind the near plane at extreme
  command-bar sizes — clip without producing flickering or NaN screen-space
  bounds for hit-testing.
- Hit-testing during the door animation — the eject region must track the
  door's current projected quad each frame, not the start or end pose.
- Unit tests that previously asserted on axis-aligned rectangles must continue
  to assert meaningful geometry against the projected mesh, not be deleted.

## Requirements *(mandatory)*

### Functional Requirements

#### Rendering subsystem

- **FR-001**: Casso MUST provide a 3D mesh rendering subsystem for UI chrome,
  architecturally parallel to the existing 2D chrome painter, that owns its
  own vertex/pixel shaders, transform constant buffer, depth-stencil state,
  depth buffer, and mesh registry.
- **FR-002**: The mesh renderer MUST accept per-vertex position, normal, UV,
  and color attributes and MUST support indexed triangle meshes.
- **FR-003**: The mesh renderer MUST expose a shared camera (view +
  projection) used by all chrome meshes in a single frame, so that
  perspective is consistent across widgets without per-widget skew math.
- **FR-004**: The mesh renderer MUST support a single directional light plus
  an ambient term, evaluated per-pixel using surface normals, producing
  visible shading on the case top and front bevel.
- **FR-005**: The mesh renderer MUST support an emissive material channel that
  bypasses the directional/ambient lighting, used at minimum by the LED
  sub-mesh.
- **FR-006**: The mesh renderer MUST support sub-node transforms parented to a
  mesh's root transform, so that the door tab can rotate independently around
  its hinge edge while remaining attached to its parent case.

#### Disk II drive mesh

- **FR-007**: The system MUST construct the Apple Disk II drive mesh
  procedurally in code, with no external asset files. The mesh MUST include,
  at minimum: a case body (box with subtly beveled front edges), an inset
  faceplate, a slot recess, and a door tab as a separate sub-node.
- **FR-008**: The faceplate MUST be textured from an in-memory
  render-to-texture that carries the existing 2D drive labels ("DRIVE N",
  "IN USE", cassowary mark, LED indicator placement).
- **FR-009**: The door tab sub-node MUST rotate around its top (hinge) edge
  to represent the open and closed states, driven by the existing
  `DriveWidgetState` finite state machine and its existing transition
  timings. No vertical translation is used to represent door motion.
- **FR-010**: Both drive widgets MUST be rendered through the shared chrome
  camera in a single 3D pass.

#### Composition and integration

- **FR-011**: The chrome 3D pass MUST execute after the existing 2D backdrop
  pass and before the emulator viewport quad, writing into the same swap
  chain back buffer with depth testing enabled within the chrome pass.
- **FR-012**: `DriveWidget::Paint` MUST be replaced by a call into the new
  mesh renderer. The legacy local helpers `FillTrapezoidApprox`,
  `DrawCaseRidge`, `FillCircleApprox`, and any other scanline-perspective
  approximations in `DriveWidget.cpp` and `LedIndicator.cpp` MUST be removed
  once superseded.
- **FR-013**: The `EmulatorShell::LayoutDriveWidgetsInCommandBar` contract
  (its inputs, outputs, and the screen-space rectangle it assigns to each
  widget) MUST be preserved. The 3D mesh MUST fit itself into the rectangle
  the layout assigns.
- **FR-014**: The `DriveWidgetState` finite state machine MUST NOT be
  modified by this feature; only the visual interpretation of its states
  changes.
- **FR-015**: Hit-testing MUST remain in screen space. The widget body
  hit-test MUST use the projected bounding rectangle of the case mesh, and
  the eject hit-region MUST use the projected quad of the door sub-node's
  front face at its current animated pose.
- **FR-016**: Unit tests in `UnitTest/UiTests/DriveWidgetHitTests.cpp` and
  `UnitTest/UiTests/LedIndicatorStateTests.cpp` MUST be updated to assert
  against projected mesh geometry rather than the old axis-aligned
  approximations. Tests MUST NOT be deleted as a way of avoiding the update.
- **FR-017**: Code analysis (`scripts\Build.ps1 -RunCodeAnalysis`) MUST pass
  on the final state of this feature.

#### Lifecycle and robustness

- **FR-018**: The mesh renderer MUST handle D3D device loss and recreation
  by rebuilding shaders, depth target, mesh registry, and faceplate
  render-textures without leaking resources.
- **FR-019**: The faceplate render-to-texture MUST be re-rasterized when the
  effective DPI changes so that label text remains crisp at the new scale.
- **FR-020**: The work MUST be phaseable such that every intermediate commit
  on the feature branch leaves Casso buildable and runnable. The 2D Disk II
  visuals may be temporarily simpler during transition, but the application
  must continue to launch, run, and pass its existing unit tests at every
  commit.

### Key Entities

- **MeshRenderer**: Owns 3D shaders, transform constant buffer, depth
  resources, mesh registry, shared camera, and lighting parameters. Renders
  registered meshes in a single chrome pass per frame.
- **ChromeMesh**: A registered procedural mesh asset (vertex buffer, index
  buffer, material slots, optional sub-node skeleton). The Disk II mesh is
  the first instance.
- **ChromeCamera**: Shared view + projection covering all chrome widgets in
  the command bar for the current frame.
- **DiskIIFaceplateTexture**: Per-widget render-to-texture carrying the 2D
  drive labels (drive number, IN USE indicator, cassowary mark, LED
  placement). Sampled by the faceplate material.
- **DoorSubNode**: Sub-transform of the Disk II mesh that rotates around its
  top hinge edge in response to `DriveWidgetState` transitions.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Both Apple Disk II drive widgets are rendered as true 3D meshes
  under a single shared camera, with no per-widget perspective-skew math
  anywhere in the chrome code path.
- **SC-002**: All scanline-perspective approximation helpers
  (`FillTrapezoidApprox`, `FillCircleApprox`, `DrawCaseRidge`, and any
  equivalent local helpers in `DriveWidget.cpp` and `LedIndicator.cpp`) are
  removed from the codebase once the 3D path supersedes them.
- **SC-003**: Door open/close is implemented as a rotation of the door
  sub-node around its hinge edge; no vertical translation is used to
  represent door motion.
- **SC-004**: The `DriveWidgetState` FSM and the public contract of
  `EmulatorShell::LayoutDriveWidgetsInCommandBar` are unchanged by this
  feature (no signature changes, no semantic changes to layout output).
- **SC-005**: All unit tests in `UnitTest/UiTests/DriveWidgetHitTests.cpp`
  and `UnitTest/UiTests/LedIndicatorStateTests.cpp` continue to exist and
  pass against the projected 3D geometry. No test is deleted as a means of
  avoiding the geometry update.
- **SC-006**: `scripts\Build.ps1 -RunCodeAnalysis` passes on the final state
  of the feature branch with no new warnings attributable to the new mesh
  renderer code.
- **SC-007**: Every commit on the feature branch builds and the application
  launches successfully — there is no "broken middle" commit.
- **SC-008**: Drive faceplate labels (drive number, IN USE, cassowary,
  LED indicator) remain readable at the standard command bar height and at
  both 100% and 200% display scaling.
- **SC-009**: With both drive widgets visible, vanishing-point lines drawn
  along corresponding case edges across the two drives converge consistently
  — i.e., the two drives demonstrably share a single camera, not two
  independent ones.
- **SC-010**: The eject hit-region accepts clicks on the door's currently
  projected screen footprint at any point during its open/close animation,
  not only at the closed-state footprint.

## Assumptions

- D3D11 remains the rendering API; this feature does not introduce D3D12,
  Vulkan, or any cross-API abstraction.
- The existing `DxUiPainter` 2D quad batcher remains in place and continues
  to own 2D label, icon, and backdrop drawing. This feature does not
  refactor it.
- No external 3D asset pipeline is introduced. All meshes are constructed
  procedurally in code; all textures used by the chrome 3D pass are either
  in-memory render-to-textures or already-loaded 2D chrome assets.
- HDR, PBR, normal mapping, and shadow mapping are explicitly out of scope.
  Lambert plus ambient plus a single emissive channel is the entire
  material model for this feature.
- The 3D chrome pass runs only within the command bar region. Animated 3D
  rendering anywhere else in the UI (e.g., the emulator viewport itself) is
  out of scope.
- The existing depth-capable swap chain target is sufficient infrastructure
  to host the chrome depth resources; whether to reuse the swap chain depth
  buffer or allocate a dedicated chrome depth buffer is left to the plan
  phase (see Open Questions).
- The cassowary mark stays sourced from its existing 2D representation;
  whether it is sampled as a texture on the faceplate or projected as a
  decal is left to the plan phase.

## Open Questions

These are intentionally left open for the `/speckit.clarify` phase rather
than guessed at here, because each one materially shapes the implementation:

- **OQ-1 (Camera)**: Perspective with a low field of view, or orthographic?
  A low-FOV perspective gives mild foreshortening that reads as 3D; ortho
  guarantees pixel-stable widget sizing across command-bar resizes. Pick
  one and pin the FOV / ortho-fit rule before Phase B.
- **OQ-2 (Depth buffer)**: Share the main swap chain depth buffer for the
  chrome pass, or allocate a dedicated, smaller depth buffer scoped to the
  command-bar region? Sharing is simpler; dedicated avoids any interaction
  with the emulator viewport's depth state.
- **OQ-3 (World-unit sizing)**: How are Disk II mesh dimensions expressed in
  world units, and how is the mesh fit to the existing 192 dp command-bar
  height? Define the world-unit-to-dp mapping (or the camera fit rule that
  derives it).
- **OQ-4 (DPI / text crispness)**: How is the faceplate render-to-texture
  sized and re-rasterized across DPI changes so labels stay crisp without
  reallocating textures every frame?
- **OQ-5 (Cassowary)**: Is the cassowary a flat texture sample on the
  faceplate, or is it a true 3D decal/sub-mesh sitting slightly above the
  faceplate surface?
- **OQ-6 (Hit-test precision)**: Is the widget body hit-test the projected
  AABB of the case mesh, or the projected convex hull? The eject hit-region
  is unambiguous (projected door front quad), but the body region trades
  precision for code simplicity.
