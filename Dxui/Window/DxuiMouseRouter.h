#pragma once

#include "Pch.h"
#include "Core/DxuiEvents.h"
#include "Core/DxuiPointerPath.h"
#include "Window/IDxuiHostClient.h"


class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiMouseRouterHost
//
//  What DxuiMouseRouter calls in its window: the modal overlay, the dialog's
//  press focus and context menu, its control tree, and the paint batch
//  around each event.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiMouseRouterHost
{
public:
    virtual ~IDxuiMouseRouterHost() = default;

    virtual IDxuiControl &  GetMouseRoot          ()                              = 0;
    virtual HWND            GetMouseHwnd          () const                        = 0;
    virtual bool            RouteOverlayMouse     (const DxuiMouseEvent & ev)     = 0;   // false when no overlay is up
    virtual void            FocusPressedControl   (POINT clientPx)                = 0;
    virtual void            TickDialogMenu        ()                              = 0;
    virtual void            BeginMouseBatch       ()                              = 0;
    virtual void            EndMouseBatch         ()                              = 0;
    virtual void            InvalidateForMouse    ()                              = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMouseRouter
//
//  Turns the window's mouse messages into DxuiMouseEvents for its control
//  tree. The controls under the pointer are tracked, so each one the
//  pointer leaves, and each on the path when the pointer leaves the window,
//  gets a Leave event.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiMouseRouter
{
public:
    explicit DxuiMouseRouter (IDxuiMouseRouterHost & host) : m_host (host) {}

    DxuiMessageResult  OnButton (DxuiMouseEventKind kind, DxuiMouseButton button, LPARAM lParam);
    DxuiMessageResult  OnMove   (WPARAM wParam, LPARAM lParam);
    DxuiMessageResult  OnWheel  (WPARAM wParam, LPARAM lParam, bool horizontal);

    //  The pointer left the client area, off the window or onto its frame:
    //  the controls get a Leave event, so hover states and tips end. Not
    //  while the window has the capture, so a drag continues until its
    //  release.
    void  OnLeave();

private:
    DxuiMessageResult  Dispatch (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, float wheelDelta, bool wheelHorizontal = false);

    static bool  ShowScrollbarMenuIn (IDxuiControl & control, HWND hwnd, POINT clientPx);

    IDxuiMouseRouterHost  & m_host;
    DxuiPointerPath         m_pointerPath;          // the controls under the pointer, outermost first
    bool                    m_wheelCtrl = false;    // Ctrl, by the flags of the wheel message being dispatched
};
