#pragma once

#include "Pch.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageResult
//
//  Self-documenting return value for IDxuiHostClient methods that
//  carry no payload in their reply — the consumer either fully
//  handled the message or punts to the host's DefWindowProc.
//
//  Polarity follows the universal Win32 / WPF / MFC convention:
//  "Handled" means the consumer took care of it (host returns 0
//  from WndProc without calling DefWindowProc); "NotHandled" means
//  the host falls through to DefWindowProc and returns its result.
//
//  This intentionally INVERTS the polarity of a literal Casso
//  ``Window``-style bool override, where ``true`` historically
//  meant "let DefWindowProc run". Using a typed enum eliminates
//  the polarity ambiguity at the call site: ``Handled`` is what
//  it says on the tin, not what some local convention infers.
//
////////////////////////////////////////////////////////////////////////////////


enum class DxuiMessageResult
{
    NotHandled,   // host calls DefWindowProc, returns its result
    Handled,      // host returns 0 from WndProc, does NOT call DefWindowProc
};




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
//  Return semantics:
//      ``DxuiMessageResult``-returning methods: return ``Handled``
//          to claim the message; ``NotHandled`` to let the host
//          call DefWindowProc. Defaults return ``NotHandled``.
//      ``LRESULT``-returning methods: return whatever a real
//          WndProc would return for that message (HT* code,
//          HBRUSH, 0 / -1, ...). Defaults call DefWindowProc so
//          the host's WndProc can simply ``return`` the value.
//      ``void``-returning methods: pure notifications. The host
//          continues processing after the call.
//
//  Threading: invoked on the UI thread from within the host's
//  WndProc dispatch.
//
////////////////////////////////////////////////////////////////////////////////


class IDxuiHostClient
{
public:
    virtual ~IDxuiHostClient() = default;

    // WM_CREATE. lParam carries the CREATESTRUCT * the host passed
    // to CreateWindowEx. Return 0 to continue window creation;
    // (LRESULT)-1 to abort (CreateWindowEx returns NULL and the
    // HWND is never produced). Default calls DefWindowProc, which
    // returns 0. Override to perform post-creation initialization
    // that must run before any other message is dispatched.
    virtual LRESULT  OnCreate         (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return DefWindowProc (hwnd, msg, wParam, lParam);
    }

    // WM_NCHITTEST. Return an HT* code (HTCLIENT, HTCAPTION,
    // HTMAXBUTTON, HTLEFT, HTTOPLEFT, ...) to claim the hit-test
    // result. DxuiHostWindow handles NC hit-testing for borderless
    // windows internally (resize edges + panel-tree walk + the
    // optional hit-test delegate set via SetHitTestDelegate);
    // this hook only fires as a fallback when the host's own
    // classification yields HTCLIENT or HTNOWHERE, so consumers
    // should only override when they need to reclassify what the
    // framework treats as plain client area. Default preserves
    // HTCLIENT so ordinary client mouse messages still fire.
    virtual LRESULT  OnNcHitTest      (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (hwnd);
        UNREFERENCED_PARAMETER (msg);
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return HTCLIENT;
    }

    // WM_CTLCOLORSTATIC. wParam = HDC of the static control;
    // lParam = HWND of the static control. Override to set the
    // HDC text/background colors via SetTextColor / SetBkColor
    // and return an HBRUSH cast to LRESULT to paint the
    // background. Return 0 to let the host fall through to
    // DefWindowProc (which returns the system default brush).
    virtual LRESULT  OnCtlColorStatic (HDC hdc, HWND hCtl)
    {
        UNREFERENCED_PARAMETER (hdc);
        UNREFERENCED_PARAMETER (hCtl);
        return 0;
    }

    // WM_DRAWITEM. wParam = control id; lParam = DRAWITEMSTRUCT *.
    // The client is responsible for drawing the owner-drawn item
    // and should return TRUE (as LRESULT) to indicate the item
    // was painted. Default calls DefWindowProc.
    virtual LRESULT  OnDrawItem       (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return DefWindowProc (hwnd, msg, wParam, lParam);
    }

    // WM_CHAR. wParam carries the translated character (typically
    // from TranslateMessage in the consumer's message pump);
    // lParam carries the key-repeat / scan-code bits. Return
    // ``Handled`` if your override fully processed the character;
    // ``NotHandled`` to let DxuiHostWindow call DefWindowProc.
    // Default returns NotHandled.
    virtual DxuiMessageResult  OnChar           (WPARAM ch, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (ch);
        UNREFERENCED_PARAMETER (lParam);
        return DxuiMessageResult::NotHandled;
    }

    // WM_COMMAND, simple form. commandId is LOWORD(wParam) — the
    // accelerator / menu / control id. Use this override for
    // menu and accelerator commands where the notification code
    // and source HWND do not matter. Return ``Handled`` if your
    // override fully processed the command; ``NotHandled`` to
    // let DxuiHostWindow call DefWindowProc. Default returns
    // NotHandled.
    virtual DxuiMessageResult  OnCommand        (WORD commandId)
    {
        UNREFERENCED_PARAMETER (commandId);
        return DxuiMessageResult::NotHandled;
    }

    // WM_COMMAND, full form. commandId = LOWORD(wParam),
    // notifyCode = HIWORD(wParam), hCtl = (HWND)lParam (NULL for
    // menu / accelerator commands). Default forwards to
    // OnCommand so most clients only have to override the simple
    // form. Override OnCommandEx when the notify code or control
    // HWND matters (e.g. button click vs. dropdown change).
    // Return ``Handled`` if your override fully processed the
    // command; ``NotHandled`` to let DxuiHostWindow call
    // DefWindowProc.
    virtual DxuiMessageResult  OnCommandEx      (WORD commandId, WORD notifyCode, HWND hCtl)
    {
        UNREFERENCED_PARAMETER (notifyCode);
        UNREFERENCED_PARAMETER (hCtl);
        return OnCommand (commandId);
    }

    // WM_KEYDOWN / WM_SYSKEYDOWN. wParam = virtual key code (VK_*);
    // lParam carries repeat count / scan code / context bits per
    // the Win32 keystroke message contract. Return ``Handled`` if
    // your override fully processed the key; ``NotHandled`` to
    // let DxuiHostWindow call DefWindowProc (which, for SYSKEY*,
    // does the menu-mnemonic / Alt-key bookkeeping). Default
    // returns NotHandled.
    virtual DxuiMessageResult  OnKeyDown        (WPARAM vk, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (vk);
        UNREFERENCED_PARAMETER (lParam);
        return DxuiMessageResult::NotHandled;
    }

    // WM_KEYUP / WM_SYSKEYUP. wParam = virtual key code (VK_*);
    // lParam carries scan code / context bits. Return ``Handled``
    // if your override fully processed the key release;
    // ``NotHandled`` to let DxuiHostWindow call DefWindowProc.
    // Default returns NotHandled.
    virtual DxuiMessageResult  OnKeyUp          (WPARAM vk, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (vk);
        UNREFERENCED_PARAMETER (lParam);
        return DxuiMessageResult::NotHandled;
    }

    // WM_MOUSEMOVE. wParam = mouse-button / modifier flags
    // (MK_LBUTTON, MK_SHIFT, ...); lParam packs the (x, y)
    // client-coordinate point (use GET_X_LPARAM / GET_Y_LPARAM
    // to extract). Return ``Handled`` if your override fully
    // processed the move; ``NotHandled`` to let DxuiHostWindow
    // call DefWindowProc. Default returns NotHandled.
    virtual DxuiMessageResult  OnMouseMove      (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return DxuiMessageResult::NotHandled;
    }

    // WM_MOUSELEAVE. Fires once after TrackMouseEvent (TME_LEAVE)
    // when the cursor exits the client area. Return ``Handled``
    // if your override fully processed the leave; ``NotHandled``
    // to let DxuiHostWindow call DefWindowProc. Default returns
    // NotHandled.
    virtual DxuiMessageResult  OnMouseLeave     ()
    {
        return DxuiMessageResult::NotHandled;
    }

    // WM_LBUTTONDOWN. wParam = mouse-button / modifier flags;
    // lParam packs the (x, y) client-coordinate point. Return
    // ``Handled`` if your override fully processed the press;
    // ``NotHandled`` to let DxuiHostWindow call DefWindowProc.
    // Default returns NotHandled.
    virtual DxuiMessageResult  OnLButtonDown    (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return DxuiMessageResult::NotHandled;
    }

    // WM_LBUTTONUP. wParam = mouse-button / modifier flags;
    // lParam packs the (x, y) client-coordinate point. Return
    // ``Handled`` if your override fully processed the release;
    // ``NotHandled`` to let DxuiHostWindow call DefWindowProc.
    // Default returns NotHandled.
    virtual DxuiMessageResult  OnLButtonUp      (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return DxuiMessageResult::NotHandled;
    }

    // WM_RBUTTONDOWN / WM_RBUTTONUP. wParam = mouse-button / modifier
    // flags; lParam packs the (x, y) client-coordinate point. Used by
    // paddle-input mode for the secondary fire button. Default returns
    // NotHandled (DxuiHostWindow calls DefWindowProc).
    virtual DxuiMessageResult  OnRButtonDown    (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return DxuiMessageResult::NotHandled;
    }

    virtual DxuiMessageResult  OnRButtonUp      (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return DxuiMessageResult::NotHandled;
    }

    // WM_ACTIVATEAPP (active = wParam != 0), WM_KILLFOCUS, and
    // WM_CANCELMODE. Surfaced so a consumer can release a live mouse
    // capture (e.g. paddle-input mode) when the app loses the
    // foreground / focus, or the OS cancels capture (secure desktop,
    // lock). Default returns NotHandled (DxuiHostWindow calls
    // DefWindowProc).
    virtual DxuiMessageResult  OnActivateApp    (bool active)
    {
        UNREFERENCED_PARAMETER (active);
        return DxuiMessageResult::NotHandled;
    }

    virtual DxuiMessageResult  OnKillFocus      ()
    {
        return DxuiMessageResult::NotHandled;
    }

    virtual DxuiMessageResult  OnCancelMode     ()
    {
        return DxuiMessageResult::NotHandled;
    }

    // WM_MOVE. (x, y) are the new client-area top-left in screen
    // coordinates per the WM_MOVE LPARAM packing. Return
    // ``Handled`` if your override fully processed the move;
    // ``NotHandled`` to let DxuiHostWindow call DefWindowProc.
    // Default returns NotHandled.
    virtual DxuiMessageResult  OnMove           (int x, int y)
    {
        UNREFERENCED_PARAMETER (x);
        UNREFERENCED_PARAMETER (y);
        return DxuiMessageResult::NotHandled;
    }

    // WM_SIZE. The host owns its own internal panel-tree layout
    // response; OnSize fires AFTER that work so the client sees
    // the final widthPx / heightPx (LOWORD/HIWORD of lParam
    // unpacked). Return ``Handled`` if your override fully
    // processed the resize; ``NotHandled`` to let DxuiHostWindow
    // call DefWindowProc. Default returns NotHandled.
    virtual DxuiMessageResult  OnSize           (UINT widthPx, UINT heightPx)
    {
        UNREFERENCED_PARAMETER (widthPx);
        UNREFERENCED_PARAMETER (heightPx);
        return DxuiMessageResult::NotHandled;
    }

    // WM_TIMER. timerId is the UINT_PTR identifier the client
    // passed to SetTimer. Return ``Handled`` if your override
    // fully processed the tick; ``NotHandled`` to let
    // DxuiHostWindow call DefWindowProc. Default returns
    // NotHandled.
    virtual DxuiMessageResult  OnTimer          (UINT_PTR timerId)
    {
        UNREFERENCED_PARAMETER (timerId);
        return DxuiMessageResult::NotHandled;
    }

    // WM_NOTIFY. wParam = control id; lParam = NMHDR *. Return
    // ``Handled`` if your override fully processed the
    // notification; ``NotHandled`` to let DxuiHostWindow call
    // DefWindowProc. Default returns NotHandled.
    virtual DxuiMessageResult  OnNotify         (WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER (wParam);
        UNREFERENCED_PARAMETER (lParam);
        return DxuiMessageResult::NotHandled;
    }

    // WM_INITMENUPOPUP. hMenu is the menu about to be shown;
    // itemIndex = LOWORD(lParam) is the submenu position;
    // isWindowMenu = HIWORD(lParam) != 0 indicates whether the
    // popup is the window (system) menu. Return ``Handled`` if
    // your override fully populated / updated the popup;
    // ``NotHandled`` to let DxuiHostWindow call DefWindowProc.
    // Default returns NotHandled.
    virtual DxuiMessageResult  OnInitMenuPopup  (HMENU hMenu, UINT itemIndex, bool isWindowMenu)
    {
        UNREFERENCED_PARAMETER (hMenu);
        UNREFERENCED_PARAMETER (itemIndex);
        UNREFERENCED_PARAMETER (isWindowMenu);
        return DxuiMessageResult::NotHandled;
    }

    // WM_PAINT. Override to drive a custom paint cycle (the
    // override is responsible for the BeginPaint / EndPaint pair
    // when claiming the message). Return ``Handled`` if your
    // override fully painted; ``NotHandled`` to let
    // DxuiHostWindow call DefWindowProc (which validates the
    // update region with an empty BeginPaint / EndPaint).
    // Default returns NotHandled.
    virtual DxuiMessageResult  OnPaint          ()
    {
        return DxuiMessageResult::NotHandled;
    }

    // WM_CLOSE. Return ``Handled`` to consume the close and
    // prevent DefWindowProc from calling DestroyWindow (e.g. for
    // "really close?" prompts that may cancel). Return
    // ``NotHandled`` to let DxuiHostWindow call DefWindowProc,
    // which destroys the window. Default returns NotHandled.
    virtual DxuiMessageResult  OnClose          ()
    {
        return DxuiMessageResult::NotHandled;
    }

    // WM_NCMOUSEMOVE. hitTest carries the HT* code under the
    // cursor; (xScreen, yScreen) are screen coordinates. Return
    // ``Handled`` to consume the move; ``NotHandled`` to let the
    // host's NC bookkeeping and DefWindowProc run. Default returns
    // NotHandled.
    virtual DxuiMessageResult  OnNcMouseMove    (LRESULT hitTest, int xScreen, int yScreen)
    {
        UNREFERENCED_PARAMETER (hitTest);
        UNREFERENCED_PARAMETER (xScreen);
        UNREFERENCED_PARAMETER (yScreen);
        return DxuiMessageResult::NotHandled;
    }

    // WM_NCMOUSELEAVE. Return ``Handled`` to consume the leave;
    // ``NotHandled`` to let the host's NC bookkeeping run. Default
    // returns NotHandled.
    virtual DxuiMessageResult  OnNcMouseLeave()
    {
        return DxuiMessageResult::NotHandled;
    }

    // WM_NCLBUTTONDOWN. hitTest carries the HT* code under the
    // cursor; (xScreen, yScreen) are screen coordinates. Return
    // ``Handled`` to consume the press; ``NotHandled`` to let the
    // host's NC bookkeeping and DefWindowProc run. Default returns
    // NotHandled.
    virtual DxuiMessageResult  OnNcLButtonDown  (LRESULT hitTest, int xScreen, int yScreen)
    {
        UNREFERENCED_PARAMETER (hitTest);
        UNREFERENCED_PARAMETER (xScreen);
        UNREFERENCED_PARAMETER (yScreen);
        return DxuiMessageResult::NotHandled;
    }

    // WM_NCLBUTTONUP. hitTest carries the original WM_NCHITTEST
    // result (HTMINBUTTON / HTMAXBUTTON / HTCLOSE / HTCAPTION /
    // ...); (xScreen, yScreen) are the click in screen coordinates.
    // Return ``Handled`` to consume the click (suppresses the OS
    // default action for system buttons); ``NotHandled`` to let the
    // host's own NC bookkeeping run through to DefWindowProc.
    // Default returns NotHandled.
    virtual DxuiMessageResult  OnNcLButtonUp    (LRESULT hitTest, int xScreen, int yScreen)
    {
        UNREFERENCED_PARAMETER (hitTest);
        UNREFERENCED_PARAMETER (xScreen);
        UNREFERENCED_PARAMETER (yScreen);
        return DxuiMessageResult::NotHandled;
    }

    // WM_DESTROY notification. The window is being destroyed;
    // do any cleanup (persist window placement, drop references,
    // ...) here. The host does NOT call PostQuitMessage — call
    // it yourself in your OnDestroy override if this is your
    // application's main window. Child / secondary windows
    // should NOT call PostQuitMessage.
    virtual void  OnDestroy        ()
    {
    }

    // WM_DPICHANGED post-resize hook. Fires AFTER the host has
    // updated its DPI scaler and applied the OS-suggested rect
    // via SetWindowPos. The client uses this to re-rasterize its
    // own DPI-dependent resources (icons, font caches, ...).
    // Notification only — no return value, host continues with
    // its own bookkeeping.
    virtual void  OnDpiChanged     (UINT newDpi)
    {
        UNREFERENCED_PARAMETER (newDpi);
    }
};
