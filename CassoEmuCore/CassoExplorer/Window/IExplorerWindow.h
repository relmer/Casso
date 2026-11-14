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

    //  While the condition is true, ExplorerInput offers the control every
    //  event first, wherever the pointer is, as Windows' mouse capture does:
    //  the press outside an open menu or a rename that closes it is offered
    //  to it. An event the control doesn't take goes on to the control under
    //  the pointer. The conditions are true for one control at a time.
    using CaptureCondition = std::function<bool()>;

    void  SetCaptureCondition (IDxuiControl * control, CaptureCondition wants) { m_captureConditions.emplace_back (control, std::move (wants)); }

    IDxuiControl * GetCaptureHolder () const
    {
        for (const auto & [control, wants] : m_captureConditions)
        {
            if (control->IsVisible() && wants())
            {
                return control;
            }
        }

        return nullptr;
    }

private:
    std::unordered_map<const IDxuiControl *, MouseHandler>    m_mouseHandlers;
    std::vector<std::pair<IDxuiControl *, CaptureCondition>>  m_captureConditions;
};
