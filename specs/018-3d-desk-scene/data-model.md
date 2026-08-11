# Data Model: 3D Desk Scene (Parity Phase)

All entities are in-memory; nothing new is persisted this phase. Names are working names —
final code follows project naming rules.

## DeskSceneLayout

Computes and holds the scene composition for the current frame context.

| Field | Type | Notes |
|---|---|---|
| viewportRectPx | RECT | Host client-px rect the scene renders into |
| dpi | UINT | Effective DPI (no more scene-scale fold; scene owns zoom) |
| deviceCount | int | Drives from machine config (0-2 this phase) |
| devicePlacements | DevicePlacement[] | World transform per device, monitor first |
| camera | SceneCamera | View + projection for the composition |
| sceneScale | float | Glass on-screen height ÷ native 384 dp (replaces `MonitorFrame::SceneScale`) |

- **Validation**: placements deterministic for a given (rect, dpi, config); scene never
  overflows the viewport rect ("contain, not crop"); exactly ONE camera — device
  perspective derives from true world placement under it (FR-016), never from per-device
  cameras or billboarding, so dock-position moves (follow-on spec) change perspective
  automatically.
- **Transitions**: rebuilt on resize/DPI/fullscreen/machine change; cached otherwise (R9).

## DevicePlacement

| Field | Type | Notes |
|---|---|---|
| kind | DeviceKind { Monitor2c, DiskII } | This phase |
| driveIndex | int | 0/1 for drives; -1 for monitor |
| world | float[16] | Model→world transform (position, uniform scale) |

## DeskSceneModel (per device kind, loaded once)

| Field | Type | Notes |
|---|---|---|
| opaqueMesh / glassMesh | vertex+index arrays | Split by Kd match (R3); glass only on Monitor2c |
| glassSurface | CurvedDisplaySurface | Monitor only |
| lampAnchors | LampAnchor[] | Power lamp (monitor), activity LED (drive), by Kd match |
| regionBoxes | RegionBox[] | Model-space AABBs: Slot, Body (drives only) |

- **Validation**: model load fails soft (scene refuses to activate → assert in debug);
  glass discovery must find exactly one connected glass sheet on Monitor2c.

## CurvedDisplaySurface

The glass sheet and its mapping — the heart of FR-001/FR-002.

| Field | Type | Notes |
|---|---|---|
| rectModel | {x0,x1,z0,z1} | Glass bounding rect in model space (XZ) |
| sphereRadius | float | `half_diag * 2.2` (matches generator) |
| sphereCenter | float3 | Derived: sag axis −Y through rect center |
| baseY | float | Front plane the sag is relative to |

- **Operations** (pure, both directions, unit-tested round-trip):
  - `UvFromModelPoint` / `ModelPointFromUv` — planar XZ mapping (R4)
  - `IntersectRay(origin, dir)` → model point or miss — analytic ray-sphere + rect clamp (R5)
  - `EmulatedPixelFromUv` / `UvFromEmulatedPixel` — 560×384 grid mapping

## DriveVisualState (existing — consumed, not changed)

`DriveWidgetState` (`Casso/Ui/DriveWidgetState.h:46`) is reused as-is: `mountedImagePath`,
atomics `motorOn`/`diskActive`, `writeProtect` + `ComposeWriteProtectTooltip`, door FSM
(Closed/Opening/Open/Closing, 350 ms). The scene is a new *view* over this state; the
sync path (`DiskManager::UpdateDriveWidgets` → `SyncFromState`) is unchanged.

## SceneHitResult

| Field | Type | Notes |
|---|---|---|
| target | { None, Glass, Drive } | |
| driveIndex | int | when Drive |
| region | DriveWidgetRegion { None, Body, Eject } | Reuses the existing enum; Slot box → Eject (parity with 2D band naming) |
| emulatedPixel | POINT | when Glass |

- **Rule**: Glass outranks region boxes; nearest device wins; miss → None (dead space does
  nothing — spec edge case).

## FullscreenStripState (pure FSM)

| Field | Type | Notes |
|---|---|---|
| mode | { Hidden, Revealing, Shown, Hiding } | slide animation states |
| pinned | bool | tooltip visible or disk browser open from strip — blocks auto-hide |
| summonedByHotkey | bool | capture was released for this interaction |
| capturedModeAtSummon | { None, Mouse, Paddle } | what to restore on dismiss |
| activityIndicator | bool | any drive active while Hidden |

- **Inputs**: pointer position (bottom-edge proximity), pointer-over-strip, hotkey,
  tooltip/browse open/close, guest-capture state, drive activity.
- **Invariants** (property-tested): edge-reveal never fires while guest owns the pointer;
  `Hidden` never entered while `pinned`; capture restored exactly once on dismiss iff
  released at summon; indicator only in `Hidden`.

## FullscreenPresentation

| Field | Type | Notes |
|---|---|---|
| active | bool | fullscreen AND skeuo theme |
| glassCamera | SceneCamera | FOV/eye solved so glass fills the monitor (R8) |
| stripLayout | DeskSceneLayout | drives-only strip composition at bottom edge |

- **Transitions**: entering fullscreen swaps camera + hides chrome bands; leaving restores
  the windowed composition; both preserve emulation uninterrupted (FR-010 analog).
