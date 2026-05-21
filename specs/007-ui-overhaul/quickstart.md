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

## P7 verification (Settings panel)

### Open-while-running (FR-041)

The Settings panel is a pure view over `SettingsPanelState` -- the CPU
thread loop in `EmulatorShell::CpuThreadProc` runs independently of
the UI thread and is NEVER blocked by the panel being visible. To
verify manually:

1. Boot Apple //e and let it sit at the BASIC prompt.
2. Open **View > Settings...** (or `Ctrl+,`).
3. Observe the cursor blink continues at the authentic ~1Hz rate.
4. Toggle a Disk II write-protect switch in the panel; the BASIC
   prompt keeps blinking with no glitch.
5. While the panel is open, type into the emulator -- keystrokes
   land in BASIC.

### Keyboard navigation (FR-044)

With the Settings panel open:

* `Tab` walks every interactive control in visual order: machine
  selector -> speed radios -> color radios -> floppy sound checkbox
  -> mechanism dropdown -> WP checkboxes -> hardware-tree
  checkboxes -> theme selector -> CRT controls -> footer buttons.
* `Shift+Tab` reverses.
* `Space` / `Enter` toggle checkboxes and activate buttons.
* `Escape` closes the panel (= Cancel; discards staged changes).
* The focused control receives the visible focus ring defined in
  each theme's `settings.rcss` `:focus` rule.

### FR -> task traceability

| FR | Verified by |
|----|-------------|
| FR-001 | `SettingsPanel::Initialize` constructs single-window panel |
| FR-002 | `SettingsPanelStateTests::MachineSwitch_RebindsToNewMachine` |
| FR-003 | `SettingsUiPrefs::writeProtect[2]` + Apply path |
| FR-004 | `HardwareTreeTests::Extract_PreservesInternalThenSlotOrdering` |
| FR-005 | `HardwareTreeTests::Extract_DefaultCapabilities_PerFR015` |
| FR-006 | `HardwareTreeTests` -- optional rendered interactive |
| FR-007 | `SettingsPanelStateTests::SetHardwareEnabled_RequiredEntryRejected` |
| FR-008 | `SettingsPanelStateTests::SetHardwareEnabled_PlatformLockedRejected` |
| FR-009 | `SettingsPanel::CommitApply` Apply button |
| FR-010 | `SettingsPanelStateTests::Apply_HardwareChangeQueuesReset` |
| FR-011 | `SettingsPanelStateTests::Apply_PushesLiveFieldsThroughSink` |
| FR-041 | Manual verification above |
| FR-044 | Manual verification above + `tabindex` attrs in `settings.rml` |


## P8 -- CRT post-process perf measurement protocol

The CRT pipeline (brightness + scanlines + bloom-h/v/composite + color
bleed + final copy = 7 fullscreen passes) and the RmlUi composite hook are
both wrapped in `ScopedPerfTimer` (`Casso/PerfStats.{h,cpp}`) so SC-005
(`<=1ms` chrome budget) can be verified at runtime without a separate
benchmark harness.

**Labels recorded each frame:**

| Label                              | Source                              |
|------------------------------------|-------------------------------------|
| `D3DRenderer.CrtPostProcess`     | `CrtPostProcess::Process`         |
| `D3DRenderer.RmlUiComposite`     | RmlUi `Context::Render` hook      |

**To measure:**

1. Run a Debug build (`Casso.exe`) and boot an Apple //e disk.
2. Open the Settings panel, enable every CRT effect (scanlines + bloom +
   color bleed) at non-trivial magnitudes (intensity 0.6, bloom radius 3.0
   strength 0.5, color-bleed width 2.0).
3. Let the emulator run for ~10 seconds so the EMA in `PerfStats::Record`
   stabilises.
4. Read back via `PerfStats::Instance().Get("D3DRenderer.RmlUiComposite")`
   (e.g., from a debugger watch window or a debug-only overlay tap). The
   rolling average `avgMs` is the chrome budget; `maxMs` is the worst
   single-frame spike since process start.

Acceptance for SC-005: `D3DRenderer.RmlUiComposite.avgMs <= 1.0`. The
CRT post-process is measured separately; it's expected to dominate the
chrome budget on integrated GPUs at 4K, which is fine -- it's emulator
output, not chrome.

The same `PerfStats` singleton is reused by future passes so adding new
`ScopedPerfTimer` scopes only requires a new label string.
