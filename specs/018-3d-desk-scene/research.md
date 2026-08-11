# Research: 3D Desk Scene (Parity Phase)

Decisions grounded in a full architecture survey of the current rendering, input, theme,
and fullscreen paths (file/line references verified on this branch at `7c3ff041`).

## R1. Rendering seam — main-window before-present hook, shared device

**Decision**: The desk scene renders inside the main window's existing before-present hook
(`DxuiRenderTarget::PaintPump` → hook → panel-tree paint), on the same D3D device
`D3DRenderer` already adopts from the host. `EmulatorShell::InitializeRenderer`'s single
hook lambda (`EmulatorShell.cpp:1062`) becomes: run the CRT chain to offscreen, then draw
the desk scene (which samples the CRT result on the glass).

**Rationale**: The hook slot is exactly where the emulator frame composites today, so the
scene inherits correct z-order (under all Dxui chrome — tooltips, menus, dialogs paint on
top). Sharing the device means the CRT chain's SRVs bind directly to the mesh pass with no
GPU→CPU→GPU copies. `SetBeforePresentHook` is a single `std::function`; composing inside
the one lambda we already own avoids growing Dxui a hook-chain mechanism.

**Alternatives considered**: (a) Printer pattern — separate `DxuiWindow` with its own
device/swap chain (`PrinterPanel.cpp:373-411`): wrong here, the display SRV lives on the
main device and cross-device sharing is needless complexity. (b) Dxui hook chain: more
framework surface for no benefit; one owner already exists.

## R2. Display-on-glass — CRT chain renders to offscreen SRV; renderer gains SetContentSrv

**Decision**: Add a render-to-offscreen mode to `CrtPostProcess::Process`: the final
`copy` pass targets a dedicated RT (sized to the aspect-fit rect) instead of the back
buffer. `Dxui3DRenderer` gains `SetContentSrv(ID3D11ShaderResourceView*)` alongside the
existing CPU-upload `UpdateContentTexture`, plus per-vertex UV support on the textured
path. The glass sub-mesh samples the CRT output; glass triangles get white tint so the
baked-Lambert `tex * col` shader passes the picture through unmodified.

**Rationale**: The whole 9-pass CRT chain (brightness → scanlines → bloom → bleed →
persistence → gamma) keeps working untouched — the picture on the glass is the *finished*
CRT image, so scanlines/bloom curve with the glass exactly like a real tube. The existing
`ComputeAspectFitRectInRect` fit becomes UV-space on the mesh.

**Alternatives considered**: (a) Sample the raw 560×384 framebuffer on the glass and run
CRT post afterward: post passes are screen-space and would smear across the monitor body.
(b) CPU upload of the CRT result via `UpdateContentTexture`: pointless GPU→CPU→GPU round
trip of a full-res image every frame.

## R3. Glass/lamp/sub-mesh identification — Kd color match (precedented), not parser changes

**Decision**: Identify sub-meshes (glass, lamps, slot regions' visual anchors) by material
`Kd` value with ±0.02 epsilon, exactly as `Printer3DScene::SetModel` does
(`Printer3DScene.cpp:478-493`). The glass Kd (0.05/0.09/0.07) becomes a named constant
shared with the generator.

**Rationale**: `ObjMeshParser` drops `g`/`o`/material names by design, and the
Tinkercad-dialect constraint exists so a hand-refined Tinkercad export (which emits
`color_<decimalRGB>` names, not semantic ones) can replace any generated model without
loader changes. Color-keyed identity survives that replacement; name-keyed identity would
break it. The approach has shipped once already (printer LEDs/platen/badge).

**Alternatives considered**: Extending parser + generators to carry semantic material
names — rejected because it silently breaks the Tinkercad-replacement contract recorded in
`scripts/modelgen/README.md`.

## R4. Glass UVs — synthesized planar projection, exact for a spherical-sag sheet

**Decision**: Synthesize UVs for glass vertices by planar projection onto the glass
sub-mesh's own XZ bounding rect (model space: X right, Z up, sag along −Y):
`u = (x−x0)/(x1−x0)`, `v = 1−(z−z0)/(z1−z0)`.

**Rationale**: The generator displaces glass vertices only along Y, so XZ planar
projection is *exact* — every emulated pixel maps to precisely one glass point and the
grid spacing matches the flat display. No parser/generator UV support needed
(`ObjMeshParser` has no `vt` slots; generators emit none).

**Alternatives considered**: Spherical (angular) UVs — would nonuniformly stretch the
picture; wrong, since a real CRT's phosphor grid is laid on the curved glass, not
projected through sphere angles.

## R5. Input inverse-projection — analytic ray↔sphere in core, replacing the viewport mapping

**Decision**: New pure math in `CassoEmuCore/Render/CurvedDisplayMath`: screen px →
NDC → inverse view-proj → world ray → analytic ray-sphere intersection against the glass
sphere (radius `half_diag * 2.2`, center derived from the glass rect) → clamp to the glass
rect in model space → UV → emulated pixel (and the exact forward transform for tests).
When the scene is active this *replaces* `EmulatorShell::UpdateGuestMouseFromHost`'s
`MulDiv`-against-viewport mapping (`EmulatorShell.cpp:6314-6315`).

**Rationale**: No unprojection code exists anywhere in the repo (verified by search) — this
is net-new and belongs in testable core per Constitution VI. Analytic intersection is
exact and cheap (one quadratic per event, no mesh raycast needed since the sag *is* a
sphere section). Round-trip property tests (pixel → world → pixel) pin accuracy to the
spec's one-pixel bar. The current mapping also has a latent bug — it maps against the
viewport rect, not the aspect-fitted picture rect, so letterbox bars count as picture; the
replacement fixes this rather than inheriting it.

**Alternatives considered**: Rasterized ID/UV pick buffer — a whole extra render pass and
GPU readback for something a closed-form quadratic answers.

## R6. Device/region hit testing — analytic ray vs model-space region boxes

**Decision**: Each device model declares its interactive regions as axis-aligned boxes in
model space (Disk II: slot region, body; monitor: none this phase). Hit testing casts the
screen ray into each device's model space (inverse of its world transform) and slab-tests
the region boxes, nearest hit wins; glass intersection (R5) outranks device boxes where
they overlap. Region semantics map 1:1 onto today's `DriveWidgetRegion { None, Body,
Eject }` so `EmulatorShell::OnLButtonUp`'s existing routing (Body → browse, Eject →
eject+browse, `EmulatorShell.cpp:6623-6641`) is unchanged.

**Rationale**: Keeps the widget→shell command contract (`IDriveCommandSink`) and the
tooltip/write-protect flow (`DriveWidgetState`, `ComposeWriteProtectTooltip`) fully
intact — parity by construction. Slab tests are pure math, trivially unit-tested against
known camera setups.

**Alternatives considered**: Screen-space projected rects (project region corners, hit
test 2D): breaks at glancing angles on curved/skewed devices and needs per-frame
recomputation; ray-vs-box is exact and stateless.

## R7. Scene layout, zoom, and the Ctrl+0 inverse

**Decision**: `DeskSceneLayout` computes device world transforms + camera from the
viewport rect and machine config (drive count from the machine, per FR-003), echoing
today's composition (monitor centered, drives below in the band position). Scene scale =
glass on-screen height ÷ native 384 dp, replacing `MonitorFrame::SceneScale()`; the
effective-DPI fold and 3-pass fixed-point settle in `EmulatorShell` (`:240`, `:2338-2343`)
are retired along with the band — the scene owns all skeuo geometry, so the circular
dependency disappears. `SceneCamera` provides the closed-form inverse (window size for a
requested glass px size) to keep Ctrl+0 / reset-to-100% working
(`MonitorFrame::CenterSizeForScreenPx`'s replacement).

**Rationale**: With one camera and fixed placements, "what window size makes the glass
exactly native scale" is a solve on the perspective fit — closed-form, testable, and
load-bearing for an existing command that must not regress. The single shared camera is
also a spec requirement in its own right (FR-016, added 2026-08-10): device perspective
MUST derive from true position in the one scene — right of center reads from the left,
below from above — with per-device cameras and billboarding prohibited, so dock-position
moves in the follow-on spec change perspective automatically. The fullscreen strip is its
own composition and applies the same rule within itself.

**Alternatives considered**: Keeping the 2D band + effective-DPI machinery under the
scene — contradicts the supersession clarification and preserves a fixed-point loop that
exists only to reconcile two layout systems.

## R8. Fullscreen presentation branch — new, glass-fills-screen camera + overlay strip

**Decision**: Introduce the first fullscreen presentation branch: when fullscreen and
skeuo, the scene switches to a glass-only camera (FOV/eye solved so the glass rect fills
the monitor, monitor body cropped offscreen) and all Dxui chrome bands are hidden. The
drive overlay strip is the same drive device objects rendered in a docked strip
composition, driven by `FullscreenStripState` — a pure FSM covering: edge-reveal arming
(host-owns-pointer only), hotkey summon (guest capture: release → interact → restore),
auto-hide on pointer-leave, pinned-while-tooltip/browse, and the hidden-state activity
indicator. Hotkey registered through the existing accelerator/command path
(`WindowCommandManager`).

**Rationale**: Today fullscreen hides nothing (verified — no `IsFullscreen()` reader
alters presentation; chrome bands all paint at monitor size), so this is green-field and
can be built clean. Making the strip the *same* device objects (not a 2D remake)
preserves interaction parity for free and exercises one code path. The FSM's
capture-interplay rules (edge-reveal disabled under guest capture; restore-on-dismiss) are
exactly the kind of sequencing that must be property-tested, hence a pure state machine.

**Alternatives considered**: Reusing the 2D compact drive cards for the strip — a second
parity surface to maintain and test, and it contradicts supersession.

## R9. Perf model — present gating preserved, geometry cached, animation marks redraw

**Decision**: Static scene geometry (device meshes, transforms, glass UVs) is built once
at load/layout and cached; per-frame work is bind + draw. `D3DRenderer::NeedsPresent`
gating stays authoritative; animated scene elements (drive LED glow, door transitions,
strip slide, activity indicator) call `MarkRedrawNeeded()`, mirroring how door animations
force presents today (`TryPresentUiFrame` `:4630-4660`).

**Rationale**: SC-002 requires no perceivable regression; the frame-skip path on static
screens is the biggest existing power/perf win and must not be defeated by an
always-animating scene. `Printer3DScene` rebuilds geometry per frame — acceptable in a
popup, not in the every-frame main window path.

**Alternatives considered**: Per-frame rebuild (printer pattern) — measurable waste on the
hot path for zero flexibility we need this phase.

## R10. 2D path retirement and pref migration

**Decision**: After parity validation (the polish phase's scripted walkthrough),
`MonitorFrame` is deleted, `DriveWidget`'s skeuo (non-compact) paint path is removed
(compact path stays for the other themes), and the `skeuoMonitorFrame` pref + its Settings
toggle are retired (stale JSON key ignored on load; Settings > Theme page loses the
checkbox). `DriveWidgetState`, `DriveWidgetController`, `IDriveCommandSink`, and
`ComposeWriteProtectTooltip` are retained unchanged — they are the state/command layer the
scene consumes.

**Rationale**: The supersession clarification mandates no fallback and no toggle. The
state layer was deliberately built UI-agnostic (atomics for CPU-thread flags, pure door
FSM) and is exactly the parity contract; deleting only the paint layer keeps the
blast radius small and the tests (`DriveWidgetStateTests`) authoritative.

**Alternatives considered**: Deleting `DriveWidget` entirely — wrong, compact themes use
it; keeping `MonitorFrame` dormant — dead code plus a stale pref pathway, contradicts the
clarification.
