# Quickstart — UI Overhaul (Developer)

This guide is for engineers implementing feature `007-ui-overhaul`.

## Prerequisites

- Visual Studio 2026 (v145), x64 + ARM64 workloads.
- Windows 11 for runtime validation.
- PowerShell for scripts.

## First-time setup

```pwsh
scripts/Build.ps1 -Configuration Debug   -Platform x64
scripts/Build.ps1 -Configuration Release -Platform x64
scripts/Build.ps1 -Configuration Debug   -Platform ARM64
scripts/Build.ps1 -Configuration Release -Platform ARM64
scripts/RunTests.ps1
```

## Iteration loop

```pwsh
scripts/Build.ps1
scripts/RunTests.ps1
.\x64\Debug\Casso.exe
```

## Theme iteration

Themes hot-reload at runtime.

1. Edit `Themes/<active>/theme.json` or assets under `images/` and `sounds/`.
2. Use Settings -> Theme -> Reload (dev command), or switch theme away/back.
3. Changes should appear immediately without restart.

## Visual verification checklist (007 reset)

1. Title/nav text matches Windows 11 system UI font.
2. Drag/min/max/close behavior matches expected native behavior.
3. Drive icons read as Disk ][ for Apple II-family variants.
4. Insert/eject triggers visible door close/open animation.
5. Door animation and drive sounds remain synchronized.
6. Variant switching works for Apple II, II+, IIe, and //c.

## Runtime screenshot validation matrix

Capture and review screenshots for each scenario below during validation:

| ID | Scenario | Required evidence |
|----|----------|-------------------|
| M1 | Startup chrome | Title bar, nav strip, and drive widgets visible |
| M2 | Menu/dropdown open | At least one top-level nav menu expanded |
| M3 | NC controls | Minimize, maximize, and close controls rendered and interactive state visible |
| M4 | Settings open | Settings panel visible with machine selector and keyboard focus cue |
| M5 | Drive closed state | Mounted disk state with door-closed visual |
| M6 | Drive open state | Ejected disk state with door-open visual |
| M7 | Apple II variants | Screenshots for Apple II, II+, IIe, and //c theme variants |

Capture guidance:

- Store captures under `TestResults/007-ui-overhaul/`.
- Name files with matrix id + platform tuple, for example:
  - `M1-startup-win11-intel-1920x1080-100.png`
  - `M2-menu-win11-amd-1280x960-150.png`
- For SC-002 evidence, capture at least two consecutive frames around each theme switch and retain the **first post-switch frame**.

## Disk ][ fidelity + sync acceptance checklist

Use this focused checklist while validating US3:

1. Insert a valid `.dsk/.nib/.woz/.po` image in Drive 1 and Drive 2.
2. Confirm each insert animates **door closing** and transitions to mounted/present state.
3. Start drive activity and verify spinning class + active LED while motor is running.
4. Eject each drive and confirm **door opening** animation and empty-state classes.
5. Verify door-open/door-close visuals and corresponding floppy audio cues begin within one frame.

## US1/US2 machine-scoped persistence notes

Use this focused loop before full matrix capture:

1. Open Settings with at least two machines in the selector.
2. For machine A, set speed to **Maximum**, apply.
3. Switch to machine B and confirm speed immediately reflects machine B's prior value (not machine A's).
4. Toggle one optional hardware component on machine B, apply, confirm reset prompt appears, and reset is dispatched.
5. Move/resize the window on one monitor arrangement, restart, and confirm FR-048 restores the per-monitor placement; then change monitor arrangement and confirm default centered placement is used when no profile exists.
6. Switch back to machine A and confirm machine A speed/component states are unchanged.

Expected persistence artifacts:

- `per-machine user JSON` stores speed under `$cassoUiPrefs.speedMode`.
- Hardware toggles persist as enable-state deltas in `internalDevices[]` / `slots[]` (`type|slot` + `enabled` only).
