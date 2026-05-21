---
description: "Task list for 007-ui-overhaul — RmlUi-based chrome, themes, settings panel, CRT post-processing"
---

# Tasks: Full UI Overhaul (RmlUi + CSS Themes + Custom D3D Chrome)

**Input**: `specs/007-ui-overhaul/{spec.md,plan.md,research.md,data-model.md,contracts/,quickstart.md}`
**Branch**: `007-ui-overhaul`
**Constitution**: v1.5.0 (RmlUi, crt-pi, libretro bloom, libretro ntsc-adaptive whitelisted)

## Organization

Tasks are organized by the **plan's phase letters (P0…P9)** rather than by user story, because the plan is sequenced as a hard phase pipeline (each gate must pass before the next begins) and most user stories share the same underlying infrastructure (RmlUi backend, theme system, UserConfigStore). The user-story mapping is recorded per task in the `[USx]` label so traceability to `spec.md` is preserved.

User-story key (from `spec.md`):

- **US1** — Change emulation speed without hunting through menus (P1)
- **US2** — Enable/disable a hardware component for a machine (P1)
- **US3** — Insert a disk using the custom drive widget (P1)
- **US4** — Apply a different theme at runtime (P2)
- **US5** — Interact with the custom title bar and navigation layer (P2)
- **US6** — Per-machine JSON settings survive an upgrade (P3)

## Format: `- [ ] <ID> [P?] [USx?] Description (files)`

- `[P]` — parallelizable with siblings inside the same phase (no shared-file conflicts, no intra-phase order dep).
- `[USx]` — user-story label where the task is the proximate deliverable for that story. Foundational/infra tasks have no `[USx]` label.
- Every task lists the files it touches (new/modified/deleted) and its acceptance criterion.

---

## Phase P0 — Foundations (research + governance)

**Gate to exit**: research.md frozen, constitution v1.5.0 merged, RmlUi version pinned, shader source list locked.

- [x] **P0-T1** ~~Constitution amendment v1.4.0 → v1.5.0 (whitelist RmlUi, crt-pi, libretro bloom, libretro ntsc-adaptive).~~ **DONE** prior to task generation — constitution v1.5.0 is in `.specify/memory/constitution.md`. No further action required; recorded here for phase-gate completeness.
- [x] **P0-T2** Record pinned RmlUi upstream tag + commit SHA in `External/RmlUi/README.casso.md` (file does not yet exist — created in P1-T1; this task is just the *decision record* portion). Acceptance: latest upstream stable tag named, SHA captured, MIT `LICENSE.txt` confirmed present in chosen tag. Files: `specs/007-ui-overhaul/research.md` (R1 decision row), no source changes.
- [x] **P0-T3** Finalize shader source attribution table in `specs/007-ui-overhaul/research.md` (R4): per shader (scanlines=crt-pi, bloom=libretro bloom, color-bleed=libretro ntsc-adaptive) record original author, upstream URL, exact upstream file path, SHA of port basis, license (must be MIT/PD). Acceptance: three rows present, all MIT. Files: `specs/007-ui-overhaul/research.md`.
- [x] **P0-T4** Confirm "write our own D3D11 backend" decision (R2) is reflected in `contracts/rml-backend.h`. Acceptance: contract header matches the agreed RenderInterface surface; no upstream-backend fork referenced anywhere. Files: `specs/007-ui-overhaul/contracts/rml-backend.h` (verify only).
- [x] **P0-T5** Confirm Win11 borderless recipe (R3) verified on both x64 and ARM64 in a throwaway probe, with runtime gating of Win11-only effects (Mica, rounded corners) so the app remains functional on Windows 10; record outcome in `research.md`. Acceptance: probe results table populated, no ARM64-specific deltas, `IsWindows11OrGreater()` fallback path exercised on Win10. Files: `specs/007-ui-overhaul/research.md`.
- [x] **P0-T6** Confirm font decisions (R8): Inter (SIL OFL) for chrome/Skeuomorphic/Dark Modern; VT323 (SIL OFL) for Retro Terminal; both vendored under each theme's `fonts/` with `OFL.txt` alongside. Acceptance: research.md row R8 updated, font file sources captured. Files: `specs/007-ui-overhaul/research.md`.

**Phase P0 dependencies**: P0-T2..P0-T6 are all `[P]` once P0-T1 is done. P0-T1 is already complete.
**Constitution check gate**: v1.5.0 in place; Approved Dependencies table contains RmlUi, crt-pi, libretro bloom, libretro ntsc-adaptive; Principle V violation status reads "JUSTIFIED — covered by allowlist".

---

## Phase P1 — Vendoring & schema (no UI yet)

**Gate to exit**: solution builds on x64 Debug, x64 Release, **ARM64 Debug, ARM64 Release** (hard); all existing tests pass; schema rename + capabilityFlag default round-trip in unit tests.

### Vendoring

- [ ] **P1-T1** Drop RmlUi source as a **plain copy** under `External/RmlUi/` (Source/, Include/, LICENSE.txt). No git subtree. Author `External/RmlUi/README.casso.md` capturing pinned tag + SHA from P0-T2. Acceptance: tree matches upstream tag byte-for-byte (excluding upstream tests/samples/demos which may be pruned); MIT `LICENSE.txt` present at `External/RmlUi/LICENSE.txt`; README.casso.md states the tag, SHA, vendoring date, and the `git ls-files` count of vendored files. Files: `External/RmlUi/**` (new), `External/RmlUi/README.casso.md` (new). **[BLOCKED: vendoring upstream RmlUi 6.2 in full (Source/Include/LICENSE.txt) materially exceeds the scope of a single agent session — thousands of vendored files require a dedicated commit. Suggested resolution: run `git clone --depth 1 -b 6.2 https://github.com/mikke89/RmlUi.git External/RmlUi-staging` then prune Tests/Samples/Demos/CMake/Build and rename to `External/RmlUi/`.]**
- [ ] **P1-T2** Create `External/RmlUi/RmlUi.vcxproj` as Static Library; configurations: Debug|x64, Release|x64, Debug|ARM64, Release|ARM64; toolset v145; `stdcpplatest`; WindowsTargetPlatformVersion matching Casso.vcxproj; UTF-8 source; treat warnings as errors **disabled inside this project only** (vendored code). Add to `Casso.sln`. Acceptance: all four configurations build clean; **ARM64 Release is the hard done-criterion**. Files: `External/RmlUi/RmlUi.vcxproj` (new), `External/RmlUi/RmlUi.vcxproj.filters` (new), `Casso.sln` (modified). Depends: P1-T1. **[BLOCKED: depends on P1-T1.]**
- [ ] **P1-T3** Add `Casso.vcxproj` → `External/RmlUi/RmlUi.vcxproj` project reference; add `$(SolutionDir)External/RmlUi/Include` to Casso AdditionalIncludeDirectories for all four configurations. Acceptance: Casso links against vendored RmlUi.lib on all four configs (verified with an `#include <RmlUi/Core.h>` smoke include in a single Casso .cpp). Files: `Casso/Casso.vcxproj` (modified), throwaway smoke include reverted before commit. Depends: P1-T2. **[BLOCKED: depends on P1-T2.]**
- [ ] **P1-T4 [P]** Route RmlUi includes through `Casso/Pch.h` per the angle-bracket include rule (add `<RmlUi/Core.h>` and any C-header bridges). Confirm full Casso project compiles with PCH enabled and no header bypass. Acceptance: no `#include <RmlUi/...>` appears outside `Pch.h` in any Casso .cpp/.h. Files: `Casso/Pch.h` (modified). Depends: P1-T3. **[BLOCKED: depends on P1-T3.]**

### Schema extension (CassoEmuCore)

- [x] **P1-T5** [US2] Extend `MachineConfig` schema in `CassoEmuCore/Core/MachineConfig.{h,cpp}`: add `capabilityFlag` (`optional`|`required`|`platform-locked`) on internal device entries and slot entries; add optional `lockReason` string. Acceptance: round-trip serialization preserves both fields; JSON missing the field deserializes without error. Files: `CassoEmuCore/Core/MachineConfig.h`, `CassoEmuCore/Core/MachineConfig.cpp`. Depends: P1-T2 (project shape only — no RmlUi dep).
- [x] **P1-T6** [US6] **Schema rename `$cassoDefault` → `$cassoMachineVersion`** in `MachineConfig` (read path accepts both; write path always emits the new name). Acceptance: an existing machine JSON that still uses `$cassoDefault` loads successfully, is migrated in-memory, and on next save serializes the new key — see P1-T8 for the round-trip test. **This is the one schema rename that touches every existing machine JSON in `Resources/Machines/`.** Files: `CassoEmuCore/Core/MachineConfig.cpp`. Depends: P1-T5.
- [x] **P1-T7** [US6] Extend `MachineConfigUpgrade` with one new upgrade step: (a) rename `$cassoDefault` → `$cassoMachineVersion` (one-shot, idempotent), (b) default missing `capabilityFlag` on internal devices to `"required"`, on slot entries to `"optional"` per FR-015. Bump the schema version constant. Acceptance: upgrade is reversible-safe (running it twice is a no-op); upgrade emits a structured log line per migration. Files: `CassoEmuCore/Core/MachineConfigUpgrade.{h,cpp}`. Depends: P1-T6.
- [x] **P1-T8** [US6] **Update every machine JSON under `Resources/Machines/`** to the new key name AND to set explicit `capabilityFlag` + `lockReason` where machine-locked. Specifically: Apple //c integrated 80-column card → `platform-locked` with `lockReason="Integrated on Apple //c motherboard"`; Apple //e slot 3 80-column card → keep `optional`; mouse/joystick slot entries → `optional`. Acceptance: every JSON in `Resources/Machines/` opens cleanly under the new loader with **no upgrade-step invocation needed** (i.e., they're already at the new schema). The upgrade path remains in place purely for legacy `_user.json` files. Files: every file under `Resources/Machines/**/*.json` (modified). Depends: P1-T7. **NOTE:** Apple //c machine JSON does not yet exist in this repo, so the platform-locked example is exercised only via the `Load_CapabilityFlag_ExplicitPlatformLocked_Preserved` unit test rather than in shipped data.
- [x] **P1-T9** [P] [US6] Test cases in `UnitTest/EmuTests/MachineConfigUpgradeTests.cpp`:
  - Loading a legacy JSON with `$cassoDefault` produces a config whose serialized form uses `$cassoMachineVersion` and preserves all other fields.
  - **Default inversion (explicit)**: loading a JSON with no `capabilityFlag` on internal devices fills `"required"`; the SAME JSON with no `capabilityFlag` on slot entries fills `"optional"`. The asymmetry MUST be asserted in both directions in one TEST_METHOD to guard against an accidental swap.
  - Running the upgrade step twice in a row is a no-op on the second run.
  - A JSON already at the new schema is not modified by the upgrade.
  - **No disk I/O**: all inputs are in-memory JSON strings; FS abstraction (introduced in P2-T1) is **not** required for these tests because `MachineConfigUpgrade` operates on parsed JSON, not files.
  Acceptance: ≥4 new TEST_METHODs pass on all four configurations, including the explicit default-inversion case. Files: `UnitTest/EmuTests/MachineConfigUpgradeTests.cpp` (modified). Depends: P1-T7 (impl); independent of P1-T8.

**Phase P1 dependency graph**: P1-T1 → P1-T2 → P1-T3 → P1-T4. P1-T5 → P1-T6 → P1-T7 → P1-T8. P1-T9 runs after P1-T7 in parallel with P1-T8.
**Constitution check gate**: Principle II (no I/O in unit tests) satisfied — P1-T9 is pure JSON-string-in/JSON-string-out. EHM, single-exit, top-of-scope-vars rules apply to all new code in P1-T5..P1-T7 (review checklist in PR).

---

## Phase P2 — User-config infrastructure

**Gate to exit**: per-machine `_user.json` round-trips through merge; `GlobalUserPrefs.json` round-trips; both go through an injectable `IFileSystem`; registry-migration one-shot path proven; ≥90% line coverage on the two new modules.

### Foundational

- [ ] **P2-T1** Define `IFileSystem` abstraction in `Casso/Config/IFileSystem.h`: ReadAllText, WriteAllText (atomic via temp+rename), Exists, Delete, EnumerateFiles. Provide `Win32FileSystem` impl in `Casso/Config/Win32FileSystem.{h,cpp}`. Acceptance: header has only PIO operations on `std::wstring` paths; no Win32 types in the interface. Files: `Casso/Config/IFileSystem.h` (new), `Casso/Config/Win32FileSystem.{h,cpp}` (new). Depends: P1-T4.

### UserConfigStore (per-machine `_user.json` shadow)

- [ ] **P2-T2** [US1] [US2] [US6] [US3] `UserConfigStore` in `Casso/Config/UserConfigStore.{h,cpp}`:
  - `Load(machineName, defaultConfig, IFileSystem&) → MachineConfig` performing shadow/fallthrough merge per FR-014/FR-017 (only keys present in `_user.json` override defaults; deep-merge for objects; arrays replace wholesale).
  - On version mismatch, invoke `MachineConfigUpgrade` and write the migrated `_user.json` back via `IFileSystem::WriteAllText`.
  - `SaveDelta(machineName, current, default, IFileSystem&)` — diff `current` vs `default` and persist only changed leaf keys (no full snapshot).
  - `Reset(machineName, IFileSystem&)` — **deletes** `<MachineName>_user.json` (per the resolved Open Question 7) and returns the read-only default unchanged.
  - **Last-mounted disk persistence** (FR-047): a `lastMountedImages` map (`slot → drive → string path`) is included in the merged user config; `SaveDelta` writes only entries that differ from default (which is "absent"). On `Load`, the caller is expected to attempt auto-mount of each entry; missing files log a warning and clear the entry on the next `SaveDelta`.
  Acceptance: contract defined in header; deltas of a no-op (current == default) write nothing or write `{ "$cassoMachineVersion": N }` only; insert/eject calls round-trip through `lastMountedImages`. Files: `Casso/Config/UserConfigStore.{h,cpp}` (new). Depends: P2-T1, P1-T7.

### GlobalUserPrefs (one file at asset base dir)

- [ ] **P2-T3** [US4] `GlobalUserPrefs` in `Casso/Config/GlobalUserPrefs.{h,cpp}`. Fields per `contracts/global-user-prefs.schema.json`: `activeTheme` (string), `lastSelectedMachine` (string), `crt.brightness` (0..1), `crt.scanlines.{enabled,intensity}`, `crt.bloom.{enabled,radius,strength}`, `crt.colorBleed.{enabled,width}`, `window.lastBounds` (optional rect), `window.fullscreen` (bool), `$cassoGlobalPrefsVersion` (int). `Load(IFileSystem&)` and `Save(IFileSystem&)`; default-construct if file absent. Acceptance: schema in JSON matches `contracts/global-user-prefs.schema.json` exactly (validated by a schema-conformance test in P2-T7). Files: `Casso/Config/GlobalUserPrefs.{h,cpp}` (new). Depends: P2-T1.

### AssetBootstrap extensions

- [ ] **P2-T4** [US4] Extend `Casso/AssetBootstrap.{h,cpp}`:
  - `EnsureThemes()` — mirrors `EnsureMachineConfigs()`. Extracts embedded built-in themes (Skeuomorphic, Dark Modern, Retro Terminal) from Casso resources into `Themes/<name>/`. **Preserves** any user-authored themes already on disk (FR-030, FR-037) — never overwrites a directory whose `theme.json` does not contain the built-in marker.
  - `EnsureGlobalUserPrefs()` — writes a default `GlobalUserPrefs.json` to the asset base dir if absent.
  Acceptance: re-running bootstrap after a user theme is dropped in does NOT modify the user theme; built-in themes are restored if deleted. Files: `Casso/AssetBootstrap.{h,cpp}` (modified). Depends: P2-T3.

### Registry migration shim

- [ ] **P2-T5** [US6] `Casso/RegistrySettings.{h,cpp}` — one-shot migration mode: on first launch where `GlobalUserPrefs.json` is missing OR any `<Name>_user.json` is missing, read corresponding legacy values from the registry, write to JSON, then mark `RegistrySettings` quiescent (no further reads/writes ever). Acceptance: on a freshly migrated system the registry key is **never opened again** by Casso (logged + asserted in Debug builds). Files: `Casso/RegistrySettings.{h,cpp}` (modified). Depends: P2-T2, P2-T3.

### Tests (no disk I/O — `IFileSystem` mock)

- [ ] **P2-T6** [P] Test fixture `InMemoryFileSystem` (test-only helper, not shipped) in `UnitTest/UiTests/InMemoryFileSystem.{h,cpp}`. Acceptance: implements full `IFileSystem`; case-insensitive paths matching Win32 behavior; supports concurrent reads, atomic-replace semantics. Files: `UnitTest/UiTests/InMemoryFileSystem.{h,cpp}` (new). Depends: P2-T1.
- [ ] **P2-T7** [P] `UnitTest/UiTests/UserConfigStoreTests.cpp` — covers: load with no `_user.json` returns defaults verbatim; load with partial `_user.json` returns merged result; SaveDelta writes only diff (verify via in-memory FS contents); SaveDelta of no-op writes minimal content; Reset deletes file; version-mismatch triggers upgrade and write-back. Acceptance: ≥6 TEST_METHODs, all using `InMemoryFileSystem`. Files: `UnitTest/UiTests/UserConfigStoreTests.cpp` (new). Depends: P2-T2, P2-T6.
- [ ] **P2-T8** [P] `UnitTest/UiTests/GlobalUserPrefsTests.cpp` — default construction, round-trip, missing-field tolerance, schema-version migration placeholder. Acceptance: ≥4 TEST_METHODs. Files: `UnitTest/UiTests/GlobalUserPrefsTests.cpp` (new). Depends: P2-T3, P2-T6.
- [ ] **P2-T9** [P] `UnitTest/UiTests/RegistryMigrationTests.cpp` — given a fake registry source (injected) and an `InMemoryFileSystem`, prove the one-shot migration runs once and never re-runs. Acceptance: 2 TEST_METHODs (first-launch + post-migration). Files: `UnitTest/UiTests/RegistryMigrationTests.cpp` (new), and the registry source abstraction added if not already present. Depends: P2-T5, P2-T6.

**Phase P2 dependency graph**: P2-T1 → {P2-T2, P2-T3} → {P2-T4, P2-T5}. Tests P2-T6 → {P2-T7, P2-T8, P2-T9} parallelizable.
**Constitution check gate**: All new tests inject `IFileSystem` — no real disk I/O. EHM around all `IFileSystem` calls in production code.

---

## Phase P3 — RmlUi D3D11 backend + shell boot

**Gate to exit**: empty RmlUi Context boots inside the existing window, presents a transparent overlay on top of the emulator viewport, dismisses cleanly on exit, survives device-lost. **Legacy Win32 chrome (menus, OptionsDialog, MachinePickerDialog) remains live in parallel-mode** — this phase only proves composition.

- [ ] **P3-T1** `Casso/Ui/RmlBackend_D3D11.{h,cpp}` — implements `Rml::RenderInterface`: `RenderGeometry`, `CompileGeometry`, `RenderCompiledGeometry`, `ReleaseCompiledGeometry`, `EnableScissorRegion`, `SetScissorRegion`, `LoadTexture`, `GenerateTexture`, `ReleaseTexture`, `SetTransform`. Uses **shared** `ID3D11Device`/`ID3D11DeviceContext` from `D3DRenderer` (no second device). Two HLSL programs: textured + untextured triangle list, premultiplied-alpha. Pre-multiplied-alpha blend state cached; scissor via `RSSetScissorRects`. Acceptance: backend matches `contracts/rml-backend.h` exactly; smoke test in P3-T7 passes. Files: `Casso/Ui/RmlBackend_D3D11.{h,cpp}` (new), `Casso/Shaders/Ui/rml_textured.hlsl` (new), `Casso/Shaders/Ui/rml_untextured.hlsl` (new). Depends: P1-T4.
- [ ] **P3-T2 [P]** `Casso/Ui/RmlSystemInterface.{h,cpp}` — `GetElapsedTime` (QueryPerformanceCounter), `LogMessage` (routes to existing Casso logger), `SetClipboardText`/`GetClipboardText` (Win32), `SetMouseCursor` (LoadCursor). Acceptance: implements every pure-virtual on `Rml::SystemInterface`. Files: `Casso/Ui/RmlSystemInterface.{h,cpp}` (new). Depends: P1-T4.
- [ ] **P3-T3 [P]** `Casso/Ui/RmlInputBridge.{h,cpp}` — maps `WM_MOUSEMOVE`, `WM_LBUTTONDOWN/UP`, `WM_RBUTTONDOWN/UP`, `WM_MBUTTONDOWN/UP`, `WM_MOUSEWHEEL`, `WM_KEYDOWN/UP`, `WM_CHAR`, `WM_SETFOCUS`/`WM_KILLFOCUS`, `WM_IME_*` to `Rml::Context::ProcessXxx` calls. Acceptance: every WM_ in the list produces the documented Rml event in a unit test using a fake `Rml::Context` stub. Files: `Casso/Ui/RmlInputBridge.{h,cpp}` (new). Depends: P1-T4.
- [ ] **P3-T4** `Casso/Ui/UiShell.{h,cpp}` — `Initialize(D3DRenderer&, HWND)`, `Render()`, `Shutdown()`. Creates Rml `Context` sized to client rect; calls `context->Update()` then `context->Render()` between the emulator's `UploadAndPresent` (frame-buffer blit) and `Present()`. Handles device-lost by tearing down + rebuilding backend GPU resources (textures, vertex buffers). Acceptance: empty overlay renders for ≥10s without leaks (validated by D3D11 debug layer in Debug build). Files: `Casso/Ui/UiShell.{h,cpp}` (new). Depends: P3-T1, P3-T2, P3-T3.
- [ ] **P3-T5** Wire `UiShell` into `Casso/EmulatorShell.{h,cpp}` lifecycle. Keep current chrome alive (parallel-mode). Acceptance: launching Casso shows the existing menus AND a transparent RmlUi overlay (proven with a 1-pixel debug rect in a corner that is removed before merging this phase). Files: `Casso/EmulatorShell.{h,cpp}` (modified). Depends: P3-T4.
- [ ] **P3-T6** `Casso/D3DRenderer.{h,cpp}` — expose shared `ID3D11Device*` + `ID3D11DeviceContext*` accessors and add a per-frame hook point `OnAfterEmulatorBlitBeforePresent` invoked by `UploadAndPresent`. Acceptance: hook fires exactly once per frame; ordering verified with a frame-counter assertion. Files: `Casso/D3DRenderer.{h,cpp}` (modified). Depends: P3-T1 (interface contract only).
- [ ] **P3-T7 [P]** Tests in `UnitTest/UiTests/RmlBackendSmokeTests.cpp` — **logic only, no real D3D device**. Use a fake device that records calls. Verify: `EnableScissorRegion(true)` followed by `SetScissorRegion(...)` produces exactly one `RSSetScissorRects` invocation with matching coordinates; `LoadTexture(empty)` returns the expected failure code; the backend never allocates a second device. Acceptance: ≥4 TEST_METHODs; runs in <100ms. Files: `UnitTest/UiTests/RmlBackendSmokeTests.cpp` (new), small `FakeD3DDevice` shim. Depends: P3-T1.
- [ ] **P3-T8 [P]** Tests in `UnitTest/UiTests/RmlInputBridgeTests.cpp` — table-driven WM_ → Rml event mapping using a stub Rml context. Acceptance: every WM_ listed in P3-T3 has at least one positive case. Files: `UnitTest/UiTests/RmlInputBridgeTests.cpp` (new). Depends: P3-T3.

**Phase P3 dependency graph**: P3-T1, P3-T2, P3-T3 parallel after P1-T4. P3-T4 needs all three. P3-T5 needs P3-T4. P3-T6 needs only P3-T1 interface. Tests P3-T7, P3-T8 parallel after their impl.
**Constitution check gate**: No real I/O / no real GPU in tests (Principle II). Backend wraps RmlUi calls in EHM helpers (Principle I — already documented in plan).

---

## Phase P4 — Borderless Win11 chrome + title bar + nav layer

**Gate to exit**: window is borderless on Win11; title-bar drag/min/max/close/double-click-fullscreen all work; Aero Snap + Snap Layouts still appear on hover over maximize button.

- [ ] **P4-T1** [US5] Strip `WS_OVERLAPPEDWINDOW` chrome from main-window registration in `Casso/Main.cpp`; switch to the recipe finalized in P0-T5 (typically `WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX`). Acceptance: window appears borderless on Win11; still resizable from edges. Files: `Casso/Main.cpp` (modified). Depends: P3-T5, P0-T5.
- [ ] **P4-T2** [US5] WM_NCCALCSIZE custom client-area extension + WM_NCHITTEST returning `HTCAPTION` over the title-bar element rect from RmlUi, `HTCLIENT` elsewhere, `HTLEFT/HTRIGHT/HTTOP/HTBOTTOM/HTTOPLEFT/...` for resize edges (8px hit zone), and `HTMINBUTTON/HTMAXBUTTON/HTCLOSE` over their respective RML element bounds so OS-level Snap Layouts surface on hover. Implemented in `Casso/Ui/TitleBarController.{h,cpp}` with a `WM_NCHITTEST` helper queried from the main WndProc. Acceptance: Snap Layouts flyout appears on hover over the RML maximize button on Win11. Files: `Casso/Ui/TitleBarController.{h,cpp}` (new — surface only; logic continues in P4-T4), `Casso/Main.cpp` (modified WndProc). Depends: P4-T1.
- [ ] **P4-T3** [US5] `DwmExtendFrameIntoClientArea` + `SetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND)` + optional `DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_MAINWINDOW` (Mica) on the non-emulated chrome region. Mica is gated by `useMicaBackdrop` in the **active theme**'s `theme.json` (Open Question 6: default false; Dark Modern sets true). Acceptance: corners are rounded on Win11; Mica visible only when the active theme opts in. Files: `Casso/Main.cpp` (modified), `Casso/Ui/TitleBarController.cpp` (modified — applies Mica on theme change). Depends: P4-T2, P5-T2 (theme activation hook).
- [ ] **P4-T4** [US5] `TitleBarController` logic: drives the RML title-bar document, exposes the drag region, dispatches min/max/close, handles double-click on caption to toggle fullscreen via existing `D3DRenderer::ToggleFullscreen`. Acceptance: each of {min, max, restore, close, double-click-fullscreen, drag} works identically to native chrome (manual smoke checklist captured in `specs/007-ui-overhaul/quickstart.md`). Files: `Casso/Ui/TitleBarController.{h,cpp}` (modified). Depends: P4-T2.
- [ ] **P4-T5** [US5] `Casso/Ui/NavLayerController.{h,cpp}` — top-level RML document with one panel per legacy menu (File, Machine, View, Disk, Edit, Help). Each menu-item element dispatches to the **same** command IDs the old menus used (FR-026, SC-006). Acceptance: every command ID present in the legacy menu resource is reachable via the new nav layer; FR-026/SC-006 traceability table appended to quickstart.md. Files: `Casso/Ui/NavLayerController.{h,cpp}` (new), `Resources/Themes/_shared/nav_layer.rml` (new — overridable per theme). Depends: P4-T4.
- [ ] **P4-T6 [P]** Tests in `UnitTest/UiTests/TitleBarHitTestTests.cpp` — table-driven `(clientRect, mousePoint, titleBarElementRect, buttonRects) → HT* result`. Pure-logic; no HWND created. Acceptance: ≥10 TEST_METHODs covering: caption hit, all 8 edges, min/max/close buttons, client area, button-takes-priority-over-caption. Files: `UnitTest/UiTests/TitleBarHitTestTests.cpp` (new). Depends: P4-T2.

**Phase P4 dependency graph**: P4-T1 → P4-T2 → P4-T4 → P4-T5. P4-T3 needs P4-T2 + P5-T2 (cross-phase). P4-T6 parallel after P4-T2.
**Constitution check gate**: Hit-test pure logic is unit-tested with no Win32 dependencies (Principle II). Snap Layouts behavior recorded as a manual quickstart step (no automated test possible).

---

## Phase P5 — Themes (ThemeManager + 3 built-ins)

**Gate to exit**: hot-swap across all three built-ins lands within one rendered frame (≤16ms, SC-002); malformed themes are rejected with structured error (FR-036); user-authored themes discovered without restart (FR-035).

- [ ] **P5-T1** [US4] `Casso/Ui/ThemeLoader.{h,cpp}` — parses `theme.json`, validates `$cassoThemeVersion`, validates `useMicaBackdrop` (bool, default false), resolves asset paths relative to the theme directory, returns a `LoadedTheme` struct **or** a structured `ThemeLoadError` (no exceptions across module boundary). Acceptance: matches `contracts/theme-manager.h` and `contracts/theme-metadata.schema.json`; malformed `theme.json` produces an error with file path + line/column. Files: `Casso/Ui/ThemeLoader.{h,cpp}` (new). Depends: P2-T1.
- [ ] **P5-T2** [US4] `Casso/Ui/ThemeManager.{h,cpp}` — `Discover()` scans `Themes/` for candidate dirs (each containing `theme.json`); `Activate(name)` unloads current Rml documents + stylesheets, clears Rml style-sheet cache, loads new `.rml`+`.rcss`, applies theme's CRT defaults if the user has not overridden them in `GlobalUserPrefs`, raises `OnThemeChanged(LoadedTheme&)` event. `ReloadCurrent()` for dev hot-iteration. Acceptance: hot-swap timing measured ≤16ms in `quickstart.md` dev loop. Files: `Casso/Ui/ThemeManager.{h,cpp}` (new). Depends: P5-T1, P3-T4, P2-T3.

### Built-in themes (parallel — three independent directories)

- [ ] **P5-T3 [P]** [US4] Author **Skeuomorphic** theme — `Resources/Themes/Skeuomorphic/{theme.json, *.rml, *.rcss, assets/, fonts/Inter*, fonts/OFL.txt}`. Palette: beige/cream/anodized. CRT defaults: scanlines off, mild bloom. `useMicaBackdrop: false`. Acceptance: theme loads cleanly; all chrome elements styled. Files: `Resources/Themes/Skeuomorphic/**` (new). Depends: P5-T1, P4-T5 (RML element set frozen).
- [ ] **P5-T4 [P]** [US4] Author **Dark Modern** theme — `Resources/Themes/DarkModern/{...}`. Dark palette + glowing LED accents. CRT defaults: subtle scanlines, neon bloom. `useMicaBackdrop: true` (only built-in that opts in). Inter font. Acceptance: Mica backdrop active when theme is selected on Win11. Files: `Resources/Themes/DarkModern/**` (new). Depends: P5-T1, P4-T5.
- [ ] **P5-T5 [P]** [US4] Author **Retro Terminal** theme — `Resources/Themes/RetroTerminal/{...}`. Phosphor green/amber. CRT defaults: scanlines ON high-intensity, color bleed ON. `useMicaBackdrop: false`. VT323 font (`fonts/VT323*`, `fonts/OFL.txt`). Acceptance: chrome unmistakably "phosphor"; FR-040 acceptance demo passes. Files: `Resources/Themes/RetroTerminal/**` (new). Depends: P5-T1, P4-T5.
- [ ] **P5-T6** [US4] Embed all three built-in themes into the Casso resource script (`Casso/Casso.rc`); verify `EnsureThemes()` (P2-T4) extracts them on first launch. Acceptance: deleting `Themes/Skeuomorphic` and re-launching restores it; user-authored `Themes/MyCustom/` is preserved. Files: `Casso/Casso.rc` (modified), `Casso/Resource.h` (modified). Depends: P5-T3, P5-T4, P5-T5, P2-T4.

### Tests

- [ ] **P5-T7 [P]** [US4] `UnitTest/UiTests/ThemeLoaderTests.cpp` — happy path, missing `$cassoThemeVersion`, future-version (rejected), malformed JSON (structured error returned, not thrown), `useMicaBackdrop` default-false on omitted, missing asset path (structured error). Acceptance: ≥6 TEST_METHODs using `InMemoryFileSystem`. Files: `UnitTest/UiTests/ThemeLoaderTests.cpp` (new). Depends: P5-T1, P2-T6.

**Phase P5 dependency graph**: P5-T1 → P5-T2. {P5-T3, P5-T4, P5-T5} parallel after P5-T1+P4-T5. P5-T6 after all three. P5-T7 parallel after P5-T1.
**Constitution check gate**: Theme JSON parsing is `IFileSystem`-injected; tests use `InMemoryFileSystem`. License files (`OFL.txt`) shipped beside fonts — verified by `scripts/CheckShaderLicenses.ps1` extension (see P8-T7) or a sibling theme-license check (out of scope unless added).

---

## Phase P6 — Custom RmlUi elements (drive widgets + LEDs)

**Gate to exit**: drag-drop AND click-to-browse mount disks via existing `EmulatorShell` command path; spinning + door animations driven by existing motor/disk-active signals; LEDs reflect three states (idle/present/active).

- [ ] **P6-T1** [US3] Record per-element decision in `research.md` R5: RCSS-only for LEDs; custom `Rml::Element` subclass for drive widgets (required because drag-drop + element-owned animation state machine). Acceptance: research.md row updated. Files: `specs/007-ui-overhaul/research.md`.
- [ ] **P6-T2 [P]** [US3] `Casso/Ui/LedElement.{h,cpp}` — RCSS-only — three CSS classes (`.led--idle`, `.led--present`, `.led--active`) toggled from C++ on the parent drive widget. Glow via `box-shadow` / radial gradient in RCSS in each theme. Acceptance: state transitions visible in all three built-in themes. Files: `Casso/Ui/LedElement.{h,cpp}` (new — thin wrapper; mostly C++ class-toggle helpers). Depends: P3-T4.
- [ ] **P6-T3** [US3] `Casso/Ui/DriveWidgetElement.{h,cpp}` — custom `Rml::Element` subclass:
  - Click-to-browse opens `IFileDialog` filtered to `.dsk`, `.nib`, `.woz`, `.po` (FR-022b).
  - Eject affordance: child element with own hit region; dispatches `EmulatorShell` eject command.
  - Spinning animation: RCSS `@keyframes` toggled by `is-spinning` class, driven from the existing CPU-thread motor-on signal observed by the UI thread via `std::atomic<bool>` (matches the existing audio-system read pattern — do NOT introduce a new sync primitive).
  - Door open/close animation: RCSS transition triggered on insert/eject class change.
  Acceptance: all four file extensions filterable; spinning matches audible drive sound. Files: `Casso/Ui/DriveWidgetElement.{h,cpp}` (new). Depends: P3-T4.
- [ ] **P6-T4** [US3] **Single main-window `RegisterDragDrop`** (Open Question 10 decision). `IDropTarget` impl lives in `Casso/Ui/RmlInputBridge.{h,cpp}` (extension): on `Drop`, hit-test the drop point via `Rml::Context::GetHoverElement`; route the file path to the `DriveWidgetElement` under cursor, or reject the drop if none. Acceptance: dragging a `.woz` over drive 1 mounts it; dragging onto empty chrome shows the no-drop cursor and does nothing. Files: `Casso/Ui/RmlInputBridge.{h,cpp}` (modified), `Casso/Main.cpp` (modified — single `RegisterDragDrop` call). Depends: P6-T3.
- [ ] **P6-T5** [US3] `Casso/Ui/DriveWidgetState.h` adds `DriveWidgetState` (per-drive struct matching `data-model.md`: `mountedImagePath` (string), `motorOn` (`atomic<bool>`, written by CPU thread), `diskActive` (`atomic<bool>`, written by CPU thread), `doorState` (enum {Closed, Opening, Open, Closing}, UI-thread only), `animationStartTimeMs` (int64, UI-thread only)). Owned per-drive by `EmulatorShell`; populated from disk insert/eject (UI thread) + motor-on/disk-active signals (CPU thread). Read by `RmlInputBridge` each frame to update element classes. Acceptance: state mirrors the CPU thread within one UI frame; field set matches data-model.md exactly. Files: `Casso/Ui/DriveWidgetState.h` (new), `Casso/EmulatorShell.{h,cpp}` (modified — adds per-drive `DriveWidgetState` members). Depends: P6-T3.
- [ ] **P6-T6 [P]** [US3] Tests in `UnitTest/UiTests/DriveWidgetStateTests.cpp` — pure-logic transitions (insert → present; motorOn → spinning class; eject → door-open then no-present; reject-drop when no widget under cursor). Acceptance: ≥5 TEST_METHODs. Files: `UnitTest/UiTests/DriveWidgetStateTests.cpp` (new). Depends: P6-T5.

- [ ] **P6-T7 [P]** [US3] [US6] Auto-mount last-inserted disks on machine load (FR-047). `EmulatorShell` consumes `lastMountedImages` from the merged `MachineUserConfig`; for each entry, attempt mount via the existing disk-mount path. Missing files: log a warning, clear the entry via `UserConfigStore::SaveDelta`. Acceptance: insert two disks, switch to a different machine, switch back — both disks remount; delete one image file off disk, switch and switch back — that drive starts empty with a warning, the other still remounts. Files: `Casso/EmulatorShell.{h,cpp}` (modified). Depends: P6-T5, P2-T2.

**Phase P6 dependency graph**: P6-T1 → {P6-T2, P6-T3} → {P6-T4, P6-T5} → P6-T6; P6-T7 parallel after P6-T5.
**Constitution check gate**: No new sync primitives — re-use existing `std::atomic<bool>` motor signal (Principle I — simplicity).

---

## Phase P7 — Settings panel

**Gate to exit**: every requirement FR-001 .. FR-011 satisfied; SC-001 (≤60s task completion) and SC-008 (≥90% findability) achievable in UX dry-run; emulation runs uninterrupted while panel is open (FR-041).

- [ ] **P7-T1** [US1] [US2] `Casso/Ui/SettingsPanelState.{h,cpp}` — in-memory transient snapshot. `LoadFromMachine(name, UserConfigStore&)`, `Apply(UserConfigStore&, EmulatorShell&)`, `Cancel()`, `IsDirty()`. **Never touches disk directly** (delegates to `UserConfigStore`). Acceptance: state cleanly cancellable with no side effects. Files: `Casso/Ui/SettingsPanelState.{h,cpp}` (new). Depends: P2-T2.
- [ ] **P7-T2** [US1] [US2] RML layout `Resources/Themes/_shared/settings_panel.rml` (overridable per theme) — sections:
  1. **Machine** selector at top (FR-002).
  2. **Emulation** (speed, write protect, floppy sound + mechanism).
  3. **Video** (color mode, CRT effects: brightness, scanlines, bloom, color bleed).
  4. **Theme** (list of installed themes, preview, refresh button → triggers `ThemeManager::Discover`).
  5. **Hardware** (tree view bound to selected machine's component list, checkbox states from `capabilityFlag`).
  6. Footer: Apply / Cancel / Reset-to-defaults (`UserConfigStore::Reset`).
  Acceptance: all six sections render in all three built-in themes. Files: `Resources/Themes/_shared/settings_panel.rml` (new), `Resources/Themes/_shared/settings_panel.rcss` (new — base styles overridable per theme). Depends: P5-T2, P7-T1.
- [ ] **P7-T3** [US2] [US6] Machine-selector change reloads `SettingsPanelState` from that machine's merged config **without closing the dialog** (FR-002 acceptance). Acceptance: switching between Apple //e and Apple //c re-populates the Hardware tree without reopening. Files: `Casso/Ui/SettingsPanel.{h,cpp}` (new — binds RML doc to state). Depends: P7-T1, P7-T2.
- [ ] **P7-T4** [US2] Hardware tree behavior (FR-004 .. FR-008):
  - `optional` → interactive checkbox.
  - `required` → checked + disabled.
  - `platform-locked` → checked + disabled + tooltip showing `lockReason`.
  Acceptance: Apple //c integrated 80-column card is rendered as platform-locked with the tooltip from P1-T8. Files: `Casso/Ui/SettingsPanel.cpp` (modified). Depends: P7-T3.
- [ ] **P7-T5** [US1] [US2] Apply path:
  - Immediate-effect fields (speed, video mode, floppy sound) push through existing `EmulatorShell` atomics + command queue (FR-011 — no reset).
  - Reset-required fields (hardware tree changes) show a confirmation dialog (RML modal), then schedule a machine reset via existing `EmulatorShell::QueueMachineReset` (FR-010).
  - All applied changes flush through `UserConfigStore::SaveDelta`.
  Acceptance: changing speed reflects within one frame; toggling a hardware checkbox prompts for confirmation then resets. Files: `Casso/Ui/SettingsPanel.cpp` (modified). Depends: P7-T4.
- [ ] **P7-T6** [US1] [US5] Keyboard navigation (FR-044): Tab cycles all controls in visual order; Space/Enter activate; Escape cancels; RCSS `:focus` styles present in all three built-in themes. Acceptance: full keyboard-only traversal of the panel proven in `quickstart.md` UX dry-run. Files: `Resources/Themes/{Skeuomorphic,DarkModern,RetroTerminal}/*.rcss` (modified — add `:focus` rules), `Resources/Themes/_shared/settings_panel.rcss` (modified — Tab order via `tabindex` attributes). Depends: P7-T2, P5-T3, P5-T4, P5-T5.
- [ ] **P7-T7** [US1] Open-while-running (FR-041): panel is purely a view over `SettingsPanelState`; emulation thread is not paused. Acceptance: opening the panel during a disk read does not cause audible glitches; verified via existing audio underrun counter not incrementing. Files: none (architectural; verified by manual test in quickstart.md). Depends: P7-T5.
- [ ] **P7-T8 [P]** Tests in `UnitTest/UiTests/SettingsPanelStateTests.cpp` — IsDirty after edit; Cancel restores; Apply pushes immediate fields through a mock `EmulatorShell`; switching machines reloads state. Acceptance: ≥5 TEST_METHODs; no RML doc instantiated (state is the pure-logic seam). Files: `UnitTest/UiTests/SettingsPanelStateTests.cpp` (new). Depends: P7-T1.

**Phase P7 dependency graph**: P7-T1 → P7-T2 → P7-T3 → P7-T4 → P7-T5 → P7-T7. P7-T6 needs P7-T2 + all built-in themes. P7-T8 parallel after P7-T1.
**Constitution check gate**: All FR-001..FR-011 traced to a specific task — append the traceability table to `quickstart.md`. Panel-state tests inject `EmulatorShell` mock and `UserConfigStore` mock (no real I/O).

---

## Phase P8 — CRT post-processing

**Gate to exit**: 60 fps maintained with all CRT effects on; SC-005 chrome budget (≤1ms) still met; per-theme defaults respected; user override path works; **GPL guard CI script passes**.

- [ ] **P8-T1 [P]** Port `crt_scanlines.hlsl` from upstream `crt-pi` (MIT). Header comment block: original author, upstream URL, SHA of upstream file (from P0-T3), `// SPDX-License-Identifier: MIT`. Acceptance: visually matches reference at scanline-intensity slider mid-position. Files: `Casso/Shaders/CRT/crt_scanlines.hlsl` (new), `Casso/Shaders/CRT/crt_common.hlsli` (new — shared utils). Depends: P0-T3.
- [ ] **P8-T2 [P]** Port `crt_bloom.hlsl` from libretro `bloom` shader (MIT). Separable Gaussian. Same header convention. Acceptance: bloom radius and strength sliders produce visible effect. Files: `Casso/Shaders/CRT/crt_bloom.hlsl` (new). Depends: P0-T3.
- [ ] **P8-T3 [P]** Port `crt_color_bleed.hlsl` from libretro `ntsc-adaptive` chroma stage (MIT). Same header convention. Acceptance: color-bleed width slider produces visible chroma lateral spread. Files: `Casso/Shaders/CRT/crt_color_bleed.hlsl` (new). Depends: P0-T3.
- [ ] **P8-T4** `Casso/Shaders/CRT/crt_composite.hlsl` — takes the emulated frame-buffer SRV + applies enabled effects in order; final output blits to the viewport region (letterboxed/pillarboxed per FR-043). Acceptance: enabling/disabling each effect at runtime works without recompile. Files: `Casso/Shaders/CRT/crt_composite.hlsl` (new), `Casso/Shaders/CRT/LICENSES.md` (new — aggregate license file per-shader). Depends: P8-T1, P8-T2, P8-T3.
- [ ] **P8-T5** `Casso/D3DRenderer.{h,cpp}` exposes a post-process pass run **before** the RmlUi composite pass (P3-T6 hook ordering: emulator-blit → CRT post-process → RmlUi composite → Present). Parameters fed from `GlobalUserPrefs.crt.*`. Acceptance: ordering verified by frame-counter assertions; SC-005 chrome budget measured ≤1ms via the existing frame-time logger. Files: `Casso/D3DRenderer.{h,cpp}` (modified). Depends: P8-T4, P3-T6.
- [ ] **P8-T6** [US1] Brightness control wired live as the user adjusts it (FR-038) — slider in settings panel pushes value via `GlobalUserPrefs` → `D3DRenderer` constant buffer each frame. Acceptance: dragging the slider visibly changes brightness with no detectable lag. Files: `Casso/Ui/SettingsPanel.cpp` (modified), `Casso/D3DRenderer.cpp` (modified — CB upload). Depends: P8-T5, P7-T5.
- [ ] **P8-T7** **GPL guard**: `scripts/CheckShaderLicenses.ps1` — fails if any file under `Casso/Shaders/` contains case-insensitive "GPL", "GNU General Public", or "copyleft" outside designated `// ATTRIBUTION:` comment markers. Wire into `scripts/Build.ps1` so the script runs as a pre-build step on Release configurations. Acceptance: script passes on a clean checkout; deliberately introducing "GPL" in a `.hlsl` causes the build to fail with a clear message. Files: `scripts/CheckShaderLicenses.ps1` (new), `scripts/Build.ps1` (modified). Depends: P8-T4.
- [ ] **P8-T8 [P]** Tests in `UnitTest/UiTests/CrtParameterMappingTests.cpp` — pure-logic mapping of `GlobalUserPrefs.crt.*` values to the constant-buffer struct passed to the shaders; clamp behavior; default values when fields missing. Acceptance: ≥4 TEST_METHODs. No HLSL execution required. Files: `UnitTest/UiTests/CrtParameterMappingTests.cpp` (new). Depends: P8-T5.

**Phase P8 dependency graph**: {P8-T1, P8-T2, P8-T3} parallel after P0-T3. P8-T4 needs all three. P8-T5 → P8-T6. P8-T7 needs P8-T4. P8-T8 parallel after P8-T5.
**Constitution check gate**: Approved Dependencies allowlist (constitution v1.5.0) lists crt-pi, libretro bloom, libretro ntsc-adaptive. `CheckShaderLicenses.ps1` enforces no GPL drift. Principle IV: 60 fps + ≤1ms chrome measured in P8-T5.

---

## Phase P9 — Retirement + polish

**Gate to exit**: legacy code deleted; CHANGELOG + README updated; full test suite green on all four configurations; code analysis green; every acceptance scenario in `spec.md` § User Scenarios passes; constitution v1.5.0 still satisfied.

- [ ] **P9-T1** [US1] [US2] Delete `Casso/OptionsDialog.{h,cpp}`, `Casso/MachinePickerDialog.{h,cpp}`, and their entries in `Casso/Casso.rc` + `Casso/Resource.h` (FR-027). Acceptance: `git grep -i "OptionsDialog\|MachinePickerDialog"` returns no production-code hits. Files: deletions listed above; `Casso/Casso.rc` (modified), `Casso/Resource.h` (modified), `Casso/Casso.vcxproj` (modified — remove from item group). Depends: P7-T5.
- [ ] **P9-T2** [US5] Remove menu-bar registration in `Casso/Main.cpp`; verify every legacy menu command ID is still served by `NavLayerController`. Acceptance: traceability table from P4-T5 stays at 100%. Files: `Casso/Main.cpp` (modified), `Casso/Casso.rc` (modified — remove menu resource). Depends: P4-T5, P9-T1.
- [ ] **P9-T3 [P]** `CHANGELOG.md` entry: `feat(ui): full RmlUi-based chrome + theme system` — list the 48 FRs addressed, the constitution amendment (v1.4.0 → v1.5.0), and the legacy deletions. Files: `CHANGELOG.md` (modified). Depends: P9-T2.
- [ ] **P9-T4 [P]** `README.md` updates: Approved Dependencies table (RmlUi tag + SHA, three shader sources), screenshots of all three built-in themes, user-theme authoring quickstart pointing at `Themes/<your-theme>/theme.json` and the schema in `specs/007-ui-overhaul/contracts/theme-metadata.schema.json`. Files: `README.md` (modified), `docs/themes/AUTHORING.md` (new). Depends: P5-T6.
- [ ] **P9-T5** Final pass: `scripts\Build.ps1 -RunCodeAnalysis` on Debug + Release × x64 + ARM64; full UnitTest run on all four. Acceptance: all four green; code-analysis report clean. Files: none (CI report attached to PR). Depends: P9-T1..P9-T4.
- [ ] **P9-T6** Run every acceptance scenario in `spec.md` § User Scenarios (US1..US6) against the built artifact; record pass/fail in a quickstart-style table in `specs/007-ui-overhaul/quickstart.md` § Acceptance Run. Acceptance: all 6 user stories pass. Files: `specs/007-ui-overhaul/quickstart.md` (modified). Depends: P9-T5.
- [ ] **P9-T7** **Success-criteria measurement run** (SC-001 + SC-008). Document the measurement protocol in `specs/007-ui-overhaul/quickstart.md` § SC Measurement: (a) **SC-001**: launch app cold, time how long from "Settings opened" to "machine switched + speed changed + confirmed", target ≤ 60 s; record the value (must be reproducible). (b) **SC-008**: instead of a formal user study (out of scope for a hobby project), execute a self-administered "first-time user" dry run — fresh git checkout, no prior knowledge bias — and document each step to find/change emulation speed and the elapsed time. Treat ≤ 30 s as a green proxy for 90 % findability. Acceptance: both numbers recorded with timestamp and Git SHA; SC-001 ≤ 60 s; SC-008 proxy ≤ 30 s. Files: `specs/007-ui-overhaul/quickstart.md` (modified). Depends: P9-T6.

**Phase P9 dependency graph**: P9-T1 → P9-T2 → {P9-T3, P9-T4} → P9-T5 → P9-T6 → P9-T7.
**Constitution check gate**: Final gate — every principle re-evaluated; deletions don't introduce regressions; Approved Dependencies table in constitution still matches what's actually vendored.

---

## Cross-Phase Dependencies

```text
P0 ──► P1 ──► P2 ──► P3 ──► P4 ──┐
                       │         ├──► P7 ──► P9
                       └► P5 ────┤
                          │      │
                          └► P6 ─┘
                       P3 ──► P8 ──► P9
```

- **P4-T3** (Mica) crosses into **P5-T2** (theme activation event).
- **P7-T6** (focus styles) requires all three built-in themes (**P5-T3..T5**).
- **P8-T5** (post-process pass ordering) requires the **P3-T6** hook point.
- **P9-T2** (menu removal) requires **P4-T5** (nav layer command parity) to be at 100%.

## Parallel Opportunities

- **Within P1**: P1-T9 (tests) ∥ P1-T8 (machine JSON migration) once P1-T7 is done.
- **Within P2**: P2-T7 ∥ P2-T8 ∥ P2-T9 once impls land.
- **Within P3**: P3-T1, P3-T2, P3-T3 are three independent files — develop in parallel; P3-T6 (D3DRenderer hook) can develop alongside.
- **Within P5**: All three built-in themes (P5-T3, P5-T4, P5-T5) are three independent directories — assign to three contributors.
- **Within P8**: The three shader ports (P8-T1, P8-T2, P8-T3) are independent files.

## MVP Scope

The plan does **not** support a "ship one user story first" MVP because the chrome is replaced wholesale — every user story requires Phases P1..P5 + P7. The natural "first runnable artifact" is **end of P3**: empty RmlUi overlay over the existing emulator, with legacy chrome still serving every command. This is the recommended checkpoint for the first integration demo.

## Schema Rename Callout (P1-T6 .. P1-T8 — explicit)

The `$cassoDefault` → `$cassoMachineVersion` rename is the **only schema-key change** in this feature and it touches **every machine JSON in `Resources/Machines/`**. The migration strategy:

1. **P1-T6**: Loader reads both keys (new takes precedence); writer always emits new key. Legacy alias stays for one upgrade cycle.
2. **P1-T7**: `MachineConfigUpgrade` includes a one-shot, idempotent rename step that also defaults missing `capabilityFlag` fields per FR-015.
3. **P1-T8**: Every shipped default machine JSON under `Resources/Machines/` is updated **at source** to the new key — they should never trigger the upgrade path on a fresh install. The upgrade path exists solely for **user-authored** `<Name>_user.json` files carried forward from prior installs.
4. **P1-T9**: Unit tests prove both round-trip directions and verify the upgrade is idempotent.

## Notes

- **No code is inlined here**; tasks describe the deliverable and acceptance, not the implementation. Implementations belong in PRs.
- Every new module in `Casso/Ui/` and `Casso/Config/` follows the constitution: EHM around external calls (including all RmlUi entry points), single exit, top-of-scope variable declarations, comments live in `.cpp` only, all system/third-party headers go through `Pch.h`.
- "No real I/O in unit tests" is enforced by `IFileSystem` injection in all P2/P5/P7 tests and by `FakeD3DDevice` in P3 tests.
- Commit cadence: one PR per phase is recommended; within a phase, group `[P]` tasks into the same PR where reviewer-friendly.
