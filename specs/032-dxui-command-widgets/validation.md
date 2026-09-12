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

- Phase 6 (CommandToolbar deleted): the chrome capture repeated on the
  Phase 6 Release build and compared against the Phase 5 set, Skeuomorphic
  and DarkModern, 21 files each: every band and every popup byte-identical;
  the one differing file is a full-window frame at row 194, which is
  emulated video below the chrome. Stored under
  `scripts/out/screenshots/branch6-chrome`.
- Toolbar walk by posted clicks on the Phase 6 build and on the master
  baseline (`scratchpad/WalkToolbar.ps1`, frames under
  `scripts/out/screenshots/branch6-walk` and `master-walk`): the theme picker
  opens under its button with the active theme checked, Down previews the
  theme live (the chrome recolors), Escape snaps it back; the color picker
  opens with Color checked; the collapsed input entry opens two rows,
  Joystick checked, since the //e has no mouse; Escape closes each. The
  idle strip and every closed-state frame are byte-identical to master's
  in the top 300 rows; the open-picker frames differ only inside the popup,
  which is the sanctioned 14 dp font.
- Keyboard walk on the Debug build (`scratchpad/WalkKeys.ps1`, frames under
  `scripts/out/screenshots/branch7-keys`): F10 enters the ring on File,
  seven Tabs reach Settings, four more reach Volume with its focus ring
  drawn, Enter opens the flyout at 100 percent, thirty Downs read 70
  percent, a letter typed while the flyout is open does not reach the //e,
  Escape closes the flyout with Volume still focused, and Escape again
  leaves the ring. This is the keyboard access added after the merge, not
  a parity check against master, which had none.
- Debug panel right-click menus (`scratchpad/WalkPanels.ps1`, frames under
  `scripts/out/screenshots/branch7-panels`): each panel opened by a posted
  WM_COMMAND, found by its window class, its column header right-clicked by
  posted input. Disk ][ debug lists Time, Uptime, Cycle, Drive, Event,
  Detail, all checked; Input events lists Wall, Uptime, Cycle, Source,
  Address, Value, Meaning, all checked. A posted click on the first row
  hides that column and relays the list, in both panels.
- Volume flyout by pointer: not provable by posted input. A posted WM_MOUSEMOVE with
  the real pointer elsewhere is followed by the WM_MOUSELEAVE that
  TrackMouseEvent fires, which closes the flyout before a capture; master
  behaves the same in the same frames. Covered by `DxuiToolbarTests`.
- A letter key posted while the theme picker is open reaches the //e on
  BOTH builds: master's frame shows two characters typed after the press,
  the branch's shows two as well. The keydown is gated by the picker in
  `OnKeyDown`, but the WM_CHAR Windows synthesizes from it is not, since
  `OnChar` checks the settings panel, the menu bar and the focus ring and
  not the toolbar picker. Pre-existing; not a regression of this branch. A
  follow-up should add `m_toolbar.IsMenuOpen()` to `OnChar`'s overlay test.

  Closed later in this branch: `8a6d096a` put `m_toolbar.OwnsKeyboard()`
  into that test while landing the strip in the chrome focus ring, which
  covers a picker opened by pointer as well as one opened by Enter. Posted
  input over a `1.24.2` build confirms it: with the Theme picker open two
  letters type nothing, and the same two letters after the picker closes
  type as before.

NOT taken:

- The matrix oracle needs a fresh master baseline on the same machine and
  scale: the existing master matrix sets are an Apple //c at 560 px and do
  not compare to a //e at 700 px. The chrome capture above stands in for it.
- The printer light needs a print in progress and was not exercised;
  `ChromeToolbarPartsTests` proves its color rule, and the strip's idle
  state (light unlit) is in every capture.
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
