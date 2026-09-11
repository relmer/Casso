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

**Decision**: two new library types and one extended in place. `DxuiCommand` is
the declaration and `DxuiToolbar` is the strip, both new. `DxuiPopupMenu` grows
into the one menu every opener uses. `DxuiMenuBar` becomes titles over it.

## R2. What exists and where it diverges

**Finding**:

- `DxuiMenuBar` (`Dxui/Widgets/DxuiMenuBar.h`) paints its own dropdown. Its
  `DxuiMenuBarSubitem` carries label, dispatch, `isChecked`, `isEnabled`,
  `labelText` functors, `isSeparator`, an accelerator hint and `enabled`.
  Titles carry `&X` mnemonics. It does not use `DxuiPopupMenu`.
- `DxuiPopupMenu` (`Dxui/Widgets/DxuiPopupMenu.h`) is label plus checked
  per item, owner-routed, with `SetOnSelect`, `SetOnHighlightChange`,
  `SetOnClosed (committed)` fired closed-before-select, and opt-in popup
  hosting via `DxuiHwndSource`'s pool. Consumers: `CommandToolbar` (three
  pickers), `Disk2DebugPanel` and `InputDebugPanel` (right-click). Its header
  mentions cascading submenus chaining through
  `DxuiPopupHost::SetParentPopup`, but that describes a capability of the
  popup host: the widget holds no child, no chain and no Right-arrow arm, so
  submenus are new code rather than a move.
- `CommandToolbar` (`CassoEmuCore/Ui/Chrome/CommandToolbar.cpp`, 1,958
  lines, 44 functions) holds button chrome, collapse, tooltips, three
  pickers, a hover flyout with `DxuiSlider`, and the emulator's entries,
  printer LED, input cluster and tips. No unit test constructs it.
- `MainMenu` (`CassoEmuCore/Ui/Chrome/MainMenu.h`) derives from
  `DxuiMenuBar`; its static `s_kEntries` table is `{commandId, menu, label,
  accelerator, checkable}` and it maps `WORD` ids to dispatch, check and
  label functors.

**Decision**: `DxuiMenuBarSubitem` is already most of a command. It becomes
`DxuiCommand` plus an item wrapper. `DxuiPopupMenu` already has the hosting,
the callbacks and the content-fitted width, so it is extended in place rather
than copied: it takes the richer item model and the menu bar's painting and
gains submenus. The menu bar keeps only what is bar-specific.

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

**Decision**: `DxuiPopupMenuItem` is a variant of three kinds: a command
reference, a separator, and a command reference with a child item list. The
command is held by pointer to a `DxuiCommand` the application owns, so
placement never copies a declaration. The dropdown evaluates the functors
at paint and at key navigation.

**Alternatives considered**: items owning a copy of their command. Rejected;
copies are the defect being removed.

## R5. One set of dropdown metrics

**Finding**: the two differ in six places. Three entries here correct what an
earlier reading of this section claimed; each was checked against the code
rather than inferred.

| Metric | `DxuiPopupMenu` today | `DxuiMenuBar` dropdown |
|---|---|---|
| Row height | 26 dp | 26 dp |
| Font | 13 dp | 14 dp |
| Text start | 28 dp always | 10 dp, or 28 dp when SOME row of that menu is checkable |
| Width | fits content, 140 dp minimum | fits content, 300 dp minimum |
| Accelerator | none | left aligned at a fixed 190 dp offset |
| Separators | none | 10 dp tall, 10 dp inset |
| Colors | elevated, hover, foreground | plus disabled, muted, divider, border |

The width row is the correction that matters most. The 300 dp is a FLOOR, not a
fixed width: `GetDropdownWidthPx` already measures every row and returns the
larger of content and floor. So menus wider than 300 dp already fit their
content and do not move at all, and only narrow menus change.

The check gutter is conditional in the painter but unconditional in the width
calculation, so a menu with no checkable row today reserves 18 dp it never
draws into. That is a pre-existing disagreement between the two functions, not
a deliberate margin.

**Decision**: the widget keeps the shared row height, takes the menu bar's
font, separators, color set and conditional gutter, and keeps its own
content-fitted width with the 140 dp floor. The owner ruled the 300 dp floor
wrong in every case. Width is the widest label, plus an accelerator column only
when some row carries accelerator text, plus padding. Width and painter use the
SAME gutter rule, which is what "fits content" has to mean: space reserved for a
check that cannot appear is not content.

**Consequence**: three visible changes, all stated in quickstart §3 so none is
mistaken for a regression.

- Menu bar dropdowns narrower than 300 dp shrink to their content. Wider ones
  are unaffected.
- The toolbar pickers and the two debug panel menus read one point larger.
- Accelerator text moves. It cannot stay at a fixed 190 dp offset once the menu
  is no longer at least 300 dp wide, since it would overflow a narrow menu, so
  it is right aligned against the trailing padding instead. This one is forced
  by the width change rather than chosen.

Row heights, row order and the top edge do not move anywhere, and the
closed-state bands stay pixel identical.

**Alternatives considered**: a fixed-width setter the menu bar would use
to stay at 300. Rejected by the owner. Keeping the 13 dp font for pickers.
Rejected; one font, and the menu bar is the reference.

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

**Decision**: pixels for the menu bar band and the toolbar band in all
three themes against a master binary on the same machine; row content,
states and row positions for each open dropdown, whose width now fits its
content. Headless tests for command propagation, dropdown layout and
navigation from three anchors, toolbar collapse and dispatch. Existing
tests keep every assertion. Only two are edited, at their construction sites
only: `DxuiMenuBarTests.cpp` (subitem aggregates become commands and items)
and `ChromeCommandRoutingTests.cpp` (walks the menu entry table; re-pointed at
the new command table so its every-id-once assertion survives).
`DxuiWidgetIDxuiControlTests.cpp` keeps its three existing conformance rows as
they stand, since the widget they name is extended rather than replaced, and
only gains rows for `DxuiToolbar`. `MainMenuDropdownTests.cpp` is untouched,
because `MainMenu`'s public surface is preserved. The owner ruled out adapters
that would keep a removed type alive for a test's sake.

## R10. Style constraints that bite this work

- Function names are VerbNoun; `OnXxx` handlers exempt; `IDxuiTheme` color
  accessors keep bare nouns (`docs/coding-standards-backlog.md` items 6, 7).
- Helpers are class statics; `PaintStatusLed` moves from a free static into
  the LED class.
- Splice ahead of `////` banners; `scripts/CheckStyle.ps1 -Mode Tree` after
  `git add -A` before the merge.
- A vtable change needs `-Target Rebuild`; the dropdown and menu bar both
  gain and lose virtuals.

## R11. What the three types are called

**Finding**: the tree already had a `DxuiDropdown`, and it is a combo box: a
box that sits in the panel layout and displays its selected value, remembers a
selection across opens, takes part in the tab order, and holds plain strings
with no behavior attached. Eleven files consume it, across the settings pages,
the create-disk dialog and the input debug panel. The widget this feature adds
shares none of that. It has no resting presence, holds no selection, and its
rows are commands, separators and submenus. The two collided on one identifier
and the plan did not catch it.

**Decision**: rename only the widget that holds the wrong name, and leave the
menu where it is. Three roles, kept lexically distinct.

| This tree | Win32 | WinUI | What it is |
|---|---|---|---|
| `DxuiComboBox` | combo box | `ComboBox` | the form field that displays its value |
| A `DxuiToolbar` entry of kind `DropDown` | `BTNS_DROPDOWN` | `DropDownButton` | a button that opens a menu and keeps its own label |
| `DxuiPopupMenu` | popup menu | `MenuFlyout` | the menu itself, whatever opened it |

The existing widget becomes `DxuiComboBox` in one mechanical pass. The
identifier is the same length, so no column alignment moves and no include
ordering changes. `DxuiDropdown` then exists nowhere, which is the point: it
was the one word that could mean either thing.

The relationship between the second and third rows is a property rather than
inheritance, as it is upstream. A drop-down button holds a menu; it does not
implement one. That is what lets the same menu type serve the menu bar, the
toolbar's pickers and a right-click, which is this feature's third user story.

**Why the toolbar pickers are drop-down buttons and not combo boxes**: the
comment on the old toolbar's button struct already states the rule. A picker
carries its purpose as its label, not the value it holds, because a label that
changes with the value moves every button to its right. A combo box does the
opposite. The current choice appears as a check on a row inside the menu, which
upstream is a radio menu item and here is a picker row with an `isChecked`
functor.

**Alternatives considered**, both rejected by the owner:

- Leaving the combo box where it was and calling the menu `DxuiMenuPopup`. It
  reads as a near-duplicate of `DxuiPopupMenu` and leaves the ambiguous word in
  the tree beside the type it is ambiguous with.
- Renaming the menu to `DxuiMenuFlyout` after WinUI. Carried for two commits and
  reversed. Once the combo box moved, nothing forced the menu to move at all;
  this tree is Win32 and Direct2D, where the term for the object is a popup
  menu; and `Flyout` is already taken in this feature by the toolbar entry kind
  that hosts the volume slider, which is not a menu. Upstream makes those two
  siblings on purpose, here they are unrelated, so the words stay apart.

Extending in place rather than copying and deleting also keeps the three
consumers compiling throughout and leaves
`UnitTest/Dxui/DxuiWidgetIDxuiControlTests.cpp` untouched, where a rename would
have retyped its three conformance rows for no behavioral reason.
