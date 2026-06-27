#include "Pch.h"

#include "DialogPrimitive.h"
#include "../Chrome/ChromeTheme.h"


static constexpr LPCWSTR  s_kpszDialogClass       = L"Casso.Dialog.Primitive";
static constexpr DWORD    s_kDialogStyle           = WS_POPUP | WS_SYSMENU;
static constexpr DWORD    s_kDialogExStyle         = 0;
static constexpr float    s_kTitleHeightDp         = 32.0f;
static constexpr float    s_kCloseButtonWidthDp    = 46.0f;
static constexpr float    s_kBodyFontDp            = 13.0f;
static constexpr float    s_kButtonFontDp          = 13.0f;
static constexpr float    s_kMaxBodyWidthDp        = 360.0f;
static constexpr float    s_kButtonHeightDp        = 28.0f;
static constexpr float    s_kButtonPaddingDp       = 16.0f;
static constexpr float    s_kButtonSpacingDp       = 8.0f;
static constexpr float    s_kIconSizeDp            = 32.0f;
static constexpr float    s_kBodyLineHeightDp      = 22.0f;
static constexpr float    s_kOuterPaddingDp        = 16.0f;
static constexpr float    s_kIconBodyGapDp         = 12.0f;
static constexpr float    s_kBodyButtonsGapDp      = 16.0f;
static constexpr float    s_kMinButtonWidthDp      = 72.0f;
static constexpr int      s_kCenterDivisor         = 2;
static constexpr INT_PTR  s_kShellExecThreshold    = 32;
static constexpr LPCWSTR  s_kpszHyperlinkError     = L"Could not open the requested link.";
static constexpr LPCWSTR  s_kpszFont               = L"Segoe UI";





////////////////////////////////////////////////////////////////////////////////
//
//  ~DialogPrimitive
//
////////////////////////////////////////////////////////////////////////////////

DialogPrimitive::~DialogPrimitive()
{
    if (m_hwnd != nullptr)
    {
        DestroyWindow (m_hwnd);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterClass
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DialogPrimitive::RegisterClass (HINSTANCE hInstance)
{
    HRESULT     hr   = S_OK;
    WNDCLASSEXW wcex = { sizeof (wcex) };
    BOOL        ok   = FALSE;
    ATOM        atom = 0;



    CBRAEx (hInstance, E_INVALIDARG);

    m_hInstance = hInstance;

    ok = GetClassInfoExW (hInstance, s_kpszDialogClass, &wcex);
    BAIL_OUT_IF (ok, S_OK);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = DialogPrimitive::s_WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = nullptr;
    wcex.hCursor       = LoadCursorW (nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName  = nullptr;
    wcex.lpszClassName = s_kpszDialogClass;
    wcex.hIconSm       = nullptr;

    atom = RegisterClassExW (&wcex);
    CWRA (atom);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  Displays the dialog as a blocking modal call. Disables the owner
//  window for the duration, runs a private GetMessage loop, and
//  returns the resultCode of the button the user chose (or -1 on
//  window-close gesture with no isCancel button).
//
////////////////////////////////////////////////////////////////////////////////

int DialogPrimitive::Show (
    HWND                     hwndOwner,
    ID3D11Device           * device,
    ID3D11DeviceContext    * context,
    const ChromeTheme      * theme,
    const DialogDefinition & def)
{
    HRESULT  hr            = S_OK;
    RECT     windowRect    = {};
    HWND     hwndCreated   = nullptr;
    BOOL     ok            = FALSE;
    MSG      msg           = {};
    bool     ownerEnabled  = false;
    UINT     actualDpi     = 0;



    CBRAEx (hwndOwner, E_INVALIDARG);
    CBRAEx (device,    E_INVALIDARG);
    CBRAEx (context,   E_INVALIDARG);
    CBRAEx (theme,     E_INVALIDARG);
    CBRA   (m_hInstance);
    BAIL_OUT_IF (m_hwnd != nullptr, S_OK);

    m_hwndOwner     = hwndOwner;
    m_device        = device;
    m_context       = context;
    m_theme         = theme;
    m_def           = &def;
    m_chosenId      = -1;
    m_closed        = false;
    m_focusedButton = SIZE_MAX;

    m_dpi = GetDpiForWindow (hwndOwner);
    if (m_dpi == 0)
    {
        m_dpi = DxuiDpiScaler::kBaseDpi;
    }

    RecomputeLayout (m_dpi);
    BuildButtons    ();

    windowRect = GetInitialWindowRect (hwndOwner, m_dpi);

    hwndCreated = CreateWindowExW (s_kDialogExStyle,
                                   s_kpszDialogClass,
                                   def.title.c_str(),
                                   s_kDialogStyle,
                                   windowRect.left,
                                   windowRect.top,
                                   windowRect.right  - windowRect.left,
                                   windowRect.bottom - windowRect.top,
                                   hwndOwner,
                                   nullptr,
                                   m_hInstance,
                                   this);
    CWRA (hwndCreated);

    // The pre-creation layout used the owner window's DPI. At startup the
    // owner is the desktop window, which is system-DPI aware and reports
    // the system DPI -- not this dialog's per-monitor DPI. When the two
    // differ (e.g. a 96-DPI system baseline with a 150%/144-DPI monitor),
    // the body would be measured and wrapped at one scale while its text
    // renders at another, so each wrapped line's cell is too narrow for the
    // rendered glyphs; DWrite then re-wraps inside the cell and the lines
    // pile on top of each other. Now that the window exists and its real
    // per-monitor DPI is known, re-sync the layout and window size to it.
    actualDpi = GetDpiForWindow (hwndCreated);
    if (actualDpi != 0 && actualDpi != m_dpi)
    {
        m_dpi = actualDpi;
        RecomputeLayout (m_dpi);
        BuildButtons    ();

        windowRect = GetInitialWindowRect (hwndOwner, m_dpi);
        SetWindowPos (hwndCreated,
                      nullptr,
                      windowRect.left,
                      windowRect.top,
                      windowRect.right  - windowRect.left,
                      windowRect.bottom - windowRect.top,
                      SWP_NOZORDER | SWP_NOACTIVATE);
    }

    EnableWindow (hwndOwner, FALSE);
    ownerEnabled = true;

    ShowWindow  (hwndCreated, SW_SHOWNORMAL);
    UpdateWindow (hwndCreated);
    SetForegroundWindow (hwndCreated);
    SetFocus    (hwndCreated);

    RenderFrame();

    while (!m_closed)
    {
        BOOL gotMessage = GetMessageW (&msg, nullptr, 0, 0);

        if (gotMessage == 0)
        {
            PostQuitMessage ((int) msg.wParam);
            break;
        }

        if (gotMessage == -1)
        {
            break;
        }

        TranslateMessage (&msg);
        DispatchMessageW (&msg);
    }

Error:
    if (ownerEnabled)
    {
        EnableWindow (hwndOwner, TRUE);
    }

    if (m_hwnd != nullptr)
    {
        DestroyWindow (m_hwnd);
    }

    m_hwnd        = nullptr;
    m_hwndOwner   = nullptr;
    m_device      = nullptr;
    m_context     = nullptr;
    m_theme       = nullptr;
    m_def         = nullptr;
    m_buttons.clear();
    m_focusedButton    = SIZE_MAX;
    m_focusedHyperlink = SIZE_MAX;
    m_hoveredHyperlink = SIZE_MAX;

    return m_chosenId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Close
//
//  Records the chosen result code, signals the message loop to exit,
//  and posts WM_NULL to unblock GetMessageW if called from within
//  a button click handler (same thread, inside DispatchMessageW).
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::Close (int chosenId)
{
    m_chosenId = chosenId;
    m_closed   = true;

    if (m_hwnd != nullptr)
    {
        PostMessageW (m_hwnd, WM_NULL, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_WndProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK DialogPrimitive::s_WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    DialogPrimitive * window = nullptr;



    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW * cs = reinterpret_cast<CREATESTRUCTW *> (lParam);


        window = reinterpret_cast<DialogPrimitive *> (cs->lpCreateParams);
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (window));
    }
    else
    {
        window = reinterpret_cast<DialogPrimitive *> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));
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

LRESULT DialogPrimitive::WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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
            OnClose();
            result = 0;
            break;

        case WM_SIZE:
            OnSize ((int) LOWORD (lParam), (int) HIWORD (lParam));
            result = 0;
            break;

        case WM_PAINT:
            RenderFrame();
            ValidateRect (hwnd, nullptr);
            result = 0;
            break;

        case WM_DPICHANGED:
            OnDpiChanged (HIWORD (wParam), *reinterpret_cast<const RECT *> (lParam));
            result = 0;
            break;

        case WM_KEYDOWN:
            OnKeyDown (wParam);
            result = 0;
            break;

        case WM_SYSCHAR:
            if (OnSysChar (wParam))
            {
                result = 0;
                break;
            }
            result = DefWindowProcW (hwnd, message, wParam, lParam);
            break;

        case WM_CHAR:
            DispatchCustomBodyInput (DialogInputEvent::Kind::Char, 0, 0, (int) wParam);
            result = 0;
            break;

        case WM_MOUSEWHEEL:
            OnMouseWheel (wParam, lParam);
            result = 0;
            break;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            OnMouse (message, wParam, lParam);
            result = 0;
            break;

        case WM_SETCURSOR:
            if (LOWORD (lParam) == HTCLIENT)
            {
                POINT  pt = {};
                size_t hl = SIZE_MAX;

                if (GetCursorPos (&pt) && ScreenToClient (m_hwnd, &pt)
                    && HitTestHyperlink (pt.x, pt.y, hl))
                {
                    SetCursor (LoadCursorW (nullptr, IDC_HAND));
                    result = TRUE;
                    break;
                }
            }
            result = DefWindowProcW (hwnd, message, wParam, lParam);
            break;

        case WM_TIMER:
            if (m_def != nullptr && m_def->onTick)
            {
                m_def->onTick (*this);
            }
            result = 0;
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

HRESULT DialogPrimitive::OnCreate (HWND hwnd)
{
    HRESULT  hr  = S_OK;
    RECT     rc  = {};
    UINT     dpi = DxuiDpiScaler::kBaseDpi;
    BOOL     ok  = FALSE;



    m_hwnd = hwnd;

    ok = GetClientRect (m_hwnd, &rc);
    CWRA (ok);

    dpi = GetDpiForWindow (m_hwnd);
    if (dpi == 0)
    {
        dpi = DxuiDpiScaler::kBaseDpi;
    }

    hr = m_renderer.Initialize (m_hwnd,
                                m_device,
                                m_context,
                                rc.right  - rc.left,
                                rc.bottom - rc.top,
                                dpi);
    CHRA (hr);

    if (m_def != nullptr && m_def->tickIntervalMs > 0)
    {
        SetTimer (m_hwnd, 1, m_def->tickIntervalMs, nullptr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDestroy
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::OnDestroy()
{
    if (m_hwnd != nullptr)
    {
        KillTimer (m_hwnd, 1);
    }

    m_renderer.Shutdown();
    m_hwnd = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::OnSize (int widthPx, int heightPx)
{
    HRESULT  hr  = S_OK;
    UINT     dpi = DxuiDpiScaler::kBaseDpi;



    BAIL_OUT_IF (m_hwnd == nullptr || !m_renderer.IsInitialized(), S_OK);

    dpi = GetDpiForWindow (m_hwnd);
    if (dpi == 0)
    {
        dpi = DxuiDpiScaler::kBaseDpi;
    }

    hr = m_renderer.Resize (widthPx, heightPx, dpi);
    IGNORE_RETURN_VALUE (hr, S_OK);

    RenderFrame();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDpiChanged
//
//  Repositions and resizes the window per the system-suggested rect,
//  recomputes the layout at the new DPI, rebuilds buttons, and resizes
//  the renderer.
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::OnDpiChanged (UINT dpi, const RECT & suggestedRect)
{
    HRESULT  hr = S_OK;
    BOOL     ok = FALSE;



    CBRA (m_hwnd);

    m_dpi = dpi;

    ok = SetWindowPos (m_hwnd,
                       nullptr,
                       suggestedRect.left,
                       suggestedRect.top,
                       suggestedRect.right  - suggestedRect.left,
                       suggestedRect.bottom - suggestedRect.top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    CWRA (ok);

    RecomputeLayout (dpi);
    BuildButtons    ();

    hr = m_renderer.Resize (suggestedRect.right  - suggestedRect.left,
                            suggestedRect.bottom - suggestedRect.top,
                            dpi);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::OnKeyDown (WPARAM vk)
{
    if (DispatchCustomBodyInput (DialogInputEvent::Kind::KeyDown, 0, 0, (int) vk))
    {
        return;
    }

    switch (vk)
    {
        case VK_RETURN:
            if (m_focusedHyperlink != SIZE_MAX)
            {
                LaunchHyperlink (m_focusedHyperlink);
            }
            else if (m_focusedButton < m_buttons.size())
            {
                m_buttons[m_focusedButton].Click();
            }
            else
            {
                ActivateDefaultButton();
            }
            break;

        case VK_ESCAPE:
            if (m_def != nullptr && m_def->closeBoxResult.has_value())
            {
                Close (m_def->closeBoxResult.value());
            }
            else
            {
                ActivateCancelButton();
            }
            break;

        case VK_TAB:
        {
            int  delta = (GetKeyState (VK_SHIFT) & 0x8000) ? -1 : 1;
            CycleFocus (delta);
            break;
        }

        case VK_SPACE:
            if (m_focusedHyperlink != SIZE_MAX)
            {
                LaunchHyperlink (m_focusedHyperlink);
            }
            else if (m_focusedButton < m_buttons.size())
            {
                m_buttons[m_focusedButton].Click();
            }
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSysChar
//
//  Alt+letter accelerator dispatch. Buttons strip a single `&` from
//  their label and remember the following character as their
//  accelerator. Returns true if the keystroke matched a button.
//
////////////////////////////////////////////////////////////////////////////////

bool DialogPrimitive::OnSysChar (WPARAM ch)
{
    wchar_t  key      = (wchar_t) towlower ((wint_t) ch);
    bool     consumed = false;


    for (size_t i = 0; key != 0 && i < m_buttons.size(); ++i)
    {
        if (!m_buttons[i].Visible() || !m_buttons[i].Enabled())
        {
            continue;
        }
        if (m_buttons[i].Accelerator() == key)
        {
            ActivateButton (i);
            consumed = true;
            break;
        }
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::OnMouse (UINT message, WPARAM wParam, LPARAM lParam)
{
    HRESULT                 hr          = S_OK;
    int                     xPx         = (int) (short) LOWORD (lParam);
    int                     yPx         = (int) (short) HIWORD (lParam);
    bool                    down        = (message == WM_LBUTTONDOWN);
    bool                    up          = (message == WM_LBUTTONUP);
    bool                    dirty       = false;
    bool                    handled     = false;
    bool                    inClose     = false;
    bool                    newHovered  = false;
    bool                    newPressed  = false;
    bool                    captionDrag = false;
    bool                    closeClick  = false;
    size_t                  hlRunIdx    = SIZE_MAX;
    size_t                  newHover    = SIZE_MAX;
    DialogInputEvent::Kind  kind        = DialogInputEvent::Kind::MouseMove;



    UNREFERENCED_PARAMETER (wParam);

    if      (message == WM_LBUTTONDOWN) { kind = DialogInputEvent::Kind::LeftButtonDown; }
    else if (message == WM_LBUTTONUP)   { kind = DialogInputEvent::Kind::LeftButtonUp;   }

    handled = DispatchCustomBodyInput (kind, xPx, yPx, 0);
    BAIL_OUT_IF (handled, S_OK);

    inClose    = IsPointInCloseButton (xPx, yPx);
    newHovered = inClose;
    newPressed = down ? inClose : (up ? false : m_closePressed);

    // A press in the title bar outside the close button starts a system
    // move; nothing else to do for this message.
    captionDrag = down && !inClose && (yPx < GetTitleHeightPx());
    if (captionDrag)
    {
        ReleaseCapture();
        SendMessageW (m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
    }
    BAIL_OUT_IF (captionDrag, S_OK);

    // Releasing inside the close button after pressing it closes the dialog.
    closeClick = up && m_closePressed && inClose;
    if (closeClick)
    {
        OnClose();
    }
    BAIL_OUT_IF (closeClick, S_OK);

    if (newHovered != m_closeHovered || newPressed != m_closePressed)
    {
        m_closeHovered = newHovered;
        m_closePressed = newPressed;
        dirty          = true;
    }

    for (size_t i = 0; i < m_buttons.size(); ++i)
    {
        bool wasHover = m_buttons[i].Focused();
        m_buttons[i].SetMouse (xPx, yPx, down);
        dirty = dirty || (m_buttons[i].Focused() != wasHover);
    }

    if (HitTestHyperlink (xPx, yPx, hlRunIdx))
    {
        newHover = hlRunIdx;
    }

    if (newHover != m_hoveredHyperlink)
    {
        m_hoveredHyperlink = newHover;
        dirty              = true;
    }

    if (up)
    {
        for (size_t i = 0; i < m_buttons.size(); ++i)
        {
            if (m_buttons[i].HitTest (xPx, yPx))
            {
                ActivateButton (i);
                handled = true;
                break;
            }
        }

        if (!handled && HitTestHyperlink (xPx, yPx, hlRunIdx))
        {
            LaunchHyperlink (hlRunIdx);
        }
    }
    BAIL_OUT_IF (handled, S_OK);

    if (dirty || message == WM_MOUSEMOVE)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseWheel
//
//  Forwards a mouse-wheel notch to the custom body (the only consumer
//  that scrolls). WM_MOUSEWHEEL reports the pointer in screen space, so
//  translate it to client coordinates the way the rest of the input path
//  expects before dispatching.
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::OnMouseWheel (WPARAM wParam, LPARAM lParam)
{
    POINT  pt    = { (int) (short) LOWORD (lParam), (int) (short) HIWORD (lParam) };
    int    delta = GET_WHEEL_DELTA_WPARAM (wParam);



    ScreenToClient (m_hwnd, &pt);
    DispatchCustomBodyInput (DialogInputEvent::Kind::Wheel, pt.x, pt.y, delta);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnClose
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::OnClose()
{
    if (m_def != nullptr && m_def->closeBoxResult.has_value())
    {
        Close (m_def->closeBoxResult.value());
        return;
    }

    ActivateCancelButton();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildButtons
//
//  (Re)builds the DxuiButton widget vector from the current definition and
//  layout, setting colors, labels, DPI, click callbacks, and focus.
//  Must be called after RecomputeLayout() and whenever DPI changes.
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::BuildButtons()
{
    HRESULT  hr      = S_OK;
    size_t   count   = 0;



    CBR (m_def != nullptr);

    count = std::min (m_def->buttons.size(), m_layout.buttonRectsPx.size());

    m_buttons.clear();
    m_buttons.resize (count);

    for (size_t i = 0; i < count; ++i)
    {
        const DialogButton & btn    = m_def->buttons[i];
        RECT                 rect   = m_layout.buttonRectsPx[i];
        int                  titleH = GetTitleHeightPx();

        rect.top    += titleH;
        rect.bottom += titleH;

        m_buttons[i].Layout   (rect);
        m_buttons[i].SetLabel (btn.label);
        m_buttons[i].SetDpi   (m_dpi);

        if (btn.isDefault)
        {
            m_buttons[i].SetEmphasis (true);
        }

        m_buttons[i].SetClick ([this, i]() { ActivateButton (i); });
    }

    if (GetCustomBodyFocusCount() > 0)
    {
        // A custom body with focusable stops (e.g. the disk picker's
        // search box + list) takes initial focus instead of a button so
        // the user can type to filter immediately; this also keeps every
        // button unfocused so a typed Space reaches the search box.
        SetCustomBodyFocus (0);
    }
    else
    {
        m_focusedButton = GetDefaultButtonIdx();
        if (m_focusedButton == SIZE_MAX && !m_buttons.empty())
        {
            m_focusedButton = 0;
        }

        if (m_focusedButton < m_buttons.size())
        {
            m_buttons[m_focusedButton].SetFocused (true);
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureLabelWidthPx
//
//  Measures the pixel width of `sv` at `fontPx` using the supplied
//  measurement renderer. Shared by the body-text-run and button-label
//  measurement hooks, which differ only in font size.
//
////////////////////////////////////////////////////////////////////////////////

float DialogPrimitive::MeasureLabelWidthPx (DxuiTextRenderer & measurer, std::wstring_view sv, float fontPx)
{
    std::wstring  text (sv);
    float         w = 0.0f;
    float         h = 0.0f;



    measurer.MeasureString (text.c_str(), fontPx, s_kpszFont, w, h);
    return w;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecomputeLayout
//
//  Creates a temporary DxuiTextRenderer for measurement-only calls
//  (no swap chain needed), builds the DialogLayoutMetrics, and stores
//  the result in m_layout.
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::RecomputeLayout (UINT dpi)
{
    HRESULT             hr       = S_OK;
    DxuiTextRenderer    measurer;
    DialogLayoutMetrics metrics;
    float               dpiScale = 0.0f;



    CBR (m_def    != nullptr);
    CBR (m_device != nullptr);

    dpiScale = static_cast<float> (dpi) / static_cast<float> (DxuiDpiScaler::kBaseDpi);

    hr = measurer.Initialize (m_device);
    CHRA (hr);

    metrics.dpiScale         = dpiScale;
    metrics.maxBodyWidthPx   = s_kMaxBodyWidthDp   * dpiScale;
    metrics.buttonHeightPx   = s_kButtonHeightDp   * dpiScale;
    metrics.buttonPaddingPx  = s_kButtonPaddingDp  * dpiScale;
    metrics.buttonSpacingPx  = s_kButtonSpacingDp  * dpiScale;
    metrics.iconSizePx       = ((m_def != nullptr && m_def->iconSizeOverrideDp > 0.0f)
                                ? m_def->iconSizeOverrideDp
                                : s_kIconSizeDp) * dpiScale;
    metrics.bodyLineHeightPx = s_kBodyLineHeightDp * dpiScale;
    metrics.outerPaddingPx   = s_kOuterPaddingDp   * dpiScale;
    metrics.iconBodyGapPx    = s_kIconBodyGapDp    * dpiScale;
    metrics.bodyButtonsGapPx = s_kBodyButtonsGapDp * dpiScale;
    metrics.minButtonWidthPx = s_kMinButtonWidthDp * dpiScale;

    metrics.measureBodyTextRun = [&measurer, dpiScale] (std::wstring_view sv) -> float
    {
        return MeasureLabelWidthPx (measurer, sv, s_kBodyFontDp * dpiScale);
    };

    metrics.measureButtonLabel = [&measurer, dpiScale] (std::wstring_view sv) -> float
    {
        return MeasureLabelWidthPx (measurer, sv, s_kButtonFontDp * dpiScale);
    };

    if (m_def->onMeasureCustomBody)
    {
        SIZE measured = m_def->onMeasureCustomBody (measurer, dpiScale);
        metrics.customBodyOverridePx = measured;
    }

    m_layout = DialogLayout::Compute (*m_def, metrics);

Error:
    measurer.Shutdown();
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFrame
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::RenderFrame()
{
    HRESULT  hr = S_OK;



    CBR (m_def  != nullptr);
    CBR (m_theme != nullptr);
    CBR (m_renderer.IsInitialized());

    IGNORE_RETURN_VALUE (hr, m_renderer.Render (*m_def, m_layout, *m_theme,
                                                GetTitleHeightPx(), m_buttons,
                                                m_focusedHyperlink,
                                                m_hoveredHyperlink,
                                                m_closeHovered,
                                                m_closePressed));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActivateButton
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::ActivateButton (size_t idx)
{
    HRESULT  hr           = S_OK;
    bool     shouldClose  = true;
    int      resultCode   = 0;



    CBR (idx < m_def->buttons.size());
    BAIL_OUT_IF (idx < m_buttons.size() && (!m_buttons[idx].Enabled() || !m_buttons[idx].Visible()), S_OK);

    resultCode = m_def->buttons[idx].resultCode;

    if (m_def->onButtonActivated)
    {
        shouldClose = m_def->onButtonActivated (idx, *this);
    }

    if (shouldClose)
    {
        Close (resultCode);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetButtonLabel
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::SetButtonLabel (size_t idx, const std::wstring & label)
{
    if (idx < m_buttons.size())
    {
        m_buttons[idx].SetLabel (label);
        RecomputeLayout (m_dpi);
        Repaint();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetButtonEnabled
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::SetButtonEnabled (size_t idx, bool enabled)
{
    if (idx < m_buttons.size())
    {
        m_buttons[idx].SetEnabled (enabled);

        if (!enabled && m_focusedButton == idx)
        {
            CycleFocus (1);
        }

        Repaint();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetButtonVisible
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::SetButtonVisible (size_t idx, bool visible)
{
    if (idx < m_buttons.size())
    {
        m_buttons[idx].SetVisible (visible);

        if (!visible && m_focusedButton == idx)
        {
            CycleFocus (1);
        }

        Repaint();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Repaint
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::Repaint ()
{
    if (m_hwnd != nullptr)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActivateDefaultButton
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::ActivateDefaultButton()
{
    size_t  idx = GetDefaultButtonIdx();

    if (idx != SIZE_MAX)
    {
        ActivateButton (idx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActivateCancelButton
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::ActivateCancelButton()
{
    size_t  idx = GetCancelButtonIdx();

    if (idx != SIZE_MAX)
    {
        ActivateButton (idx);
    }
    else
    {
        Close (-1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CycleFocus
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::CycleFocus (int delta)
{
    HRESULT  hr                 = S_OK;
    int      cbN                = GetCustomBodyFocusCount();
    size_t   buttonN            = m_buttons.size();
    size_t   linkN              = GetHyperlinkCount();
    size_t   total              = (size_t) cbN + buttonN + linkN;
    size_t   cur                = 0;
    size_t   next               = 0;
    bool     hadCustomBodyFocus = (m_focusedCustomBody >= 0);



    CBR (total != 0);

    // Unified Tab ring:
    //   [0 .. cbN)               custom-body stops (e.g. search box, list)
    //   [cbN .. cbN+buttonN)     action buttons
    //   [cbN+buttonN .. total)   body hyperlinks
    if (m_focusedCustomBody >= 0)
    {
        cur = (size_t) m_focusedCustomBody;
    }
    else if (m_focusedButton < buttonN)
    {
        cur = (size_t) cbN + m_focusedButton;
    }
    else if (m_focusedHyperlink != SIZE_MAX)
    {
        size_t  hlOrdinal = 0;

        for (size_t bi = 0; bi < m_def->body.size() && bi < m_focusedHyperlink; bi++)
        {
            if (m_def->body[bi].isHyperlink)
            {
                hlOrdinal++;
            }
        }
        cur = (size_t) cbN + buttonN + hlOrdinal;
    }
    else
    {
        cur = (delta > 0) ? (total - 1) : 0;
    }

    if (delta > 0)
    {
        next = (cur + 1) % total;
    }
    else
    {
        next = (cur == 0) ? (total - 1) : (cur - 1);
    }

    // Skip past hidden/disabled buttons (custom-body + hyperlink stops are
    // always focusable). Bail after `total` attempts so we never spin.
    for (size_t guard = 0; guard < total; guard++)
    {
        bool  isButton = (next >= (size_t) cbN) && (next < (size_t) cbN + buttonN);

        if (!isButton)
        {
            break;
        }

        if (m_buttons[next - (size_t) cbN].Visible() && m_buttons[next - (size_t) cbN].Enabled())
        {
            break;
        }

        next = (delta > 0) ? ((next + 1) % total)
                           : (next == 0 ? total - 1 : next - 1);
    }

    if (m_focusedButton < buttonN)
    {
        m_buttons[m_focusedButton].SetFocused (false);
    }

    m_focusedButton     = SIZE_MAX;
    m_focusedHyperlink  = SIZE_MAX;
    m_focusedCustomBody = -1;

    if (next < (size_t) cbN)
    {
        m_focusedCustomBody = (int) next;
    }
    else if (next < (size_t) cbN + buttonN)
    {
        m_focusedButton = next - (size_t) cbN;
        m_buttons[m_focusedButton].SetFocused (true);
    }
    else
    {
        m_focusedHyperlink = GetNthHyperlinkBodyIdx (next - (size_t) cbN - buttonN);
    }

    if (m_def != nullptr && m_def->onCustomBodyFocusChanged)
    {
        if (m_focusedCustomBody >= 0)
        {
            m_def->onCustomBodyFocusChanged (m_focusedCustomBody);
        }
        else if (hadCustomBodyFocus)
        {
            m_def->onCustomBodyFocusChanged (-1);
        }
    }

    InvalidateRect (m_hwnd, nullptr, FALSE);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCustomBodyFocusCount
//
//  Returns the number of keyboard-focus stops the custom body exposes
//  (DialogDefinition::customBodyFocusableCount), or 0 when the dialog has
//  no focusable custom body. The Tab ring places these stops ahead of the
//  action buttons.
//
////////////////////////////////////////////////////////////////////////////////

int DialogPrimitive::GetCustomBodyFocusCount () const
{
    return (m_def != nullptr && m_def->customBodyFocusableCount > 0) ? m_def->customBodyFocusableCount : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCustomBodyFocus
//
//  Moves keyboard focus onto custom-body stop `idx` (0-based), or clears
//  it with -1, defocusing any button / hyperlink. The custom body calls
//  this from its input hook when a click lands on one of its focusable
//  sub-widgets so the dialog's Tab ring and the body stay in sync, and
//  the dialog calls it to give a focusable custom body initial focus.
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::SetCustomBodyFocus (int idx)
{
    if (m_focusedButton < m_buttons.size())
    {
        m_buttons[m_focusedButton].SetFocused (false);
    }

    m_focusedButton     = SIZE_MAX;
    m_focusedHyperlink  = SIZE_MAX;
    m_focusedCustomBody = idx;

    if (m_def != nullptr && m_def->onCustomBodyFocusChanged)
    {
        m_def->onCustomBodyFocusChanged (idx);
    }

    InvalidateRect (m_hwnd, nullptr, FALSE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHyperlinkCount
//
//  Returns the number of hyperlink runs in the dialog body (the runs the
//  Tab ring and mouse hit-testing treat as focusable links).
//
////////////////////////////////////////////////////////////////////////////////

size_t DialogPrimitive::GetHyperlinkCount() const
{
    size_t  count = 0;


    if (m_def != nullptr)
    {
        for (const DialogTextRun & run : m_def->body)
        {
            if (run.isHyperlink)
            {
                count++;
            }
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNthHyperlinkBodyIdx
//
//  Maps a zero-based hyperlink ordinal to its index within the body-run
//  list (body runs interleave plain text and hyperlinks). Returns
//  SIZE_MAX when `hyperlinkIdx` is out of range.
//
////////////////////////////////////////////////////////////////////////////////

size_t DialogPrimitive::GetNthHyperlinkBodyIdx (size_t hyperlinkIdx) const
{
    size_t  seen   = 0;
    size_t  result = SIZE_MAX;


    for (size_t bi = 0; m_def != nullptr && bi < m_def->body.size(); bi++)
    {
        if (!m_def->body[bi].isHyperlink)
        {
            continue;
        }

        if (seen == hyperlinkIdx)
        {
            result = bi;
            break;
        }

        seen++;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchCustomBodyInput
//
//  When the definition has custom-body hooks, forwards the input
//  event in window-relative coordinates (translated into custom-body
//  coordinates) and closes the dialog with the returned result code
//  if the hook requests it. Returns true when the event was consumed
//  so the default button/hyperlink dispatch is suppressed.
//
////////////////////////////////////////////////////////////////////////////////

bool DialogPrimitive::DispatchCustomBodyInput (DialogInputEvent::Kind kind, int xPx, int yPx, int intArg)
{
    HRESULT             hr       = S_OK;
    bool                result   = false;
    DialogInputEvent    ev       = {};
    std::optional<int>  req;
    int                 titleH   = GetTitleHeightPx();
    RECT                rc       = m_layout.customBodyRectPx;
    bool                consumed = false;
    bool                isKey    = (kind == DialogInputEvent::Kind::KeyDown || kind == DialogInputEvent::Kind::Char);



    // Precondition: input only arrives while the dialog is showing.
    CBRAEx (m_def != nullptr, E_UNEXPECTED);

    // A dialog with no custom-body input hook simply has nothing to
    // forward -- a normal case, not an error.
    BAIL_OUT_IF (!m_def->onInputCustomBody, S_OK);

    rc.top    += titleH;
    rc.bottom += titleH;

    if (isKey)
    {
        ev.kind = kind;
        if (kind == DialogInputEvent::Kind::KeyDown)
        {
            ev.vkCode = intArg;
        }
        else
        {
            ev.ch = (wchar_t) intArg;
        }
        req = m_def->onInputCustomBody (ev);
    }
    else
    {
        BAIL_OUT_IF (xPx < rc.left || xPx >= rc.right || yPx < rc.top || yPx >= rc.bottom, S_OK);

        ev.kind = kind;
        ev.xPx  = xPx - rc.left;
        ev.yPx  = yPx - rc.top;
        if (kind == DialogInputEvent::Kind::Wheel)
        {
            ev.wheelDelta = intArg;
        }
        req      = m_def->onInputCustomBody (ev);
        consumed = true;
    }

    if (req.has_value())
    {
        Close (req.value());
        result = true;
    }
    else
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
        result = consumed;
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHyperlink
//
//  Returns true when (xPx, yPx) falls inside any hyperlink hit rect
//  from the layout, and sets outBodyRunIdx to the corresponding body
//  run index. The hit rects are window-relative (title height already
//  added by BuildButtons callers; here we add it to the raw layout
//  rects for the same reason).
//
////////////////////////////////////////////////////////////////////////////////

bool DialogPrimitive::HitTestHyperlink (int xPx, int yPx, size_t & outBodyRunIdx) const
{
    int     titleH = GetTitleHeightPx();
    size_t  hlIdx  = 0;
    bool    hit    = false;


    outBodyRunIdx = SIZE_MAX;

    for (size_t i = 0; !hit && m_def != nullptr && i < m_def->body.size(); ++i)
    {
        if (!m_def->body[i].isHyperlink)
        {
            continue;
        }

        if (hlIdx >= m_layout.hyperlinkHitRectsPx.size())
        {
            break;
        }

        const RECT & rect = m_layout.hyperlinkHitRectsPx[hlIdx];

        int  adjLeft   = rect.left;
        int  adjTop    = rect.top    + titleH;
        int  adjRight  = rect.right;
        int  adjBottom = rect.bottom + titleH;

        if (xPx >= adjLeft && xPx < adjRight && yPx >= adjTop && yPx < adjBottom)
        {
            outBodyRunIdx = i;
            hit           = true;
        }

        ++hlIdx;
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchHyperlink
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::LaunchHyperlink (size_t bodyRunIdx)
{
    HRESULT  hr       = S_OK;
    INT_PTR  shellRes = 0;
    bool     shellOk  = false;



    CBRA (m_def != nullptr);
    CBRA (bodyRunIdx < m_def->body.size());
    CBRA (m_def->body[bodyRunIdx].isHyperlink);

    shellRes = (INT_PTR) ShellExecuteW (nullptr, L"open",
                                        m_def->body[bodyRunIdx].hyperlinkUrl.c_str(),
                                        nullptr, nullptr, SW_SHOWNORMAL);
    shellOk  = (shellRes > s_kShellExecThreshold);

    hr = shellOk ? S_OK : E_FAIL;
    CHRN (hr, s_kpszHyperlinkError);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTitleHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DialogPrimitive::GetTitleHeightPx() const
{
    return static_cast<int> (s_kTitleHeightDp * static_cast<float> (m_dpi) / static_cast<float> (DxuiDpiScaler::kBaseDpi));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCloseButtonRectPx
//
//  Returns the close caption widget rect in client coordinates. Sized
//  46dp wide x titleH tall, right-aligned in the title bar.
//
////////////////////////////////////////////////////////////////////////////////

RECT DialogPrimitive::GetCloseButtonRectPx() const
{
    RECT  rect    = {};
    int   widthPx = static_cast<int> (s_kCloseButtonWidthDp * static_cast<float> (m_dpi) / static_cast<float> (DxuiDpiScaler::kBaseDpi));
    int   clientW = 0;
    RECT  clientRect = {};



    if (m_hwnd != nullptr && GetClientRect (m_hwnd, &clientRect))
    {
        clientW = clientRect.right - clientRect.left;
    }

    rect.right  = clientW;
    rect.left   = clientW - widthPx;
    rect.top    = 0;
    rect.bottom = GetTitleHeightPx();

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPointInCloseButton
//
////////////////////////////////////////////////////////////////////////////////

bool DialogPrimitive::IsPointInCloseButton (int xPx, int yPx) const
{
    RECT  rect = GetCloseButtonRectPx();



    return (xPx >= rect.left) && (xPx < rect.right)
        && (yPx >= rect.top)  && (yPx < rect.bottom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetInitialWindowRect
//
//  Centers the dialog over the owner window, then clamps to the
//  monitor's work area.
//
////////////////////////////////////////////////////////////////////////////////

RECT DialogPrimitive::GetInitialWindowRect (HWND hwndOwner, UINT dpi) const
{
    int          titleH   = GetTitleHeightPx();
    int          clientW  = m_layout.totalSizePx.cx;
    int          clientH  = m_layout.totalSizePx.cy + titleH;
    RECT         ownerRect = {};
    RECT         workRect  = {};
    MONITORINFO  mi        = { sizeof (mi) };
    HMONITOR     monitor   = nullptr;
    int          x         = 0;
    int          y         = 0;



    if (!GetWindowRect (hwndOwner, &ownerRect))
    {
        ownerRect = { 0, 0, clientW, clientH };
    }

    monitor = MonitorFromWindow (hwndOwner, MONITOR_DEFAULTTONEAREST);
    if (monitor != nullptr && GetMonitorInfoW (monitor, &mi))
    {
        workRect = mi.rcWork;
    }
    else
    {
        workRect = ownerRect;
    }

    x = (int) ownerRect.left + ((int) (ownerRect.right  - ownerRect.left) - clientW) / s_kCenterDivisor;
    y = (int) ownerRect.top  + ((int) (ownerRect.bottom - ownerRect.top)  - clientH) / s_kCenterDivisor;

    x = std::max ((int) workRect.left, std::min (x, (int) workRect.right  - clientW));
    y = std::max ((int) workRect.top,  std::min (y, (int) workRect.bottom - clientH));

    return { x, y, x + clientW, y + clientH };
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDefaultButtonIdx
//
////////////////////////////////////////////////////////////////////////////////

size_t DialogPrimitive::GetDefaultButtonIdx() const
{
    size_t  result = SIZE_MAX;


    for (size_t i = 0; m_def != nullptr && i < m_def->buttons.size(); ++i)
    {
        if (m_def->buttons[i].isDefault)
        {
            result = i;
            break;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCancelButtonIdx
//
////////////////////////////////////////////////////////////////////////////////

size_t DialogPrimitive::GetCancelButtonIdx() const
{
    size_t  result = SIZE_MAX;


    for (size_t i = 0; m_def != nullptr && i < m_def->buttons.size(); ++i)
    {
        if (m_def->buttons[i].isCancel)
        {
            result = i;
            break;
        }
    }

    return result;
}
