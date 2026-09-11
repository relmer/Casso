# Contract: DxuiToolbar

**Feature**: 032-dxui-command-widgets | **Date**: 2026-09-10

`Dxui/Widgets/DxuiToolbar.h`. Signatures are intent; the header is
authoritative once it exists. Commands: [dxui-command.md](dxui-command.md).
Pickers open the dropdown in [dxui-dropdown.md](dxui-dropdown.md).

## Entries

```cpp
struct IDxuiToolbarCustomEntry
{
    virtual int              GetWidthPx   (bool labeled, const DxuiDpiScaler &, IDxuiTextRenderer *) const = 0;
    virtual void             Layout       (const RECT & rc, bool labeled, const DxuiDpiScaler &) = 0;
    virtual void             Paint        (IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &, bool hovered, bool pressed, bool labeled) = 0;
    virtual const wchar_t *  GetTooltipAt (int x, int y, RECT & anchor) const = 0;
    virtual bool             OnClick      (int x, int y) = 0;
};

class DxuiToolbar : public IDxuiControl
{
public:
    enum class Kind { Command, Toggle, Picker, Flyout };
    using ChoiceFn     = std::function<void (int index)>;
    using DecorationFn = std::function<void (IDxuiPainter &, const IDxuiTheme &, const RECT & iconRc, bool collapsed)>;

    struct Entry
    {
        const DxuiCommand *        command;
        Kind                       kind;
        int                        group;
        DecorationFn               decoration;
        IDxuiToolbarCustomEntry *  custom;
    };

    void  SetEntries   (std::vector<Entry> entries);
    void  SetIconFace  (const wchar_t * face);      // default: Segoe MDL2 Assets
```

Rules: `SetEntries` replaces the table and resets runtime state. Label,
glyph, tip, enabled and checked come from the command at paint and click
time. A `Toggle` draws pressed while `IsChecked` is true. The toolbar
prefers `GetShortText` for its label.

## Planning, layout, paint

```cpp
    int   PlanForWidth    (int clientWidthPx, const DxuiDpiScaler & scaler);
    int   GetBandDp       () const;
    void  Layout          (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint           (IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &) override;
    void  SetTextRenderer (IDxuiTextRenderer * text);
```

Rules: plan before docking, then lay out the docked band. `Paint` reads
`DxuiTheme` fields only, meaning `navStrip`, `navItemText`, `buttonIdle`,
`buttonHover`, `buttonPressed`, `buttonBorder`, and never downcasts. Label
measurement uses the text renderer with a character-width fallback when
measurement returns zero.

## Pointer and keyboard

```cpp
    void             OnToolbarMouseMove   (int x, int y);
    void             OnToolbarMouseLeave  ();
    void             OnToolbarLButtonDown (int x, int y);
    void             OnToolbarLButtonUp   (int x, int y);
    const wchar_t *  GetTooltipAt         (int x, int y, RECT & anchor) const;
    bool             IsMenuOpen           () const;
    bool             HandleKey            (WPARAM vk);
    bool             HitTest              (int x, int y) const;
```

Rules: coordinates are client space. A `Command` or `Toggle` entry
dispatches its command on button up only when the button went down on the
same entry and the command is enabled. `HandleKey` returns true only while
a picker is open and consumed the key.

## Pickers and flyout

```cpp
    void  SetPickerItems   (int commandId, std::vector<DxuiDropdownItem> items);
    void  SetPickerSinks   (int commandId, ChoiceFn preview, ChoiceFn commit);
    void  SetFlyoutControl (int commandId, IDxuiControl * control, SIZE panelDp);
    bool  IsFlyoutOpen     (int commandId) const;
```

Rules: a picker's rows are commands too, typically with `isChecked`
functors reading the application's current choice. Preview fires as the
highlight moves and persists nothing; commit fires once on selection;
dismissal replays preview with the row the dropdown opened on. A flyout
opens on dwell over its entry and closes when the pointer leaves the union
of entry and panel, unless a drag on the hosted control is in progress.

## Hosting

```cpp
    void  SetPopupHost      (DxuiHwndSource * host);
    void  SetHostClientRect (const RECT & clientRect);
```

Rules: the picker dropdown is hosted through the popup pool and kept inside
the host client rect. A live popup is released before the host is
repointed, as the menu bar does.

## Decoration and custom entry

A decoration is painted after the widget paints the entry's icon and knows
nothing of what it draws. A custom entry owns everything inside its rect
while the widget owns its place, collapse schedule, hover and press state.
The emulator's printer LED is a decoration; its input cluster is a custom
entry.

A custom entry's `OnClick` returns true when it consumed the click. When it
returns false the widget treats the click as a click on the entry itself
and acts on the entry's `kind`: a `Picker` opens its item list, a
`Command` dispatches its command. This is how the collapsed input cluster
offers its three choices as a dropdown: the entry is a `Picker` with a
custom entry, `OnClick` consumes segment clicks while expanded and returns
false while collapsed.

## Dependency rule

`DxuiToolbar.h/.cpp` include only `Pch.h` and Dxui headers. A grep of
`Dxui/` for `CassoEmuCore`, `CassoTheme`, `Resource.h` or `IDM_` returns
nothing new.
