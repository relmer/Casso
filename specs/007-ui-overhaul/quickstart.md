# Quickstart — UI Overhaul (Developer)

This guide is for engineers picking up implementation tasks for feature
`007-ui-overhaul`. It is **not** a user-facing quickstart.

## Prerequisites

- Visual Studio 2026 with v145 toolset, x64 + ARM64 build targets installed.
- Windows 11 (any edition) for runtime testing — Win10 is below the new
  floor (FR-042) and **will not be supported** after this feature ships.
- PowerShell 7 (`pwsh`) for the build scripts.
- An audited copy of RmlUi source pinned to the tag agreed in P0-T2 (will
  live under `External/RmlUi/`).

## First-time setup after this branch lands

```pwsh
# 1. Verify RmlUi vendored source is present:
Test-Path External/RmlUi/CMakeLists.txt   # should print True

# 2. Verify the constitution amendment is in:
Select-String -Path .specify/memory/constitution.md -Pattern 'Approved Dependencies'

# 3. Build all configurations:
scripts/Build.ps1 -Configuration Debug   -Platform x64
scripts/Build.ps1 -Configuration Release -Platform x64
scripts/Build.ps1 -Configuration Debug   -Platform ARM64
scripts/Build.ps1 -Configuration Release -Platform ARM64

# 4. Run the test suite (must be green):
scripts/RunTests.ps1
```

## Iteration loop while implementing a phase

```pwsh
# Make changes, then:
scripts/Build.ps1 -RunCodeAnalysis      # build + warnings + analysis must be clean
scripts/RunTests.ps1                    # full test suite

# Launch the app for visual verification:
./x64/Debug/Casso.exe
```

## Theme iteration (Phase 5+)

Themes hot-reload at runtime. With the app running:

1. Edit any `.rml` or `.rcss` file under `Themes/<active-theme>/`.
2. Open Settings → Theme → click the **Reload** button (dev-only UI), or
   just toggle to another theme and back.
3. Visual change should appear within one frame.

To author a brand-new theme from scratch:

1. Copy an existing theme directory: `cp -r Themes/Skeuomorphic Themes/MyTheme`
2. Edit `Themes/MyTheme/theme.json` — change `name` and (optionally)
   `crtDefaults`.
3. Edit the `.rcss` files to taste.
4. In the running app, open Settings → Theme; click **Refresh list**; pick
   "MyTheme".

## Per-machine user-override smoke test

```pwsh
# Locate a default machine JSON:
Get-ChildItem Machines/apple2e/

# Manually craft a user override:
@'
{
  "$cassoMachineVersion": 1,
  "speedMode": "Maximum",
  "videoColorMode": "Green"
}
'@ | Set-Content Machines/apple2e/apple2e_user.json

# Launch; selecting apple2e should reflect these values without touching
# the default JSON.
```

## Adding a new persistable field to MachineUserConfig

1. Add the field to `MachineConfig` (schema + struct).
2. Extend `MachineConfigUpgrade` with a step that defaults missing values.
3. Bump `$cassoMachineVersion` in all default JSONs.
4. Extend `UserConfigStore::SaveDelta` to include the field if changed.
5. Extend `SettingsPanelState` to bind a UI control to it.
6. Tests: round-trip + upgrade + cancel-discards.

## Running the validation suites

Required when touching CPU emulator or assembler — **not** typically
required for this feature, but the constitution mentions them and a UI
change that accidentally touches CPU timing would warrant them:

```pwsh
scripts/RunDormannTest.ps1
scripts/RunHarteTests.ps1 -SkipGenerate
```

## Pitfalls observed during planning

- `#include <RmlUi/Core.h>` only ever in `Pch.h`. Any `.cpp` doing its own
  angle-bracket include violates the project's include rule and will be
  rejected in review.
- RmlUi's own callback APIs may want to throw on error. Don't let them —
  wrap calls in your own helpers that return `HRESULT` and follow EHM.
- The shared D3D device path is precious. Do not let RmlBackend_D3D11
  create a second device; assert in Debug if asked.
- The CPU thread writes `atomic<bool>` motor/disk-active. Reading from the
  UI thread is fine; never write to those flags from the UI thread.
- Drag-drop registration stays on the main window; the per-element route
  is just a hit-test dispatch step inside our drop handler.
