#include "Pch.h"

#include "DialogPrimitive.h"
#include "../Chrome/ChromeTheme.h"


static constexpr LPCWSTR  s_kpszDialogClass       = L"Casso.Dialog.Primitive";
static constexpr DWORD    s_kDialogStyle           = WS_POPUP | WS_SYSMENU;
static constexpr DWORD    s_kDialogExStyle         = 0;
static constexpr float    s_kTitleHeightDp         = 32.0f;
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
static constexpr float    s_kOutlineThicknessDp    = 1.0f;
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
        m_dpi = DpiScaler::kBaseDpi;
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

        case WM_CHAR:
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
    UINT     dpi = DpiScaler::kBaseDpi;
    BOOL     ok  = FALSE;



    m_hwnd = hwnd;

    ok = GetClientRect (m_hwnd, &rc);
    CWRA (ok);

    dpi = GetDpiForWindow (m_hwnd);
    if (dpi == 0)
    {
        dpi = DpiScaler::kBaseDpi;
    }

    hr = m_renderer.Initialize (m_hwnd,
                                m_device,
                                m_context,
                                rc.right  - rc.left,
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

void DialogPrimitive::OnDestroy()
{
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
    UINT     dpi = DpiScaler::kBaseDpi;



    BAIL_OUT_IF (m_hwnd == nullptr || !m_renderer.IsInitialized(), S_OK);

    dpi = GetDpiForWindow (m_hwnd);
    if (dpi == 0)
    {
        dpi = DpiScaler::kBaseDpi;
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
            ActivateCancelButton();
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
//  OnMouse
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::OnMouse (UINT message, WPARAM wParam, LPARAM lParam)
{
    int      xPx       = (int) (short) LOWORD (lParam);
    int      yPx       = (int) (short) HIWORD (lParam);
    bool     down      = (message == WM_LBUTTONDOWN);
    bool     up        = (message == WM_LBUTTONUP);
    bool     dirty     = false;
    size_t   hitIdx    = SIZE_MAX;
    size_t   hlRunIdx  = SIZE_MAX;
    size_t   newHover  = SIZE_MAX;
    DialogInputEvent::Kind  kind = DialogInputEvent::Kind::MouseMove;

    UNREFERENCED_PARAMETER (wParam);

    if      (message == WM_LBUTTONDOWN) { kind = DialogInputEvent::Kind::LeftButtonDown; }
    else if (message == WM_LBUTTONUP)   { kind = DialogInputEvent::Kind::LeftButtonUp;   }

    if (DispatchCustomBodyInput (kind, xPx, yPx, 0))
    {
        return;
    }

    for (size_t i = 0; i < m_buttons.size(); ++i)
    {
        bool wasHover   = m_buttons[i].Focused();
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
        dirty = true;
    }

    if (up)
    {
        for (size_t i = 0; i < m_buttons.size(); ++i)
        {
            if (m_buttons[i].HitTest (xPx, yPx))
            {
                hitIdx = i;
                break;
            }
        }

        if (hitIdx != SIZE_MAX)
        {
            ActivateButton (hitIdx);
            return;
        }

        if (HitTestHyperlink (xPx, yPx, hlRunIdx))
        {
            LaunchHyperlink (hlRunIdx);
        }
    }

    if (dirty || message == WM_MOUSEMOVE)
    {
        InvalidateRect (m_hwnd, nullptr, FALSE);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnClose
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::OnClose()
{
    ActivateCancelButton();
}




////////////////////////////////////////////////////////////////////////////////
//
//  BuildButtons
//
//  (Re)builds the Button widget vector from the current definition and
//  layout, setting colors, labels, DPI, click callbacks, and focus.
//  Must be called after RecomputeLayout() and whenever DPI changes.
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::BuildButtons()
{
    HRESULT  hr      = S_OK;
    size_t   count   = 0;
    float    outlinePx = 0.0f;



    CBR (m_def != nullptr);

    count     = std::min (m_def->buttons.size(), m_layout.buttonRectsPx.size());
    outlinePx = static_cast<float> (m_dpi) / static_cast<float> (DpiScaler::kBaseDpi) * s_kOutlineThicknessDp;

    m_buttons.clear();
    m_buttons.resize (count);

    for (size_t i = 0; i < count; ++i)
    {
        const DialogButton & btn    = m_def->buttons[i];
        RECT                 rect   = m_layout.buttonRectsPx[i];
        int                  titleH = TitleHeightPx();

        rect.top    += titleH;
        rect.bottom += titleH;

        m_buttons[i].Layout       (rect);
        m_buttons[i].SetLabel     (btn.label);
        m_buttons[i].SetDpi       (m_dpi);
        m_buttons[i].SetColors    (m_theme != nullptr ? m_theme->navStripArgb       : 0xFF202020,
                                   m_theme != nullptr ? m_theme->dropdownHoverArgb  : 0xFF3D6FB5,
                                   m_theme != nullptr ? m_theme->navHoverArgb       : 0xFF2D4058);
        m_buttons[i].SetTextColor (m_theme != nullptr ? m_theme->navItemTextArgb    : 0xFFF0F0F0);

        if (btn.isDefault)
        {
            m_buttons[i].SetOutline (outlinePx, m_theme != nullptr ? m_theme->navHoverArgb : 0xFF3D6FB5);
        }

        m_buttons[i].SetClick ([this, i]() { ActivateButton (i); });
    }

    m_focusedButton = DefaultButtonIdx();
    if (m_focusedButton == SIZE_MAX && !m_buttons.empty())
    {
        m_focusedButton = 0;
    }

    if (m_focusedButton < m_buttons.size())
    {
        m_buttons[m_focusedButton].SetFocused (true);
    }

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  RecomputeLayout
//
//  Creates a temporary DwriteTextRenderer for measurement-only calls
//  (no swap chain needed), builds the DialogLayoutMetrics, and stores
//  the result in m_layout.
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::RecomputeLayout (UINT dpi)
{
    HRESULT             hr       = S_OK;
    DwriteTextRenderer  measurer;
    DialogLayoutMetrics metrics;
    float               dpiScale = 0.0f;



    CBR (m_def    != nullptr);
    CBR (m_device != nullptr);

    dpiScale = static_cast<float> (dpi) / static_cast<float> (DpiScaler::kBaseDpi);

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
        std::wstring  text (sv);
        float         w = 0.0f;
        float         h = 0.0f;

        measurer.MeasureString (text.c_str(), s_kBodyFontDp * dpiScale, s_kpszFont, w, h);
        return w;
    };

    metrics.measureButtonLabel = [&measurer, dpiScale] (std::wstring_view sv) -> float
    {
        std::wstring  text (sv);
        float         w = 0.0f;
        float         h = 0.0f;

        measurer.MeasureString (text.c_str(), s_kButtonFontDp * dpiScale, s_kpszFont, w, h);
        return w;
    };

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
                                                TitleHeightPx(), m_buttons,
                                                m_focusedHyperlink,
                                                m_hoveredHyperlink));

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
    HRESULT  hr = S_OK;



    CBR (idx < m_def->buttons.size());

    Close (m_def->buttons[idx].resultCode);

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ActivateDefaultButton
//
////////////////////////////////////////////////////////////////////////////////

void DialogPrimitive::ActivateDefaultButton()
{
    size_t  idx = DefaultButtonIdx();

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
    size_t  idx = CancelButtonIdx();

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
    HRESULT  hr        = S_OK;
    size_t   buttonN   = m_buttons.size();
    size_t   linkN     = HyperlinkCount();
    size_t   total     = buttonN + linkN;
    size_t   cur       = 0;
    size_t   next      = 0;



    CBR (total != 0);

    if (m_focusedHyperlink != SIZE_MAX)
    {
        size_t  hlOrdinal = 0;

        for (size_t bi = 0; bi < m_def->body.size() && bi < m_focusedHyperlink; bi++)
        {
            if (m_def->body[bi].isHyperlink)
            {
                hlOrdinal++;
            }
        }
        cur = buttonN + hlOrdinal;
    }
    else if (m_focusedButton < buttonN)
    {
        cur = m_focusedButton;
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

    if (m_focusedButton < buttonN)
    {
        m_buttons[m_focusedButton].SetFocused (false);
    }

    m_focusedButton    = SIZE_MAX;
    m_focusedHyperlink = SIZE_MAX;

    if (next < buttonN)
    {
        m_focusedButton = next;
        m_buttons[next].SetFocused (true);
    }
    else
    {
        m_focusedHyperlink = NthHyperlinkBodyIdx (next - buttonN);
    }

    InvalidateRect (m_hwnd, nullptr, FALSE);

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  HyperlinkCount
//
////////////////////////////////////////////////////////////////////////////////

size_t DialogPrimitive::HyperlinkCount() const
{
    size_t  count = 0;

    if (m_def == nullptr)
    {
        return 0;
    }

    for (const DialogTextRun & run : m_def->body)
    {
        if (run.isHyperlink)
        {
            count++;
        }
    }

    return count;
}




////////////////////////////////////////////////////////////////////////////////
//
//  NthHyperlinkBodyIdx
//
////////////////////////////////////////////////////////////////////////////////

size_t DialogPrimitive::NthHyperlinkBodyIdx (size_t hyperlinkIdx) const
{
    size_t  seen = 0;

    if (m_def == nullptr)
    {
        return SIZE_MAX;
    }

    for (size_t bi = 0; bi < m_def->body.size(); bi++)
    {
        if (!m_def->body[bi].isHyperlink)
        {
            continue;
        }

        if (seen == hyperlinkIdx)
        {
            return bi;
        }

        seen++;
    }

    return SIZE_MAX;
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

bool DialogPrimitive::DispatchCustomBodyInput (DialogInputEvent::Kind kind, int xPx, int yPx, int vkCode)
{
    DialogInputEvent           ev   = {};
    std::optional<int>         req;
    int                        titleH = TitleHeightPx();
    RECT                       rc = m_layout.customBodyRectPx;
    bool                       consumed = false;

    if (m_def == nullptr || !m_def->onInputCustomBody)
    {
        return false;
    }

    rc.top    += titleH;
    rc.bottom += titleH;

    if (kind == DialogInputEvent::Kind::KeyDown)
    {
        ev.kind   = kind;
        ev.vkCode = vkCode;
        req       = m_def->onInputCustomBody (ev);
    }
    else
    {
        bool insideX = (xPx >= rc.left && xPx < rc.right);
        bool insideY = (yPx >= rc.top  && yPx < rc.bottom);
        if (!(insideX && insideY))
        {
            return false;
        }

        ev.kind = kind;
        ev.xPx  = xPx - rc.left;
        ev.yPx  = yPx - rc.top;
        req     = m_def->onInputCustomBody (ev);
        consumed = true;
    }

    if (req.has_value())
    {
        Close (req.value());
        return true;
    }

    InvalidateRect (m_hwnd, nullptr, FALSE);
    return consumed;
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
    int     titleH     = TitleHeightPx();
    size_t  hlIdx      = 0;

    outBodyRunIdx = SIZE_MAX;

    if (m_def == nullptr)
    {
        return false;
    }

    for (size_t i = 0; i < m_def->body.size(); ++i)
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
            return true;
        }

        ++hlIdx;
    }

    return false;
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
//  TitleHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DialogPrimitive::TitleHeightPx() const
{
    return static_cast<int> (s_kTitleHeightDp * static_cast<float> (m_dpi) / static_cast<float> (DpiScaler::kBaseDpi));
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
    int          titleH   = TitleHeightPx();
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
//  DefaultButtonIdx
//
////////////////////////////////////////////////////////////////////////////////

size_t DialogPrimitive::DefaultButtonIdx() const
{
    if (m_def == nullptr)
    {
        return SIZE_MAX;
    }

    for (size_t i = 0; i < m_def->buttons.size(); ++i)
    {
        if (m_def->buttons[i].isDefault)
        {
            return i;
        }
    }

    return SIZE_MAX;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CancelButtonIdx
//
////////////////////////////////////////////////////////////////////////////////

size_t DialogPrimitive::CancelButtonIdx() const
{
    if (m_def == nullptr)
    {
        return SIZE_MAX;
    }

    for (size_t i = 0; i < m_def->buttons.size(); ++i)
    {
        if (m_def->buttons[i].isCancel)
        {
            return i;
        }
    }

    return SIZE_MAX;
}
