#include "Pch.h"

#include "DebugConsole.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugConsole
//
////////////////////////////////////////////////////////////////////////////////

DebugConsole::DebugConsole ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DebugConsole
//
////////////////////////////////////////////////////////////////////////////////

DebugConsole::~DebugConsole ()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DebugConsole::OnCreate (HWND hwnd, CREATESTRUCT * pcs)
{
    HFONT hFont = nullptr;



    // Create a read-only multi-line edit control
    m_editCtrl = CreateWindowEx (
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, 600, 400,
        hwnd, nullptr, pcs->hInstance, nullptr);

    // Use a monospace font
    hFont = CreateFont (
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    if (hFont != nullptr)
    {
        SendMessage (m_editCtrl, WM_SETFONT, (WPARAM) hFont, TRUE);
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnClose
//
////////////////////////////////////////////////////////////////////////////////

bool DebugConsole::OnClose (HWND hwnd)
{
    UNREFERENCED_PARAMETER (hwnd);

    Hide ();
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
////////////////////////////////////////////////////////////////////////////////

bool DebugConsole::OnSize (HWND hwnd, UINT width, UINT height)
{
    UNREFERENCED_PARAMETER (hwnd);

    if (m_editCtrl != nullptr)
    {
        MoveWindow (m_editCtrl, 0, 0,
                    static_cast<int> (width),
                    static_cast<int> (height), TRUE);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::Show (HINSTANCE hInstance)
{
    if (m_visible)
    {
        SetForegroundWindow (m_hwnd);
        return;
    }

    m_kpszWndClass  = L"Casso65DebugConsole";
    m_hbrBackground = reinterpret_cast<HBRUSH> (COLOR_WINDOW + 1);

    Window::Initialize (hInstance);
    Window::Create (0,
                    L"Casso65 Debug Console",
                    WS_OVERLAPPEDWINDOW,
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    600, 400,
                    nullptr);

    if (m_hwnd != nullptr)
    {
        ShowWindow (m_hwnd, SW_SHOW);
        m_visible = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::Hide()
{
    if (!m_visible)
    {
        return;
    }

    if (m_hwnd != nullptr)
    {
        DestroyWindow (m_hwnd);
        m_hwnd     = nullptr;
        m_editCtrl = nullptr;
    }

    m_visible = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Log
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::Log (const wstring & message)
{
    wstring text;
    int          len = 0;



    if (!m_visible || m_editCtrl == nullptr)
    {
        return;
    }

    // Append text with CRLF
    text = message + L"\r\n";
    len  = GetWindowTextLength (m_editCtrl);
    SendMessage (m_editCtrl, EM_SETSEL, len, len);
    SendMessage (m_editCtrl, EM_REPLACESEL, FALSE, (LPARAM) text.c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogConfig
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::LogConfig (const string & summary)
{
    wstring wide;



    if (!m_visible || m_editCtrl == nullptr)
    {
        return;
    }

    wide.assign (summary.begin(), summary.end());
    Log (wide);
}





