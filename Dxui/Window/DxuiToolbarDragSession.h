#pragma once

#include "Pch.h"

class IDxuiToolbarDragSite;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession
//
//  One drag of a toolbar by its grab handle, from the press to the release,
//  as Visual Studio's toolbars are dragged: docked, it slides along its
//  bands until pulled away, then tears off to float under the pointer; the
//  floating window follows the pointer, held where it was grabbed, onto any
//  monitor at any scale; over a dock band the place it would take is shown,
//  and the release docks it there. Nothing docks while the button is down.
//  Escape, or losing the mouse to another window while the button is down,
//  puts everything back where it was when the drag began.
//
//  The session is fed the messages the drag sees, with the pointer in
//  screen pixels and the left button's state as of each message, and makes
//  its decisions through an IDxuiToolbarDragSite. It holds no window, so a
//  test feeds it a recorded sequence of messages.
//
//  THE GRAB IS KEPT IN DIPS, along the toolbar and across it, from the
//  toolbar's own top left. The floating window's rectangle is worked out
//  again from the pointer on every move and on every DPI change, at the
//  window's DPI, so the toolbar stays under the pointer at the same place on
//  a monitor at any scale, standing up or lying down.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToolbarDragSession
{
public:
    enum class Phase
    {
        Idle,
        Docked,         // carrying the docked toolbar along its bands
        Floating,       // moving the floating window
        Canceled,       // put back; the rest of the press is swallowed
    };

    //  A message as the drag sees it. `id` is WM_MOUSEMOVE, WM_LBUTTONUP,
    //  WM_CAPTURECHANGED, WM_CANCELMODE, WM_KEYDOWN, WM_ENTERSIZEMOVE, or
    //  WM_TIMER for the poll on each frame.
    struct Message
    {
        UINT    id         = 0;
        POINT   screenPx   = {};
        bool    buttonDown = false;
        WPARAM  key        = 0;
    };

    explicit DxuiToolbarDragSession (IDxuiToolbarDragSite & site) : m_site (site) {}

    DxuiToolbarDragSession             (const DxuiToolbarDragSession &) = delete;
    DxuiToolbarDragSession & operator= (const DxuiToolbarDragSession &) = delete;

    //  A press on the grab handle: of the docked toolbar, whose rectangle
    //  is `toolbarPx`, in a window at `dpi`; or of the floating one, whose
    //  window is `windowPx`, at `dpi`.
    void  BeginDocked   (POINT screenPx, const RECT & toolbarPx, bool vertical, UINT dpi);
    void  BeginFloating (POINT screenPx, const RECT & windowPx,  bool vertical, UINT dpi);

    //  True when the session keeps the message from the window's own
    //  handling.
    bool  OnMessage (const Message & msg);

    //  The floating window's DPI changed under the drag: the rectangle it
    //  takes at `newDpi`, with the toolbar held under the pointer where it
    //  was grabbed. False when no floating drag is under way.
    bool  TryGetDpiRect (UINT newDpi, RECT & outRectPx) const;

    Phase  GetPhase    () const { return m_phase; }
    bool   IsActive    () const { return m_phase == Phase::Docked || m_phase == Phase::Floating; }
    bool   IsFloating  () const { return m_phase == Phase::Floating; }
    bool   IsVertical  () const { return m_vertical; }
    POINT  GetGrabDip  () const { return m_grabDip; }

    //  The floating window for a pointer at `pointerPx`, holding the toolbar
    //  `grabDip` (along, across) from its top left, `sizePx` in all with the
    //  toolbar `insetPx` in from each end of it, at `dpi`.
    static RECT  GetWindowRect (POINT pointerPx, POINT grabDip, SIZE sizePx, int insetPx, bool vertical, UINT dpi);

private:
    void  Move        (POINT screenPx);
    void  TearOff     (POINT screenPx);
    void  PlaceAt     (POINT screenPx);
    void  ShowPreview (POINT screenPx);
    void  Release     (POINT screenPx);
    void  Cancel      (bool buttonDown);
    RECT  GetRectAt   (POINT screenPx, UINT dpi) const;
    bool  OnSwallowed (const Message & msg);

    IDxuiToolbarDragSite  & m_site;
    Phase                   m_phase    = Phase::Idle;
    bool                    m_vertical = false;
    POINT                   m_grabDip  = {};
    POINT                   m_lastPx   = {};
    POINT                   m_pressPx  = {};
    bool                    m_hasMoved = false;
    UINT                    m_dpi      = USER_DEFAULT_SCREEN_DPI;
};
