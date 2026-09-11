# Validation: Dxui Command Widgets

**Feature**: 032-dxui-command-widgets | **Date**: 2026-09-11

What was proved, how, and what was not. Procedures are in
[quickstart.md](quickstart.md); this records outcomes.

## Headless (quickstart §5, §6)

| Gate | Result |
|------|--------|
| Debug x64 suite | 5167 of 5167 passed after Phase 6 |
| Release x64 suite | 5165 of 5165 passed after Phase 6 |
| `CheckStyle.ps1 -Mode Tree` | 1445 files, clean |
| `git diff master -- UnitTest/` | nine files: five new, `DxuiMenuBarTests` and `DxuiWidgetIDxuiControlTests` changed at construction sites only, `ChromeCommandRoutingTests` re-pointed at `EmulatorCommands` with every assertion kept and one added, `UnitTest.vcxproj` entries |
| Four-configuration rebuild with analysis | see the note at the end of this file |

New test files and what they hold:

- `DxuiCommandTests.cpp`: functor defaults and label precedence.
- `DxuiPopupMenuTests.cpp`: 19 tests over both openers, content-fit width,
  clamping, navigation, submenus, callback order, disabled rows, reopen guard.
- `DxuiToolbarTests.cpp`: collapse order at three widths, tooltips, dispatch
  rules, toggle, picker preview and snap-back and single commit, flyout dwell
  and collapse, stub custom entry; plus three `IDxuiControl` conformance rows.
- `DxuiCommandSurfacesTests.cpp`: one command in a menu bar list, a toolbar
  entry and a popup list; an enabled flip and a label change land on all three.
- `ChromeToolbarPartsTests.cpp`: printer light colors, input cluster segments,
  tips, collapsed click, picker rows.

## Structural (quickstart §7)

- `CassoEmuCore/Ui/Chrome/CommandToolbar.cpp` does not exist.
- `Dxui/Widgets/DxuiPopupMenu.cpp` EXISTS. The plan's original intent to
  replace it with a new widget was reversed in Phase 3 at the owner's
  direction: the widget was extended in place and is the one dropdown the
  menu bar, the toolbar pickers and the context menus share, so a grep of
  `CassoEmuCore/` for `DxuiPopupMenu` returns its item type in
  `EmulatorCommands`, which is the intended use.
- A grep of `CassoEmuCore/` for `CommandToolbar` and `MainMenuCommandEntry`
  returns nothing.
- A grep of `Dxui/` for `CassoEmuCore`, `CassoTheme`, `Resource.h` and `IDM_`
  returns one pre-existing comment in `DxuiTheme.h` and nothing new.

## Pixels (quickstart §1 through §4)

Taken so far, all against the master baseline built at 8e58f86b:

- Phase 4 (menu bar over the shared dropdown): every top-level menu in
  Skeuomorphic, DarkModern and RetroTerminal, 21 popups, same rows, same
  order, same heights and top edges; widths fitted per the three sanctioned
  changes in §3 (e.g. Machine 375 to 346 px physical). The closed strips in
  Skeuomorphic and DarkModern were byte-identical; RetroTerminal's scanline
  overlay differs run to run even master against master and is excluded.
- Phase 5 (toolbar behind the wrapper): the toolbar rows outside the open
  popup matched master in every capture in both comparable themes, including
  master's own Settings-label flip between captures.

NOT taken:

- No capture after Phase 6. Every capture run launches the emulator, and the
  matrix script takes the foreground from the operator; the Phase 5 run
  interrupted his typing. The Phase 6 capture and the quickstart §4 walk
  (pickers, volume flyout, input entry, printer light, keys while a dropdown
  is open, debug panel right-click) are owed before the master merge and need
  a window when nobody is at the machine, or the operator doing §4 by hand.
- The matrix oracle needs a fresh master baseline on the same machine and
  scale: the existing master matrix sets are an Apple //c at 560 px and do
  not compare to a //e at 700 px.
- Collapse widths are proved by `DxuiToolbarTests` (`PlanForWidth`), not by
  a window sweep.

## Allowed differences observed (quickstart §3)

1. Dropdowns fit their content down to the 140 dp floor; the check gutter
   appears only in a list with a checkable row. Seen on every menu.
2. Accelerator text is right aligned against the trailing padding. Seen on
   every menu with accelerators.
3. Toolbar pickers and the debug panels' right-click menus use the 14 dp
   menu font. Not captured (see above).

Nothing else was seen to differ in the captures taken.
