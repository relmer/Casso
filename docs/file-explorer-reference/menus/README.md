# File Explorer menu captures

Reference captures of Windows File Explorer's menus, for checking Casso
Explorer's menus against them: items, order, separators, submenus, check and
radio marks, disabled rows and icons.

## Naming

`<surface>-<target-or-place>[-<submenu>-submenu].png`, in lower case with
hyphens.

| Surface      | Opened from                                    |
|--------------|------------------------------------------------|
| `cmdbar`     | A command bar button: New, Sort, View, Filter, Search options, See more (...) |
| `context`    | A right-click in the file list, on an item or on the background |
| `navpane`    | A right-click in the navigation pane           |
| `tab`        | A right-click on a tab                         |
| `addressbar` | A right-click in the address bar               |
| `header`     | A right-click on a Details column header       |

The place follows `-in-`: `cmdbar-sort-in-search` is the Sort button in
search results, and `context-file-in-folder` is a right-click on a file in an
ordinary folder. `-with-selection` marks a capture taken with an item
selected. A submenu capture includes its parent menu, for example
`context-background-folder-group-by-submenu`.

## Themes

- `dark/`: Windows in dark mode.
- `light/`: empty for now. Switch Windows to light mode and retake each
  capture in `dark/` under the same file name.

## Conditions

Taken on 2026-11-03 on Windows 11 build 26300.9711 (26H2), on a 3840 x 2160
display at 125 percent scale (120 DPI), in a File Explorer window on that
display. Third-party shell extensions installed on that PC (WinRAR, PowerToys,
File Locksmith, Beyond Compare's Select Left, OneDrive) appear in the context
menus; they are not part of File Explorer.

## Retaking them

1. Open a File Explorer window large enough that every menu and submenu falls
   inside it, so nothing from another window appears in the margin.
2. Open the menu with the mouse. Submenus open by clicking their row.
3. Capture the screen rectangle of the menu's popup windows (class
   `Microsoft.UI.Content.PopupWindowSiteBridge`, or `#32768` for the column
   header menu) with a 12-pixel margin on every side for the border and
   shadow.
4. Save the image under the name above, replacing the old one.
