# Tasks: 3D Desk Scene (Parity Phase)

**Input**: Design documents from `specs/018-3d-desk-scene/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Included — the constitution mandates unit coverage for all new logic (Principle II/VI). Test tasks accompany their implementation tasks; pure-math contract tests come first within each group.

**Organization**: Phases 3-5 map to spec user stories US1-US3. Fullscreen (FR-014/FR-015) is its own phase after the stories; 2D-path retirement is gated behind the parity walkthrough in Polish.

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup

**Purpose**: Assets and project plumbing so every later task compiles and links.

- [X] T001 Embed Monitor2c + DiskII models: add `IDR_MODEL_MONITOR2C_OBJ/_MTL`, `IDR_MODEL_DISKII_OBJ/_MTL` to `Casso/resource.h` (after the ImageWriter IDs ~:137) and RCDATA entries in `Casso/Casso.rc` (~:48) pointing at `Resources/Models/{Monitor2c,DiskII}/`
- [X] T002 [P] Scaffold `CassoEmuCore/Render/` (SceneCamera, CurvedDisplayMath stubs) in `CassoEmuCore.vcxproj` + `.filters`, and `Casso/Ui/Scene/` (DeskSceneLayout, DeskSceneModel, DeskSceneHitTester, FullscreenStripState, DeskScene stubs) in `Casso/Casso.vcxproj` + `.filters`
- [X] T003 [P] Add UnitTest plumbing: new test files `UnitTest/UiTests/{SceneCameraTests,CurvedDisplayMathTests,DeskSceneLayoutTests,DeskSceneModelTests,DeskSceneHitTesterTests,FullscreenStripStateTests}.cpp` to `UnitTest/UnitTest.vcxproj`, plus recompile-by-path entries for the `Casso/Ui/Scene/*.cpp` sources (same pattern as `DriveWidget.cpp` at `UnitTest.vcxproj:403`)

**Checkpoint**: Solution builds x64 Debug with empty stubs; test binary links.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The math and pipeline capabilities every story consumes. No user-visible change yet.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T004 [P] `CassoEmuCore/Render/SceneCamera.h/.cpp`: hoist `LookAtRH`/`PerspectiveFovRH`/`Mul44` from `Casso/Ui/Printer3DScene.cpp:225-309` as class statics, add `Inverse44` and `FitContain` (contain-not-crop, mirroring `Printer3DScene.cpp:2157-2160`); tests in `UnitTest/UiTests/SceneCameraTests.cpp` (identity/inverse round-trip, fit containment)
- [X] T005 [P] `CassoEmuCore/Render/CurvedDisplayMath.h/.cpp`: `CurvedDisplaySurface` struct + `UvFromModelPoint`/`ModelPointFromUv` (planar XZ, R4), `IntersectRay` (analytic sphere + rect clamp, R5), `EmulatedPixelFromScreenPx`/`ScreenPxFromEmulatedPixel`; tests in `UnitTest/UiTests/CurvedDisplayMathTests.cpp` per contracts/scene-geometry.md (pixel round-trip over corner/edge/center grid, glancing rays, edge-maps-to-outermost-pixel, off-glass miss)
- [X] T006 [P] `Dxui/Render/Dxui3DRenderer.h/.cpp`: add `SetContentSrv(ID3D11ShaderResourceView*)` alongside `UpdateContentTexture`, and per-vertex UV support on the textured path (vertex struct + input layout + `DrawTriangles` plumbing); keep the 1×1 white fallback behavior
- [X] T007 `Casso/CrtPostProcess.h/.cpp` + `Casso/D3DRenderer.h/.cpp`: render-to-offscreen mode — final `copy` pass targets a dedicated RT sized to the aspect-fit rect instead of the back buffer; `D3DRenderer` exposes the CRT output SRV and a scene-aware composite path in `UploadAndComposite` (existing direct-to-backbuffer path unchanged for non-skeuo); extend `UnitTest/UiTests/CrtLetterboxLayoutTests.cpp` for the offscreen-rect sizing
- [X] T008 `Casso/Ui/Scene/DeskSceneModel.h/.cpp`: parse embedded OBJ/MTL via `ObjMeshParser`, split sub-meshes by Kd match ±0.02 (glass Kd 0.05/0.09/0.07 as shared named constant; lamp colors per model), build `CurvedDisplaySurface` from the glass bbox + `half_diag * 2.2`, synthesize glass UVs, declare model-space region boxes (DiskII: Slot, Body); tests in `UnitTest/UiTests/DeskSceneModelTests.cpp` against synthetic OBJ/MTL text buffers AND the real resource text (glass found exactly once, UV corners exact, region boxes sane)
- [X] T009 `Casso/Ui/Scene/DeskSceneLayout.h/.cpp`: `Compute(viewportRectPx, dpi, machineConfig)` → placements (monitor centered, drives in former band position), camera, `sceneScale` (glass px height ÷ 384 dp); supports deviceCount 0-2; single shared camera only (FR-016 — no per-device cameras/billboarding); tests in `UnitTest/UiTests/DeskSceneLayoutTests.cpp` (determinism, containment at extreme aspects, sceneScale formula, drive-count mapping, off-center devices show position-correct parallax: projected silhouette of a below-center drive exposes its top face)

**Checkpoint**: All new math green in UnitTest; renderer + CRT capabilities in place; nothing user-visible changed.

---

## Phase 3: User Story 1 - Curved-glass monitor with the live picture (Priority: P1) 🎯 MVP

**Goal**: In skeuo, a 3D Monitor //c replaces the 2D `MonitorFrame`, with the finished CRT image on true-curvature glass, pixel-accurate input, power lamp, and working Ctrl+0 — with no drives yet.

**Independent Test**: quickstart.md § US1 — launch skeuo (temporarily zero drive objects), MousePaint corner/edge clicks land within one pixel, resize/DPI/gestalt checks pass.

- [ ] T010 [US1] `Casso/Ui/Scene/DeskScene.h/.cpp`: D3D submission over `Dxui3DRenderer` — cached geometry upload (R9: build once, rebuild on layout change only), draw order backdrop-preserving (depth pass → opaque monitor → textured glass → lamp glow last, per `Printer3DScene::Render` order), `MarkRedrawNeeded()` on animated elements
- [ ] T011 [US1] `Casso/EmulatorShell.cpp` hook integration: extend the single before-present lambda (`InitializeRenderer` ~:1062) to run CRT-to-offscreen then `DeskScene::Render` when skeuo; `MonitorFrame` hidden while scene active (deletion deferred to T029/T030); scene picks up `NeedsPresent` gating
- [ ] T012 [US1] [P] Power lamp + branding: lamp sub-mesh tint driven by machine running state in `Casso/Ui/Scene/DeskScene.cpp` (brand stripes are model geometry — verify render)
- [ ] T013 [US1] `Casso/Ui/Scene/DeskSceneHitTester.h/.cpp` (glass path): screen px → ray → glass/None classification via `CurvedDisplayMath`; tests in `UnitTest/UiTests/DeskSceneHitTesterTests.cpp` (glass hit/miss, dead-space None)
- [ ] T014 [US1] Guest mouse rerouting in `Casso/EmulatorShell.cpp`: `UpdateGuestMouseFromHost` (~:6287) uses the hit tester's emulated pixel when scene active (replaces viewport `MulDiv` mapping + fixes letterbox bug); `ClearHostTarget` on glass-leave; `OnSetCursor` hides cursor over glass only; button press gated on glass hit (release ungated, parity)
- [ ] T015 [US1] Layout integration in `Casso/EmulatorShell.cpp`: `UpdateViewportLayout`/`OnSize`/`OnDpiChanged` drive `DeskSceneLayout`; scene `sceneScale` replaces `MonitorFrame::SceneScale()` consumers; Ctrl+0 reset-size uses `SceneCamera::SolveWindowForGlassPx` (replaces `CenterSizeForScreenPx` at `WindowCommandManager` reset path); add `SolveWindowForGlassPx` round-trip case to `UnitTest/UiTests/SceneCameraTests.cpp`
- [ ] T016 [US1] Runtime validation per quickstart.md § US1: launch + captures (`scripts/CaptureScreenshotMatrix.ps1`), MousePaint corner/edge accuracy, resize/DPI sweep, gestalt vs reference; fix what fails

**Checkpoint**: US1 fully functional — curved live display with accurate input, no drives.

---

## Phase 4: User Story 2 - 3D disk drives with full drive interaction (Priority: P2)

**Goal**: Two Disk II objects with complete 2D-band interaction parity: activity light, door/loaded state, tooltips (017 wording verbatim), slot=eject+browse / body=browse.

**Independent Test**: quickstart.md § US2 — boot DOS 3.3: LED flashes; tooltips match; slot/body/dead-space clicks behave; door animates; single-drive config shows one drive.

- [ ] T017 [US2] Drive placements: enable deviceCount 1-2 in the shell's layout call (`Casso/EmulatorShell.cpp`), drives rendered via `DeskScene` (geometry cached per layout)
- [ ] T018 [US2] Drive visual state binding in `Casso/Ui/Scene/DeskScene.cpp`: activity LED glow from `DriveWidgetState` atomics (`motorOn`/`diskActive`), door position from the existing door FSM progress, loaded/empty distinction, write-protect visual cue; `MarkRedrawNeeded` while animating (mirrors `TryPresentUiFrame` door forcing ~:4630)
- [ ] T019 [US2] Drive hit regions: extend `Casso/Ui/Scene/DeskSceneHitTester.cpp` with per-device ray-vs-region-box classification mapping Slot→`DriveWidgetRegion::Eject`, Body→`Body`, glass outranks boxes, nearest device wins; extend `UnitTest/UiTests/DeskSceneHitTesterTests.cpp` (two-drive resolution, slot-inside-body precedence parity with `DriveWidget::HitTest`)
- [ ] T020 [US2] Shell routing in `Casso/EmulatorShell.cpp`: `OnMouseMove` hover path (~:6043) uses scene hits for marquee/WP tooltips via `ComposeWriteProtectTooltip` unchanged; `OnLButtonUp` drive walk (~:6623) consumes `SceneHitResult` when scene active (Body → `BrowseForDisk`, Eject → `Eject(6,drive)` + `BrowseForDisk`); `OnMouseLeave` clears hover state
- [ ] T021 [US2] Runtime validation per quickstart.md § US2: boot/LED/tooltip/click/door/single-drive matrix; fix what fails

**Checkpoint**: US1 + US2 — full windowed parity.

---

## Phase 5: User Story 3 - Theme switching stays seamless (Priority: P3)

**Goal**: Skeuo ⇄ compact themes switch in <1 s with emulation uninterrupted; non-skeuo themes pixel-identical to current release.

**Independent Test**: quickstart.md § US3 — cycle all themes with a machine running; capture comparison.

- [ ] T022 [US3] Theme switch wiring in `Casso/EmulatorShell.cpp` (`ApplyThemeToChrome` ~:3281): scene activates/deactivates with `compactDrives`; arrival state correct (picture, disks, activity, WP) without emulation hitches; window-size delta rules preserved for compact themes; CRT path returns to direct-to-backbuffer when scene inactive
- [ ] T023 [US3] Runtime validation per quickstart.md § US3: cycle themes under load (<1 s, no pause), before/after captures of DarkModern + RetroTerminal pixel-compared; fix what fails

**Checkpoint**: All three user stories independently validated.

---

## Phase 6: Fullscreen presentation (FR-014 / FR-015)

**Purpose**: Glass-only fullscreen with the drive overlay strip and capture-safe summoning.

- [ ] T024 [P] `SceneCamera::SolveGlassFillCamera` in `CassoEmuCore/Render/SceneCamera.cpp` (glass corners → viewport corners); cases in `UnitTest/UiTests/SceneCameraTests.cpp`
- [ ] T025 [P] `Casso/Ui/Scene/FullscreenStripState.h/.cpp`: pure FSM (Hidden/Revealing/Shown/Hiding, pinned, summonedByHotkey, capturedModeAtSummon, activityIndicator) with `Tick(state, inputs)`; property tests in `UnitTest/UiTests/FullscreenStripStateTests.cpp` for the four data-model invariants + hotkey-under-capture release/restore-exactly-once + tooltip pin + indicator-only-while-hidden
- [ ] T026 Fullscreen presentation branch in `Casso/EmulatorShell.cpp`: when fullscreen ∧ skeuo — glass-fill camera, chrome bands hidden (first `IsFullscreen()` presentation reader), clean enter/leave transitions with emulation uninterrupted; windowed compact themes unaffected
- [ ] T027 Strip integration: strip composition renders the same drive objects (drives-only `DeskSceneLayout` at bottom edge — its own composed presentation with its own single camera, FR-016 per-composition: strip drives keep position-derived perspective, never billboarded) in `Casso/Ui/Scene/DeskScene.cpp`; FSM inputs wired in `Casso/EmulatorShell.cpp` (edge dwell from `OnMouseMove`, guest-capture state from pointer mode, tooltip/browse pin, activity); new `IDM_VIEW_DRIVE_STRIP` hotkey command in `Casso/resource.h` + accelerator + handler in `Casso/Shell/WindowCommandManager.cpp` performing capture release/interact/restore; hidden-state activity indicator draw
- [ ] T028 Runtime validation per quickstart.md § Fullscreen: host-pointer edge reveal + auto-hide + tooltip pin; mouse-mode and paddle-mode hotkey matrix (capture restored cleanly); activity indicator; enter/leave; fix what fails

**Checkpoint**: Fullscreen behavior complete per FR-014/FR-015.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T029 Parity walkthrough (SC-003): scripted checklist over every interaction in contracts/scene-interaction.md, windowed + fullscreen; record results in the PR; gestalt sign-off vs reference photos (SC-006) including position-derived perspective (FR-016: below-center drives read from slightly above, off-center flanks visible); explicit FR-012 capture check (PrintWindow of the main window contains the composed scene, not black, windowed + fullscreen)
- [ ] T030 Retire the 2D skeuo paths (gated on T029): delete `Casso/Ui/Chrome/MonitorFrame.h/.cpp` + all references, remove `DriveWidget`'s non-compact paint path (compact retained for other themes — `UnitTest/UiTests/DriveWidgetHitTests.cpp` reduced accordingly), retire `skeuoMonitorFrame` pref + Settings > Theme toggle (`Casso/Ui/Settings/ThemePage.cpp`, `EmulatorShell::SetSkeuoMonitorFrame` ~:3378; stale JSON key ignored), remove the effective-DPI scene-scale fold + 3-pass settle loop (`EmulatorShell.cpp:240`, `:2338-2343`)
- [ ] T031 Perf validation (SC-002): static-screen present-skip confirmed (GPU busy comparable to current release), boot/door animation smoothness; no per-frame allocations in the render path (spot-check)
- [ ] T032 Merge gates: x64 Debug + Release full suites green, ARM64 builds, Code Analysis zero warnings, CheckStyle clean; CHANGELOG.md Unreleased entry; README screenshot refresh deferred to release

---

## Dependencies & Execution Order

- **Phase 1 → Phase 2 → Phase 3 (US1) → Phase 4 (US2) → Phase 5 (US3) → Phase 6 → Phase 7**
- US2 depends on US1's scene/hook/hit-tester wiring (T010-T015). US3 depends on the scene existing (US1) — drive coverage (US2) makes its validation complete. Fullscreen depends on US1 (glass camera) + US2 (strip drives).
- T030 is **gated on T029** (supersession only after parity is proven).
- Within phases: T004/T005/T006 parallel; T008/T009 after T004/T005; T012/T013 parallel after T010-T011; T024/T025 parallel.

## Parallel Example: Phase 2

```text
Parallel: T004 (SceneCamera), T005 (CurvedDisplayMath), T006 (Dxui3DRenderer UV/SRV)
Then:     T007 (CRT offscreen — uses T006's SRV shape)
Parallel: T008 (DeskSceneModel — uses T005), T009 (DeskSceneLayout — uses T004)
```

## Implementation Strategy

MVP is Phase 1-3 (US1): the curved live display with accurate input is the riskiest and most valuable slice — stop and validate there (quickstart § US1) before touching drives. Then US2 → US3 → fullscreen as independent increments, each with its runtime validation task. Retirement (T030) lands last so the parity reference exists in-tree until proven. Commit + push per phase (constitution commit discipline); reference GH issue if one is opened for the initiative.
