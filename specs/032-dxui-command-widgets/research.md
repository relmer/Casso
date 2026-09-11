# Research: Dxui Command Widgets

**Feature**: 032-dxui-command-widgets | **Date**: 2026-09-10

No NEEDS CLARIFICATION markers came out of the spec. The questions below are
the design choices the work forces, each settled by reading the code as it
stands or by the model WPF and WinUI use.

## R1. The model being adopted

**Finding**: WPF's `RoutedUICommand` and WinUI's `XamlUICommand` are one
object carrying label, icon, description, accelerator, access key, execute
and can-execute, and enabled state. A `MenuItem`, a toolbar button and a
context menu item bind to it; enabled and label changes propagate to every
surface. WinUI's dropdown, `MenuFlyout`, is the same type whether opened
from a `MenuBarItem`, an `AppBarButton`'s flyout property, or a right-click.
The toolbar knows nothing about menus; it opens whatever flyout the button
holds.

**Decision**: three library types. `DxuiCommand` is the declaration.
`DxuiDropdown` is the one dropdown, evolved from `DxuiPopupMenu`.
`DxuiToolbar` is the strip. `DxuiMenuBar` becomes titles over `DxuiDropdown`.

## R2. What exists and where it diverges

**Finding**:

- `DxuiMenuBar` (`Dxui/Widgets/DxuiMenuBar.h`) paints its own dropdown. Its
  `DxuiMenuBarSubitem` carries label, dispatch, `isChecked`, `isEnabled`,
  `labelText` functors, `isSeparator`, an accelerator hint and `enabled`.
  Titles carry `&X` mnemonics. It does not use `DxuiPopupMenu`.
- `DxuiPopupMenu` (`Dxui/Widgets/DxuiPopupMenu.h`) is label plus checked
  per item, owner-routed, with `SetOnSelect`, `SetOnHighlightChange`,
  `SetOnClosed (committed)` fired closed-before-select, and opt-in popup
  hosting via `DxuiHwndSource`'s pool with submenu chaining through
  `DxuiPopupHost::SetParentPopup`. Consumers: `CommandToolbar` (three
  pickers), `Disk2DebugPanel` and `InputDebugPanel` (right-click).
- `CommandToolbar` (`CassoEmuCore/Ui/Chrome/CommandToolbar.cpp`, 1,958
  lines, 44 functions) holds button chrome, collapse, tooltips, three
  pickers, a hover flyout with `DxuiSlider`, and the emulator's entries,
  printer LED, input cluster and tips. No unit test constructs it.
- `MainMenu` (`CassoEmuCore/Ui/Chrome/MainMenu.h`) derives from
  `DxuiMenuBar`; its static `s_kEntries` table is `{commandId, menu, label,
  accelerator, checkable}` and it maps `WORD` ids to dispatch, check and
  label functors.

**Decision**: `DxuiMenuBarSubitem` is already most of a command. It becomes
`DxuiCommand` plus a dropdown item wrapper. `DxuiPopupMenu` already has the
hosting, the callbacks and the submenu chain; it becomes `DxuiDropdown` by
taking the richer item model and the menu bar's painting. The menu bar
keeps only what is bar-specific.

## R3. DxuiCommand fields

**Decision**: id (int), label, optional short label, glyph, tip,
accelerator text, dispatch functor, checked functor, enabled functor, label
functor. Static `enabled` and `checked` booleans are dropped; a functor
returning a constant covers them and keeps one code path. Ids are ints so
`IDM_*` values cast without change and `HandleCommand (WORD)` stays the
emulator's dispatch target.

**Alternatives considered**: a class hierarchy with toggle and picker
subtypes. Rejected; kind is a property of the placement, not of the
command. Reset is one command whether it sits in a menu or a toolbar.

## R4. Dropdown item model

**Decision**: `DxuiDropdownItem` is a variant of three kinds: a command
reference, a separator, and a command reference with a child item list. The
command is held by pointer to a `DxuiCommand` the application owns, so
placement never copies a declaration. The dropdown evaluates the functors
at paint and at key navigation.

**Alternatives considered**: items owning a copy of their command. Rejected;
copies are the defect being removed.

## R5. Menu bar over the dropdown, pixel identical

**Finding**: the menu bar's dropdown metrics, check glyph, accelerator
column and disabled color live in `DxuiMenuBar.cpp`; `DxuiPopupMenu` has
its own, simpler metrics. They differ today.

**Decision**: `DxuiDropdown` takes the menu bar's metrics and painting,
since the menu bar is the reference surface the emulator user sees most.
The toolbar pickers and the two debug panel menus therefore change
appearance to match the menu bar. That is a visible change in three
places, and the spec's identical-window bar is scoped to the menu bar, the
toolbar band and the closed state of the debug panels; the open picker and
open debug menus are compared for row content and behavior, not pixels.
This is stated in quickstart so no one treats it as a regression.

**Alternatives considered**: keeping two skins on one widget. Rejected; a
skin switch is a second code path with no consumer wanting it.

## R6. The toolbar's generic and specific split

**Finding**: of the 44 functions in `CommandToolbar.cpp`, the
emulator-specific ones are the constructor's entry table,
`RebuildActionTips`, `SetMachineDisplayName`, `SetFullscreen`, `SetThemes`,
`SetThemeIndex`, `SetMonitorColorIndex`, `GetStatusCoreColor`,
`SetInputState`, `InputSegSelected`, `IsInputExpanded`, `PaintInputCluster`,
`GetGlyphStroke`, `PaintJoystickMono`, `PaintPaddleMono`, and input branches
inside `GetEntryWidthPx`, `Layout`, `GetTooltipAt`, `OnToolbarLButtonUp`
and `PaintEntryIcon`. The rest is generic.

**Decision**: the generic set becomes `DxuiToolbar`. The printer LED becomes
a decoration callable. The input cluster becomes a class implementing
`IDxuiToolbarCustomEntry`. The tips become the commands' tooltips, with the
machine-name tips as label functors. The three row lists become picker
content. `CommandToolbar` is deleted; the shell holds a `DxuiToolbar`.

## R7. Theme access

**Finding**: `CommandToolbar::Paint` downcasts to `CassoTheme`. Every color
it reads except `ledActive` is on `DxuiTheme`.

**Decision**: the widgets read `DxuiTheme` fields through `IDxuiTheme` and
never downcast. The LED decoration receives the same `IDxuiTheme&` and may
downcast inside emulator code. The icon face is a setter defaulting to
Segoe MDL2 Assets. Glyph codepoints go through `Dxui/Core/UnicodeSymbols.h`.

## R8. Input routing

**Finding**: the shell hand-routes mouse and key events to the toolbar and
the menu bar; `CommandToolbar` does not implement `IDxuiControl::OnMouse`.

**Decision**: the widgets keep explicit handlers with plain coordinates.
Routing through the panel tree is out of scope by the spec's assumptions.

## R9. Regression oracles

**Finding**: `scripts/CaptureScreenshotMatrix.ps1` captures nine states at
the default size, varying monitor color and settings tab, not width, theme
or DPI. No unit test constructs the toolbar; `DxuiMenuBarTests.cpp` is the
headless model with `MockDxuiTextRenderer`, `MockDxuiPainter` and
`MockDxuiTheme`.

**Decision**: pixels for the menu bar band, each open menu bar dropdown,
and the toolbar band, in all three themes, against a master binary on the
same machine. Headless tests for command propagation, dropdown layout and
navigation from three anchors, toolbar collapse and dispatch. Existing
tests untouched; `DxuiMenuBarTests.cpp` must keep passing against the
rebuilt bar, which is the strongest behavioral oracle the tree has.

## R10. Style constraints that bite this work

- Function names are VerbNoun; `OnXxx` handlers exempt; `IDxuiTheme` color
  accessors keep bare nouns (`docs/coding-standards-backlog.md` items 6, 7).
- Helpers are class statics; `PaintStatusLed` moves from a free static into
  the LED class.
- Splice ahead of `////` banners; `scripts/CheckStyle.ps1 -Mode Tree` after
  `git add -A` before the merge.
- A vtable change needs `-Target Rebuild`; the dropdown and menu bar both
  gain and lose virtuals.
