#pragma once

#include "Pch.h"





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

protected:
    WORD      m_idIcon        = 0;
    WORD      m_idIconSmall   = 0;
    WORD      m_idMenuName    = 0;
    HBRUSH    m_hbrBackground = nullptr;
    LPCWSTR   m_kpszWndClass  = nullptr;
    HINSTANCE m_hInstance      = nullptr;
    HWND      m_hwnd           = nullptr;
};



