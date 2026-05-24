#include "Pch.h"

#include "Window.h"
#include "Ui/WindowsThemeColors.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Window
//
////////////////////////////////////////////////////////////////////////////////

Window::Window()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~Window
//
////////////////////////////////////////////////////////////////////////////////

Window::~Window()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Window::Initialize (HINSTANCE hInstance)
{
    HRESULT hr   = S_OK;
    ATOM    atom = 0;



    atom = RegisterWindowClass (hInstance);
    CWRA (atom);

    m_hInstance = hInstance;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterWindowClass
//
////////////////////////////////////////////////////////////////////////////////

ATOM Window::RegisterWindowClass (HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = { sizeof (wcex) };



    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = s_WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = LoadIconW (hInstance, MAKEINTRESOURCE (m_idIcon));
    wcex.hCursor       = LoadCursorW (nullptr, IDC_ARROW);
    wcex.hbrBackground = m_hbrBackground;
    wcex.lpszMenuName  = MAKEINTRESOURCEW (m_idMenuName);
    wcex.lpszClassName = m_kpszWndClass;
    wcex.hIconSm       = LoadIconW (hInstance, MAKEINTRESOURCE (m_idIconSmall));

    return RegisterClassExW (&wcex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Window::Create (
    DWORD     dwExStyle,
    LPCWSTR   pszTitle,
    DWORD     dwStyle,
    int       x,
    int       y,
    int       width,
    int       height,
    HWND      hwndParent)
{
    HRESULT hr = S_OK;



    m_hwnd = CreateWindowExW (dwExStyle,
                              m_kpszWndClass,
                              pszTitle,
                              dwStyle,
                              x, y,
                              width, height,
                              hwndParent,
                              nullptr,
                              m_hInstance,
                              this);
    CWRA (m_hwnd);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_GetSetThisPointer
//
////////////////////////////////////////////////////////////////////////////////

Window * Window::s_GetSetThisPointer (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window * pThis = nullptr;



    UNREFERENCED_PARAMETER (wParam);



    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW * pcs = reinterpret_cast<CREATESTRUCTW *> (lParam);
        pThis = static_cast<Window *> (pcs->lpCreateParams);
        SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (pThis));
    }
    else
    {
        pThis = reinterpret_cast<Window *> (GetWindowLongPtr (hwnd, GWLP_USERDATA));
    }

    return pThis;
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_WndProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Window::s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    bool      callDefWndProc = false;
    Window  * pThis          = nullptr;



    pThis = s_GetSetThisPointer (hwnd, message, wParam, lParam);

    if (pThis == nullptr)
    {
        return DefWindowProc (hwnd, message, wParam, lParam);
    }

    // Custom-chrome NC mouse handling lives in the per-message switch
    // below. WS_CAPTION is intentionally dropped from the window
    // style so DWM does not draw a second set of system caption
    // buttons over our DX-rendered ones; with WS_CAPTION gone there
    // is nothing for DwmDefWindowProc to coordinate, so the
    // forwarding shim that used to live here has been retired.

    switch (message)
    {
        case WM_CHAR:
            callDefWndProc = pThis->OnChar (wParam, lParam);
            break;

        case WM_COMMAND:
            callDefWndProc = pThis->OnCommandEx (hwnd,
                                                 LOWORD (wParam),
                                                 HIWORD (wParam),
                                                 reinterpret_cast<HWND> (lParam));
            break;

        case WM_CTLCOLORSTATIC:
        {
            HBRUSH  hbr = pThis->OnCtlColorStatic (hwnd,
                                                   reinterpret_cast<HDC> (wParam),
                                                   reinterpret_cast<HWND> (lParam));

            if (hbr != nullptr)
            {
                return reinterpret_cast<LRESULT> (hbr);
            }

            callDefWndProc = true;
            break;
        }

        case WM_CLOSE:
            callDefWndProc = pThis->OnClose (hwnd);
            break;

        case WM_CREATE:
            return pThis->OnCreate (hwnd, reinterpret_cast<CREATESTRUCT *> (lParam));

        case WM_DESTROY:
            callDefWndProc = pThis->OnDestroy (hwnd);
            break;

        case WM_DRAWITEM:
            callDefWndProc = pThis->OnDrawItem (hwnd,
                                                static_cast<int> (wParam),
                                                reinterpret_cast<DRAWITEMSTRUCT *> (lParam));
            break;

        case WM_INITMENUPOPUP:
            callDefWndProc = pThis->OnInitMenuPopup (hwnd,
                                                     reinterpret_cast<HMENU> (wParam),
                                                     LOWORD (lParam),
                                                     HIWORD (lParam) != 0);
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            // Route system-key events (Alt, Alt+key, F10) through the same
            // OnKeyDown handler as plain keys so the emulated keyboard
            // sees Alt-modifier state. Returns true (call DefWindowProc)
            // for unconsumed events so Windows still handles Alt+F4,
            // Alt+Space (system menu), F10, etc.
            callDefWndProc = pThis->OnKeyDown (wParam, lParam);
            break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            callDefWndProc = pThis->OnKeyUp (wParam, lParam);
            break;

        case WM_NCCALCSIZE:
        {
            LRESULT  ncRes  = 0;
            bool     callDef = pThis->OnNcCalcSize (hwnd, wParam, lParam, ncRes);

            if (!callDef)
            {
                return ncRes;
            }
            callDefWndProc = true;
            break;
        }

        case WM_NCHITTEST:
        {
            LRESULT  ncRes = pThis->OnNcHitTest (hwnd,
                                                 (int) (short) LOWORD (lParam),
                                                 (int) (short) HIWORD (lParam));

            if (ncRes != HTNOWHERE)
            {
                return ncRes;
            }
            callDefWndProc = true;
            break;
        }

        case WM_NCLBUTTONDOWN:
        {
            POINT  ptScreen = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
            POINT  ptClient = ptScreen;


            ScreenToClient (hwnd, &ptClient);

            // Surface the press to the chrome painter so caption
            // buttons can light up Pressed visuals.
            (void) pThis->OnMouseMove (MK_LBUTTON,
                                       MAKELPARAM (ptClient.x, ptClient.y));

            // For caption-button hits we swallow the message so
            // DefWindowProc does not run its own system-button
            // tracking (which both draws ghost min/max/close icons
            // over our chrome on WS_CAPTION windows and consumes
            // the first click for itself, forcing a double-click
            // for any single-click min/max action). OnNcLButtonUp
            // dispatches the action on the matching release.
            // Drag (HTCAPTION) and resize (HTLEFT etc.) hits stay
            // unconsumed so DefWindowProc's modal loop continues
            // to own them.
            switch (wParam)
            {
                case HTMINBUTTON:
                case HTMAXBUTTON:
                case HTCLOSE:
                    return 0;

                default:
                    break;
            }

            callDefWndProc = true;
            break;
        }

        case WM_NCLBUTTONUP:
        {
            POINT  ptScreen = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
            POINT  ptClient = ptScreen;
            bool   consumed = false;


            ScreenToClient (hwnd, &ptClient);

            // Clear the press visual.
            (void) pThis->OnMouseMove (0, MAKELPARAM (ptClient.x, ptClient.y));

            // For HTMINBUTTON / HTMAXBUTTON / HTCLOSE the chrome
            // dispatches the action and consumes the message. For
            // every other hit (HTCAPTION drag end, HTLEFT, etc.) we
            // fall through so DefWindowProc finishes its NC loop.
            consumed = pThis->OnNcLButtonUp (hwnd,
                                              (LRESULT) wParam,
                                              ptScreen.x,
                                              ptScreen.y);

            if (consumed)
            {
                return 0;
            }
            callDefWndProc = true;
            break;
        }

        case WM_NCMOUSEMOVE:
        {
            POINT  ptScreen   = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
            POINT  ptClient   = ptScreen;
            WPARAM mouseFlags = (GetKeyState (VK_LBUTTON) & 0x8000) ? MK_LBUTTON : 0;


            ScreenToClient (hwnd, &ptClient);

            // Hover / press tracking for the custom caption buttons.
            // VK_LBUTTON is sampled live so the press visual stays in
            // sync while the OS runs its modal NC loop.
            (void) pThis->OnMouseMove (mouseFlags, MAKELPARAM (ptClient.x, ptClient.y));
            callDefWndProc = true;
            break;
        }

        case WM_NCMOUSELEAVE:
        {
            POINT  ptOff = { -1, -1 };


            // Force the chrome to drop its hot-button state when the
            // cursor leaves the non-client area entirely.
            (void) pThis->OnMouseMove (0, MAKELPARAM ((WORD) ptOff.x, (WORD) ptOff.y));
            callDefWndProc = true;
            break;
        }

        case WM_SETTINGCHANGE:
        {
            const wchar_t *  sectionName = reinterpret_cast<const wchar_t *> (lParam);


            // Refresh the cached Windows light/dark-mode flag so the
            // chrome picks up Settings -> Personalization -> Colors
            // changes without restarting Casso. ImmersiveColorSet is
            // the documented signal for theme swaps.
            if (sectionName != nullptr &&
                _wcsicmp (sectionName, L"ImmersiveColorSet") == 0)
            {
                WindowsThemeColors::Instance().Refresh();
            }
            callDefWndProc = true;
            break;
        }

        case WM_NOTIFY:
            callDefWndProc = pThis->OnNotify (hwnd, wParam, lParam);
            break;

        case WM_PAINT:
            callDefWndProc = pThis->OnPaint (hwnd);
            break;

        case WM_MOUSEMOVE:
            callDefWndProc = pThis->OnMouseMove (wParam, lParam);
            break;

        case WM_LBUTTONDOWN:
            callDefWndProc = pThis->OnLButtonDown (wParam, lParam);
            break;

        case WM_LBUTTONUP:
            callDefWndProc = pThis->OnLButtonUp (wParam, lParam);
            break;

        case WM_MOVE:
            callDefWndProc = pThis->OnMove (hwnd,
                                            (int) (short) LOWORD (lParam),
                                            (int) (short) HIWORD (lParam));
            break;

        case WM_SIZE:
            callDefWndProc = pThis->OnSize (hwnd, LOWORD (lParam), HIWORD (lParam));
            break;

        case WM_TIMER:
            callDefWndProc = pThis->OnTimer (hwnd, static_cast<UINT_PTR> (wParam));
            break;

        default:
            callDefWndProc = true;
            break;
    }

    if (callDefWndProc)
    {
        return DefWindowProc (hwnd, message, wParam, lParam);
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnChar (WPARAM ch, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (ch);
    UNREFERENCED_PARAMETER (lParam);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCommand
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnCommand (HWND hwnd, int id)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (id);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCommandEx
//
//  Default behavior: forward to OnCommand for backwards compatibility
//  with derived classes that don't care about the notification code or
//  control handle. Override OnCommandEx instead when you need either.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnCommandEx (HWND hwnd, int id, int notifyCode, HWND hCtl)
{
    UNREFERENCED_PARAMETER (notifyCode);
    UNREFERENCED_PARAMETER (hCtl);

    return OnCommand (hwnd, id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCtlColorStatic
//
//  Default: return nullptr so the caller falls back to DefWindowProc
//  (which returns the system default static brush). Override to paint
//  a specific static control with custom text / background colors.
//
////////////////////////////////////////////////////////////////////////////////

HBRUSH Window::OnCtlColorStatic (HWND hwndDlg, HDC hdc, HWND hwndStatic)
{
    UNREFERENCED_PARAMETER (hwndDlg);
    UNREFERENCED_PARAMETER (hdc);
    UNREFERENCED_PARAMETER (hwndStatic);

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnClose
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnClose (HWND hwnd)
{
    UNREFERENCED_PARAMETER (hwnd);

    // Default: let DefWindowProc call DestroyWindow
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
////////////////////////////////////////////////////////////////////////////////

LRESULT Window::OnCreate (HWND hwnd, CREATESTRUCT * pcs)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (pcs);

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDestroy
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnDestroy (HWND hwnd)
{
    UNREFERENCED_PARAMETER (hwnd);

    PostQuitMessage (0);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (vk);
    UNREFERENCED_PARAMETER (lParam);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnInitMenuPopup
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnInitMenuPopup (HWND hwnd, HMENU hMenu, UINT itemIndex, bool isWindowMenu)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (hMenu);
    UNREFERENCED_PARAMETER (itemIndex);
    UNREFERENCED_PARAMETER (isWindowMenu);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyUp
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnKeyUp (WPARAM vk, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (vk);
    UNREFERENCED_PARAMETER (lParam);

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnMouseMove (WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnLButtonDown (WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnLButtonUp (WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnNotify
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnNotify (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPaint
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnPaint (HWND hwnd)
{
    UNREFERENCED_PARAMETER (hwnd);

    return true;
}






////////////////////////////////////////////////////////////////////////////////
//
//  OnMove
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnMove (HWND hwnd, int x, int y)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (x);
    UNREFERENCED_PARAMETER (y);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnSize (HWND hwnd, UINT width, UINT height)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (width);
    UNREFERENCED_PARAMETER (height);

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnDrawItem
//
//  Default behavior: tell DefWindowProc to handle the WM_DRAWITEM (which
//  it processes as a no-op for non-button controls). Override in derived
//  classes to paint owner-drawn controls.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnDrawItem (HWND hwnd, int idCtl, DRAWITEMSTRUCT * pdis)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (idCtl);
    UNREFERENCED_PARAMETER (pdis);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnTimer
//
//  Default behavior: pass through to DefWindowProc. Override in derived
//  classes that set up timers via SetTimer.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnTimer (HWND hwnd, UINT_PTR timerId)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (timerId);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcCalcSize / OnNcHitTest / OnNcLButtonUp
//
//  Default behavior: pass through to DefWindowProc (legacy chromed
//  window). Derived classes that opt into a borderless / custom-chrome
//  layout override to return their own NC math.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnNcCalcSize (HWND hwnd, WPARAM wParam, LPARAM lParam, LRESULT & outResult)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    outResult = 0;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcHitTest
//
////////////////////////////////////////////////////////////////////////////////

LRESULT Window::OnNcHitTest (HWND hwnd, int xScreen, int yScreen)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (xScreen);
    UNREFERENCED_PARAMETER (yScreen);

    return HTNOWHERE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnNcLButtonUp (HWND hwnd, LRESULT hitTest, int xScreen, int yScreen)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (hitTest);
    UNREFERENCED_PARAMETER (xScreen);
    UNREFERENCED_PARAMETER (yScreen);

    return false;
}




