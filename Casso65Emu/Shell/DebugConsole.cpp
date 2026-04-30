#include "Pch.h"

#include "DebugConsole.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugConsole
//
////////////////////////////////////////////////////////////////////////////////

DebugConsole::DebugConsole ()
    : m_visible  (false),
      m_hwnd     (nullptr),
      m_editCtrl (nullptr)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DebugConsole
//
////////////////////////////////////////////////////////////////////////////////

DebugConsole::~DebugConsole ()
{
    Hide ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WndProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK DebugConsole::WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DebugConsole * self = reinterpret_cast<DebugConsole *> (GetWindowLongPtr (hwnd, GWLP_USERDATA));

    switch (msg)
    {
        case WM_CREATE:
        {
            CREATESTRUCT * cs = reinterpret_cast<CREATESTRUCT *> (lParam);
            SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (cs->lpCreateParams));
            return 0;
        }

        case WM_SIZE:
        {
            if (self != nullptr && self->m_editCtrl != nullptr)
            {
                RECT rc;
                GetClientRect (hwnd, &rc);
                MoveWindow (self->m_editCtrl, 0, 0, rc.right, rc.bottom, TRUE);
            }
            return 0;
        }

        case WM_CLOSE:
        {
            // Just hide — don't destroy the emulator
            if (self != nullptr)
            {
                self->Hide ();
            }
            return 0;
        }
    }

    return DefWindowProc (hwnd, msg, wParam, lParam);
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

    // Register window class (once)
    static bool classRegistered = false;

    if (!classRegistered)
    {
        WNDCLASSEX wc    = {};
        wc.cbSize        = sizeof (wc);
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor (nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
        wc.lpszClassName = L"Casso65DebugConsole";
        RegisterClassEx (&wc);
        classRegistered = true;
    }

    m_hwnd = CreateWindowEx (
        0, L"Casso65DebugConsole", L"Casso65 Debug Console",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        nullptr, nullptr, hInstance, this);

    if (m_hwnd != nullptr)
    {
        // Create a read-only multi-line edit control
        m_editCtrl = CreateWindowEx (
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 600, 400,
            m_hwnd, nullptr, hInstance, nullptr);

        // Use a monospace font
        HFONT hFont = CreateFont (
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        if (hFont != nullptr)
        {
            SendMessage (m_editCtrl, WM_SETFONT, (WPARAM) hFont, TRUE);
        }

        ShowWindow (m_hwnd, SW_SHOW);
        m_visible = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::Hide ()
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

void DebugConsole::Log (const std::wstring & message)
{
    if (!m_visible || m_editCtrl == nullptr)
    {
        return;
    }

    // Append text with CRLF
    std::wstring text = message + L"\r\n";
    int len = GetWindowTextLength (m_editCtrl);
    SendMessage (m_editCtrl, EM_SETSEL, len, len);
    SendMessage (m_editCtrl, EM_REPLACESEL, FALSE, (LPARAM) text.c_str ());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogConfig
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::LogConfig (const std::string & summary)
{
    if (!m_visible || m_editCtrl == nullptr)
    {
        return;
    }

    std::wstring wide (summary.begin (), summary.end ());
    Log (wide);
}
