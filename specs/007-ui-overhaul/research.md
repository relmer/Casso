# Phase 0 Research — Full UI Overhaul

This document resolves the open technical questions identified in `plan.md`.
Each decision is captured as Decision / Rationale / Alternatives.

---

## R-1 — Theme selection persistence location

**Decision**: Persist the active theme name in a new
`<assetBaseDir>/user.json` file under a `global` section. Not per-machine.

**Rationale**: FR-034 explicitly states theme is "not machine-specific, as
theme is a UI preference not a machine property". A global file keeps the
theme selection portable across machine switches and avoids replicating it in
every `_user.json`. Co-locating it with other future global preferences
(`crtBrightness`, scanline toggles per FR-040) gives one logical home for "user
display prefs".

**Alternatives considered**:
- Store in every per-machine `_user.json` → violates DRY, contradicts FR-034.
- Store in Windows registry under `HKCU\Software\relmer\Casso\Theme` → moves us
  back toward what the spec explicitly retires (FR-016).
- Store in a separate `theme.json` → splits global prefs across multiple files
  for no benefit; one `user.json` aggregates all global prefs.

---

## R-2 — CRT FX parameter persistence (brightness, scanlines, bloom, bleed)

**Decision**: Persist user overrides in the global `user.json` (R-1), under a
`crt` section. Per-theme defaults live in each theme's JSON.

**Rationale**: FR-040 says user CRT overrides are "not machine-specific — they
are display preferences". The defaults belong to the theme so a user switching
themes still gets the theme's intended look until they personally override.

**Alternatives considered**:
- Persist per-machine → contradicts FR-040.
- Persist per-theme (overwrite theme JSON when user adjusts) → would mutate
  user-authored or built-in theme files. Built-in themes are re-extracted on
  startup (FR-037), which would wipe the user's settings.

---

## R-3 — Generalize `MachineConfigUpgrade` vs. parallel `ThemeUpgrade`

**Decision**: Create a parallel `ThemeUpgrade` class with the same shape (Plan
+ priorHash table + NormalizeBytes/ParseStamp/BytesToHex helpers) rather than
introducing a generic `JsonSchemaUpgrade<T>` template.

**Rationale**: Constitution Principle V ("YAGNI", "Simplicity"): only two schema
types exist today (machines + themes). Generalizing on a sample of two is
speculative — the two schemas' embedded resource enumeration, prior-hash table
layout, and per-type upgrade logic are different enough that the abstraction
would be 90% boilerplate and 10% genuine sharing. If a third schema type
appears, that is the right time to refactor.

**Alternatives considered**:
- Generic `JsonSchemaUpgrade<TStamp, TPriorHash>` template → premature
  abstraction; will be revisited if/when a third schema appears.
- Reuse `MachineConfigUpgrade` directly with a different prior-hash list →
  conflates two unrelated schemas under one class name; bad SRP.

---

## R-4 — `ThemeManager` project location

**Decision**: `ThemeManager` lives in the `Casso` GUI project under
`Casso/Theme/`.

**Rationale**: A theme has no emulator semantics — no machine, CPU, or device
ever reads from it. The emulator core projects (`CassoEmuCore`,
`CassoCore`) must remain testable headless and free of GUI concerns.
`ThemeLoader::ParseFromString` is pure and could in principle live in core, but
keeping it next to its only consumer (`ThemeManager`) is the simpler choice
(Principle V).

**Alternatives considered**:
- `CassoEmuCore` → adds a GUI concept to an emulator-only library; no
  CassoCli benefit.
- Brand-new `CassoUiCore` static lib → infrastructure not yet justified by the
  scope; only one consumer.

---

## R-5 — Text + vector chrome rendering: D2D-on-D3D11 + DWrite

**Decision**: Use Direct2D on a shared D3D11 surface for vector chrome and
DirectWrite for text layout/rendering. The existing `D3DRenderer` swap chain
remains the present target. Sprite atlases (drive faces, LED glow, title-bar
textures) are uploaded as `ID3D11ShaderResourceView`s and drawn through a
simple instanced quad batcher (`D3DUiContext`'s sprite path).

**Rationale**:
- DWrite/D2D ship in the Windows SDK — no third-party dep (Constitution
  "Dependencies: Windows SDK, STL only; no third-party libraries").
- DWrite gives proper subpixel-accurate text, kerning, and IME support — needed
  for FR-044 (keyboard navigation: focus indicators) and for the Settings
  panel's labels.
- D2D-on-D3D11 (via `ID2D1Device::CreateDeviceContext` + DXGI surface) shares
  the existing swap chain — no extra HWND, no extra device-lost surface.
- Sprite batching gives the SC-005 budget headroom (≤ 1 ms) — chrome is a few
  hundred quads at most.

**Alternatives considered**:
- Raw D3D11 + bitmap fonts → ugly text, no kerning, IME pain. Rejected.
- GDI+ / Win32 controls → defeats the entire "fully D3D chrome" goal (FR-018,
  FR-027).
- ImGui or other third-party UI lib → forbidden by Constitution.

---

## R-6 — Settings panel ↔ live emulation interaction (FR-041)

**Decision**: The Settings panel reads emulator state (speed mode, color mode,
component enable flags) from existing `atomic<SpeedMode>` / `atomic<ColorMode>`
fields and the merged `MachineConfig` snapshot in `EmulatorShell`. On apply:
- Non-reset settings (speed, color, CRT FX params, drive audio toggle) are
  written directly to the atomics — they are picked up on the next CPU frame
  with no further coordination.
- Reset-required settings (hardware component enable/disable, slot
  reconfiguration) are posted to the existing `m_commandQueue` as an
  `EmulatorCommand` that triggers `SwitchMachine`/`PowerCycle` on the CPU
  thread. The user is prompted before this happens (FR-010).

**Rationale**: No new threading primitive needed; the spec acknowledges
emulation keeps running (FR-041) so the snapshot model + atomics + existing
command queue is enough. The transient `SettingsPanelState` lives entirely on
the UI thread and only mutates the emulator on Apply.

**Alternatives considered**:
- Pause emulation while panel is open → contradicts FR-041.
- A new mutex-guarded "live settings" struct → over-engineering for what
  atomics already cover.

---

## R-7 — First-launch bootstrap order

**Decision**: `AssetBootstrap::EnsureMachineConfigs` runs first (unchanged),
then `AssetBootstrap::EnsureThemes` runs immediately after, both before D3D
initialization. Theme assets must be on disk before `ThemeManager` is
constructed.

**Rationale**: Symmetry with the existing machine bootstrap; both are pure
disk operations that don't need D3D. Constructing `ThemeManager` after both
ensure-calls means the active-theme load on startup can succeed on first run.

**Alternatives considered**:
- Lazy bootstrap on first SettingsPanel open → leaves the application chrome
  unstyled on startup, contradicts the "every visible pixel" goal.

---

## R-8 — Drag-and-drop registration with borderless window

**Decision**: `EmulatorShell` registers a single `IDropTarget` on its HWND;
the drop target dispatches to `DriveWidget`s based on screen-coordinate hit
testing through `D3DUiContext`. Existing `RegisterDragDrop` path is retained
(today it routes by HWND owned by the status bar — that goes away with the
status-bar removal in Phase C8).

**Rationale**: A borderless window still has exactly one HWND, so one
`IDropTarget`. Routing to widgets by hit-test keeps the per-widget logic clean
and lets the LED "unrecognized drop" feedback (FR-022 / acceptance #5) live
inside `DriveWidget`.

---

## R-9 — Per-machine vs. per-user CRT brightness (FR-038 vs. FR-040)

**Tension**: FR-038 says brightness is "persisted per-machine in the user
JSON"; FR-040 says CRT effect overrides are "not machine-specific — they are
display preferences" and live in the user JSON (singular, global).

**Decision**: Treat **brightness as global** (with the other CRT params) in
the global `user.json`. Flag the inconsistency in spec for clarification — the
weight of the spec (FR-040 explicit, edge-case rationale "display preferences")
points to global; FR-038's "per-machine" reads like an oversight from before
FR-040 was added.

**Action item for /speckit.clarify (if rerun)**: Confirm with the author that
FR-038 should be amended to "persisted globally with the other CRT params per
FR-040". Implementation proceeds with the global interpretation; if clarified
otherwise, the change is local to `SettingsPanelState` + `UserConfigStore`.

---

## R-10 — Windows 11 minimum & DWM API set

**Decision**: Use the following Win11 DWM APIs:
- `DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, ...)`
  with `DWMWCP_ROUND` for rounded corners.
- `DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, ...)` with
  `DWMSBT_MAINWINDOW` (Mica) for the title-bar backdrop.
- `DwmExtendFrameIntoClientArea` with `MARGINS = {-1,-1,-1,-1}` so the system
  composes the shadow under our borderless client area.
- `WM_NCHITTEST` returning `HTCAPTION`/`HTCLIENT`/`HTLEFT`/`HTRIGHT`/
  `HTTOP`/`HTTOPLEFT`/.../`HTBOTTOMRIGHT` for snap, Aero Shake, Task View
  compatibility (FR-028).

**Rationale**: Spec FR-042 grants Win11-only APIs. These are the standard
borderless-window-with-system-chrome-feel recipe.

**Alternatives considered**:
- Pure custom shadow drawn in D3D → loses OS integration (no Aero Shake on
  the title-bar drag). Rejected.

---

## Resolution summary

All NEEDS CLARIFICATION items raised during Technical Context fill are
resolved (R-1 through R-10). R-9 carries a spec-clarification request that
does not block implementation.
