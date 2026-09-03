# Quickstart: validating per-field CRT user overrides

How to prove this feature works. Automated coverage first, because most of it is
automated by design. The manual scenarios cover only what the test project
cannot reach.

## Prerequisites

Nothing beyond a normal checkout. The unit tests touch no file system and need
no prefs file, no theme on disk and no device.

## Automated

```bash
scripts\Build.ps1 -Configuration Debug -Platform x64
```

```bash
scripts\RunTests.ps1 -Configuration Debug -Platform x64
```

`RunTests.ps1` does not build. Check that `x64\Debug\UnitTest.dll` is newer than
your build before trusting a green run, or it will confidently pass against a
stale assembly.

If the harness reports the assembly stale for files the test project does not
compile, `-AllowStale` is the right answer rather than `-Build`. That is not the
case for most of this feature, which deliberately adds its files to the test
project, but it does apply to `MonitorCatalog.h`.

**What proves what:**

| Scenario | Where |
|---|---|
| The four-row resolution matrix, per field | `UiTests/CrtResolverTests.cpp` |
| Gamma and persistence never report a theme source | `UiTests/CrtResolverTests.cpp` |
| A theme change leaves user fields intact | `UiTests/CrtResolverTests.cpp` |
| Provenance flips for the touched field only | `UiTests/CrtResolverTests.cpp` |
| Conversion of a v1 document, both flag states | `UiTests/GlobalUserPrefsTests.cpp` |
| Load-save-load is a fixed point | `UiTests/GlobalUserPrefsTests.cpp` |
| Every top-level key appears exactly once | `UiTests/GlobalUserPrefsTests.cpp` |
| Downgrade round trip keeps overrides | `UiTests/GlobalUserPrefsTests.cpp` |
| A legacy upgrade writes the converted document | `UiTests/UserConfigStoreTests.cpp` |
| Shipped monitor identifiers are frozen | `UiTests/MonitorCatalogTests.cpp` |
| Row-to-field mapping and badge strings | `UiTests/DisplayPageTests.cpp` |

The resolution rules, the conversion and the identifier freeze are fully covered
here. Nothing in that list requires launching the application.

## Before running the app: the trap

**The prefs file is shared.** `%LOCALAPPDATA%\Casso\UserPrefs.json` is one file
for every worktree on this machine, and other builds are reading and writing it.
Redirecting `%LOCALAPPDATA%` does not isolate it, because the path resolver
calls `SHGetKnownFolderPath` first and only falls back to the environment
variable.

**To change a setting for one observation without writing to disk**: open the
Settings sheet, change the control, capture or observe while the sheet is still
open, then click Cancel. Settings apply live and Cancel writes nothing.

**Launch in the background** for any run you are doing to check your own work.
Minimized, without activation, and never call `SetForegroundWindow`. The window
is still fully drivable, because posted `WM_MOUSEMOVE` and `WM_LBUTTONDOWN` and
`PrintWindow` capture need neither focus nor foreground.

**Never select the window by title.** Several worktrees run builds with the same
title. Match on the process path containing this worktree's name, for capture,
for `PostMessage` and for `Stop-Process` alike.

```bash
x64\Debug\Casso.exe --machine Apple2e --disk1 Apple2\Demos\casso-rocks.dsk
```

`casso-rocks.dsk` is the most revealing CRT test, because it is a dither, so
bloom and scanline changes show clearly.

## Manual scenario 1: a tweak survives a theme change

The headline behavior. Automated at the resolver level; this confirms the wiring
reaches the picture.

1. Launch on Apple //e with Retro Terminal active.
2. Settings, Display. Note the bloom radius and scanline intensity, and that
   every row reads as a monitor or theme default.
3. Drag bloom strength down. That row now reads custom. No other row changes
   its label.
4. Settings, Theme. Switch to Skeuomorphic. Apply.
5. Back on Display: bloom radius, color bleed and brightness have all moved to
   Skeuomorphic's values, and bloom strength still holds yours and still reads
   custom.

**Expected**: exactly one row custom, ten rows following the new theme.

**Today's behavior, for contrast**: all eleven rows keep Retro Terminal's
values, including color bleed at 1.2 where Skeuomorphic deliberately turns it
off.

## Manual scenario 2: two monitors, two sets of tweaks

1. On the //e, set bloom strength to something obvious.
2. Switch the machine to //c, which uses a different monitor.
3. The //c shows its own defaults, not the //e's tuning.
4. Set a different bloom strength on the //c.
5. Switch back to the //e. Its tuning is intact.

**Expected**: two entries in `crtOverrides`, one per monitor, and neither
affects the other.

**Verify on disk** after closing:

```bash
Select-String -Path "$env:LOCALAPPDATA\Casso\UserPrefs.json" -Pattern "crtOverrides" -Context 0,12
```

## Manual scenario 3: per-row reset and Restore Defaults

1. Override three rows in different groups, for example brightness, bloom
   radius and scanline intensity.
2. Reset one row. Only that row returns to a default label. The other two keep
   their values.
3. Press Restore Defaults. All three return, and nothing is written to express
   that they were removed.

**Expected**: after step 3 the pair has no entry in `crtOverrides` at all,
rather than an entry holding default values.

## Manual scenario 4: the upgrade

The one scenario worth doing against a real file, because it is about a file
this build did not write.

1. Back up your prefs file.
2. Confirm it has a `crt` block and no `crtOverrides`. If every block reads
   `"userOverride": false`, set one to `true` first so the conversion has
   something to carry.
3. Note the eleven values in that block.
4. Launch the new build with the matching machine and mode. The picture is
   unchanged.
5. Close, and inspect the file: `crt` is gone, and `crtOverrides` holds two
   entries carrying those eleven values, one per v1-era monitor.
6. Launch again. The file is unchanged by the second run.

**Expected**: step 4 shows no visible difference, and step 6 proves the trigger
retired.

## Manual scenario 5: the downgrade

1. With a converted file in place, launch an older build from another worktree.
2. Its picture is the preset plus theme look rather than your tuning.
3. Close it, and confirm `crtOverrides` is still present and unchanged in the
   file.
4. Launch the new build again. Your tuning is back.

**Expected**: the older build passes the key through untouched. Step 3 is the
one that matters; step 2 is the accepted cost.

## Gate before proposing to land

```bash
scripts\CheckStyle.ps1 -Against origin/master -Revision HEAD
```

CI runs the style checker in tree mode on every master push, and this feature
rewrites several function banner comments, which is exactly what that checker
polices.

Code analysis is a separate gate and is not reproducible locally. Watch CI
rather than reporting a local run as a passed gate.
