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
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DebugConsole::OnCreate (HWND hwnd, CREATESTRUCT * pcs)
{
    HRESULT hr       = S_OK;
    UINT    dpi      = 0;
    int     fontSize = 0;
    HFONT   hFont    = nullptr;



    dpi = GetDpiForWindow (hwnd);
    CWRA (dpi);

    fontSize = MulDiv (16, dpi, 96);



    m_editCtrl = CreateWindowEx (WS_EX_CLIENTEDGE,
                                 L"EDIT",
                                 L"",
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                 0, 0, 600, 400,
                                 hwnd,
                                 nullptr,
                                 pcs->hInstance,
                                 nullptr);
    CWRA (m_editCtrl);

    hFont = CreateFont (fontSize, 0,
                        0, 0,
                        FW_NORMAL,
                        FALSE,
                        FALSE,
                        FALSE,
                        DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY,
                        FIXED_PITCH | FF_MODERN,
                        L"Consolas");
    CWRA (hFont);

    SendMessage (m_editCtrl, WM_SETFONT, (WPARAM) hFont, TRUE);

Error:
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
        MoveWindow (m_editCtrl, 0, 0, static_cast<int> (width), static_cast<int> (height), TRUE);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeConsole
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugConsole::InitializeConsole (HINSTANCE hInstance)
{
    HRESULT hr = S_OK;



    m_kpszWndClass  = L"Casso65DebugConsole";
    m_hbrBackground = reinterpret_cast<HBRUSH> (COLOR_WINDOW + 1);

    hr = Window::Initialize (hInstance);
    CHR (hr);

    hr = Window::Create (0,
                         L"Casso65 Debug Console",
                         WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         600, 400,
                         nullptr);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

bool DebugConsole::Show (HINSTANCE hInstance)
{
    HRESULT hr      = S_OK;
    bool    created = false;



    if (m_hwnd == nullptr)
    {
        hr = InitializeConsole (hInstance);
        CHR (hr);

        created = true;
    }

    if (m_hwnd != nullptr)
    {
        ShowWindow (m_hwnd, SW_SHOW);
        SetForegroundWindow (m_hwnd);
    }

Error:
    return created;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::Hide ()
{
    if (m_hwnd == nullptr)
    {
        return;
    }

    ShowWindow (m_hwnd, SW_HIDE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DebugConsole::IsVisible () const
{
    return m_hwnd != nullptr && IsWindowVisible (m_hwnd);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Log
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::Log (const wstring & message)
{
    wstring text;
    int     len = 0;
    size_t  pos = 0;



    if (m_editCtrl == nullptr)
    {
        return;
    }

    // Win32 EDIT controls require \r\n for line breaks
    text = message + L"\r\n";

    while ((pos = text.find (L'\n', pos)) != wstring::npos)
    {
        if (pos == 0 || text[pos - 1] != L'\r')
        {
            text.insert (pos, 1, L'\r');
            pos++;
        }

        pos++;
    }

    len = GetWindowTextLength (m_editCtrl);

    SendMessage (m_editCtrl, EM_SETSEL, len, len);
    SendMessage (m_editCtrl, EM_REPLACESEL, FALSE, (LPARAM) text.c_str ());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogConfig
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::LogConfig (const string & summary)
{
    wstring wide;



    if (m_editCtrl == nullptr)
    {
        return;
    }

    wide.assign (summary.begin (), summary.end ());
    Log (wide);
}





