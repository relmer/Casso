#include "Pch.h"

#include "Window.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Window
//
////////////////////////////////////////////////////////////////////////////////

Window::Window ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~Window
//
////////////////////////////////////////////////////////////////////////////////

Window::~Window ()
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
    CPRA (m_hwnd);

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

    switch (message)
    {
        case WM_CHAR:
            callDefWndProc = pThis->OnChar (wParam, lParam);
            break;

        case WM_COMMAND:
            callDefWndProc = pThis->OnCommand (hwnd, LOWORD (wParam));
            break;

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

        case WM_NOTIFY:
            callDefWndProc = pThis->OnNotify (hwnd, wParam, lParam);
            break;

        case WM_PAINT:
            callDefWndProc = pThis->OnPaint (hwnd);
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




