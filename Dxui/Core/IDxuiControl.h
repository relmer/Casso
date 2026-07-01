#pragma once

#include "Pch.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiEvents.h"



////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiControl
//
//  Unified control interface. Every Dxui widget, panel, and chrome
//  primitive ultimately derives from this. The base supplies storage
//  and concrete accessors for the universal bits (bounds, visibility,
//  enabled, focusable, parent, tab index); subclasses override the
//  pure-virtual layout / paint / input hooks.
//
//  Visibility uses Collapsed-only semantics (FR-011): a hidden control
//  takes zero layout space and the parent relayouts.
//
//  All public sizes / positions are DIPs; identifiers use the `Dip`
//  suffix. All public string accessors return `std::wstring`. Every
//  public method is called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////



class IDxuiPainter;
class IDxuiTextRenderer;
class IDxuiTheme;
class IDxuiControl;
class IDxuiLayout;



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
    Generic,
    Button,
    Checkbox,
    Radio,
    Slider,
    Dropdown,
    TextInput,
    TabStrip,
    Label,
    ListView,
    TreeView,
    Panel,
    Dialog,
    CaptionBar,
    Viewport,
    Custom,
};



class IDxuiControl
{
public:
    virtual ~IDxuiControl() = default;

    static constexpr int  kTabIndexGeometry = -1;
    static constexpr int  kTabIndexExcluded = -2;

    virtual void  Layout          (const RECT          & boundsDip,
                                   const DxuiDpiScaler & scaler)                = 0;
    virtual void  Paint           (IDxuiPainter        & painter,
                                   IDxuiTextRenderer   & text,
                                   const IDxuiTheme    & theme)                 = 0;

    virtual bool  OnMouse         (const DxuiMouseEvent & ev)                   { (void) ev; return false; }
    virtual bool  OnKey           (const DxuiKeyEvent   & ev)                   { (void) ev; return false; }
    virtual bool  OnChar          (wchar_t ch)                                 { (void) ch; return false; }

    virtual void  OnFocusChanged  (bool focused)                                { (void) focused; }
    virtual void  OnThemeChanged  ()                                            {}
    virtual void  Tick            (int64_t nowMs)                               { (void) nowMs; }

    virtual DxuiHitTestKind  ClassifyHit  (POINT clientDip) const               { (void) clientDip; return DxuiHitTestKind::Client; }

    virtual std::wstring        AccessibleName  () const                        { return L""; }
    virtual DxuiAccessibleRole  AccessibleRole  () const                        { return DxuiAccessibleRole::Generic; }

    RECT  Bounds      () const                                                  { return m_boundsDip; }
    void  SetBounds   (RECT boundsDip);

    bool  Visible     () const                                                  { return m_visible; }
    void  SetVisible  (bool visible);

    bool  Enabled     () const                                                  { return m_enabled; }
    void  SetEnabled  (bool enabled);

    bool  Focusable   () const                                                  { return m_focusable; }
    void  SetFocusable(bool focusable);

    int   TabIndex    () const                                                  { return m_tabIndex; }
    void  SetTabIndex (int tabIndex);

    IDxuiControl *  Parent     () const                                         { return m_parent; }
    void            SetParent  (IDxuiControl * parent)                          { m_parent = parent; }

    virtual size_t          ChildCount  () const                                { return 0; }
    virtual IDxuiControl *  Child       (size_t index) const                    { (void) index; return nullptr; }

protected:
    virtual void  OnVisibilityChanged()                                        {}

    IDxuiControl *  m_parent     = nullptr;
    RECT            m_boundsDip  = {};
    bool            m_visible    = true;
    bool            m_enabled    = true;
    bool            m_focusable  = false;
    int             m_tabIndex   = kTabIndexGeometry;
};
