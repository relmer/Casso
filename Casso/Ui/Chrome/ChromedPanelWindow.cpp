#include "Pch.h"

#include "ChromedPanelWindow.h"

#include "IChromedPanelContent.h"
#include "ChromeTheme.h"
#include "TitleBar.h"

#include "../../resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr DWORD  s_kChromedPanelStyle    = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_VISIBLE;
static constexpr DWORD  s_kChromedPanelExStyle  = WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP;
static constexpr int    s_kBaseDpi              = 96;
static constexpr int    s_kCenterDivisor        = 2;
static constexpr int    s_kMinResizeBorderPx    = 8;
static constexpr int    s_kIconSizePx           = 32;
static constexpr WORD   s_kBgraBitCount         = 32;





////////////////////////////////////////////////////////////////////////////////
//
//  LoadIconAsPremulBgra
//
//  Lifted verbatim from SettingsWindow. Used by OnCreate to upload the
//  Casso app icon into the title-bar's BGRA cache.
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
//  ~ChromedPanelWindow
//
////////////////////////////////////////////////////////////////////////////////

ChromedPanelWindow::~ChromedPanelWindow ()
{
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterClass
//
//  Idempotent. className is owned by the caller (typically a constexpr
//  string literal in the content TU). The same class can be registered
//  more than once cheaply -- GetClassInfoExW short-circuits.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ChromedPanelWindow::RegisterClass (HINSTANCE hInstance, LPCWSTR className)
{
    HRESULT     hr   = S_OK;
    WNDCLASSEXW wcex = { sizeof (wcex) };
    BOOL        ok   = FALSE;
    ATOM        atom = 0;



    CBRAEx (hInstance, E_INVALIDARG);
    CBRAEx (className, E_INVALIDARG);

    ok = GetClassInfoExW (hInstance, className, &wcex);
    if (ok)
    {
        m_hInstance = hInstance;
        m_className = className;
        BAIL_OUT_IF (true, S_OK);
    }

    wcex.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = ChromedPanelWindow::s_WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = nullptr;
    wcex.hCursor       = LoadCursorW (nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName  = nullptr;
    wcex.lpszClassName = className;
    wcex.hIconSm       = nullptr;

    atom = RegisterClassExW (&wcex);
    CWRA (atom);

    m_hInstance = hInstance;
    m_className = className;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ChromedPanelWindow::Create (
    HWND                       hwndOwner,
    IChromedPanelContent     * content,
    ID3D11Device             * device,
    ID3D11DeviceContext      * context,
    const ChromeTheme        * theme)
{
    HRESULT  hr            = S_OK;
    UINT     dpi           = s_kBaseDpi;
    RECT     windowRect    = {};
    HWND     hwndCreated   = nullptr;
    BOOL     ok            = FALSE;
    LPCWSTR  effectiveClass = nullptr;
    LPCWSTR  title          = nullptr;



    CBRAEx (hwndOwner, E_INVALIDARG);
    CBRAEx (content, E_INVALIDARG);
    CBRAEx (device, E_INVALIDARG);
    CBRAEx (context, E_INVALIDARG);
    CBRAEx (theme, E_INVALIDARG);
    CBRA   (m_hInstance);
    BAIL_OUT_IF (m_hwnd != nullptr, S_OK);

    m_hwndOwner = hwndOwner;
    m_content   = content;
    m_device    = device;
    m_context   = context;
    m_theme     = theme;

    effectiveClass = (m_className != nullptr) ? m_className : content->GetWindowClassName();
    title          = content->GetWindowTitle();
    CBRAEx (effectiveClass, E_INVALIDARG);
    CBRAEx (title,          E_INVALIDARG);

    dpi        = GetDpiForWindow (hwndOwner);
    windowRect = GetInitialWindowRect (hwndOwner, dpi);

    hwndCreated = CreateWindowExW (s_kChromedPanelExStyle,
                                   effectiveClass,
                                   title,
                                   s_kChromedPanelStyle,
                                   windowRect.left,
                                   windowRect.top,
                                   windowRect.right - windowRect.left,
                                   windowRect.bottom - windowRect.top,
                                   hwndOwner,
                                   nullptr,
                                   m_hInstance,
                                   this);
    CWRA (hwndCreated);

    ok = SetWindowTextW (hwndCreated, title);
    CWRA (ok);

    ShowWindow (hwndCreated, SW_SHOWNORMAL);
    SetForegroundWindow (hwndCreated);
    SetFocus (hwndCreated);

Error:
    if (FAILED (hr))
    {
        m_hwndOwner = nullptr;
        m_content   = nullptr;
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

void ChromedPanelWindow::Destroy ()
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

void ChromedPanelWindow::SetTheme (const ChromeTheme * theme)
{
    m_theme = theme;
    if (m_content != nullptr)
    {
        m_content->SetChromeTheme (&m_titleBar, m_theme);
    }

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

HRESULT ChromedPanelWindow::Render ()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_content == nullptr || m_hwnd == nullptr, S_OK);

    hr = m_content->Render();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_WndProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK ChromedPanelWindow::s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ChromedPanelWindow * window = nullptr;



    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW * cs = reinterpret_cast<CREATESTRUCTW *> (lParam);


        window = reinterpret_cast<ChromedPanelWindow *> (cs->lpCreateParams);
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (window));
    }
    else
    {
        window = reinterpret_cast<ChromedPanelWindow *> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));
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

LRESULT ChromedPanelWindow::WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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

        case WM_NCCALCSIZE:
            if (!OnNcCalcSize (hwnd, wParam, lParam, result))
            {
                break;
            }
            result = DefWindowProcW (hwnd, message, wParam, lParam);
            break;

        case WM_NCHITTEST:
            result = OnNcHitTest (hwnd, (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam));
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
            if (m_content != nullptr)
            {
                m_content->OnChar ((wchar_t) wParam);
            }
            result = 0;
            break;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_MOUSEWHEEL:
            OnMouse (message, wParam, lParam);
            result = 0;
            break;

        case WM_GETOBJECT:
            // TODO: expose the panel content tree through UI Automation.
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

HRESULT ChromedPanelWindow::OnCreate (HWND hwnd)
{
    HRESULT               hr         = S_OK;
    RECT                  rc         = {};
    BOOL                  ok         = FALSE;
    UINT                  dpi        = s_kBaseDpi;
    std::vector<uint32_t> iconPixels;
    int                   iconW      = 0;
    int                   iconH      = 0;



    m_hwnd = hwnd;
    ok = GetClientRect (m_hwnd, &rc);
    CWRA (ok);

    dpi = GetDpiForWindow (m_hwnd);
    m_titleBar.UpdateGeometry (rc.right - rc.left, dpi);

    if (LoadIconAsPremulBgra (m_hInstance, IDI_CASSO, s_kIconSizePx, iconPixels, iconW, iconH))
    {
        m_titleBar.SetAppIcon (std::move (iconPixels), iconW, iconH);
    }

    CBRA (m_content);
    hr = m_content->OnHostCreated (m_hwnd,
                                   m_device,
                                   m_context,
                                   rc.right - rc.left,
                                   rc.bottom - rc.top,
                                   dpi,
                                   &m_titleBar,
                                   m_theme);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDestroy
//
////////////////////////////////////////////////////////////////////////////////

void ChromedPanelWindow::OnDestroy ()
{
    if (m_content != nullptr)
    {
        m_content->OnHostDestroyed();
    }
    m_hwnd      = nullptr;
    m_hwndOwner = nullptr;
    m_content   = nullptr;
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

void ChromedPanelWindow::OnSize (int widthPx, int heightPx)
{
    HRESULT  hr  = S_OK;
    UINT     dpi = s_kBaseDpi;



    BAIL_OUT_IF (m_hwnd == nullptr || m_content == nullptr, S_OK);

    dpi = GetDpiForWindow (m_hwnd);
    m_titleBar.UpdateGeometry (widthPx, dpi);
    hr  = m_content->OnHostResize (widthPx, heightPx, dpi);
    IGNORE_RETURN_VALUE (hr, S_OK);

    // Force a render at the new size. WM_SIZE arrives inside Windows'
    // modal resize loop, which blocks the host's main render loop
    // until the user releases the mouse. Without this explicit
    // Render(), the popup paints with stale layout for the entire drag.
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

void ChromedPanelWindow::OnDpiChanged (UINT dpi, const RECT & suggestedRect)
{
    HRESULT  hr = S_OK;
    BOOL     ok = FALSE;



    CBRA (m_hwnd);
    CBRA (m_content);

    ok = SetWindowPos (m_hwnd,
                       nullptr,
                       suggestedRect.left,
                       suggestedRect.top,
                       suggestedRect.right  - suggestedRect.left,
                       suggestedRect.bottom - suggestedRect.top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    CWRA (ok);

    m_titleBar.UpdateGeometry (suggestedRect.right - suggestedRect.left, dpi);

    hr = m_content->OnHostResize (suggestedRect.right  - suggestedRect.left,
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

void ChromedPanelWindow::OnGetMinMax (MINMAXINFO * minMaxInfo)
{
    RECT  rc        = {};
    UINT  dpi       = s_kBaseDpi;
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
    AdjustWindowRectExForDpi (&rc, s_kChromedPanelStyle, FALSE, s_kChromedPanelExStyle, dpi);

    minMaxInfo->ptMinTrackSize.x = rc.right  - rc.left;
    minMaxInfo->ptMinTrackSize.y = rc.bottom - rc.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcCalcSize
//
////////////////////////////////////////////////////////////////////////////////

bool ChromedPanelWindow::OnNcCalcSize (HWND hwnd, WPARAM wParam, LPARAM lParam, LRESULT & outResult)
{
    NCCALCSIZE_PARAMS * pParams     = nullptr;
    LRESULT            defResult    = 0;
    LONG               originalTop  = 0;



    if (wParam == FALSE)
    {
        outResult = 0;
        return false;
    }

    pParams = reinterpret_cast<NCCALCSIZE_PARAMS *> (lParam);
    if (pParams == nullptr)
    {
        outResult = 0;
        return false;
    }

    originalTop = pParams->rgrc[0].top;
    defResult   = DefWindowProcW (hwnd, WM_NCCALCSIZE, wParam, lParam);
    if (defResult != 0)
    {
        outResult = defResult;
        return false;
    }

    pParams->rgrc[0].top = originalTop;
    outResult            = 0;
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcHitTest
//
////////////////////////////////////////////////////////////////////////////////

LRESULT ChromedPanelWindow::OnNcHitTest (HWND hwnd, int xScreen, int yScreen)
{
    POINT                 pt       = { xScreen, yScreen };
    RECT                  rcClient = {};
    RECT                  rcTitle  = {};
    RECT                  rcMin    = {};
    RECT                  rcMax    = {};
    RECT                  rcClose  = {};
    TitleBarHitTestInput  in       = {};
    UINT                  dpi      = s_kBaseDpi;
    int                   framePx  = 0;
    int                   padPx    = 0;
    int                   borderPx = 0;



    if (!ScreenToClient (hwnd, &pt))
    {
        return HTNOWHERE;
    }

    if (!GetClientRect (hwnd, &rcClient))
    {
        return HTNOWHERE;
    }

    rcTitle = m_titleBar.GetTitleBarRect();
    rcMin   = m_titleBar.GetButtonRect (SystemButton::Minimize);
    rcMax   = m_titleBar.GetButtonRect (SystemButton::Maximize);
    rcClose = m_titleBar.GetButtonRect (SystemButton::Close);

    dpi      = GetDpiForWindow (hwnd);
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

    return TitleBarHitTest::Test (in);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool ChromedPanelWindow::OnNcLButtonDown (HWND hwnd, LRESULT hitTest)
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

void ChromedPanelWindow::OnNcMouse (UINT message, WPARAM wParam, LPARAM lParam)
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

void ChromedPanelWindow::OnMouse (UINT message, WPARAM wParam, LPARAM lParam)
{
    HRESULT  hr     = S_OK;
    int      x      = (int) (short) LOWORD (lParam);
    int      y      = (int) (short) HIWORD (lParam);
    int      delta  = 0;
    POINT    pt     = {};



    BAIL_OUT_IF (m_content == nullptr, S_OK);

    if (message == WM_MOUSEWHEEL)
    {
        pt.x = (int) (short) LOWORD (lParam);
        pt.y = (int) (short) HIWORD (lParam);
        ScreenToClient (m_hwnd, &pt);
        x     = pt.x;
        y     = pt.y;
        delta = GET_WHEEL_DELTA_WPARAM (wParam);
    }

    m_titleBar.SetMousePosition (x, y, (wParam & MK_LBUTTON) != 0);

    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
    {
        SetCapture (m_hwnd);
        SetFocus (m_hwnd);
        m_content->OnLButtonDown (x, y);
    }
    else if (message == WM_LBUTTONUP)
    {
        ReleaseCapture();
        m_content->OnLButtonUp (x, y);
    }
    else if (message == WM_RBUTTONDOWN)
    {
        SetFocus (m_hwnd);
        m_content->OnRButtonDown (x, y);
    }
    else if (message == WM_MOUSEWHEEL)
    {
        m_content->OnMouseWheel (x, y, delta);
    }
    else
    {
        m_content->OnMouseMove (x, y);
    }

    DestroyIfContentInactive();
    (void) wParam;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

void ChromedPanelWindow::OnKeyDown (WPARAM vk)
{
    HRESULT  hr       = S_OK;
    bool     consumed = false;



    BAIL_OUT_IF (m_content == nullptr, S_OK);

    consumed = m_content->OnKey (vk);
    if (!consumed && vk == VK_RETURN)
    {
        m_content->Accept();
    }
    else if (!consumed && vk == VK_ESCAPE)
    {
        m_content->Cancel();
    }

    DestroyIfContentInactive();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CloseWithCancel
//
////////////////////////////////////////////////////////////////////////////////

void ChromedPanelWindow::CloseWithCancel ()
{
    if (m_content != nullptr)
    {
        m_content->Cancel();
    }
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DestroyIfContentInactive
//
////////////////////////////////////////////////////////////////////////////////

void ChromedPanelWindow::DestroyIfContentInactive ()
{
    if (m_content != nullptr && !m_content->IsContentActive())
    {
        Destroy();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPreferredClientSize
//
////////////////////////////////////////////////////////////////////////////////

SIZE ChromedPanelWindow::GetPreferredClientSize (UINT dpi) const
{
    SIZE  size = {};



    if (m_content != nullptr)
    {
        size = m_content->PreferredClientSize (dpi);
        size.cy += TitleBarLayout::DefaultTitleHeight (dpi);
    }
    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetInitialWindowRect
//
////////////////////////////////////////////////////////////////////////////////

RECT ChromedPanelWindow::GetInitialWindowRect (HWND hwndOwner, UINT dpi) const
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
    AdjustWindowRectExForDpi (&windowRect, s_kChromedPanelStyle, FALSE, s_kChromedPanelExStyle, dpi);

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

    // Placement rules (matching SettingsWindow):
    //   1. Owner maximized -> center on owner.
    //   2. Else, prefer right edge of owner, top-aligned, if the popup
    //      fits entirely on the owner's monitor.
    //   3. Else, try left edge, same rule.
    //   4. Else, pick the side with more room and align flush with
    //      that monitor's work-area edge.
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

    // Final clamp to monitor work area.
    x = std::max<int> (workRect.left, std::min<int> (x, workRect.right  - width));
    y = std::max<int> (workRect.top,  std::min<int> (y, workRect.bottom - height));

    return { x, y, x + width, y + height };
}
