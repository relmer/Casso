# Contract: DxuiDropdown and the menu bar over it

**Feature**: 032-dxui-command-widgets | **Date**: 2026-09-10

`Dxui/Widgets/DxuiDropdown.h`, evolved from `DxuiPopupMenu`. Signatures are
intent; the header is authoritative once it exists.

## Items

```cpp
struct DxuiDropdownItem
{
    enum class Kind { Command, Separator, Submenu };
    Kind                            kind;
    const DxuiCommand *             command;    // Command and Submenu
    std::vector<DxuiDropdownItem>   children;   // Submenu only

    static DxuiDropdownItem  ForCommand   (const DxuiCommand * cmd);
    static DxuiDropdownItem  ForSeparator ();
    static DxuiDropdownItem  ForSubmenu   (const DxuiCommand * cmd, std::vector<DxuiDropdownItem> children);
};
```

## Widget

```cpp
class DxuiDropdown : public IDxuiControl
{
public:
    using IndexFn  = std::function<void (int index)>;
    using ClosedFn = std::function<void (bool committed)>;

    void  SetDpi               (UINT dpi);
    void  SetTheme             (const IDxuiTheme * theme);
    void  SetPopupHost         (DxuiHwndSource * host);
    void  SetOnHighlightChange (IndexFn fn);
    void  SetOnClosed          (ClosedFn fn);
    void  SetOnSelect          (IndexFn fn);

    void  ShowUnder   (const RECT & anchor, std::vector<DxuiDropdownItem> items, IDxuiTextRenderer &, const RECT & hostClient);
    void  ShowAt      (int x, int y,       std::vector<DxuiDropdownItem> items, IDxuiTextRenderer &, const RECT & hostClient);
    void  Hide        ();

    bool  IsVisible   () const;
    int   GetHighlight () const;
    bool  HitTest     (int x, int y) const;     // includes open children
    void  OnMouseMove (int x, int y);
    bool  OnLButtonDown (int x, int y);
    bool  OnLButtonUp   (int x, int y);
    bool  OnKey       (WPARAM vk);
    void  Paint       (IDxuiPainter &, IDxuiTextRenderer &) const;
};
```

Rules:

- **Layout**: rows are 26 dp, text starts 28 dp in after a 10 dp pad and
  an 18 dp check gutter, font 14 dp, separators 10 dp tall with a 10 dp
  inset, 1 dp border. Width always fits content: the widest label, plus an
  accelerator column only when some row carries accelerator text, plus
  padding, with a 140 dp minimum. There is no fixed width and no setter
  for one.
- **Painting** reads the command at paint time: checked draws the check
  glyph, disabled draws in the disabled color, an accelerator draws right
  aligned, a submenu draws the arrow.
- **Navigation**: Up and Down skip separators and disabled rows and wrap.
  Right on a submenu row opens the child with its first enabled row
  highlighted. Left or Escape with a child open closes only the child.
  Escape on the root hides uncommitted. Enter on an enabled command row
  commits. Enter on a submenu row opens it.
- **Callbacks**: highlight change fires on every highlight move, pointer or
  key. Closed fires before select with `committed` true only when a row was
  picked. Select fires with the picked row's index and then calls that
  command's `dispatch`, if the command is enabled.
- **Hosting**: with a popup host, each level acquires a pooled popup on
  show and releases it on hide; children link to their parent through the
  popup host chain so click-outside dismisses the whole chain. Without a
  host, the owner paints it and routes to it, as today.
- **Reopen guard**: a show requested within the close window of the last
  hide from the same anchor is ignored. This preserves the click-to-toggle
  behavior the toolbar pickers and the menu bar titles both have.

## Context menu call

```cpp
class DxuiContextMenu
{
public:
    static void  Show (DxuiHwndSource & host, int x, int y, std::vector<DxuiDropdownItem> items);
};
```

Rules: uses a dropdown owned by the host window, with the host's DPI,
theme, text renderer and client rect. The host routes input to it while it
is visible, which `DxuiHwndSource` already does for pooled popups.

## Menu bar

`DxuiMenuBar` keeps its public surface for titles, mnemonics, open and
close, focus and keyboard, and replaces `DxuiMenuBarItem::subitems` with
`std::vector<DxuiDropdownItem>`. `DxuiMenuBarSubitem` is deleted. The bar
owns one `DxuiDropdown`, shows it under the open title, swaps its items on
Left, Right and hover-swap, and forwards Up, Down, Enter and Escape to it.
`SetStripColors` stays; `SetDropdownColors` becomes a setter on the
dropdown that the bar forwards, so the emulator's chrome overrides land in
the one place.

## Consumers after the change

| Consumer | Before | After |
|---|---|---|
| `MainMenu` | `DxuiMenuBar` painting its own dropdown | titles over `DxuiDropdown` |
| Toolbar pickers | three `DxuiPopupMenu` by value | one `DxuiDropdown` inside `DxuiToolbar` |
| `Disk2DebugPanel`, `InputDebugPanel` | `DxuiPopupMenu` by value | `DxuiContextMenu::Show` |

`DxuiPopupMenu.h/.cpp` are removed once the three consumers have moved.
