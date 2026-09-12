#pragma once

#include "Pch.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiEvents.h"
#include "Core/DxuiStandardCommand.h"





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

    // Win32 system-cursor id (IDC_*, e.g. IDC_SIZEWE over a column-resize
    // divider) for a client-space point, or nullptr for "no preference"
    // (host uses the default arrow). Panels fan this to children like
    // OnMouse; the host queries the tree on WM_SETCURSOR.
    virtual LPCWSTR  GetCursorForPoint  (POINT clientPx) const                       { (void) clientPx; return nullptr; }

    virtual void  OnFocusChanged  (bool focused)                                { (void) focused; }

    // Focus arrived by a Tab walk, forward or backward. A control with stops
    // INSIDE it (a hex view's two columns, say) starts at the first when Tab
    // brought focus in and at the last when Shift+Tab did, so walking the
    // window in reverse runs its insides in reverse too. Controls with no
    // internal stops ignore it; OnFocusChanged still fires either way.
    virtual void  OnFocusEntered  (bool forward)                                { (void) forward; }

    // The standard commands -- Copy, Select all and the rest -- whose meaning
    // follows the focus rather than the window. A control that answers one
    // implements both: QueryCommand says whether the command is its to answer
    // and, if so, whether it can be run right now; InvokeCommand runs it and
    // reports that it did. Both default to "not mine", and DxuiCommandRouter
    // then carries the command out to the control containing this one. A menu
    // row and the accelerator for it both come through here, so what the row
    // says and what the keystroke does cannot disagree.
    virtual bool  QueryCommand  (DxuiStandardCommand command, bool & outEnabled) const
    {
        (void) command;
        (void) outEnabled;
        return false;
    }

    virtual bool  InvokeCommand (DxuiStandardCommand command)
    {
        (void) command;
        return false;
    }

    virtual void  OnThemeChanged  ()                                            {}
    virtual void  Tick            (int64_t nowMs)                               { (void) nowMs; }

    virtual DxuiHitTestKind  ClassifyHit  (POINT clientDip) const               { (void) clientDip; return DxuiHitTestKind::Client; }

    virtual std::wstring        GetAccessibleName () const                        { return L""; }
    virtual DxuiAccessibleRole  GetAccessibleRole () const                        { return DxuiAccessibleRole::Generic; }

    RECT  GetBounds    () const                                                  { return m_boundsDip; }
    void  SetBounds    (RECT boundsDip);

    bool  IsVisible    () const                                                  { return m_visible; }
    void  SetVisible   (bool visible);

    bool  IsEnabled    () const                                                  { return m_enabled; }
    void  SetEnabled   (bool enabled);

    bool  IsFocusable  () const                                                  { return m_focusable; }
    void  SetFocusable (bool focusable);

    int   GetTabIndex  () const                                                  { return m_tabIndex; }
    void  SetTabIndex  (int tabIndex);

    IDxuiControl *  GetParent () const                                         { return m_parent; }
    void            SetParent (IDxuiControl * parent)                          { m_parent = parent; }

    virtual size_t          GetChildCount () const                                { return 0; }
    virtual IDxuiControl *  GetChild      (size_t index) const                    { (void) index; return nullptr; }

protected:
    virtual void  OnVisibilityChanged()                                        {}

    IDxuiControl *  m_parent     = nullptr;
    RECT            m_boundsDip  = {};
    bool            m_visible    = true;
    bool            m_enabled    = true;
    bool            m_focusable  = false;
    int             m_tabIndex   = kTabIndexGeometry;
};
