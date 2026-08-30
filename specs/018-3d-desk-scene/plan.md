# Implementation Plan: 3D Desk Scene (Parity Phase)

**Branch**: `desk-scene-models` (feature `018-3d-desk-scene`) | **Date**: 2026-08-10 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/018-3d-desk-scene/spec.md`

## Summary

Replace the skeuomorphic theme's 2D `MonitorFrame` and drive band with a real-time 3D desk scene in the main window: the Monitor //c and two Disk II parametric models rendered via the existing `Dxui3DRenderer`, with the CRT post-process output texture-mapped onto the monitor's spherical-sag glass mesh and pointer input inverse-projected from screen px through the curved glass to emulated pixels. Fullscreen becomes a glass-only presentation with an auto-hiding drive overlay strip. Compact themes (`compactDrives == true`) are untouched; the 2D skeuo paths are removed once parity is validated.

Technical approach: reuse the printer's proven mesh pipeline (`ObjMeshParser` → `Dxui3DRenderer`) but on the main window's shared device inside the existing before-present hook; add a render-to-offscreen mode to `CrtPostProcess` so the finished CRT image is an SRV the glass mesh samples; put all new geometry math (UV synthesis, projection/unprojection, placement/fit, overlay-strip FSM) in unit-testable files following the `CrtPostProcess`/`DriveWidget` recompile-into-UnitTest precedent.

## Technical Context

**Language/Version**: C++ (stdcpplatest, MSVC v145+)

**Primary Dependencies**: Windows SDK only — D3D11/DXGI (existing `Dxui3DRenderer`, `D3DRenderer`, `CrtPostProcess`), Direct2D/DirectWrite via Dxui. No new third-party dependencies; models are first-party generated assets already on this branch.

**Storage**: `GlobalUserPrefs` (existing JSON prefs). No new persisted state this phase (layout persistence is the follow-on docking spec). The `skeuoMonitorFrame` opt-in pref is retired (scene is unconditional in skeuo; stale key ignored on load).

**Testing**: Microsoft C++ Unit Test Framework (`UnitTest/` project); new scene math files compiled into UnitTest by path, per existing `CrtPostProcess.cpp`/`DriveWidget.cpp` precedent.

**Target Platform**: Windows 10/11, x64 + ARM64 (ARM64 build-only validation)

**Project Type**: Desktop app (thin `Casso.exe` shell over linked libs: `CassoCore`, `CassoEmuCore`, `Dxui`)

**Performance Goals**: No user-perceivable drop vs. current release (SC-002). Preserve `D3DRenderer::NeedsPresent` frame-skip gating on static screens; scene animation (LED glow, door) calls `MarkRedrawNeeded()`. Static scene geometry built once, not per frame (improves on `Printer3DScene`'s per-frame rebuild).

**Constraints**: Dxui paint is UI-thread immediate-mode; `SetBeforePresentHook` is a single `std::function` already owned by the emulator composite (`EmulatorShell.cpp:1062`) — the scene must compose within it, not replace it. Swap chain uses oversized backbuffer + `SetSourceSize` (never `ResizeBuffers` on drag). Fullscreen is borderless-windowed (`D3DRenderer::ToggleFullscreen`), no DXGI exclusive mode.

**Scale/Scope**: 3 device models this phase (~10k triangles total), 560×384 source framebuffer, CRT output at letterbox resolution, 2 drives max, 1 fixed camera.

## Constitution Check

*GATE: evaluated pre-Phase-0 and re-checked post-design — PASS (no violations to justify).*

- **I. Code Quality**: All new code follows EHM single-exit, decl-at-top, helper-as-class-static, `s_k` file constants, spacing rules. No anonymous namespaces. ✅
- **II. Testing Discipline**: All decision-making math is pure data-in/data-out and unit-tested: glass UV synthesis, screen↔glass↔emulated-pixel projection both directions, device placement/fit, region hit classification, overlay-strip FSM, capture release/restore sequencing, scene-scale inverse for Ctrl+0. No GPU, file, or window handles in tests. D3D submission code is the irreducible platform edge (precedent: `Dxui3DRenderer`, `Printer3DScene`) and stays thin. ✅
- **III. UX Consistency**: No CLI changes. Tooltip strings reuse `ComposeWriteProtectTooltip` verbatim (FR-006). ✅
- **IV. Performance**: Present gating preserved; geometry cached; no per-frame allocations in the render path. ✅
- **V. Simplicity**: Reuses the existing mesh renderer instead of introducing a scene graph/engine; sub-mesh identification reuses the precedented Kd-match approach rather than growing the parser; YAGNI on camera controls, environment, docking (all deferred by spec). ✅
- **VI. Thin Executable, Testable Core**: New logic lands in files reachable from UnitTest (recompiled-by-path like `DriveWidget`/`CrtPostProcess`/`ThemeManager` — the sanctioned pattern for `Casso/Ui` chrome). Pure geometry that has no UI coupling goes next to `ObjMeshParser` in `CassoEmuCore`. The exe-only additions are D3D bind/draw calls and message-loop wiring. ✅

## Project Structure

### Documentation (this feature)

```text
specs/018-3d-desk-scene/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   ├── scene-geometry.md     # Pure-math API contract (projection, UV, placement)
│   └── scene-interaction.md  # UX parity contract (regions, tooltips, fullscreen strip)
└── tasks.md             # Phase 2 output (/speckit-tasks)
```

### Source Code (repository root)

```text
CassoEmuCore/Render/                      # pure, UI-free geometry (new)
├── CurvedDisplayMath.h/.cpp              # sphere-sag UV synthesis; ray↔glass intersection;
│                                         #   screen px ↔ glass UV ↔ emulated pixel, both directions
└── SceneCamera.h/.cpp                    # LookAt/Perspective/Mul44 (hoisted from Printer3DScene),
                                          #   fit math: glass-fills-rect FOV solve (fullscreen + Ctrl+0 inverse)

Casso/Ui/Scene/                           # scene logic (new; recompiled into UnitTest)
├── DeskSceneLayout.h/.cpp                # device placement from viewport rect + machine config;
│                                         #   scene-scale computation replacing MonitorFrame::SceneScale
├── DeskSceneModel.h/.cpp                 # per-device: mesh load, glass/lamp/slot sub-mesh discovery (Kd match),
│                                         #   interactive region boxes in model space, region hit classification
├── DeskSceneHitTester.h/.cpp             # screen px → ray → device/region/glass resolution (pure)
├── FullscreenStripState.h/.cpp           # overlay-strip FSM: edge-reveal, hotkey, auto-hide, pin-while-tooltip,
│                                         #   capture release/restore sequencing (pure FSM)
└── DeskScene.h/.cpp                      # D3D submission over Dxui3DRenderer (thin; not unit-tested)

Casso/                                    # modified
├── CrtPostProcess.h/.cpp                 # render-to-offscreen-SRV mode (final pass → RT instead of backbuffer)
├── D3DRenderer.h/.cpp                    # expose CRT output SRV; scene-aware UploadAndComposite path
├── EmulatorShell.h/.cpp                  # scene wiring, input rerouting, fullscreen presentation branch,
│                                         #   retire MonitorFrame/skeuo band paths after parity
├── Shell/WindowCommandManager.cpp        # fullscreen command interplay; strip hotkey
├── resource.h / Casso.rc                 # IDR_MODEL_MONITOR2C/_DISKII OBJ+MTL RCDATA entries
└── Ui/Chrome/MonitorFrame.*              # removed at parity validation (with skeuo DriveWidget paint path)

Dxui/Render/Dxui3DRenderer.h/.cpp         # SetContentSrv() overload (bind external SRV, no CPU round-trip);
                                          #   per-vertex UV support on the textured path

UnitTest/UiTests/                         # new tests
├── CurvedDisplayMathTests.cpp
├── SceneCameraTests.cpp
├── DeskSceneLayoutTests.cpp
├── DeskSceneModelTests.cpp
├── DeskSceneHitTesterTests.cpp
└── FullscreenStripStateTests.cpp
```

**Structure Decision**: Pure math with zero UI coupling goes to `CassoEmuCore/Render/` (linked into UnitTest, next to `ObjMeshParser`). Scene logic that touches Dxui types but not the GPU goes to `Casso/Ui/Scene/` and is compiled into UnitTest by path (the established chrome pattern). Only `DeskScene.cpp` (D3D bind/draw) and `EmulatorShell` wiring are untestable, matching the constitution's irreducible-platform-edge allowance.

## Complexity Tracking

No constitution violations to justify.
