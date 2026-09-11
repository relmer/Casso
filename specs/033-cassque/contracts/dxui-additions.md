# Contract: Dxui additions for Cassque

**Feature**: 033-cassque | **Date**: 2026-09-10

Each is a general widget or extension in `Dxui/`, tested headlessly on
the existing mocks, with no knowledge of disks or Cassque.

## DxuiTreeView extensions

```cpp
struct DxuiTreeNode
{
    std::wstring                id;            // NEW: stable identity; callbacks report it, not the label
    std::wstring                label;
    bool                        expanded;
    bool                        childrenLoaded;   // NEW: false means ask the provider on first expand
    bool                        dimmed;           // NEW: drawn in the muted color
    std::vector<DxuiTreeNode>   children;
    // existing checkbox fields unchanged
};

void  SetShowCheckboxes  (bool show);                          // NEW, default true for the capability checklist
void  SetChildProvider   (std::function<std::vector<DxuiTreeNode> (const std::wstring & id)>);   // NEW
void  SetOnSelect        (std::function<void (const std::wstring & id)>);                         // NEW
void  SetOnExpand        (std::function<void (const std::wstring & id, bool expanded)>);          // NEW
```

Rules: the existing `SetOnToggle` keeps reporting by label so the
hardware checklist is untouched. Expanding a node with `childrenLoaded`
false calls the provider once, stores the result, sets the flag and
re-flattens. Right on a collapsed node expands, Left on an expanded node
collapses, Left on a leaf moves to its parent. Ids are unique per tree.

## DxuiListView extensions

```cpp
void  SetMultiSelect       (bool on);                                   // NEW
const std::vector<int> &  GetSelectedRows () const;                     // NEW
void  SetSelectedRows      (std::vector<int> rows, int anchor);         // NEW
void  SetOnSort            (std::function<void (int column, bool descending)>);   // NEW: header click
void  SetOnActivate        (std::function<void (int row)>);            // NEW: Enter or double-click
```

Rules: click selects one; Ctrl+click toggles; Shift+click and Shift+arrow
extend from the anchor; Ctrl+A selects all. The widget never sorts rows
itself; it reports the header click and the consumer resupplies rows and
sets the indicator.

## DxuiSplitter

```cpp
class DxuiSplitter : public IDxuiControl
{
public:
    enum class Orientation { Vertical, Horizontal };
    void  SetOrientation   (Orientation o);
    void  SetPositionDip   (int dip);
    int   GetPositionDip   () const;
    void  SetLimitsDip     (int minFirst, int minSecond);
    void  SetOnMoved       (std::function<void (int dip)>);
    // Layout, Paint, OnMouse, OnKey overrides
};
```

Rules: a 4 dp sash; drag moves it within limits; Left and Right (or Up and
Down) move it 8 dp with focus; the consumer re-arranges its dock layout in
`OnMoved`. Paints the divider color; hover paints the hover color.

## DxuiStatusBar

```cpp
class DxuiStatusBar : public IDxuiControl
{
public:
    struct Field { std::wstring text; int widthDip; bool stretch; };
    void  SetFields   (std::vector<Field> fields);
    void  SetText     (size_t index, std::wstring text);
    int   GetBandDp   () const;
};
```

Rules: one stretch field takes the remainder; text elides with
`DxuiTextElide`; band height matches the menu bar's strip metrics.

## DxuiDragDropSource

```cpp
class DxuiDragDropSource
{
public:
    struct Format { CLIPFORMAT format; std::function<HRESULT (std::vector<Byte> &)> render; };   // render on demand
    HRESULT  Begin (std::vector<Format> formats, DWORD allowedEffects, DWORD & resultEffect);
};
```

Rules: implements `IDropSource` and `IDataObject`; `FILECONTENTS` streams
are rendered when the target asks, per index; the call blocks in
`DoDragDrop` on the UI thread as OLE requires. Escape cancels.

## DxuiFramebufferView

```cpp
class DxuiFramebufferView : public IDxuiControl
{
public:
    void  SetFramebuffer (const uint32_t * bgra, int width, int height);   // copied
    void  SetScaling     (bool integer, bool keepAspect);
};
```

Paints via `DxuiTextRenderer::DrawFramebuffer`, centered, integer-scaled
when it fits.

## Light and Dark palettes

`DxuiLightTheme` and `DxuiDarkTheme`, two `IDxuiTheme` implementations
built from the Fluent tokens in `DxuiWindowsThemeColors`. Casso does not
use them; Cassque does.

## DialogDefinition image

```cpp
struct DialogImage { std::vector<Byte> rgba; int width; int height; float displayDp; };
std::optional<DialogImage>  image;   // NEW field on DialogDefinition, painted above the body
```
