#include "Pch.h"

#include "DebugConsole.h"

#include "Ui/Win11DwmHelpers.h"





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
    ReleaseGdiObjects();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseGdiObjects
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::ReleaseGdiObjects ()
{
    if (m_hbrText != nullptr)
    {
        DeleteObject (m_hbrText);
        m_hbrText = nullptr;
    }

    if (m_hFont != nullptr)
    {
        DeleteObject (m_hFont);
        m_hFont = nullptr;
    }

    if (m_hbrBackground != nullptr)
    {
        DeleteObject (m_hbrBackground);
        m_hbrBackground = nullptr;
    }
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



    dpi = GetDpiForWindow (hwnd);
    CWRA (dpi);

    fontSize = MulDiv (16, dpi, 96);

    Win11DwmHelpers::ApplyImmersiveDarkMode (hwnd, true);

    m_hbrText = CreateSolidBrush (s_kBgColor);
    CWRA (m_hbrText);

    m_editCtrl = CreateWindowEx (0,
                                 L"EDIT",
                                 L"",
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                 0, 0, 600, 400,
                                 hwnd,
                                 nullptr,
                                 pcs->hInstance,
                                 nullptr);
    CWRA (m_editCtrl);

    SetWindowTheme (m_editCtrl, L"DarkMode_Explorer", nullptr);

    m_hFont = CreateFont (fontSize, 0,
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
    CWRA (m_hFont);

    SendMessage (m_editCtrl, WM_SETFONT, (WPARAM) m_hFont, TRUE);

Error:
    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCtlColorStatic
//
//  Read-only EDIT controls send WM_CTLCOLORSTATIC; theme the text and
//  background to match the rest of the chrome.
//
////////////////////////////////////////////////////////////////////////////////

HBRUSH DebugConsole::OnCtlColorStatic (HWND hwndDlg, HDC hdc, HWND hwndStatic)
{
    UNREFERENCED_PARAMETER (hwndDlg);
    UNREFERENCED_PARAMETER (hwndStatic);

    SetTextColor (hdc, s_kTextColor);
    SetBkColor   (hdc, s_kBgColor);
    return m_hbrText;
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
//  OnKeyDown
//
//  Intercepts Alt+F4 and forwards it to the main Casso window so the
//  whole app exits, matching the user's mental model that Alt+F4 is
//  an "exit Casso" gesture regardless of which Casso window currently
//  has focus. Without this override the secondary window would
//  swallow the close to itself via the standard WM_SYSCOMMAND/SC_CLOSE
//  -> WM_CLOSE -> OnClose -> Hide() chain.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugConsole::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    static constexpr LONG_PTR  s_kAltContextBit = 1LL << 29;


    if (vk == VK_F4 && (lParam & s_kAltContextBit) && m_hwndMain != nullptr)
    {
        PostMessage (m_hwndMain, WM_CLOSE, 0, 0);
        return false;
    }

    return Window::OnKeyDown (vk, lParam);
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



    m_kpszWndClass  = L"CassoDebugConsole";
    m_hbrBackground = CreateSolidBrush (s_kBgColor);
    CWRA (m_hbrBackground);

    hr = Window::Initialize (hInstance);
    CHR (hr);

    hr = Window::Create (0,
                         L"Casso debug console",
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





