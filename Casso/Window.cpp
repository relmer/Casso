#include "Pch.h"

#include "Window.h"
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
//  SetInitialDpi
//
////////////////////////////////////////////////////////////////////////////////

void Window::SetInitialDpi (UINT dpi)
{
    // Pre-Create only. WM_CREATE overwrites with GetDpiForWindow,
    // which is the authoritative source once the window exists.
    if (m_hwnd != nullptr)
    {
        return;
    }
    m_scaler.SetDpi (dpi);
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

        // Seed the per-window DPI as early as possible. WM_NCCREATE
        // is the first message that has both an HWND and the `this`
        // pointer bound, so every subsequent message handler can rely
        // on m_scaler being current. WM_DPICHANGED keeps it current
        // afterward.
        if (pThis != nullptr)
        {
            pThis->m_scaler.SetDpi (GetDpiForWindow (hwnd));
        }
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
//  Thin dispatch table. Maps each handled Win32 message to the
//  matching virtual handler (OnXxx) or per-message helper (HandleXxx)
//  and falls through to DefWindowProc when the handler returns true.
//  All per-message logic lives in those handlers/helpers -- this
//  function should stay a flat switch with no inline processing.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Window::s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HRESULT   hr             = S_OK;
    bool      callDefWndProc = true;
    LRESULT   retval         = 0;
    Window  * pThis          = nullptr;



    pThis = s_GetSetThisPointer (hwnd, message, wParam, lParam);
    CBR (pThis);

    // Adopt-mode delegation hook. Subclasses can install a
    // DxuiHostWindow (or any other forwarder) that classifies the
    // message and either claims it end-to-end or lets the legacy
    // dispatch table below run.
    if (pThis->TryDelegateMessage (hwnd, message, wParam, lParam, retval))
    {
        return retval;
    }

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
            callDefWndProc = pThis->HandleCtlColorStatic (hwnd, wParam, lParam, retval);
            break;

        case WM_CLOSE:
            callDefWndProc = pThis->OnClose (hwnd);
            break;

        case WM_CREATE:
            callDefWndProc = pThis->HandleCreate (hwnd, lParam, retval);
            break;

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
            callDefWndProc = pThis->OnKeyDown (wParam, lParam);
            break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            callDefWndProc = pThis->OnKeyUp (wParam, lParam);
            break;

        case WM_NCLBUTTONDOWN:
            callDefWndProc = pThis->HandleNcLButtonDown (hwnd, wParam, lParam);
            break;

        case WM_NCLBUTTONUP:
            callDefWndProc = pThis->HandleNcLButtonUp (hwnd, wParam, lParam);
            break;

        case WM_NCMOUSEMOVE:
            callDefWndProc = pThis->HandleNcMouseMove (hwnd, lParam);
            break;

        case WM_NCMOUSELEAVE:
            callDefWndProc = pThis->HandleNcMouseLeave();
            break;

        case WM_SETTINGCHANGE:
            callDefWndProc = pThis->HandleSettingChange (lParam);
            break;

        case WM_DPICHANGED:
            callDefWndProc = pThis->HandleDpiChanged (hwnd, wParam, lParam);
            break;

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
    }

Error:
    if (callDefWndProc)
    {
        retval = DefWindowProc (hwnd, message, wParam, lParam);
    }
    return retval;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleCtlColorStatic
//
//  Forwards to the subclass-provided HBRUSH handler and publishes the
//  returned brush as the message LRESULT. Falls through to
//  DefWindowProc when the subclass returns nullptr (the standard
//  static-control coloring path).
//
////////////////////////////////////////////////////////////////////////////////

bool Window::HandleCtlColorStatic (HWND hwnd, WPARAM wParam, LPARAM lParam, LRESULT & outRetval)
{
    HBRUSH  hbr = OnCtlColorStatic (hwnd,
                                    reinterpret_cast<HDC> (wParam),
                                    reinterpret_cast<HWND> (lParam));

    if (hbr != nullptr)
    {
        outRetval = reinterpret_cast<LRESULT> (hbr);
        return false;
    }
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleCreate
//
//  Forwards WM_CREATE to the subclass and publishes its LRESULT
//  result. WM_CREATE never falls through to DefWindowProc -- the
//  subclass owns the decision via OnCreate's return value (0 to
//  continue, -1 to abort window creation).
//
////////////////////////////////////////////////////////////////////////////////

bool Window::HandleCreate (HWND hwnd, LPARAM lParam, LRESULT & outRetval)
{
    outRetval = OnCreate (hwnd, reinterpret_cast<CREATESTRUCT *> (lParam));
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleNcLButtonDown
//
//  Surface the press into the chrome painter (so caption buttons can
//  light up Pressed visuals) and swallow the message for custom-
//  caption-button hits when the subclass opts in via
//  WantsCustomCaptionButtons. Drag (HTCAPTION) and resize hits stay
//  unconsumed so DefWindowProc's modal loop continues to own them.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::HandleNcLButtonDown (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    POINT  ptScreen = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    POINT  ptClient = ptScreen;
    bool   consume  = false;



    ScreenToClient (hwnd, &ptClient);

    (void) OnMouseMove (MK_LBUTTON, MAKELPARAM (ptClient.x, ptClient.y));

    if (WantsCustomCaptionButtons())
    {
        consume = (wParam == HTMINBUTTON || wParam == HTMAXBUTTON || wParam == HTCLOSE);
    }

    return !consume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleNcLButtonUp
//
//  Clear the press visual on the chrome painter, then dispatch the
//  release through OnNcLButtonUp. For HTMINBUTTON / HTMAXBUTTON /
//  HTCLOSE the chrome runs the action and consumes the message; every
//  other hit falls through so DefWindowProc finishes its NC loop.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::HandleNcLButtonUp (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    POINT  ptScreen = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    POINT  ptClient = ptScreen;
    bool   consumed = false;



    ScreenToClient (hwnd, &ptClient);

    (void) OnMouseMove (0, MAKELPARAM (ptClient.x, ptClient.y));

    consumed = OnNcLButtonUp (hwnd, (LRESULT) wParam, ptScreen.x, ptScreen.y);

    return !consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleNcMouseMove
//
//  Forward the NC cursor position into OnMouseMove so hover / press
//  tracking for the custom caption buttons stays in sync while the
//  OS runs its modal NC loop. VK_LBUTTON is sampled live because the
//  NC modal loop does not deliver real mouse-button flags here.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::HandleNcMouseMove (HWND hwnd, LPARAM lParam)
{
    POINT  ptScreen   = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    POINT  ptClient   = ptScreen;
    WPARAM mouseFlags = (GetKeyState (VK_LBUTTON) & 0x8000) ? MK_LBUTTON : 0;



    ScreenToClient (hwnd, &ptClient);

    (void) OnMouseMove (mouseFlags, MAKELPARAM (ptClient.x, ptClient.y));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleNcMouseLeave
//
//  Forward to the subclass OnMouseLeave so chrome painters (caption
//  buttons, nav menu) can clear their hot state when the cursor
//  exits the window via the non-client area.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::HandleNcMouseLeave ()
{
    return OnMouseLeave();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleSettingChange
//
//  Refresh the cached Windows light/dark-mode flag when the OS posts
//  an ImmersiveColorSet change so the chrome picks up Settings ->
//  Personalization -> Colors swaps without restarting Casso.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::HandleSettingChange (LPARAM lParam)
{
    const wchar_t *  sectionName = reinterpret_cast<const wchar_t *> (lParam);



    if (sectionName != nullptr &&
        _wcsicmp (sectionName, L"ImmersiveColorSet") == 0)
    {
        DxuiWindowsThemeColors::Instance().Refresh();
    }
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleDpiChanged
//
//  WM_DPICHANGED owns the per-window DPI lifecycle here. Updates the
//  authoritative DxuiDpiScaler, fires the pre-resize subclass hook,
//  applies the OS-suggested rect, then fires the post-resize hook.
//  The base class owns the SetWindowPos so subclasses cannot
//  accidentally skip it by forgetting to call base.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::HandleDpiChanged (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    RECT *  pSuggested = reinterpret_cast<RECT *> (lParam);
    UINT    newDpi     = HIWORD (wParam);



    if (newDpi != 0)
    {
        m_scaler.SetDpi (newDpi);
    }

    OnDpiChanging (m_scaler);

    if (pSuggested != nullptr)
    {
        SetWindowPos (hwnd,
                      nullptr,
                      pSuggested->left,
                      pSuggested->top,
                      pSuggested->right  - pSuggested->left,
                      pSuggested->bottom - pSuggested->top,
                      SWP_NOZORDER | SWP_NOACTIVATE);
    }

    OnDpiChanged (m_scaler);
    return false;
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
//  OnMouseLeave
//
//  Default no-op. Subclasses with hover state (caption buttons, nav
//  menu items) override to clear that state when the cursor exits
//  the window via the NC area.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::OnMouseLeave ()
{
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
//  TryDelegateMessage
//
//  Default implementation — no delegation. Subclasses that adopt a
//  DxuiHostWindow override this to forward the message through the
//  host's HandleMessage shim.
//
////////////////////////////////////////////////////////////////////////////////

bool Window::TryDelegateMessage (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT & outRetval)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (message);
    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);
    UNREFERENCED_PARAMETER (outRetval);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNcLButtonUp
//
//  Default behavior: pass through to DefWindowProc. Derived classes
//  that opt into custom system-button click routing (Min / Max /
//  Close) override to consume HTMINBUTTON / HTMAXBUTTON / HTCLOSE.
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




