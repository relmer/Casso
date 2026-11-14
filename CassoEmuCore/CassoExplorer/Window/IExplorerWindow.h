#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IExplorerWindow
//
//  The Casso Explorer window's operations its components call.
//
////////////////////////////////////////////////////////////////////////////////

class IExplorerWindow
{
public:
    virtual ~IExplorerWindow() = default;

    virtual HWND                           GetHwnd         () const                            = 0;
    virtual void                           Invalidate      ()                                  = 0;
    virtual DxuiHwndSource               * GetPopupHost    () const                            = 0;
    virtual IDxuiTextRenderer            * GetTextRenderer () const                            = 0;
    virtual void                           SetTheme        (const IDxuiTheme * theme)          = 0;
    virtual DxuiPanel                    & GetChildPanel   ()                                  = 0;
    virtual CassoExplorerPrefs::Placement  GetPlacement    () const                            = 0;
    virtual void                           ShowMessage     (const std::wstring & text, UINT icon) = 0;
    virtual void                           ShowAbout       ()                                  = 0;
    virtual void                           SaveSession     ()                                  = 0;
    virtual void                           ArmTick         ()                                  = 0;
    virtual void                           CloseWindow     ()                                  = 0;
    //  A control of the window. ExplorerInput offers it the mouse where it
    //  is, as a panel offers its children theirs, so a control made here
    //  takes clicks with nothing more written.
    template <typename T, typename... Args>
    T *  CreateChild (Args &&... args) { return GetChildPanel().template CreateChild<T> (std::forward<Args> (args)...); }

    //  A pane's handling of the mouse for a control made by CreateChild, which
    //  ExplorerInput calls in place of the control's OnMouse: the pane's
    //  focus, menus and drags, and a call to the control's OnMouse for the
    //  rest. It is offered the same events the control would be.
    using MouseHandler = std::function<bool (const DxuiMouseEvent & ev)>;

    void  SetMouseHandler (const IDxuiControl * control, MouseHandler handler) { m_mouseHandlers[control] = std::move (handler); }

    bool  OfferMouse (IDxuiControl & control, const DxuiMouseEvent & ev) const
    {
        auto  handler = m_mouseHandlers.find (&control);

        return handler != m_mouseHandlers.end() ? handler->second (ev) : control.OnMouse (ev);
    }

    //  A control whose pane routes the mouse to it, in a call from
    //  ExplorerInput::HandleMouse, for what a press there does besides: the
    //  pane's focus, its menus, its drags. ExplorerInput doesn't offer it
    //  events a second time.
    template <typename T, typename... Args>
    T *  CreatePaneRoutedChild (Args &&... args)
    {
        T  * child = CreateChild<T> (std::forward<Args> (args)...);

        m_paneRouted.insert (child);
        return child;
    }

    bool  IsPaneRouted (const IDxuiControl * control) const { return m_paneRouted.contains (control); }

private:
    std::unordered_set<const IDxuiControl *>                m_paneRouted;
    std::unordered_map<const IDxuiControl *, MouseHandler>  m_mouseHandlers;
};
