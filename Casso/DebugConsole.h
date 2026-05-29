#pragma once

#include "Pch.h"
#include "Window.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugConsole
//
//  In-app debug log window (opened via Ctrl+D).
//
////////////////////////////////////////////////////////////////////////////////

class DebugConsole : public Window
{
public:
    DebugConsole ();
    ~DebugConsole ();

    void SetMainWindow (HWND hwndMain) { m_hwndMain = hwndMain; }

    bool Show (HINSTANCE hInstance);
    void Hide ();
    bool IsVisible () const;

    void Log (const wstring & message);
    void LogConfig (const string & summary);

protected:
    LRESULT OnCreate         (HWND hwnd, CREATESTRUCT * pcs) override;
    bool    OnClose          (HWND hwnd)                     override;
    bool    OnKeyDown        (WPARAM vk, LPARAM lParam)      override;
    bool    OnSize           (HWND hwnd, UINT width, UINT height) override;
    HBRUSH  OnCtlColorStatic (HWND hwndDlg, HDC hdc, HWND hwndStatic) override;

private:
    HRESULT InitializeConsole (HINSTANCE hInstance);
    void    ReleaseGdiObjects ();

    static constexpr COLORREF  s_kBgColor    = RGB (0x14, 0x14, 0x14);
    static constexpr COLORREF  s_kTextColor  = RGB (0xE6, 0xE2, 0xD8);

    HWND    m_editCtrl     = nullptr;
    HWND    m_hwndMain     = nullptr;
    HBRUSH  m_hbrText      = nullptr;
    HFONT   m_hFont        = nullptr;
};





