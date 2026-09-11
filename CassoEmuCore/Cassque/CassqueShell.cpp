#include "Pch.h"

#include "Cassque/CassqueShell.h"
#include "Theme/DxuiDwm.h"
#include "Theme/DxuiWindowsThemeColors.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::~CassqueShell
//
////////////////////////////////////////////////////////////////////////////////

CassqueShell::~CassqueShell()
{
    if (m_hwnd != nullptr)
    {
        DestroyWindow (m_hwnd);
        m_hwnd = nullptr;
    }

    if (m_oleInitialized)
    {
        OleUninitialize();
        m_oleInitialized = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::ParseArguments
//
//  `--owner` takes the decimal the emulator writes; `--title` takes the rest
//  of one argument, which the emulator quotes. Either may be given once.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueShell::ParseArguments (const std::vector<std::wstring> & arguments, CassqueLaunchOptions & outOptions)
{
    HRESULT  hr    = S_OK;
    size_t   i     = 0;
    bool     valid = true;



    outOptions = CassqueLaunchOptions();

    for (i = 0; i < arguments.size() && valid; i++)
    {
        const std::wstring &  argument = arguments[i];
        bool                  hasValue = i + 1 < arguments.size();

        if (argument == L"--owner" && hasValue && !outOptions.hasOwner)
        {
            const std::wstring &  value  = arguments[++i];
            wchar_t *             end    = nullptr;
            uint64_t              handle = 0;

            valid = !value.empty() && iswdigit (value[0]);

            if (valid)
            {
                handle = wcstoull (value.c_str(), &end, 10);
                valid  = end != nullptr && *end == L'\0' && handle != 0;
            }

            if (valid)
            {
                outOptions.owner    = (HWND) (UINT_PTR) handle;
                outOptions.hasOwner = true;
            }
            else
            {
                outOptions.refusal = L"--owner takes a window handle in decimal, not \"" + value + L"\".";
            }
        }
        else if (argument == L"--title" && hasValue && outOptions.titlePrefix.empty())
        {
            outOptions.titlePrefix = arguments[++i];
        }
        else
        {
            valid              = false;
            outOptions.refusal = L"Cassque does not take \"" + argument + L"\" here. It takes --owner <handle> and --title <label>.";
        }
    }

    CBREx (valid, E_INVALIDARG);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::FindWindowForOwner
//
////////////////////////////////////////////////////////////////////////////////

HWND CassqueShell::FindWindowForOwner (HWND owner)
{
    HWND  found = nullptr;
    HWND  next  = nullptr;



    if (owner == nullptr)
    {
        return nullptr;
    }

    do
    {
        next = FindWindowExW (nullptr, next, kWindowClass, nullptr);

        if (next != nullptr && (HWND) GetPropW (next, kOwnerProperty) == owner)
        {
            found = next;
        }
    }
    while (next != nullptr && found == nullptr);

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::FrontWindow
//
//  A process launched by the emulator's menu item holds the foreground
//  permission the click granted, so it can pass it to the window it found.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueShell::FrontWindow (HWND hwnd)
{
    BOOL  result = FALSE;



    if (IsIconic (hwnd))
    {
        result = ShowWindow (hwnd, SW_RESTORE);
        IGNORE_RETURN_VALUE (result, TRUE);
    }

    result = SetForegroundWindow (hwnd);
    IGNORE_RETURN_VALUE (result, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::ChooseTheme
//
////////////////////////////////////////////////////////////////////////////////

const DxuiTheme & CassqueShell::ChooseTheme (const std::string   & theme,
                                             bool                  systemDark,
                                             const DxuiLightTheme & light,
                                             const DxuiDarkTheme  & dark)
{
    if (theme == CassquePrefs::kThemeLight)
    {
        return light;
    }

    if (theme == CassquePrefs::kThemeDark)
    {
        return dark;
    }

    if (systemDark)
    {
        return dark;
    }

    return light;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::ComposeTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueShell::ComposeTitle (const std::wstring & titlePrefix)
{
    if (titlePrefix.empty())
    {
        return kAppName;
    }

    return titlePrefix + L" - " + kAppName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::NotifyUser
//
////////////////////////////////////////////////////////////////////////////////

void CassqueShell::NotifyUser (const wchar_t * message)
{
    int  choice = 0;



    choice = MessageBoxW (nullptr, message != nullptr ? message : L"", kAppName, MB_OK | MB_ICONERROR | MB_TASKMODAL);
    IGNORE_RETURN_VALUE (choice, IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::ReportAssertion
//
//  One dialog at a time, and never while one is already up: a message box
//  runs its own loop and can dispatch the paint that failed, which would stack
//  a second box on the first. A debugger attached gets the break instead.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueShell::ReportAssertion (const wchar_t * message)
{
    static std::atomic<bool>  reporting;
    int                       choice = IDIGNORE;



    if (IsDebuggerPresent())
    {
        __debugbreak();
        return;
    }

    if (reporting.exchange (true))
    {
        return;
    }

    choice = MessageBoxW (nullptr, message != nullptr ? message : L"", L"Cassque assertion failed",
                          MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_TASKMODAL);

    reporting = false;

    if (choice == IDABORT)
    {
        ExitProcess (3);
    }

    if (choice == IDRETRY)
    {
        __debugbreak();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::Initialize
//
//  COM first, because the drag source and drop target the browser will carry
//  need OLE, and OLE initialization has to precede any window that registers
//  for drops.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueShell::Initialize (HINSTANCE instance, const CassqueLaunchOptions & options, const CassquePrefs & prefs, int showCommand)
{
    HRESULT  hr = S_OK;



    m_instance = instance;
    m_options  = options;
    m_prefs    = prefs;
    m_theme    = &ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

    hr = OleInitialize (nullptr);
    CHR (hr);

    m_oleInitialized = true;

    hr = RegisterWindowClass();
    CHR (hr);

    hr = CreateMainWindow (showCommand);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::RegisterWindowClass
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueShell::RegisterWindowClass()
{
    HRESULT      hr         = S_OK;
    WNDCLASSEXW  windowInfo = { sizeof (windowInfo) };
    ATOM         atom       = 0;
    bool         usable     = false;



    windowInfo.style         = CS_HREDRAW | CS_VREDRAW;
    windowInfo.lpfnWndProc   = &CassqueShell::WindowProc;
    windowInfo.hInstance     = m_instance;
    windowInfo.hIcon         = LoadIconW (m_instance, MAKEINTRESOURCEW (IDI_CASSQUE));
    windowInfo.hIconSm       = windowInfo.hIcon;
    windowInfo.hCursor       = LoadCursorW (nullptr, IDC_ARROW);
    windowInfo.lpszClassName = kWindowClass;

    atom   = RegisterClassExW (&windowInfo);
    usable = atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    CWR (usable);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::TryGetRememberedRect
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueShell::TryGetRememberedRect (const CassquePrefs::Placement & placement, RECT & outRect)
{
    HMONITOR  monitor = nullptr;



    if (!placement.valid || placement.w <= 0 || placement.h <= 0)
    {
        return false;
    }

    outRect = RECT { placement.x, placement.y, placement.x + placement.w, placement.y + placement.h };
    monitor = MonitorFromRect (&outRect, MONITOR_DEFAULTTONULL);

    return monitor != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::CreateMainWindow
//
//  The default size is scaled for the primary monitor, where a window with no
//  remembered placement opens.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueShell::CreateMainWindow (int showCommand)
{
    HRESULT       hr         = S_OK;
    RECT          remembered = {};
    bool          hasRect    = TryGetRememberedRect (m_prefs.placement, remembered);
    UINT          dpi        = GetDpiForSystem();
    int           x          = CW_USEDEFAULT;
    int           y          = CW_USEDEFAULT;
    int           width      = MulDiv (kDefaultWidthDip,  (int) dpi, USER_DEFAULT_SCREEN_DPI);
    int           height     = MulDiv (kDefaultHeightDip, (int) dpi, USER_DEFAULT_SCREEN_DPI);
    std::wstring  title      = ComposeTitle (m_options.titlePrefix);
    BOOL          result     = FALSE;
    bool          maximize   = hasRect && m_prefs.placement.maximized;



    if (hasRect)
    {
        x      = remembered.left;
        y      = remembered.top;
        width  = remembered.right - remembered.left;
        height = remembered.bottom - remembered.top;
    }

    m_hwnd = CreateWindowExW (0, kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW,
                              x, y, width, height, nullptr, nullptr, m_instance, this);
    CWR (m_hwnd != nullptr);

    if (m_options.hasOwner)
    {
        result = SetPropW (m_hwnd, kOwnerProperty, (HANDLE) m_options.owner);
        CWR (result);
    }

    DxuiDwm::ApplyImmersiveDarkMode (m_hwnd, m_theme == &m_darkTheme);

    //  A launch that asked to start minimized keeps that; otherwise a
    //  remembered maximized window opens maximized.
    if (maximize && showCommand != SW_SHOWMINIMIZED && showCommand != SW_SHOWMINNOACTIVE && showCommand != SW_MINIMIZE)
    {
        showCommand = SW_SHOWMAXIMIZED;
    }

    result = ShowWindow (m_hwnd, showCommand);
    IGNORE_RETURN_VALUE (result, FALSE);

    result = UpdateWindow (m_hwnd);
    IGNORE_RETURN_VALUE (result, TRUE);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::RunMessageLoop
//
////////////////////////////////////////////////////////////////////////////////

int CassqueShell::RunMessageLoop()
{
    MSG   message = {};
    BOOL  got     = FALSE;



    for (;;)
    {
        got = GetMessageW (&message, nullptr, 0, 0);

        if (got == 0 || got == -1)
        {
            break;
        }

        TranslateMessage (&message);
        DispatchMessageW (&message);
    }

    return (got == -1) ? 1 : (int) message.wParam;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::ToColorRef
//
////////////////////////////////////////////////////////////////////////////////

COLORREF CassqueShell::ToColorRef (uint32_t argb)
{
    return RGB ((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::WindowProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK CassqueShell::WindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CassqueShell *  shell = nullptr;



    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTW *  create = (const CREATESTRUCTW *) lParam;

        shell         = (CassqueShell *) create->lpCreateParams;
        shell->m_hwnd = hwnd;
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, (LONG_PTR) shell);
    }
    else
    {
        shell = (CassqueShell *) GetWindowLongPtrW (hwnd, GWLP_USERDATA);
    }

    if (shell == nullptr)
    {
        return DefWindowProcW (hwnd, message, wParam, lParam);
    }

    return shell->HandleMessage (message, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell::HandleMessage
//
//  The window is empty for now, so painting is the background and nothing
//  more.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CassqueShell::HandleMessage (UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND     hwnd = m_hwnd;
    LRESULT  lr   = 0;



    switch (message)
    {
        case WM_ERASEBKGND:
        {
            RECT    client = {};
            HBRUSH  brush  = CreateSolidBrush (ToColorRef (m_theme->Background()));

            GetClientRect (hwnd, &client);
            FillRect ((HDC) wParam, &client, brush);
            DeleteObject (brush);

            lr = 1;
            break;
        }

        case WM_DESTROY:
            RemovePropW (hwnd, kOwnerProperty);
            SetWindowLongPtrW (hwnd, GWLP_USERDATA, 0);
            m_hwnd = nullptr;
            PostQuitMessage (0);
            break;

        default:
            lr = DefWindowProcW (hwnd, message, wParam, lParam);
            break;
    }

    return lr;
}
