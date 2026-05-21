# Implementation Plan: Full UI Overhaul

**Branch**: `007-ui-overhaul` | **Date**: 2026-05-20 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/007-ui-overhaul/spec.md`

## Summary

Replace Casso's Win32 chrome (title bar, menu bar, status bar, modal option
dialogs) with a fully custom Direct3D 11 chrome, consolidate every per-feature
setting into a single machine-aware **Uber Settings** panel backed by per-machine
user-override JSON files, and introduce a hot-swappable **JSON theme system**
(`Themes/` bootstrapped via the existing `AssetBootstrap` pattern). The work also
introduces CRT post-processing effects (scanlines, phosphor bloom, color bleed,
brightness) on the emulated viewport, a 4:3 letterboxed viewport, full keyboard
navigation, Windows 11 minimum (Mica + rounded corners), and a unified config
schema versioning convention (`$cassoMachineVersion` for machines,
`$cassoThemeVersion` for themes — renaming the legacy `$cassoDefault`).

The technical approach is three coordinated tracks built on a shared foundation:

1. **Shared foundation (Phase F)** — Schema versioning rename, `UserConfigStore`
   (per-machine JSON shadow + merge + upgrade), `capabilityFlag` schema
   extension, and a `D3DUiContext` (DWrite/D2D-on-D3D11 text + 9-slice sprite
   batcher) that all chrome and the Settings panel render through.
2. **Theme system (Phase T)** — `ThemeManager` + `ThemeData` loaded from
   `Themes/<Name>/<Name>.json`, asset bootstrap, three built-in themes, theme
   upgrade pipeline. Lands before chrome so chrome can pull colors/geometry from
   themes from day one (avoids hardcode-then-themify churn).
3. **D3D chrome (Phase C)** — Borderless window, custom title bar, drive
   widgets (DnD + click-to-browse + animations + LEDs), navigation layer, CRT
   post-FX shaders, 4:3 letterbox.
4. **Uber Settings panel (Phase S)** — D3D-rendered settings panel reading the
   merged `MachineConfig + UserConfig` snapshot, hardware tree with
   `capabilityFlag`-driven enable/disable, transient `SettingsPanelState`,
   keyboard nav. Retires `OptionsDialog` and `MachinePickerDialog`.

## Technical Context

**Language/Version**: C++ stdcpplatest (MSVC v145+, VS 2026)
**Primary Dependencies**: Windows SDK only — Direct3D 11, DXGI, DirectWrite,
Direct2D (D2D1 on D3D11 surface for text/vector — no third-party UI toolkit),
DWM (`DwmSetWindowAttribute`, `DwmExtendFrameIntoClientArea`), Shell32 (DnD,
file dialogs), WASAPI (existing). STL only beyond the SDK.
**Storage**: JSON files on disk under `<assetBaseDir>/Machines/<Name>/` and
`<assetBaseDir>/Themes/<Name>/`. Existing `JsonParser` (in CassoEmuCore) is
reused. Windows Registry retained only as a one-time migration source for
legacy `RegistrySettings`.
**Testing**: Microsoft C++ Unit Test Framework (CppUnitTestFramework). New
tests added to `UnitTest/` project, grouped per component
(`UserConfigStoreTests.cpp`, `ThemeLoaderTests.cpp`, `ThemeUpgradeTests.cpp`,
`SettingsPanelStateTests.cpp`, `CapabilityFlagTests.cpp`, `D3DHitTestTests.cpp`,
extended `MachineConfigUpgradeTests.cpp`). Per constitution II, all I/O is
abstracted — disk reads/writes occur behind a `IUserConfigIo` interface so tests
operate on synthetic byte buffers.
**Target Platform**: Windows 11 (minimum, per FR-042), x64 and ARM64. Windows 11
DWM APIs (`DWMWA_WINDOW_CORNER_PREFERENCE`, `DWMWA_SYSTEMBACKDROP_TYPE` for
Mica) are permitted and used.
**Project Type**: Desktop application — single solution, three production
projects (CassoCore, CassoEmuCore, Casso) plus CassoCli and UnitTest. Most new
code lands in the `Casso` GUI project; schema-versioning and `capabilityFlag`
changes land in `CassoEmuCore` (next to `MachineConfig` and
`MachineConfigUpgrade`).
**Performance Goals**: SC-005 — full chrome layer adds ≤ 1 ms/frame on a
mid-range GPU at native rendering resolution (D3D11 feature level 11.0+). Theme
swap completes within one displayed frame (SC-002). Settings panel must remain
interactive while emulation runs at all speed modes (FR-041).
**Constraints**: 4:3 emulated viewport with letterbox/pillarbox (FR-043). Full
keyboard operability of Settings panel (FR-044). Borderless window must
preserve OS-level window management — snap, Aero Shake, Task View — via correct
`WM_NCHITTEST` (FR-028). Emulation never pauses on settings open (FR-041).
**Scale/Scope**: Three built-in themes, three machine profiles (Apple ][, ][+,
//e), ~12 settings controls, ~20 hardware component rows per machine, two drive
widgets, ~6 nav top-level groups (File / Machine / View / Disk / Edit / Help).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Gates derived from Casso Constitution v1.4.0:

| # | Principle | Gate | Plan Compliance |
|---|-----------|------|-----------------|
| I | Code Quality (EHM, single exit, no nested calls in macros, no mid-scope decls, function spacing, function comments in .cpp only) | All new code follows EHM, `Error:` label, helper extraction for >2 indent levels | ✅ Plan calls out helper-extraction explicitly for theme loader and settings panel hit-testing; no `OnRender`/`HandleMessage` mega-functions allowed |
| II | Testing Discipline (no disk/registry/network in unit tests; abstract behind interfaces) | All file I/O behind `IUserConfigIo` and `IThemeIo` interfaces; D3D rendering itself is excluded from unit tests but layout/hit-test/state is pure-function tested | ✅ See Testing strategy below; mock IO interfaces supply synthetic JSON to tests |
| III | UX Consistency (CLI conventions, stderr, --help) | No CLI surface affected; menu commands retained via D3D nav layer (SC-006) | ✅ Pure GUI feature; CLI unchanged |
| IV | Performance (avoid waste, simple direct impls) | 1 ms chrome budget enforced via single batched draw pass (one sprite batcher, one text layout cache); theme switch uses pre-uploaded textures | ✅ Plan specifies pre-uploading theme assets at theme select-time, single command list per frame for chrome |
| V | Simplicity (YAGNI, SRP, ≤ 50 lines/function, ≤ 2-3 indent beyond EHM) | No animation framework, no scripting engine — direct enums + interpolators; ThemeData is a plain struct, not a class hierarchy | ✅ See Complexity Tracking — zero justified violations |

**Initial gate result**: PASS. No violations require justification.

**Post-Phase-1 re-check**: PASS — the data-model and contract files (see
`data-model.md`, `contracts/`) preserve the single-responsibility split:
`UserConfigStore` owns shadow merge, `MachineConfigUpgrade` owns version
migration (extended only, not redesigned), `ThemeManager` owns theme lifetime,
`D3DUiContext` owns chrome rendering primitives. No God-objects introduced.

## Project Structure

### Documentation (this feature)

```text
specs/007-ui-overhaul/
├── plan.md                # This file
├── spec.md                # Input feature specification
├── research.md            # Phase 0 — decisions resolving NEEDS CLARIFICATION
├── data-model.md          # Phase 1 — entities, fields, state transitions
├── quickstart.md          # Phase 1 — how to build / how to validate manually
├── contracts/
│   ├── machine-user-config.schema.md   # _user.json schema (v1) + merge rules
│   ├── theme.schema.md                 # theme JSON schema (v1) + upgrade rules
│   ├── capability-flag.schema.md       # capabilityFlag values, defaults, tooltip
│   └── d3d-ui-context.md               # D3DUiContext public contract (text, sprite, hit-test, focus)
└── tasks.md               # Phase 2 — produced by /speckit.tasks, NOT this command
```

### Source Code (repository root)

```text
CassoEmuCore/
├── Core/
│   ├── MachineConfig.{h,cpp}              # EXTENDED: + capabilityFlag, + $cassoMachineVersion rename
│   ├── MachineConfigUpgrade.{h,cpp}       # EXTENDED: + v1→v2 capabilityFlag back-fill, + $cassoDefault→$cassoMachineVersion
│   └── MachineUserConfig.{h,cpp}          # NEW: user override struct + merge logic (pure)

Casso/
├── AssetBootstrap.{h,cpp}                 # EXTENDED: + EnsureThemes() mirroring EnsureMachineConfigs()
├── Config/
│   ├── UserConfigStore.{h,cpp}            # NEW: per-machine user JSON load/save/merge — uses IUserConfigIo
│   ├── IUserConfigIo.h                    # NEW: abstract IO seam for tests
│   └── DiskUserConfigIo.{h,cpp}           # NEW: production IO (writes <assetBaseDir>/Machines/<Name>/<Name>_user.json)
├── Theme/
│   ├── ThemeData.h                        # NEW: plain struct — palette, geometry, animation, CRT defaults
│   ├── ThemeLoader.{h,cpp}                # NEW: parse JSON → ThemeData (pure, takes string content)
│   ├── ThemeManager.{h,cpp}               # NEW: list themes, set active, hot-swap, persist selection
│   ├── ThemeUpgrade.{h,cpp}               # NEW: $cassoThemeVersion migration (pattern from MachineConfigUpgrade)
│   └── IThemeIo.h                         # NEW: abstract IO seam
├── Ui/
│   ├── D3DUiContext.{h,cpp}               # NEW: DWrite text + D2D on D3D11 + 9-slice sprite batcher + hit-test
│   ├── ChromeWindow.{h,cpp}               # NEW: borderless window, WM_NCHITTEST, DWM Mica/corners
│   ├── TitleBar.{h,cpp}                   # NEW: drag/double-click/min/max/close
│   ├── NavLayer.{h,cpp}                   # NEW: D3D nav menu replacing Win32 menu bar
│   ├── DriveWidget.{h,cpp}                # NEW: drive face, eject affordance, animations, DnD target, click-to-browse
│   ├── LedWidget.{h,cpp}                  # NEW: soft-glow LED, states {idle, present, active}
│   ├── ViewportLayout.{h,cpp}             # NEW: 4:3 letterbox/pillarbox math + sub-region calc
│   └── CrtPostFx.{h,cpp} + CrtPostFx.hlsl # NEW: scanlines, phosphor bloom, color bleed, brightness
├── Settings/
│   ├── SettingsPanel.{h,cpp}              # NEW: D3D-rendered Uber Settings panel (replaces OptionsDialog)
│   ├── SettingsPanelState.{h,cpp}         # NEW: transient snapshot, apply/cancel semantics (pure)
│   ├── HardwareTreeView.{h,cpp}           # NEW: capabilityFlag-aware tree control
│   └── KeyboardFocusRing.{h,cpp}          # NEW: Tab/Shift-Tab/Enter/Space/Esc focus management
├── D3DRenderer.{h,cpp}                    # EXTENDED: + chrome render pass + CrtPostFx hook
├── EmulatorShell.{h,cpp}                  # EXTENDED: + OpenSettingsPanel(), + theme manager wiring, - statusbar drive parts
├── OptionsDialog.{h,cpp}                  # REMOVED (replaced by SettingsPanel)
├── MachinePickerDialog.{h,cpp}            # REMOVED (machine selector lives inside SettingsPanel)
├── MenuSystem.{h,cpp}                     # REMOVED or reduced to command-id table only (NavLayer owns rendering)
└── RegistrySettings.{h,cpp}               # KEPT, but only used by one-time migration into UserConfigStore on first launch

Resources/
├── Machines/                              # EXISTING — schema migrated by Phase F
└── Themes/                                # NEW — three built-in themes shipped here, extracted by AssetBootstrap
    ├── Skeuomorphic/
    │   ├── Skeuomorphic.json
    │   └── *.png (drive face, eject, LED, title-bar textures)
    ├── DarkModern/
    │   ├── DarkModern.json
    │   └── *.png
    └── RetroTerminal/
        ├── RetroTerminal.json
        └── *.png

UnitTest/EmuTests/                         # all new tests, IO-abstracted
├── UserConfigStoreTests.cpp
├── CapabilityFlagTests.cpp
├── MachineConfigUpgradeTests.cpp          # EXTENDED with v1→v2 cases
├── ThemeLoaderTests.cpp
├── ThemeUpgradeTests.cpp
├── SettingsPanelStateTests.cpp
└── ViewportLayoutTests.cpp                # pure 4:3 math
```

**Structure Decision**: The single-solution layout is unchanged; the feature
slots into existing project boundaries. New code is grouped into four subfolders
of the existing `Casso` project (`Config/`, `Theme/`, `Ui/`, `Settings/`) so
that the chrome/theme/settings concerns stay separate from the legacy top-level
shell files. `MachineUserConfig` and the `capabilityFlag` extension live in
`CassoEmuCore` next to `MachineConfig` because they are part of the machine
schema (and therefore unit-testable without any GUI). Production IO is split
from interfaces (`IUserConfigIo`/`DiskUserConfigIo`, `IThemeIo`/`DiskThemeIo`)
so tests can substitute synthetic byte buffers per constitution II.

## Phases

The work decomposes into a foundation phase (F) and three feature phases (T, C,
S) that depend on F but are largely independent of each other. Tasks within
each phase are sequential; phases T and C can proceed in parallel after F lands.
S depends on both T (for chrome host) and C (for the drawing primitives).

### Phase F — Foundation (blocking)

**Goal**: Schema versioning rename, user-config shadow merge, capabilityFlag,
shared D3D UI context. Nothing user-visible ships in this phase.

| Step | Deliverable | Tests |
|------|-------------|-------|
| F1 | Rename `$cassoDefault` → `$cassoMachineVersion` in all three Resources/Machines/*.json; add `$cassoMachineVersion: 2` and bump embedded version constant | `MachineConfigUpgradeTests` v1-stamp legacy-acceptance case |
| F2 | Extend `MachineConfigUpgrade::ParseStamp` to read either field name (back-compat for in-the-wild v1 files); add migrator for v1→v2 that injects `capabilityFlag` defaults | `MachineConfigUpgradeTests` v1→v2 backfill case |
| F3 | Extend `MachineConfig`/`InternalDevice`/`SlotConfig` to carry `capabilityFlag` enum + optional `lockReason`; JSON parser fills it; defaults per FR-015 (slots = optional, internal = required); add `displayName` | `CapabilityFlagTests` (defaults, explicit values, unknown values) |
| F4 | Create `MachineUserConfig` struct + pure-function `MergeUserOverDefault(defaultCfg, userCfg) → MachineConfig` (in `CassoEmuCore/Core/`) | `UserConfigStoreTests::MergeRespectsUserOverDefault` etc. |
| F5 | Create `UserConfigStore` + `IUserConfigIo` + `DiskUserConfigIo` (`Casso/Config/`); load → upgrade → merge pipeline; one-time `RegistrySettings` import on first launch | `UserConfigStoreTests` with synthetic-byte IO mock |
| F6 | Create `D3DUiContext` (`Casso/Ui/`): wraps DirectWrite (text), Direct2D-on-D3D11 (vector chrome), 9-slice sprite batcher, hit-test rect stack. Single render pass appended to existing `D3DRenderer::UploadAndPresent` | Layout primitives (rect packing, 9-slice slicing math, hit-test stack) tested as pure functions |

### Phase T — JSON Theme System (depends on F6)

**Goal**: themes load, three built-ins ship, hot-swap works.

| Step | Deliverable | Tests |
|------|-------------|-------|
| T1 | `ThemeData` struct + `ThemeLoader::ParseFromString` (pure) | `ThemeLoaderTests` (valid, malformed, missing texture ref) |
| T2 | `ThemeUpgrade::Plan` and migrators (pattern copied from `MachineConfigUpgrade`); `$cassoThemeVersion` field handling | `ThemeUpgradeTests` |
| T3 | `ThemeManager` (`Casso/Theme/`): list/load/setActive; persistence via `UserConfigStore` global section (not per-machine) | unit tests with mock `IThemeIo` |
| T4 | `AssetBootstrap::EnsureThemes` (mirrors `EnsureMachineConfigs`); embed three built-in theme dirs as RT_RCDATA blobs in `Casso.rc` | `AssetBootstrapTests` extension |
| T5 | Author three built-in theme JSONs + reference textures (Skeuomorphic / Dark Modern / Retro Terminal); place in `Resources/Themes/` | Manual visual review per `quickstart.md` |
| T6 | Wire `ThemeManager` into `D3DUiContext` so palette/geometry tokens are queried by chrome widgets | N/A (consumed by Phase C) |

### Phase C — Full Custom D3D Chrome (depends on F6; can start in parallel with T after F)

**Goal**: borderless window + title bar + nav + drive widgets + LEDs + CRT FX
+ 4:3 letterbox. Settings panel still uses the legacy `OptionsDialog` at end of
this phase; Phase S retires that dialog.

| Step | Deliverable | Tests |
|------|-------------|-------|
| C1 | `ChromeWindow` + borderless style + `WM_NCHITTEST` returning per-region codes (HTCAPTION/HTCLIENT/HTLEFT/...); DWM Mica + rounded corners (Win11) | `D3DHitTestTests` for pure rect→hittest mapping |
| C2 | `TitleBar` (drag, double-click fullscreen, min/max/close) | hit-test + state-machine tests |
| C3 | `ViewportLayout` 4:3 letterbox/pillarbox math; integrate with `D3DRenderer` viewport setup | `ViewportLayoutTests` |
| C4 | `LedWidget` (soft glow shader, three states) | state transition tests |
| C5 | `DriveWidget` — face render, eject affordance, click-to-browse (file dialog), DnD target via `RegisterDragDrop`/`IDropTarget`, animation state machine (door open/close, spin), wired to existing `m_driveAudioMixer` motor signals | DnD format negotiation + state machine tested with mock `IDropTargetCallbacks` |
| C6 | `NavLayer` — replaces Win32 menu bar, command-id-driven (reuses existing `IDM_*` command set so `EmulatorShell::HandleCommand` stays unchanged); SC-006 audit checklist | hit-test + flyout state machine |
| C7 | `CrtPostFx.hlsl` shader: scanlines, phosphor bloom (single-pass separable), color bleed, brightness uniform; toggles + params via `ThemeData` defaults + per-user overrides | shader compilation smoke test |
| C8 | Remove status-bar drive parts from `EmulatorShell` (`CreateStatusBar` etc.); migration of drive-status display into `DriveWidget` | regression: existing `EmulatorShellResetTests` still pass |

### Phase S — Uber Settings Panel (depends on F + T + C)

**Goal**: Single D3D-rendered Settings panel; retire `OptionsDialog` and
`MachinePickerDialog`.

| Step | Deliverable | Tests |
|------|-------------|-------|
| S1 | `SettingsPanelState` (pure transient snapshot — machine selector + every control + dirty-flag); apply/cancel semantics | `SettingsPanelStateTests` |
| S2 | `SettingsPanel` shell built on `D3DUiContext` — modal-overlay rendered above chrome, emulation continues (FR-041) | layout tests |
| S3 | Machine selector + reactive control rebind on machine change (US1) — `SettingsPanelState::OnMachineChanged` pulls from `UserConfigStore` | state tests covering "switch machine without close/reopen" |
| S4 | `HardwareTreeView` driven by `capabilityFlag`: interactive / disabled-checked / disabled-checked-with-tooltip (US2, FR-006/007/008) | rendering decisions tested as pure functions |
| S5 | CRT FX controls (brightness + scanline/bloom/bleed toggles + intensities); persisted via `UserConfigStore` user section (not per-machine — they are display prefs, FR-040) | persistence tests |
| S6 | Theme picker page (lists `ThemeManager::AvailableThemes()`, hot-swap via `SetActiveTheme()`) | persistence tests |
| S7 | `KeyboardFocusRing` — Tab order, Space/Enter activation, Escape dismiss, visible focus indicator in all themes (FR-044) | focus-traversal tests |
| S8 | Reset-required prompt for hardware changes (FR-010); immediate apply for non-reset settings (FR-011) | apply-flow tests |
| S9 | Remove `OptionsDialog.{h,cpp}` and `MachinePickerDialog.{h,cpp}`; remove their menu command IDs (or repoint to `SettingsPanel`) | build verification |

### Dependencies summary

```
F (Foundation)
├──> T (Themes)         ─┐
├──> C (Chrome)         ─┼──> S (Settings panel)
└──> (T6 wires F+T into C, can land in T)
```

## Open Technical Questions

Resolved in Phase 0 (`research.md`). Initial unknowns from spec review:

1. ~~How is theme selection persisted — per-machine user JSON or a separate
   global file?~~ → **Resolved**: global section in a new
   `<assetBaseDir>/user.json` (not per-machine), per FR-034 ("not
   machine-specific"). See research.md R-1.
2. ~~CRT FX parameter persistence: per-machine or global?~~ → **Global** per
   FR-040 ("display preferences"). See research.md R-2.
3. ~~Should `MachineConfigUpgrade` be reused verbatim or paralleled by
   `ThemeUpgrade`?~~ → **Paralleled** — `ThemeUpgrade` is a sibling class with
   the same shape but a separate prior-hash table; sharing would couple two
   independent schemas. See research.md R-3.
4. ~~Where does `ThemeManager` live — `Casso` or `CassoEmuCore`?~~ → **Casso**,
   because themes are GUI-only with no emulator semantics. See research.md R-4.
5. ~~Direct2D on D3D11 vs. raw D3D11 for text?~~ → **D2D-on-D3D11 + DWrite** —
   gives subpixel-accurate text and antialiased vector chrome without a third
   party dep, fully compliant with constitution "STL + Windows SDK only". See
   research.md R-5.
6. ~~How does the Settings panel safely read/write `atomic<SpeedMode>` /
   `atomic<ColorMode>` while emulation runs?~~ → **Existing command queue** —
   atomics are written directly for non-reset settings, queue-posted commands
   for reset-required settings. See research.md R-6.
7. ~~First-launch order: theme bootstrap before or after machine bootstrap?~~ →
   **After** — themes can't display until D3D is up, machine bootstrap is
   prerequisite for D3D init order today. See research.md R-7.

No unresolved NEEDS CLARIFICATION remain at end of Phase 0.

## Testing Strategy

Per Constitution Principle II:

- **Pure logic in `CassoEmuCore`** (`MachineUserConfig` merge,
  `MachineConfigUpgrade` v1→v2, `capabilityFlag` defaults) is fully unit-tested
  with synthetic strings — no disk access.
- **All `Casso` GUI logic with IO** is split: a pure parser/state-machine half
  plus a thin IO adapter behind an interface (`IUserConfigIo`, `IThemeIo`,
  `IDropTargetCallbacks`). Tests instantiate the pure half with a mock
  adapter that returns synthetic bytes.
- **D3D rendering itself is excluded** from unit tests (would require a real
  device + present queue). Instead, we test:
  - Layout math (rect packing, 9-slice slicing, 4:3 letterbox, hit-test stack)
    as pure functions.
  - Hit-test region mapping for `WM_NCHITTEST` as a pure function table.
  - Focus-ring Tab order as a pure function over a control list.
  - Theme data binding (palette token lookup, geometry token lookup) as pure
    functions.
- **Manual validation** of the actual visual chrome is covered in
  `quickstart.md` — the visual regression is a manual checklist run by the
  developer before merge, not a test.
- Test files reside in `UnitTest/EmuTests/` (or a new `UnitTest/UiTests/`
  subdirectory if we add a separate `.vcxproj` filter — TBD in Phase 2 tasks).

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| _None_    | _None_     | _None_                               |

The plan introduces no new top-level projects, no new third-party deps, and no
new abstractions beyond what the spec's three areas explicitly require. Each
new class has one responsibility (per Principle V). The "parallel
`ThemeUpgrade` instead of generalized `JsonSchemaUpgrade`" decision is the only
place we deliberately accept duplication over abstraction — justified in
research.md R-3 by YAGNI (only two schema types exist today; generalizing on a
sample size of two would be speculative).

## Artifacts Generated By This Plan

- `plan.md` (this file)
- `research.md` (Phase 0)
- `data-model.md` (Phase 1)
- `contracts/machine-user-config.schema.md` (Phase 1)
- `contracts/theme.schema.md` (Phase 1)
- `contracts/capability-flag.schema.md` (Phase 1)
- `contracts/d3d-ui-context.md` (Phase 1)
- `quickstart.md` (Phase 1)
- `.github/copilot-instructions.md` SPECKIT block updated to point here
