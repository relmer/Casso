# Quickstart: Validating the Dxui Command Widgets

**Feature**: 032-dxui-command-widgets | **Date**: 2026-09-10

Two oracles: pixels for "the emulator did not change", and headless tests
for "the widgets behave on their own". Contracts:
[dxui-command.md](contracts/dxui-command.md),
[dxui-dropdown.md](contracts/dxui-dropdown.md),
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

## 2. Open dropdowns, three themes

For master and branch, in Skeuomorphic, DarkModern and RetroTerminal: open
each top-level menu by click and capture the window. Expected: the same
rows in the same order, the same separators, disabled rows, check marks
and accelerator hints, at the same row height and the same top edge. The
dropdown is narrower than master, fitted to its widest row.

## 3. What is allowed to look different

Two things, and only these:

- Menu bar dropdowns are no longer a fixed 300 dp wide. They fit their
  content, with the accelerator column present only when a row has one.
- The toolbar pickers and the two debug panel right-click menus use the
  dropdown's 14 dp font instead of the old popup menu's 13 dp. Rows stay
  26 dp tall and text starts at the same offset.

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
- `DxuiDropdownTests.cpp`: identical layout from `ShowUnder` and `ShowAt`
  for one list; Down skips a separator and a disabled row and wraps; Right
  opens a submenu, Left closes only it; closed fires before select with
  the committed flag; a disabled command does not dispatch on Enter;
  reopen guard swallows a show inside the close window.
- `DxuiMenuBarTests.cpp`: existing file, unmodified, passing against the
  rebuilt bar.
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

Expected: all pass, zero existing tests modified. Then:

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

Expected: both false. And a grep of `CassoEmuCore/Ui/Chrome` for the old
menu entry struct and the toolbar's entry enum returns nothing.
