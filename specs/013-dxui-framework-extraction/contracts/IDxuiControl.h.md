# Contract: `IDxuiControl`

Public-header sketch. Concrete header lands in `Dxui/Core/IDxuiControl.h` during Migration Phase 6.

```cpp
// Dxui/Core/IDxuiControl.h
#pragma once

#include "Pch.h"
#include "DxuiEvents.h"


class IDxuiPainter;
class IDxuiTextRenderer;
class IDxuiTheme;
class IDxuiControl;
class IDxuiLayout;
class DxuiDpiScaler;


enum class DxuiHitTestKind
{
    None,
    Client,
    Caption,
    MinButton,
    MaxButton,
    CloseButton,
    ResizeEdgeLeft,
    ResizeEdgeRight,
    ResizeEdgeTop,
    ResizeEdgeBottom,
    ResizeCornerTL,
    ResizeCornerTR,
    ResizeCornerBL,
    ResizeCornerBR,
};


enum class DxuiAccessibleRole
{
    Button, Checkbox, Radio, Slider, Dropdown, TextInput, TabStrip,
    Label, ListView, TreeView, Panel, Dialog, CaptionBar, Viewport,
    Custom,
};


class IDxuiControl
{
public:
    virtual ~IDxuiControl() = default;

    static constexpr int kTabIndexGeometry = -1;
    static constexpr int kTabIndexExcluded = -2;

    // Layout & paint
    virtual void Layout       (const RECT & bounds,
                               const DxuiDpiScaler & scaler)               = 0;
    virtual void Paint        (IDxuiPainter & painter,
                               IDxuiTextRenderer & text,
                               const IDxuiTheme & theme)                   = 0;

    // Input
    virtual bool OnMouse      (const DxuiMouseEvent & ev)                  { return false; }
    virtual bool OnKey        (const DxuiKeyEvent & ev)                    { return false; }

    // Lifecycle
    virtual void OnFocusChanged (bool focused)                             {}
    virtual void OnThemeChanged ()                                         {}
    virtual void Tick           (int64_t nowMs)                            {}

    // Hit-testing (for DxuiHostWindow NC walk)
    virtual DxuiHitTestKind ClassifyHit (POINT clientDip) const            { return DxuiHitTestKind::Client; }

    // Accessibility (UIA provider deferred per FR-081)
    virtual std::wstring        AccessibleName () const                    { return L""; }
    virtual DxuiAccessibleRole  AccessibleRole () const                    { return DxuiAccessibleRole::Custom; }

    // Concrete-on-base accessors (containers override ChildCount/Child)
    RECT            Bounds      () const                                    { return m_boundsDip; }
    void            SetBounds   (RECT boundsDip);                              // triggers OnBoundsChanged for subscribers

    bool            Visible     () const                                    { return m_visible; }
    void            SetVisible  (bool visible);                                // Collapsed mode (FR-011); parent recalcs layout

    bool            Enabled     () const                                    { return m_enabled; }
    void            SetEnabled  (bool enabled);

    bool            Focusable   () const                                    { return m_focusable; }
    void            SetFocusable(bool focusable);

    int             TabIndex    () const                                    { return m_tabIndex; }
    void            SetTabIndex (int tabIndex);                                // kTabIndexGeometry or explicit non-negative order; kTabIndexExcluded skips Tab

    IDxuiControl *  Parent      () const                                    { return m_parent; }

    virtual size_t          ChildCount () const                             { return 0; }
    virtual IDxuiControl *  Child      (size_t i) const                     { return nullptr; }

protected:
    IDxuiControl *  m_parent     = nullptr;
    RECT            m_boundsDip  = {};
    bool            m_visible    = true;
    bool            m_enabled    = true;
    bool            m_focusable  = false;
    int             m_tabIndex   = kTabIndexGeometry;
};
```

## Contract notes

- `Paint` takes `IDxuiPainter` / `IDxuiTextRenderer` by reference and `IDxuiTheme` by const reference (FR-032) — never the concrete types.
- `OnMouse` / `OnKey` return `true` to consume the event; `DxuiPanel` fan-out stops at the first consumer (front-to-back z-order, i.e., last-added child first).
- `ClassifyHit` defaults to `Client`; chrome controls (`DxuiCaptionBar`, `DxuiSystemButton`, `DxuiDragRegion`) override it. Parent/tree walking handles pass-through behaviour without a separate hit-test enum member.
- `SetVisible(false)` is **Collapsed** semantics only in v1 (FR-011 clarification Q3): hidden control takes 0 layout space; siblings fill in. A future `Hidden` mode (retain space) is deferred.
- All public string accessors return `std::wstring` and all string parameters use `std::wstring` (FR-080).
- All public sizes / positions are DIPs; identifiers use `Dip` suffix (FR-022, FR-082).
- Every public method is called on the UI thread (FR-083); debug builds assert.
