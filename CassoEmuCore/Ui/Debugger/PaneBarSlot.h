#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"

class DebuggerPaneFrame;





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot
//
//  A pane's toolbar and the place at the top of the pane it is laid out in.
//  The slot is a part of the pane's frame; the bar is a control of its own,
//  laid out over the slot in the window the pane is in, so it goes with the
//  pane into a floating window. The bar's strip and whatever menu it has
//  open take the left button, and its entries have tips.
//
////////////////////////////////////////////////////////////////////////////////

class PaneBarSlot
{
public:
    using PressFn = std::function<void()>;

    PaneBarSlot (IDebuggerPaneHost & host, std::wstring pane, std::wstring accessibleName);
    ~PaneBarSlot();

    PaneBarSlot (const PaneBarSlot &)             = delete;
    PaneBarSlot & operator= (const PaneBarSlot &) = delete;

    //  Creates the bar, after the controls it paints over, and sets it up as
    //  a compact strip of glyphs with a See more menu, hidden until placed.
    void                 Create      ();
    void                 Configure   ();

    //  Called before a press on a closed bar, to build its drop-downs' rows
    //  again with the checks they have now.
    void                 SetOnPress  (PressFn press) { m_press = std::move (press); }

    DxuiToolbar        * GetBar      () const { return m_bar; }
    DebuggerPaneFrame  * GetSlot     () const { return m_slot.get(); }

    //  The slot's height, a compact toolbar's band.
    static int           GetHeightPx (int widthDip, const DxuiDpiScaler & scaler);

    void                 Place       ();
    bool                 RouteMouse  (const DxuiMouseEvent & ev) { return RouteToolbarMouse (m_host, m_pane, m_bar, ev, m_press); }
    const wchar_t      * GetTip      (POINT clientPx, RECT & anchor) const { return GetToolbarTip (m_host, m_pane, m_bar, clientPx, anchor); }

    //  The same for any toolbar of a pane, such as one laid out outside the
    //  slot.
    static bool            RouteToolbarMouse (IDebuggerPaneHost & host, const std::wstring & pane, DxuiToolbar * bar, const DxuiMouseEvent & ev, const PressFn & press);
    static const wchar_t * GetToolbarTip     (IDebuggerPaneHost & host, const std::wstring & pane, const DxuiToolbar * bar, POINT clientPx, RECT & anchor);

private:
    IDebuggerPaneHost                   & m_host;
    std::wstring                          m_pane;
    std::unique_ptr<DebuggerPaneFrame>    m_slot;
    DxuiToolbar                         * m_bar = nullptr;
    PressFn                               m_press;
};
