# Data Model: Dxui Command Widgets

**Feature**: 032-dxui-command-widgets | **Date**: 2026-09-10

All state is in-memory. Nothing is persisted by this feature.

## DxuiCommand

One action's declaration. Owned by the application, referenced by pointer
from every surface.

| Field | Type | Meaning |
|---|---|---|
| id | `int` | Application id; the emulator uses its `IDM_*` values |
| label | `std::wstring` | Full label, as a menu shows it |
| shortLabel | `std::wstring` | Optional; a toolbar prefers it when present |
| glyph | `const wchar_t *` | Icon codepoint in the icon face; may be empty |
| tip | `std::wstring` | Tooltip |
| accelerator | `std::wstring` | Hint text, such as `Ctrl+R` |
| dispatch | `std::function<void ()>` | The action |
| isChecked | `std::function<bool ()>` | Optional; absent means never checked |
| isEnabled | `std::function<bool ()>` | Optional; absent means enabled |
| labelText | `std::function<std::wstring ()>` | Optional; overrides `label` when present |

Validation: ids unique within an application; `dispatch` required.

## DxuiMenuFlyoutItem

| Kind | Fields |
|---|---|
| Command | `const DxuiCommand *` |
| Separator | none |
| Submenu | `const DxuiCommand *` for the row, `std::vector<DxuiMenuFlyoutItem>` children |

## DxuiMenuFlyout

| Field | Meaning |
|---|---|
| items | The list shown |
| anchor | A rect to hang under, or a point |
| hostClient | Rect the dropdown is kept inside |
| highlight | Index of the highlighted row, or none |
| child | The open submenu, if any |
| openedOn | Row the dropdown opened on, replayed on dismissal |
| onHighlight, onClosed, onSelect | Callbacks; closed fires before select with a committed flag |

States: Closed, Open, Open with child. Navigation skips separators and
disabled rows. Right on a submenu row opens the child; Left or Escape
closes only the child. Escape on the root closes it uncommitted.

## Menu bar title

| Field | Meaning |
|---|---|
| label | With `&X` mnemonic |
| altLetter | Optional override of the mnemonic |
| items | The dropdown item list for that title |

The bar keeps: open index, opened-by-keyboard flag, focused title, hover
title. It owns one `DxuiMenuFlyout` and reuses it for whichever title is open.

## Toolbar entry

| Field | Type | Meaning |
|---|---|---|
| command | `const DxuiCommand *` | Label, glyph, tip, enabled and checked come from here |
| kind | `enum { Command, Toggle, DropDown, Flyout }` | What a click does |
| group | `int` | Equal groups sit together with a narrower gap |
| decoration | `DecorationFn` (optional) | Painted over the icon |
| custom | `IDxuiToolbarCustomEntry *` (optional) | Owns width, layout, paint, tooltip, click |

Runtime per entry: `rc`, `hovered`, `pressed`, `labeled`.

Validation: `custom` and `decoration` are mutually exclusive; a `DropDown`
must have items set before it opens; a `Flyout` must have a control set.
A custom entry may carry any kind; when its `OnClick` returns false the
widget acts on the kind, which is how a collapsed cluster opens a picker.

## Toolbar

| Field | Meaning |
|---|---|
| entries | Ordered list |
| labeledCount | Result of planning: entries from the left that keep labels |
| bandDp | Band height for the plan |
| pickers | One item list, `openedOn`, preview and commit sinks per picker entry, keyed by command id |
| dropdown | One `DxuiMenuFlyout` reused for whichever picker is open |
| flyout | Hosted control, panel rect, keep-alive rect, open flag |

Plan rule: from all labeled, drop the rightmost label until the strip fits
or none is labeled.

## Context menu

Not a type: a static call that takes a host, a point and an item list, and
drives one `DxuiMenuFlyout` owned by the host window. A picked row runs its
command; there is no completion callback. `Disk2DebugPanel` and
`InputDebugPanel` use it.

## Emulator command table

One static table in `Ui/Chrome` declaring every emulator action as a
`DxuiCommand`, replacing `MainMenu`'s `s_kEntries` and `CommandToolbar`'s
entry table. Two placement tables beside it: menu titles to item lists, and
toolbar entries to kinds and groups. The machine-name tips are label
functors reading shell state through a sink set at startup.

## Printer LED and input cluster

- `PrinterStatusLed`: status-to-color rule plus a static painter usable as
  a decoration.
- `InputClusterEntry`: implements `IDxuiToolbarCustomEntry`; holds the
  joystick, pointer-mode and mouse-available state, the three segment
  rects, the skeuo and monoline flags, and the glyph painters.
