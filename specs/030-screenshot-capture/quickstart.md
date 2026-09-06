# Quickstart: Validating screenshot capture

**Feature**: 030-screenshot-capture | **Date**: 2026-09-05

How to prove this feature works end to end. Details of *what* each artifact contains live
in [data-model.md](data-model.md) and
[contracts/screenshot-metadata.md](contracts/screenshot-metadata.md); this page is the
run guide.

## Prerequisites

- A build of `Casso.sln` (not `Casso.vcxproj` -- building the project alone puts the exe
  in a different directory and you end up testing a stale binary).
- `Apple2/Demos/casso-rocks.dsk` for a picture with dithered artwork worth looking at.
- `exiftool` on `PATH` for the metadata checks. Any tool that prints PNG `tEXt` will do.

## Build and unit tests

```powershell
pwsh scripts/Build.ps1 -Configuration Debug
```

```powershell
pwsh scripts/RunTests.ps1 -Configuration Debug
```

`RunTests.ps1` does **not** build. Before trusting a green run, confirm
`UnitTest.dll`'s `LastWriteTime` is newer than your build -- a stale DLL reports a
confident pass against the old code.

The unit tests cover every decision in this feature: mode token round-tripping, plan
resolution for all three modes including the refusal and no-desk-scene cases, per-mode
metadata emission, the filename policy including collisions, the PNG `tEXt` round trip,
and the preferences round trip with unknown-key passthrough. **They do not cover the
readback**, which needs a GPU and a window -- that is what the manual scenarios below are
for.

## Manual validation

Launch minimized so the run does not steal focus:

```powershell
pwsh scripts/Build.ps1 -Configuration Debug; Start-Process .\Casso\x64\Debug\Casso.exe -ArgumentList '--machine Apple2e' -WindowStyle Minimized
```

Boot `casso-rocks.dsk` and answer `M` so the dithered image draws. Select a theme with a
visible CRT look (Retro Terminal) so effects are unmistakable.

### Scenario 1 - Default capture (FR-001..FR-017, SC-001, SC-002)

1. Turn on the frame-rate and scene-pose readouts from the View menu.
2. Press the Screenshot toolbar button.

**Expect**: a notice naming the written file. A PNG in `<Pictures>\Casso Screenshots`
named `Casso <date> <time>.png`. Opening it shows the desk scene with scanlines and
bloom, **no** menu bar, toolbar or drive band, and **no** compass, frame-rate readout or
pose readout. The clipboard holds the same image. No dialog appeared and the machine kept
running.

### Scenario 2 - The three modes (FR-002..FR-006, US2)

Capture once in each mode from Settings > Printing and Screenshots.

**Expect**: `scene` shows the desk scene. `crt` shows the picture with effects and no
scene furniture, sized to the window. `raw` is exactly 560x384 whatever the window size,
with no effects, but still green (or amber) under a monochrome monitor -- the tint lives
in the framebuffer.

Resize the window and repeat: `scene` and `crt` change size, `raw` does not.

### Scenario 3 - Persistence is captured (R-003)

Set a monitor with persistence above zero. Scroll a text listing so trails are visible on
screen, then capture in `crt` mode while they are.

**Expect**: the trails are in the file. This is the scenario that ruled out a one-shot
offscreen render, so it is the one worth checking on any change to the capture path.

### Scenario 4 - Metadata (FR-020..FR-026, US3, SC-005, SC-006)

```powershell
exiftool "$env:USERPROFILE\Pictures\Casso Screenshots\Casso 2026-09-05 143207.png"
```

**Expect**: the entry set for that mode exactly as the
[contract](contracts/screenshot-metadata.md) specifies -- seven entries for `scene`, six
for `crt`, five for `raw`.

**Then check what is absent**: no filesystem path, no user or host name, no GPU string,
no `pHYs`. Grep the output for your Windows account name; it must not appear.

**Round trip the pose**: read `Casso Scene Pose` out of a `scene` capture, restore that
view in Casso, capture again. The two images should match (SC-007).

### Scenario 5 - Destination and the off switch (US4, FR-033, FR-034)

1. Turn file saving off; capture. **Expect**: clipboard receives the image, no file
   written, folder controls visibly disabled.
2. Turn it back on, point the folder at a new directory; capture. **Expect**: the file
   lands there, and the directory is created if it did not exist.
3. Delete the configured folder while Casso is running; capture. **Expect**: it is
   recreated and the capture succeeds.

### Scenario 6 - Edge cases (spec Edge Cases, SC-003)

| Do this | Expect |
|---|---|
| Capture ten times quickly | Ten distinct files, ` (2)`, ` (3)` … suffixes, none overwritten |
| Minimize, then capture in `scene` or `crt` | A clear refusal, not a black or stale image |
| Minimize, then capture in `raw` | Succeeds normally |
| Switch to a compact/flat theme, capture in `scene` | The viewport region, not an error and not a silent mode switch |
| Turn every CRT effect off, capture in `crt` | The picture with no effects, at window size -- not identical to `raw` |
| Open Settings, capture with the sheet over the window | The sheet does not appear in the image |
| Hold the clipboard with another app, capture | File still written, clipboard failure reported |

## Pre-merge gates

```powershell
pwsh scripts/CheckStyle.ps1 -Mode Tree
```

```powershell
pwsh scripts/Build.ps1 -Configuration Release
```

Both configurations must build clean on x64. ARM64 is build-only -- there is no ARM64
device to run on, so x64 Debug + Release green is the bar.

Code Analysis is **not** reproducible locally: CI runs it with settings that surface
warnings local runs miss, so `-RunCodeAnalysis` passing here is not evidence the gate
passes. Watch the CI run after pushing.
