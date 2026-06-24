#pragma once

#include "Pch.h"

#include "Ui/DpiScaler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Window
//
//  Base class for Win32 windows.  Provides window class registration,
//  creation, and a virtual message-handler dispatch so derived classes
//  only override the messages they care about.
//
////////////////////////////////////////////////////////////////////////////////

class Window
{
public:
    Window();
    virtual ~Window();

    virtual HRESULT Initialize (HINSTANCE hInstance);
    virtual HRESULT Create (
        DWORD     dwExStyle,
        LPCWSTR   pszTitle,
        DWORD     dwStyle,
        int       x,
        int       y,
        int       width,
        int       height,
        HWND      hwndParent);

    HWND GetHwnd() const { return m_hwnd; }

    // Authoritative per-window DPI. Set automatically on WM_CREATE
    // (from GetDpiForWindow) and updated on WM_DPICHANGED. Subclasses
    // and consumers should read DPI through this scaler -- never call
    // GetDpiForWindow directly, never cache a copy, never plumb a
    // `dpi` parameter through a call chain.
    const DpiScaler &  Scaler () const { return m_scaler; }
    UINT               Dpi    () const { return m_scaler.Dpi(); }

    // Pre-Create bootstrap. Lets the caller seed the DPI from the
    // monitor the window will be created on so any pre-window sizing
    // math (e.g. initial client rect) has a coherent DPI to work
    // against. WM_CREATE overwrites this with GetDpiForWindow, which
    // is authoritative. No-op if called after the HWND exists.
    void  SetInitialDpi (UINT dpi);

protected:
    ATOM RegisterWindowClass (HINSTANCE hInstance);

    static Window * s_GetSetThisPointer (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Virtual message handlers — return true to call DefWindowProc
    virtual bool    OnChar     (WPARAM ch, LPARAM lParam);
    virtual bool    OnCommand  (HWND hwnd, int id);
    virtual bool    OnCommandEx (HWND hwnd, int id, int notifyCode, HWND hCtl);
    virtual HBRUSH  OnCtlColorStatic (HWND hwndDlg, HDC hdc, HWND hwndStatic);
    virtual bool    OnClose    (HWND hwnd);
    virtual LRESULT OnCreate   (HWND hwnd, CREATESTRUCT * pcs);
    virtual bool    OnDestroy  (HWND hwnd);
    virtual bool    OnDrawItem (HWND hwnd, int idCtl, DRAWITEMSTRUCT * pdis);
    virtual bool    OnInitMenuPopup (HWND hwnd, HMENU hMenu, UINT itemIndex, bool isWindowMenu);
    virtual bool    OnKeyDown  (WPARAM vk, LPARAM lParam);
    virtual bool    OnKeyUp    (WPARAM vk, LPARAM lParam);
    virtual bool    OnMouseMove (WPARAM wParam, LPARAM lParam);
    virtual bool    OnMouseLeave ();
    virtual bool    OnLButtonDown (WPARAM wParam, LPARAM lParam);
    virtual bool    OnLButtonUp (WPARAM wParam, LPARAM lParam);
    virtual bool    OnRButtonDown (WPARAM wParam, LPARAM lParam) { UNREFERENCED_PARAMETER (wParam); UNREFERENCED_PARAMETER (lParam); return true; }
    virtual bool    OnRButtonUp   (WPARAM wParam, LPARAM lParam) { UNREFERENCED_PARAMETER (wParam); UNREFERENCED_PARAMETER (lParam); return true; }
    virtual bool    OnActivateApp (bool active)                  { UNREFERENCED_PARAMETER (active); return true; }
    virtual bool    OnKillFocus   ()                             { return true; }
    virtual bool    OnCancelMode  ()                             { return true; }
    virtual bool    OnGetMinMaxInfo (MINMAXINFO * info)          { UNREFERENCED_PARAMETER (info); return false; }
    virtual bool    OnNotify   (HWND hwnd, WPARAM wParam, LPARAM lParam);
    virtual bool    OnPaint    (HWND hwnd);
    virtual bool    OnMove     (HWND hwnd, int x, int y);
    virtual bool    OnSize     (HWND hwnd, UINT width, UINT height);
    virtual bool    OnTimer    (HWND hwnd, UINT_PTR timerId);

    // Custom-chrome dispatchers (P4). Return true to fall through to
    // DefWindowProc with the original message; return false to short-
    // circuit. Sub-classes that want to short-circuit must also call
    // SetCustomLResult to publish the LRESULT.
    virtual bool    OnNcCalcSize  (HWND hwnd, WPARAM wParam, LPARAM lParam, LRESULT & outResult);
    virtual LRESULT OnNcHitTest   (HWND hwnd, int xScreen, int yScreen);
    virtual bool    OnNcLButtonUp (HWND hwnd, LRESULT hitTest, int xScreen, int yScreen);

    // Custom-chrome opt-in for WM_NCLBUTTONDOWN. Returning true tells
    // s_WndProc to consume the message when the hit-test is HTMINBUTTON
    // / HTMAXBUTTON / HTCLOSE so DefWindowProc does not run its system-
    // button tracking loop (which on WS_CAPTION windows draws ghost
    // caption icons over custom chrome and forces a double-click for
    // the actual action). Default is false so non-chrome windows
    // keep their standard X-button behavior.
    virtual bool    WantsCustomCaptionButtons () const { return false; }

    // DPI lifecycle hooks (Non-Virtual Interface). The base WndProc
    // updates m_scaler, calls OnDpiChanging (subclass relayout BEFORE
    // SetWindowPos), applies the OS-suggested rect, then calls
    // OnDpiChanged (subclass post-resize work). Subclasses override
    // either hook -- they never need to remember to call base.
    virtual void  OnDpiChanging (const DpiScaler & newScaler) { UNREFERENCED_PARAMETER (newScaler); }
    virtual void  OnDpiChanged  (const DpiScaler & newScaler) { UNREFERENCED_PARAMETER (newScaler); }

private:
    // Per-message dispatch helpers. s_WndProc forwards each handled
    // message into one of these so the wndproc itself stays a flat
    // dispatch table. Each helper returns true to fall through to
    // DefWindowProc with the original message; helpers that need to
    // publish a specific LRESULT do so via outRetval.
    bool  HandleCtlColorStatic (HWND hwnd, WPARAM wParam, LPARAM lParam, LRESULT & outRetval);
    bool  HandleCreate         (HWND hwnd, LPARAM lParam, LRESULT & outRetval);
    bool  HandleNcHitTest      (HWND hwnd, LPARAM lParam, LRESULT & outRetval);
    bool  HandleNcLButtonDown  (HWND hwnd, WPARAM wParam, LPARAM lParam);
    bool  HandleNcLButtonUp    (HWND hwnd, WPARAM wParam, LPARAM lParam);
    bool  HandleNcMouseMove    (HWND hwnd, LPARAM lParam);
    bool  HandleNcMouseLeave   ();
    bool  HandleSettingChange  (LPARAM lParam);
    bool  HandleDpiChanged     (HWND hwnd, WPARAM wParam, LPARAM lParam);

protected:
    DpiScaler m_scaler;
    WORD      m_idIcon        = 0;
    WORD      m_idIconSmall   = 0;
    WORD      m_idMenuName    = 0;
    HBRUSH    m_hbrBackground = nullptr;
    LPCWSTR   m_kpszWndClass  = nullptr;
    HINSTANCE m_hInstance      = nullptr;
    HWND      m_hwnd           = nullptr;
};




