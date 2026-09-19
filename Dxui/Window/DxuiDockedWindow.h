#pragma once

#include "Pch.h"
#include "Window/DxuiWindow.h"
#include "Widgets/DxuiDockSite.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow
//
//  A top-level window whose content is one DxuiDockSite: the window a pane
//  floats in. The application moves the pane's controls in with AttachChild
//  and out again with DetachChild, so a control passes between windows
//  intact, and adds the pane to the site as it would in its main window.
//
//  INPUT THE SITE DOES NOT TAKE GOES TO THE APPLICATION. Panes route their
//  own input in the application that owns them, so the window offers each
//  mouse and key event to the site and hands the rest on unchanged.
//
//  A CAPTION DRAG IS REPORTED. While the user moves the window by its title
//  bar the OS runs its move loop; each tick reports the cursor, in screen
//  pixels, so the owner can show its drop zones under it, and the first
//  PollCaptionDrag after the button is released reports where it ended, so
//  the owner can dock the pane there. A resize is not reported.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockedWindow : public DxuiWindow
{
public:
    using  MouseFn   = std::function<bool (const DxuiMouseEvent & ev)>;
    using  KeyFn     = std::function<bool (const DxuiKeyEvent & ev)>;
    using  PointFn   = std::function<void (POINT screenPx)>;
    using  ClosedFn  = std::function<void ()>;
    using  CommandFn = std::function<bool (int commandId)>;

    DxuiDockedWindow  () = default;
    ~DxuiDockedWindow () override = default;

    HRESULT  Create (const CreateParams & params);

    DxuiDockSite &  GetSite () { return *m_site; }

    void  SetOnContentMouse    (MouseFn fn)  { m_onMouse     = std::move (fn); }
    void  SetOnContentKey      (KeyFn fn)    { m_onKey       = std::move (fn); }
    void  SetOnCaptionDrag     (PointFn fn)  { m_onDrag      = std::move (fn); }
    void  SetOnCaptionDragEnd  (PointFn fn)  { m_onDragEnd   = std::move (fn); }
    void  SetOnClosed          (ClosedFn fn) { m_onClosed    = std::move (fn); }

    //  A key the window's key map translates, for an owner that keeps one
    //  set of commands across its windows.
    void  SetOnMappedCommand   (CommandFn fn) { m_onCommand  = std::move (fn); }

    //  Once a frame: reports the end of a caption drag.
    void  PollCaptionDrag ();

    //  Moves and sizes the window, in screen pixels.
    void  SetScreenRect (const RECT & rectPx);
    RECT  GetScreenRect () const;

protected:
    void  OnCreate      () override;
    void  OnWindowClose () override;
    void  Layout        (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse       (const DxuiMouseEvent & ev) override;
    bool  OnKey         (const DxuiKeyEvent   & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;
    bool  OnMappedCommand (int commandId) override;

private:
    void  OnMoveLoopTick ();

    DxuiDockSite  * m_site          = nullptr;
    MouseFn         m_onMouse;
    KeyFn           m_onKey;
    PointFn         m_onDrag;
    PointFn         m_onDragEnd;
    ClosedFn        m_onClosed;
    CommandFn       m_onCommand;
    bool            m_dragging      = false;
    SIZE            m_dragSize      = {};
};