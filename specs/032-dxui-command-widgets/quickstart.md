# Quickstart: Validating the Dxui Command Widgets

**Feature**: 032-dxui-command-widgets | **Date**: 2026-09-10

Two oracles: pixels for "the emulator did not change", and headless tests
for "the widgets behave on their own". Contracts:
[dxui-command.md](contracts/dxui-command.md),
[dxui-popup-menu.md](contracts/dxui-popup-menu.md),
[dxui-toolbar.md](contracts/dxui-toolbar.md). Model:
[data-model.md](data-model.md).

## Prerequisites

- x64 Debug and Release green via `scripts/Build.ps1` against `Casso.sln`.
  Use `-Target Rebuild` after any virtual is added or removed.
- A master build of `Casso.exe` kept aside for baseline captures, from a
  second worktree or taken before checkout.
- Same machine, same DPI, same window size for both runs.

## 1. Pixel oracle at the default width

```powershell
scripts\CaptureScreenshotMatrix.ps1 -Configuration Release -CassoPath <master>\Casso.exe -OutDir scripts\out\screenshots\master
```

```powershell
scripts\CaptureScreenshotMatrix.ps1 -Configuration Release -OutDir scripts\out\screenshots\branch
```

Crop the menu bar band and the toolbar band from each boot-screen capture
and compare byte for byte. Expected: identical for all four monitor colors.
Neither band contains emulated video, so a difference is a defect.

**Do not compare whole captures.** Measured on the 1.24.2 baseline: the same
binary captured twice produces a different file for three of the four boot
states, because those images carry live emulated video with a blinking cursor
and non-deterministic startup memory. A whole-image hash therefore reports a
difference that means nothing. Cropped to the top 100 rows, which covers both
chrome bands at the default size of 560 by 601, all nine captures were
identical across two runs of the same binary. That control is what makes the
band comparison an oracle rather than a coin toss, and it is worth re-running
whenever the capture method changes.

**Run the capture with a settle delay.** At the default two seconds the script
reached the wrong state for three of the nine captures, producing byte-identical
frames for states that should differ. Six seconds cut that to one. Always hash
the nine outputs and count the distinct values before trusting a set; identical
hashes for states that should differ mean the run is bad, not that the states
match. Use the SAME delay for the master and branch runs, so a timing
difference can never be read as a code difference.

## 2. Open dropdowns, three themes

**Switching theme: two files carry `activeTheme`, and the obvious one loses.**
`GlobalUserPrefs.json` has one at its top level and setting it changes nothing.
The effective one is `activeTheme` under `global` in `UserPrefs.json`. Set both,
then prove it took by comparing bands against the previous theme; a run that
wrote only the first produces nine files, reports success, carries the name of
the theme you asked for and the pixels of the one you already had. That cost
four capture runs to notice.

Redirecting `LOCALAPPDATA` to a throwaway prefs tree does NOT work, so there is
no zero-impact route: `PathResolver::GetLocalAppDataDir` asks
`SHGetKnownFolderPath` first and only reads the variable if that call fails.
Editing the real prefs and putting them back is the way.

**Capture without foreground, at physical pixels.** The baseline was taken
by posting a click to each title and printing the main window plus the
dropdown's own popup window with `PrintWindow`, composited at its offset.
Nothing takes focus: `SetForegroundWindow` is refused to a background process
and `SendKeys` then types into whatever is in front, which happened once. The
capture process must set its DPI awareness before touching the display,
because this monitor runs at 125 percent; a non-aware process gets a
downscaled image, and the popup, being per-monitor aware, prints at physical
size into a virtual-sized bitmap and loses its bottom rows. The procedure
and its lessons are in `scripts/out/screenshots/README.txt`, and the script
is the scratchpad's `CaptureChrome.ps1`; run the same script for the branch.

For master and branch, in Skeuomorphic, DarkModern and RetroTerminal: open
each top-level menu by click and capture the window. Expected: the same
rows in the same order, the same separators, disabled rows, check marks
and accelerator hints, at the same row height and the same top edge. The
dropdown is fitted to its widest row, which for every current menu is
narrower than master's fixed width.

## 3. What is allowed to look different

Three things, and only these:

- Menu bar dropdowns no longer have a 300 dp floor under their width. They fit
  their content down to a 140 dp floor, with the accelerator column present
  only when a row has one. A menu whose content already exceeded 300 dp does
  not move. A menu with no checkable row also loses the 18 dp of check gutter
  that the old width calculation reserved and the old painter never used.
- Accelerator text is right aligned against the trailing padding instead of
  sitting at a fixed 190 dp offset. This follows from the width change rather
  than being chosen: a fixed offset overflows a menu narrower than it.
- The toolbar pickers and the two debug panel right-click menus use the 14 dp
  menu font instead of the old 13 dp. Rows stay 26 dp tall.

Row heights, row order, the top edge and the closed-state bands do not move.
Anything else that differs is a defect.

## 4. Behavior unchanged

On the branch build, exercise each and confirm master-identical results:

- Menu bar: click a title toggles it; hover swaps to an adjacent title
  while open; Alt+letter opens the matching menu; Left and Right swap; Up
  and Down move the highlight and skip separators and disabled rows; Enter
  dispatches; Escape closes and returns focus to the bar.
- Toolbar: narrow the window and record the client width at which each
  entry loses its label, right to left; the sequence matches master.
- Theme picker: arrow through, preview follows; Escape snaps back; Enter
  commits and persists. Monitor color picker: same.
- Volume flyout: hover the button, drag the slider, leave; it closes and
  the level sticks.
- Input entry: expanded, a segment click toggles its mode; collapsed, the
  icon opens the same three choices as a dropdown.
- Printer: start a print, the green dot appears at the glyph corner and
  stays when the entry collapses.
- Reset and Power tips carry the machine's display name.
- Debug panels: right-click opens the menu, a row runs its action.
- Keys while any dropdown is open never reach the emulated machine.

## 5. Headless tests

```powershell
scripts\RunTests.ps1 -Configuration Debug -Filter Dxui
```

Check `UnitTest.dll` is newer than the build. Expected: all pass, and the
new files include nothing from CassoEmuCore. Required coverage:

- `DxuiCommandTests.cpp`: defaults for absent functors; `GetLabelText` and
  `GetShortText` precedence.
- `DxuiPopupMenuTests.cpp`: identical layout from `ShowUnder` and `ShowAt`
  for one list; width fits content and grows only when a row has
  accelerator text; a `ShowAt` near the client edge is clamped inside the
  host rect; Down skips a separator and a disabled row and wraps; Right
  opens a submenu with its first enabled row highlighted, hover dwell
  opens it unhighlighted, hovering another parent row closes it, Left
  closes only it; closed fires before select with the committed flag; a
  disabled command does not dispatch on Enter; reopen guard swallows a
  show inside the close window.
- `DxuiMenuBarTests.cpp`: existing file, item construction retyped to
  `DxuiPopupMenuItem` over `DxuiCommand`, every assertion unchanged,
  passing against the rebuilt bar.
- `DxuiWidgetIDxuiControlTests.cpp`: the three `DxuiPopupMenu` conformance
  rows become `DxuiPopupMenu` rows, plus new `DxuiToolbar` rows.
- `ChromeCommandRoutingTests.cpp`: walks `EmulatorCommands` instead of
  the old menu table; its every-id-present and no-duplicate assertions
  unchanged.
- `ChromeToolbarPartsTests.cpp`: new; `PrinterStatusLed` status-to-color
  rule for each `PrinterStatus`; `InputClusterEntry` segment hit test,
  mode reported per segment, `OnClick` true while expanded and false
  while collapsed.
- `DxuiToolbarTests.cpp`: collapse order at widths fitting five, four and
  none; tooltip and anchor per entry, nullptr in a gap; dispatch fires
  once on down-and-up on one entry, not across entries, not when
  disabled; toggle draws pressed from `isChecked`; picker preview,
  snap-back and single commit; flyout opens on dwell and closes on leave;
  a stub custom entry receives width, layout, paint, tooltip and click.
- One cross-surface test: a command in a menu bar item list, a toolbar
  entry and a context list; flip its `isEnabled` and assert all three
  report disabled.

## 6. Full suite, style, analysis

```powershell
scripts\RunTests.ps1 -Configuration Debug
```

```powershell
scripts\RunTests.ps1 -Configuration Release
```

Expected: all pass. `git diff master -- UnitTest/` shows changes only in
the four files listed in §5 and the new files, and every change in an
existing file is a construction site, not an assertion. Then:

```powershell
git add -A; scripts\CheckStyle.ps1 -Mode Tree
```

Expected: clean. Then the four-configuration rebuild with analysis:

```powershell
scripts\Build.ps1 -Target Rebuild -RunCodeAnalysis
```

## 7. Structural checks

```powershell
Test-Path CassoEmuCore\Ui\Chrome\CommandToolbar.cpp; Test-Path Dxui\Widgets\DxuiPopupMenu.cpp
```

Expected: both false. And a grep of `CassoEmuCore/` for `CommandToolbar`,
`MainMenuCommandEntry` and `DxuiPopupMenu` returns nothing.

**RetroTerminal is excluded from the strip oracle.** Its scanline overlay
differs between two runs of the same binary by several hundred pixels in
the strip rows, measured master against master. Compare that theme's menus by
row content and row positions, and use Skeuomorphic and DarkModern for the
byte comparison.
