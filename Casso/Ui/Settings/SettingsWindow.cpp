#include "Pch.h"

#include "SettingsWindow.h"
#include "SettingsPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr LPCWSTR  s_kpszSettingsWindowClass = L"Casso.Settings.Window";
static constexpr LPCWSTR  s_kpszSettingsWindowTitle = L"Casso settings";
static constexpr DWORD    s_kSettingsWindowStyle    = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE;
static constexpr DWORD    s_kSettingsWindowExStyle  = WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW;
static constexpr int      s_kBaseDpi                = 96;
static constexpr int      s_kCenterDivisor          = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  ~SettingsWindow
//
////////////////////////////////////////////////////////////////////////////////

SettingsWindow::~SettingsWindow()
{
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterClass
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindow::RegisterClass (HINSTANCE hInstance)
{
    HRESULT     hr   = S_OK;
    WNDCLASSEXW wcex = { sizeof (wcex) };
    BOOL        ok   = FALSE;
    ATOM        atom = 0;



    CBRAEx (hInstance, E_INVALIDARG);

    ok = GetClassInfoExW (hInstance, s_kpszSettingsWindowClass, &wcex);
    BAIL_OUT_IF (ok, S_OK);

    wcex.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = SettingsWindow::s_WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = nullptr;
    wcex.hCursor       = LoadCursorW (nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName  = nullptr;
    wcex.lpszClassName = s_kpszSettingsWindowClass;
    wcex.hIconSm       = nullptr;

    atom = RegisterClassExW (&wcex);
    CWRA (atom);

    m_hInstance = hInstance;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindow::Create (
    HWND                   hwndOwner,
    SettingsPanel        * panel,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context)
{
    HRESULT  hr            = S_OK;
    UINT     dpi           = s_kBaseDpi;
    RECT     windowRect    = {};
    HWND     hwndCreated   = nullptr;
    BOOL     ok            = FALSE;



    CBRAEx (hwndOwner, E_INVALIDARG);
    CBRAEx (panel, E_INVALIDARG);
    CBRAEx (device, E_INVALIDARG);
    CBRAEx (context, E_INVALIDARG);
    CBRA   (m_hInstance);
    BAIL_OUT_IF (m_hwnd != nullptr, S_OK);

    m_hwndOwner = hwndOwner;
    m_panel     = panel;
    m_device    = device;
    m_context   = context;

    dpi        = GetDpiForWindow (hwndOwner);
    windowRect = GetInitialWindowRect (hwndOwner, dpi);

    hwndCreated = CreateWindowExW (s_kSettingsWindowExStyle,
                                   s_kpszSettingsWindowClass,
                                   s_kpszSettingsWindowTitle,
                                   s_kSettingsWindowStyle,
                                   windowRect.left,
                                   windowRect.top,
                                   windowRect.right - windowRect.left,
                                   windowRect.bottom - windowRect.top,
                                   hwndOwner,
                                   nullptr,
                                   m_hInstance,
                                   this);
    CWRA (hwndCreated);

    ok = SetWindowTextW (hwndCreated, s_kpszSettingsWindowTitle);
    CWRA (ok);

    ShowWindow (hwndCreated, SW_SHOWNORMAL);
    SetForegroundWindow (hwndCreated);
    SetFocus (hwndCreated);

Error:
    if (FAILED (hr))
    {
        m_hwndOwner = nullptr;
        m_panel     = nullptr;
        m_device    = nullptr;
        m_context   = nullptr;
    }
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Destroy
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::Destroy()
{
    HWND  hwnd = m_hwnd;



    if (hwnd != nullptr)
    {
        DestroyWindow (hwnd);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindow::Render()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_panel == nullptr || m_hwnd == nullptr, S_OK);

    hr = m_renderer.Render (*m_panel);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_WndProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK SettingsWindow::s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    SettingsWindow * window = nullptr;



    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW * cs = reinterpret_cast<CREATESTRUCTW *> (lParam);


        window = reinterpret_cast<SettingsWindow *> (cs->lpCreateParams);
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (window));
    }
    else
    {
        window = reinterpret_cast<SettingsWindow *> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));
    }

    if (window != nullptr)
    {
        return window->WndProc (hwnd, message, wParam, lParam);
    }

    return DefWindowProcW (hwnd, message, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WndProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT SettingsWindow::WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT  result = 0;



    switch (message)
    {
        case WM_CREATE:
            result = FAILED (OnCreate (hwnd)) ? -1 : 0;
            break;

        case WM_DESTROY:
            OnDestroy();
            result = 0;
            break;

        case WM_CLOSE:
            CloseWithCancel();
            result = 0;
            break;

        case WM_GETMINMAXINFO:
            OnGetMinMax (reinterpret_cast<MINMAXINFO *> (lParam));
            result = 0;
            break;

        case WM_SIZE:
            OnSize ((int) LOWORD (lParam), (int) HIWORD (lParam));
            result = 0;
            break;

        case WM_DPICHANGED:
            OnDpiChanged (HIWORD (wParam), *reinterpret_cast<RECT *> (lParam));
            result = 0;
            break;

        case WM_SETFOCUS:
            m_hasFocus = true;
            result     = 0;
            break;

        case WM_KILLFOCUS:
            m_hasFocus = false;
            result     = 0;
            break;

        case WM_KEYDOWN:
            OnKeyDown (wParam);
            result = 0;
            break;

        case WM_CHAR:
            result = 0;
            break;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
            OnMouse (message, wParam, lParam);
            result = 0;
            break;

        case WM_GETOBJECT:
            // TODO: expose the native settings widget tree through UI Automation.
            result = DefWindowProcW (hwnd, message, wParam, lParam);
            break;

        default:
            result = DefWindowProcW (hwnd, message, wParam, lParam);
            break;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsWindow::OnCreate (HWND hwnd)
{
    HRESULT  hr   = S_OK;
    RECT     rc   = {};
    BOOL     ok   = FALSE;
    UINT     dpi  = s_kBaseDpi;



    m_hwnd = hwnd;
    ok = GetClientRect (m_hwnd, &rc);
    CWRA (ok);

    dpi = GetDpiForWindow (m_hwnd);
    hr  = m_renderer.Initialize (m_hwnd,
                                 m_device,
                                 m_context,
                                 rc.right - rc.left,
                                 rc.bottom - rc.top,
                                 dpi);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDestroy
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::OnDestroy()
{
    m_renderer.Shutdown();
    m_hwnd      = nullptr;
    m_hwndOwner = nullptr;
    m_panel     = nullptr;
    m_device    = nullptr;
    m_context   = nullptr;
    m_hasFocus  = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::OnSize (int widthPx, int heightPx)
{
    HRESULT  hr  = S_OK;
    UINT     dpi = s_kBaseDpi;



    BAIL_OUT_IF (m_hwnd == nullptr || !m_renderer.IsInitialized(), S_OK);

    dpi = GetDpiForWindow (m_hwnd);
    hr  = m_renderer.Resize (widthPx, heightPx, dpi);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDpiChanged
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::OnDpiChanged (UINT dpi, const RECT & suggestedRect)
{
    HRESULT  hr = S_OK;
    BOOL     ok = FALSE;



    CBRA (m_hwnd);

    ok = SetWindowPos (m_hwnd,
                       nullptr,
                       suggestedRect.left,
                       suggestedRect.top,
                       suggestedRect.right  - suggestedRect.left,
                       suggestedRect.bottom - suggestedRect.top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    CWRA (ok);

    hr = m_renderer.Resize (suggestedRect.right  - suggestedRect.left,
                            suggestedRect.bottom - suggestedRect.top,
                            dpi);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnGetMinMax
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::OnGetMinMax (MINMAXINFO * minMaxInfo)
{
    RECT  rc  = {};
    UINT  dpi = s_kBaseDpi;
    SIZE  minClient = {};



    if (minMaxInfo == nullptr)
    {
        return;
    }

    if (m_hwnd != nullptr)
    {
        dpi = GetDpiForWindow (m_hwnd);
    }

    minClient = GetPreferredClientSize (dpi);
    rc        = { 0, 0, minClient.cx, minClient.cy };
    AdjustWindowRectExForDpi (&rc, s_kSettingsWindowStyle, FALSE, s_kSettingsWindowExStyle, dpi);

    minMaxInfo->ptMinTrackSize.x = rc.right  - rc.left;
    minMaxInfo->ptMinTrackSize.y = rc.bottom - rc.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::OnMouse (UINT message, WPARAM wParam, LPARAM lParam)
{
    HRESULT  hr = S_OK;
    int      x  = (int) (short) LOWORD (lParam);
    int      y  = (int) (short) HIWORD (lParam);
    POINT    pt = {};



    BAIL_OUT_IF (m_panel == nullptr, S_OK);

    if (message == WM_MOUSEWHEEL)
    {
        pt.x = (int) (short) LOWORD (lParam);
        pt.y = (int) (short) HIWORD (lParam);
        ScreenToClient (m_hwnd, &pt);
        x = pt.x;
        y = pt.y;
    }

    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
    {
        SetCapture (m_hwnd);
        SetFocus (m_hwnd);
        m_panel->OnLButtonDown (x, y);
    }
    else if (message == WM_LBUTTONUP)
    {
        ReleaseCapture();
        m_panel->OnLButtonUp (x, y);
    }
    else
    {
        m_panel->OnMouseMove (x, y);
    }

    DestroyIfPanelClosed();
    (void) wParam;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::OnKeyDown (WPARAM vk)
{
    HRESULT  hr       = S_OK;
    bool     consumed = false;



    BAIL_OUT_IF (m_panel == nullptr, S_OK);

    consumed = m_panel->OnKey (vk);
    if (!consumed && vk == VK_RETURN)
    {
        m_panel->Accept();
    }
    else if (!consumed && vk == VK_ESCAPE)
    {
        m_panel->Cancel();
    }

    DestroyIfPanelClosed();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CloseWithCancel
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::CloseWithCancel()
{
    if (m_panel != nullptr)
    {
        m_panel->Cancel();
    }
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DestroyIfPanelClosed
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::DestroyIfPanelClosed()
{
    if (m_panel != nullptr && !m_panel->IsVisible())
    {
        Destroy();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPreferredClientSize
//
////////////////////////////////////////////////////////////////////////////////

SIZE SettingsWindow::GetPreferredClientSize (UINT dpi) const
{
    SIZE  size = {};



    if (m_panel != nullptr)
    {
        size = m_panel->PreferredClientSize (dpi);
    }
    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetInitialWindowRect
//
////////////////////////////////////////////////////////////////////////////////

RECT SettingsWindow::GetInitialWindowRect (HWND hwndOwner, UINT dpi) const
{
    RECT         windowRect = {};
    RECT         ownerRect  = {};
    RECT         workRect   = {};
    HMONITOR     monitor    = nullptr;
    MONITORINFO  mi         = { sizeof (mi) };
    SIZE         client     = GetPreferredClientSize (dpi);
    int          width      = 0;
    int          height     = 0;
    int          x          = 0;
    int          y          = 0;
    BOOL         ok         = FALSE;



    windowRect = { 0, 0, client.cx, client.cy };
    AdjustWindowRectExForDpi (&windowRect, s_kSettingsWindowStyle, FALSE, s_kSettingsWindowExStyle, dpi);

    width   = windowRect.right  - windowRect.left;
    height  = windowRect.bottom - windowRect.top;
    ok      = GetWindowRect (hwndOwner, &ownerRect);
    monitor = MonitorFromWindow (hwndOwner, MONITOR_DEFAULTTONEAREST);
    if (monitor != nullptr && GetMonitorInfoW (monitor, &mi))
    {
        workRect = mi.rcWork;
    }
    else
    {
        workRect = ownerRect;
    }

    if (ok)
    {
        x = ownerRect.left + (ownerRect.right  - ownerRect.left - width)  / s_kCenterDivisor;
        y = ownerRect.top  + (ownerRect.bottom - ownerRect.top  - height) / s_kCenterDivisor;
    }
    else
    {
        x = workRect.left + (workRect.right  - workRect.left - width)  / s_kCenterDivisor;
        y = workRect.top  + (workRect.bottom - workRect.top  - height) / s_kCenterDivisor;
    }

    x = std::max<int> (workRect.left, std::min<int> (x, workRect.right  - width));
    y = std::max<int> (workRect.top,  std::min<int> (y, workRect.bottom - height));

    return { x, y, x + width, y + height };
}




