# Contract: Scene Geometry Math (pure, unit-tested)

All functions are data-in/data-out — no GPU, no window handles, no globals. Locations per
plan.md structure (`CassoEmuCore/Render/`, `Casso/Ui/Scene/`). Signatures are working
shapes; code follows project style (HRESULT/EHM where fallible, class statics).

## CurvedDisplayMath

```text
UvFromModelPoint  (surface, modelPt)            -> {u, v}            // planar XZ (R4)
ModelPointFromUv  (surface, u, v)               -> modelPt           // includes sag Y
IntersectRay      (surface, rayOrigin, rayDir)  -> modelPt | miss    // analytic sphere + rect clamp
EmulatedPixelFromScreenPx (camera, world, surface, screenPx) -> POINT | miss
ScreenPxFromEmulatedPixel (camera, world, surface, pixel)    -> screenPx   // forward, for tests
```

**Contract tests**:
- Round-trip: for every emulated pixel corner/edge/center sample, `EmulatedPixelFromScreenPx(ScreenPxFromEmulatedPixel(p)) == p` (FR-002's one-pixel bar, exact in math).
- Edge behavior: screen points visually on the glass edge map to outermost pixels, never miss (spec edge case).
- Off-glass rays miss; rays at glancing angles still resolve when they visually intersect.

## SceneCamera

```text
LookAtRH / PerspectiveFovRH / Mul44 / Inverse44      // hoisted from Printer3DScene statics
FitContain (contentAspect, viewportRect)             -> proj adjustments   // contain-not-crop
SolveGlassFillCamera (surface, world, viewportRect)  -> camera             // fullscreen glass-only (R8)
SolveWindowForGlassPx (surface, desiredGlassPx)      -> client size        // Ctrl+0 inverse (R7)
```

**Contract tests**: fullscreen camera puts glass corners exactly at viewport corners
(within a px); window solve round-trips against layout (layout at solved size yields the
requested glass px height).

## DeskSceneLayout

```text
Compute (viewportRectPx, dpi, machineConfig) -> DeskSceneLayout
```

**Contract tests**: determinism; drive count follows config (0/1/2); monitor centered with
drives in the former band position; scene contained at extreme aspect ratios; sceneScale
== glassPxHeight / (384dp at dpi); exactly ONE camera in the layout output (FR-016 — no
per-device cameras); off-center placements produce position-correct parallax under the
shared camera (projected silhouette of a below-center drive exposes its top face; an
off-center drive exposes its inward flank).

## DeskSceneHitTester

```text
Classify (layout, models, screenPx) -> SceneHitResult
```

**Contract tests**: glass-outranks-boxes; slot vs body boxes reproduce the 2D
`DriveWidget::HitTest` semantics (Eject checked before Body); dead space → None;
per-device resolution with two drives present.

## FullscreenStripState

```text
Tick (state, inputs) -> state'      // inputs: pointerPx, overStrip, hotkey, pinned events,
                                    //         guestCapture, driveActivity, elapsedMs
```

**Contract tests**: the four invariants in data-model.md, plus: edge-dwell then leave
hides after grace; hotkey under paddle capture releases and restores capture exactly once;
tooltip pin blocks auto-hide until closed; activity indicator only while Hidden.
