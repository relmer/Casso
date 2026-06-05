#pragma once

#include "Pch.h"



////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiHostClient
//
//  Optional client interface installed on a full-ownership
//  DxuiHostWindow via DxuiHostWindow::SetClient. Lets a consumer
//  receive the Win32 messages that the host does not own end-to-end
//  (commands, keyboard input, painting, timers, ...) without
//  inheriting from the host or from the legacy ``Window`` base
//  class.
//
//  Return semantics: every bool-returning method returns ``true``
//  to indicate the client handled the message end-to-end (the host
//  short-circuits and returns 0 from its WndProc), or ``false`` to
//  let the host fall through to DefWindowProc. The defaults return
//  ``false`` so a partial implementation only needs to override the
//  methods it cares about.
//
//  Threading: invoked on the UI thread from within the host's
//  WndProc dispatch.
//
////////////////////////////////////////////////////////////////////////////////


class IDxuiHostClient
{
public:
    virtual ~IDxuiHostClient() = default;

    // WM_CHAR. wParam carries the translated character (typically
    // from TranslateMessage). Return true to consume.
    virtual bool  OnChar           (WPARAM ch, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (ch);
        UNREFERENCED_PARAMETER (lParam);
        return false;
    }

    // WM_COMMAND. commandId is the LOWORD(wParam); the full message
    // form (notification code + control HWND) is available via
    // OnCommandEx below.
    virtual bool  OnCommand        (WORD commandId)
    {
        UNREFERENCED_PARAMETER (commandId);
        return false;
    }

    // WM_COMMAND, full form. Default forwards to OnCommand so most
    // clients only have to override the simple form. Override
    // OnCommandEx when the notify code or control HWND matters
    // (e.g. button click vs. dropdown change).
    virtual bool  OnCommandEx      (WORD commandId, WORD notifyCode, HWND hCtl)
    {
        UNREFERENCED_PARAMETER (notifyCode);
        UNREFERENCED_PARAMETER (hCtl);
        return OnCommand (commandId);
    }

    // WM_KEYDOWN / WM_SYSKEYDOWN.
    virtual bool  OnKeyDown        (WPARAM vk, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (vk);
        UNREFERENCED_PARAMETER (lParam);
        return false;
    }

    // WM_KEYUP / WM_SYSKEYUP.
    virtual bool  OnKeyUp          (WPARAM vk, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (vk);
        UNREFERENCED_PARAMETER (lParam);
        return false;
    }

    // WM_MOUSEMOVE.
    virtual bool  OnMouseMove      (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return false;
    }

    // WM_MOUSELEAVE.
    virtual bool  OnMouseLeave     ()
    {
        return false;
    }

    // WM_LBUTTONDOWN / WM_LBUTTONUP.
    virtual bool  OnLButtonDown    (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return false;
    }
    virtual bool  OnLButtonUp      (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return false;
    }

    // WM_MOVE. (x, y) are the new client-area top-left in screen
    // coordinates per the WM_MOVE LPARAM packing.
    virtual bool  OnMove           (int x, int y)
    {
        UNREFERENCED_PARAMETER (x);
        UNREFERENCED_PARAMETER (y);
        return false;
    }

    // WM_SIZE. The host owns its own internal panel-tree layout
    // response; OnSize fires AFTER that work so the client sees
    // the final widthPx / heightPx.
    virtual bool  OnSize           (UINT widthPx, UINT heightPx)
    {
        UNREFERENCED_PARAMETER (widthPx);
        UNREFERENCED_PARAMETER (heightPx);
        return false;
    }

    // WM_TIMER.
    virtual bool  OnTimer          (UINT_PTR timerId)
    {
        UNREFERENCED_PARAMETER (timerId);
        return false;
    }

    // WM_NOTIFY.
    virtual bool  OnNotify         (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return false;
    }

    // WM_DRAWITEM. The client is responsible for drawing the
    // owner-drawn item; return true to indicate handled.
    virtual bool  OnDrawItem       (int idCtl, DRAWITEMSTRUCT * pdis)
    {
        UNREFERENCED_PARAMETER (idCtl);
        UNREFERENCED_PARAMETER (pdis);
        return false;
    }

    // WM_INITMENUPOPUP. itemIndex is LOWORD(lParam);
    // isWindowMenu is HIWORD(lParam) != 0.
    virtual bool  OnInitMenuPopup  (HMENU hMenu, UINT itemIndex, bool isWindowMenu)
    {
        UNREFERENCED_PARAMETER (hMenu);
        UNREFERENCED_PARAMETER (itemIndex);
        UNREFERENCED_PARAMETER (isWindowMenu);
        return false;
    }

    // WM_PAINT.
    virtual bool  OnPaint          ()
    {
        return false;
    }

    // WM_CLOSE. Return true to consume and prevent DefWindowProc
    // from calling DestroyWindow (e.g. for "really close?" prompts).
    virtual bool  OnClose          ()
    {
        return false;
    }

    // WM_DESTROY. Notification only — the HWND is already in
    // teardown by the time this fires. Use to persist state
    // (window placement, etc.) before the HWND goes away.
    virtual void  OnDestroy        ()
    {
    }

    // WM_NCLBUTTONUP. Fires AFTER the host has run its own NC
    // bookkeeping. hitTest carries the original WM_NCHITTEST result
    // (HTMINBUTTON / HTMAXBUTTON / HTCLOSE / HTCAPTION / ...);
    // (xScreen, yScreen) are the click in screen coordinates.
    // Return true to consume the click (suppress the OS default
    // action for system buttons).
    virtual bool  OnNcLButtonUp    (LRESULT hitTest, int xScreen, int yScreen)
    {
        UNREFERENCED_PARAMETER (hitTest);
        UNREFERENCED_PARAMETER (xScreen);
        UNREFERENCED_PARAMETER (yScreen);
        return false;
    }

    // WM_DPICHANGED post-resize hook. Fires AFTER the host has
    // updated its DPI scaler and applied the OS-suggested rect
    // via SetWindowPos. The client uses this to relayout its own
    // resources (icons re-rasterized, font caches, etc.) that
    // depend on the new DPI.
    virtual void  OnDpiChanged     (UINT newDpi)
    {
        UNREFERENCED_PARAMETER (newDpi);
    }
};
