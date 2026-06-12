#include "Pch.h"

#include "SettingsWindow.h"
#include "SettingsPanel.h"

#include "Core/DxuiTitleBarHitTest.h"
#include "Window/DxuiHostWindow.h"

#include "../../resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr LPCWSTR  s_kpszSettingsWindowClass = L"Casso.Settings.Window";
static constexpr LPCWSTR  s_kpszSettingsWindowTitle = L"Casso - Settings";
static constexpr DWORD    s_kSettingsWindowStyle    = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_VISIBLE;
static constexpr DWORD    s_kSettingsWindowExStyle  = WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP;
static constexpr int      s_kBaseDpi                = 96;
static constexpr int      s_kCenterDivisor          = 2;
static constexpr int      s_kMinResizeBorderPx      = 8;
static constexpr int      s_kIconSizePx             = 32;
static constexpr WORD     s_kBgraBitCount           = 32;





////////////////////////////////////////////////////////////////////////////////
//
//  LoadIconAsPremulBgra
//
////////////////////////////////////////////////////////////////////////////////

static bool LoadIconAsPremulBgra (
    HINSTANCE                hInstance,
    int                      iconResourceId,
    int                      sizePx,
    std::vector<uint32_t>  & outPixels,
    int                    & outW,
    int                    & outH)
{
    static constexpr int  s_kAlphaShift = 24;
    static constexpr int  s_kRedShift   = 16;
    static constexpr int  s_kGreenShift = 8;
    static constexpr int  s_kByteMask   = 0xFF;
    static constexpr int  s_kByteMax    = 255;

    HICON       hIcon       = nullptr;
    HDC         screenDc    = nullptr;
    HDC         memDc       = nullptr;
    HBITMAP     dib         = nullptr;
    HBITMAP     oldBitmap   = nullptr;
    void      * dibBits     = nullptr;
    BITMAPINFO  bmi         = {};
    bool        success     = false;
    size_t      pixelCount  = (size_t) sizePx * (size_t) sizePx;



    hIcon = (HICON) LoadImageW (hInstance,
                                MAKEINTRESOURCEW (iconResourceId),
                                IMAGE_ICON,
                                sizePx, sizePx,
                                LR_DEFAULTCOLOR);
    if (hIcon == nullptr)
    {
        return false;
    }

    screenDc = GetDC (nullptr);
    memDc    = CreateCompatibleDC (screenDc);

    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = sizePx;
    bmi.bmiHeader.biHeight      = -sizePx;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = s_kBgraBitCount;
    bmi.bmiHeader.biCompression = BI_RGB;

    dib = CreateDIBSection (memDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);

    if (dib != nullptr && dibBits != nullptr)
    {
        oldBitmap = (HBITMAP) SelectObject (memDc, dib);
        memset (dibBits, 0, pixelCount * sizeof (uint32_t));

        if (DrawIconEx (memDc, 0, 0, hIcon, sizePx, sizePx, 0, nullptr, DI_NORMAL))
        {
            uint32_t  * src  = (uint32_t *) dibBits;
            size_t      i    = 0;



            outPixels.assign (pixelCount, 0);

            for (i = 0; i < pixelCount; i++)
            {
                uint32_t  px = src[i];
                uint8_t   a  = (uint8_t) ((px >> s_kAlphaShift) & s_kByteMask);
                uint8_t   r  = (uint8_t) ((px >> s_kRedShift)   & s_kByteMask);
                uint8_t   g  = (uint8_t) ((px >> s_kGreenShift) & s_kByteMask);
                uint8_t   b  = (uint8_t) ( px                   & s_kByteMask);

                r = (uint8_t) ((r * a) / s_kByteMax);
                g = (uint8_t) ((g * a) / s_kByteMax);
                b = (uint8_t) ((b * a) / s_kByteMax);

                outPixels[i] = ((uint32_t) a << s_kAlphaShift) |
                               ((uint32_t) r << s_kRedShift)   |
                               ((uint32_t) g << s_kGreenShift) |
                                (uint32_t) b;
            }

            outW    = sizePx;
            outH    = sizePx;
            success = true;
        }

        SelectObject (memDc, oldBitmap);
    }

    if (dib != nullptr)      { DeleteObject (dib); }
    if (memDc != nullptr)    { DeleteDC (memDc); }
    if (screenDc != nullptr) { ReleaseDC (nullptr, screenDc); }
    DestroyIcon (hIcon);

    return success;
}





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
    ID3D11DeviceContext  * context,
    const ChromeTheme    * theme)
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
    CBRAEx (theme, E_INVALIDARG);
    CBRA   (m_hInstance);
    BAIL_OUT_IF (m_hwnd != nullptr, S_OK);

    m_hwndOwner = hwndOwner;
    m_panel     = panel;
    m_device    = device;
    m_context   = context;
    m_theme     = theme;

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

    m_titleBar.SetTitle (s_kpszSettingsWindowTitle);

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
        m_theme     = nullptr;
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
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::SetTheme (const ChromeTheme * theme)
{
    m_theme = theme;
    m_renderer.SetChrome (&m_titleBar, m_theme);

    if (m_hwnd != nullptr)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
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
    LRESULT  result    = 0;
    LRESULT  delegated = 0;



    if (m_hostWindow != nullptr && m_hostWindow->HandleMessage (message, wParam, lParam, delegated))
    {
        return delegated;
    }

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

        case WM_NCLBUTTONDOWN:
            OnNcMouse (message, wParam, lParam);
            if (OnNcLButtonDown (hwnd, (LRESULT) wParam))
            {
                result = 0;
                break;
            }
            result = DefWindowProcW (hwnd, message, wParam, lParam);
            break;

        case WM_NCLBUTTONUP:
        case WM_NCMOUSEMOVE:
        case WM_NCMOUSELEAVE:
            OnNcMouse (message, wParam, lParam);
            result = DefWindowProcW (hwnd, message, wParam, lParam);
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
    HRESULT                       hr         = S_OK;
    RECT                          rc         = {};
    BOOL                          ok         = FALSE;
    UINT                          dpi        = s_kBaseDpi;
    std::vector<uint32_t>         iconPixels;
    int                           iconW      = 0;
    int                           iconH      = 0;
    DxuiHostWindow::CreateParams  hostParams;



    m_hwnd = hwnd;
    ok = GetClientRect (m_hwnd, &rc);
    CWRA (ok);

    dpi = GetDpiForWindow (m_hwnd);
    m_titleBar.UpdateGeometry (rc.right - rc.left, dpi);
    m_renderer.SetChrome (&m_titleBar, m_theme);

    // Stand up the DxuiHostWindow in adopt mode. The HWND, swap
    // chain, and rendering pipeline stay owned by SettingsWindow and
    // its SettingsWindowRenderer (DComp visual + transparency
    // compositor); the host takes over WM_NCCALCSIZE / WM_NCHITTEST
    // classification (with the legacy TitleBar classifier plugged in
    // via SetHitTestDelegate) and observes DPI / theme messages for
    // tree-side propagation.
    hostParams.title           = L"";
    hostParams.hInstance       = m_hInstance;
    hostParams.ownerHwnd       = m_hwndOwner;
    hostParams.borderless      = true;
    hostParams.resizable       = true;
    hostParams.roundedCorners  = true;
    hostParams.darkMode        = true;
    hostParams.backdrop        = DxuiHostWindowBackdrop::None;
    hostParams.resizeBorderDip = 6.0f;

    hr = DxuiHostWindow::CreateInAdoptMode (m_hwnd, hostParams, m_hostWindow);
    CHRA (hr);

    m_hostWindow->SetHitTestDelegate ([this] (POINT ptScreen) -> LRESULT
    {
        return this->ClassifyHitForLegacyChrome (ptScreen);
    });

    if (LoadIconAsPremulBgra (m_hInstance, IDI_CASSO, s_kIconSizePx, iconPixels, iconW, iconH))
    {
        m_titleBar.SetAppIcon (std::move (iconPixels), iconW, iconH);
    }

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
    // Notify the panel so its page-owned dropdowns release any
    // pooled popup hosts AND clear their host pointer before we
    // tear the DxuiHostWindow down. Skipping this would leave the
    // dropdowns with a dangling host pointer (and a leaked popup
    // entry, if one was open at close time) that the next Show()
    // can't recover from.
    if (m_panel != nullptr)
    {
        m_panel->DetachPopupHosts();
    }

    m_renderer.Shutdown();
    m_hostWindow.reset();
    m_hwnd      = nullptr;
    m_hwndOwner = nullptr;
    m_panel     = nullptr;
    m_device    = nullptr;
    m_context   = nullptr;
    m_theme     = nullptr;
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
    m_titleBar.UpdateGeometry (widthPx, dpi);
    hr  = m_renderer.Resize (widthPx, heightPx, dpi);
    IGNORE_RETURN_VALUE (hr, S_OK);

    // Force a render at the new size. WM_SIZE arrives inside Windows'
    // modal resize loop, which blocks the EmulatorShell's main render
    // loop until the user releases the mouse. Without this explicit
    // Render(), the popup paints with stale layout (OK/Cancel buttons
    // stuck at the old right edge) for the entire drag.
    hr = Render();
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

    m_titleBar.UpdateGeometry (suggestedRect.right - suggestedRect.left, dpi);

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
//  ClassifyHitForLegacyChrome
//
//  Bespoke NC hit-test classifier for the legacy TitleBar surfaces.
//  Plugged into the DxuiHostWindow via SetHitTestDelegate so the
//  adopt-mode shim consults it before falling back to the framework
//  resize-edge classifier. Operates in screen coordinates; converts
//  to client on the way to DxuiTitleBarHitTest.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT SettingsWindow::ClassifyHitForLegacyChrome (POINT ptScreen)
{
    POINT                 pt       = ptScreen;
    RECT                  rcClient = {};
    RECT                  rcTitle  = {};
    RECT                  rcMin    = {};
    RECT                  rcMax    = {};
    RECT                  rcClose  = {};
    DxuiTitleBarHitTestInput  in       = {};
    UINT                  dpi      = s_kBaseDpi;
    int                   framePx  = 0;
    int                   padPx    = 0;
    int                   borderPx = 0;



    if (m_hwnd == nullptr)
    {
        return HTNOWHERE;
    }

    if (!ScreenToClient (m_hwnd, &pt))
    {
        return HTNOWHERE;
    }

    if (!GetClientRect (m_hwnd, &rcClient))
    {
        return HTNOWHERE;
    }

    rcTitle = m_titleBar.GetTitleBarRect();
    rcMin   = m_titleBar.GetButtonRect (SystemButton::Minimize);
    rcMax   = m_titleBar.GetButtonRect (SystemButton::Maximize);
    rcClose = m_titleBar.GetButtonRect (SystemButton::Close);

    dpi      = GetDpiForWindow (m_hwnd);
    framePx  = GetSystemMetricsForDpi (SM_CXSIZEFRAME, dpi);
    padPx    = GetSystemMetricsForDpi (SM_CXPADDEDBORDER, dpi);
    borderPx = framePx + padPx;
    if (borderPx < s_kMinResizeBorderPx)
    {
        borderPx = s_kMinResizeBorderPx;
    }

    in.clientWidth    = rcClient.right - rcClient.left;
    in.clientHeight   = rcClient.bottom - rcClient.top;
    in.mouseX         = pt.x;
    in.mouseY         = pt.y;
    in.titleLeft      = rcTitle.left;
    in.titleTop       = rcTitle.top;
    in.titleRight     = rcTitle.right;
    in.titleBottom    = rcTitle.bottom;
    in.minLeft        = rcMin.left;     in.minTop       = rcMin.top;
    in.minRight       = rcMin.right;    in.minBottom    = rcMin.bottom;
    in.maxLeft        = rcMax.left;     in.maxTop       = rcMax.top;
    in.maxRight       = rcMax.right;    in.maxBottom    = rcMax.bottom;
    in.closeLeft      = rcClose.left;   in.closeTop     = rcClose.top;
    in.closeRight     = rcClose.right;  in.closeBottom  = rcClose.bottom;
    in.resizeBorderPx = borderPx;

    return DxuiTitleBarHitTest::Test (in);
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnNcLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsWindow::OnNcLButtonDown (HWND hwnd, LRESULT hitTest)
{
    WPARAM  command = 0;



    switch (hitTest)
    {
        case HTCLOSE:
            command = SC_CLOSE;
            break;

        case HTMINBUTTON:
            command = SC_MINIMIZE;
            break;

        case HTMAXBUTTON:
            command = IsZoomed (hwnd) ? SC_RESTORE : SC_MAXIMIZE;
            break;

        default:
            return false;
    }

    PostMessageW (hwnd, WM_SYSCOMMAND, command, 0);
    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnNcMouse
//
////////////////////////////////////////////////////////////////////////////////

void SettingsWindow::OnNcMouse (UINT message, WPARAM wParam, LPARAM lParam)
{
    POINT  pt       = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    bool   leftDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    if (message == WM_NCMOUSELEAVE)
    {
        pt.x     = -1;
        pt.y     = -1;
        leftDown = false;
    }
    else
    {
        ScreenToClient (m_hwnd, &pt);
        if (message == WM_NCLBUTTONDOWN)
        {
            leftDown = true;
        }
        else if (message == WM_NCLBUTTONUP)
        {
            leftDown = false;
        }
    }

    m_titleBar.SetMousePosition (pt.x, pt.y, leftDown);
    InvalidateRect (m_hwnd, nullptr, FALSE);
    (void) wParam;
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

    m_titleBar.SetMousePosition (x, y, (wParam & MK_LBUTTON) != 0);

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
        size.cy += TitleBarLayout::DefaultTitleHeight (dpi);
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
    constexpr int  s_kSideGapPx = 8;

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
    bool         ownerKnown = false;



    windowRect = { 0, 0, client.cx, client.cy };
    AdjustWindowRectExForDpi (&windowRect, s_kSettingsWindowStyle, FALSE, s_kSettingsWindowExStyle, dpi);

    width      = windowRect.right  - windowRect.left;
    height     = windowRect.bottom - windowRect.top;
    ownerKnown = (GetWindowRect (hwndOwner, &ownerRect) != FALSE);
    monitor    = MonitorFromWindow (hwndOwner, MONITOR_DEFAULTTONEAREST);
    if (monitor != nullptr && GetMonitorInfoW (monitor, &mi))
    {
        workRect = mi.rcWork;
    }
    else
    {
        workRect = ownerRect;
    }

    // Placement rules (per user spec):
    //   1. Owner maximized -> center on owner.
    //   2. Else, prefer right edge of owner, top-aligned, if the popup
    //      fits entirely on the owner's monitor.
    //   3. Else, try left edge, same rule.
    //   4. Else (neither side fits without clipping the popup off the
    //      monitor), pick the side with more room and align flush
    //      with that monitor's work-area edge. Popup partially
    //      overlaps the owner, which is fine -- never blankets it
    //      since the popup is on one edge of the screen.
    //   5. NEVER span monitor boundaries (final clamp to workRect).
    bool  ownerMaximized = ownerKnown && (IsZoomed (hwndOwner) != FALSE);

    if (! ownerKnown)
    {
        x = workRect.left + (workRect.right  - workRect.left - width)  / s_kCenterDivisor;
        y = workRect.top  + (workRect.bottom - workRect.top  - height) / s_kCenterDivisor;
    }
    else if (ownerMaximized)
    {
        x = ownerRect.left + (ownerRect.right  - ownerRect.left - width)  / s_kCenterDivisor;
        y = ownerRect.top  + (ownerRect.bottom - ownerRect.top  - height) / s_kCenterDivisor;
    }
    else if (ownerRect.right + s_kSideGapPx + width <= workRect.right)
    {
        x = ownerRect.right + s_kSideGapPx;
        y = ownerRect.top;
    }
    else if (ownerRect.left - s_kSideGapPx - width >= workRect.left)
    {
        x = ownerRect.left - s_kSideGapPx - width;
        y = ownerRect.top;
    }
    else
    {
        int  roomRight = workRect.right - ownerRect.right;
        int  roomLeft  = ownerRect.left - workRect.left;

        if (roomRight >= roomLeft)
        {
            x = workRect.right - width;
        }
        else
        {
            x = workRect.left;
        }
        y = ownerRect.top;
    }

    x = std::max<int> (workRect.left, std::min<int> (x, workRect.right  - width));
    y = std::max<int> (workRect.top,  std::min<int> (y, workRect.bottom - height));

    return { x, y, x + width, y + height };
}

