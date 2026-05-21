# Quickstart — Full UI Overhaul (007-ui-overhaul)

How to build, run, and validate the feature manually. Unit tests cover the
pure logic; this checklist covers what an actual D3D11 device must render.

## Build

```pwsh
# From repo root. Either x64 Debug or x64 Release.
# Prefer the VS Code task "Build + Test Debug" — it wraps msbuild and runs
# the UnitTest project.
```

After build, `Resources/Themes/` must contain `Skeuomorphic/`, `DarkModern/`,
`RetroTerminal/` next to `Resources/Machines/`. `Casso.rc` embeds each theme
as `RT_RCDATA` for first-launch extraction by `AssetBootstrap::EnsureThemes`.

## First-launch validation

1. Delete `%LOCALAPPDATA%\Casso\Themes\` (or wherever `assetBaseDir` resolves
   to on your machine).
2. Launch `Casso.exe`.
3. Expected: `Themes/` directory is created, all three built-in themes appear
   on disk. Application opens with **Skeuomorphic** active and the borderless
   D3D chrome visible (no Win32 title bar, no menu bar, no status bar).

## Functional checklist (mapped to user stories)

### US1 — Settings panel, machine switch
- [ ] Open the Settings panel from the nav layer.
- [ ] Switch the machine selector from Apple //e to Apple II+ — every other
      control updates within one frame to reflect II+'s persisted values.
- [ ] Change Apple II+ speed to "Maximum"; close + reopen panel — the value
      persists.
- [ ] Switch back to //e — //e's speed is unchanged.

### US2 — Hardware component tree
- [ ] In the hardware tree, optional components show interactive checkboxes;
      required components show checked, disabled; platform-locked components
      show checked, disabled with a tooltip on hover.
- [ ] Uncheck "Disk II Controller (Slot 6)" → apply → confirm reset → machine
      boots without a disk controller. Re-check + apply + reset → controller
      back.

### US3 — Drive widget
- [ ] Drag a `.dsk` file from Explorer onto Drive 1 — door-close animation
      plays, LED brightens, disk label appears.
- [ ] During a disk read, the spin animation runs.
- [ ] Click the eject affordance — door-open animation plays, LED returns to
      idle.
- [ ] Click the drive body (not the eject area) — file-open dialog appears
      filtered to `.dsk`/`.nib`/`.woz`/`.po`.
- [ ] Drag an unrecognized file type — no animation, brief LED flicker or
      error indication.

### US4 — Theme hot-swap
- [ ] In Settings, switch theme from Skeuomorphic to Dark Modern — chrome
      updates within one frame; emulated screen is pixel-identical.
- [ ] Switch to Retro Terminal — scanlines + bloom + bleed visible on the
      emulated viewport; chrome is phosphor green.
- [ ] Switch back to Skeuomorphic — CRT effects disabled (theme defaults).
- [ ] Drop a new theme directory into `Themes/` while running — re-open the
      Settings theme page → new theme appears.

### US5 — Custom title bar + nav
- [ ] Drag the title bar — window moves.
- [ ] Double-click the title bar — fullscreen toggles.
- [ ] Close button → clean shutdown (dirty disk images flushed).
- [ ] Snap (Win+Left/Right), Aero Shake, Task View (Win+Tab) all work →
      proves `WM_NCHITTEST` handling is correct (FR-028).
- [ ] Nav layer items open flyouts; flyouts dismiss on outside click.

### US6 — Per-machine user JSON upgrade
- [ ] Manually edit `<assetBaseDir>/Machines/Apple2e/Apple2e_user.json` so
      `$cassoMachineVersion` is `1` (or rename it to legacy `$cassoDefault`).
- [ ] Launch app — migration runs silently, file is rewritten with v2 schema,
      user customizations are preserved.

### FR-041 — Emulation runs while panel open
- [ ] Start a long-running disk operation, open Settings panel mid-operation —
      operation completes normally with no perceptible pause.

### FR-043 — 4:3 viewport
- [ ] Resize window wider than 4:3 → pillarbox bars appear on both sides.
- [ ] Resize taller than 4:3 → letterbox bars appear top + bottom.

### FR-044 — Keyboard navigation in Settings panel
- [ ] Open Settings, never touch the mouse:
  - Tab / Shift-Tab cycles focus through every interactive control.
  - Space / Enter activates the focused control.
  - Escape closes the panel (discarding changes via the standard prompt).
  - Focus ring is visible in all three themes.

## Performance smoke (SC-005)

With the application running on a mid-range GPU at native resolution:
- Frame time in the dev overlay (Ctrl+Shift+D debug console area) shows
  chrome overhead < 1 ms additional vs. a "chrome disabled" build flag.

## Unit test sweep

```pwsh
# VS Code task: "Build + Test Debug" — runs the full UnitTest project.
# New test files for this feature:
#   UserConfigStoreTests, CapabilityFlagTests, ThemeLoaderTests,
#   ThemeUpgradeTests, SettingsPanelStateTests, ViewportLayoutTests,
#   D3DHitTestTests, plus extensions to MachineConfigUpgradeTests.
```

All tests run without touching disk, registry, or network (Constitution II).

## Known limitations / out of scope

- Software renderer / RDP scenarios: chrome perf target SC-005 does not apply
  (per spec Assumptions).
- The Win32 `OptionsDialog` and `MachinePickerDialog` are fully removed; there
  is no legacy fallback.
- Only the three built-in themes ship. User-authored themes are supported but
  unstyled in shipping examples beyond what the three built-ins demonstrate.
