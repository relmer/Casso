#pragma once

#include "Pch.h"
#include "Window/DxuiCaptionDragTracker.h"
#include "Window/DxuiWindow.h"
#include "Widgets/DxuiToolbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow
//
//  A small top-level window whose content is one toolbar: the window a
//  toolbar floats in once it is torn off the edge of its main window, as
//  Visual Studio's toolbars float. It has no title bar and no title; the
//  toolbar's grab handle moves it.
//
//  The application moves the toolbar in with AttachChild and out again with
//  DetachChild, so it passes between windows intact, then hands it to
//  SetToolbar. Mouse input the grab handle does not take goes to the
//  application, which routes it to the toolbar as it would in its main
//  window.
//
//  A DRAG BY THE GRAB HANDLE IS REPORTED as a floating pane's caption drag
//  is: each tick of the system's move loop reports the cursor, in screen
//  pixels, and the end of the loop reports where it ended, so the owner can
//  dock the toolbar there.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToolbarWindow : public DxuiWindow
{
public:
    using  MouseFn  = std::function<bool (const DxuiMouseEvent & ev)>;
    using  PointFn  = std::function<void (POINT screenPx)>;
    using  ClosedFn = std::function<void ()>;

    DxuiToolbarWindow  ();
    ~DxuiToolbarWindow () override = default;

    HRESULT  Create (const CreateParams & params);

    void            SetToolbar (DxuiToolbar * toolbar) { m_toolbar = toolbar; }
    DxuiToolbar  *  GetToolbar () const                { return m_toolbar; }

    void  SetOnContentMouse      (MouseFn fn)  { m_onMouse      = std::move (fn); }
    void  SetOnCaptionDrag       (PointFn fn)  { m_onDrag       = std::move (fn); }
    void  SetOnCaptionDragEnd    (PointFn fn)  { m_onDragEnd    = std::move (fn); }
    void  SetOnCaptionDragCancel (ClosedFn fn) { m_onDragCancel = std::move (fn); }

    //  Run on every tick of the system's move loop, after the drag is
    //  reported: the OS owns the thread while the window is dragged, so an
    //  owner that must keep drawing frames, and keep its machine running,
    //  pumps one from here.
    void  SetOnMoveLoopFrame     (ClosedFn fn) { m_onMoveFrame  = std::move (fn); }

    //  Starts the system's move loop as if the grab handle had been pressed,
    //  for an owner that tears the toolbar off while its button is down.
    void  BeginMove ();

    //  Once a frame: reports the end of a drag the end of the move loop did
    //  not.
    void  PollCaptionDrag ();

    //  Moves and sizes the window, in screen pixels.
    void  SetScreenRect (const RECT & rectPx);
    RECT  GetScreenRect () const;

protected:
    void     Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;
    void     OnWindowPlaced    () override;
    void     OnDpiChanged      (UINT newDpi) override;

private:
    void  OnMoveLoopTick ();
    void  Report         (DxuiCaptionDragTracker::Event ev);
    SIZE  GetScreenSize  () const;

    DxuiToolbar             * m_toolbar      = nullptr;
    MouseFn                   m_onMouse;
    PointFn                   m_onDrag;
    PointFn                   m_onDragEnd;
    ClosedFn                  m_onDragCancel;
    ClosedFn                  m_onMoveFrame;
    DxuiCaptionDragTracker    m_drag;
};
